/**
 * this sample code snapshot a video yuv422 frame, and record for 10 seconds.
 * trigger snapshot yuv422 per second. 
 * Please use gmdvr_mem_3_3_3_3.cfg for the buffer config. 
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "dvr_common_api.h"
#include "dvr_enc_api.h"
#include "gmavi_api.h"

int dvr_fd = 0;
int enc_fd = 0;

//test record
unsigned char *bs_buf;
HANDLE  rec_file;
int enc_buf_size;
struct pollfd rec_fds;
char file_name[128];
int sub1_bs_buf_offset;

dvr_enc_channel_param   ch_param;    
ReproduceBitStream  sub_ch_param;
EncParam_Ext3 enc_param_ext = {0};    
dvr_enc_control  enc_ctrl;
FuncTag tag;        

dvr_enc_channel_param   user_rec_ch_setting = 
{
    { 
        3,                  /* channel number */
        ENC_TYPE_FROM_CAPTURE,
//        {1280, 720},      /* channel 0 */
//        {640, 480},       /* channel 1 */
//        {320, 240},       /* channel 2 */
        {160, 112},       /* channel 3 */
        LVFRAME_EVEN_ODD, 
        LVFRAME_FRAME_MODE, 
        DMAORDER_PACKET, 
        CAPSCALER_NOT_KEEP_RATIO,
        MCP_VIDEO_NTSC,    
        CAPCOLOR_YUV422,  
        { FALSE, FALSE, GM3DI_FIELD }   
    },
    {
        DVR_ENC_EBST_ENABLE,  
        0,
        ENC_TYPE_H264,
        FALSE,
        DVR_ENC_EBST_DISABLE, 
//        {1280, 720},      /* channel 0 */
//        {640, 480},       /* channel 1 */
//        {320, 240},       /* channel 2 */
        {160, 112},       /* channel 3 */
        {ENC_INPUT_H2642D, 30, 200*1000,  30, 25, 51, 1 , FALSE, {0, 0, 160, 112}},   
        {SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 },          
        {JCS_yuv420, 0, JENC_INPUT_MP42D, 70}                                        
    }
};

ReproduceBitStream   user_rec_sub_ch_setting = 
{
    DVR_ENC_EBST_ENABLE,  //enabled
    1,  // sub1-bitstream
    ENC_TYPE_YUV422, //enc_type, 0: ENC_TYPE_H264, 1:ENC_TYPE_MPEG, 2:ENC_TYPE_MJPEG, 3:ENC_TYPE_YUV422
    FALSE,  // is_blocked
    DVR_ENC_EBST_DISABLE, // en_snapshot,
//    {640, 480},       /* channel 1 */
//    {320, 240},       /* channel 2 */
    {160, 112},       /* channel 3 */
    {ENC_INPUT_H2642D, 30, 262144,  15, 25, 51, 1 , FALSE, {0, 0, 160, 112}},   //EncParam
    {SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 }, //ScalerParam
    {JCS_yuv420, 0, JENC_INPUT_MP42D, 70 }  //snapshot_param    
};

void do_record_start(void)
{
    memcpy(&ch_param, &user_rec_ch_setting, sizeof(ch_param)); //main-bitstream
    ch_param.main_bs.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    ch_param.main_bs.enc.pext_data = &enc_param_ext;
        
    enc_param_ext.feature_enable = 0;     
    
    ioctl(enc_fd, DVR_ENC_SET_CHANNEL_PARAM, &ch_param);
    
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SIZE, &enc_buf_size);

    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SUB1_BS_OFFSET, &sub1_bs_buf_offset);

    bs_buf = (unsigned char*) mmap(NULL, enc_buf_size, PROT_READ|PROT_WRITE, 
                                          MAP_SHARED, enc_fd, 0);    

    memcpy(&sub_ch_param, &user_rec_sub_ch_setting, sizeof(sub_ch_param));  //sub1-raw
    ioctl(enc_fd, DVR_ENC_SET_SUB_BS_PARAM, &sub_ch_param);        

    //record start
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = 0;    
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = sub_ch_param.out_bs;    
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);

    // set function tag paremeter to dvr graph level
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_param.src.channel);
    FN_SET_SUB1_REC_CH(&tag, ch_param.src.channel);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
}

void do_record_stop(void)
{
    //record stop
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));    

    enc_ctrl.stream = sub_ch_param.out_bs;
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    enc_ctrl.stream = 0;
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);

    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_param.src.channel);
    FN_SET_SUB1_REC_CH(&tag, ch_param.src.channel);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    munmap((void*)bs_buf, enc_buf_size);      
}

/**
 * @brief main function
 * @return 0 on success, !0 on error
 */
int main(int argc, char *argv[])
{
    int ret = 0, ch_num;    
    dvr_enc_queue_get   data;
    unsigned char *buf;
    int buf_size;          
    struct timeval t1,t2;  
    char tmp_str[128];
    int raw_no = 1;
    int flag_raw = 0;
                    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
    ch_num = user_rec_ch_setting.src.channel;

    do_record_start();

    sprintf(file_name, "CH%d_video_%d", ch_num, 0);      
    sprintf(tmp_str, "%s.h264", file_name);            
    rec_file = fopen ( tmp_str , "wb+" );
    gettimeofday(&t1, NULL);        
    
    while(1) {          
        // prepare to select(or poll)
        rec_fds.fd = enc_fd;      
        rec_fds.revents = 0;            
        rec_fds.events = (POLLIN_MAIN_BS | POLLIN_SUB1_BS);
            
        poll(&rec_fds, 1, 500);     
            
        if (rec_fds.revents & POLLIN_SUB1_BS) {    
            ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET_SUB1_BS, &data);
            if (ret >= 0) {
                FILE * fd;              
                
                buf = bs_buf + sub1_bs_buf_offset + data.bs.offset;
                buf_size = data.bs.length;
                printf("<buf_size=%d>\n", buf_size);
                sprintf(tmp_str, "CH%d_raw%03d.yuv", ch_num, ++flag_raw);
                fd = fopen ( tmp_str , "wb+" );
                fwrite(buf , 1 , buf_size , fd);                
                fclose(fd);
                printf("capture raw: output file %s\n", tmp_str);
                ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);
            }
        }
            
        if (!(rec_fds.revents & POLLIN_MAIN_BS)) 
            continue;
    
        // get dataout buffer   
        ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET, &data);
        if(ret < 0)    
            continue;
        
        buf = bs_buf + data.bs.offset;
        buf_size = data.bs.length;
    
        fwrite (buf , 1 , buf_size , rec_file);
        fflush(rec_file);  
        ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
                
        gettimeofday(&t2, NULL);
    
        if ((t2.tv_sec - t1.tv_sec) == 1) {      //< trigger capture yuv422 per second,
            memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
            enc_ctrl.command = ENC_RAW;
            enc_ctrl.output.count = (int *)raw_no;     //< do capture yuv422 1 times.
            enc_ctrl.stream = sub_ch_param.out_bs;     //< sub_bitstream number
            ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);                               
            t1.tv_sec = t2.tv_sec;
        }       
        if (flag_raw >= 10) {      //< record for 10 seconds. then finish record.                 
            fclose(rec_file);       
            break;               
        }  
    }

    do_record_stop();    

    printf("----------------------------------\n");
    printf(" Record finish\n");
    printf("----------------------------------\n");    

    close(enc_fd);      //close_dvr_encode    
    close(dvr_fd);      //close_dvr_common
    return 0;
}
