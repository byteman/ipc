#ifndef SPS_DECODE_H
#define SPS_DECODE_H

bool h264_decode_sps(unsigned char * buf,unsigned int nLen,int &width,int &height);
#endif
