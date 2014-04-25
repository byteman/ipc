/**
 * this sample code implement snapshot function, and record for 15 seconds.
 * trigger snapshot at 5th seconds, and do snapshot 3 times.
 *
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
int enc_fd_channel[DVR_RECORD_CHANNEL_NUM] = {0};

//test record
unsigned char *bs_buf;
HANDLE  rec_file;
int avi_str_id;
int enc_buf_size;
struct pollfd rec_fds;
char file_name[128];

int sub1_bs_buf_offset;
int sub2_bs_buf_offset;
int bs_buf_snap_offset;
int encode_frame_count = 0;

dvr_enc_channel_param   ch_param;    
EncParam_Ext3 enc_param_ext = {0};    
dvr_enc_control  enc_ctrl;
FuncTag tag;        

dvr_enc_channel_param   user_rec_ch_setting = 
{
    { 
        0,                  /* channel number */
        ENC_TYPE_FROM_CAPTURE,
        {1280, 720},      /* channel 0 */
//        {640, 480},       /* channel 1 */
//        {320, 240},       /* channel 2 */
//        {160, 112},       /* channel 3 */
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
        DVR_ENC_EBST_ENABLE, 
        {1280, 720},      /* channel 0 */
//        {640, 480},       /* channel 1 */
//        {320, 240},       /* channel 2 */
//        {160, 112},       /* channel 3 */
        {ENC_INPUT_H2642D, 30, 2000*1000,  30, 25, 51, 1 , FALSE, {0, 0, 320, 240}},   
        {SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 },          
        {JCS_yuv420, 0, JENC_INPUT_MP42D, 70}                                        
    }
};

void do_record_start(void)
{
    memcpy(&ch_param, &user_rec_ch_setting, sizeof(ch_param));

    ch_param.main_bs.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    ch_param.main_bs.enc.pext_data = &enc_param_ext;
        
    enc_param_ext.feature_enable = 0;     
    
    ioctl(enc_fd, DVR_ENC_SET_CHANNEL_PARAM, &ch_param);
    
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SIZE, &enc_buf_size);

    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SNAP_OFFSET, &bs_buf_snap_offset);
        
    bs_buf = (unsigned char*) mmap(NULL, enc_buf_size, PROT_READ|PROT_WRITE, 
                                          MAP_SHARED, enc_fd, 0);    
    //record start
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = 0;    
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    // set function tag paremeter to dvr graph level
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_param.src.channel);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
}

void do_record_stop(void)
{
    //record stop
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));    
    enc_ctrl.stream = 0;
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_param.src.channel);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    munmap((void*)bs_buf, enc_buf_size);      
}

/**
 * @brief main function
 * @return 0 on success, !0 on error
 */
int main(int argc, char *argv[])
{
    int ret = 0, ch_num, i;    
    dvr_enc_queue_get   data;
    unsigned char *buf;
    int buf_size;          
    struct timeval t1,t2;  
    char tmp_str[128];
    int snap_no = 3, flag = 0;
    int flag_snapshot = 0;
                    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common

    for(i=0; i<DVR_RECORD_CHANNEL_NUM; i++ ) {
        enc_fd_channel[i] = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
    }
    ch_num = user_rec_ch_setting.src.channel;
    enc_fd = enc_fd_channel[ch_num];

    do_record_start();
    

    sprintf(file_name, "CH%d_video_%d", ch_num, 0);      
    sprintf(tmp_str, "%s.h264", file_name);            
    rec_file = fopen ( tmp_str , "wb+" );
    gettimeofday(&t1, NULL);        
    
    while(1) {          
        // prepare to select(or poll)
        rec_fds.fd = enc_fd;      
        rec_fds.revents = 0;            
        rec_fds.events = (POLLIN_MAIN_BS | POLLIN_SNAP_BS);
            
        poll(&rec_fds, 1, 500);     
            
        if (rec_fds.revents & POLLIN_SNAP_BS) {    
            ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET_SNAP, &data);
            if (ret >= 0) {
                FILE * fd;              
                
                buf = bs_buf + bs_buf_snap_offset + data.bs.offset;
                buf_size = data.bs.length;
                
                -- flag_snapshot;                        
                sprintf(tmp_str, "CH%d_snapshot%02d_%03d.jpg", ch_num, flag_snapshot, encode_frame_count);
                fd = fopen ( tmp_str , "wb+" );
                fwrite(buf , 1 , buf_size , fd);                
                fclose(fd);
                
                printf("snapshot: output file %s\n", tmp_str);
                ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);
            }
        }
            
        if (!(rec_fds.revents & POLLIN_MAIN_BS)) 
            continue;
    
        // get dataout buffer   
        ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET, &data);
        if(ret < 0)    
            continue;
        
        encode_frame_count ++;      
        buf = bs_buf + data.bs.offset;
        buf_size = data.bs.length;
    
        fwrite (buf , 1 , buf_size , rec_file);
        fflush(rec_file);  
        ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
                
        gettimeofday(&t2, NULL);
    
        if ((t2.tv_sec - t1.tv_sec) == 5) {      //< trigger snapshot at 5th seconds,
            if (flag == 0){                      //< and do snapshot 3 times.
                flag = 1;                
                memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
                enc_ctrl.command = ENC_SNAP;
                flag_snapshot = snap_no;    
                enc_ctrl.output.count = (int *)snap_no;     //< do snapshot 3 times.
                ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);                               
            }
        }       
                                  
        if ((t2.tv_sec - t1.tv_sec) == 15) {      //< record for 15 seconds. then finish record.                 
            fclose(rec_file);       
            break;               
        }       
    }

    do_record_stop();    

    printf("----------------------------------\n");
    printf(" Record finish\n");
    printf("----------------------------------\n");    
    
    for(i=0; i<DVR_RECORD_CHANNEL_NUM; i++ ) {
        close(enc_fd_channel[i]);      //close_dvr_encode    
    }

    close(dvr_fd);      //close_dvr_common

    return 0;
}
