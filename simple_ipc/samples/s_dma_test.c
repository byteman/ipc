/*
 * Special AHB DMA functions
 * Main code for "special_ahbdma". This is the special functions we want to use AHB DMA to do them.
 * We can set command "s_dma_test" to select items we want to run
 *
 * This file is released under the GM license.
 * Copyright (c) 2004-2012 Justin 
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include "special_ahbdma.h"

int dma_fd=-1;

void listitem() {
    printf("<<SPECIAL AHB DMA function test>>\n");
    printf("1. COPY SEQUENTIAL\n");
    printf("2. FILL 2D\n");
    printf("3. COPY 2D\n");		
    printf("4. Exit\n");
    printf("Please input your choice>>");
}

int main(int argc, char *argv[])
{
    ahb_trans_t ahb_trans = {};
    unsigned int *src_va_addr, *tmp;
    unsigned int screensize, size, data = 0x12345678;
    unsigned int pitch = 720, width = 640, height = 480, pixel_byte = 4;
    int ret = -1, i = 0;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo vinfo;
    
    FILE *pFile=NULL;
    
    pFile = open("/dev/fb0", O_RDWR);
    
    if(pFile < 0)
    {
        printf("open fb failed!\n");
        exit(1);
    }
    
    if(ioctl(pFile,FBIOGET_FSCREENINFO,&fix) != 0)
    {
        printf("Error FBIOGET_FSCREENINFO\n");
        exit(1);
    }
    
    if(ioctl(pFile, FBIOGET_VSCREENINFO, &vinfo) != 0)
    {
        printf("Error FBIOGET_VSCREENINFO\n");
        exit(1);
    } 
    
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8; 
    src_va_addr = (char*)mmap(0,screensize,PROT_READ|PROT_WRITE,MAP_SHARED, pFile, 0);
    printf("win x = %d, y = %d, pixel bit = %d, size = 0x%x\n",vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, screensize);

  
    dma_fd = open("/dev/s_dma", O_RDWR);
    if(dma_fd<0) {
      printf("fosd1 open fail!");
      exit(1);
    }

    tmp = src_va_addr;
    size = width * height * pixel_byte;
    for(i = 0; i < size/4; i++, tmp ++, data++)
      memcpy(tmp, &data, 4);   
    //printf("Alloc size = %d, addr = 0x%x\n",size,src_va_addr);
    //while(1);
        
    while(1)
    {
      listitem();
      if (scanf("%d",&i)==0) {
      	scanf("%*[\n]");
      	getchar();
      	continue;
      }
      switch (i) {
          case 1 :
              ahb_trans.src_va_addr = src_va_addr;
              ahb_trans.dst_pa_addr	= (unsigned int *)(fix.smem_start + size);
              ahb_trans.size = size;    			
              ahb_trans.no_wait = 0;	
              
              printf("dst phy = 0x%x\n",(unsigned int)ahb_trans.dst_pa_addr);
    			
              ret = ioctl(dma_fd, FSDMA_COPY_SEQUENTIAL, &ahb_trans);
              if(ret < 0) {
                 printf("FSDMA_COPY_SEQUENTIAL Fail!\n");
                 goto end;
              }					
              break;
          case 2 :
              ahb_trans.dst_pa_addr = (unsigned int *)fix.smem_start;
              ahb_trans.dst_pitch = pitch;
              ahb_trans.width = width;
              ahb_trans.height = height;
              ahb_trans.pixel_byte = pixel_byte;
              ahb_trans.pattern = 0x23456789;
              ahb_trans.no_wait = 0;		
              
              printf("dst phy = 0x%x\n",(unsigned int)ahb_trans.dst_pa_addr);		
          
              ret = ioctl(dma_fd, FSDMA_FILL_2D, &ahb_trans);
              if(ret < 0) {
                printf("FSDMA_FILL_2D Fail!\n");
                goto end;
              }					
              break;
          case 3 :
              ahb_trans.src_pa_addr = (unsigned int *)fix.smem_start;
              ahb_trans.dst_pa_addr = (unsigned int *)(fix.smem_start + size);
              ahb_trans.src_pitch = width;
              ahb_trans.dst_pitch = pitch;
              ahb_trans.width = width;
              ahb_trans.height = height;
              ahb_trans.pixel_byte = pixel_byte;	
              ahb_trans.llp_mode = 0;		
              ahb_trans.no_wait = 0;
			    
			        printf("src phy = 0x%x, dst = 0x%x\n",(unsigned int)ahb_trans.src_pa_addr,(unsigned int)ahb_trans.dst_pa_addr);			
              	
              ret = ioctl(dma_fd, FSDMA_COPY_2D, &ahb_trans);
              if(ret < 0) {
                printf("FSDMA_COPY_2D Fail!\n");
                goto end;
              }					
              break;			    
          case 4: 
              close(dma_fd); 
              exit(0);
              break;
          default:
          		break;
          					 
      }	// end of switch
    }	//end of while								
 end:	
    munmap(src_va_addr, screensize);
    close(dma_fd); 
    close(pFile);
    return 0; 
}
