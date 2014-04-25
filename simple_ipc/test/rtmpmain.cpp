#include <stdio.h>    
#include "RTMPStream.h"    
    
int main(int argc,char* argv[])    
{    
    CRTMPStream rtmpSender;    
    
    bool bRet = rtmpSender.Connect("rtmp://192.168.1.104/live/test");    
    
    rtmpSender.SendH264File("E:\\video\\test.264");    
    
    rtmpSender.Close();    
} 
