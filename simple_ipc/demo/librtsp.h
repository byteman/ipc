/*
 * Copyright (C) 2009 Faraday Technology Corporation.
 * Author:	Carl Lin.  <nhlin@faraday-tech.com>
 *
 * Version	Date		Description
 * ----------------------------------------------
 * 0.1.0	2009-12-30 	First release.
 *
 */

#ifndef __LIBRTSP_H

#define	__LIBRTSP_H

#define GM_SS_VIDEO_MIN		1
#define	GM_SS_TYPE_H264		1
#define	GM_SS_TYPE_MP4		2
#define	GM_SS_TYPE_MJPEG	3
#define GM_SS_VIDEO_MAX		3

#define GM_SS_AUDIO_MIN		10
#define GM_SS_TYPE_MP2		10
#define GM_SS_TYPE_AMR		11
#define GM_SS_AUDIO_MAX		11

#define GM_STREAM_CMD_OPEN		0	// Setup the steam resource.
#define GM_STREAM_CMD_PLAY		1	// Play.
#define GM_STREAM_CMD_PAUSE		2	// Stop feeding media frames.
#define GM_STREAM_CMD_TEARDOWN	3	// Stop playing and release the stream resource.

#define GM_SS_QUERY_SR			0
#define GM_SS_QUERY_LIVE		1
#define GM_SS_QUERY_FILE		2
#define GM_SS_QUERY_ALL			3

#define SERVER_READY			0
#define SERVER_BUSY				1
#define SERVER_NOT_RUN			2

#define STREAM_NOT_STREAMING	0
#define STREAM_STREAMING		1

#define ERR_GENERAL				(-1)
#define ERR_PARAM				(-2)
#define ERR_MUTEX				(-3)
#define ERR_MEM					(-4)
#define ERR_RESOURCE			(-5)
#define ERR_NOTINIT				(-6)
#define ERR_NOTRUN				(-7)
#define ERR_BUSY				(-8)
#define ERR_NOAVAIL				(-9)
#define ERR_FULL				(-10)
#define ERR_RUNNING				(-11)
#define ERR_IPC					(-12)
#define ERR_THREAD				(-13)
#define ERR_OTHER				(-14)

typedef struct st_gm_ss_entity {
	unsigned int	timestamp;	// Global across all entities, in 90 KHz.
	int				size;		// Actual size.
	char			*data;		// Point to actual data buffer.
} gm_ss_entity;

typedef struct st_gm_clnt {
	struct st_gm_clnt	*next;
	struct sockaddr_in	addr;
} gm_ss_clnt_t;

typedef struct st_gm_sr {
	struct st_gm_sr	*next;
	int				index;
	char			*name;
	gm_ss_clnt_t	*client;
} gm_ss_sr_t;

/* Startup & Shutdown */
int stream_server_init(char *ipif, int port, int cons, int streams, 
                       int vqno,int vqlen, int aqno, int aqlen,
                       int (*frm_cb)(int type, int qno, gm_ss_entity *entity),
                       int (*cmd_cb)(char *name, int sno, int cmd, void *p));
int stream_server_file_init(char *path);
int stream_server_start(void);
int stream_server_stop(void);
int stream_server_reset(void);
int stream_server_reset_force(void);
int stream_server_status(void);

/* Queue Manipulation */
int stream_queue_alloc(int type);
int stream_queue_release(int qno);

/* Stream Registration & Deregistration */
int stream_reg(char *name, int vqno, char *vsdpstr, int aqno, char *asdpstr, int live);
int stream_dereg(int sno, int free_queue);
int stream_reset(int free_queue);
int stream_status(int sno);

/* Media Manipulation */
int stream_media_enqueue(int type, int qno, gm_ss_entity *entity);
int stream_media_flush(int type, int qno);

/* Query */
int stream_query_clients(int type, int sno, gm_ss_sr_t **client_tree);
char *stream_query_localaddr(void);
char *stream_query_version(void);
#endif

