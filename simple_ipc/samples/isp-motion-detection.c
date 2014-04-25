/**
 * this sample code implement motion detection function with ISP module, and record for 30 seconds.
 *
 * here we have to notice one thing, after enable ISP motion detection feature, 
 * we need to wait 5~10 seconds, then start to do motion detection, 
 * that's because when we enable ISP motion detection feature, 
 * ISP will analyze the background, and during this period, 
 * isp motion detection threshold is big, if we do motion detection right now, 
 * we will trigger motion alarm.
 * so we have to wait 5~10 seconds after enable ISP motion detection feature.
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
#include "ioctl_isp.h"

int dvr_fd = 0;
int enc_fd = 0;
int isp_fd = 0;
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
//isp motion detection
int demo_motion = 1;
static unsigned int frame_no_drv = 0;
static unsigned int frame_no_drv_start = 0;
static unsigned int frame_no_ipc = 0;

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
    int flag_snapshot = 1;
    int mb_width, mb_height;    
    int i,j;
    unsigned char md_info[2048 + 12];
    unsigned int md_info_filt[2048 * 8];
    int md_threshold = 40;
                    
    isp_fd = open("/dev/isp", O_RDWR);   //open isp
    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
            
    do_record_start(ch_num);
            
    // do isp210 motion detection init.     
    ioctl(isp_fd, ISP_IO_ENABLE_MD, &demo_motion);
    
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
        
        ioctl(isp_fd, ISP_IO_GET_MD_INFO, &md_info);        
        
        if (frame_no_ipc == 0)
            frame_no_drv_start = ((md_info[0]) | (md_info[1] << 8) | (md_info[2] << 16)
                                    | (md_info[3] << 24));        
        
        if ((t2.tv_sec - t1.tv_sec) >= 10) {     //after enable ISP motion detection feature in 10 seconds,
            unsigned int active_num = 0;         //start to do motion detection
            if (frame_no_drv != ((md_info[0]) | (md_info[1] << 8) | (md_info[2] << 16) | (md_info[3] << 24))){
                frame_no_drv = ((md_info[0]) | (md_info[1] << 8) | (md_info[2] << 16) 
                                 | (md_info[3] << 24));
                mb_width = ((md_info[4]) | (md_info[5] << 8) | (md_info[6] << 16) 
                                 | (md_info[7] << 24));
                mb_height = ((md_info[8]) | (md_info[9] << 8) | (md_info[10] << 16)
                                 | (md_info[11] << 24));
        
                for(i = 0; i < (int)((mb_width*mb_height + 7) / 8); i++) {
                    for(j = 0; j < 8; j++){
                        md_info_filt[i * 8 + j] = ((md_info[i + 12] >> j)& 0x1);
                        if((i * 8 + j) == (mb_width * mb_height))
                            break;
                    }
                }    
        
                favc_do_noise_filtering(md_info_filt, mb_width, mb_height);            
                for(i = 0; i < mb_height; i++){
                    for(j = 0; j < mb_width; j++){
                        active_num += md_info_filt[i * mb_width + j];                  
                    }           
                }
       
                if (active_num >= md_threshold) { //bigger than md_threshold, motion alarm !!
                    printf("(drv %d) Motion Alarm %dx%d < %d >\n", (frame_no_drv - frame_no_drv_start), mb_width, mb_height, active_num);
                    snapshot_trigger(ch_num);
                }              
            }
        }
        
        frame_no_ipc ++;
        
        fwrite(buf , 1 , buf_size , rec_file);
        fflush(rec_file);          
        ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
                
        gettimeofday(&t2, NULL);            
                                  
        if ((t2.tv_sec - t1.tv_sec) == 30) {      //< record for 30 seconds. then finish record.                 
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
    close(isp_fd);
    return 0;
}
