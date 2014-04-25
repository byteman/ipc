/**
 * This sample code is for ROI function, 
 * main_bitstream record for 20 seconds H264 format.
 * sub1_bitstram record for ROI(320x240)/ROI(640x480)/disable_ROI, and dynamically change position per second.
 * ROI_win.y should be align at 2 pxls, ROI_win.x have no alignment limitation.
 * ROI_win.width/ROI_win.height should be align at 16,
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

#define ROI_COORDINATE_X_STEP 20
#define ROI_COORDINATE_Y_STEP 20

int dvr_fd = 0;
int enc_fd = 0;

//test record
unsigned char *bs_buf;
HANDLE  rec_file;
int enc_buf_size;
struct pollfd rec_fds;
char file_name[128];
int sub1_bs_buf_offset;
char sub_rec_filename[64];
HANDLE  rec_sub_bs_file;

dvr_enc_channel_param   ch_param;    
ReproduceBitStream  sub_ch_param;
EncParam_Ext5 enc_param_ext = {0};    
dvr_enc_control  enc_ctrl;
FuncTag tag;        

dvr_enc_channel_param   user_rec_ch_setting = 
{
    { 
        0,                  /* channel number */
        ENC_TYPE_FROM_CAPTURE,
        {1280, 720},      /* channel 0 */
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
        {1280, 720},      /* channel 0 */
        {ENC_INPUT_H2642D, 30, 200*1000,  30, 25, 51, 1 , FALSE, {0, 0, 320, 240}},   
        {SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 },          
        {JCS_yuv420, 0, JENC_INPUT_MP42D, 70}                                        
    }
};

ReproduceBitStream   user_rec_sub_ch_setting = 
{
    DVR_ENC_EBST_ENABLE,  //enabled
    1,  // sub1-bitstream
    ENC_TYPE_H264, //enc_type, 0: ENC_TYPE_H264, 1:ENC_TYPE_MPEG, 2:ENC_TYPE_MJPEG,
    FALSE,  // is_blocked
    DVR_ENC_EBST_DISABLE, // en_snapshot,
    {1280, 720},      /* channel 0 */
    {ENC_INPUT_H2642D, 30, 262144,  30, 25, 51, 1 , TRUE, {0, 0, 320, 240}},    //EncParam
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

    // sub1_bitstream ROI setting.
    user_rec_sub_ch_setting.enc.is_use_ROI=TRUE;
    user_rec_sub_ch_setting.enc.ROI_win.x = 0;
    user_rec_sub_ch_setting.enc.ROI_win.y = 0;
    user_rec_sub_ch_setting.enc.ROI_win.width = 320;
    user_rec_sub_ch_setting.enc.ROI_win.height = 240;
    memcpy(&sub_ch_param, &user_rec_sub_ch_setting, sizeof(sub_ch_param));  //sub1-roi
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
    enc_ctrl.stream = sub_ch_param.out_bs;
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    enc_ctrl.stream = 0; // 0: Stop for all main and sub 
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);

    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_param.src.channel);
    FN_SET_SUB1_REC_CH(&tag, ch_param.src.channel);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    munmap((void*)bs_buf, enc_buf_size);      
}

void do_roi_coordinate_win_update(int ch_num, int ROI_win_x, int ROI_win_y, int ROI_win_w, int ROI_win_h)
{
    dvr_enc_control  enc_update;    
    EncParam_Ext5 enc_param_ext = {0};
    int window_w, window_h;

    window_w = ROI_win_x + ROI_win_w;
    window_h = ROI_win_y + ROI_win_h;
    if((window_w > sub_ch_param.dim.width) || (window_h > sub_ch_param.dim.height)) {
        printf("%s: ROI window over! (x=%d, y=%d, width=%d, height=%d) (%dx%d).\n", __FUNCTION__, 
                            ROI_win_x,
                            ROI_win_y,
                            ROI_win_w,
                            ROI_win_h,
                            sub_ch_param.dim.width,
                            sub_ch_param.dim.height);
        return;                            
    }
    if(ROI_win_y%2) {
        printf("%s: <ROI_win_y=%d should be align at 2 pxls.>\n",__FUNCTION__, ROI_win_y);
        return;
    }
    printf("ROI_win_x=%d, ROI_win_y=%d \n", ROI_win_x, ROI_win_y);
    memset(&enc_update, 0x0, sizeof(dvr_enc_control));    

    enc_update.stream = 1;  /* sub1-bistream update */

    enc_update.update_parm.stream_enable = 1;
    enc_update.update_parm.frame_rate = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.bit_rate = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.ip_interval = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.dim.width = GMVAL_DO_NOT_CARE;  
    enc_update.update_parm.dim.height = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.di_mode = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.mode = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.scale_indep = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.is_3DI = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.is_denoise = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.init_quant = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.max_quant = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.min_quant = GMVAL_DO_NOT_CARE;

    enc_update.update_parm.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    enc_update.update_parm.pext_data = &enc_param_ext;

    enc_param_ext.target_rate_max = GMVAL_DO_NOT_CARE;
    enc_param_ext.reaction_delay_max = GMVAL_DO_NOT_CARE;
    enc_param_ext.enc_type = GMVAL_DO_NOT_CARE;
    enc_param_ext.MJ_quality = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_enable = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_interval = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_init_pattern = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_init_interval = GMVAL_DO_NOT_CARE;

    // ROI update
    enc_param_ext.feature_enable |= DVR_ENC_ROI_ALL;
    enc_param_ext.roi_all.is_use_ROI = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.width = ROI_win_w;
    enc_param_ext.roi_all.win.height = ROI_win_h;
    enc_param_ext.roi_all.win.x = ROI_win_x;
    enc_param_ext.roi_all.win.y = ROI_win_y;
    enc_update.command = ENC_UPDATE;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_update);

    FN_RESET_TAG(&tag);
    FN_SET_SUB1_REC_CH(&tag, ch_num); 
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);                          
}

void do_roi_enable_update(int ch_num, int is_use_ROI)
{
    dvr_enc_control  enc_update;    
    EncParam_Ext5 enc_param_ext = {0};

    printf("is_use_ROI=%d \n", is_use_ROI);
    memset(&enc_update, 0x0, sizeof(dvr_enc_control));    

    enc_update.stream = 1;  /* sub1-bistream update */

    enc_update.update_parm.stream_enable = 1;
    enc_update.update_parm.frame_rate = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.bit_rate = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.ip_interval = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.dim.width = GMVAL_DO_NOT_CARE;  
    enc_update.update_parm.dim.height = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.di_mode = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.mode = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.scale_indep = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.is_3DI = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.is_denoise = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.init_quant = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.max_quant = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.min_quant = GMVAL_DO_NOT_CARE;

    enc_update.update_parm.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    enc_update.update_parm.pext_data = &enc_param_ext;

    enc_param_ext.target_rate_max = GMVAL_DO_NOT_CARE;
    enc_param_ext.reaction_delay_max = GMVAL_DO_NOT_CARE;
    enc_param_ext.enc_type = GMVAL_DO_NOT_CARE;
    enc_param_ext.MJ_quality = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_enable = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_interval = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_init_pattern = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_init_interval = GMVAL_DO_NOT_CARE;
    
    // ROI update
    enc_param_ext.feature_enable |= DVR_ENC_ROI_ALL;
    enc_param_ext.roi_all.is_use_ROI = is_use_ROI;
    enc_param_ext.roi_all.win.width = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.height = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.x = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.y = GMVAL_DO_NOT_CARE;
    
    enc_update.command = ENC_UPDATE;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_update);

    FN_RESET_TAG(&tag);
    FN_SET_SUB1_REC_CH(&tag, ch_num); 
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);                          
}

void do_roi_coordinate_update(int ch_num, int ROI_win_x, int ROI_win_y)
{
    dvr_enc_control  enc_update;    
    EncParam_Ext5 enc_param_ext = {0};
    int window_w, window_h;

    window_w = ROI_win_x + sub_ch_param.enc.ROI_win.width;
    window_h = ROI_win_y + sub_ch_param.enc.ROI_win.height;
    if((window_w > sub_ch_param.dim.width) || (window_h > sub_ch_param.dim.height)) {
        printf("%s: ROI window over! (x=%d, y=%d, width=%d, height=%d) (%dx%d).\n", __FUNCTION__, 
                            ROI_win_x,
                            ROI_win_y,
                            sub_ch_param.enc.ROI_win.width,
                            sub_ch_param.enc.ROI_win.height,
                            sub_ch_param.dim.width,
                            sub_ch_param.dim.height);
        return;                            
    }
    if(ROI_win_y%2) {
        printf("%s: <ROI_win_y=%d should be align at 2 pxls.>\n",__FUNCTION__, ROI_win_y);
        return;
    }
    printf("ROI_win_x=%d, ROI_win_y=%d \n", ROI_win_x, ROI_win_y);
    memset(&enc_update, 0x0, sizeof(dvr_enc_control));    

    enc_update.stream = 1;  /* sub1-bistream update */

    enc_update.update_parm.stream_enable = 1;
    enc_update.update_parm.frame_rate = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.bit_rate = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.ip_interval = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.dim.width = GMVAL_DO_NOT_CARE;  
    enc_update.update_parm.dim.height = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.di_mode = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.mode = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.scale_indep = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.is_3DI = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.src.is_denoise = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.init_quant = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.max_quant = GMVAL_DO_NOT_CARE;
    enc_update.update_parm.min_quant = GMVAL_DO_NOT_CARE;

    enc_update.update_parm.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    enc_update.update_parm.pext_data = &enc_param_ext;

    enc_param_ext.target_rate_max = GMVAL_DO_NOT_CARE;
    enc_param_ext.reaction_delay_max = GMVAL_DO_NOT_CARE;
    enc_param_ext.enc_type = GMVAL_DO_NOT_CARE;
    enc_param_ext.MJ_quality = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_enable = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_interval = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_init_pattern = GMVAL_DO_NOT_CARE;
    enc_param_ext.watermark_init_interval = GMVAL_DO_NOT_CARE;

    // ROI update
    enc_param_ext.feature_enable |= DVR_ENC_ROI_ALL;
    enc_param_ext.roi_all.is_use_ROI = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.width = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.height = GMVAL_DO_NOT_CARE;
    enc_param_ext.roi_all.win.x = ROI_win_x;
    enc_param_ext.roi_all.win.y = ROI_win_y;
    
    enc_update.command = ENC_UPDATE;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_update);

    FN_RESET_TAG(&tag);
    FN_SET_SUB1_REC_CH(&tag, ch_num); 
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);                          
}

/**
 * @brief main function
 * @return 0 on success, !0 on error
 */
int main(int argc, char *argv[])
{
    int ret = 0, ch_num, sub_num = 1;    
    dvr_enc_queue_get   data;
    unsigned char *buf;
    int buf_size;          
    struct timeval t1,t2;  
    char tmp_str[128];
    int count = 0, ROI_win_x = 0, ROI_win_y = 0, ROI_win_w = 0, ROI_win_h = 0, is_use_ROI=1;
                    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
    ch_num = user_rec_ch_setting.src.channel;

    do_record_start();

    //main bitstream    
    sprintf(file_name, "CH%d_video_%d", ch_num, 0);      
    sprintf(tmp_str, "%s.h264", file_name);            
    rec_file = fopen ( tmp_str , "wb+" );

    //sub1 bitstream    
    sprintf(sub_rec_filename, "CH%d_Sub%d_Video_%03d", ch_num, sub_num, 001);
    sprintf(tmp_str, "%s.h264", sub_rec_filename);    
    rec_sub_bs_file = fopen ( tmp_str , "wb+" );
    
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
                buf = bs_buf + sub1_bs_buf_offset + data.bs.offset;
                buf_size = data.bs.length;
                if(data.new_bs == 1) {
                    fclose(rec_sub_bs_file);       
                    sprintf(sub_rec_filename, "CH%d_Sub%d_Video_%03d", ch_num, sub_num, count);
                    sprintf(tmp_str, "%s.h264", sub_rec_filename);    
                    rec_sub_bs_file = fopen ( tmp_str , "wb+" );
                    printf("%s:%d <file=%s>\n",__FUNCTION__,__LINE__, tmp_str);
                }
                
                fwrite (buf , 1 , buf_size , rec_sub_bs_file);
                fflush(rec_sub_bs_file);  
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
    
        if ((t2.tv_sec - t1.tv_sec) == 1) {      
            if(count == 5) {    //< update roi coordinate x, y, width and height
                ROI_win_x = ROI_win_y = 0; 
                ROI_win_w = 640;
                ROI_win_h = 480;
                do_roi_coordinate_win_update(ch_num, ROI_win_x, ROI_win_y, ROI_win_w, ROI_win_h);
            } else if (count == 10) {
                is_use_ROI = 0;     //< disable ROI
                do_roi_enable_update(ch_num, is_use_ROI);
            } else if (count == 15) {
                is_use_ROI = 1;     //< enable ROI
                do_roi_enable_update(ch_num, is_use_ROI);
            } else {  //< update roi coordinate x and y,
                if(is_use_ROI) {
                    do_roi_coordinate_update(ch_num, ROI_win_x, ROI_win_y);
                    ROI_win_x += ROI_COORDINATE_X_STEP;  
                    ROI_win_y += ROI_COORDINATE_Y_STEP;
                }
            }
            t1.tv_sec = t2.tv_sec;
            ++count;
        }       
        if (count >= 20) {      //< record for 10 seconds. then finish record.                 
            fclose(rec_file);       
            fclose(rec_sub_bs_file);       
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
