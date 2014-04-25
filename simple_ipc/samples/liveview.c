/**
 * this sample code implement liveview function, and liveview for 20 seconds.
 * 
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "dvr_common_api.h"
#include "dvr_disp_api.h"

int dvr_fd = 0;
int disp_fd = 0;
int main_disp_no = 0;
int main_plane_id = 0;
int open_flag = 0;

int main(int argc, char *argv[])
{
    int fd, i ;    
    char isp_file[]="dev/isp";
    dvr_disp_disp_param disp_param;
    dvr_disp_update_disp_param disp_update_param;
    dvr_disp_plane_param plane_param[3];
    dvr_disp_update_plane_param plane_update_pa;
    dvr_disp_control dsp_ctl;
    dvr_disp_control disp_ctrl;
    DIM dim;
    RECT win;    
    FuncTag tag;
    struct timeval t1,t2;  
    gettimeofday(&t1, NULL);        
    int ch_num = 2;         
    
    fd = open(isp_file, O_RDWR);
    dvr_fd = open("/dev/dvr_common", O_RDWR);
    //do_disp_startup
    open_flag = 1;
    disp_fd = open("/dev/dvr_disp", O_RDWR);
    memset(&disp_param, 0x0, sizeof(dvr_disp_disp_param));
    memset(&disp_update_param, 0x0, sizeof(dvr_disp_update_disp_param));
    memset(plane_param, 0x0, sizeof(dvr_disp_plane_param) * 3);
    memset(&plane_update_pa, 0x0, sizeof(dvr_disp_update_plane_param));
    memset(&dsp_ctl, 0x0, sizeof(dvr_disp_control));
    
    main_disp_no = 0;
    // query LCD1 information
    disp_param.disp_num = main_disp_no;
    ioctl(disp_fd, DVR_DISP_GET_DISP_PARAM, &disp_param);
    
    usleep(100000);
    
    disp_update_param.disp_num = main_disp_no;
    disp_update_param.param = DISP_PARAM_PLANE_COMBINATION;
    disp_update_param.val.plane_comb = BG_ONLY; //BG_AND_2PLANE;
    ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    
    disp_update_param.disp_num = main_disp_no;
    disp_update_param.param = DISP_PARAM_OUTPUT_SYSTEM;
    disp_update_param.val.output_system = MCP_VIDEO_VGA;
    disp_update_param.val.display_rate = 30;
    ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    
    disp_update_param.param = DISP_PARAM_APPLY;
    ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    
    // query 3 planes information
    for (i = 0; i < 3; i++) {
        plane_param[i].disp_num = main_disp_no;
        plane_param[i].plane_num = i;
        ioctl(disp_fd, DVR_DISP_GET_PLANE_PARAM, &plane_param[i]);
        
        usleep(50000);
        if (i == 0)
            main_plane_id = plane_param[i].param.plane_id;
    }
    
    // set color mode for background plane
    plane_update_pa.plane_id = plane_param[0].param.plane_id;
    plane_update_pa.param = PLANE_PARAM_COLOR_MODE;
    plane_update_pa.val.color_mode = LCD_COLOR_YUV422;
    ioctl(disp_fd, DVR_DISP_UPDATE_PLANE_PARAM, &plane_update_pa);    

    // set data mode for background plane
    plane_update_pa.plane_id = plane_param[0].param.plane_id;
    plane_update_pa.param = PLANE_PARAM_DATA_MODE;
    plane_update_pa.val.data_mode = LCD_PROGRESSIVE;
    ioctl(disp_fd, DVR_DISP_UPDATE_PLANE_PARAM, &plane_update_pa);
    
    plane_update_pa.param = PLANE_PARAM_APPLY;
    ioctl(disp_fd, DVR_DISP_UPDATE_PLANE_PARAM, &plane_update_pa);
    //do_liveview_t1
    dim.width = 640;
    dim.height = 360;
    win.x = 0;
    win.y = 0;
    win.width = 640;
    win.height = 360;
    //setup_lv_channel    
    memset(&disp_ctrl, 0x0, sizeof(dvr_disp_control));
    disp_ctrl.type = DISP_TYPE_LIVEVIEW;

    disp_ctrl.channel = ch_num;

    disp_ctrl.command = DISP_START;
    disp_ctrl.src_param.lv.cap_path = ch_num;   
    disp_ctrl.src_param.lv.di_mode = LVFRAME_WEAVED_TWO_FIELDS;
    disp_ctrl.src_param.lv.mode = LVFRAME_FRAME_MODE;
    disp_ctrl.src_param.lv.vp_param.is_3DI = FALSE;
    disp_ctrl.src_param.lv.vp_param.is_denoise = FALSE;
    disp_ctrl.src_param.lv.vp_param.denoise_mode = 0;
    disp_ctrl.src_param.lv.dma_order = DMAORDER_PACKET;
    disp_ctrl.src_param.lv.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
    disp_ctrl.src_param.lv.input_system = MCP_VIDEO_NTSC;
    disp_ctrl.src_param.lv.cap_rate = 30 ;
    disp_ctrl.src_param.lv.color_mode = CAPCOLOR_YUV422;
    disp_ctrl.dst_param.lv.plane_id = main_plane_id;
    disp_ctrl.src_param.lv.is_use_scaler = 0;
    disp_ctrl.dst_param.lv.win.x = win.x;
    disp_ctrl.dst_param.lv.win.y = win.y;
    disp_ctrl.dst_param.lv.win.width = win.width;
    disp_ctrl.dst_param.lv.win.height = win.height;
    
    ioctl(disp_fd, DVR_DISP_CONTROL, &disp_ctrl);
    
    //run_lv_command
    FN_RESET_TAG(&tag);
    for (i = 0; i < 16; i++) {
        FN_SET_LV_CH(&tag, i);
        FN_SET_PB_CH(&tag, i);
    }    
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    
    gettimeofday(&t2, NULL);        
    //  set 20 seconds to do liveview        
    while(t2.tv_sec - t1.tv_sec < 20){
        gettimeofday(&t2, NULL);        
    }
    
    //do_liveview_close1ch
    memset(&disp_ctrl, 0x0, sizeof(dvr_disp_control));
    disp_ctrl.type = DISP_TYPE_LIVEVIEW;

    disp_ctrl.channel = ch_num;

    disp_ctrl.command = DISP_STOP;
    ioctl(disp_fd, DVR_DISP_CONTROL, &disp_ctrl);
    
    //run_lv_command
    FN_RESET_TAG(&tag);
    for (i = 0; i < 16; i++) {
        FN_SET_LV_CH(&tag, i);
        FN_SET_PB_CH(&tag, i);
    }
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    
    close(disp_fd);
    disp_fd = 0;
    open_flag = 0;
    
    close(dvr_fd);
    printf("finish\n");
    return 0;
}
