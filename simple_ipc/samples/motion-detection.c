/**
 * this sample code implement H264 motion detection function without ISP module, and record for 20 seconds.
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

//test record
unsigned char *bs_buf;
HANDLE  rec_file;
int avi_str_id;
int enc_buf_size;
struct pollfd rec_fds;
char file_name[128];

int bs_buf_snap_offset;
int encode_frame_count = 0;

dvr_enc_channel_param   ch_param;    
EncParam_Ext3 enc_param_ext = {0};    
dvr_enc_control  enc_ctrl;
FuncTag tag;        

//global motion detection structure
#include "favc_motiondet.h"
struct favc_md gfavcmd; 
struct md_cfg md_param;
struct md_res active;    

dvr_enc_channel_param   user_rec_ch_setting = 
{
    { 
        0,
        ENC_TYPE_FROM_CAPTURE,
        {1280, 720},
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
        {1280, 720},         
        {ENC_INPUT_H2642D, 30, 1048576,  30, 25, 51, 1 , FALSE, {0, 0, 320, 240}},   
        {SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 },          
        {JCS_yuv420, 0, JENC_INPUT_MP42D, 70}                                        
    }
};

void do_record_start(int ch_num)
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
    FN_SET_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);       
}

void do_record_stop(int ch_num)
{
    //record stop
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));    
    enc_ctrl.stream = 0;
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    munmap((void*)bs_buf, enc_buf_size);      
}

void snapshot_trigger(int ch_num)
{
    int snap_no = 1;
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_SNAP;    
    enc_ctrl.output.count = (int *)snap_no;     //< do snapshot 1 time
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);  
}

/**
 * @brief main function
 * @return 0 on success, !0 on error
 */
int main(int argc, char *argv[])
{
    int ret = 0, ch_num = 0;    
    dvr_enc_queue_get   data;
    unsigned char *buf;
    int buf_size;          
    struct timeval t1,t2;  
    char tmp_str[128];    
    int mb_width,mb_height;
    int count;
    int flag_snapshot = 1;
                    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
            
    do_record_start(ch_num);
    
    //do motion detection init
    mb_width = (ch_param.src.dim.width + 16 - 1) / 16;
    mb_height = (ch_param.src.dim.height + 16 - 1) / 16;
    
    favc_motion_info_init(&md_param, &active, &gfavcmd, mb_width, mb_height);      
    
    for (count = 0; count < 1; count ++) {   
        int i,j;        
        md_param.interlace_mode = 0;
        md_param.usesad = 1;  //no sad
        md_param.mb_time_th = 3; 
        md_param.rolling_prot_th = 30;
        md_param.alarm_th = 10;
 
        for (i = 0; i < mb_height; i++)
            for (j = 0; j < mb_width; j++)
                md_param.mb_cell_en[(i*mb_width + j)] = 1;  
 
        for (i = 0; i < mb_height; i++)
            for (j = 0; j < mb_width; j++)
                md_param.mb_cell_th[(i*mb_width + j)] = 10;    
    }        

    sprintf(file_name, "CH%d_video_%d", 0, 0);      
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
                
        //do motion detection
        for (count = 0; count < 1; count ++) {            
            favc_motion_detection(bs_buf + data.bs.mv_offset, &md_param, &active, &gfavcmd);                       
            if ((active.active_num >= md_param.alarm_th)) {
                printf("Motion Alarm @ %s< %d >\n", "main_stream", active.active_num);          
                snapshot_trigger(ch_num);
            }
        }
        
        fwrite(buf , 1 , buf_size , rec_file);
        fflush(rec_file);          
        ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
                
        gettimeofday(&t2, NULL);            
                                  
        if ((t2.tv_sec - t1.tv_sec) == 20) {      //< record for 20 seconds. then finish record.                 
            fclose(rec_file);       
            break;               
        }       
    }

    do_record_stop(ch_num);    

    printf("----------------------------------\n");
    printf(" Record finish\n");
    printf("----------------------------------\n");    
    
    close(enc_fd);      //close_dvr_encode    
    close(dvr_fd);      //close_dvr_common

    return 0;
}
