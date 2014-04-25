#ifndef __GM_TYPES_H__
#define __GM_TYPES_H__

/* Common definition */
typedef void* HANDLE;
typedef unsigned char* FM_PTR;
#define GM_TIME     int

/* AVI flag, defined by aviriff.h */
#define AVIF_HASINDEX        0x00000010 // Index at end of file?
#define AVIF_MUSTUSEINDEX    0x00000020
#define AVIF_ISINTERLEAVED   0x00000100
#define AVIF_TRUSTCKTYPE     0x00000800 // Use CKType to find key frames
#define AVIF_WASCAPTUREFILE  0x00010000
#define AVIF_COPYRIGHTED     0x00020000

#define GM_FRAME_TYPE_KEYFRAME  0x1
#define GM_FRAME_TYPE_NO_TIME   0x2


/* FOURCC */
#define GM_MAKE_FOURCC(a,b,c,d)         (int)((a)|(b)<<8|(c)<<16|(d)<<24)
#define GM_GET_NUM_FROM_FOURCC(x)       (int)((x&0xFF000000)>>24|(x&0x00FF0000)>>16)
//video
#define GMAVI_TYPE_H264     GM_MAKE_FOURCC('H','2','6','4')
#define GMAVI_TYPE_MPEG4    GM_MAKE_FOURCC('D','I','V','X')
#define GMAVI_TYPE_MJPEG    GM_MAKE_FOURCC('M','J','P','G')
#define GMAVI_TYPE_GMTAG    GM_MAKE_FOURCC('G','M','T','G')
//audio
#define GMAVI_TYPE_PCM      GM_MAKE_FOURCC('P','C','M',' ')
#define GMAVI_TYPE_MP3      GM_MAKE_FOURCC('M','P','E','G')


typedef struct GMSearchIndexTag {
    int offset;     //movi offset
    int index;      //idx1 offset
}GMSearchIndex;

typedef struct GMFMChannelProfileTag {
    //video
    int video_type;
    int width;
    int height;
    int framerate;
    union {
        int cbr_bitrate;
        int vbr_level;
    }quality;
    int ip_interval;
    int has_audio;

    //audio
    int au_type;
    int au_channel_num;    //mono or stereo
    int au_sample_rate;
    int au_bitrate;

}GMFMChannelProfile;


typedef struct tagGMEventData {
    int     type;
}GMEventData;

#define GMTAG_TYPE_CH_START     0
#define GMTAG_TYPE_CH_STOP      1
#define GMTAG_TYPE_CH_CONTINUE  2
#define GMTAG_TYPE_EVENT        3
typedef struct tagGMStreamTag {
    int             type;
    GM_TIME         timestamp;
    union {
        GMFMChannelProfile  ch_prof;
        GMEventData     gm_event;
    }data;
}GMStreamTag;


/* this structure is appending following each packet (tag as JUNK) */
typedef struct tagGMExtraInfo {
    GM_TIME   timestamp;
    int       profile_index;
    int       flag;
}GMExtraInfo;


/* Status Code */
typedef enum GmAviStatusTag {
    GMSTS_OK                =  0,
    GMSTS_VARIANT_FORMAT    =  1,
    GMSTS_END_OF_DATA       =  2,
    GMSTS_READ_MESSAGE_DATA =  3,

    //error
    GMSTS_OPEN_FAILED       = -1,
    GMSTS_INVALID_INPUT     = -2,
    GMSTS_WRONG_STATE       = -3,
    GMSTS_INVALID_FORAMT    = -4,
    GMSTS_DATA_FULL         = -5,
    GMSTS_NOT_ENOUGH_SPACE  = -6,
    GMSTS_INTERNAL_ERROR    = -7,
    GMSTS_SEEK_FAILED       = -8,
    GMSTS_RECORD_NOT_FOUND  = -9,
    GMSTS_WRITE_FAILED      = -10,
    GMSTS_READ_FAILED       = -11,
    GMSTS_FILE_SYSTEM_INEXISTENT  = -100,
    GMSTS_FILE_SYSTEM_BROKEN      = -101

}GmAviStatus;


#endif /* __GM_TYPES_H__ */

