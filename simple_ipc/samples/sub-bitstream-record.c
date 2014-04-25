/**
 * this sample code implement main bitstream + sub1 bitstream + sub2 bitstream record function  
 * and record for 20 seconds.
 * (1)  for H264 record, file format please indicate xxx.h264
 *      for MPEG record, file format please indicate xxx.m4v
 *      for MOTION JPEG record, file format please indicate xxx.jpg
 * (2)  for H264 record, max_quant < 51, 1 < min_quant 
 *      for MPEG record, max_quant < 31, 1 < min_quant 
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
int sub1_bs_buf_offset;
int sub2_bs_buf_offset;
int bs_buf_snap_offset;
struct pollfd rec_fds;
char file_name[128];

//sub record
char sub_rec_filename[DVR_ENC_REPD_BT_NUM - 1][64];
int multi_bitstream = 0;
HANDLE  rec_sub_bs_file[DVR_ENC_REPD_BT_NUM - 1];

/**
 * @brief main function
 * @return 0 on success, !0 on error
 */
int main(int argc, char *argv[])
{
    int ret = 0, ch_num = 0, sub1_num = 1, sub2_num = 2;    
    dvr_enc_channel_param   ch_param;    
    ReproduceBitStream sub1_param; 
    ReproduceBitStream sub2_param; 
    EncParam_Ext3 enc_param_ext = {0};    
    EncParam_Ext3 enc_param_ext1 = {0};    
    EncParam_Ext3 enc_param_ext2 = {0};    
    dvr_enc_control  enc_ctrl;
    dvr_enc_queue_get   data;
    unsigned char *buf;
    int buf_size;      
    FuncTag tag;        
    struct timeval t1,t2;  
    char tmp_str[128];
                    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
            
    //set dvr encode source parameter
    ch_param.src.input_system = MCP_VIDEO_NTSC;
    ch_param.src.channel = ch_num;
    ch_param.src.enc_src_type = ENC_TYPE_FROM_CAPTURE;
    
    ch_param.src.dim.width = 1280;
    ch_param.src.dim.height = 720;
    
    ch_param.src.di_mode = LVFRAME_EVEN_ODD;
    ch_param.src.mode = LVFRAME_FRAME_MODE;
    ch_param.src.dma_order = DMAORDER_PACKET;
    ch_param.src.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
    ch_param.src.input_system = MCP_VIDEO_NTSC;
    ch_param.src.color_mode = CAPCOLOR_YUV422;
    
    ch_param.src.vp_param.is_3DI = FALSE;
    ch_param.src.vp_param.is_denoise = FALSE;
    ch_param.src.vp_param.denoise_mode = GM3DI_FIELD;
    ///////////////////////////////////////////////////////////////
    //set dvr encode main bitstream parameter
    ch_param.main_bs.enabled = DVR_ENC_EBST_ENABLE;
    ch_param.main_bs.out_bs = 0;
    ch_param.main_bs.enc_type = ENC_TYPE_H264;    
    ch_param.main_bs.is_blocked = FALSE;
    ch_param.main_bs.en_snapshot = DVR_ENC_EBST_DISABLE;
    
    ch_param.main_bs.dim.width = 1280;
    ch_param.main_bs.dim.height = 720;
    
    //set main bitstream encode parameter
    ch_param.main_bs.enc.input_type = ENC_INPUT_H2642D;
    ch_param.main_bs.enc.frame_rate = 30;
    ch_param.main_bs.enc.bit_rate = 1048576;
    ch_param.main_bs.enc.ip_interval = 30;
    ch_param.main_bs.enc.init_quant = 25;
    ch_param.main_bs.enc.max_quant = 51;
    ch_param.main_bs.enc.min_quant = 1;
    ch_param.main_bs.enc.is_use_ROI = FALSE;
    ch_param.main_bs.enc.ROI_win.x = 0;
    ch_param.main_bs.enc.ROI_win.y = 0;
    ch_param.main_bs.enc.ROI_win.width = 320;
    ch_param.main_bs.enc.ROI_win.height = 240;
    
    //set main bitstream scalar parameter
    ch_param.main_bs.scl.src_fmt = SCALE_YUV422;
    ch_param.main_bs.scl.dst_fmt = SCALE_YUV422;
    ch_param.main_bs.scl.scale_mode = SCALE_LINEAR;
    ch_param.main_bs.scl.is_dither = FALSE;
    ch_param.main_bs.scl.is_correction = FALSE;
    ch_param.main_bs.scl.is_album = TRUE;
    ch_param.main_bs.scl.des_level = 0;
    
    //set main bitstream snapshot parameter
    ch_param.main_bs.snap.sample = JCS_yuv420;   
    ch_param.main_bs.snap.RestartInterval = 0;   
    ch_param.main_bs.snap.u82D = JENC_INPUT_MP42D;   
    ch_param.main_bs.snap.quality = 70;   
    
    //associate the ext. structure                      
    ch_param.main_bs.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    ch_param.main_bs.enc.pext_data = &enc_param_ext;
    
    enc_param_ext.feature_enable = 0;      //CBR

    ////////////////////////////////////////////////////////////////
    //sub1 bitstream    
    memset(&sub1_param, 0x0, sizeof(ReproduceBitStream));    

    sub1_param.enabled = DVR_ENC_EBST_ENABLE;
    sub1_param.out_bs = 1;
    sub1_param.enc_type = ENC_TYPE_H264;    
    sub1_param.is_blocked = FALSE;
    sub1_param.en_snapshot = DVR_ENC_EBST_DISABLE;
    
    sub1_param.dim.width = 1280;
    sub1_param.dim.height = 720;
    
    sub1_param.enc.input_type = ENC_INPUT_H2642D;
    sub1_param.enc.frame_rate = 30;
    sub1_param.enc.bit_rate = 262144;
    sub1_param.enc.ip_interval = 15;
    sub1_param.enc.init_quant = 25;
    sub1_param.enc.max_quant = 51;
    sub1_param.enc.min_quant = 1;
    sub1_param.enc.is_use_ROI = FALSE;
    sub1_param.enc.ROI_win.x = 0;
    sub1_param.enc.ROI_win.y = 0;
    sub1_param.enc.ROI_win.width = 320;
    sub1_param.enc.ROI_win.height = 240;
    
    //set sub1 bitstream scalar parameter
    sub1_param.scl.src_fmt = SCALE_YUV422;
    sub1_param.scl.dst_fmt = SCALE_YUV422;
    sub1_param.scl.scale_mode = SCALE_LINEAR;
    sub1_param.scl.is_dither = FALSE;
    sub1_param.scl.is_correction = FALSE;
    sub1_param.scl.is_album = TRUE;
    sub1_param.scl.des_level = 0;
    
    //set sub1 bitstream snapshot parameter
    sub1_param.snap.sample = JCS_yuv420;   
    sub1_param.snap.RestartInterval = 0;   
    sub1_param.snap.u82D = JENC_INPUT_MP42D;   
    sub1_param.snap.quality = 70;   
    //associate the ext. structure                      
    sub1_param.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext1));
    sub1_param.enc.pext_data = &enc_param_ext1;            
        
    enc_param_ext1.feature_enable &= ~DVR_ENC_MJPEG_FUNCTION;
            
    //////////////////////////////////////////////////////////////////////
    //sub2 bitstream
    memset(&sub2_param, 0x0, sizeof(ReproduceBitStream));    

    sub2_param.enabled = DVR_ENC_EBST_ENABLE;
    sub2_param.out_bs = 2;
    sub2_param.enc_type = ENC_TYPE_H264;    
    sub2_param.is_blocked = FALSE;
    sub2_param.en_snapshot = DVR_ENC_EBST_DISABLE;
    
    sub2_param.dim.width = 1280;
    sub2_param.dim.height = 720;
    
    sub2_param.enc.input_type = ENC_INPUT_H2642D;
    sub2_param.enc.frame_rate = 30;
    sub2_param.enc.bit_rate = 131072;
    sub2_param.enc.ip_interval = 15;
    sub2_param.enc.init_quant = 25;
    sub2_param.enc.max_quant = 51;
    sub2_param.enc.min_quant = 1;
    sub2_param.enc.is_use_ROI = FALSE;
    sub2_param.enc.ROI_win.x = 0;
    sub2_param.enc.ROI_win.y = 0;
    sub2_param.enc.ROI_win.width = 320;
    sub2_param.enc.ROI_win.height = 240;
    
    //set sub2 bitstream scalar parameter
    sub2_param.scl.src_fmt = SCALE_YUV422;
    sub2_param.scl.dst_fmt = SCALE_YUV422;
    sub2_param.scl.scale_mode = SCALE_LINEAR;
    sub2_param.scl.is_dither = FALSE;
    sub2_param.scl.is_correction = FALSE;
    sub2_param.scl.is_album = TRUE;
    sub2_param.scl.des_level = 0;
    
    //set sub2 bitstream snapshot parameter
    sub2_param.snap.sample = JCS_yuv420;   
    sub2_param.snap.RestartInterval = 0;   
    sub2_param.snap.u82D = JENC_INPUT_MP42D;   
    sub2_param.snap.quality = 70;   
    //associate the ext. structure                      
    sub2_param.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext2));
    sub2_param.enc.pext_data = &enc_param_ext2;
    
    enc_param_ext2.feature_enable &= ~DVR_ENC_MJPEG_FUNCTION;        
    
    //////////////////////////////////////////////////////////////////////    
    
    ioctl(enc_fd, DVR_ENC_SET_CHANNEL_PARAM, &ch_param);
    
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SIZE, &enc_buf_size);
    
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SUB1_BS_OFFSET, &sub1_bs_buf_offset);
    
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SUB2_BS_OFFSET, &sub2_bs_buf_offset);
    
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SNAP_OFFSET, &bs_buf_snap_offset);
        
    bs_buf = (unsigned char*) mmap(NULL, enc_buf_size, PROT_READ|PROT_WRITE, 
                                          MAP_SHARED, enc_fd, 0);
    
    ioctl(enc_fd, DVR_ENC_SET_SUB_BS_PARAM, &sub1_param);        
    
    ioctl(enc_fd, DVR_ENC_SET_SUB_BS_PARAM, &sub2_param);        
    /////////////////////////////////////////////////////////////////            
    //main bitstream record start
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = 0;    
    ret = ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    // set function tag paremeter to dvr graph level
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    ///////////////////////////////////////////////////////////////    
    multi_bitstream = DVR_ENC_EBST_ENABLE;    
    //sub1 bitstream record start
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = 1;    
    ret = ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    // set function tag paremeter to dvr graph level
    FN_RESET_TAG(&tag);    
    FN_SET_SUB1_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    ///////////////////////////////////////////////////////////////         
    //sub2 bitstream record start
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = 2;    
    ret = ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    // set function tag paremeter to dvr graph level
    FN_RESET_TAG(&tag);    
    FN_SET_SUB2_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    ///////////////////////////////////////////////////////////////         
    //main bitstream    
    sprintf(file_name, "CH%d_video_%d", 0, 0);      
    sprintf(tmp_str, "%s.h264", file_name);        
    rec_file = fopen ( tmp_str , "wb+" );
    ///////////////////////////////////////////////////////////////
    //sub1 bitstream    
    sprintf(sub_rec_filename[sub1_num - 1], "CH%d_Sub%d_Video_%03d", ch_num, sub1_num, 001);
    sprintf(tmp_str, "%s.h264", sub_rec_filename[sub1_num - 1]);    
    rec_sub_bs_file[sub1_num - 1] = fopen ( tmp_str , "wb+" );
    ///////////////////////////////////////////////////////////////
    //sub2 bitstream
    sprintf(sub_rec_filename[sub2_num - 1], "CH%d_Sub%d_Video_%03d", ch_num, sub2_num, 001);        
    sprintf(tmp_str, "%s.h264", sub_rec_filename[sub2_num - 1]);
    rec_sub_bs_file[sub2_num - 1] = fopen ( tmp_str , "wb+" );
    ///////////////////////////////////////////////////////////////
    gettimeofday(&t1, NULL);        
    
    while(1) {          
            // prepare to select(or poll)
            rec_fds.fd = enc_fd;      
            rec_fds.revents = 0;
            rec_fds.events = POLLIN_MAIN_BS;
            
            if (multi_bitstream == DVR_ENC_EBST_ENABLE)
                rec_fds.events |= (POLLIN_SUB1_BS | POLLIN_SUB2_BS);
            
            poll(&rec_fds, 1, 500);     
            
            if (rec_fds.revents & POLLIN_SUB1_BS) {                
                ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET_SUB1_BS, &data); 
                if(ret < 0)    
                    continue;
                   
                buf = bs_buf + sub1_bs_buf_offset + data.bs.offset;         
                buf_size = data.bs.length;
    
                fwrite (buf , 1 , buf_size , rec_sub_bs_file[sub1_num - 1]);
                fflush(rec_sub_bs_file[sub1_num - 1]);  
                ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
            }    
            
            if (rec_fds.revents & POLLIN_SUB2_BS) {                
                ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET_SUB2_BS, &data); 
                if(ret < 0)    
                    continue;
                   
                buf = bs_buf + sub2_bs_buf_offset + data.bs.offset;         
                buf_size = data.bs.length;
    
                fwrite (buf , 1 , buf_size , rec_sub_bs_file[sub2_num - 1]);
                fflush(rec_sub_bs_file[sub2_num - 1]);  
                ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
            }
            
            if (!(rec_fds.revents & POLLIN_MAIN_BS)) 
                continue;
    
            // get dataout buffer   
            ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET, &data);
            if(ret < 0)    
                continue;
        
            buf = bs_buf + data.bs.offset;
            buf_size = data.bs.length;
    
            ret = fwrite (buf , 1 , buf_size , rec_file);
            fflush(rec_file);  
            ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
                
            gettimeofday(&t2, NULL);
        
            if ((t2.tv_sec - t1.tv_sec) == 20) {    //<record for 20 seconds. then finish record.                                      
                fclose(rec_file);      
                fclose(rec_sub_bs_file[sub1_num - 1]);       
                fclose(rec_sub_bs_file[sub2_num - 1]);       
                break;               
            }       
    }
        
    //record stop
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));    
    enc_ctrl.stream = 0;    // for all main and sub 
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_num);
    FN_SET_SUB1_REC_CH(&tag, ch_num);
    FN_SET_SUB2_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    munmap((void*)bs_buf, enc_buf_size);      
    
    printf("----------------------------------\n");
    printf(" Record finish\n");
    printf("----------------------------------\n");    
    
    close(enc_fd);      //close_dvr_encode    
    close(dvr_fd);      //close_dvr_common

    return 0;
}











