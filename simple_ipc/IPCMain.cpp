#include <stdio.h>    
#include "RTMPStream.h"    
#include <signal.h>    
#define SERVER "rtmp://192.168.1.200/live/100"
bool quit = 0;
void handler(int signo)
{
     quit = 1;
}
int main(int argc,char* argv[])    
{    
    CRTMPStream rtmpSender;    
    
    signal(SIGINT, handler);
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
