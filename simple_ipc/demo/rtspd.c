/*
 * Copyright (C) 2009 Faraday Technology Corporation.
 * NOTE:
 *  1. librtsp is statically linked into this program.
 *  2. Only one rtsp server thread created. No per-thread variables needed.
 *  3. Write for testing librtsp library, not optimized for performance.
 *  Version	Date		Description
 *  ----------------------------------------------
 *  0.1.0	        	First release.
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "librtsp.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include "dvr_enc_api.h"
#include "dvr_common_api.h"
#include "gmavi_api.h"
#include "ioctl_isp.h"
#include "file_cfg.h"


#define IPC_CFG_FILE       "/mnt/mtd/ipc.cfg"
#define SDPSTR_MAX		128
#define FILE_PATH_MAX	128
#define SR_MAX			64
#define	VQ_MAX			(SR_MAX)
#define	VQ_LEN			5
#define AQ_MAX			64			/* 1 MP2 and 1 AMR for live streaming, another 2 for file streaming. */
#define AQ_LEN			2			/* 1 MP2 and 1 AMR for live streaming, another 2 for file streaming. */
#define AV_NAME_MAX		127

#define RTP_HZ				90000

#define ERR_GOTO(x, y)			do { ret = x; goto y; } while(0)
#define MUTEX_FAILED(x)			(x == ERR_MUTEX)

#define TIME_INTERVAL 4*RTP_HZ 
#define MACRO_BLOCK_SIZE 100
#define PRINT_ENC_AVERAGE 1
#define VIDEO_FRAME_NUMBER VQ_LEN+1

#define MAIN_BS_NUM	0
#define SUB1_BS_NUM	1
#define SUB2_BS_NUM	2
#define SUB3_BS_NUM	3
#define SUB4_BS_NUM	4
#define SUB5_BS_NUM	5
#define SUB6_BS_NUM	6
#define SUB7_BS_NUM	7
#define SUB8_BS_NUM	8
//#define ALL_BS_NUM	3

#define NONE_BS_EVENT	0
#define START_BS_EVENT	1
#define UPDATE_BS_EVENT	2
#define STOP_BS_EVENT	3

#define RATE_CTL_CBR	0
#define RATE_CTL_VBR	1
#define RATE_CTL_ECBR	2
#define RATE_CTL_EVBR	3

#define NTSC  0   ///< video mode : NTSC
#define PAL   1   ///< video mode : PAL
#define CMOS   2   ///< video mode : VGA


#define CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num)	\
	do {	\
		if((ch_num >= DVR_RECORD_CHANNEL_NUM || ch_num < 0) || \
			(sub_num >= DVR_ENC_REPD_BT_NUM || sub_num < 0)) {	\
			fprintf(stderr, "%s: ch_num=%d, sub_num=%d error!\n",__FUNCTION__, ch_num, sub_num);	\
			return -1; \
		}	\
	} while(0)	\


typedef int (*open_container_fn)(int ch_num, int sub_num);
typedef int (*close_container_fn)(int ch_num, int sub_num);
typedef int (*read_bs_fn)(int ch_num, int sub_num, dvr_enc_queue_get *data);
typedef int (*write_bs_fn)(int ch_num, int sub_num, void *data);
typedef int (*free_bs_fn)(int ch_num, int sub_num, dvr_enc_queue_get *data);

typedef enum st_opt_type {
    OPT_NONE=0,
    RTSP_LIVE_STREAMING,
    FILE_AVI_RECORDING,
    FILE_H264_RECORDING
} opt_type_t;

typedef struct st_video_frame {
	int ch_num;
	int sub_num;
	dvr_enc_queue_get queue;	
	int search_key;
} video_frame_t;

typedef struct st_frame_slot {
	struct st_frame_slot *next;
	struct st_frame_slot *prev;
	video_frame_t *vf;
} frame_slot_t;

typedef struct st_frame_info {
	frame_slot_t *avail;
	frame_slot_t *used;
	pthread_mutex_t mutex;
} frame_info_t;

typedef struct st_vbs {
	int enabled; //DVR_ENC_EBST_ENABLE: enabled, DVR_ENC_EBST_DISABLE: disabled
	int enc_type;	// 0:ENC_TYPE_H264, 1:ENC_TYPE_MPEG, 2:ENC_TYPE_MJPEG
	int width;
	int height;
	int fps;
	int ip_interval;
	int bps;	// if bps = 0, for default bps, generate by get_bitrate()
	int rate_ctl_type;	// 0:cbr, 1:vbr, 2:ecbr, 3:evbr
	int target_rate_max;
	int reaction_delay_max;
	int max_quant;
	int min_quant;
	int init_quant;
	int mjpeg_quality;
	int enabled_snapshot; //1: enabled, 0: disabled, not ready
	int enabled_roi;      //1: enabled, 0: disabled, not ready
    int roi_x;            //roi x position
    int roi_y;            //roi y position
    int roi_w;            //roi width in pixel
    int roi_h;            //roi height in pixel
} vbs_t;

typedef struct st_priv_vbs {
	int offs;		// bitstream mmap buffer offset.
	int buf_usage;
	char sdpstr[SDPSTR_MAX];
	unsigned int tick;
 	int	tinc;	/* interval, in unit of 1/9KHz. */
	int timed;
	int itvl_fps;
	int avg_fps;
	int itvl_bps;
	int avg_bps;
	unsigned int timeval;
	int qno;
} priv_vbs_t;

typedef struct st_abs {
	int reserved;
	//int enabled;  	// DVR_ENC_EBST_ENABLE: enabled, DVR_ENC_EBST_DISABLE: disabled
	//int enc_type;	// 0:mp2, 1:adpcm, 2:amr
	//int bps;
} abs_t;

typedef struct st_priv_abs {
	int reserved;
} priv_abs_t;

typedef struct st_bs {
	int event; // config change please set 1 for enq_thread to config this 
	int enabled; //DVR_ENC_EBST_ENABLE: enabled, DVR_ENC_EBST_DISABLE: disabled
	opt_type_t opt_type;  /* 1:rtsp_live_streaming, 2: file_avi_recording 3:file_h264_recording */
	vbs_t video;  /* VIDEO, 0: main-bitstream, 1: sub1-bitstream, 2:sub2-bitstream */
	abs_t audio;  /* AUDIO, 0: main-bitstream, 1: sub1-bitstream, 2:sub2-bitstream */
} avbs_t;

typedef struct st_update_bs {
    int stream_enable;  ///< 0:disable, 1:enable
    int enc_type;	    ///< 0:ENC_TYPE_H264, 1:ENC_TYPE_MPEG, 2:ENC_TYPE_MJPEG
    int frame_rate;     ///< frame rate per second
    int bit_rate;       ///< Bitrate per second
    int ip_interval;    ///< The I-P interval frames
    int width;          ///< width
    int height;         ///< hieght
    int rate_ctl_type;	// 0:cbr, 1:vbr, 2:ecbr, 3:evbr
    int target_rate_max;
    int reaction_delay_max;
    int init_quant;     ///< The initial quant value
    int max_quant;      ///< The max quant value
    int min_quant;      ///< The min quant value
    int mjpeg_quality;
	int enabled_roi;    //1: enabled, 0: disabled, not ready
    int roi_x;          ///< roi x position
    int roi_y;          ///< roi y position
    int roi_w;          ///< roi width
    int roi_h;          ///< roi height
} update_avbs_t;

typedef struct st_priv_bs {
	//char rtsp_live_name[AV_NAME_MAX];
	int play;
	int congest;
	int sr;
 	char name[AV_NAME_MAX];
	void *fd;
    int reset_intra_frame;
	int avi_str_id;
	open_container_fn open;
	close_container_fn close;
	read_bs_fn read;
	write_bs_fn write;
	free_bs_fn free;
	priv_vbs_t video;  /* VIDEO, 0: main-bitstream, 1: sub1-bitstream, 2:sub2-bitstream */
	priv_abs_t audio;  /* AUDIO, 0: main-bitstream, 1: sub1-bitstream, 2:sub2-bitstream */
} priv_avbs_t;

typedef struct st_av {
	/* public data */
	avbs_t bs[DVR_ENC_REPD_BT_NUM];  /* VIDEO, 0: main-bitstream, 1: sub1-bitstream, 2:sub2-bitstream */
    /* update date */
	pthread_mutex_t ubs_mutex;
    update_avbs_t ubs[DVR_ENC_REPD_BT_NUM];

    /* per channel data */
	int denoise;  /* 3D-denoise */
	int de_interlace;  /* 3d-deInterlace  */
    int input_system; 

	/* private data */
	int enabled;  	//DVR_ENC_EBST_ENABLE: enabled, DVR_ENC_EBST_DISABLE: disabled
	priv_avbs_t priv_bs[DVR_ENC_REPD_BT_NUM];	
	int enc_fd; 
	unsigned char *v_bs_mbuf_addr;
	int v_bs_mbuf_size;
} av_t;

int isp_fd=0;
pthread_t		enq_thread_id;
unsigned int	sys_tick = 0;
__time_t		sys_sec;
int				sys_port	= 554;
char			file_path[FILE_PATH_MAX]	= "file";
char			*ipptr = NULL;
char			ipstr[32] = {0};
static int t_ch_num=DVR_RECORD_CHANNEL_NUM;
static int rtspd_sysinit=0;
static int rtspd_set_event=0;
static int enable_print_average = 0;

static frame_info_t frame_info[VQ_MAX];
//pthread_mutex_t rtspd_mutex;

av_t enc[DVR_RECORD_CHANNEL_NUM];
fcfg_file_t   *cfg_file;

struct pollfd poll_fds[DVR_RECORD_CHANNEL_NUM];  // poll

//extern int is_NTSC;   //if FALSE, it's PAL
//static int input_system = MCP_VIDEO_NTSC;   //if FALSE, it's PAL
int rtspd_dvr_fd = 0;

static char *opt_type_def_str[]={  
                "None",
                "RTSP",
                " AVI",
                "H264"};

static char *enc_type_def_str[]={  
                " H264",
                "MPEG4",
                "MJPEG"};

static char *rcCtlTypeString[]={
                "CBR",
				"VBR",
				"ECBR",
				"EVBR"};	

static char *rcInSysString[]={
                "NTSC",
				"PAL",
				"CMOS"};	
						
static int dvr_enc_queue_get_def_table[]={  
                DVR_ENC_QUEUE_GET,		    /* main-bitstream */
                DVR_ENC_QUEUE_GET_SUB1_BS,	/* sub1-bitstream */
                DVR_ENC_QUEUE_GET_SUB2_BS,	/* sub2-bitstream */
                DVR_ENC_QUEUE_GET_SUB3_BS,	/* sub2-bitstream */
                DVR_ENC_QUEUE_GET_SUB4_BS,	/* sub2-bitstream */
                DVR_ENC_QUEUE_GET_SUB5_BS,	/* sub2-bitstream */
                DVR_ENC_QUEUE_GET_SUB6_BS,	/* sub2-bitstream */
                DVR_ENC_QUEUE_GET_SUB7_BS,	/* sub2-bitstream */
                DVR_ENC_QUEUE_GET_SUB8_BS};	/* sub3-bitstream */

static int dvr_enc_queue_offset_def_table[]={  
                0,		                                 /* main-bitstream */
			    DVR_ENC_QUERY_OUTPUT_BUFFER_SUB1_BS_OFFSET,	 /* sub1-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB2_BS_OFFSET,	 /* sub2-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB3_BS_OFFSET,	 /* sub2-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB4_BS_OFFSET,	 /* sub2-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB5_BS_OFFSET,	 /* sub2-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB6_BS_OFFSET,	 /* sub2-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB7_BS_OFFSET,	 /* sub2-bitstream */
                DVR_ENC_QUERY_OUTPUT_BUFFER_SUB8_BS_OFFSET}; /* sub3-bitstream */

dvr_enc_channel_param   main_ch_setting = 
{
    { 
		0,
    	ENC_TYPE_FROM_CAPTURE,
        {320, 240},
        LVFRAME_EVEN_ODD, //LVFRAME_EVEN_ODD, LVFRAME_GM3DI_FORMAT,
		LVFRAME_FRAME_MODE, //LVFRAME_FIELD_MODE, //LVFRAME_FRAME_MODE,
		DMAORDER_PACKET, //DMAORDER_PACKET, DMAORDER_3PLANAR, DMAORDER_2PLANAR
        CAPSCALER_NOT_KEEP_RATIO,
        MCP_VIDEO_NTSC,     // MCP_VIDEO_NTSC
		CAPCOLOR_YUV422,  // CAPCOLOR_YUV420_M0, CAPCOLOR_YUV422
        { FALSE, FALSE, GM3DI_FRAME }    //Denoise off
    },
    {
		DVR_ENC_EBST_ENABLE,  //enabled
		0,	// main-bitstream
		ENC_TYPE_MPEG,
		FALSE,
		DVR_ENC_EBST_DISABLE, // en_snapshot
		{320, 240},			// DIM
		{ENC_INPUT_1D422, 30, 1048576,  30, 25, 51, 1 , FALSE, {0, 0, 320, 240}},	//EncParam
		{SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 },	//ScalerParam
		{JCS_yuv420, 0, JENC_INPUT_MP42D, 70} //snapshot_param	
    }
};

ReproduceBitStream   sub_ch_setting = 
{
	DVR_ENC_EBST_DISABLE,  //enabled
	1,	// sub1-bitstream
	ENC_TYPE_MPEG, //enc_type, 0: ENC_TYPE_H264, 1:ENC_TYPE_MPEG, 2:ENC_TYPE_MJPEG
	FALSE,  // is_blocked
	DVR_ENC_EBST_DISABLE, // en_snapshot,
	{1280, 720},			// DIM
	{ENC_INPUT_1D422, 5, 262144,  15, 25, 51, 1 , FALSE, {0, 0, 320, 240}},	//EncParam
	{SCALE_YUV422, SCALE_YUV422, SCALE_LINEAR, FALSE, FALSE, TRUE, 0 },	//ScalerParam
	{JCS_yuv420, 0, JENC_INPUT_MP42D, 70 }	//snapshot_param	
};

static void pt_list(int qno)
{
	int num;
	frame_slot_t *fs;

	fprintf(stderr, "----------------- avail -------------------------\n");
	for(fs = frame_info[qno].avail, num=0; fs; fs = fs->next, num++) 
		fprintf(stderr, "num=%d, fs->prev=%p, fs=%p, fs->next=%p\n", num, fs->prev, fs, fs->next);
	fprintf(stderr, "----------------- used -------------------------\n");
	for(fs = frame_info[qno].used, num=0; fs; fs = fs->next, num++) 
		fprintf(stderr, "num=%d, fs->prev=%p, fs=%p, fs->next=%p\n", num, fs->prev, fs, fs->next);
}

static void dbg_rtsp(void)
{
	int ch_num, sub_num;
	av_t *e;
	avbs_t *b;
	priv_avbs_t *pb;
	
	/* public data initial */
	for(ch_num=0; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
		e = &enc[ch_num];
		fprintf(stderr, "----------------------------------------------\n");
		fprintf(stderr, "ch_num=%d, revent=%x, events=%x, fd=%d\n", ch_num, poll_fds[ch_num].revents, poll_fds[ch_num].events, poll_fds[ch_num].fd);
		fprintf(stderr, "----------------------------------------------\n");
		for(sub_num=0; sub_num < DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &e->bs[sub_num];
			pb = &e->priv_bs[sub_num];
			fprintf(stderr, "----------------------------------------------\n");
			fprintf(stderr, "sub_num=%d_%d: \n",ch_num, sub_num);
			fprintf(stderr, "----------------------------------------------\n");
			fprintf(stderr, "\tevent=%d, enable=%x, opt_type=%d\n", b->event, b->enabled, b->opt_type);
			fprintf(stderr, "\tvideo :enabled=%x, enc_type=%d, w=%d, h=%d, fps=%d, bps=%d\n", b->video.enabled, 
															b->video.enc_type, 
															b->video.width,
															b->video.height,
															b->video.fps,
															b->video.bps);
			fprintf(stderr, "\toffs=%x, sdpstr=%s, tick=%d, timed=%d, qno=%d, buf_usage=%d\n", pb->video.offs, 
										pb->video.sdpstr,
										pb->video.tick,
										pb->video.timed,
										pb->video.qno, 
										pb->video.buf_usage);
			fprintf(stderr, "\tplay=%d, congest=%d, sr=%d, name=%s, fd=%p\n", pb->play, 
										pb->congest, 
										pb->sr, 
										pb->name,
										pb->fd);
			if(pb->video.qno>=0) pt_list(pb->video.qno);
		}
	}
}	

static inline unsigned int timediff(unsigned int ptime, unsigned int ctime)
{
	unsigned int diff;

	if(ctime >= ptime ) diff = ctime - ptime;
	else diff = ((unsigned int)0xffffffff - ptime) + ctime;
	
	return diff;
}

static void show_enc_cfg(void)
{
	avbs_t *b;
	priv_avbs_t *pb;
	int ch_num, sub_num, is_enable;

//	printf("------------------------------------------------\n");
//	printf("\rinput_syst=%s\n", rcInSysString[input_system]);
	printf("------------------------------------------------\n");
	for(ch_num=0; ch_num <DVR_RECORD_CHANNEL_NUM; ch_num++) {
    	is_enable = 0;
    	for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &enc[ch_num].bs[sub_num];
        	if(b->opt_type != OPT_NONE) { is_enable = 1; break; }
    	}
    	if(is_enable) printf("\r%02d  : input_system=%s, 3DN=%d, 3DI=%d\n", ch_num, 
    	                                    rcInSysString[enc[ch_num].input_system],
    	                                    enc[ch_num].denoise,
    	                                    enc[ch_num].de_interlace);
    	for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &enc[ch_num].bs[sub_num];
			if(b->opt_type == OPT_NONE) continue;
			pb = &enc[ch_num].priv_bs[sub_num];
			printf("\r%02d_%d: %s(%s):%s %02dfps, %dx%d, gop=%d\n",
							ch_num,
							sub_num,
							enc_type_def_str[b->video.enc_type],
							(b->enabled==DVR_ENC_EBST_ENABLE) ? "ON" : "OFF", 
							opt_type_def_str[b->opt_type],
							b->video.fps,
							b->video.width,
							b->video.height, 
							b->video.ip_interval);
            switch(b->video.enc_type) {
                case ENC_TYPE_H264:
                case ENC_TYPE_MPEG:
                    switch(b->video.rate_ctl_type) {
                        case RATE_CTL_CBR:
                	        printf("      rctype=%s, bs=%dk\n", 
                							rcCtlTypeString[b->video.rate_ctl_type], 
                							b->video.bps/1000);
                	        printf("      maxQ=%d, minQ=%d, iniQ=%d\n", 
                							b->video.max_quant,
                							b->video.min_quant,
                							b->video.init_quant);
                            break;
                        case RATE_CTL_VBR:
                	        printf("      rctype=%s\n", 
                							rcCtlTypeString[b->video.rate_ctl_type]);
                	        printf("      maxQ=%d, minQ=%d, iniQ=%d\n", 
                							b->video.max_quant,
                							b->video.min_quant,
                							b->video.init_quant);
                            break;
                        case RATE_CTL_ECBR:
                	        printf("      rctype=%s, bs=%dk, trm=%dk, rdm=%ds,\n", 
                							rcCtlTypeString[b->video.rate_ctl_type], 
                							b->video.bps/1000,
                							b->video.target_rate_max/1000, 
                							b->video.reaction_delay_max/1000);
                	        printf("      maxQ=%d, minQ=%d, iniQ=%d\n", 
                							b->video.max_quant,
                							b->video.min_quant,
                							b->video.init_quant);
                            break;
                        case RATE_CTL_EVBR:
                	        printf("      rctype=%s, trm=%dk\n", 
                							rcCtlTypeString[b->video.rate_ctl_type], 
                							b->video.target_rate_max/1000);
                	        printf("      maxQ=%d, minQ=%d, iniQ=%d\n", 
                							b->video.max_quant,
                							b->video.min_quant,
                							b->video.init_quant);
                            break;
                    }
                    break;
                case ENC_TYPE_MJPEG:
        	        printf("      mjQ=%d\n", b->video.mjpeg_quality);
                    break;
            }
            if(b->video.enabled_roi == 1) {
    	        printf("      ROI=ON (x=%d, y=%d, w=%d, h=%d)\n", 
    							b->video.roi_x,
    							b->video.roi_y,
    							b->video.roi_w,
    							b->video.roi_h);
			}
    	}
		printf("\n");
	}
}

#if PRINT_ENC_AVERAGE
static void print_enc_average(int tick)
{
	avbs_t *b;
	priv_avbs_t *pb;
	int ch_num, sub_num, diff_tv;
	static int average_tick=0;

	if(enable_print_average==0) return;
	diff_tv = timediff(average_tick, tick);
	if (diff_tv < TIME_INTERVAL) return;
	printf("------------------------------------------------\n");
	for(ch_num=0; ch_num <DVR_RECORD_CHANNEL_NUM; ch_num++) {
    	for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &enc[ch_num].bs[sub_num];
			if(b->opt_type == OPT_NONE || b->video.enabled == DVR_ENC_EBST_DISABLE) continue;
			pb = &enc[ch_num].priv_bs[sub_num];
			printf("\r%02d_%d: %s:%s %2d.%02d_fps, %5d_kbps, %4dx%-4d\n",
							ch_num,
							sub_num,
							enc_type_def_str[b->video.enc_type],
							opt_type_def_str[b->opt_type],
							pb->video.avg_fps/100, pb->video.avg_fps%100,
							pb->video.avg_bps/1024,
							b->video.width,
							b->video.height);
    	}
		printf("\n");
	}
	average_tick = tick;
}
#endif

static char getch(void)
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

static void set_sdpstr(char *sdpstr, int enc_type)
{
	// Not ready !
	switch(enc_type) {
	    case ENC_TYPE_MPEG:
        	// mpeg4, 352x240
        	strcpy(sdpstr, "000001B001000001B50900000100000001200086C400670C281078518F");
	        break;
	    case ENC_TYPE_MJPEG:
	        // mjpeg, 352x240
        	strcpy(sdpstr, "281e19");
	        break;
	    case ENC_TYPE_H264:
	    default: 
        	// h264, 352x240
        	strcpy(sdpstr, "Z0IAKOkCw/I=,aM44gA==");
	        break;
	}
}

static inline int get_num_of_list(frame_slot_t **list)
{
	int num=0;
	frame_slot_t *fs;
	
	for(fs = *list; fs; fs = fs->next) num++;
	return num;
}

static inline void add_to_list(frame_slot_t *fs, frame_slot_t **list)
{
	fs->prev = NULL;
	fs->next = *list;
	if( fs->next ) fs->next->prev = fs;
	*list = fs;
}

static inline frame_slot_t *remove_from_listhead(frame_slot_t **list)
{
	frame_slot_t *fs;
	
	fs = *list;
	if(fs->next) fs->next->prev = NULL;
	*list = fs->next;
	return fs;
}

static frame_slot_t *search_from_list(int search_key, frame_slot_t **list)
{
	frame_slot_t *fs, *next, *ret_fs=NULL;
	
	for( fs = *list; fs; fs = next ) {
		next = fs->next;
		if( fs->vf->search_key == search_key ) {
			ret_fs = fs;
			break;
		}
	}
	return ret_fs;
}

static void remove_from_list(frame_slot_t *fs, frame_slot_t **list)
{
	if( fs->next ) fs->next->prev = fs->prev;
	if( fs->prev ) fs->prev->next = fs->next;
	else *list = fs->next;
}

static int init_video_frame(int qno)
{
	int i;
	frame_slot_t *fs;

	if(qno>=VQ_MAX || qno<0) {
		fprintf(stderr, "%s: qno=%d err.\n", __func__, qno);
		return -1;
	}
		
	if (pthread_mutex_init(&frame_info[qno].mutex, NULL)) {
		perror("init_video_frame: mutex init failed:");
		return -1;
	}
	frame_info[qno].avail = NULL;
	frame_info[qno].used = NULL;
	for(i=0; i<VIDEO_FRAME_NUMBER; i++) {
		if((fs = (frame_slot_t *) malloc(sizeof(frame_slot_t))) == NULL)
			goto err_exit;
		if((fs->vf = (video_frame_t *)malloc(sizeof(video_frame_t))) == NULL)
			goto err_exit;
		add_to_list(fs, &frame_info[qno].avail);
	}
	return 0;

err_exit: 
	fprintf(stderr, "%s: qno=%d, malloc failed!\n", __func__, qno);
	return -1;
}

static int get_num_of_used_video_frame(int qno)
{
	int num;
	
	pthread_mutex_lock( &frame_info[qno].mutex);
	num = get_num_of_list(&frame_info[qno].used);
	pthread_mutex_unlock(&frame_info[qno].mutex);
	return num;
}

static int free_video_frame(int qno)
{
	frame_slot_t *fs, *next;
	int i, ret=-1;

	pthread_mutex_lock( &frame_info[qno].mutex );
	for( fs = frame_info[qno].avail, i=0; fs; fs = next, i++) {
		next = fs->next;
		if(fs->vf) free(fs->vf);
		if(fs) free(fs);
		if(i >= VIDEO_FRAME_NUMBER) goto err_exit;
	}
	frame_info[qno].avail=NULL;
	frame_info[qno].used=NULL;
	
	if(i != VIDEO_FRAME_NUMBER) ret = -1;
	else ret = 0; 

err_exit:
	pthread_mutex_unlock(&frame_info[qno].mutex );
	pthread_mutex_destroy(&frame_info[qno].mutex);
	if(ret < 0) fprintf(stderr, "%s: qno=%d, i=%d free failed!\n", __func__, qno, i);
	return ret;
}

static frame_slot_t *search_video_frame(int qno, int search_key)
{
	frame_slot_t *fs;
	pthread_mutex_lock( &frame_info[qno].mutex );
	fs = search_from_list(search_key, &frame_info[qno].used);
	pthread_mutex_unlock( &frame_info[qno].mutex );
	return fs;
}

static frame_slot_t *take_video_frame(int qno)
{
	frame_slot_t *fs;
	
	if(qno>=VQ_MAX || qno<0) {
		fprintf(stderr, "%s: qno=%d err\n", __func__, qno);
		return NULL;
	}

	pthread_mutex_lock( &frame_info[qno].mutex );
	if((fs = remove_from_listhead(&frame_info[qno].avail)) == NULL) goto err_exit;
	add_to_list(fs, &frame_info[qno].used);
err_exit:
	pthread_mutex_unlock( &frame_info[qno].mutex );
	if(!fs) fprintf(stderr, "%s: qno=%d, video frame full.\n", __func__, qno);
	return fs;
}

static void put_video_frame(int qno, frame_slot_t *fs)
{
	if(qno>=VQ_MAX || qno<0 || fs==NULL) {
		fprintf(stderr, "%s: qno=%d err\n", __func__, qno);
		return;
	}
	
	pthread_mutex_lock(&frame_info[qno].mutex);
	remove_from_list(fs, &frame_info[qno].used);
	add_to_list(fs, &frame_info[qno].avail);
	pthread_mutex_unlock(&frame_info[qno].mutex);
	return;
}

static int do_queue_alloc(int type)
{
	int	rc;
	do {
		rc = stream_queue_alloc(type);
	} while MUTEX_FAILED(rc);

	return rc;
}

#if 0
static int do_queue_release(int qi)
{
	int	rc;
	do {
		rc = stream_queue_release(qi);
	} while MUTEX_FAILED(rc);

	return rc;
}
#endif

static unsigned int get_tick(struct timeval *tv)
{
	struct timeval	t, *tval;
    __time_t 		tv_sec;
    unsigned int	tick;

    if(tv == NULL) {
        tval = &t;
        gettimeofday(tval, NULL);
    } else {
        tval = tv;
    }
	tv_sec = tval->tv_sec-sys_sec;
	sys_tick += tv_sec*RTP_HZ;
	sys_sec = tval->tv_sec;
	tick = tval->tv_usec*(RTP_HZ/1000)/1000;
	tick += sys_tick;
	return tick;	
}

static int convert_gmss_media_type(int type)
{
	int media_type;

	switch(type) {
		case ENC_TYPE_H264: media_type=GM_SS_TYPE_H264; break;
		case ENC_TYPE_MPEG: media_type=GM_SS_TYPE_MP4; break;
		case ENC_TYPE_MJPEG: media_type=GM_SS_TYPE_MJPEG; break;
		default: media_type  = -1; fprintf(stderr, "convert_gmss_media_type: type=%d, error!\n", type); break;
	}
	return media_type;	
}

void get_time_str(char *time_str)
{
	long ltime;	
	struct tm *newtime;
	
	time( &ltime );
	newtime = gmtime(&ltime);	
	sprintf(time_str,"%04d%02d%02d%02d%02d%02d",newtime->tm_year+1900,	
												newtime->tm_mon+1,
												newtime->tm_mday,
												newtime->tm_hour,
												newtime->tm_min,							
												newtime->tm_sec);
}

static int env_cfg_check_section(const char *section, fcfg_file_t * cfile)
{
    return(fcfg_check_section( section,  cfile));
}

static int env_cfg_get_value_4_erret(const char *section, const char *keyname, fcfg_file_t * cfile)
{
    int value, ret; 

    ret = fcfg_getint(section, keyname, &value, cfg_file);
    if(ret<=0) 
        value = GMVAL_DO_NOT_CARE;
    return(value);
}

static int env_cfg_get_value(const char *section, const char *keyname, fcfg_file_t * cfile)
{
    int value, ret; 

    ret = fcfg_getint(section, keyname, &value, cfg_file);
    if(ret<=0) {
        printf("<\"%s:%s:%d\" is not exist!>\n", section, keyname, ret);
        printf("Please copy file /dvr/ipc_conf/ipc_xxx.cfg to /mnt/mtd/ipc.cfg\n");
        exit(-1);
    }
    return(value);
}

static void env_cfg_init(void)
{
	int ch_num, sub_num;
	av_t *e;
	avbs_t *b;
	char tmp_str[32];

    cfg_file=fcfg_open(IPC_CFG_FILE , "r" ) ;
    if(cfg_file == NULL) {
        printf("please copy file /product/IPC-XXX/demo/ipc.cfg to /mnt/mtd/ipc.cfg\n");
        exit(-1);
    }

	/* public data initial */
	for(ch_num=0; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
		e = &enc[ch_num];
        snprintf(tmp_str, 32, "enc_%d", ch_num);
        if(env_cfg_check_section(tmp_str, cfg_file) == 0) 
            continue;
		e->denoise=env_cfg_get_value(tmp_str, "temporal_denoise", cfg_file); 
		e->de_interlace=env_cfg_get_value(tmp_str, "de_interlace", cfg_file); 
    	e->input_system = env_cfg_get_value(tmp_str, "input_system", cfg_file);
		for(sub_num=0; sub_num < DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &e->bs[sub_num];
            snprintf(tmp_str, 32, "enc_%d_%d", ch_num, sub_num);
            if(env_cfg_check_section(tmp_str, cfg_file) == 0) {
				b->opt_type = 0;
				b->enabled = DVR_ENC_EBST_DISABLE;
				b->video.enabled = DVR_ENC_EBST_DISABLE;
				continue;
            }
			b->opt_type=env_cfg_get_value(tmp_str, "opt_type", cfg_file);
			b->video.enc_type=env_cfg_get_value(tmp_str, "enc_type", cfg_file);
			b->video.fps=env_cfg_get_value(tmp_str, "fps", cfg_file);
			b->video.ip_interval= b->video.fps * 2;
			b->video.width=env_cfg_get_value(tmp_str, "width", cfg_file);
			b->video.height=env_cfg_get_value(tmp_str, "height", cfg_file);
            b->video.mjpeg_quality=env_cfg_get_value(tmp_str, "mjpeg_quality", cfg_file);
            b->video.rate_ctl_type=env_cfg_get_value(tmp_str, "rate_ctl_type", cfg_file); 
			b->video.max_quant=env_cfg_get_value(tmp_str, "max_quant", cfg_file); 
			b->video.min_quant=env_cfg_get_value(tmp_str, "min_quant", cfg_file); 
			b->video.init_quant=env_cfg_get_value(tmp_str, "init_quant", cfg_file); 
			b->video.bps=env_cfg_get_value(tmp_str, "kbps", cfg_file)*1000; 
			b->video.target_rate_max=env_cfg_get_value(tmp_str, "target_rate_max", cfg_file)*1000; 
			b->video.reaction_delay_max=env_cfg_get_value(tmp_str, "reaction_delay_max", cfg_file)*1000; 
			//b->video.enabled_snapshot=env_cfg_get_value(tmp_str, "enabled_snapshot", cfg_file); 
			b->video.enabled_roi=env_cfg_get_value_4_erret(tmp_str, "enabled_roi", cfg_file); 
			b->video.roi_x=env_cfg_get_value_4_erret(tmp_str, "roi_x", cfg_file); 
			b->video.roi_y=env_cfg_get_value_4_erret(tmp_str, "roi_y", cfg_file); 
			b->video.roi_w=env_cfg_get_value_4_erret(tmp_str, "roi_w", cfg_file); 
			b->video.roi_h=env_cfg_get_value_4_erret(tmp_str, "roi_h", cfg_file); 
		}
	}
    fcfg_close(cfg_file);
}


static void env_enc_init(void)
{
	int ch_num, sub_num;
	av_t *e;
	avbs_t *b;
	priv_avbs_t *pb;

	memset(enc,0,sizeof(enc));
	e = &enc[0];
	b = &e->bs[0];
    b->opt_type = RTSP_LIVE_STREAMING;
    b->video.enc_type = ENC_TYPE_H264;
    b->video.bps=2000000;
	b->event = NONE_BS_EVENT;
	b->enabled = DVR_ENC_EBST_DISABLE;
	b->video.enabled = DVR_ENC_EBST_DISABLE;
	b->video.width=1280;
	b->video.height=720;
	b->video.fps=30;
	b->video.ip_interval = b->video.fps *2;
	b->video.rate_ctl_type = RATE_CTL_ECBR;
	b->video.target_rate_max = 8*1024*1024;
	b->video.reaction_delay_max = 10000;
	b->video.max_quant = 51;
	b->video.min_quant = 1;
	b->video.init_quant = 30;
	for(ch_num=1; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
		e = &enc[ch_num];
    	if (pthread_mutex_init(&e->ubs_mutex, NULL)) {
    		perror("env_enc_init: mutex init failed:");
    		exit(-1);
    	}
		for(sub_num=0; sub_num < DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &e->bs[sub_num];
			b->enabled = DVR_ENC_EBST_DISABLE;
        	b->video.enabled = DVR_ENC_EBST_DISABLE;
		}
    }
	/* private data initial */
	for(ch_num=0; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
		e = &enc[ch_num];
		e->enc_fd = -1;
		for(sub_num=0; sub_num < DVR_ENC_REPD_BT_NUM; sub_num++) {
			pb = &e->priv_bs[sub_num];
			pb->video.qno = -1;
			pb->sr = -1;
		}
	}
}

static inline int get_bitrate(int width, int height, int fps)
{
	int bitrate;

	bitrate = (width*height)/256 * MACRO_BLOCK_SIZE * fps;
	return bitrate;
}

static inline int open_channel(int ch_num)
{
	int enc_fd;
	if(enc[ch_num].enabled == DVR_ENC_EBST_ENABLE) {
		fprintf(stderr, "open_channel: ch_num=%d already enabled, error!\n", ch_num);
		return -1;
	}
	
    if((enc_fd=open("/dev/dvr_enc", O_RDWR)) < 0) {
        perror("RTSP open_channel [/dev/dvr_enc] failed:");
        return -1;
    }
	enc[ch_num].enabled = DVR_ENC_EBST_ENABLE;
	if (rtspd_dvr_fd == 0) {
	    rtspd_dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
	}
	return enc_fd;
}

static inline int close_channel(int ch_num)
{
    int i, is_close_dvr_fd=1;
    
	munmap((void*)enc[ch_num].v_bs_mbuf_addr, enc[ch_num].v_bs_mbuf_size);
    close(enc[ch_num].enc_fd);
	enc[ch_num].enc_fd = -1;
	enc[ch_num].enabled = DVR_ENC_EBST_DISABLE;
    for(i=0; i< DVR_RECORD_CHANNEL_NUM; i++ ) {
        if(enc[i].enabled == DVR_ENC_EBST_ENABLE) {
            is_close_dvr_fd = 0;
            break;
        }
    }
    if(is_close_dvr_fd) {
        close(rtspd_dvr_fd);      //close_dvr_common
        rtspd_dvr_fd = 0;
    }
    return 0;
}

static inline int set_bs_res(int ch_num, int sub_num, int width, int height)
{
	av_t *e;

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if((width%16) || (height%16)) {
		fprintf(stderr, "set_bs_res: width=%d, height=%d error!\n", width, height);
		return -1;
	}
	e = &enc[ch_num];
	e->bs[sub_num].video.width = width;
	e->bs[sub_num].video.height = height;
	return 0;
}

static inline int set_bs_fps(int ch_num, int sub_num, int fps)
{
	av_t *e;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if(fps > 30 || fps <= 0) {
		fprintf(stderr, "set_bs_fps: fps=%d error!\n", fps);
		return -1;
	}
	e = &enc[ch_num];
	e->bs[sub_num].video.fps = fps;
	return 0;
}

#if 0
static int set_bs_rc_evbr(int ch_num, int sub_num, int max_bps, int intQ)
{
	av_t *e;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if(intQ <= 0 || intQ > 51) {
		fprintf(stderr, "%s: intQ=%d error!\n", __FUNCTION__, intQ);
		return -1;
	}
	if(max_bps < 0) {
		fprintf(stderr, "%s: bps=%d error!\n", __FUNCTION__, max_bps);
		return -1;
	}
	e = &enc[ch_num];
	e->bs[sub_num].video.target_rate_max = max_bps;
	e->bs[sub_num].video.reaction_delay_max = 0;
	e->bs[sub_num].video.max_quant = intQ;
	e->bs[sub_num].video.min_quant= intQ;
	e->bs[sub_num].video.init_quant= intQ;
	e->bs[sub_num].video.rate_ctl_type = RATE_CTL_EVBR;
	return 0;
}

static int set_bs_rc_ecbr(int ch_num, int sub_num, int bps, int max_bps, int reacDyMax)
{
	av_t *e;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if((max_bps < 0) || (bps < 0)) {
		fprintf(stderr, "%s: bps=%d, max_bps=%d error!\n", __FUNCTION__, bps, max_bps);
		return -1;
	}
	if(reacDyMax < 0) {
		fprintf(stderr, "%s: reaction_delay_max=%d error!\n", __FUNCTION__, reacDyMax);
		return -1;
	}
		
	e = &enc[ch_num];
	e->bs[sub_num].video.bps = bps;
	e->bs[sub_num].video.target_rate_max = max_bps;
	e->bs[sub_num].video.reaction_delay_max = reacDyMax;  //unit: milliseconds
	e->bs[sub_num].video.max_quant = 51;
	e->bs[sub_num].video.min_quant= 1;
	e->bs[sub_num].video.init_quant= 25;
	e->bs[sub_num].video.rate_ctl_type = RATE_CTL_ECBR;
	return 0;
}

static int set_bs_rc_vbr(int ch_num, int sub_num, int intQ)
{
	av_t *e;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if(intQ <= 0 || intQ > 51) {
		fprintf(stderr, "%s: intQ=%d error!\n", __FUNCTION__, intQ);
		return -1;
	}
	e = &enc[ch_num];
	e->bs[sub_num].video.target_rate_max = 0;
	e->bs[sub_num].video.reaction_delay_max = 0;  //unit: milliseconds
	e->bs[sub_num].video.max_quant = intQ;
	e->bs[sub_num].video.min_quant= intQ;
	e->bs[sub_num].video.init_quant= intQ;
	e->bs[sub_num].video.rate_ctl_type = RATE_CTL_VBR;
	return 0;
}

static int set_bs_rc_cbr(int ch_num, int sub_num, int bps)
{
	av_t *e;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if(bps < 0) {
		fprintf(stderr, "%s: bps=%d error!\n", __FUNCTION__, bps);
		return -1;
	}
	e = &enc[ch_num];
	e->bs[sub_num].video.bps = bps;
	e->bs[sub_num].video.target_rate_max = 0;
	e->bs[sub_num].video.reaction_delay_max = 0;  //unit: milliseconds
	e->bs[sub_num].video.max_quant = 51;
	e->bs[sub_num].video.min_quant= 1;
	e->bs[sub_num].video.init_quant= 25;
	e->bs[sub_num].video.rate_ctl_type = RATE_CTL_CBR;
	return 0;
}
#endif

static inline int set_bs_bps(int ch_num, int sub_num, int bps, int rate_ctl_type)
{
	av_t *e;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	if(bps < 0) {
		fprintf(stderr, "set_bs_bps: bps=%d error!\n", bps);
		return -1;
	}
	e = &enc[ch_num];
	e->bs[sub_num].video.bps = bps;
	return 0;
}

static inline int set_bs_opt_type(int ch_num, int sub_num, int opt_type)
{
	avbs_t *b;
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	b = &enc[ch_num].bs[sub_num];
	b->opt_type = opt_type;
	return 0;
}

static void set_tag(int ch_num, int sub_num, FuncTag *tag)
{
    FN_RESET_TAG(tag);
	switch(sub_num) {
		case MAIN_BS_NUM: 
			FN_SET_REC_CH(tag, ch_num); 
			break;
		case SUB1_BS_NUM: 
			FN_SET_SUB1_REC_CH(tag, ch_num); 
			break;
		case SUB2_BS_NUM: 
			FN_SET_SUB2_REC_CH(tag, ch_num); 
			break;
		case SUB3_BS_NUM: 
			FN_SET_SUB3_REC_CH(tag, ch_num); 
			break;
		case SUB4_BS_NUM: 
			FN_SET_SUB4_REC_CH(tag, ch_num); 
			break;
		case SUB5_BS_NUM: 
			FN_SET_SUB5_REC_CH(tag, ch_num); 
			break;
		case SUB6_BS_NUM: 
			FN_SET_SUB6_REC_CH(tag, ch_num); 
			break;
		case SUB7_BS_NUM: 
			FN_SET_SUB7_REC_CH(tag, ch_num); 
			break;
		case SUB8_BS_NUM: 
			FN_SET_SUB8_REC_CH(tag, ch_num); 
			break;
		default: 
			printf("%s: ch_num=%d, sub_num %d failed!\n", __FUNCTION__, ch_num, sub_num); 
	}
}

static inline void cfgud_init(int ch_num, int sub_num)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->stream_enable = GMVAL_DO_NOT_CARE; 
    ubs->enc_type = GMVAL_DO_NOT_CARE; 
    ubs->frame_rate = GMVAL_DO_NOT_CARE; 
    ubs->bit_rate = GMVAL_DO_NOT_CARE; 
    ubs->ip_interval = GMVAL_DO_NOT_CARE; 
    ubs->width = GMVAL_DO_NOT_CARE; 
    ubs->height = GMVAL_DO_NOT_CARE; 
    ubs->rate_ctl_type = GMVAL_DO_NOT_CARE; 
    ubs->target_rate_max = GMVAL_DO_NOT_CARE; 
    ubs->reaction_delay_max = GMVAL_DO_NOT_CARE; 
    ubs->init_quant = GMVAL_DO_NOT_CARE; 
    ubs->max_quant = GMVAL_DO_NOT_CARE; 
    ubs->min_quant = GMVAL_DO_NOT_CARE; 
    ubs->mjpeg_quality = GMVAL_DO_NOT_CARE; 
    ubs->enabled_roi = GMVAL_DO_NOT_CARE; 
    ubs->roi_y = GMVAL_DO_NOT_CARE; 
    ubs->roi_x = GMVAL_DO_NOT_CARE; 
    ubs->roi_w = GMVAL_DO_NOT_CARE; 
    ubs->roi_h = GMVAL_DO_NOT_CARE; 
}

static inline void cfgud_stream_enable(int ch_num, int sub_num, int stream_enable)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->stream_enable = stream_enable;
}

static inline void cfgud_mjpeg_quality(int ch_num, int sub_num, int mjpeg_quality)
{
    update_avbs_t *ubs;

	if(mjpeg_quality<1 || mjpeg_quality >100) {
	    printf("mjpeg quality fail!\n");
	    printf("mjpeg quality val 1~100, input val=%d\n", mjpeg_quality);
	}
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->mjpeg_quality = mjpeg_quality; 
}

static inline void cfgud_rate_ctl_type(int ch_num, int sub_num, int rate_ctl_type)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->rate_ctl_type = rate_ctl_type;
}

static inline void cfgud_bps(int ch_num, int sub_num, int bit_rate)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->bit_rate = bit_rate;
}

static inline void cfgud_trm(int ch_num, int sub_num, int target_rate_max)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->target_rate_max = target_rate_max;
}

static inline void cfgud_rdm(int ch_num, int sub_num, int reaction_delay_max)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->reaction_delay_max = reaction_delay_max;
}

static inline int cfgud_enc_type(int ch_num, int sub_num, int enc_type)
{
    update_avbs_t *ubs;

	if(enc_type < ENC_TYPE_H264 || enc_type >= ENC_TYPE_YUV422) {
	    printf("%s: ch=%d sub=%d, Input codec (%d) is illegal. (0:H264, 1:MPEG4, 2:MJPEG)\n",
	                            __FUNCTION__,
	                            ch_num,
	                            sub_num,
	                            enc_type);
        goto err_exit;
    }
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->enc_type = enc_type;
    return 0;

err_exit: 
    return -1;
}

static inline void cfgud_iniQ(int ch_num, int sub_num, int enc_type, int init_quant)
{
    update_avbs_t *ubs;
	switch (enc_type) {
		case ENC_TYPE_H264:
        	if ((init_quant > 51) || (init_quant < 1)) {
        	    printf("%s: ch=%d sub=%d, Input init quant (%d) is illegal. (h264 max:51,min:1)\n",
        	                            __FUNCTION__,
        	                            ch_num,
        	                            sub_num,
        	                            init_quant);
        	    goto err_exit;
            }
            break;
		case ENC_TYPE_MPEG:
        	if ((init_quant > 31) || (init_quant < 1)) {
        	    printf("%s: ch=%d sub=%d, Input init quant (%d) is illegal. (mpeg4 max:31,min:1)\n",
        	                            __FUNCTION__,
        	                            ch_num,
        	                            sub_num,
        	                            init_quant);
        	    goto err_exit;
            }
            break;
		case ENC_TYPE_MJPEG:
        default:
    	    printf("%s: ch=%d sub=%d, Codec (%d) is illegal.)\n",
    	                            __FUNCTION__,
    	                            ch_num,
    	                            sub_num,
    	                            enc_type);
            goto err_exit;
    }	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->init_quant = init_quant;
    return;

err_exit:
    return;
}

static inline void cfgud_maxQ(int ch_num, int sub_num, int enc_type, int max_quant)
{
    update_avbs_t *ubs;
	switch (enc_type) {
		case ENC_TYPE_H264:
        	if ((max_quant > 51) || (max_quant < 1)) {
        	    printf("%s: ch=%d sub=%d, Input max quant (%d) is illegal. (h264 max:51,min:1)\n",
        	                            __FUNCTION__,
        	                            ch_num,
        	                            sub_num,
        	                            max_quant);
        	    goto err_exit;
            }
            break;
		case ENC_TYPE_MPEG:
        	if ((max_quant > 31) || (max_quant < 1)) {
        	    printf("%s: ch=%d sub=%d, Input max quant (%d) is illegal. (mpeg4 max:31,min:1)\n",
        	                            __FUNCTION__,
        	                            ch_num,
        	                            sub_num,
        	                            max_quant);
        	    goto err_exit;
            }
            break;
		case ENC_TYPE_MJPEG:
        default:
    	    printf("%s: ch=%d sub=%d, Codec (%d) is illegal.)\n",
    	                            __FUNCTION__,
    	                            ch_num,
    	                            sub_num,
    	                            enc_type);
            goto err_exit;
    }	
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->max_quant = max_quant;
    return;

err_exit:
    return;
}

static inline void cfgud_minQ(int ch_num, int sub_num, int enc_type, int min_quant)
{
    update_avbs_t *ubs;
	switch (enc_type) {
		case ENC_TYPE_H264:
        	if ((min_quant > 51) || (min_quant < 1)) {
        	    printf("%s: ch=%d sub=%d, Input min quant (%d) is illegal. (h264 max:51,min:1)\n",
        	                            __FUNCTION__,
        	                            ch_num,
        	                            sub_num,
        	                            min_quant);
        	    goto err_exit;
            }
            break;
		case ENC_TYPE_MPEG:
        	if ((min_quant > 31) || (min_quant < 1)) {
        	    printf("%s: ch=%d sub=%d, Input min quant (%d) is illegal. (mpeg4 max:31,min:1)\n",
        	                            __FUNCTION__,
        	                            ch_num,
        	                            sub_num,
        	                            min_quant);
        	    goto err_exit;
            }
            break;
		case ENC_TYPE_MJPEG:
        default:
    	    printf("%s: ch=%d sub=%d, Codec (%d) is illegal.)\n",
    	                            __FUNCTION__,
    	                            ch_num,
    	                            sub_num,
    	                            enc_type);
            goto err_exit;
    }	
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->min_quant = min_quant;
    return;

err_exit:
    return;
}

static inline int cfgud_res(int ch_num, int sub_num, int width, int height)
{
    update_avbs_t *ubs;
	
	if((width%16) || (height%16)) {
		fprintf(stderr, "set_bs_res: width=%d, height=%d error!\n", width, height);
		return -1;
	}

	ubs = &enc[ch_num].ubs[sub_num];
    ubs->width= width;
    ubs->height = height;
    return 0;
}

static inline void cfgud_fps(int ch_num, int sub_num, int fps)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->frame_rate = fps;
}

static inline void cfgud_enabled_roi(int ch_num, int sub_num, int enabled_roi)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->enabled_roi= enabled_roi;
}

static inline void cfgud_roi_x(int ch_num, int sub_num, int roi_x)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->roi_x= roi_x;
}

static inline void cfgud_roi_y(int ch_num, int sub_num, int roi_y)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->roi_y= roi_y;
}

static inline void cfgud_roi_w(int ch_num, int sub_num, int roi_w)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->roi_w= roi_w;
}

static inline void cfgud_roi_h(int ch_num, int sub_num, int roi_h)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->roi_h= roi_h;
}

static inline void cfgud_ip_interval(int ch_num, int sub_num, int ip_interval)
{
    update_avbs_t *ubs;
	
	ubs = &enc[ch_num].ubs[sub_num];
    ubs->ip_interval = ip_interval;
}

#define NEW_ROI_FUNC
static int cfgud_chk_parm(int ch_num, int sub_num)
{
    update_avbs_t *ubs;
	avbs_t *b;
	
	ubs = &enc[ch_num].ubs[sub_num];
	b = &enc[ch_num].bs[sub_num];

	if(b->opt_type == OPT_NONE) {
	    printf("This channel %d:%d is disable that cannot be change parameter.\n", ch_num, sub_num);
	    goto err_exit;
	}

	if(ubs->stream_enable != GMVAL_DO_NOT_CARE) {
	    if(ubs->stream_enable) {
    	    b->video.enabled = DVR_ENC_EBST_ENABLE;
    	    b->enabled = DVR_ENC_EBST_ENABLE;
    	} else {
    	    b->video.enabled = DVR_ENC_EBST_DISABLE;
    	    b->enabled = DVR_ENC_EBST_DISABLE;
    	}
	}
	if(ubs->enc_type != GMVAL_DO_NOT_CARE) {
	    b->video.enc_type = ubs->enc_type;
	}
	if(ubs->frame_rate != GMVAL_DO_NOT_CARE) {
	    b->video.fps = ubs->frame_rate;
	}
	if(ubs->bit_rate != GMVAL_DO_NOT_CARE) {
	    b->video.bps = ubs->bit_rate;
	}
	if(ubs->ip_interval != GMVAL_DO_NOT_CARE) {
	    b->video.ip_interval = ubs->ip_interval;
	}
	if(ubs->width != GMVAL_DO_NOT_CARE) {
	    b->video.width = ubs->width;
	}
	if(ubs->height != GMVAL_DO_NOT_CARE) {
	    b->video.height = ubs->height;
	}
	if(ubs->rate_ctl_type != GMVAL_DO_NOT_CARE) {
	    b->video.rate_ctl_type = ubs->rate_ctl_type;
	}
	if(ubs->target_rate_max != GMVAL_DO_NOT_CARE) {
	    b->video.target_rate_max = ubs->target_rate_max;
	}
	if(ubs->reaction_delay_max != GMVAL_DO_NOT_CARE) {
	    b->video.reaction_delay_max = ubs->reaction_delay_max;
	}
	if(ubs->init_quant != GMVAL_DO_NOT_CARE) {
	    b->video.init_quant = ubs->init_quant;
	}
	if(ubs->max_quant != GMVAL_DO_NOT_CARE) {
	    b->video.max_quant = ubs->max_quant;
	}
	if(ubs->min_quant != GMVAL_DO_NOT_CARE) {
	    b->video.min_quant = ubs->min_quant;
	}
	if(ubs->mjpeg_quality != GMVAL_DO_NOT_CARE) {
	    b->video.mjpeg_quality = ubs->mjpeg_quality;
	}
#ifdef NEW_ROI_FUNC
	if(ubs->enabled_roi != GMVAL_DO_NOT_CARE) {
	    b->video.enabled_roi = ubs->enabled_roi;
	}
	if(ubs->roi_w != GMVAL_DO_NOT_CARE) {
	    b->video.roi_w = ubs->roi_w;
	}
	if(ubs->roi_h != GMVAL_DO_NOT_CARE) {
	    b->video.roi_h = ubs->roi_h;
	}
#endif
	if(ubs->roi_x != GMVAL_DO_NOT_CARE) {
	    b->video.roi_x = ubs->roi_x;
	}
	if(ubs->roi_y != GMVAL_DO_NOT_CARE) {
	    b->video.roi_y = ubs->roi_y;
	}

	return 0;
err_exit: 
    return -1; 
}

static void update_bs(int ch_num, int sub_num)
{
	av_t *e;
	dvr_enc_control	enc_parm;
    dvr_enc_update_channel_param *uparm;
	update_avbs_t *ubs;
	EncParam_Ext5 enc_param_ext = {0};
    FuncTag tag;        

	e = &enc[ch_num];
	ubs = &e->ubs[sub_num];
	e->priv_bs[sub_num].video.avg_bps = 0;
	e->priv_bs[sub_num].video.avg_fps= 0;
    memset(&enc_parm, 0x0, sizeof(dvr_enc_control));    
    enc_parm.stream = sub_num;
    uparm = &enc_parm.update_parm;
    uparm->stream_enable = ubs->stream_enable;
    uparm->bit_rate = ubs->bit_rate;
    uparm->frame_rate = ubs->frame_rate;
    uparm->ip_interval = ubs->ip_interval;
    uparm->dim.width = ubs->width;  
    uparm->dim.height = ubs->height;
    uparm->src.di_mode = GMVAL_DO_NOT_CARE;
    uparm->src.mode = GMVAL_DO_NOT_CARE;
    uparm->src.scale_indep = GMVAL_DO_NOT_CARE;
    uparm->src.is_3DI = GMVAL_DO_NOT_CARE;
    uparm->src.is_denoise = GMVAL_DO_NOT_CARE;
    uparm->bit_rate = ubs->bit_rate;
    uparm->init_quant = ubs->init_quant;
    uparm->max_quant = ubs->max_quant;
    uparm->min_quant = ubs->min_quant;

    /* extend function update */
	uparm->ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
	uparm->pext_data = &enc_param_ext;
    enc_param_ext.target_rate_max = ubs->target_rate_max;
    enc_param_ext.reaction_delay_max = ubs->reaction_delay_max;
    enc_param_ext.enc_type = ubs->enc_type;
    enc_param_ext.MJ_quality = ubs->mjpeg_quality;
#ifdef NEW_ROI_FUNC
    enc_param_ext.roi_all.is_use_ROI= ubs->enabled_roi;
    enc_param_ext.roi_all.win.x = ubs->roi_x;
    enc_param_ext.roi_all.win.y = ubs->roi_y;
    enc_param_ext.roi_all.win.width= ubs->roi_w;
    enc_param_ext.roi_all.win.height = ubs->roi_h;
#else
    enc_param_ext.roi_pos.x = ubs->roi_x;
    enc_param_ext.roi_pos.y = ubs->roi_y;
#endif
    
	enc_param_ext.feature_enable = 0;
	/* update mjpeg quality */
	if(ubs->mjpeg_quality != GMVAL_DO_NOT_CARE) {
	    enc_param_ext.feature_enable |= DVR_ENC_MJPEG_FUNCTION;
	} 
    /* update codec change */
	if((ubs->enc_type != GMVAL_DO_NOT_CARE) && (ubs->stream_enable == 1)) {
        enc_param_ext.feature_enable |= DVR_ENC_MJPEG_FUNCTION;
	}
	/* update rate control */
	if(ubs->rate_ctl_type != GMVAL_DO_NOT_CARE) {
        enc_param_ext.feature_enable |= DVR_ENC_ENHANCE_H264_RATECONTROL;
    }
#ifdef NEW_ROI_FUNC
	/* update ROI coordinate x, y */
	if((ubs->roi_x != GMVAL_DO_NOT_CARE) || (ubs->roi_y != GMVAL_DO_NOT_CARE)
	                                     || (ubs->roi_w != GMVAL_DO_NOT_CARE)
	                                     || (ubs->roi_h != GMVAL_DO_NOT_CARE)
	                                     || (ubs->enabled_roi != GMVAL_DO_NOT_CARE)) {
        enc_param_ext.feature_enable |= DVR_ENC_ROI_ALL;
    }
#else
	/* update ROI coordinate x, y */
	if((ubs->roi_x != GMVAL_DO_NOT_CARE) || (ubs->roi_y != GMVAL_DO_NOT_CARE)) {
        enc_param_ext.feature_enable |= DVR_ENC_ROI_POS;
    }
#endif
    /* no feature enable */
    if(enc_param_ext.feature_enable == 0)
	    enc_param_ext.feature_enable = GMVAL_DO_NOT_CARE;
        
    enc_parm.command = ENC_UPDATE;
    ioctl(e->enc_fd, DVR_ENC_CONTROL, &enc_parm);
    set_tag(ch_num, sub_num, &tag);
    ioctl(rtspd_dvr_fd, DVR_COMMON_APPLY, &tag);                          
}

static int set_bs_intra_frame(int ch_num, int sub_num)
{
	av_t *e;
	
	e = &enc[ch_num];

	if(ioctl(e->enc_fd, DVR_ENC_RESET_INTRA, &sub_num)) {
	    perror("set_bs_intra_frame : error to use DVR_ENC_RESET_INTRA\n");
        return -1;
	}
	return 0;
}

static int apply_bs(int ch_num, int sub_num, int command)
{
	av_t *e;
    FuncTag tag;
    dvr_enc_control enc_ctrl;
	
	enc_ctrl.command = command;
	enc_ctrl.stream = sub_num;
	e = &enc[ch_num];
	if(ioctl(e->enc_fd, DVR_ENC_CONTROL, &enc_ctrl)<0) {
	    perror("apply_bs : error to use DVR_ENC_CONTROL\n");
        return -1;
	}
    set_tag(ch_num, sub_num, &tag);
	if(ioctl(rtspd_dvr_fd, DVR_COMMON_APPLY, &tag)<0) {
	    perror("apply_bs: Error to use DVR_COMMON_APPLY\n");
        return -1;
	}
	return 0;
}

#if 0
static inline int apply_main_bs(int ch_num, int command)
{
	return(apply_bs(ch_num, MAIN_BS_NUM, command));
}

static inline int apply_sub1_bs(int ch_num, int command)
{
	return(apply_bs(ch_num, SUB1_BS_NUM, command));
}

static inline int apply_sub2_bs(int ch_num, int command)
{
	return(apply_bs(ch_num, SUB2_BS_NUM, command));
}
#endif

int is_bs_all_disable(void)
{
	av_t *e;
    int ch_num, sub_num;

    for(ch_num=0; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
	    e = &enc[ch_num];
        for(sub_num=0; sub_num < DVR_ENC_REPD_BT_NUM; sub_num++) {
        	if(e->bs[sub_num].enabled == DVR_ENC_EBST_ENABLE) return 0;  /* already enabled */
        }
    }
    return 1;
}

static inline int check_bs_param(int ch_num, int sub_num)
{
	av_t *e;
	int fps, width, height, enc_type, opt_type, bps;
	
	if((ch_num >= DVR_RECORD_CHANNEL_NUM || ch_num < 0) || 
		(sub_num >= DVR_ENC_REPD_BT_NUM || sub_num < 0)) {
		fprintf(stderr, "check_bs_param: ch_num=%d, sub_num=%d error!\n", ch_num, sub_num);
		return -1;
	}
	
	e = &enc[ch_num];
	fps = e->bs[sub_num].video.fps;
	bps = e->bs[sub_num].video.bps;
	width = e->bs[sub_num].video.width;
	height = e->bs[sub_num].video.height;
	enc_type = e->bs[sub_num].video.enc_type;
	opt_type = e->bs[sub_num].opt_type;

	if((opt_type < RTSP_LIVE_STREAMING) || (opt_type>FILE_H264_RECORDING)) {
		fprintf(stderr, "check_bs_param: ch_num=%d, sub_num=%d, opt_type=%d error!\n", ch_num, sub_num, opt_type);
		return -1;
	}
	if((enc_type < ENC_TYPE_H264) || (enc_type > ENC_TYPE_MJPEG)) {
		fprintf(stderr, "check_bs_param: ch_num=%d, sub_num=%d, enc_type=%d error!\n", ch_num, sub_num, enc_type);
		return -1;
	}
	if(fps > 30 || fps <= 0) {
		fprintf(stderr, "check_bs_param: ch_num=%d, sub_num=%d, fps=%d error!\n", ch_num, sub_num, fps);
		return -1;
	}
	if(bps < 0) {
		fprintf(stderr, "check_bs_param: ch_num=%d, sub_num=%d, bps=%d error!\n", ch_num, sub_num, bps);
		return -1;
	}
//++ fullhd
#if 0
	if((width%16) || (height%16) || (width<=0) || (height<=0)) {
		fprintf(stderr, "check_bs_param: ch_num=%d, sub_num=%d, width=%d, height=%d error!\n", ch_num, sub_num, width, height);
		return -1;
	}
#endif
//-- fullhd
	return 0;
}

#if 0
static inline int check_main_bs_param(int ch_num)
{
	return(check_bs_param(ch_num, MAIN_BS_NUM));
}

static inline int check_sub1_bs_param(int ch_num)
{
	int ret=0;
	av_t *e;
	
	if ((ret = check_bs_param(ch_num, SUB1_BS_NUM)) < 0) goto err_exit;
	e = &enc[ch_num];
	if(e->enc_fd == 0) {
		fprintf(stderr, "check_sub1_bs_param: ch_num=%d, enc_fd=%d error!\n", ch_num, e->enc_fd);
		ret = -1;
	}
	
err_exit:
	return ret;
}

static inline int check_sub2_bs_param(int ch_num)
{
	int ret=0;
	av_t *e;
	
	if ((ret = check_bs_param(ch_num, SUB2_BS_NUM)) < 0) goto err_exit;
	e = &enc[ch_num];
	if(e->enc_fd == 0) {
		fprintf(stderr, "check_sub2_bs_param: ch_num=%d, enc_fd=%d error!\n", ch_num, e->enc_fd);
		ret = -1;
	}
	
err_exit:
	return ret;
}
#endif

static int set_bs_buf(int ch_num)
{
	av_t *e;
	int i;

	e = &enc[ch_num];
	if (ioctl(e->enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SIZE, &e->v_bs_mbuf_size)<0) {
		perror("open_main_bs query output buffer error");
        return -1;
	}

	e->priv_bs[0].video.offs=0;
    for(i=1; i<DVR_ENC_REPD_BT_NUM; i++) {
    	if(ioctl(e->enc_fd, dvr_enc_queue_offset_def_table[i], &e->priv_bs[i].video.offs)<0) {
    	    printf("%s:%d <open_main_bs sub%d error>\n",__FUNCTION__,__LINE__, i);
            return -1;
    	}
    }
	e->v_bs_mbuf_addr = (unsigned char*) mmap(NULL, e->v_bs_mbuf_size, PROT_READ|PROT_WRITE, MAP_SHARED, e->enc_fd, 0);
    if(e->v_bs_mbuf_addr==MAP_FAILED) {
		perror("Enc mmap failed");
        return -1;
	}
    return 0;
}

#if 0
static void cif_res(dvr_enc_channel_param *ch_param)
{
  	ch_param->src.di_mode = LVFRAME_EVEN_ODD;
   	ch_param->src.vp_param.is_3DI = FALSE;
  	ch_param->src.mode = LVFRAME_FIELD_MODE;
   	ch_param->src.dma_order = DMAORDER_PACKET;
  	ch_param->src.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
  	ch_param->src.color_mode = CAPCOLOR_YUV422;
   	ch_param->src.vp_param.is_denoise = TRUE;
  	ch_param->src.vp_param.denoise_mode = GM3DI_FIELD;
}

static void d1_res(dvr_enc_channel_param *ch_param)
{
	ch_param->src.di_mode = LVFRAME_GM3DI_FORMAT;
	ch_param->src.vp_param.is_3DI = TRUE;
	ch_param->src.mode = LVFRAME_FRAME_MODE;
	ch_param->src.dma_order = DMAORDER_PACKET;
	ch_param->src.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
	ch_param->src.color_mode = CAPCOLOR_YUV422;
	ch_param->src.vp_param.is_denoise = TRUE;
	ch_param->src.vp_param.denoise_mode = GM3DI_FIELD;
}

static void chk_res(dvr_enc_channel_param *ch_param)
{
	if(ch_param->src.dim.width == 720) d1_res(ch_param);
	else cif_res(ch_param);
}
#endif

static void set_main_3di(int ch_num, dvr_enc_channel_param *ch_param)
{
	av_t *e;
    
	e = &enc[ch_num];

	if(e->input_system == CMOS) {
    	ch_param->src.vp_param.is_3DI = FALSE;
    	ch_param->src.vp_param.denoise_mode = GM3DI_FRAME;
        if(e->denoise == 1) 
            ch_param->src.vp_param.is_denoise = TRUE;
        else
            ch_param->src.vp_param.is_denoise = FALSE;

    } else {  /* PAL, NTSC system */
       	ch_param->src.dma_order = DMAORDER_PACKET;
      	ch_param->src.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
      	ch_param->src.color_mode = CAPCOLOR_YUV422;
	    ch_param->src.vp_param.denoise_mode = GM3DI_FIELD;
    	if(ch_param->src.dim.width == 720) {   /* D1 */
        	ch_param->src.di_mode = LVFRAME_GM3DI_FORMAT;
        	ch_param->src.mode = LVFRAME_FRAME_MODE;
        	if(e->de_interlace == 1) {
            	ch_param->src.vp_param.is_denoise = TRUE;
            	ch_param->src.vp_param.is_3DI = TRUE;
        	} else if(e->denoise == 1) {
            	ch_param->src.vp_param.is_denoise = TRUE;
            	ch_param->src.vp_param.is_3DI = FALSE;
        	} else {
            	ch_param->src.vp_param.is_denoise = FALSE;
            	ch_param->src.vp_param.is_3DI = FALSE;
        	}
    	} else {     /* cif, qcif */
          	ch_param->src.di_mode = LVFRAME_EVEN_ODD;
          	ch_param->src.mode = LVFRAME_FIELD_MODE;
    	    if(e->denoise == 1) 
               	ch_param->src.vp_param.is_denoise = TRUE;
            else 
               	ch_param->src.vp_param.is_denoise = FALSE;
           	ch_param->src.vp_param.is_3DI = FALSE;
    	}
    }
}

static int set_main_bs(int ch_num)
{
    dvr_enc_channel_param ch_param;
	int width, height;
	av_t *e;
    EncParam_Ext3 enc_param_ext = {0};

	if(check_bs_param(ch_num, MAIN_BS_NUM)<0) return -1;

	e = &enc[ch_num];
	if(e->bs[0].enabled == DVR_ENC_EBST_ENABLE) return -1;  /* already enabled */
	if(e->enc_fd > 0) return -1;
	if((e->enc_fd=open_channel(ch_num)) < 0) return -1;
	width = e->bs[0].video.width;
	height = e->bs[0].video.height;
	memcpy(&ch_param, &main_ch_setting, sizeof(ch_param));
    switch(e->input_system) {
        case NTSC: ch_param.src.input_system = MCP_VIDEO_NTSC; break;
        case PAL: ch_param.src.input_system = MCP_VIDEO_PAL; break;
        case CMOS: default: ch_param.src.input_system = MCP_VIDEO_NTSC; break;
    }
    ch_param.src.channel = ch_num;
	ch_param.src.dim.width = ch_param.main_bs.dim.width = width;
	ch_param.src.dim.height = ch_param.main_bs.dim.height = height;
	e->bs[0].video.fps = ((ch_param.src.input_system == MCP_VIDEO_PAL)&&(e->bs[0].video.fps > 25)) ? 25 : e->bs[0].video.fps;
	ch_param.main_bs.enc_type = e->bs[0].video.enc_type;
	ch_param.main_bs.enc.frame_rate = e->bs[0].video.fps;
	//ch_param.main_bs.enc.ip_interval= e->bs[0].video.fps*2;  // 2 seconds 
	ch_param.main_bs.enc.ip_interval= e->bs[0].video.ip_interval;  // 2 seconds 
	ch_param.main_bs.enc.bit_rate = (e->bs[0].video.bps <= 0) ? get_bitrate(width, height, e->bs[0].video.fps) : e->bs[0].video.bps;
    if(e->bs[0].video.enabled_roi == 1) {
        ch_param.main_bs.enc.is_use_ROI = TRUE;
        ch_param.main_bs.enc.ROI_win.x = e->bs[0].video.roi_x;
        ch_param.main_bs.enc.ROI_win.y = e->bs[0].video.roi_y;
        ch_param.main_bs.enc.ROI_win.width = e->bs[0].video.roi_w;
        ch_param.main_bs.enc.ROI_win.height = e->bs[0].video.roi_h;
    } else {
        ch_param.main_bs.enc.is_use_ROI = FALSE;
    }
    ch_param.main_bs.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    ch_param.main_bs.enc.pext_data = &enc_param_ext;
    enc_param_ext.feature_enable = 0;

	switch (ch_param.main_bs.enc_type) {
		case ENC_TYPE_MJPEG: 
	        enc_param_ext.feature_enable |= DVR_ENC_MJPEG_FUNCTION;
			enc_param_ext.MJ_quality = e->bs[0].video.mjpeg_quality;
			break;
		case ENC_TYPE_H264:
		case ENC_TYPE_MPEG:
			enc_param_ext.feature_enable &= ~DVR_ENC_MJPEG_FUNCTION;
			break;
		default: 
            printf(" Encoder not support! (%d)\n", ch_param.main_bs.enc_type);
			ch_param.main_bs.enc_type = ENC_TYPE_H264;
			break;
	}
	switch(e->bs[0].video.rate_ctl_type) {
		case RATE_CTL_ECBR:
	        ch_param.main_bs.enc.bit_rate = (e->bs[0].video.bps <= 0) ? get_bitrate(width, height, e->bs[0].video.fps) : e->bs[0].video.bps;
	        enc_param_ext.feature_enable |= DVR_ENC_ENHANCE_H264_RATECONTROL;
	        enc_param_ext.target_rate_max = e->bs[0].video.target_rate_max;
	        enc_param_ext.reaction_delay_max = e->bs[0].video.reaction_delay_max;
	        ch_param.main_bs.enc.max_quant = e->bs[0].video.max_quant;
	        ch_param.main_bs.enc.min_quant = e->bs[0].video.min_quant;
			ch_param.main_bs.enc.init_quant = e->bs[0].video.init_quant; 
			break;
		case RATE_CTL_EVBR:
	        enc_param_ext.feature_enable |= DVR_ENC_ENHANCE_H264_RATECONTROL;
	        ch_param.main_bs.enc.bit_rate = 0;
	        enc_param_ext.target_rate_max = e->bs[0].video.target_rate_max;
	        enc_param_ext.reaction_delay_max = 0;
	        ch_param.main_bs.enc.max_quant = e->bs[0].video.max_quant; 
	        ch_param.main_bs.enc.min_quant = e->bs[0].video.min_quant; 
	        ch_param.main_bs.enc.init_quant = e->bs[0].video.init_quant; 
	        break;
		case RATE_CTL_VBR:
	        ch_param.main_bs.enc.max_quant = e->bs[0].video.init_quant; 
	        ch_param.main_bs.enc.min_quant = e->bs[0].video.init_quant; 
	        ch_param.main_bs.enc.init_quant = e->bs[0].video.init_quant; 
	        break;
		case RATE_CTL_CBR:
		default: 
	        ch_param.main_bs.enc.bit_rate = (e->bs[0].video.bps <= 0) ? get_bitrate(width, height, e->bs[0].video.fps) : e->bs[0].video.bps;
	        ch_param.main_bs.enc.max_quant = e->bs[0].video.max_quant;
	        ch_param.main_bs.enc.min_quant = e->bs[0].video.min_quant;
			ch_param.main_bs.enc.init_quant = e->bs[0].video.init_quant; 
			break;
	}
	e->bs[0].enabled = DVR_ENC_EBST_ENABLE;
	e->bs[0].video.enabled = DVR_ENC_EBST_ENABLE;
    set_main_3di(ch_num, &ch_param);
	if(ioctl(e->enc_fd, DVR_ENC_SET_CHANNEL_PARAM, &ch_param)<0) {
		perror("set_main_bs: set channel param error");
        return -1;
	}
	if(set_bs_buf(ch_num)<0) return -1;
    return 0;
}

static int set_sub_bs(int ch_num, int sub_num)
{
	ReproduceBitStream sub_bs;
	int width, height, fps;
	av_t *e;
    EncParam_Ext3 enc_param_ext = {0};

	if(check_bs_param(ch_num, sub_num)<0) return -1;
	
	e = &enc[ch_num];
	if(e->bs[sub_num].video.enabled == DVR_ENC_EBST_ENABLE) return 0;
    memcpy(&sub_bs, &sub_ch_setting, sizeof(ReproduceBitStream));
	sub_bs.out_bs = sub_num;
	sub_bs.enabled = DVR_ENC_EBST_ENABLE;
	sub_bs.dim.width = width = e->bs[sub_num].video.width;
	sub_bs.dim.height = height = e->bs[sub_num].video.height;
	sub_bs.enc.frame_rate = fps = e->bs[sub_num].video.fps;
//	sub_bs.enc.ip_interval= fps*2;
	sub_bs.enc.ip_interval= e->bs[sub_num].video.ip_interval;
//	sub_bs.enc.bit_rate = (e->bs[sub_num].video.bps <= 0) ? get_bitrate(width, height, fps) : e->bs[sub_num].video.bps;
	sub_bs.enc_type = e->bs[sub_num].video.enc_type;

    if(e->bs[sub_num].video.enabled_roi == 1) {
        sub_bs.enc.is_use_ROI = TRUE;
        sub_bs.enc.ROI_win.x = e->bs[sub_num].video.roi_x;
        sub_bs.enc.ROI_win.y = e->bs[sub_num].video.roi_y;
        sub_bs.enc.ROI_win.width = e->bs[sub_num].video.roi_w;
        sub_bs.enc.ROI_win.height = e->bs[sub_num].video.roi_h;
    } else {
        sub_bs.enc.is_use_ROI = FALSE;
    }

    sub_bs.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    sub_bs.enc.pext_data = &enc_param_ext;
    enc_param_ext.feature_enable = 0;
	switch (sub_bs.enc_type) {
		case ENC_TYPE_MJPEG: 
	        enc_param_ext.feature_enable |= DVR_ENC_MJPEG_FUNCTION;
			enc_param_ext.MJ_quality = e->bs[sub_num].video.mjpeg_quality;
			break;
		case ENC_TYPE_H264:
		case ENC_TYPE_MPEG:
			enc_param_ext.feature_enable &= ~DVR_ENC_MJPEG_FUNCTION;
			break;
		default: 
            printf(" Encoder not support! (%d)\n", sub_bs.enc_type);
			sub_bs.enc_type = ENC_TYPE_H264;
			break;
	}

	switch(e->bs[sub_num].video.rate_ctl_type) {
		case RATE_CTL_ECBR:
            sub_bs.enc.bit_rate = (e->bs[sub_num].video.bps <= 0) ? get_bitrate(width, height, fps) : e->bs[sub_num].video.bps;
	        enc_param_ext.feature_enable |= DVR_ENC_ENHANCE_H264_RATECONTROL;
	        enc_param_ext.target_rate_max = e->bs[sub_num].video.target_rate_max;
	        enc_param_ext.reaction_delay_max = e->bs[sub_num].video.reaction_delay_max;
	        sub_bs.enc.max_quant = e->bs[sub_num].video.max_quant;
	        sub_bs.enc.min_quant = e->bs[sub_num].video.min_quant;
			sub_bs.enc.init_quant = e->bs[sub_num].video.init_quant; 
			break;
		case RATE_CTL_EVBR:
	        enc_param_ext.feature_enable |= DVR_ENC_ENHANCE_H264_RATECONTROL;
	        sub_bs.enc.bit_rate = 0;
	        enc_param_ext.target_rate_max = e->bs[sub_num].video.target_rate_max;
	        enc_param_ext.reaction_delay_max = 0;
	        sub_bs.enc.max_quant = e->bs[sub_num].video.max_quant; 
	        sub_bs.enc.min_quant = e->bs[sub_num].video.min_quant; 
	        sub_bs.enc.init_quant = e->bs[sub_num].video.init_quant; 
	        break;
		case RATE_CTL_VBR:
	        sub_bs.enc.max_quant = e->bs[sub_num].video.init_quant; 
	        sub_bs.enc.min_quant = e->bs[sub_num].video.init_quant; 
	        sub_bs.enc.init_quant = e->bs[sub_num].video.init_quant; 
	        break;
		case RATE_CTL_CBR:
		default: 
            sub_bs.enc.bit_rate = (e->bs[sub_num].video.bps <= 0) ? get_bitrate(width, height, fps) : e->bs[sub_num].video.bps;
	        sub_bs.enc.max_quant = e->bs[sub_num].video.max_quant;
	        sub_bs.enc.min_quant = e->bs[sub_num].video.min_quant;
			sub_bs.enc.init_quant = e->bs[sub_num].video.init_quant; 
			break;
	}
	if(ioctl(e->enc_fd, DVR_ENC_SET_SUB_BS_PARAM, &sub_bs)<0) {
		perror("set_sub_bs");
        return -1;
	}
	e->bs[sub_num].enabled = DVR_ENC_EBST_ENABLE;
	e->bs[sub_num].video.enabled = DVR_ENC_EBST_ENABLE;

	return 0;
}

#if 0
static int set_sub1_bs(int ch_num)
{
	return (set_sub_bs(ch_num, SUB1_BS_NUM));	
}

static int set_sub2_bs(int ch_num)
{
	return (set_sub_bs(ch_num, SUB2_BS_NUM));	
}
#endif

static int get_bs_data(int ch_num, int sub_num, dvr_enc_queue_get *data)
{
	av_t *e;
	int ret=0, diff_tv, time_c;
	priv_avbs_t	*pb;

	if((ch_num >= DVR_RECORD_CHANNEL_NUM || ch_num < 0) || 
		(sub_num >= DVR_ENC_REPD_BT_NUM || sub_num < 0)) {
		fprintf(stderr, "get_bs_data: ch_num=%d, sub_num=%d error!\n", ch_num, sub_num);
		return -1;
	}
	e = &enc[ch_num];
    ret = ioctl(e->enc_fd, dvr_enc_queue_get_def_table[sub_num], data);

	if(ret >= 0) {
 		pb = &enc[ch_num].priv_bs[sub_num];
		pb->video.buf_usage++; 
	#if PRINT_ENC_AVERAGE
		pb->video.itvl_fps++;
		pb->video.itvl_bps +=  data->bs.length;
		time_c = get_tick(NULL);
		if(pb->video.timeval == 0) {
			pb->video.timeval = time_c;
		} else {
			diff_tv = timediff(pb->video.timeval, time_c);

			if (diff_tv >= TIME_INTERVAL) {
				pb->video.avg_fps = pb->video.itvl_fps * 9000000 / diff_tv;
				pb->video.avg_bps = pb->video.itvl_bps * 80 / (diff_tv / 9000);
				print_enc_average(time_c);
				pb->video.itvl_fps = 0;
				pb->video.itvl_bps = 0;
				pb->video.timeval = time_c;
			}
		}
	#endif
	}
	return ret;
}

static int free_bs_data(int ch_num, int sub_num, dvr_enc_queue_get *data)
{
	av_t *e;
	int ret;
	
	if((ch_num >= DVR_RECORD_CHANNEL_NUM || ch_num < 0) || 
		(sub_num >= DVR_ENC_REPD_BT_NUM || sub_num < 0)) {
		fprintf(stderr, "free_bs_data: ch_num=%d, sub_num=%d error!\n", ch_num, sub_num);
		return -1;
	}
	e = &enc[ch_num];

	ret = ioctl(e->enc_fd, DVR_ENC_QUEUE_PUT, data);
	if(ret<0) 
		fprintf(stderr, "free_bs_data sub%d_bitstream failed...\n", sub_num);
	else
		e->priv_bs[sub_num].video.buf_usage--;
	
	return ret;
}

static int open_live_streaming(int ch_num, int sub_num)
{
	int media_type;
	avbs_t *b;
	priv_avbs_t *pb;
	char livename[64];

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	b = &enc[ch_num].bs[sub_num];
 	pb = &enc[ch_num].priv_bs[sub_num];
	media_type = convert_gmss_media_type(b->video.enc_type);
	pb->video.qno = do_queue_alloc(media_type);

	sprintf(livename, "live/ch%02d_%d", ch_num, sub_num);
    pb->sr = stream_reg(livename, pb->video.qno, pb->video.sdpstr, -1, NULL, 1);
	if (pb->sr < 0) {
		fprintf(stderr, "open_live_streaming: ch_num=%d, sub_num=%d setup error\n", ch_num, sub_num);
	}
	init_video_frame(pb->video.qno);
	strcpy(pb->name, livename);
	return 0;
}

static int write_rtp_frame(int ch_num, int sub_num, void *data)
{
	int ret, media_type;
	avbs_t *b;
	priv_avbs_t *pb;
	unsigned int tick;
	dvr_enc_queue_get *q= (dvr_enc_queue_get *) data;
	gm_ss_entity entity;
	frame_slot_t *fs;

 	pb = &enc[ch_num].priv_bs[sub_num];
	b = &enc[ch_num].bs[sub_num];

	if(pb->play==0 || (b->event != NONE_BS_EVENT)) {
		ret = 1;
		goto exit_free_buf;
	}
	tick = get_tick(&q->bs.timestamp);
	if(pb->video.timed==0) {
		pb->congest = 0;
		pb->video.tick = tick;
		pb->video.timed = 1;
	}
    if(pb->reset_intra_frame == 1) {
        set_bs_intra_frame(ch_num, sub_num);
        pb->reset_intra_frame = 0;
    }

	entity.data =(char *) enc[ch_num].v_bs_mbuf_addr + pb->video.offs + q->bs.offset;
	entity.size = q->bs.length;
	entity.timestamp = tick;
	media_type = convert_gmss_media_type(b->video.enc_type);
	fs = take_video_frame(pb->video.qno);
	fs->vf->search_key =(int) entity.data;
	fs->vf->ch_num = ch_num;
	fs->vf->sub_num = sub_num;
	memcpy(&fs->vf->queue, data, sizeof(dvr_enc_queue_get));
	ret = stream_media_enqueue(media_type, pb->video.qno, &entity);
	if (ret < 0) {
		if (ret == ERR_FULL) {
			pb->congest = 1;
			fprintf(stderr, "enqueue queue ch_num=%d, sub_num=%d full\n", ch_num, sub_num);
		} else if ((ret != ERR_NOTINIT) && (ret != ERR_MUTEX) && (ret != ERR_NOTRUN)) {
			fprintf(stderr, "enqueue queue ch_num=%d, sub_num=%d error %d\n", ch_num, sub_num, ret);
		}
		fprintf(stderr, "enqueue queue ch_num=%d, sub_num=%d error %d\n", ch_num, sub_num, ret);
		goto exit_free_video_buf;
	}
	return 0;
	
exit_free_video_buf: 
	put_video_frame(pb->video.qno, fs);
exit_free_buf: 
	free_bs_data(ch_num, sub_num, q);
	return ret;
}

static int close_live_streaming(int ch_num, int sub_num)
{
	avbs_t *b;
	priv_avbs_t *pb;
	int ret = 0;

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	b = &enc[ch_num].bs[sub_num];
 	pb = &enc[ch_num].priv_bs[sub_num];
	free_video_frame(pb->video.qno);
	if(pb->sr >= 0) {
		ret = stream_dereg(pb->sr, 1);
		if (ret < 0) goto err_exit;
		pb->sr = -1; 
		pb->video.qno = -1;
	}
err_exit: 
	if(ret < 0) 
		fprintf(stderr, "%s: stream_dereg(%d) err %d\n", __func__, pb->sr, ret);
	return ret;
}

static int open_avifile(int ch_num, int sub_num)
{
	priv_avbs_t *pb;
	avbs_t *b;
	char str[32], tmp_filename[64];
	AviMainHeader main_header;
	AviStreamHeader stream_header;
	GmAviStreamFormat stream_format;
	HANDLE	avi_handle;
	int bps, ret=0, avi_type;

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	pb = &enc[ch_num].priv_bs[sub_num];
	b = &enc[ch_num].bs[sub_num];
	get_time_str(str);
	sprintf(pb->name, "CH%d_%d_%s.%s", ch_num, sub_num, str, "avi");
	bps = (b->video.bps <= 0) ? get_bitrate(b->video.width, b->video.height, b->video.fps) : b->video.bps;
    sprintf(tmp_filename, "%s_tmp", pb->name);
    if((avi_handle = GMAVIOpen(tmp_filename, GMAVI_FILEMODE_CREATE, 0))==NULL)  goto err_exit;
    ret = GMAVIFillAviMainHeaderValues(&main_header, 
									b->video.width, 
									b->video.height, 
                					b->video.fps, 
									bps, 
									256);
	if(ret < 0) goto err_exit;

	switch(b->video.enc_type) {
	    case ENC_TYPE_MPEG: 
	        avi_type = GMAVI_TYPE_MPEG4;  
	        break;
	    case ENC_TYPE_MJPEG: 
	        avi_type = GMAVI_TYPE_MJPEG;  
	        break;
	    case ENC_TYPE_H264:
	    default: 
	        avi_type = GMAVI_TYPE_H264;  break;
	}
    ret = GMAVIFillVideoStreamHeaderValues(&stream_header, 
										&stream_format, 
										avi_type, 
										b->video.width, 
										b->video.height, 
										b->video.fps, 
										bps, 
										256);
	if(ret < 0) goto err_exit;
    ret = GMAVISetAviMainHeader(avi_handle, &main_header);
	if(ret < 0) goto err_exit;
    ret = GMAVISetStreamHeader(avi_handle, &stream_header, &stream_format, &pb->avi_str_id);
	if(ret < 0) goto err_exit;
	pb->fd = (void *)avi_handle; 

	return 0;
err_exit:
	fprintf(stderr, "%s: ch_num=%d, sub_num=%d, avi_handle=%p, ret=%d error!\n", __func__, ch_num, sub_num, avi_handle, ret);
	return -1;
}

static int write_avifile(int ch_num, int sub_num, void *data)
{
	priv_avbs_t *pb;
	HANDLE	avi_handle;
    unsigned char *buf;
	dvr_enc_queue_get *q= (dvr_enc_queue_get *) data;
	int len, ret, is_keyframe;


	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	pb = &enc[ch_num].priv_bs[sub_num];
	pb = &enc[ch_num].priv_bs[sub_num];
	buf = enc[ch_num].v_bs_mbuf_addr + pb->video.offs + q->bs.offset;
	len = q->bs.length;
	is_keyframe = q->bs.is_keyframe;
	avi_handle = (HANDLE) pb->fd;
	ret = GMAVISetStreamDataAndIndex(avi_handle, 
										pb->avi_str_id, 
										buf, 
										len, 
										is_keyframe, 
										NULL, 
										0);
	if(ret<0)
		fprintf(stderr, "%s: ch_num=%d, sub_num=%d, ret=%d error!\n", __func__, ch_num, sub_num, ret);
	return ret;
}

static int close_avifile(int ch_num, int sub_num)
{
	priv_avbs_t *pb;
	HANDLE	avi_handle;
	char tmp_str1[64];
	int ret;

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
 	pb = &enc[ch_num].priv_bs[sub_num];
	if(pb->fd != NULL) {
		avi_handle = (HANDLE) pb->fd;
		ret = GMAVIClose(avi_handle);
		if(ret < 0) goto err_exit;
		pb->fd = NULL;
		sprintf(tmp_str1, "%s_tmp", pb->name);
		ret = rename(tmp_str1, pb->name);
		if(!ret)
			printf("Record Ch%d file is %s\n", ch_num, pb->name);
		else
			printf("rename %s failed!\n", tmp_str1);

		sprintf(tmp_str1, "%s_idx", tmp_str1);
		unlink(tmp_str1);
	}
	return 0;
err_exit:
	fprintf(stderr, "%s: ch_num=%d, sub_num=%d, ret=%d error!\n", __func__, ch_num, sub_num, ret);
	return -1;
}

static int open_h264file(int ch_num, int sub_num)
{
	priv_avbs_t *pb;
	char str[32];
	
	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	pb = &enc[ch_num].priv_bs[sub_num];
	get_time_str(str);
	sprintf(pb->name, "CH%d_%d_%s.%s", ch_num, sub_num, str, "h264");
	pb->fd = (void *) fopen(pb->name, "wb+");
	if(pb->fd == NULL) perror("open_h264file");
	return 0;
}

static int write_h264file(int ch_num, int sub_num, void *data)
{
	int len, ret;
    unsigned char *buf;
	priv_avbs_t *pb;
	dvr_enc_queue_get *q= (dvr_enc_queue_get *) data;

	pb = &enc[ch_num].priv_bs[sub_num];
	buf = enc[ch_num].v_bs_mbuf_addr + pb->video.offs + q->bs.offset;
	len = q->bs.length;
	ret = fwrite (buf , 1 , len, pb->fd);
	if(ret<len) perror("write_h264file");

	return ret;
}

static int close_h264file(int ch_num, int sub_num)
{
	priv_avbs_t *pb;
	pb = &enc[ch_num].priv_bs[sub_num];
	fclose(pb->fd); 
	pb->fd=0;
	return 0;
}


int open_bs(int ch_num, int sub_num)
{
	//av_t *e;
	avbs_t *b;
	priv_avbs_t *pb;
	int ret;

	if((ch_num >= DVR_RECORD_CHANNEL_NUM || ch_num < 0) || 
		(sub_num >= DVR_ENC_REPD_BT_NUM || sub_num < 0)) {
		fprintf(stderr, "open_bs: ch_num=%d, sub_num=%d error!\n", ch_num, sub_num);
		return -1;
	}
	pb = &enc[ch_num].priv_bs[sub_num];
	b = &enc[ch_num].bs[sub_num];

    if(sub_num == MAIN_BS_NUM) {
        if((ret = set_main_bs(ch_num))<0) 
            goto err_exit;
    } else {
        if((ret = set_sub_bs(ch_num, sub_num))<0) 
            goto err_exit; 
    }
	if((ret = apply_bs(ch_num, sub_num, ENC_START))<0) goto err_exit;
	switch(b->opt_type) {
		case RTSP_LIVE_STREAMING:
			if(b->video.fps <= 0) 
				fprintf(stderr, "open_bs: ch_num=%d, sub_num=%d, fps=%d error!\n", ch_num, sub_num, b->video.fps);
			pb->video.tinc = 90000/b->video.fps;
			pb->video.buf_usage = 0;
			set_sdpstr(pb->video.sdpstr, b->video.enc_type);
			pb->open = open_live_streaming;
			pb->close = close_live_streaming;
			pb->read = get_bs_data;
			pb->free = NULL;
			pb->write = write_rtp_frame;
			break;
		case FILE_AVI_RECORDING:
			pb->video.buf_usage = 0;
			pb->open = open_avifile;
			pb->close = close_avifile;
			pb->read = get_bs_data;
			pb->free = free_bs_data;
			pb->write = write_avifile;
			break;
		case FILE_H264_RECORDING:
			pb->video.buf_usage = 0;
			pb->open = open_h264file;
			pb->close = close_h264file;
			pb->read = get_bs_data;
			pb->free = free_bs_data;
			pb->write = write_h264file;
			break;
		case OPT_NONE:
		default: 
			break;
	}
err_exit:
	return ret;
}

int close_bs(int ch_num, int sub_num)
{
	av_t *e;
	priv_avbs_t *pb;
	int ret=0, sub, is_close_channel=1;

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	e = &enc[ch_num];
	pb = &e->priv_bs[sub_num];

	if(e->bs[sub_num].video.enabled == DVR_ENC_EBST_ENABLE) {
		pb->video.timeval = 0;
		pb->video.itvl_fps = 0;
		pb->video.itvl_bps = 0;
		pb->video.avg_fps = 0;
		pb->video.avg_bps = 0;
		if((ret = apply_bs(ch_num, sub_num, ENC_STOP))<0) goto err_exit;
		e->bs[sub_num].video.enabled = DVR_ENC_EBST_DISABLE;
		e->bs[sub_num].enabled = DVR_ENC_EBST_DISABLE;
		for(sub=0; sub < DVR_ENC_REPD_BT_NUM; sub++) {
		    if(e->bs[sub].video.enabled == DVR_ENC_EBST_ENABLE) {
		        is_close_channel = 0;
		        break;
		    }
		}
		if(is_close_channel == 1) close_channel(ch_num);
	} else {
		e->bs[sub_num].video.enabled = DVR_ENC_EBST_DISABLE;
		e->bs[sub_num].enabled = DVR_ENC_EBST_DISABLE;
	}
	return ret;

err_exit:
	fprintf(stderr, "close_bs: ch_num=%d, sub_num=%d error!\n", ch_num, sub_num);
	return ret;
}

static int bs_check_event(void)
{
	int ch_num, sub_num, ret=0;
	avbs_t *b;
	
	for(ch_num=0; ch_num<DVR_RECORD_CHANNEL_NUM; ch_num++) {
		for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &enc[ch_num].bs[sub_num];
			if(b->event != NONE_BS_EVENT) {
				ret = 1;
				break;
			}
		}
	}
	return ret;
}

static int bs_check_event_type(int bs_event)
{
	int ch_num, sub_num, ret=0;
	avbs_t *b;
	
	for(ch_num=0; ch_num<DVR_RECORD_CHANNEL_NUM; ch_num++) {
		for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &enc[ch_num].bs[sub_num];
			if(b->event == bs_event) { 
				ret = 1;
				break;
			}
		}
	}
	return ret;
}

void bs_new_event(void)
{
	int ch_num, sub_num;
	avbs_t *b;
	priv_avbs_t *pb;
	static int bs_event=START_BS_EVENT;
	//int sub_num_b, sub_num_e, i, exitflag=0;
	int sub_num_b, sub_num_e, i;

	if(bs_check_event() == 0) {
		bs_event=START_BS_EVENT;
		rtspd_set_event = 0;
		return;
	}
	
	for(ch_num=0; ch_num<DVR_RECORD_CHANNEL_NUM; ch_num++) {
		pthread_mutex_lock(&enc[ch_num].ubs_mutex);
		for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &enc[ch_num].bs[sub_num];
			pb = &enc[ch_num].priv_bs[sub_num];
			if(b->event != bs_event) continue;
			//exitflag = 1;
			switch(b->event){
				case START_BS_EVENT: 
					open_bs(ch_num, sub_num);
					if(pb->open) pb->open(ch_num, sub_num);
					b->event = NONE_BS_EVENT;
					break;
				case UPDATE_BS_EVENT:
					if(pb->video.qno >= 0) {  // for live streaming 
						if(get_num_of_used_video_frame(pb->video.qno)==0) {
							if(pb->close) pb->close(ch_num, sub_num);  /* close RTSP Streaming */
					        if(pb->open) pb->open(ch_num, sub_num); /* re-open RTSP streaming */
							b->event = NONE_BS_EVENT;
						}
					}
					break;
				case STOP_BS_EVENT:
					if(sub_num == MAIN_BS_NUM) {
						sub_num_b = DVR_ENC_REPD_BT_NUM-1;
						sub_num_e = 0;
					} else {
						sub_num_b = sub_num_e = sub_num;
					}
					for(i=sub_num_b; i>=sub_num_e; i--) {
					 	pb = &enc[ch_num].priv_bs[i];
						b = &enc[ch_num].bs[i];
						pb->open = NULL;
						pb->read = NULL;
						pb->free = NULL;
						pb->write = NULL;
						if(pb->video.qno >= 0) {  // for live streaming 
							if(get_num_of_used_video_frame(pb->video.qno)==0) {
								if(pb->close) pb->close(ch_num, i);
								pb->close = NULL;
								close_bs(ch_num, i);
								b->event = NONE_BS_EVENT;
							} else {
							    break;
							}
						} else if(pb->close) {  /* for recording */
							pb->close(ch_num, i);
							pb->close = NULL;
							close_bs(ch_num, i);
							b->event = NONE_BS_EVENT;
						} else {
							b->event = NONE_BS_EVENT;
						}
					}
					break;
			}
		}
		//if(exitflag == 1) break;
		pthread_mutex_unlock(&enc[ch_num].ubs_mutex);
	}
	switch(bs_event) {
		case START_BS_EVENT: 
			if(bs_check_event_type(START_BS_EVENT) == 0) 
				bs_event=STOP_BS_EVENT; 
			break;
		case STOP_BS_EVENT: 
			if(bs_check_event_type(STOP_BS_EVENT) == 0) 
				bs_event=UPDATE_BS_EVENT; 
			break;
		case UPDATE_BS_EVENT: 
			if(bs_check_event_type(UPDATE_BS_EVENT) == 0) 
				bs_event=START_BS_EVENT; 
			break;
	}
}

int env_set_bs_new_event(int ch_num, int sub_num, int event)
{
	avbs_t *b;
	int ret = 0;

	CHECK_CHANNUM_AND_SUBNUM(ch_num, sub_num);
	b = &enc[ch_num].bs[sub_num];
	switch(event){
		case START_BS_EVENT: 
			if(b->opt_type == OPT_NONE) goto err_exit;
			if(b->enabled == DVR_ENC_EBST_ENABLE) {
				fprintf(stderr, "Already enabled: ch_num=%d, sub_num=%d\n", ch_num, sub_num);
				ret = -1;
				goto err_exit;
			}
			break;
		case UPDATE_BS_EVENT:
			break;
		case STOP_BS_EVENT:
		    if(sub_num == 0) { /* for main, and disable all sub */
		        if(is_bs_all_disable()) {
    				fprintf(stderr, "Already disabled. ch_num=%d\n", ch_num);
    				goto err_exit;
		        } else {
		            break;
		        }
		    } else {
    			if(b->enabled != DVR_ENC_EBST_ENABLE) {
    				fprintf(stderr, "Already disabled: ch_num=%d, sub_num=%d\n", ch_num, sub_num);
    				ret = -1;
    				goto err_exit;
    			}
		    }
			break;
		default: 
			fprintf(stderr, "env_set_bs_new_event: ch_num=%d, sub_num=%d, event=%d, error\n", ch_num, sub_num, event);
			ret = -1;
			goto err_exit;
	}
	b->event = event;
	rtspd_set_event = 1;

err_exit:
	return ret;
}

int set_poll_event(void)
{
	int ch_num, sub_num, ret = -1;
	av_t *e;
	avbs_t *b;

	for(ch_num=0; ch_num<DVR_RECORD_CHANNEL_NUM; ch_num++) {
		poll_fds[ch_num].revents = 0;
		poll_fds[ch_num].events = 0;
		poll_fds[ch_num].fd = -1;
		e = &enc[ch_num];
		if(e->enabled != DVR_ENC_EBST_ENABLE) continue;
		poll_fds[ch_num].fd = e->enc_fd;
		for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
			b = &e->bs[sub_num];
			if(b->video.enabled == DVR_ENC_EBST_ENABLE) {
				poll_fds[ch_num].events |= (POLLIN_MAIN_BS << sub_num);
				ret = 0;
			}
		}
	}
	return ret;
}

#define POLL_WAIT_TIME 20000 /* microseconds */

void do_poll_event(void)
{
	int ch_num, sub_num, ret;
	priv_avbs_t *pb;
	dvr_enc_queue_get data;
	static struct timeval prev;
	struct timeval cur, tout;
	static int timeval_init = 0;	
	int diff;

	gettimeofday(&cur, NULL);
	if(timeval_init==0) {
		timeval_init=1;
		tout.tv_sec=0;
		tout.tv_usec=POLL_WAIT_TIME;
	} else {
		diff= (cur.tv_usec < prev.tv_usec) ? (cur.tv_usec+1000000-prev.tv_usec) : (cur.tv_usec-prev.tv_usec);
		tout.tv_usec = (diff > POLL_WAIT_TIME) ? (tout.tv_usec=0) : (POLL_WAIT_TIME-diff);
	}
	usleep(tout.tv_usec);
	gettimeofday(&prev, NULL);
	ret =poll(poll_fds, DVR_RECORD_CHANNEL_NUM, 500);
	if(ret < 0) {
	    perror("poll:");
	    printf("%s:%d <ret=%d>\n",__FUNCTION__,__LINE__, ret);
	    return;
	}
	for(ch_num=0; ch_num<DVR_RECORD_CHANNEL_NUM; ch_num++) {
		if(poll_fds[ch_num].revents == 0) continue;
		for(sub_num=0; sub_num < DVR_ENC_REPD_BT_NUM; sub_num++){
			if(poll_fds[ch_num].revents & (POLLIN_MAIN_BS << sub_num)) {
				pb = &enc[ch_num].priv_bs[sub_num];
				if(pb->read) ret=pb->read(ch_num, sub_num, &data);
				if(ret<0) continue;
				if(pb->write) pb->write(ch_num, sub_num, &data);
				if(pb->free) pb->free(ch_num, sub_num, &data);
			}
		}
	}
}

static int init_resources(void)
{
	struct timeval	tval;
	
	srand((unsigned int) time(NULL));
	gettimeofday(&tval, NULL);
	sys_sec = tval.tv_sec;
	memset(frame_info, 0, sizeof(frame_info));
    return 0;
}

static void	env_release_resources(void)
{
	int ret, ch_num;
	av_t *e;

	if ((ret = stream_server_stop())) 
		fprintf(stderr, "stream_server_stop() error %d\n", ret);

	for(ch_num=0; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
		e = &enc[ch_num];
        pthread_mutex_destroy(&e->ubs_mutex);
    }
}

static int frm_cb(int type, int qno, gm_ss_entity *entity)
{
	frame_slot_t *fs;
	
	if ((GM_SS_VIDEO_MIN <= type) && (type <= GM_SS_VIDEO_MAX)) {
		fs = search_video_frame(qno, (int)entity->data);
		CHECK_CHANNUM_AND_SUBNUM(fs->vf->ch_num, fs->vf->sub_num);
		free_bs_data(fs->vf->ch_num, fs->vf->sub_num, &fs->vf->queue);
		put_video_frame(qno, fs);
	}
	return 0;
}

priv_avbs_t *find_file_sr(char *name, int srno)
{
	int	ch_num, sub_num, hit=0;
	priv_avbs_t	*pb;

	for(ch_num=0; ch_num<DVR_RECORD_CHANNEL_NUM; ch_num++) {
		for(sub_num=0; sub_num<DVR_ENC_REPD_BT_NUM; sub_num++) {
	 		pb = &enc[ch_num].priv_bs[sub_num];
		    if ((pb->sr == srno) && (pb->name) && (strcmp(pb->name, name) == 0)) {
				hit = 1;	
                pb->reset_intra_frame = 1;  
				break;
			}
		}
		if(hit) break;
	}
	return (hit ? pb : NULL); 
}

static int cmd_cb(char *name, int sno, int cmd, void *p)
{
	int	ret = -1;
	priv_avbs_t		*pb;

	switch (cmd) {
		case GM_STREAM_CMD_OPEN:
			printf("%s:%d <GM_STREAM_CMD_OPEN>\n",__FUNCTION__,__LINE__);
			ERR_GOTO(-10, cmd_cb_err);
			break;
		case GM_STREAM_CMD_PLAY:
			if ((pb = find_file_sr(name, sno)) == NULL) goto cmd_cb_err;
			if(pb->video.qno >= 0) pb->play = 1;
			ret = 0;
			break;
		case GM_STREAM_CMD_PAUSE:
			printf("%s:%d <GM_STREAM_CMD_PAUSE>\n",__FUNCTION__,__LINE__);
			ret = 0;
			break;
		case GM_STREAM_CMD_TEARDOWN:
			if ((pb = find_file_sr(name, sno)) == NULL) goto cmd_cb_err;
			pb->play = 0;
			pb->video.timed = 0;
			ret = 0;
			break;
		default:
			fprintf(stderr, "%s: not support cmd %d\n", __func__, cmd);
			ret = -1;
	}

cmd_cb_err:
	if (ret < 0) {
		fprintf(stderr, "%s: cmd %d error %d\n", __func__, cmd, ret);
	}
	return ret;
}

void *enq_thread(void *ptr)
{

	while (1) {
		if(rtspd_sysinit == 0) break;
		if(rtspd_set_event) bs_new_event();
		if(set_poll_event() < 0) {
			sleep(1);
			continue;
		}
		do_poll_event();
	}
	env_release_resources();
	pthread_exit(NULL);
	return NULL;
}

int env_resources(void)
{
	int	ret;
	
	if ((ret = init_resources()) < 0) {
		fprintf(stderr, "init_resources, ret %d\n", ret);
	}
	return ret;
}

int env_server_init(void)
{
	int	ret;
	
	if ((ret = stream_server_init(ipptr, (int) sys_port, 256, SR_MAX, VQ_MAX, VQ_LEN, AQ_MAX, AQ_LEN, frm_cb, cmd_cb)) < 0) {
		fprintf(stderr, "stream_server_init, ret %d\n", ret);
	}
	return ret;
}


int env_file_init(void)
{
	int	ret;
	
	if ((ret = stream_server_file_init(file_path)) < 0) {
		fprintf(stderr, "stream_server_file_init, ret %d\n", ret);
	}
	return ret;
}

int env_server_start(void)
{
	int	ret;
	
	if ((ret = stream_server_start()) < 0) {
		fprintf(stderr, "stream_server_start, ret %d\n", ret);
	}
	return ret;
}

int env_init(void)
{
	int	ret;

	env_enc_init();
	env_cfg_init();
	if ((ret = env_resources()) < 0) return ret;
	if ((ret = env_server_init()) < 0) return ret;		
	if ((ret = env_file_init()) < 0) return ret;
	if ((ret = env_server_start()) < 0) return ret;
	return 0;
}

void show_clients(gm_ss_sr_t *srtable)
{
	gm_ss_sr_t		*st;
	gm_ss_clnt_t	*client;

	while (srtable) {
		st = srtable;
		srtable = srtable->next;

		fprintf(stderr, "SR[%d] %s:\n", st->index, st->name);
		free(st->name);
		while (st->client) {
			client = st->client;
			st->client = client->next;
			fprintf(stderr, "    %s:%d\n",  inet_ntoa(client->addr.sin_addr) , client->addr.sin_port);
			free(client);
		}
		free(st);
	}
}

#if 0
static char get_char(void)
{
	int		i;
	char	cbuf[4];
	fgets(cbuf, 4, stdin);
	for (i = 0; cbuf[i] == ' '; ++i);
	return toupper(cbuf[i]);
}
#endif

int init_isp(void)
{
	char isp_file[] = "dev/isp";

	if((access(isp_file, F_OK)) >= 0) {
		printf("isp start.\n");
	    isp_fd = open(isp_file, O_RDWR);
        if(isp_fd < 0) {
        	printf("Open ISP fail\n");
        	return -1;
        }	
	}
    return 0;
}

void close_isp(void)
{
	printf("close isp.\n");
    close(isp_fd);
    isp_fd = -1;
}

static int rtspd_start(int port)
{
	int				ret;
	pthread_attr_t	attr;

	if(rtspd_sysinit == 1) {
		return -1;
	}

	if ((0 < port) && (port < 0x10000)) sys_port = port;

	if ((ret = env_init()) < 0) return ret;
	rtspd_sysinit = 1;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&enq_thread_id, &attr, &enq_thread, NULL);
	pthread_attr_destroy(&attr);
	return 0;
}

#if 0
static void rtspd_stop(void)
{
	rtspd_sysinit = 0;
}
#endif

static int get_int_from_stdin(int *integer)
{
	char string[16];
	int i;

	if(scanf("%15s", string) < 0 ) goto err_exit;
	for(i=0; i< sizeof(string); i++) 
		if(!isdigit(string[0]))  goto err_exit;
	*integer = atoi(string);
	return 0; 

err_exit:
	return -1;
}

static int init_isp_pw_frequency(void)
{
	int ret = -1, val;
	
    printf("Enter power frequency 50/60: ");
	if((get_int_from_stdin(&val)<0) && (val!=50) && (val!=60)) {
	    printf("Set power frequency %d fail!\n", val);
        goto err_exit;
	}
	
    printf("Set power frequency %d!\n", val);
    ret = ioctl(isp_fd, ISP_IO_SET_AE_FREQ, &val);
    if (ret < 0) {
        printf("Error to set ISP_IO_SET_AE_FREQ\n");
        goto err_exit;
    }
    return 0;
    
err_exit: 
    return ret; 
}

static int update_enc_type_param(int ch_num, int sub_num, int enc_type)
{
	int val, ret;
	avbs_t *b;
    update_avbs_t *ubs;

    /* All of the parameters need to be update !! */	
    b = &enc[ch_num].bs[sub_num];
	ubs = &enc[ch_num].ubs[sub_num];
    ret = cfgud_enc_type(ch_num, sub_num, enc_type);
    if(ret < 0) goto err_exit;
    
	switch (enc_type) {
		case ENC_TYPE_MJPEG: 
            printf("Enter mjpeg quality(%d): ", b->video.mjpeg_quality);
        	if(get_int_from_stdin(&val) >= 0) 
		        cfgud_mjpeg_quality(ch_num, sub_num, val);
		    else 
		        cfgud_mjpeg_quality(ch_num, sub_num, b->video.mjpeg_quality);
			break;
		case ENC_TYPE_H264:
		case ENC_TYPE_MPEG:
            cfgud_rate_ctl_type(ch_num, sub_num, 2);  /* 0:cbr, 1:vbr, 2:ecbr, 3:evbr */
            printf("Enter bitrate(%dk): ", b->video.bps/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_bps(ch_num, sub_num, val*1000);
        	else 
        	    cfgud_bps(ch_num, sub_num, b->video.bps);
    	    cfgud_trm(ch_num, sub_num, b->video.bps*4); /* set target_rate_max = 4*bps */
    	    cfgud_rdm(ch_num, sub_num, 10*1000);  /* 10 seconds */
    	    cfgud_iniQ(ch_num, sub_num, enc_type, 25);
            if(enc_type == ENC_TYPE_H264)
    	        cfgud_maxQ(ch_num, sub_num, enc_type, 51);
    	    else 
    	        cfgud_maxQ(ch_num, sub_num, enc_type, 31);
    	    cfgud_minQ(ch_num, sub_num, enc_type, 1);
    	    break;
        default:
            goto err_exit;
	}

	pthread_mutex_lock(&enc[ch_num].ubs_mutex);
    env_set_bs_new_event(ch_num, sub_num, UPDATE_BS_EVENT);	
	pthread_mutex_unlock(&enc[ch_num].ubs_mutex);
	return 0; 

err_exit:
    return -1;
}

static void update_enc_rate_ctl_type_param(int ch_num, int sub_num)
{
	int val;
	av_t *e;
	avbs_t *b;
	
    b = &enc[ch_num].bs[sub_num];
	e = &enc[ch_num];
    printf("Enter rate control type(%d:%s), 0:CBR, 1:VBR, 2:ECBR, 3:EVBR.: ", 
                                b->video.rate_ctl_type, 
                                rcCtlTypeString[b->video.rate_ctl_type]);
	if(get_int_from_stdin(&val) >= 0) {
    	switch(val) {
	    case RATE_CTL_CBR:
            cfgud_rate_ctl_type(ch_num, sub_num, val);
            printf("Enter bitrate(%dk): ", b->video.bps/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_bps(ch_num, sub_num, val*1000);

            printf("Enter init quant(%d): ", b->video.init_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_iniQ(ch_num, sub_num, b->video.enc_type, val);

            printf("Enter max quant(%d): ", b->video.max_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_maxQ(ch_num, sub_num, b->video.enc_type, val);
            
            printf("Enter min quant(%d): ", b->video.min_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_minQ(ch_num, sub_num, b->video.enc_type, val);
    	    cfgud_trm(ch_num, sub_num, 0);
    	    cfgud_rdm(ch_num, sub_num, 0);
            break;
        case RATE_CTL_VBR:
            cfgud_rate_ctl_type(ch_num, sub_num, val);
            printf("Enter init quant(%d): ", b->video.init_quant);
        	if(get_int_from_stdin(&val) >= 0) {
        	    cfgud_iniQ(ch_num, sub_num, b->video.enc_type, val);
        	    cfgud_maxQ(ch_num, sub_num, b->video.enc_type, val);
        	    cfgud_minQ(ch_num, sub_num, b->video.enc_type, val);
        	}
    	    cfgud_trm(ch_num, sub_num, 0);
    	    cfgud_rdm(ch_num, sub_num, 0);
            break;
        case RATE_CTL_ECBR:
            cfgud_rate_ctl_type(ch_num, sub_num, val);
            printf("Enter bitrate(%dk): ", b->video.bps/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_bps(ch_num, sub_num, val*1000);

            printf("Enter target bitrate max(%dk): ", b->video.target_rate_max/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_trm(ch_num, sub_num, val*1000);

            printf("Enter reaction delay time max(%ds): ", b->video.reaction_delay_max/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_rdm(ch_num, sub_num, val*1000);

            printf("Enter init quant(%d): ", b->video.init_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_iniQ(ch_num, sub_num, b->video.enc_type, val);

            printf("Enter max quant(%d): ", b->video.max_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_maxQ(ch_num, sub_num, b->video.enc_type, val);
            
            printf("Enter min quant(%d): ", b->video.min_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_minQ(ch_num, sub_num, b->video.enc_type, val);

            break;
        case RATE_CTL_EVBR:
            cfgud_rate_ctl_type(ch_num, sub_num, val);
            printf("Enter target bitrate max(%dk): ", b->video.target_rate_max/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_trm(ch_num, sub_num, val*1000);

            printf("Enter init quant(%d): ", b->video.init_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_iniQ(ch_num, sub_num, b->video.enc_type, val);

            printf("Enter max quant(%d): ", b->video.max_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_maxQ(ch_num, sub_num, b->video.enc_type, val);
            
            printf("Enter min quant(%d): ", b->video.min_quant);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_minQ(ch_num, sub_num, b->video.enc_type, val);

    	    cfgud_bps(ch_num, sub_num, 0);
    	    cfgud_rdm(ch_num, sub_num, 0);
            break;
        default:
            break;
        }
    }
}

static void update_enc_rate_ctl_param(int ch_num, int sub_num)
{
	int val;
	av_t *e;
	avbs_t *b;
    b = &enc[ch_num].bs[sub_num];
	e = &enc[ch_num];
	switch (e->bs[sub_num].video.enc_type) {
		case ENC_TYPE_MJPEG: 
            printf("Enter mjpeg quality(%d): ", b->video.mjpeg_quality);
        	if(get_int_from_stdin(&val) >= 0) 
		        cfgud_mjpeg_quality(ch_num, sub_num, val);
			break;
		case ENC_TYPE_H264:
		case ENC_TYPE_MPEG:
            printf("Enter bitrate(%dk): ", b->video.bps/1000);
        	if(get_int_from_stdin(&val) >= 0) 
        	    cfgud_bps(ch_num, sub_num, val*1000);
        	else 
                update_enc_rate_ctl_type_param(ch_num, sub_num);
        default:
            break;
	}
}

static void update_enc_param(void)
{
	int val, width, height, ch_num, sub_num, ret;
	avbs_t *b;
	
    printf("Enter channel numble: ");
	if(get_int_from_stdin(&val) >= 0) 
	    ch_num = val;
	else 
	    goto exit_update;

    printf("Enter sub-channel numble: ");
	if(get_int_from_stdin(&val) >= 0) 
	    sub_num = val;
	else 
	    goto exit_update;

    cfgud_init(ch_num, sub_num);
    b = &enc[ch_num].bs[sub_num];
    
    printf("Enter 0(OFF), 1(ON) : ");
	if(get_int_from_stdin(&val) >= 0) {
	    cfgud_stream_enable(ch_num, sub_num, val);
	    goto run_update;
	}
    printf("Enter codec type(%d:%s), 0:H264, 1:MPEG4, 2:MJPEG.: ", b->video.enc_type, enc_type_def_str[b->video.enc_type]);
	if(get_int_from_stdin(&val) >= 0) {
        ret = update_enc_type_param(ch_num, sub_num, val); 
        if (ret >= 0) goto run_update;
        if (ret < 0) goto exit_update;
	}
	
	update_enc_rate_ctl_param(ch_num, sub_num);
	
    printf("Enter width(%d) ,must multiple of 16: ", b->video.width);
	if((get_int_from_stdin(&val)>=0) && (val%16==0)) {
		width = val;
	    printf("Enter height(%d), must multiple of 16: ", b->video.height);
		if((get_int_from_stdin(&val)>=0) && (val%16==0)) {
			height = val;
			ret = cfgud_res(ch_num, sub_num, width, height); 
			if(ret<0) 
			    goto exit_update;
		}
	}

//    printf("Enter gop (%d): ", b->video.ip_interval);
//	if(get_int_from_stdin(&val) >= 0) 
//	    cfgud_ip_interval(ch_num, sub_num, val);

    printf("Enter frame rate(%dfps): ", b->video.fps);
	if(get_int_from_stdin(&val) >= 0) 
	    cfgud_fps(ch_num, sub_num, val);
#if 0
#ifdef NEW_ROI_FUNC
        printf("Enter enabled_roi(%d), 0(OFF), 1(ON): ", b->video.enabled_roi);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_enabled_roi(ch_num, sub_num, val);
        
        printf("Enter ROI_x(%d): ", b->video.roi_x);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_roi_x(ch_num, sub_num, val);
    
        printf("Enter ROI_y(%d): ", b->video.roi_y);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_roi_y(ch_num, sub_num, val);
    
        printf("Enter ROI_w(%d): ", b->video.roi_w);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_roi_w(ch_num, sub_num, val);
    
        printf("Enter ROI_h(%d): ", b->video.roi_h);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_roi_h(ch_num, sub_num, val);
#else
    if(b->video.enabled_roi == 1) {
        printf("Enter ROI_x(%d): ", b->video.roi_x);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_roi_x(ch_num, sub_num, val);
    
        printf("Enter ROI_y(%d): ", b->video.roi_y);
    	if(get_int_from_stdin(&val) >= 0) 
    	    cfgud_roi_y(ch_num, sub_num, val);
    }
#endif
#endif

run_update:
    if(cfgud_chk_parm(ch_num, sub_num) >= 0) 
        update_bs(ch_num, sub_num);
exit_update: 
    return;
}

static void show_rtsp_test_menu(void)
{
    printf("----------------<RTSP>-------------------------------------------------\n");
    printf(" Note: If users want to use PC's Player to connect to a RTSP live\n");
    printf("    streaming, the following are examples:\n");
    printf("    channel=0, rtsp://IP/live/ch00_0\n");
    printf("    channel=1, rtsp://IP/live/ch01_0\n");
    printf("    channel=2, rtsp://IP/live/ch02_0\n");
    printf("-----------------------------------------------------------------------\n");
	printf(" Setup:\n");
	printf(" f: Set power frequency 60/50 for ISP.\n");
	printf(" u: Update configuration.\n");
    printf("-----------------------------------------------------------------------\n");
	printf(" 0: Stop\n");
	printf(" 1: Start\n");
    printf(" s: Show configuration\n");
    printf(" Q: Exit\n");
    printf("----------------------------------------\n");
}

void rtsp_test(void)
{
    int  ch_num, stream, i;
    char key;
	
	rtspd_start(554);
	show_rtsp_test_menu();
	while(1)
	{
		key = getch();
		if (key == 'q' || key == 'Q') 
			break;
        switch(key) {
			case 'f':  /* ch2 */
			    init_isp_pw_frequency();
				show_rtsp_test_menu();
				break;
			case '0':  /* 0: Stop */
				for(ch_num=0; ch_num < DVR_RECORD_CHANNEL_NUM; ch_num++) {
                	pthread_mutex_lock(&enc[ch_num].ubs_mutex);
					env_set_bs_new_event(ch_num, 0, STOP_BS_EVENT);  /* disable main + sub */
            		pthread_mutex_unlock(&enc[ch_num].ubs_mutex);

				}
				for(i=0; i<10; i++) {
				    sleep(1);
				    if(is_bs_all_disable()) break;
				}
				break;
			case '1': /* 1: Start */
				for(ch_num=0; ch_num < t_ch_num; ch_num++) {
                	pthread_mutex_lock(&enc[ch_num].ubs_mutex);
                    for(stream=0; stream<DVR_ENC_REPD_BT_NUM; stream++) {
    					env_set_bs_new_event(ch_num, stream, START_BS_EVENT);
                    }
            		pthread_mutex_unlock(&enc[ch_num].ubs_mutex);
				}
				break;
			case '@':
				enable_print_average = ! enable_print_average;
				break;
			case '<':
				system("cat /proc/videograph/grab");
				break;
			case '/':
				system("cat /proc/videograph/job");
				break;
			case '>':
				dbg_rtsp();
				break;
			case '[':
				system("top -n 1 | grep 'CPU:' | grep -v 'grep'");
				break;
			case ']':
				system("ps");
				break;
			case '|':
				{
					int id, inout;
					char cmd[100];
					
					printf("\nid: ");
					scanf("%d", &id);
					printf("1/2(IN/OUT): ");
					scanf("%d", &inout);
					memset(cmd,0,sizeof(cmd));
					sprintf(cmd,"echo %d %d > /proc/videograph/grab", id, inout);
					printf("\n%s\n",cmd);
					system(cmd);
				}
				break;
			case 'p': show_rtsp_test_menu(); break; 
			case 's': show_enc_cfg(); break; 
			case 'u': update_enc_param(); break; 
			case 'c': env_cfg_init(); show_enc_cfg(); break; 
			default: continue;
		}
	}
}


