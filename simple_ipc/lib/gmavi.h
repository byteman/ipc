#ifndef __GM_AVI_H__
#define __GM_AVI_H__

#ifdef WIN32
#include "Windows.h"
#else
#include <pthread.h>
#endif



#ifdef WIN32
#define CRITICAL_SECTION_OBJECT_DEFINE(x)   CRITICAL_SECTION x
#define CRITICAL_SECTION_INITIAL(x)         InitializeCriticalSection(&x);
#define CRITICAL_SECTION_DELETE(x)          DeleteCriticalSection(&x);
#define CRITICAL_SECTION_LOCK(x)            EnterCriticalSection(&x); 
#define CRITICAL_SECTION_UNLOCK(x)          LeaveCriticalSection(&x);
#else
#define CRITICAL_SECTION_OBJECT_DEFINE(x)   pthread_mutex_t x;
#define CRITICAL_SECTION_INITIAL(x)         pthread_mutex_init(&x, NULL);
#define CRITICAL_SECTION_DELETE(x)          pthread_mutex_destroy(&x);
#define CRITICAL_SECTION_LOCK(x)            pthread_mutex_lock(&x); 
#define CRITICAL_SECTION_UNLOCK(x)          pthread_mutex_unlock(&x);
#endif


#define GMAVI_IS_CREATE(x)          (x&0x1)
#define GMAVI_IS_READ(x)            (x&0x2)
#define GMAVI_IS_WRITE(x)           (x&0x4)
#define GMAVI_IS_FIXED_SIZE(x)      (x&0x8)

#define GMAVI_IS_AVI_HEADER_SECTION(x)      (x&0x1)
#define GMAVI_IS_STREAM_HEADER_SECTION(x)   (x&0x2)
#define GMAVI_IS_STREAM_DATA_SECTION(x)     (x&0x4)
#define GMAVI_IS_INDEX_SECTION(x)           (x&0x8)


/*  AVI structures (GM implement)
  Another general layout, please refer to [MS platform SDK\Include\aviriff.h]
-------------------------------------------------------------------------------
RIFF - (size) - AVI      => size: avi_len,     position: AviLength_riff_avi
 
LIST - (size) - hdrl     => size: hdrl_len,    position: AviLength_list_hdrl
                         <= MainHeaderStart
  avih - (size)      
    ooooooo
                         <= StreamHeaderStart
  LIST - (size) - strl
   strh - (size)
     ooooooo
   strf - (size)
     ooooooo
                         <= strh_pos (from StreamHeaderStart)
  JUNK - (size)
     xxxxxxx
     xxxxxxx

LIST - (size) - movi     => size: movi_len,    position: AviLength_list_movi
                         <= StreamDataStart
     ooooooo
     ooooooo
                         <= data_pos (from StreamDataStart)
  JUNK - (size)
     xxxxxxx
     xxxxxxx

idx1 - (size)            => size: idx1_len,    position: AviLength_idx1
                         <= IndexStart
     ooooooo
     ooooooo
                         <= index_pos (from IndexStart)
  JUNK - (size)
     xxxxxxx
     xxxxxxx
-------------------------------------------------------------------------------
*/


typedef struct GmAviFileChunkEntryTag{
    //length entry
    int     AviLength_riff_avi;   //RIFF (length) AVI
    int     AviLength_list_hdrl;  //LIST (length) hdrl
    int     AviLength_list_movi;  //LIST (length) movi
    int     AviLength_idx1;       //idx1 (length)

    //block start/end
    int     MainHeaderStart;
    int     StreamHeaderStart;
    int     StreamDataStart;
    int     IndexStart;
}GmAviFileChunkEntry;


typedef struct GmAviFileTag {
    FILE    *file;
    FILE    *idxfile;
    int     fileno; // descriptor of this file
    int     mode;
    int     file_size;
    int     stream_count;
    int     frame_count;
    
    GmAviFileChunkEntry entry;
    int     strh_pos;   //current stream header offset from StreamHeaderStart
    int     data_pos;   //current stream data offset from StreamDataStart
    int     index_pos;  //current index offset from IndexStart
    int     avi_len;
    int     hdrl_len;
    int     movi_len;
    int     idx1_len;

    CRITICAL_SECTION_OBJECT_DEFINE(CriticalSection);

}GmAviFile;





#endif /* __GM_AVI_H__ */

