#include <stdio.h>    
#include "RTMPStream.h"    
    
#define SERVER "rtmp://192.168.1.200/live/100"
int main(int argc,char* argv[])    
{    
    CRTMPStream rtmpSender;    
    printf("connect to %s\n",SERVER);
    if(!rtmpSender.Connect(SERVER))
    {
    	printf("failed\n");
    	return -1;
    }    
    printf("ok\n");
    
    //rtmpSender.SendH264File("E:\\video\\test.264");    
    rtmpSender.SendVideo();    
    rtmpSender.Close();    
} 
