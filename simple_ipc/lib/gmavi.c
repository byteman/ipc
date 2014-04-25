
#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
  #include <unistd.h>
  #include <string.h>
#endif

#include "gmavi_api.h"
#include "gmavi.h"

static void write_char(FILE *file, char c)
{
    fwrite(&c, sizeof(char), 1, file);
}
static void write_int32(FILE *file, int i)
{
    fwrite(&i, sizeof(int), 1, file);
}
static void write_fourcc(FILE *file, int fourcc)
{
    fwrite(&fourcc, sizeof(char)*4, 1, file);
}
static void avi_write_data(FILE *file, char *data, int size)
{
    fwrite(data, size, 1, file);
}
static void avi_put_chunk(FILE *file, int fourcc, int size)
{
    write_fourcc(file, fourcc); 
    write_int32(file, size);
}
static void avi_put_header(FILE *file, int filetype, int size)
{
    avi_put_chunk(file, GM_MAKE_FOURCC('R','I','F','F'), size);
    write_fourcc(file, filetype);
}
static void avi_put_list(FILE *file, int listtype, int size)
{
    avi_put_chunk(file, GM_MAKE_FOURCC('L','I','S','T'), size);
    write_fourcc(file, listtype);
}
static void avi_write_avi_main_header(FILE *file, AviMainHeader *avi_main_header)
{
    fwrite(avi_main_header, sizeof(AviMainHeader), 1, file);
}
static void avi_write_stream_header(FILE *file, AviStreamHeader *avi_stream_header)
{
    fwrite(avi_stream_header, sizeof(AviStreamHeader), 1, file);
}
static void avi_write_stream_format(FILE *file, GmAviStreamFormat *avi_stream_format)
{
    fwrite(avi_stream_format, sizeof(GmAviStreamFormat), 1, file);
}
static void avi_write_index(FILE *file, AviIndex *index)
{
    fwrite(index, sizeof(AviIndex), 1, file);
}

static void read_char(FILE *file, char c)
{
}
static void read_int32(FILE *file, int *i)
{
    fread(i, sizeof(int), 1, file);
}
static void read_fourcc(FILE *file, int *fourcc)
{
    fread(fourcc, sizeof(char)*4, 1, file);
}
static void avi_read_data(FILE *file, char *data, int size)
{
    fread(data, size, 1, file);
}
static void avi_get_chunk(FILE *file, int *fourcc, int *size)
{
    read_fourcc(file, fourcc); 
    read_int32(file, size);
}
static int avi_get_header(FILE *file, int *filetype, int *size)
{
    int riff_tag;
    avi_get_chunk(file, &riff_tag, size);
    if(riff_tag!=GM_MAKE_FOURCC('R','I','F','F'))
        return GMSTS_INVALID_FORAMT;
    read_fourcc(file, filetype);
    return GMSTS_OK;
}
static int avi_get_list(FILE *file, int *listtype, int *size)
{
    int list_tag;
    avi_get_chunk(file, &list_tag, size);
    if(list_tag!=GM_MAKE_FOURCC('L','I','S','T'))
        return GMSTS_INVALID_FORAMT;
    read_fourcc(file, listtype);
    return GMSTS_OK;
}
static void avi_read_avi_main_header(FILE *file, AviMainHeader *avi_main_header)
{
    fread(avi_main_header, sizeof(AviMainHeader), 1, file);
}
static void avi_read_stream_header(FILE *file, AviStreamHeader *avi_stream_header)
{
    fread(avi_stream_header, sizeof(AviStreamHeader), 1, file);
}
static void avi_read_stream_format(FILE *file, GmAviStreamFormat *avi_stream_format)
{
    fread(avi_stream_format, sizeof(GmAviStreamFormat), 1, file);
}
static void avi_read_index(FILE *file, AviIndex *index)
{
    fread(index, sizeof(AviIndex), 1, file);
}


static int avi_skip_junk(FILE *file)
{
    int junk_tag, junk_size;
    avi_get_chunk(file, &junk_tag, &junk_size);
    if(junk_tag==GM_MAKE_FOURCC('J','U','N','K'))
    {
        junk_size += junk_size%2;   /* WORD alignment */
        fseek(file, junk_size, SEEK_CUR);
        return (junk_size+8);
    }
    else
    {
        fseek(file, -8, SEEK_CUR);
        return 0;
    }
}



//===========================================================================//
HANDLE GMAVIOpen(char *filename, int mode ,int size)
{
    FILE  *pFile=NULL, *pIdxFile=NULL;
    int   is_existed=FALSE, file_no=0;
    char  file_mode[4];
    char  tmp_str[128];
    GmAviFile *avi_file;

    // examine file existence
#ifdef WIN32
    if(pFile=fopen(filename,"r"))
    {
        fclose(pFile);
        is_existed = TRUE;
    }
#else /* WIN32 */
    if(access(filename, F_OK)==0)
        is_existed = TRUE;
#endif /* WIN32 */
    if(GMAVI_IS_CREATE(mode)&&!is_existed)
        strcpy(file_mode, "wb+");
    else if(GMAVI_IS_READ(mode) || GMAVI_IS_WRITE(mode))
        strcpy(file_mode, "rb+");
    else
        //goto GMSTS_OPEN_FAILED;
        strcpy(file_mode, "wb+");

    pFile = fopen (filename, file_mode);
    if(pFile)
    {

        if(GMAVI_IS_CREATE(mode))
        {
            if(GMAVI_IS_FIXED_SIZE(mode))
            {
                // enlarge this file
              #ifdef WIN32
                int i;
                for(i=0; i<size; i++)
                    fputc(0, pFile);
              #else /* WIN32 */
                file_no = fileno(pFile);
                if(ftruncate(file_no,size)==-1)
                {
                    perror("truncate");
                    goto GMSTS_OPEN_FAILED;
                }
              #endif /* WIN32 */
                fseek(pFile, 0, SEEK_SET);
            }
            else
            {
                strcpy(file_mode, "wb+");
                sprintf(tmp_str, "%s_idx", filename);
                pIdxFile = fopen (tmp_str, file_mode);
                if(!pIdxFile)
                {
                    perror("Open1");
                    goto GMSTS_OPEN_FAILED;
                }
            }
        }
    }
    else
    {
        perror("Open2");
        goto GMSTS_OPEN_FAILED;
    }

    avi_file = (GmAviFile*)malloc(sizeof(GmAviFile));
    memset(avi_file, 0x0, sizeof(GmAviFile));
    if(!avi_file)
    {
        printf("AVI malloc fail!\n");fflush(stdout);
        goto GMSTS_OPEN_FAILED;
    }

    // setup avi structure
    avi_file->file = pFile;
    avi_file->mode = mode;
    avi_file->file_size = size;
#ifndef WIN32
    avi_file->fileno = fileno(pFile);
#endif

    avi_file->stream_count = 0;
    avi_file->entry.StreamDataStart = 0;
    avi_file->entry.IndexStart = 0;
    avi_file->frame_count = 0;
    avi_file->data_pos = 0;
    avi_file->index_pos = 0;
    avi_file->strh_pos = 0;

    if(GMAVI_IS_CREATE(mode))
    {
        // set default headers
        avi_put_header(avi_file->file, GM_MAKE_FOURCC('A','V','I',' '), 0);
        avi_file->entry.AviLength_riff_avi = ftell(avi_file->file)-8;   //should be 4
        avi_put_list(avi_file->file, GM_MAKE_FOURCC('h','d','r','l'), 0);
        avi_file->entry.AviLength_list_hdrl = ftell(avi_file->file)-8;  //should be 16
        avi_file->entry.MainHeaderStart = ftell(avi_file->file);
        avi_file->entry.StreamHeaderStart = avi_file->entry.MainHeaderStart+sizeof(AviMainHeader);
        avi_file->entry.StreamDataStart = avi_file->entry.StreamHeaderStart + 12;
        if(GMAVI_IS_FIXED_SIZE(mode))
        {
            avi_file->idxfile = avi_file->file;
        }
        else
        {
            avi_file->idxfile = pIdxFile;
            //fseek(avi_file->idxfile, 0, SEEK_SET);
        }
    }
    else 
    {
        int ret, junk_size1=0, junk_size2=0;
        int avi_tag, hdrl_tag, movi_tag, idx1_tag;
        ret = avi_get_header(avi_file->file, &avi_tag, &avi_file->avi_len);
        if(ret<0 || avi_tag!=GM_MAKE_FOURCC('A','V','I',' '))
        {
            printf("AVI tag fail!\n");fflush(stdout);
            goto GMSTS_OPEN_FAILED;
        }
        avi_file->entry.AviLength_riff_avi = ftell(avi_file->file)-8;   //should be 4
        //hdrl
        ret = avi_get_list(avi_file->file, &hdrl_tag, &avi_file->hdrl_len);
        if(ret<0 || hdrl_tag!=GM_MAKE_FOURCC('h','d','r','l'))
        {
            printf("hdrl tag fail!\n");fflush(stdout);
            goto GMSTS_OPEN_FAILED;
        }
        avi_file->entry.AviLength_list_hdrl = ftell(avi_file->file)-8;  //should be 16
        fseek(avi_file->file, avi_file->hdrl_len-4, SEEK_CUR);
        junk_size1 = avi_skip_junk(avi_file->file);
        //movi
        ret = avi_get_list(avi_file->file, &movi_tag, &avi_file->movi_len);
        if(ret<0 || movi_tag!=GM_MAKE_FOURCC('m','o','v','i'))
        {
            printf("movi tag fail!\n");fflush(stdout);
            goto GMSTS_OPEN_FAILED;
        }
        fseek(avi_file->file, avi_file->movi_len-4, SEEK_CUR);
        junk_size2 = avi_skip_junk(avi_file->file);
        //idx1
        avi_get_chunk(avi_file->file, &idx1_tag, &avi_file->idx1_len);
        if(idx1_tag!=GM_MAKE_FOURCC('i','d','x','1'))
        {
            printf("idxl tag fail!\n");fflush(stdout);
            goto GMSTS_OPEN_FAILED;
        }

        avi_file->entry.MainHeaderStart = 24;
        avi_file->entry.StreamHeaderStart = avi_file->entry.MainHeaderStart+sizeof(AviMainHeader);
        avi_file->entry.StreamDataStart = (avi_file->entry.MainHeaderStart-4)+avi_file->hdrl_len+junk_size1+12;
        avi_file->entry.IndexStart = (avi_file->entry.StreamDataStart-4)+avi_file->movi_len+junk_size2+8;
        avi_file->entry.AviLength_list_movi = avi_file->entry.StreamDataStart-8;
        avi_file->entry.AviLength_idx1 = avi_file->entry.IndexStart-4;
        avi_file->idxfile = avi_file->file;

        if(GMAVI_IS_WRITE(avi_file->mode))
        {
            avi_file->strh_pos = avi_file->hdrl_len-sizeof(AviMainHeader)-4;
            avi_file->data_pos = avi_file->movi_len-4;
            avi_file->index_pos = avi_file->idx1_len;
        }
    }

	CRITICAL_SECTION_INITIAL(avi_file->CriticalSection);
    

    return (HANDLE)avi_file;

GMSTS_OPEN_FAILED:

    return NULL;
}

int GMAVIClose(HANDLE handle)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int count;
    AviIndex index;
    int i;

    if(!avi_file)
        return GMSTS_INVALID_INPUT;

    // update avi structure
    if(GMAVI_IS_CREATE(avi_file->mode) || GMAVI_IS_WRITE(avi_file->mode))
    {
        if(GMAVI_IS_FIXED_SIZE(avi_file->mode) || GMAVI_IS_WRITE(avi_file->mode))
        {
            fseek(avi_file->file, 0, SEEK_END);
            count = ftell(avi_file->file);
            fseek(avi_file->file, avi_file->entry.AviLength_riff_avi, SEEK_SET);
            write_int32(avi_file->file, count-8);
            // update stream header
            fseek(avi_file->file, avi_file->entry.AviLength_list_hdrl, SEEK_SET);
            write_int32(avi_file->file, avi_file->entry.StreamHeaderStart-(avi_file->entry.AviLength_list_hdrl+4)+avi_file->strh_pos);
            fseek(avi_file->file, avi_file->entry.StreamHeaderStart+avi_file->strh_pos, SEEK_SET);
            avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('J','U','N','K'), (avi_file->entry.StreamDataStart-12)-(avi_file->entry.StreamHeaderStart+avi_file->strh_pos)-8);
            // update stream data chunk
            fseek(avi_file->file, avi_file->entry.AviLength_list_movi, SEEK_SET);
            write_int32(avi_file->file, avi_file->data_pos+4);
            fseek(avi_file->file, avi_file->entry.StreamDataStart+avi_file->data_pos, SEEK_SET);
            avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('J','U','N','K'), (avi_file->entry.IndexStart-8)-(avi_file->entry.StreamDataStart+avi_file->data_pos)-8);
            // update index chunk
            fseek(avi_file->file, avi_file->entry.AviLength_idx1, SEEK_SET);
            write_int32(avi_file->file, avi_file->index_pos);
            fseek(avi_file->file, avi_file->entry.IndexStart+avi_file->index_pos, SEEK_SET);
            avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('J','U','N','K'), count-(avi_file->entry.IndexStart+avi_file->index_pos)-8);
        }
        else
        {
//            unsigned char buf[16];
            
            //merge avi with index
			fseek(avi_file->file, avi_file->entry.MainHeaderStart+24, SEEK_SET);
			write_int32(avi_file->file, avi_file->frame_count);
            fseek(avi_file->idxfile, 0, SEEK_SET);
            fseek(avi_file->file, 0, SEEK_END);
            avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('i','d','x','1'), 16*avi_file->frame_count);
            for(i=0; i<avi_file->frame_count; i++)
            {
                avi_read_index(avi_file->idxfile, &index);
                avi_write_index(avi_file->file, &index);
            }
            count = ftell(avi_file->file);
            fseek(avi_file->file, avi_file->entry.AviLength_riff_avi, SEEK_SET);
            write_int32(avi_file->file, count-8);
            fseek(avi_file->file, avi_file->entry.AviLength_list_hdrl, SEEK_SET);
            write_int32(avi_file->file, (avi_file->entry.StreamDataStart-12)-(avi_file->entry.AviLength_list_hdrl+4));
            fseek(avi_file->file, avi_file->entry.AviLength_list_movi, SEEK_SET);
            write_int32(avi_file->file, avi_file->data_pos+4);
        }
    }

    if(avi_file->file)
    {
        fclose(avi_file->file);
        avi_file->file = NULL;
    }
    if(avi_file->idxfile && GMAVI_IS_CREATE(avi_file->mode) && !GMAVI_IS_FIXED_SIZE(avi_file->mode))
    {
        fclose(avi_file->idxfile);
        avi_file->idxfile = NULL;
    }
    
    CRITICAL_SECTION_DELETE(avi_file->CriticalSection);
    if(avi_file)
    {
        free(avi_file);
        avi_file = NULL;
    }

    return GMSTS_OK;
}

int GMAVISetChunkSize(HANDLE handle, GmAviChunkSize *chunk_size)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    
    if(!avi_file || !chunk_size)
        return GMSTS_INVALID_INPUT;

    //create LIST movi tag
    fseek(avi_file->file, avi_file->entry.StreamHeaderStart + chunk_size->stream_header_size, SEEK_SET);
    avi_put_list(avi_file->file, GM_MAKE_FOURCC('m','o','v','i'), 0);
    avi_file->entry.StreamDataStart = avi_file->entry.StreamHeaderStart + chunk_size->stream_header_size + 12;
    avi_file->entry.AviLength_list_movi = avi_file->entry.StreamDataStart-8;

    //create index tag
    fseek(avi_file->file, avi_file->entry.StreamDataStart+chunk_size->stream_data_size, SEEK_SET);
    avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('i','d','x','1'), 0);
    avi_file->entry.IndexStart = avi_file->entry.StreamDataStart + chunk_size->stream_data_size + 8;
    avi_file->entry.AviLength_idx1 = avi_file->entry.IndexStart-4;

    avi_file->hdrl_len = chunk_size->stream_header_size+4;
    avi_file->movi_len = chunk_size->stream_data_size+4;
    avi_file->idx1_len = chunk_size->index_size;

    return GMSTS_OK;
}

int GMAVISeek(HANDLE handle, int whence, GMSearchIndex *offset)
{
    GmAviFile *avi_file = (GmAviFile*) handle;

    if(!avi_file)
        return GMSTS_INVALID_INPUT;
/*
    if(whence==GMAVI_SEEK_SET)
        fseek(avi_file->file, avi_file->data_pos, SEEK_SET);
    else if(whence==GMAVI_SEEK_CUR)
        fseek(avi_file->file, avi_file->data_pos, SEEK_CUR);
    else if(whence==GMAVI_SEEK_END)
        fseek(avi_file->file, avi_file->data_pos, SEEK_END);
    else if(whence==GMAVI_SEEK_STREAM_HEADER_OFFSET)
        avi_file->strh_pos = offset;
    else if(whence==GMAVI_SEEK_STREAM_DATA_OFFSET)
        avi_file->data_pos = offset;
    else if(whence==GMAVI_SEEK_INDEX_OFFSET)
        avi_file->index_pos = offset;
*/
    if(whence==GMAVI_SEEK_GIVEN_INDEX)
    {
        avi_file->data_pos = offset->offset;
        avi_file->index_pos = offset->index;
    }
    else if(whence==GMAVI_SEEK_TO_END)  // this is in effect before any writing
    {
        avi_file->strh_pos = avi_file->hdrl_len-sizeof(AviMainHeader)-4;
        avi_file->data_pos = avi_file->movi_len-4;
        avi_file->index_pos = avi_file->idx1_len;
    }
    else if(whence==GMAVI_SEEK_TO_BEGINNING)
    {
        avi_file->frame_count=0;
        avi_file->data_pos = 0;
        avi_file->index_pos = 0; 		      
    }
    else
        return GMSTS_INVALID_INPUT;

    return GMSTS_OK;
}

int GMAVITell(HANDLE handle, int tell_what, GMSearchIndex *offset)
{
    GmAviFile *avi_file = (GmAviFile*) handle;

    if(!avi_file)
        return GMSTS_INVALID_INPUT;

    if(tell_what==GMAVI_TELL_CUR)
    {
        offset->offset = avi_file->data_pos;
        offset->index = avi_file->index_pos;
    }
    else
        return GMSTS_INVALID_INPUT;
    
    return GMSTS_OK;
}

/* GMAVIReset resets AVI offset, let this file can be reused. */
int GMAVIReset(HANDLE handle, int items)
{
    GmAviFile *avi_file = (GmAviFile*) handle;

    if(!avi_file || !GMAVI_IS_WRITE(avi_file->mode))
        return GMSTS_INVALID_INPUT;

    if(GMAVI_IS_STREAM_HEADER_SECTION(items))
    {
        avi_file->hdrl_len = 4;
        avi_file->strh_pos = 0;
    }
    if(GMAVI_IS_STREAM_DATA_SECTION(items))
    {
        avi_file->movi_len = 4;
        avi_file->data_pos = 0;
    }
    if(GMAVI_IS_INDEX_SECTION(items))
    {
        avi_file->idx1_len = 0;
        avi_file->index_pos = 0;
    }

    return GMSTS_OK;
}


int GMAVISetAviMainHeader(HANDLE handle, AviMainHeader *avi_main_header)
{
    GmAviFile *avi_file = (GmAviFile*) handle;

    if(!avi_file || !avi_main_header)
        return GMSTS_INVALID_INPUT;

  //critical section start
    fseek(avi_file->file, avi_file->entry.MainHeaderStart, SEEK_SET);
    avi_write_avi_main_header(avi_file->file, avi_main_header);
  //critical section end
    return GMSTS_OK;
}

int GMAVISetStreamHeader(HANDLE handle, AviStreamHeader *avi_stream_header, GmAviStreamFormat *avi_stream_format, int *out_streamid)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int size = 0;

    if(!avi_file || !avi_stream_header || !avi_stream_format)
        return GMSTS_INVALID_INPUT;

  CRITICAL_SECTION_LOCK(avi_file->CriticalSection); 
  CRITICAL_SECTION_UNLOCK(avi_file->CriticalSection);


    fseek(avi_file->file, avi_file->entry.StreamHeaderStart+avi_file->strh_pos, SEEK_SET);

    size = sizeof(AviStreamHeader) + sizeof(GmAviStreamFormat) + 12;
    avi_put_list(avi_file->file, GM_MAKE_FOURCC('s','t','r','l'), size);
    avi_write_stream_header(avi_file->file, avi_stream_header);
    avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('s','t','r','f'), sizeof(GmAviStreamFormat));
    avi_write_stream_format(avi_file->file, avi_stream_format);
    avi_file->strh_pos += size + 8;

    if(GMAVI_IS_CREATE(avi_file->mode))
    {
        if(GMAVI_IS_FIXED_SIZE(avi_file->mode))
        {
            // LIST movi is already created
        }
        else
        {
            avi_put_list(avi_file->file, GM_MAKE_FOURCC('m','o','v','i'), 0);
            avi_file->entry.StreamDataStart = ftell(avi_file->file);
            avi_file->entry.AviLength_list_movi = avi_file->entry.StreamDataStart-8;
        }
    }

    
    // return fourcc as stream-id, like '01dc'
    if(*((int*)avi_stream_header->fccType) == GM_MAKE_FOURCC('v','i','d','s'))
        *out_streamid = GM_MAKE_FOURCC('0','0'+avi_file->stream_count,'d','c');
    else if(*((int*)avi_stream_header->fccType) == GM_MAKE_FOURCC('a','u','d','s'))
        *out_streamid = GM_MAKE_FOURCC('0','0'+avi_file->stream_count,'w','b');
    else 
        *out_streamid = GM_MAKE_FOURCC('0','0'+avi_file->stream_count,'g','m');
    avi_file->stream_count ++;

    return GMSTS_OK;
}



int GMAVISetStreamDataAndIndex(HANDLE handle, int streamid, unsigned char *data, int length, int intra,
                                      unsigned char *extra_data, int extra_length)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    AviIndex   index;
    int        is_align=0, is_extra_align=0, input_size=length;

//printf("gmavi-1\n");

    if(!avi_file)
        return GMSTS_INVALID_INPUT;
//printf("gmavi-2\n");

    if(extra_data)
        input_size += extra_length;
    if(avi_file->movi_len /* If movi_len is 0, this means it is not a fixed-size avi. */
        && input_size>((avi_file->entry.IndexStart-8)-(avi_file->entry.StreamDataStart-12)-avi_file->data_pos))
        return GMSTS_DATA_FULL;
//printf("gmavi-3\n");

    is_align = length % 2;

    //write extra data first (this lets index search much easier)
    fseek(avi_file->file, avi_file->entry.StreamDataStart+avi_file->data_pos, SEEK_SET);
    if(extra_data && extra_length)
    {
        is_extra_align = extra_length % 2;
        avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('J','U','N','K'), extra_length+is_extra_align);
        avi_write_data(avi_file->file, extra_data, extra_length);
        if(is_extra_align)
            write_char(avi_file->file, 0);
    }
    
    //write data
    avi_put_chunk(avi_file->file, streamid, length+is_align);
    avi_write_data(avi_file->file, data, length);
    if(is_align)
        write_char(avi_file->file, 0);


    //write index
    fseek(avi_file->idxfile, avi_file->entry.IndexStart+avi_file->index_pos, SEEK_SET);
    index.dwChunkId = streamid;
    index.dwFlags = (intra) ? 0x10:0;
    index.dwOffset = avi_file->data_pos + 4;
    index.dwSize = length;
    avi_write_index(avi_file->idxfile, &index);

    avi_file->data_pos += (8+length+is_align);
    if(extra_data && extra_length)
        avi_file->data_pos += (8+extra_length+is_extra_align);
    avi_file->index_pos += sizeof(AviIndex);
    avi_file->frame_count++;

    return GMSTS_OK;
}




int GMAVIGetAviMainHeader(HANDLE handle, AviMainHeader *avi_main_header)
{
    GmAviFile *avi_file = (GmAviFile*) handle;

    if(!avi_file || !avi_main_header)
        return GMSTS_INVALID_INPUT;

    fseek(avi_file->file, avi_file->entry.MainHeaderStart, SEEK_SET);
    avi_read_avi_main_header(avi_file->file, avi_main_header);

    if(*((int*)avi_main_header->fcc)!=GM_MAKE_FOURCC('a','v','i','h'))
        return GMSTS_VARIANT_FORMAT;
    else
        return GMSTS_OK;
}

int GMAVIGetStreamHeaderNum(HANDLE handle, int *count)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int strl_tag, strl_size, ret;

    if(!avi_file || !count)
        return GMSTS_INVALID_INPUT;

    (*count) = 0;
    fseek(avi_file->file, avi_file->entry.StreamHeaderStart, SEEK_SET);

    while(1)
    {
        ret = avi_get_list(avi_file->file, &strl_tag, &strl_size);
        if(ret==GMSTS_OK && strl_tag==GM_MAKE_FOURCC('s','t','r','l'))
        {
            (*count)++;
            fseek(avi_file->file, strl_size-4, SEEK_CUR);
        }
        else
            break;
    }
    if(ret<0)
        return GMSTS_INVALID_FORAMT;
    else
        return GMSTS_OK;
}

/*
int GMAVIGetStreamHeaderSize(HANDLE handle, int num, int *header_size, int *format_size)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int i, strl_tag, strl_size, strf_tag;
    AviStreamHeader avi_stream_header;
    if(!avi_file || !num)
        return GMSTS_INVALID_INPUT;
    fseek(avi_file->file, avi_file->entry.StreamHeaderStart, SEEK_SET);
    for(i=0; i<num-1; i++)
    {
        avi_get_list(avi_file->file, &strl_tag, &strl_size);
        fseek(avi_file->file, strl_size-4, SEEK_CUR);
    }
    avi_read_stream_header(avi_file->file, &avi_stream_header);
    *header_size = avi_stream_header.cb;
    avi_get_chunk(avi_file->file, &strf_tag, format_size);
    return GMSTS_OK;
}
*/

int GMAVIGetStreamHeader(HANDLE handle, int num, AviStreamHeader *avi_stream_header, GmAviStreamFormat *avi_stream_format, int *out_streamid)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int i, strl_tag, strl_size, strf_tag, strf_size;

    if(!avi_file || !avi_stream_header || !avi_stream_format || !out_streamid)
        return GMSTS_INVALID_INPUT;

    fseek(avi_file->file, avi_file->entry.StreamHeaderStart, SEEK_SET);

    for(i=0; i<num-1; i++)
    {
        avi_get_list(avi_file->file, &strl_tag, &strl_size);
        fseek(avi_file->file, strl_size-4, SEEK_CUR);
    }

    avi_get_list(avi_file->file, &strl_tag, &strl_size);
	avi_read_stream_header(avi_file->file, avi_stream_header);
    avi_get_chunk(avi_file->file, &strf_tag, &strf_size);
    avi_read_stream_format(avi_file->file, avi_stream_format);

    if(*((int*)avi_stream_header->fccType) == GM_MAKE_FOURCC('v','i','d','s'))
        *out_streamid = GM_MAKE_FOURCC('0','0'+(num-1),'d','c');
    else if(*((int*)avi_stream_header->fccType) == GM_MAKE_FOURCC('a','u','d','s'))
        *out_streamid = GM_MAKE_FOURCC('0','0'+(num-1),'w','b');
    else 
        *out_streamid = GM_MAKE_FOURCC('0','0'+(num-1),'g','m');

    return GMSTS_OK;
}

int GMAVIGetStreamDataAndIndex(HANDLE handle, int *streamid, unsigned char *data,
    int *length, int *intra,unsigned char *extra_data, int *extra_length, 
    int frame_no, int reverse,int *pos)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int strdata_len, junk_tag, junk_size;
    AviIndex   index;
//printf("<%d,%x>",frame_no,avi_file->data_pos);

	if(reverse)
	{
      	avi_file->data_pos=*pos;//backup_offset[I_offset-1];
	}
    else
    {
        *pos=avi_file->data_pos;
    }
        
    if(!avi_file)
        return GMSTS_INVALID_INPUT;

    if(avi_file->data_pos>=(avi_file->movi_len-4))
        return GMSTS_END_OF_DATA;

    //read extra data
    fseek(avi_file->file, avi_file->entry.StreamDataStart+avi_file->data_pos, SEEK_SET);
    avi_get_chunk(avi_file->file, &junk_tag, &junk_size);
    if(junk_tag==GM_MAKE_FOURCC('J','U','N','K'))
    {
        junk_size += junk_size%2; /* WORD alignment */
        if(extra_data)
        {
            if(junk_size>*extra_length)
            {
                *extra_length = junk_size;
                return GMSTS_NOT_ENOUGH_SPACE;
            }
            avi_read_data(avi_file->file, extra_data, junk_size);
        }
        else
        {
            fseek(avi_file->file, junk_size, SEEK_CUR);
        }
    }
    else
    {
        fseek(avi_file->file, -8, SEEK_CUR);
        junk_size=0;
    }

    //read data
    avi_get_chunk(avi_file->file, streamid, &strdata_len);
    strdata_len += strdata_len%2;     /* WORD alignment */
    if(strdata_len>*length)
    {
        *length = strdata_len;
        return GMSTS_NOT_ENOUGH_SPACE;
    }
    avi_read_data(avi_file->file, data, strdata_len);
	//*length = strdata_len;

        
    avi_file->data_pos += (8+strdata_len);
    if(junk_size)
        avi_file->data_pos += (8+junk_size);
    
    fseek(avi_file->file, avi_file->entry.IndexStart+avi_file->frame_count*sizeof(AviIndex), SEEK_SET);
    avi_read_index(avi_file->file, &index);
    *length = index.dwSize;
    //read index
    if(intra)
    {
        fseek(avi_file->file, avi_file->entry.IndexStart+avi_file->frame_count*sizeof(AviIndex), SEEK_SET);
        avi_read_index(avi_file->file, &index);
        if ((index.dwFlags & 0x10) == 0x10)
        	*intra = 1;//I frame
        else
        	*intra = 0;//P frame justin
        avi_file->index_pos = avi_file->frame_count*sizeof(AviIndex);
    }
		avi_file->frame_count++;
		
    return GMSTS_OK;
}


int GMAVIConditionIndexSearch(HANDLE handle, GMSearchIndex *out_value, int mode, int value1, int value2)
{
    int ret=GMSTS_RECORD_NOT_FOUND, junk_tag, junk_size;
    GmAviFile *avi_file = (GmAviFile*) handle;
    AviIndex  index;

    if(!avi_file || !out_value)
        return GMSTS_INVALID_INPUT;

    if(mode==GMAVI_SEARCH_GivenTime)
    {
        int streamid = value1;
        int timestamp = value2;
        int count=0, offset=0;
        GMExtraInfo extra_info;

        fseek(avi_file->file, avi_file->entry.StreamDataStart, SEEK_SET);
        while(offset<avi_file->movi_len)
        {
            avi_get_chunk(avi_file->file, &junk_tag, &junk_size);
            if(junk_tag==GM_MAKE_FOURCC('J','U','N','K') && junk_size==sizeof(GMExtraInfo))
            {
                avi_read_data(avi_file->file, (unsigned char*)&extra_info, junk_size);
                if(extra_info.timestamp>=timestamp)
                {
                    out_value->offset = offset;
                    out_value->index = (count+1)*sizeof(AviIndex);
                    ret = GMSTS_OK;
                    break;
                }
            }
            else
            {
                fseek(avi_file->file, junk_size, SEEK_CUR);
                count++;
            }
            offset += 8+junk_size;
        }
    }
    else if(mode==GMAVI_SEARCH_StartTag)
    {
        int streamid = value1;
        int cur_idx = value2;
        int bw_offset = 2*sizeof(AviIndex);
        
        fseek(avi_file->file, avi_file->entry.IndexStart+cur_idx+sizeof(AviIndex), SEEK_SET);
        while(cur_idx>=sizeof(AviIndex))
        {
            fseek(avi_file->file, -bw_offset, SEEK_CUR);    //backward search
            cur_idx -= sizeof(AviIndex);
            avi_read_index(avi_file->file, &index);
            if(index.dwChunkId == streamid)
            {
                if(index.dwFlags == GMTAG_TYPE_CH_START || index.dwFlags == GMTAG_TYPE_CH_CONTINUE)
                {
                    out_value->offset = index.dwOffset-4;   /* minus "movi" tag length */
                    out_value->index = cur_idx;
                    ret = GMSTS_OK;
                    break;
                }
            }
        }
    }
    else
        ret = GMSTS_INVALID_INPUT;

    return ret;
}


int GMAVIUpdateStreamHeader(HANDLE handle, int num, AviStreamHeader *avi_stream_header, GmAviStreamFormat *avi_stream_format)
{
    GmAviFile *avi_file = (GmAviFile*) handle;
    int i, size, strl_tag, strl_size;

    if(!avi_file || !avi_stream_header || !avi_stream_format)
        return GMSTS_INVALID_INPUT;

    fseek(avi_file->file, avi_file->entry.StreamHeaderStart, SEEK_SET);

    for(i=0; i<num; i++)
    {
        avi_get_list(avi_file->file, &strl_tag, &strl_size);
        fseek(avi_file->file, strl_size-4, SEEK_CUR);
    }

    size = sizeof(AviStreamHeader) + sizeof(GmAviStreamFormat) + 12;
    avi_put_list(avi_file->file, GM_MAKE_FOURCC('s','t','r','l'), size);
    avi_write_stream_header(avi_file->file, avi_stream_header);
    avi_put_chunk(avi_file->file, GM_MAKE_FOURCC('s','t','r','f'), sizeof(GmAviStreamFormat));
    avi_write_stream_format(avi_file->file, avi_stream_format);

    return GMSTS_OK;
}


int GMAVIFillAviMainHeaderValues(AviMainHeader *header, int width, int height, int framerate, int bitrate, int framecount)
{
    if(!header || framerate==0)
        return GMSTS_INVALID_INPUT;
#if 1 
    memset(header, 0x0, sizeof(AviMainHeader));
    *((unsigned int*)header->fcc) = GM_MAKE_FOURCC('a','v','i','h');
    header->cb = sizeof(AviMainHeader)-8;
    header->dwMicroSecPerFrame = 1000000/framerate;
    header->dwMaxBytesPerSec = bitrate;
    header->dwPaddingGranularity = 0;
    header->dwFlags = AVIF_HASINDEX|AVIF_MUSTUSEINDEX|AVIF_WASCAPTUREFILE;
    header->dwTotalFrames = framecount;     //<==need update
    header->dwInitialFrames = 0;
    header->dwStreams = 1;                  //<==need update
    header->dwSuggestedBufferSize = width*height*3;
    header->dwWidth = width;
    header->dwHeight = height;
    //avi_main_header->dwReserved[4];
#else
//AviMainHeader
    memset(header, 0x0, sizeof(AviMainHeader));
    *((unsigned int*)header->fcc) = GM_MAKE_FOURCC('a','v','i','h');
    header->cb = sizeof(AviMainHeader)-8;
    header->dwMicroSecPerFrame = 33333;
    header->dwMaxBytesPerSec = 128000;
    header->dwPaddingGranularity = 0;
    header->dwFlags = 0x910;
    header->dwTotalFrames = 60;
    header->dwInitialFrames = 0;
    header->dwStreams = 1;
    header->dwSuggestedBufferSize = 1048576;
    header->dwWidth = 720;
    header->dwHeight = 480;
#endif



    return GMSTS_OK;
}

int GMAVIFillVideoStreamHeaderValues(AviStreamHeader *header, GmAviStreamFormat *format, int type,
                                int width, int height, int framerate, int bitrate, int framecount)
{
    int fcc_type=0;
    if(!header)
        return GMSTS_INVALID_INPUT;


    switch(type)
    {
    case GMAVI_TYPE_H264:
    case GMAVI_TYPE_MPEG4:
    case GMAVI_TYPE_MJPEG:
        fcc_type = GM_MAKE_FOURCC('v','i','d','s');
        break;
    default:
        break;
    }
#if 1    
    memset(header, 0x0, sizeof(AviStreamHeader));
    *((unsigned int*)header->fcc) = GM_MAKE_FOURCC('s','t','r','h');
    header->cb = sizeof(AviStreamHeader)-8;
    *((unsigned int*)header->fccType) = fcc_type;
    *((unsigned int*)header->fccHandler) = type;
    header->dwFlags = 0x0;
    header->wPriority = 0;
    header->wLanguage = 0;
    header->dwInitialFrames = 0;
    header->dwScale = 1001000;
    header->dwRate = framerate*header->dwScale;
    header->dwStart = 0;
    header->dwLength = framecount;          //<==need update
    header->dwSuggestedBufferSize = width*height*3;
    header->dwQuality = -1;
    header->dwSampleSize = 0;
    header->rcFrame.left = 0;
    header->rcFrame.top = 0;
    header->rcFrame.right = width;
    header->rcFrame.bottom = height;

    memset(format, 0x0, sizeof(GmAviStreamFormat));
    format->video_format.bmiHeader.biSize = sizeof(BitMapInfoHeader);
    format->video_format.bmiHeader.biWidth = width;
    format->video_format.bmiHeader.biHeight = height;
    format->video_format.bmiHeader.biPlanes = 1; 
    format->video_format.bmiHeader.biBitCount = 24; 
    format->video_format.bmiHeader.biCompression = type; 
    format->video_format.bmiHeader.biSizeImage = 0; 
    format->video_format.bmiHeader.biXPelsPerMeter = 0; 
    format->video_format.bmiHeader.biYPelsPerMeter = 0; 
    format->video_format.bmiHeader.biClrUsed = 0; 
    format->video_format.bmiHeader.biClrImportant = 0; 

#else
    memset(header, 0x0, sizeof(AviStreamHeader));
    *((unsigned int*)header->fcc) = GM_MAKE_FOURCC('s','t','r','h');
    header->cb = sizeof(AviStreamHeader)-8;;
    *((unsigned int*)header->fccType) = GM_MAKE_FOURCC('v','i','d','s');
    *((unsigned int*)header->fccHandler) = GM_MAKE_FOURCC('H','2','6','4');
    header->dwFlags = 0x0;
    header->wPriority = 0;
    header->wLanguage = 0;
    header->dwInitialFrames = 0;
    header->dwScale = 1001000;
    header->dwRate = 30030000;
    header->dwStart = 0;
    header->dwLength = 60;
    header->dwSuggestedBufferSize = 1048576;
    header->dwQuality = -1;
    header->dwSampleSize = 1036800;
    header->rcFrame.left = 0;
    header->rcFrame.top = 0;
    header->rcFrame.right = 720;
    header->rcFrame.bottom = 480;
//GmAviStreamFormat
    memset(format, 0x0, sizeof(GmAviStreamFormat));
    format->video_format.bmiHeader.biSize = sizeof(BitMapInfoHeader);
    format->video_format.bmiHeader.biWidth = 720;
    format->video_format.bmiHeader.biHeight = 480;
    format->video_format.bmiHeader.biPlanes = 1; 
    format->video_format.bmiHeader.biBitCount = 24; 
    format->video_format.bmiHeader.biCompression = GM_MAKE_FOURCC('H','2','6','4');; 
    format->video_format.bmiHeader.biSizeImage = 1036800; 
    format->video_format.bmiHeader.biXPelsPerMeter = 0; 
    format->video_format.bmiHeader.biYPelsPerMeter = 0; 
    format->video_format.bmiHeader.biClrUsed = 0; 
    format->video_format.bmiHeader.biClrImportant = 0; 
#endif


    return GMSTS_OK;
}

int GMAVIFillAudioStreamHeaderValues(AviStreamHeader *header, GmAviStreamFormat *format, int type,
                                int channels, int sample_size, int bitrate)
{
    int fcc_type=0;
    if(!header)
        return GMSTS_INVALID_INPUT;


    switch(type)
    {
    case GMAVI_TYPE_PCM:
    case GMAVI_TYPE_MP3:
        fcc_type = GM_MAKE_FOURCC('a','u','d','s');
        break;
    default:
        break;
    }
    
    memset(header, 0x0, sizeof(AviStreamHeader));
    *((unsigned int*)header->fcc) = GM_MAKE_FOURCC('s','t','r','h');
    header->cb = sizeof(AviStreamHeader)-8;
    *((unsigned int*)header->fccType) = fcc_type;
    *((unsigned int*)header->fccHandler) = 0/*type*/;
    header->dwFlags = 0x0;
    header->wPriority = 0;
    header->wLanguage = 0;
    header->dwInitialFrames = 0;
    header->dwScale = 1001000;
    header->dwRate = 176400;
    header->dwStart = 0;
    header->dwLength = 12345;          //<==need update
    header->dwSuggestedBufferSize = 56789;
    header->dwQuality = -1;
    header->dwSampleSize = 4;

    memset(format, 0x0, sizeof(GmAviStreamFormat));
    format->audio_format.wFormatTag = 1;
    format->audio_format.nChannels = channels;
    format->audio_format.nSamplesPerSec = 5787888;
    format->audio_format.nAvgBytesPerSec = 1234; 
    format->audio_format.nBlockAlign = 5678; 
    format->audio_format.wBitsPerSample = sample_size; 
    format->audio_format.cbSize = 3456; 

    return GMSTS_OK;
}



