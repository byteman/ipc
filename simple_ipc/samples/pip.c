/**
 * this sample code implement liveview PIP function and update pip channel's window position & resolution. 
 * step :
 * push key "a" to start channel 2 as layer0, 
 * push key "x" to start channel 1 as layer1,
 * push key "1" to update channel 1's window position at (0, 0)
 * push key "2" to update channel 1's window position at (352, 0)
 * push key "3" to update channel 1's window position at (0, 240)
 * push key "4" to update channel 1's window position at (352, 240)
 * push key "s" to swap channel 1 as layer0, channel 2 as layer1
 * push key "5" to update channel 2's window position at (0, 0)
 * push key "6" to update channel 2's window position at (352, 0)
 * push key "7" to update channel 2's window position at (0, 240)
 * push key "8" to update channel 2's window position at (352, 240)
 * push key "b" to stop channel 2,
 * push key "y" to stop channel 1,
 * push key "q" to exit program,
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
#include <sys/mman.h>
#include <poll.h>

#include "dvr_common_api.h"
#include "dvr_disp_api.h"
#include "dvr_enc_api.h"

int dvr_fd = 0;
int disp_fd = 0;
int main_disp_no = 0;
int main_plane_id = 0;
int open_flag = 0;
int is_NTSC = TRUE;   //if FALSE, it's PAL

char getch(void)
{
    int n = 1;
    unsigned char ch;
    struct timeval tv;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    n = select(1, &rfds, NULL, NULL, &tv);
    if (n > 0) {
        n = read(0, &ch, 1);
        if (n == 1)
            return ch;
        return n;
    }
    return -1;
}

int setup_lv_channel(int ch_num, int cmd, int is_use_scaler, int di_mode, int mode, int is_3DI, int is_denoise, int dn_mode, DIM *src_dim, RECT *src_rect, RECT *dst_rect, int pip)
{
    int ret;
    dvr_disp_control    disp_ctrl;
    DispParam_Ext1  disp_param_ext = {0}; 

    memset(&disp_ctrl, 0x0, sizeof(dvr_disp_control));
    disp_ctrl.type = DISP_TYPE_LIVEVIEW;
    disp_ctrl.channel = ch_num;
    disp_ctrl.command = cmd;
    if(cmd != DISP_STOP)
    {
        disp_ctrl.src_param.lv.cap_path = ch_num;   
        disp_ctrl.src_param.lv.di_mode = di_mode;
        disp_ctrl.src_param.lv.mode = mode;
    
        disp_ctrl.src_param.lv.vp_param.is_3DI = is_3DI;        
        disp_ctrl.src_param.lv.vp_param.is_denoise = is_denoise;
        disp_ctrl.src_param.lv.vp_param.denoise_mode = dn_mode;

        disp_ctrl.src_param.lv.dma_order = DMAORDER_PACKET;
        disp_ctrl.src_param.lv.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
        disp_ctrl.src_param.lv.input_system = (is_NTSC)? MCP_VIDEO_NTSC: MCP_VIDEO_PAL;
        disp_ctrl.src_param.lv.cap_rate = (is_NTSC)? 30: 25;
        
        disp_ctrl.src_param.lv.color_mode = CAPCOLOR_YUV422;
        disp_ctrl.dst_param.lv.plane_id = main_plane_id;

        disp_ctrl.src_param.lv.is_use_scaler = is_use_scaler;
        disp_ctrl.src_param.lv.dim.width = src_dim->width;
        disp_ctrl.src_param.lv.dim.height = src_dim->height;
        disp_ctrl.src_param.lv.ext_size = DVR_DISP_MAGIC_ADD_VAL(sizeof(disp_param_ext));
        disp_ctrl.src_param.lv.pext_data = &disp_param_ext;
        
        if(pip == 1) {
            disp_param_ext.chn_type = DISP_LAYER1_CHN;                    
        }
        else {
            disp_param_ext.chn_type = DISP_LAYER0_CHN;            
        }
         
        if(is_use_scaler)
        {
            if(src_rect)
            {
                disp_ctrl.src_param.lv.win.x = src_rect->x;
                disp_ctrl.src_param.lv.win.y = src_rect->y;
                disp_ctrl.src_param.lv.win.width = src_rect->width;
                disp_ctrl.src_param.lv.win.height = src_rect->height;
            }
            else
            {
                disp_ctrl.src_param.lv.win.x = 0;
                disp_ctrl.src_param.lv.win.y = 0;
                disp_ctrl.src_param.lv.win.width = src_dim->width;
                disp_ctrl.src_param.lv.win.height = src_dim->height;
            }

            disp_ctrl.src_param.lv.scl_param.src_fmt = SCALE_YUV422;
            disp_ctrl.src_param.lv.scl_param.dst_fmt = SCALE_YUV422;
            disp_ctrl.src_param.lv.scl_param.scale_mode = SCALE_LINEAR;
            disp_ctrl.src_param.lv.scl_param.is_dither = FALSE;
            disp_ctrl.src_param.lv.scl_param.is_correction = FALSE;
            disp_ctrl.src_param.lv.scl_param.is_album = TRUE;
            disp_ctrl.src_param.lv.scl_param.des_level = 0;
        }
        else
        {
            disp_ctrl.src_param.lv.win.x = 0;
            disp_ctrl.src_param.lv.win.y = 0;
            disp_ctrl.src_param.lv.win.width = src_dim->width;
            disp_ctrl.src_param.lv.win.height = src_dim->height;
            
            disp_ctrl.src_param.lv.scl_param.src_fmt = SCALE_YUV422;
            disp_ctrl.src_param.lv.scl_param.dst_fmt = SCALE_YUV422;
            disp_ctrl.src_param.lv.scl_param.scale_mode = SCALE_LINEAR;
            disp_ctrl.src_param.lv.scl_param.is_dither = FALSE;
            disp_ctrl.src_param.lv.scl_param.is_correction = FALSE;
            disp_ctrl.src_param.lv.scl_param.is_album = TRUE;
            disp_ctrl.src_param.lv.scl_param.des_level = 0;
        }
        disp_ctrl.dst_param.lv.win.x = dst_rect->x;
        disp_ctrl.dst_param.lv.win.y = dst_rect->y;
        disp_ctrl.dst_param.lv.win.width = dst_rect->width;
        disp_ctrl.dst_param.lv.win.height = dst_rect->height;
    }

    ret = ioctl(disp_fd, DVR_DISP_CONTROL, &disp_ctrl);

    return ret;
}

int update_lv_pip_channel(int ch_num, RECT *dst_rect)
{
    int ret;
    dvr_disp_control    disp_ctrl;    
    FuncTag tag;

    memset(&disp_ctrl, 0x0, sizeof(dvr_disp_control));    
    
    disp_ctrl.channel = ch_num;
    disp_ctrl.command = DISP_UPDATE;
    //PIP mode does not support to update the following parameter
    disp_ctrl.src_param.lv.cap_path = GMVAL_DO_NOT_CARE;   
    disp_ctrl.src_param.lv.di_mode = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.mode = GMVAL_DO_NOT_CARE;
    
    disp_ctrl.src_param.lv.vp_param.is_3DI = GMVAL_DO_NOT_CARE;        
    disp_ctrl.src_param.lv.vp_param.is_denoise = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.vp_param.denoise_mode = GMVAL_DO_NOT_CARE;

    disp_ctrl.src_param.lv.dma_order = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scale_indep = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.input_system = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.cap_rate = GMVAL_DO_NOT_CARE;        
    disp_ctrl.src_param.lv.color_mode = GMVAL_DO_NOT_CARE;
    disp_ctrl.dst_param.lv.plane_id = GMVAL_DO_NOT_CARE;

    disp_ctrl.src_param.lv.is_use_scaler = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.dim.width = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.dim.height = GMVAL_DO_NOT_CARE;
                        
    disp_ctrl.src_param.lv.win.x = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.win.y = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.win.width = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.win.height = GMVAL_DO_NOT_CARE;        
    
    disp_ctrl.src_param.lv.scl_param.src_fmt = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scl_param.dst_fmt = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scl_param.scale_mode = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scl_param.is_dither = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scl_param.is_correction = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scl_param.is_album = GMVAL_DO_NOT_CARE;
    disp_ctrl.src_param.lv.scl_param.des_level = GMVAL_DO_NOT_CARE;
    // PIP mode only support to update the following parameter
    disp_ctrl.dst_param.lv.win.x = dst_rect->x;
    disp_ctrl.dst_param.lv.win.y = dst_rect->y;
    disp_ctrl.dst_param.lv.win.width = dst_rect->width;
    disp_ctrl.dst_param.lv.win.height = dst_rect->height;
    
    ret = ioctl(disp_fd, DVR_DISP_CONTROL, &disp_ctrl);
    
    FN_RESET_TAG(&tag);        
    FN_SET_LV_CH(&tag, ch_num);
    
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);

    return ret;
}

int run_lv_command(int ch_num)
{
    int ret;    
    FuncTag tag;

    FN_RESET_TAG(&tag);
    FN_SET_LV_CH(&tag, ch_num);
    
    ret = ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    if(ret<0)
        return -1;

    return ret;
}

int do_liveview_closech(int ch_num)
{
    int ret;

    ret = setup_lv_channel(ch_num, DISP_STOP, 0, 0, 0, FALSE, FALSE, 0, NULL, NULL, NULL, 0);    
    if(ret<0)
        return -1;

    ret = run_lv_command(ch_num);

    return ret;
}

int do_disp_startup()
{
    int i, ret;
    dvr_disp_disp_param         disp_param;
    dvr_disp_update_disp_param  disp_update_param;
    dvr_disp_plane_param        plane_param[3];
    dvr_disp_update_plane_param plane_update_pa;
    dvr_disp_control    dsp_ctl;
    
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
    ret = ioctl(disp_fd, DVR_DISP_GET_DISP_PARAM, &disp_param);            

    // set BG_AND_2PLANE, which means we need 1 background and another 2 planes
    disp_update_param.disp_num = main_disp_no;
    disp_update_param.param = DISP_PARAM_PLANE_COMBINATION;
    disp_update_param.val.plane_comb = BG_ONLY;//BG_AND_2PLANE;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    if (ret < 0)
        return -1;
    
    disp_update_param.disp_num = main_disp_no;
    disp_update_param.param = DISP_PARAM_OUTPUT_SYSTEM;

    disp_update_param.val.output_system = MCP_VIDEO_VGA;
    disp_update_param.val.display_rate = is_NTSC? 30 : 25;
    
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    if (ret < 0)
        return -1;

    disp_update_param.param = DISP_PARAM_APPLY;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    if (ret < 0)
        return -1;

    // query 3 planes information
    for(i = 0; i < 3; i++) {
        plane_param[i].disp_num = main_disp_no;
        plane_param[i].plane_num = i;
        ret = ioctl(disp_fd, DVR_DISP_GET_PLANE_PARAM, &plane_param[i]);
        if (ret < 0)
            return -1;
        
        if (i == 0)
            main_plane_id = plane_param[i].param.plane_id;
    }

    // set color mode for background plane
    plane_update_pa.plane_id = plane_param[0].param.plane_id;
    plane_update_pa.param = PLANE_PARAM_COLOR_MODE;
    plane_update_pa.val.color_mode = LCD_COLOR_YUV422;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_PLANE_PARAM, &plane_update_pa);
    if (ret < 0)
        return -1;

    // set data mode for background plane
    plane_update_pa.plane_id = plane_param[0].param.plane_id;
    plane_update_pa.param = PLANE_PARAM_DATA_MODE;
    plane_update_pa.val.data_mode = LCD_PROGRESSIVE;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_PLANE_PARAM, &plane_update_pa);
    if (ret < 0)
        return -1;

    plane_update_pa.param = PLANE_PARAM_APPLY;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_PLANE_PARAM, &plane_update_pa);
    if (ret < 0)
        return -1;

    return 0;
}

int do_disp_endup()
{
    if (!open_flag) {
        printf("Multi close\n");
        return -1;
    }           

    if (disp_fd > 0)
        close(disp_fd);
    
    disp_fd = 0;
    open_flag = 0;    
    
    return 0;
}

int open_dvr_common(void)
{
    dvr_fd = open("/dev/dvr_common", O_RDWR);
    if (dvr_fd < 0) {
        perror("Open [/dev/dvr_common] failed:");
        return -1;
    }
    return 0;
}

void close_dvr_common(void)
{
    close(dvr_fd);
}

int main(int argc, char *argv[])
{    
    int ret = 0;
    DIM dim;
    RECT win;        
    char key;                
        
    open_dvr_common();    
    
    do_disp_startup();
    
    while(1) 
    {
        key = getch();
        if (key == 'q' || key == 'Q') 
            break;
        switch(key)
        {            
            case 'x':   
                dim.width = 352;
                dim.height = 240;
                win.x = 352;
                win.y = 240;
                win.width = 352;
                win.height = 240;
                setup_lv_channel(1, DISP_START, 0, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE, FALSE, 0, &dim, NULL, &win, 1);                
                ret = run_lv_command(1);
                break;                                  
            case 'y':   
                ret = do_liveview_closech(1);  
                break;
            case 'a':   
                dim.width = 720;
                dim.height = 480;
                win.x = 0;
                win.y = 0;
                win.width = 720;
                win.height = 480;
                setup_lv_channel(2, DISP_START, 0, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE, FALSE, 0, &dim, NULL, &win, 0);
                ret = run_lv_command(2);
                break;                                  
            case 'b':   
                ret = do_liveview_closech(2);  
                break;    
            case '1':                   
                win.x = 0;
                win.y = 0;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(1, &win);                
                break;    
            case '2':                   
                win.x = 352;
                win.y = 0;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(1, &win);    
                break;
            case '3':                   
                win.x = 0;
                win.y = 240;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(1, &win);    
                break;    
            case '4':                   
                win.x = 352;
                win.y = 240;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(1, &win);    
                break;        
            case '5':                   
                win.x = 0;
                win.y = 0;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(2, &win);                
                break;    
            case '6':                   
                win.x = 352;
                win.y = 0;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(2, &win);    
                break;
            case '7':                   
                win.x = 0;
                win.y = 240;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(2, &win);    
                break;    
            case '8':                   
                win.x = 352;
                win.y = 240;
                win.width = 352;
                win.height = 240;
                ret = update_lv_pip_channel(2, &win);    
                break;        
            case 's':
                do_liveview_closech(1);  
                do_liveview_closech(2);  
                dim.width = 352;
                dim.height = 240;
                win.x = 0;
                win.y = 0;
                win.width = 720;
                win.height = 480;
                setup_lv_channel(1, DISP_START, 1, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE, FALSE, 0, &dim, NULL, &win, 0);     
                ret = run_lv_command(1);
                
                dim.width = 352;
                dim.height = 240;
                win.x = 352;
                win.y = 240;
                win.width = 352;
                win.height = 240;
                setup_lv_channel(2, DISP_START, 0, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE, FALSE, 0, &dim, NULL, &win, 1);        
                ret = run_lv_command(2);
                break;
        }
    }   
       
    do_disp_endup();
    
    close_dvr_common();
    printf("finish\n");
    return 0;
}
