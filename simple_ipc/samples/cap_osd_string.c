/**
 * this sample code implement capture osd function, and record for 10 seconds.
 * When you play this encode file, you can find osd string.
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
#include "vcap_osd.h" 

int dvr_fd = 0;
int enc_fd = 0;
int video_fd[8] = { 0 };

#define input_width 1280
#define input_height 720
uint8_t space[36] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

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
        0,
        ENC_TYPE_FROM_CAPTURE,
        {input_width, input_height},
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
        {input_width, input_height},         
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

void initial_font(int channel)
{
    fiosd_char_t ch;
    //fiosd_charmap_t pr;
    int ret;
    char tmp;
    //int i;

    tmp = 'A';                  //take off 'A'
    ret = ioctl(video_fd[channel], FIOSDS_RMCHAR, &tmp);

    ch.font = 'A';
    memcpy((void *)ch.fbitmap, (void *)space, sizeof(uint8_t) * 36);
    ioctl(video_fd[channel], FIOSDS_CHAR, &ch);
}

void set_osd_win_color(int osd_fd, int idx, int x, int y, int w, int h, char *str, int start,
                       uint8_t pal_fg, uint8_t pal_bg, uint8_t PY1, uint8_t PCb1, uint8_t PCr1, uint8_t PY2, uint8_t PCb2, uint8_t PCr2)
{
    fiosd_win_t win;
    fiosd_palette_t palette;
    fiosd_string_t string;
    fiosd_transparent_t transparent;
    int index;
    int ret;    

    /* disable OSD windows */
    index = idx;            
    ret   = ioctl(osd_fd, FIOSDS_OFF, &index);    
    if(ret < 0) {
        printf("FIOSDS_OFF Fail!");
        return;
    }
    
    /* set OSD window x,y postion and dimension */            
    win.x      = x;
    win.y      = y;
    win.hdim   = w;
    win.vdim   = h;
    win.windex = idx;
    ret = ioctl(osd_fd, FIOSDS_WIN, &win);
    if(ret < 0) {
        printf("FIOSDS_WIN Fail!");
        return;
    }

    /* set OSD foreground color */
    palette.index = pal_fg;
    palette.y     = PY1;
    palette.cb    = PCb1;
    palette.cr    = PCr1;
    ret = ioctl(osd_fd, FIOSDS_PALTCOLOR, &palette);
    if(ret < 0) {
        printf("FIOSDS_PALTCOLOR 0 Fail!");
        return;
    }
    
    /* set OSD backfrpund color */           
    palette.index = pal_bg;
    palette.y     = PY2;
    palette.cb    = PCb2;
    palette.cr    = PCr2;
    ret = ioctl(osd_fd, FIOSDS_PALTCOLOR, &palette);
    if(ret < 0) {
        printf("FIOSDS_PALTCOLOR 1 Fail!");
        return;
    }
        
    /* set OSD Transparent */    
    transparent.level  = FOSD_TRANSPARENT_100PERCENT;
    transparent.windex = index;
    ret = ioctl(osd_fd, FIOSDS_TRANSPARENT, &transparent);
    if(ret < 0) {
        printf("FIOSDMASKS_TRANSPARENT Fail!");
        return;
    }
    
    /* set OSD window color and display string */    
    string.windex   = index;
    string.start    = start;
    string.fg_color = pal_fg;
    string.bg_color = pal_bg;
    if(str)
      strcpy(&string.string[0], str);            
    ret = ioctl(osd_fd, FIOSDS_STRING, &string);
    if(ret < 0) {
        printf("FIOSDS_STRING Fail!");
        return;
    }
    
    /* enable OSD display */        
    ret = ioctl(osd_fd, FIOSDS_ON, &index);
    if(ret < 0) {
        printf("FIOSDS_ON Fail!");
        return;
    }
}

int osd_setting(void)
{
    char str[32];
    int  disp_idx, i;

    char devicename[100];
    	
    for (i = 0; i < 1; i++) {
        memset(devicename, 100, 0);
        sprintf(devicename, "/dev/fosd%d%d", i, 0);
        printf("open %s\n", devicename);
        video_fd[i] = open(devicename, O_RDWR);
        if (video_fd[i] < 0) {
            printf("fosd1 open fail!");
            exit(1);
        }
        initial_font(i);
        printf("open device:%s successfully!\n", devicename);
			
			 memset(str, 0, 32);
        strcpy(str, "WIN 0");
        disp_idx = 32;
        set_osd_win_color(video_fd[i], 0, 10, 50, 5, 1, str, disp_idx, 0, 1,
                          200, 200, 200, 60, 60, 60);

        memset(str, 0, 32);
        strcpy(str, "WIN 1");
        disp_idx = 96;
        set_osd_win_color(video_fd[i], 1, 10, 150, 5, 1, str, disp_idx, 1, 0,
                          200, 200, 200, 60, 60, 60);
        
        memset(str, 0, 32);
        strcpy(str, "WIN 2");
        disp_idx = 0;
        set_osd_win_color(video_fd[i], 2, 100, 50, 5, 1, str, disp_idx, 0, 1,
                          200, 200, 200, 60, 60, 60);

        memset(str, 0, 32);
        strcpy(str, "WIN 3");
        disp_idx = 64;   
        set_osd_win_color(video_fd[i], 3, 100, 150, 5, 1, str, disp_idx, 1, 0,
                          200, 200, 200, 60, 60, 60);
    }
    return 0;
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
    int buf_size, i;          
    struct timeval t1,t2;  
    char tmp_str[128];
                    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
    
    ret = osd_setting();
    if(ret < 0)
    	goto end;
        	      
    do_record_start(ch_num);

    sprintf(file_name, "CH%d_video_%d", 0, 0);      
    sprintf(tmp_str, "%s.h264", file_name);            
    rec_file = fopen ( tmp_str , "wb+" );
    gettimeofday(&t1, NULL);        
    
    while(1) {          
        // prepare to select(or poll)
        rec_fds.fd = enc_fd;      
        rec_fds.revents = 0;            
        rec_fds.events = POLLIN_MAIN_BS;
            
        poll(&rec_fds, 1, 500);     
                    	    
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
                         
        if ((t2.tv_sec - t1.tv_sec) == 10) {      //< record for 10 seconds. then finish record.                 
            fclose(rec_file);       
            break;               
        }       
    }

    do_record_stop(ch_num);    

    printf("----------------------------------\n");
    printf(" Record finish\n");
    printf("----------------------------------\n");    
    
    ch_num = 0; 
    for (i = 0; i < 1; i++) {
    	ret = ioctl(video_fd[i], FIOSDS_OFF, &ch_num);
    	close(video_fd[i]);
    }
end:                    
    close(enc_fd);      //close_dvr_encode    
    close(dvr_fd);      //close_dvr_common

    return 0;
}
