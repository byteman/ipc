/********************************************************************  
filename:   RTMPStream.cpp 
created:    2013-04-3 
author:     firehood  
purpose:    发送H264视频到RTMP Server，使用libRtmp库 
*********************************************************************/   
#include "RTMPStream.h"  
#include "SpsDecode.h"  
#ifdef WIN32    
#include <windows.h>  
#endif  


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

#include <ctype.h>
#include <limits.h>
#include <unistd.h>

#include "dvr_common_api.h"
#include "dvr_enc_api.h"

#ifdef WIN32  
#pragma comment(lib,"WS2_32.lib")  
#pragma comment(lib,"winmm.lib")  
#endif  
  
int dvr_fd = 0;
int enc_fd = 0;
extern bool quit = 0;
#define WIDTH 1280
#define HEIGHT 720
//test record
unsigned char *bs_buf;

int avi_str_id;
int enc_buf_size;
struct pollfd rec_fds;

enum  
{  
    FLV_CODECID_H264 = 7,  
};  
  
int InitSockets()    
{    
#ifdef WIN32    
    WORD version;    
    WSADATA wsaData;    
    version = MAKEWORD(1, 1);    
    return (WSAStartup(version, &wsaData) == 0);    
#else    
    return TRUE;    
#endif    
}    
  
inline void CleanupSockets()    
{    
#ifdef WIN32    
    WSACleanup();    
#endif    
}    
  
char * put_byte( char *output, uint8_t nVal )    
{    
    output[0] = nVal;    
    return output+1;    
}    
char * put_be16(char *output, uint16_t nVal )    
{    
    output[1] = nVal & 0xff;    
    output[0] = nVal >> 8;    
    return output+2;    
}    
char * put_be24(char *output,uint32_t nVal )    
{    
    output[2] = nVal & 0xff;    
    output[1] = nVal >> 8;    
    output[0] = nVal >> 16;    
    return output+3;    
}    
char * put_be32(char *output, uint32_t nVal )    
{    
    output[3] = nVal & 0xff;    
    output[2] = nVal >> 8;    
    output[1] = nVal >> 16;    
    output[0] = nVal >> 24;    
    return output+4;    
}    
char *  put_be64( char *output, uint64_t nVal )    
{    
    output=put_be32( output, nVal >> 32 );    
    output=put_be32( output, nVal );    
    return output;    
}    
char * put_amf_string( char *c, const char *str )    
{    
    uint16_t len = strlen( str );    
    c=put_be16( c, len );    
    memcpy(c,str,len);    
    return c+len;    
}    
char * put_amf_double( char *c, double d )    
{    
    *c++ = AMF_NUMBER;  /* type: Number */    
    {    
        unsigned char *ci, *co;    
        ci = (unsigned char *)&d;    
        co = (unsigned char *)c;    
        co[0] = ci[7];    
        co[1] = ci[6];    
        co[2] = ci[5];    
        co[3] = ci[4];    
        co[4] = ci[3];    
        co[5] = ci[2];    
        co[6] = ci[1];    
        co[7] = ci[0];    
    }    
    return c+8;    
}  
  
CRTMPStream::CRTMPStream(void):  
m_pRtmp(NULL),  
m_nFileBufSize(0),  
m_nCurPos(0)  
{  
    m_pFileBuf = new unsigned char[FILEBUFSIZE];  
    memset(m_pFileBuf,0,FILEBUFSIZE);  
    InitSockets();  
    m_pRtmp = RTMP_Alloc();    
    RTMP_Init(m_pRtmp);    
}  
  
CRTMPStream::~CRTMPStream(void)  
{  
    Close();  
#ifdef WIN32   
    WSACleanup();    
#endif
    delete[] m_pFileBuf;  
}  
  
bool CRTMPStream::Connect(const char* url)  
{  
    if(RTMP_SetupURL(m_pRtmp, (char*)url)<0)  
    {  
        return FALSE;  
    }  
    RTMP_EnableWrite(m_pRtmp);  
    if(RTMP_Connect(m_pRtmp, NULL)<0)  
    {  
        return FALSE;  
    }  
    if(RTMP_ConnectStream(m_pRtmp,0)<0)  
    {  
        return FALSE;  
    }  
    return TRUE;  
}  
  
void CRTMPStream::Close()  
{  
    if(m_pRtmp)  
    {  
        RTMP_Close(m_pRtmp);  
        RTMP_Free(m_pRtmp);  
        m_pRtmp = NULL;  
    }  
}  
  
int CRTMPStream::SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp)  
{  
    if(m_pRtmp == NULL)  
    {  
        return FALSE;  
    }  
  
    RTMPPacket packet;  
    RTMPPacket_Reset(&packet);  
    RTMPPacket_Alloc(&packet,size);  
  
    packet.m_packetType = nPacketType;  
    packet.m_nChannel = 0x04;    
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;    
    packet.m_nTimeStamp = nTimestamp;    
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;  
    packet.m_nBodySize = size;  
    memcpy(packet.m_body,data,size);  
  
    int nRet = RTMP_SendPacket(m_pRtmp,&packet,0);  
  
    RTMPPacket_Free(&packet);  
  
    return nRet;  
}  
  
bool CRTMPStream::SendMetadata(LPRTMPMetadata lpMetaData)  
{  
    if(lpMetaData == NULL)  
    {  
        return false;  
    }  
    char body[1024] = {0};;  
      
    char * p = (char *)body;    
    p = put_byte(p, AMF_STRING );  
    p = put_amf_string(p , "@setDataFrame" );  
  
    p = put_byte( p, AMF_STRING );  
    p = put_amf_string( p, "onMetaData" );  
  
    p = put_byte(p, AMF_OBJECT );    
    p = put_amf_string( p, "copyright" );    
    p = put_byte(p, AMF_STRING );    
    p = put_amf_string( p, "firehood" );    
  
    p =put_amf_string( p, "width");  
    p =put_amf_double( p, lpMetaData->nWidth);  
  
    p =put_amf_string( p, "height");  
    p =put_amf_double( p, lpMetaData->nHeight);  
  
    p =put_amf_string( p, "framerate" );  
    p =put_amf_double( p, lpMetaData->nFrameRate);   
  
    p =put_amf_string( p, "videocodecid" );  
    p =put_amf_double( p, FLV_CODECID_H264 );  
  
    p =put_amf_string( p, "" );  
    p =put_byte( p, AMF_OBJECT_END  );  
  
    int index = p-body;  
  
    SendPacket(RTMP_PACKET_TYPE_INFO,(unsigned char*)body,p-body,0);  
  
    int i = 0;  
    body[i++] = 0x17; // 1:keyframe  7:AVC  
    body[i++] = 0x00; // AVC sequence header  
  
    body[i++] = 0x00;  
    body[i++] = 0x00;  
    body[i++] = 0x00; // fill in 0;  
  
    // AVCDecoderConfigurationRecord.  
    body[i++] = 0x01; // configurationVersion  
    body[i++] = lpMetaData->Sps[1]; // AVCProfileIndication  
    body[i++] = lpMetaData->Sps[2]; // profile_compatibility  
    body[i++] = lpMetaData->Sps[3]; // AVCLevelIndication   
    body[i++] = 0xff; // lengthSizeMinusOne    
  
    // sps nums  
    body[i++] = 0xE1; //&0x1f  
    // sps data length  
    body[i++] = lpMetaData->nSpsLen>>8;  
    body[i++] = lpMetaData->nSpsLen&0xff;  
    // sps data  
    memcpy(&body[i],lpMetaData->Sps,lpMetaData->nSpsLen);  
    i= i+lpMetaData->nSpsLen;  
  
    // pps nums  
    body[i++] = 0x01; //&0x1f  
    // pps data length   
    body[i++] = lpMetaData->nPpsLen>>8;  
    body[i++] = lpMetaData->nPpsLen&0xff;  
    // sps data  
    memcpy(&body[i],lpMetaData->Pps,lpMetaData->nPpsLen);  
    i= i+lpMetaData->nPpsLen;  
  
    return SendPacket(RTMP_PACKET_TYPE_VIDEO,(unsigned char*)body,i,0);  
  
}  
  
bool CRTMPStream::SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp)  
{  
    if(data == NULL && size<11)  
    {  
        return false;  
    }  
  
    unsigned char *body = new unsigned char[size+9];  
  
    int i = 0;  
    if(bIsKeyFrame)  
    {  
        body[i++] = 0x17;// 1:Iframe  7:AVC  
    }  
    else  
    {  
        body[i++] = 0x27;// 2:Pframe  7:AVC  
    }  
    body[i++] = 0x01;// AVC NALU  
    body[i++] = 0x00;  
    body[i++] = 0x00;  
    body[i++] = 0x00;  
  
    // NALU size  
    body[i++] = size>>24;  
    body[i++] = size>>16;  
    body[i++] = size>>8;  
    body[i++] = size&0xff;;  
  
    // NALU data  
    memcpy(&body[i],data,size);  
  
    bool bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp);  
  
    delete[] body;  
  
    return bRet;  
}  
bool CRTMPStream::SendVideo()
{
	int ret = 0, ch_num = 0;    

    dvr_enc_channel_param   ch_param;    
    EncParam_Ext3 enc_param_ext = {0};    
    dvr_enc_control  enc_ctrl;
    
   
    FuncTag tag;        
    struct timeval t1,t2;  
    char tmp_str[128];
                 
    printf("send video\n");    
    dvr_fd = open("/dev/dvr_common", O_RDWR);   //open_dvr_common
    if(dvr_fd == -1)
  	{
  		 printf("open dvr_common failed\n" );
  		 return false;
  	}       
    enc_fd = open("/dev/dvr_enc", O_RDWR);      //open_dvr_encode
    if(enc_fd == -1)
  	{
  		 printf("open dvr_enc failed\n" );
  		 return false;
  	} 
 
    //set dvr encode source parameter
    ch_param.src.input_system = MCP_VIDEO_NTSC;
    ch_param.src.channel = ch_num;
    ch_param.src.enc_src_type = ENC_TYPE_FROM_CAPTURE;
    
    ch_param.src.dim.width = WIDTH;
    ch_param.src.dim.height = HEIGHT;
    
    ch_param.src.di_mode = LVFRAME_EVEN_ODD;
    ch_param.src.mode = LVFRAME_FRAME_MODE;
    ch_param.src.dma_order = DMAORDER_PACKET;
    ch_param.src.scale_indep = CAPSCALER_NOT_KEEP_RATIO;
    ch_param.src.input_system = MCP_VIDEO_NTSC;
    ch_param.src.color_mode = CAPCOLOR_YUV422;
    
    ch_param.src.vp_param.is_3DI = FALSE;
    ch_param.src.vp_param.is_denoise = FALSE;
    ch_param.src.vp_param.denoise_mode = GM3DI_FIELD;
    
    //set dvr encode main bitstream parameter
    ch_param.main_bs.enabled = DVR_ENC_EBST_ENABLE;
    ch_param.main_bs.out_bs = 0;
    ch_param.main_bs.enc_type = ENC_TYPE_H264;
    ch_param.main_bs.is_blocked = FALSE;
    ch_param.main_bs.en_snapshot = DVR_ENC_EBST_DISABLE;
    
    ch_param.main_bs.dim.width = WIDTH;
    ch_param.main_bs.dim.height = HEIGHT;
    
    //set main bitstream encode parameter
    ch_param.main_bs.enc.input_type = ENC_INPUT_H2642D;
    ch_param.main_bs.enc.frame_rate = 30;
    ch_param.main_bs.enc.bit_rate = 1048576;
    ch_param.main_bs.enc.ip_interval = 30;
    ch_param.main_bs.enc.init_quant = 25;
    ch_param.main_bs.enc.max_quant = 51;
    ch_param.main_bs.enc.min_quant = 1;
    ch_param.main_bs.enc.is_use_ROI = FALSE;
    ch_param.main_bs.enc.ROI_win.x = 0;
    ch_param.main_bs.enc.ROI_win.y = 0;
    ch_param.main_bs.enc.ROI_win.width = 352;
    ch_param.main_bs.enc.ROI_win.height = 240;
    
    //set main bitstream scalar parameter
    ch_param.main_bs.scl.src_fmt = SCALE_YUV422;
    ch_param.main_bs.scl.dst_fmt = SCALE_YUV422;
    ch_param.main_bs.scl.scale_mode = SCALE_LINEAR;
    ch_param.main_bs.scl.is_dither = FALSE;
    ch_param.main_bs.scl.is_correction = FALSE;
    ch_param.main_bs.scl.is_album = TRUE;
    ch_param.main_bs.scl.des_level = 0;
    
    //set main bitstream snapshot parameter
    ch_param.main_bs.snap.sample = JCS_yuv420;   
    ch_param.main_bs.snap.RestartInterval = 0;   
    ch_param.main_bs.snap.u82D = JENC_INPUT_MP42D;   
    ch_param.main_bs.snap.quality = 70;   
    
    //associate the ext. structure                      
    ch_param.main_bs.enc.ext_size = DVR_ENC_MAGIC_ADD_VAL(sizeof(enc_param_ext));
    ch_param.main_bs.enc.pext_data = &enc_param_ext;
        
    enc_param_ext.feature_enable = 0;      //CBR
        
    ioctl(enc_fd, DVR_ENC_SET_CHANNEL_PARAM, &ch_param);   
    ioctl(enc_fd, DVR_ENC_QUERY_OUTPUT_BUFFER_SIZE, &enc_buf_size);
    printf("enc_buf_size=%d\n",enc_buf_size);              
    bs_buf = (unsigned char*) mmap(NULL, enc_buf_size, PROT_READ|PROT_WRITE, 
                                               MAP_SHARED, enc_fd, 0);
    /////////////////////////////////////////////////////////////////            
    //record start
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));                    
    enc_ctrl.command = ENC_START;
    enc_ctrl.stream = 0;    
    ret = ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    if(ret < 0)
  	{
  		 printf("DVR_ENC_CONTROL failed\n" );
  		 return false;
  	} 
       
    // set function tag paremeter to dvr graph level
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_num);
    ret = ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);
    if(ret < 0)
  	{
  		 printf("DVR_COMMON_APPLY failed\n" );
  		 return false;
  	} 
   
    ///////////////////////////////////////////////////////////////         
    //sprintf(file_name, "CH%d_video_%d", 0, 0);      
    //sprintf(tmp_str, "%s.h264", file_name);            
    //rec_file = fopen ( tmp_str , "wb+" );
    gettimeofday(&t1, NULL);        
        
    
    
  	RTMPMetadata metaData;  
	memset(&metaData,0,sizeof(RTMPMetadata));  
	  
	  // 读取SPS帧  
	NaluUnit naluUnit; 
	naluUnit.type = 0; 
	while(naluUnit.type != 8 && !quit)
    {
	  	ReadOneNaulFromVpu(naluUnit);
	  	printf("read sps get[%d]\n",naluUnit.type);
    }
    metaData.nSpsLen = naluUnit.size;  
	memcpy(metaData.Sps,naluUnit.data,naluUnit.size);  
	
// 读取PPS帧  
	while(naluUnit.type != 7 && !quit)
	{
		ReadOneNaulFromVpu(naluUnit);
		printf("read pps get[%d]\n",naluUnit.type);
	}
	metaData.nPpsLen = naluUnit.size;  
	memcpy(metaData.Pps,naluUnit.data,naluUnit.size);  
  
	// 解码SPS,获取视频图像宽、高信息  
	int width = WIDTH,height = HEIGHT;  
	if(h264_decode_sps(metaData.Sps, metaData.nSpsLen , width, height))
	{
		metaData.nWidth = width;  
		metaData.nHeight = height;  
		printf("decode_sps ok!\n");
	}
	printf("width=%d,height=%d\n",width,height);
	metaData.nFrameRate = 25;  
		 
	// 发送MetaData  
	SendMetadata(&metaData);  
    while(!quit) 
    {          
		unsigned int tick = 0;  
		while(ReadOneNaluFromBuf(naluUnit))  
		{  
			bool bKeyframe  = (naluUnit.type == 0x05) ? TRUE : FALSE;  
			printf("send packet type=%d,size=%d\n",naluUnit.type, naluUnit.size);
			// 发送H264数据帧  
			SendH264Packet(naluUnit.data,naluUnit.size,bKeyframe,tick);  
			
			tick +=40;  //按帧率25计算，累加时间戳。
			
		}  
		gettimeofday(&t2, NULL);

		if ((t2.tv_sec - t1.tv_sec) == 60) {      //<record for 20 seconds. then finish record.     

			break;               
		}       
    }
        
    //record stop
    memset(&enc_ctrl, 0x0, sizeof(dvr_enc_control));    
    enc_ctrl.stream = 0;
    enc_ctrl.command = ENC_STOP;
    ioctl(enc_fd, DVR_ENC_CONTROL, &enc_ctrl);
    
    FN_RESET_TAG(&tag);
    FN_SET_REC_CH(&tag, ch_num);
    ioctl(dvr_fd, DVR_COMMON_APPLY, &tag);    
    munmap((void*)bs_buf, enc_buf_size);      
    
    printf("----------------------------------\n");
    printf(" Send finish\n");
    printf("----------------------------------\n");    
    
    close(enc_fd);      //close_dvr_encode    
    close(dvr_fd);      //close_dvr_common

	return true;
}

bool CRTMPStream::ReadOneNaulFromVpu(NaluUnit &nalu)
{
	unsigned char *buf;
	int buf_size; 
	bool find = true;     
	dvr_enc_queue_get   data;
	// prepare to select(or poll)
	while(1)
	{
		  rec_fds.fd = enc_fd;      
		  rec_fds.revents = 0;
		  rec_fds.events = POLLIN_MAIN_BS;
		  
		  poll(&rec_fds, 1, 500);     
		  
		  if (!(rec_fds.revents & POLLIN_MAIN_BS)) 
			  continue;
	
		  // get dataout buffer   
		  int ret = ioctl(enc_fd, DVR_ENC_QUEUE_GET, &data);
		  if(ret < 0) 
		  {   
		  	  printf("DVR_ENC_QUEUE_GET failed\n");
			  continue;
		  }
	
		  buf = bs_buf + data.bs.offset;
		  buf_size = data.bs.length;
	
		  if( (buf[0] == 0) && (buf[1] == 0) && (buf[2] == 1) )
		  {
		  		nalu.data = &buf[4];
		  		nalu.type = buf[3]&0x1f;
		  } 
		  else if((buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0)  && (buf[3] == 1) )
		  {
		  		nalu.data = &buf[5];
		  		nalu.type = buf[4]&0x1f;
		  }
		  else
		  {
		  		printf("error naul \n");
		  	 	find = false;   
		  }
		  ret = ioctl(enc_fd, DVR_ENC_QUEUE_PUT, &data);   
		  if(ret < 0)
		  {
		  	 printf("warn: DVR_ENC_QUEUE_PUT failed\n");
		  }
		  break;
	}
	return find;
                
}  
bool CRTMPStream::SendH264File(const char *pFileName)  
{  
    if(pFileName == NULL)  
    {  
        return FALSE;  
    }  
    FILE *fp = fopen(pFileName, "rb");    
    if(!fp)    
    {    
        printf("ERROR:open file %s failed!",pFileName);  
    }    
    fseek(fp, 0, SEEK_SET);  
    m_nFileBufSize = fread(m_pFileBuf, sizeof(unsigned char), FILEBUFSIZE, fp);  
    if(m_nFileBufSize >= FILEBUFSIZE)  
    {  
        printf("warning : File size is larger than BUFSIZE\n");  
    }  
    fclose(fp);    
  
    RTMPMetadata metaData;  
    memset(&metaData,0,sizeof(RTMPMetadata));  
  
    NaluUnit naluUnit;  
    // 读取SPS帧  
    ReadOneNaluFromBuf(naluUnit);  
    metaData.nSpsLen = naluUnit.size;  
    memcpy(metaData.Sps,naluUnit.data,naluUnit.size);  
  
    // 读取PPS帧  
    ReadOneNaluFromBuf(naluUnit);  
    metaData.nPpsLen = naluUnit.size;  
    memcpy(metaData.Pps,naluUnit.data,naluUnit.size);  
  
    // 解码SPS,获取视频图像宽、高信息  
    int width = 0,height = 0;  
    h264_decode_sps(metaData.Sps,metaData.nSpsLen,width,height);  
    metaData.nWidth = width;  
    metaData.nHeight = height;  
    metaData.nFrameRate = 25;  
     
    // 发送MetaData  
    SendMetadata(&metaData);  
  
    unsigned int tick = 0;  
    while(ReadOneNaluFromBuf(naluUnit))  
    {  
        bool bKeyframe  = (naluUnit.type == 0x05) ? TRUE : FALSE;  
        // 发送H264数据帧  
        SendH264Packet(naluUnit.data,naluUnit.size,bKeyframe,tick);  
        msleep(40);  
        tick +=40;  
    }  
  
    return TRUE;  
}  
  
bool CRTMPStream::ReadOneNaluFromBuf(NaluUnit &nalu)
{
	int i = m_nCurPos;
	while( i < m_nFileBufSize  )
	{
		if(m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x01
			)
		{
			int pos = i;
			int num = 4;
			while (pos < m_nFileBufSize)
			{
				if(m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x00)
				{
					unsigned char c = m_pFileBuf[pos++];
					if(c == 0x1)
					{
						num = 3;
						break;
					}
					else if( (c == 0) && ( m_pFileBuf[pos++] == 0x01) )
					{
						num = 4;
						break;
					}
					
					
				}
			}
			if(pos == m_nFileBufSize)
			{
				nalu.size = pos-i;	
			}
			else
			{
				nalu.size = (pos-num)-i;
			}
			nalu.type = m_pFileBuf[i]&0x1f;
			nalu.data = &m_pFileBuf[i];

			m_nCurPos = pos-num;
			return TRUE;
		}
	}
	return FALSE;
}
