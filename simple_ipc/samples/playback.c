/**
 * this sample code implement playback function, and playback for 20 seconds. 
 * demo file is CH0_video_0.avi, 
 * please put the demo file at the same directory with executed binary file.
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
#include "dvr_disp_api.h"
#include "dvr_enc_api.h"
#include "dvr_dec_api.h"
#include "gmavi_api.h"

int dvr_fd = 0;
int disp_fd = 0;
int dec_fd = 0 ;
int main_disp_no = 0;
int main_plane_id = 0;
int open_flag = 0;
int is_NTSC = TRUE;  

#define FUNCTION_NULL       -1
#define FUNCTION_NORMAL     0

int menu_func = FUNCTION_NULL;
#define USE_UPDATE_METHOD   1

unsigned char *pbbs_buf;
int dec_buf_size;
HANDLE  pb_file= NULL;
AviMainHeader pb_main_header;
AviStreamHeader pb_stream_header;
GmAviStreamFormat pb_stream_format;
int stream_id_pkt = 0;
struct pollfd dec_fds;
int is_st_vga = TRUE;

RECT D1_4CH[4] = {
        {  0,  0, 352,240},    {352,  0, 352,240},
        {  0,240, 352,240},    {352,240, 352,240}
        };
        
RECT VGA_4CH[4] = {
        {  0,  0, 720,480},    {0,  0, 640,360},
        {  0,  0, 352,240},    {0,  0, 640,480}
        };        

int do_disp_startup()
{
    int i, ret;
    dvr_disp_disp_param         disp_param;
    dvr_disp_update_disp_param  disp_update_param;
    dvr_disp_plane_param        plane_param[3];
    dvr_disp_update_plane_param plane_update_pa;
    dvr_disp_control    dsp_ctl;

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
    
    main_disp_no = 0;

    // query LCD1 information
    disp_param.disp_num = main_disp_no;
    ret = ioctl(disp_fd, DVR_DISP_GET_DISP_PARAM, &disp_param);
    if (ret < 0)
        return -1;
    printf("LCD(%d): dim(%d-%d) res(in=%d,out=%d) plane_comb(%d) system(%d) mode(%d)\n",
        1, disp_param.dim.width, disp_param.dim.height, disp_param.res.input_res, disp_param.res.output_type,
        disp_param.plane_comb, disp_param.output_system, disp_param.output_mode);
    printf("       : color attrib.: br(%d)sa(%d)co(%d)-hus(%d)huc(%d)sh0(%d)sh1-(%d)shth0(%d)shth1(%d)\n",
        disp_param.color_attrib.brightness, disp_param.color_attrib.saturation, disp_param.color_attrib.contrast,
        disp_param.color_attrib.huesin, disp_param.color_attrib.huecos, disp_param.color_attrib.sharpnessk0,
        disp_param.color_attrib.sharpnessk1, disp_param.color_attrib.sharpness_thres0, disp_param.color_attrib.shaprness_thres1);
    printf("       : transparent: color1(%d,%d) color2(%d,%d)\n",
        disp_param.transparent_color[0].is_enable, disp_param.transparent_color[0].color,
        disp_param.transparent_color[1].is_enable, disp_param.transparent_color[1].color);
    usleep(100000);

#if USE_UPDATE_METHOD
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
#else
    // set BG_AND_2PLANE, which means we need 1 background and another 2 planes
    disp_param.plane_comb = DISP_PARAM_PLANE_COMBINATION;
    ret = ioctl(disp_fd, DVR_DISP_SET_DISP_PARAM, &disp_param);
    if (ret < 0)
        return -1;
#endif
    // query 3 planes information
    for(i = 0; i < 3; i++) {
        plane_param[i].disp_num = main_disp_no;
        plane_param[i].plane_num = i;
        ret = ioctl(disp_fd, DVR_DISP_GET_PLANE_PARAM, &plane_param[i]);
        if (ret < 0)
            return -1;
        printf("LCD(%d)-plane(%d): ID(%d) rect(%d-%d,%d-%d) data_mode(%d) color_mode(%d)\n",
            main_disp_no, plane_param[i].plane_num, plane_param[i].param.plane_id, 
            plane_param[i].param.win.x, plane_param[i].param.win.y, plane_param[i].param.win.width, plane_param[i].param.win.height,
            plane_param[i].param.data_mode, plane_param[i].param.color_mode
            );
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

int open_pb_files(int ch_num, int index)
{
    int strh_count;
    char tmp_str[64];

    sprintf(tmp_str, "CH%d_video_%d.avi", ch_num, index);

    pb_file = GMAVIOpen(tmp_str, GMAVI_FILEMODE_READ, 0);
    if (!pb_file) {
        printf("Open [%s] failed!\n", tmp_str);
        return -1;
    }
    GMAVIGetAviMainHeader(pb_file, &pb_main_header);
    GMAVIGetStreamHeaderNum(pb_file, &strh_count);
    GMAVIGetStreamHeader(pb_file, 1/*get stream1*/, &pb_stream_header, &pb_stream_format, &stream_id_pkt);
    printf("dec type:<%c%c%c%c>\n",pb_stream_header.fccHandler[0]
                                ,pb_stream_header.fccHandler[1]
                                ,pb_stream_header.fccHandler[2]
                                ,pb_stream_header.fccHandler[3]);

    return 0;
}

void close_pb_files(int ch_num)
{    
    if(pb_file) {
        GMAVIClose(pb_file);
        pb_file = NULL;
    }
}

#define MAX_RECORD 4096
void packet_reader(int arg)
{
    int ret, ch_num;    
    int pb_idx = 0, pb_total = 0;    
    int pb_offset[MAX_RECORD] = {0};
    int reverse;
    dvr_enc_queue_get   data;
    unsigned char *buf;
    int buf_size = 0,intra = 0, backup_data = 0;    
    unsigned int i = 0;
    int file_idx = 1;
    dvr_dec_control    dec_ctrl;
    FuncTag tag;  
    struct timeval t1,t2;  

    memset(&dec_ctrl, 0x0, sizeof(dvr_dec_control));

    ch_num = arg;

    // prepare to select(or poll)
    dec_fds.fd = dec_fd;
    dec_fds.events = POLLIN;
    dec_fds.revents = 0;
  
    gettimeofday(&t1, NULL);        
  
    while(1) {
        gettimeofday(&t2, NULL);
    
        if ((t2.tv_sec - t1.tv_sec) == 20) {      //<playback for 20 seconds. then finish playback 
            break;               
        }     
        
        ret = poll(&dec_fds, 1, 2000);
        
        //(Dual8181) make request for playback data, then will call DVR_DEC_QUEUE_GET and DVR_DEC_QUEUE_PUT from message handler        
        ret = ioctl(dec_fd, DVR_DEC_QUEUE_GET, &data);
        backup_data = data.bs.length;
        if (ret < 0) {
            printf("buffer is not ready...\n");
            usleep(10000);
            continue;
        }
        
        buf = pbbs_buf + data.bs.offset;        
        
        // read bs(start)
        buf_size = backup_data;        
        reverse = 0;
        ret = GMAVIGetStreamDataAndIndex(pb_file, &stream_id_pkt, 
                buf, &buf_size, &intra, NULL, 0, i, reverse, &pb_offset[pb_idx]);
        if((intra == 1) && (pb_idx<MAX_RECORD))
            pb_total = (pb_idx++);       
                
        if (ret == GMSTS_END_OF_DATA) {
            printf("CH(%d,%d) - End of Playback!\n", ch_num, file_idx);
        
OPEN_PB_FILE_AGAIN:
            close_pb_files(ch_num);
            ret = open_pb_files(ch_num, file_idx++);
            if (ret < 0) {
                file_idx = 0;
                goto OPEN_PB_FILE_AGAIN;
            }
            GMAVISeek(pb_file, GMAVI_SEEK_TO_BEGINNING, NULL);

            //return job
            data.bs.length = 0;
            ret = ioctl(dec_fd, DVR_DEC_QUEUE_PUT, &data);
            if (ret < 0)
                printf("put failed when EOF...\n");

            dec_ctrl.command = DEC_UPDATE;
            dec_ctrl.src_param.dim.width = pb_main_header.dwWidth;
            dec_ctrl.src_param.dim.height = pb_main_header.dwHeight;
            dec_ctrl.src_param.win.x = 0;
            dec_ctrl.src_param.win.y = 0;
            dec_ctrl.src_param.win.width = pb_main_header.dwWidth;
            dec_ctrl.src_param.win.height = pb_main_header.dwHeight;
            dec_ctrl.src_param.bs_rate = (int) (1000000/(pb_main_header.dwMicroSecPerFrame));
            dec_ctrl.dst_param.plane_id = GMVAL_DO_NOT_CARE;
            dec_ctrl.dst_param.win.x = GMVAL_DO_NOT_CARE;
            dec_ctrl.dst_param.win.y = GMVAL_DO_NOT_CARE;
            dec_ctrl.dst_param.win.width = GMVAL_DO_NOT_CARE;
            dec_ctrl.dst_param.win.height = GMVAL_DO_NOT_CARE;
            dec_ctrl.dst_param.is_display = GMVAL_DO_NOT_CARE;
            dec_ctrl.dst_param.display_rate = GMVAL_DO_NOT_CARE;
           
            ret = ioctl(dec_fd, DVR_DEC_CONTROL, &dec_ctrl);
            if (ret < 0)
                printf("can't update playback parameters!");

            FN_RESET_TAG(&tag);
            FN_SET_PB_CH(&tag, ch_num);
            ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
            buf_size = 0;
            continue;
        }
        else if (ret < 0) 
            printf("CH(%d) - read error!\n", ch_num);

        data.bs.length = buf_size;        
        // read bs(end)
        ret = ioctl(dec_fd, DVR_DEC_QUEUE_PUT, &data);
        if (ret < 0) 
            printf("put failed...\n");                     
    }       
    
}

int do_pb_init(int ch_num)
{
    int ret;
    dvr_dec_channel_param   ch_param;

    memset(&ch_param, 0x0, sizeof(dvr_dec_channel_param));

    ret = open_pb_files(ch_num, 0);
    if (ret < 0)
        goto DO_PB_INIT_FAILED;    

    dec_fd = open("/dev/dvr_dec", O_RDWR);
    if (dec_fd < 0) {
        perror("Open [/dev/dvr_dec] failed:");
        goto DO_PB_INIT_FAILED;
    }
    
    switch(*(int *)pb_stream_header.fccHandler) {    
        case GMAVI_TYPE_H264: 
            ch_param.dec_type = ENC_TYPE_H264; 
            break;
        case GMAVI_TYPE_MPEG4:             
            ch_param.dec_type = ENC_TYPE_MPEG; 
            break;
        case GMAVI_TYPE_MJPEG: 
            ch_param.dec_type = ENC_TYPE_MJPEG;             
            break;
        default: 
            printf("%s:%d <dec_type err. %d>\n",__FUNCTION__,__LINE__, ch_param.dec_type);      
        return -1; 
    }

    ch_param.channel = ch_num; 
    ch_param.is_use_scaler = 1;
    ch_param.dec_param.output_type = DEC_OUTPUT_COLOR_YUV422;
    ch_param.scl_param.src_fmt = SCALE_YUV422;
    ch_param.scl_param.dst_fmt = SCALE_YUV422;
    ch_param.scl_param.scale_mode = SCALE_LINEAR;
    ch_param.scl_param.is_dither = 0;
    ch_param.scl_param.is_correction = 0;
    ch_param.scl_param.is_album = 1;
    ch_param.scl_param.des_level = 0;

    ret = ioctl(dec_fd, DVR_DEC_SET_CHANNEL_PARAM, &ch_param);
    if (ret < 0) {
        perror("Decoder DVR_DEC_SET_CHANNLE_PARAM failed:");
        goto DO_PB_INIT_FAILED;
    }

    ret = ioctl(dec_fd, DVR_DEC_QUERY_OUTPUT_BUFFER_SIZE, &dec_buf_size);
    if (ret < 0) {
        perror("Decoder DVR_DEC_QUERY_OUTPUT_BUFFER_SIZE failed:");
        goto DO_PB_INIT_FAILED;
    }
    
    pbbs_buf = (unsigned char*) mmap(NULL, dec_buf_size, PROT_READ|PROT_WRITE, MAP_SHARED, dec_fd, 0);
    if (pbbs_buf == MAP_FAILED) {
        perror("Dec mmap failed");
        goto DO_PB_INIT_FAILED;
    }
    return 0;

DO_PB_INIT_FAILED:
    if (dec_fd) {
        if (pbbs_buf) {
            munmap((void*)pbbs_buf, dec_buf_size);
            pbbs_buf = NULL;
        }
        close(dec_fd);
        dec_fd = 0;
    }
    if (pb_file) {
        GMAVIClose(pb_file);
        pb_file = NULL;
    }
    return -1;
}

int do_pb_start(int ch_num)
{
    int ret;
    dvr_dec_control    dec_ctrl;
    FuncTag tag;

    memset(&dec_ctrl, 0x0, sizeof(dvr_dec_control));

    if (!pb_file) {
        printf("Open file failed!\n");
        return -1;
    }
        
    dec_ctrl.command = DEC_START;
    dec_ctrl.src_param.dim.width = pb_main_header.dwWidth;
    dec_ctrl.src_param.dim.height = pb_main_header.dwHeight;
    dec_ctrl.src_param.win.x = 0;   //ROI, after decoder, or the input to scalar
    dec_ctrl.src_param.win.y = 0;   //ROI, after decoder, or the input to scalar
    dec_ctrl.src_param.win.width = pb_main_header.dwWidth;      //ROI, after decoder
    dec_ctrl.src_param.win.height = pb_main_header.dwHeight;    //ROI, after decoder
    dec_ctrl.src_param.bs_rate=(int) (1000000/(pb_main_header.dwMicroSecPerFrame));

    if(is_st_vga) {
        dec_ctrl.dst_param.win.x = VGA_4CH[ch_num].x;
        dec_ctrl.dst_param.win.y = VGA_4CH[ch_num].y;                    
    }           
    else {
        dec_ctrl.dst_param.win.x = D1_4CH[ch_num].x;    //final position of screen
        dec_ctrl.dst_param.win.y = D1_4CH[ch_num].y;    //final position of screen
    }
    dec_ctrl.dst_param.win.width = VGA_4CH[ch_num].width;      //final width in screen
    dec_ctrl.dst_param.win.height = VGA_4CH[ch_num].height;    //final width in screen
    dec_ctrl.dst_param.plane_id = main_plane_id;
    dec_ctrl.dst_param.is_display = TRUE;
    dec_ctrl.dst_param.display_rate = (is_NTSC)? 30 : 25;    
    
    ret = ioctl(dec_fd, DVR_DEC_CONTROL, &dec_ctrl);
    if (ret < 0)
        return -1;

    FN_RESET_TAG(&tag);
    FN_SET_PB_CH(&tag, ch_num);
    ret = ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    if (ret < 0)
        return -1;    

    menu_func = FUNCTION_NORMAL;

    return 0;
}

int do_pb_stop(int ch_num)
{
    int ret;
    dvr_dec_control    dec_ctrl;
    FuncTag tag;

    memset(&dec_ctrl, 0x0, sizeof(dvr_dec_control));
    
    menu_func = FUNCTION_NULL;    
    dec_ctrl.command = DEC_STOP;
    if ((ret = ioctl(dec_fd, DVR_DEC_CONTROL, &dec_ctrl)) < 0)
        goto pb_exit;

    FN_RESET_TAG(&tag);
    FN_SET_PB_CH(&tag, ch_num);
    if ((ret = ioctl(dvr_fd, DVR_COMMON_APPLY, &tag)) < 0)
        goto pb_exit;

pb_exit:    
    return 0;
}

int do_pb_exit(int ch_num)
{
    if (dec_fd) {
        if (pbbs_buf) {
            munmap((void*)pbbs_buf, dec_buf_size);
            pbbs_buf = NULL;
        }
        close(dec_fd);        
        dec_fd = 0;
    }
    close_pb_files(ch_num);
    
    return 0;
}

int main(void)
{    
    //open dvr_common
    dvr_fd = open("/dev/dvr_common", O_RDWR);
    
    do_disp_startup();  
    do_pb_init(0);  
    do_pb_start(0);  
        
    packet_reader(0);
    
    do_pb_stop(0);  
    do_pb_exit(0);  
    do_disp_endup();  
    
    //close dvr_common
    close(dvr_fd);
    return 0;
}
