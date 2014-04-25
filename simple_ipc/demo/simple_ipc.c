#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "dvr_common_api.h"
#include "dvr_disp_api.h"
#include "file_cfg.h"

#define USE_UPDATE_METHOD   1
int dvr_fd = 0;
int disp_fd = 0;
int main_disp_no = 0;
int main_plane_id = 0;
int is_NTSC = TRUE;             //if FALSE, it's PAL
int open_flag = 0;
extern int osd_mask(void);
extern int osd_display(void);
extern void rtsp_test(void);
extern int is_bs_all_disable(void);
extern int init_isp(void);
extern void close_isp(void);

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

int setup_lv_channel(int ch_num, int cmd, int is_use_scaler, int di_mode, int mode, int is_3DI,
                       int is_denoise, int dn_mode, DIM * src_dim, RECT * src_rect, RECT * dst_rect)
{
    int ret;
    dvr_disp_control disp_ctrl;
    memset(&disp_ctrl, 0x0, sizeof(dvr_disp_control));
    disp_ctrl.type = DISP_TYPE_LIVEVIEW;
    disp_ctrl.channel = ch_num;
    disp_ctrl.command = cmd;
    if (cmd != DISP_STOP) {
        printf("ch(%d) cmd(%d) scl(%d) di(%d,%d) wh(%d,%d) rect(%d,%d,%d,%d)\n", 
                    ch_num, 
                    cmd,
                    is_use_scaler, 
                    di_mode, mode, 
                    src_dim->width, 
                    src_dim->height, 
                    dst_rect->x,
                    dst_rect->y, 
                    dst_rect->width, 
                    dst_rect->height);
        
        disp_ctrl.src_param.lv.di_mode = di_mode;
        disp_ctrl.src_param.lv.mode = mode;
        disp_ctrl.src_param.lv.vp_param.is_3DI = is_3DI;
        disp_ctrl.src_param.lv.vp_param.is_denoise = is_denoise;
        disp_ctrl.src_param.lv.vp_param.denoise_mode = dn_mode;
        disp_ctrl.src_param.lv.dma_order = DMAORDER_PACKET;
        disp_ctrl.src_param.lv.scale_indep = CAPSCALER_KEEP_RATIO;
        disp_ctrl.src_param.lv.input_system = (is_NTSC) ? MCP_VIDEO_NTSC : MCP_VIDEO_PAL;
        disp_ctrl.src_param.lv.cap_rate = (is_NTSC) ? 30 : 25;
        disp_ctrl.src_param.lv.color_mode = CAPCOLOR_YUV422;
        disp_ctrl.dst_param.lv.plane_id = main_plane_id;
        disp_ctrl.src_param.lv.is_use_scaler = is_use_scaler;
        if (is_use_scaler) {
            disp_ctrl.src_param.lv.dim.width = src_dim->width;
            disp_ctrl.src_param.lv.dim.height = src_dim->height;
            if (src_rect) {
                disp_ctrl.src_param.lv.win.x = src_rect->x;
                disp_ctrl.src_param.lv.win.y = src_rect->y;
                disp_ctrl.src_param.lv.win.width = src_rect->width;
                disp_ctrl.src_param.lv.win.height = src_rect->height;
            } else {
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
        disp_ctrl.dst_param.lv.win.x = dst_rect->x;
        disp_ctrl.dst_param.lv.win.y = dst_rect->y;
        disp_ctrl.dst_param.lv.win.width = dst_rect->width;
        disp_ctrl.dst_param.lv.win.height = dst_rect->height;
    }
    ret = ioctl(disp_fd, DVR_DISP_CONTROL, &disp_ctrl);
    return ret;
}

int run_lv_command(void)
{
    int ret, i;

    FuncTag tag;
    FN_RESET_TAG(&tag);
    for (i = 0; i < 16; i++) {
        FN_SET_LV_CH(&tag, i);
        FN_SET_PB_CH(&tag, i);
    }
    ret = ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    if (ret < 0)
        return -1;
    return ret;
}

int do_disp_startup()
{
    int i, ret;
    dvr_disp_disp_param disp_param;
    dvr_disp_update_disp_param disp_update_param;
    dvr_disp_plane_param plane_param[3];
    dvr_disp_update_plane_param plane_update_pa;
    dvr_disp_control dsp_ctl;
    if (open_flag) {
        printf("multi open\n");
        return -1;
    }
    open_flag = 1;
    disp_fd = open("/dev/dvr_disp", O_RDWR);
    if (disp_fd < 0) {
        perror("Open failed:");
        open_flag = 0;
        return -1;
    }
    memset(&disp_param, 0x0, sizeof(dvr_disp_disp_param));
    memset(&disp_update_param, 0x0, sizeof(dvr_disp_update_disp_param));
    memset(plane_param, 0x0, sizeof(dvr_disp_plane_param) * 3);
    memset(&plane_update_pa, 0x0, sizeof(dvr_disp_update_plane_param));
    memset(&dsp_ctl, 0x0, sizeof(dvr_disp_control));

    // initialize
    ret = ioctl(disp_fd, DVR_DISP_INITIATE, 0);
    if (ret < 0)
        return -1;
    main_disp_no = 0;

    // query LCD1 information
    disp_param.disp_num = main_disp_no;
    ret = ioctl(disp_fd, DVR_DISP_GET_DISP_PARAM, &disp_param);
    if (ret < 0)
        return -1;
    printf("LCD(%d): target(%d) dim(%d-%d) res(in=%d,out=%d) plane_comb(%d) system(%d) mode(%d)\n",
            1, 
            disp_param.target_id, 
            disp_param.dim.width, 
            disp_param.dim.height,
            disp_param.res.input_res, 
            disp_param.res.output_type, 
            disp_param.plane_comb,
            disp_param.output_system, 
            disp_param.output_mode);
    printf("       : color attrib.: br(%d)sa(%d)co(%d)-hus(%d)huc(%d)sh0(%d)sh1-(%d)shth0(%d)shth1(%d)\n",
            disp_param.color_attrib.brightness, 
            disp_param.color_attrib.saturation,
            disp_param.color_attrib.contrast, 
            disp_param.color_attrib.huesin,
            disp_param.color_attrib.huecos, 
            disp_param.color_attrib.sharpnessk0,
            disp_param.color_attrib.sharpnessk1, 
            disp_param.color_attrib.sharpness_thres0,
            disp_param.color_attrib.shaprness_thres1);
    printf("       : transparent: color1(%d,%d) color2(%d,%d)\n",
            disp_param.transparent_color[0].is_enable, 
            disp_param.transparent_color[0].color,
            disp_param.transparent_color[1].is_enable, 
            disp_param.transparent_color[1].color);
    printf("       : vbi info: lineno(%d) lineheight(%d) fb_offset(%d) fb_size(%d)\n",
            disp_param.vbi_info.lineno, 
            disp_param.vbi_info.lineheight,
            disp_param.vbi_info.fb_offset, 
            disp_param.vbi_info.fb_size);
    printf("       : lcd scaler: enable(%d) WH(%d-%d)\n", 
            disp_param.scl_info.is_enable,
            disp_param.scl_info.dim.width, 
            disp_param.scl_info.dim.height);
    usleep(100000);

#if USE_UPDATE_METHOD
    // set BG_AND_2PLANE, which means we need 1 background and another 2 planes
    disp_update_param.disp_num = main_disp_no;
    disp_update_param.param = DISP_PARAM_PLANE_COMBINATION;
    disp_update_param.val.plane_comb = BG_ONLY; //BG_AND_2PLANE;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    if (ret < 0)
        return -1;
    disp_update_param.disp_num = main_disp_no;
    disp_update_param.param = DISP_PARAM_OUTPUT_SYSTEM;
    disp_update_param.val.output_system = MCP_VIDEO_VGA;
    disp_update_param.val.display_rate = is_NTSC ? 30 : 25;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    if (ret < 0)
        return -1;
    disp_update_param.param = DISP_PARAM_APPLY;
    ret = ioctl(disp_fd, DVR_DISP_UPDATE_DISP_PARAM, &disp_update_param);
    if (ret < 0)
        return -1;

#else /*  */
    // set BG_AND_2PLANE, which means we need 1 background and another 2 planes
    disp_param.plane_comb = DISP_PARAM_PLANE_COMBINATION;
    ret = ioctl(disp_fd, DVR_DISP_SET_DISP_PARAM, &disp_param);
    if (ret < 0)
        return -1;

#endif /*  */

    // query 3 planes information
    for (i = 0; i < 3; i++) {
        plane_param[i].disp_num = main_disp_no;
        plane_param[i].plane_num = i;
        ret = ioctl(disp_fd, DVR_DISP_GET_PLANE_PARAM, &plane_param[i]);
        if (ret < 0)
            return -1;
        printf("LCD(%d)-plane(%d): ID(%d) rect(%d-%d,%d-%d) data_mode(%d) color_mode(%d)\n",
               main_disp_no, plane_param[i].plane_num, plane_param[i].param.plane_id,
               plane_param[i].param.win.x, plane_param[i].param.win.y,
               plane_param[i].param.win.width, plane_param[i].param.win.height,
               plane_param[i].param.data_mode, plane_param[i].param.color_mode);
        usleep(50000);
        if (i == 0)
            main_plane_id = plane_param[i].param.plane_id;
    }

#if USE_UPDATE_METHOD
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

#else /* USE_UPDATE_METHOD */
    plane_param[0].param.color_mode = LCD_COLOR_YUV422;
    plane_param[0].param.data_mode = LCD_PROGRESSIVE;
    ret = ioctl(disp_fd, DVR_DISP_SET_PLANE_PARAM, &plane_param[0]);
    if (ret < 0)
        return -1;
    ret = ioctl(disp_fd, DVR_DISP_SET_PLANE_PARAM, &plane_param[1]);
    if (ret < 0)
        return -1;

#endif /* USE_UPDATE_METHOD */
    return 0;
}

int do_disp_endup()
{
    int ret;
    if (!open_flag) {
        printf("Multi close\n");
        return -1;
    }
    ret = ioctl(disp_fd, DVR_DISP_TERMINATE, 0);
    if (ret < 0)
        return -1;
    if (disp_fd > 0)
        close(disp_fd);
    disp_fd = 0;
    open_flag = 0;
    return 0;
}

int do_liveview_t1()
{
    int ret;

    //dvr_disp_control    disp_ctrl;
    DIM dim;
    RECT win;
    if (!open_flag) {
        printf("Must select 0: Start up first\n");
        return -1;
    }
    dim.width = 720;
    dim.height = 480;
    win.x = 0;
    win.y = 0;
    win.width = 640;
    win.height = 360;
    ret =
        setup_lv_channel(2, DISP_START, FALSE, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE,
                         FALSE, 0, &dim, NULL, &win);
    if (ret < 0)
        return -1;

//++ fullhd
#if 0
    win.x = 360;
    win.y = 0;
    win.width = 360;
    win.height = 240;
    ret =
        setup_lv_channel(1, DISP_START, FALSE, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE,
                         FALSE, 0, &dim, NULL, &win);
    if (ret < 0)
        return -1;
    win.x = 0;
    win.y = 240;
    win.width = 360;
    win.height = 240;
    ret =
        setup_lv_channel(2, DISP_START, FALSE, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE,
                         FALSE, 0, &dim, NULL, &win);
    if (ret < 0)
        return -1;
    win.x = 360;
    win.y = 240;
    win.width = 360;
    win.height = 240;
    ret =
        setup_lv_channel(3, DISP_START, FALSE, LVFRAME_WEAVED_TWO_FIELDS, LVFRAME_FRAME_MODE, FALSE,
                         FALSE, 0, &dim, NULL, &win);
    if (ret < 0)
        return -1;

#endif /*  */
//-- fullhd
    ret = run_lv_command();
    return ret;
}

int do_liveview_close1ch(void)
{
    int ret;
    ret = setup_lv_channel(2, DISP_STOP, 0, 0, 0, FALSE, FALSE, 0, NULL, NULL, NULL);
    if (ret < 0)
        return -1;
    ret = run_lv_command();
    return ret;
}

void show_system_test_menu(void)
{

    printf("--------------<Live View>------------\n");
    printf(" 1: LV Start \n");
    printf(" 2: LV Stop \n");
    printf("----------------<rtsp>---------------\n");
    printf(" r: RTSP Test\n");
    printf("-------------------------------------\n");
    printf(" Q: back to main menu \n");
    printf("-------------------------------------\n");
}

void system_test()
{
    int ret = 0;
    char key;

    show_system_test_menu();
    open_dvr_common();
    init_isp();
    while (1) {
        key = getch();
        if (key == 'q' || key == 'Q') {
            if(is_bs_all_disable()) {
                break;
            } else {
                printf("Please stop RTSP Test.\n");
            }
        }
        switch (key) {
        case '1':
            ret = do_disp_startup();
            if (ret < 0)
                return;
            ret = do_liveview_t1();
            if (ret < 0)
                return;
            break;
        case '2':
            ret = do_liveview_close1ch();
            if (ret < 0)
                return;
            ret = do_disp_endup();
            if (ret < 0)
                return;
            break;
        case 'm':
            osd_mask();
            break;
        case 'o':
            osd_display();
            break;
        case 'r':
            rtsp_test();
            break;
        case 'p':
            show_system_test_menu();
            break;
        default:
            continue;
        }
        if (ret < 0) {
            printf("333something wrong!!\n");
        }
        show_system_test_menu();
    }
    close_dvr_common();
    close_isp();
}

int main(int argc, char *argv[])
{
    system_test();
    return 0;
}
