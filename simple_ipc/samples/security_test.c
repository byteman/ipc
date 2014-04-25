/*
 * Security Tools
 * Main code for "security_test". This is the generic tool for most
 * manipulations...
 * We can set command "security_test -h" to know how to use it
 *
 * This file is released under the GM license.
 *     Copyright (c) 2004-2012 Justin 
 */

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
#include "security.h"

#define DEBUG

/************************* MISC SUBROUTINES **************************/

/*------------------------------------------------------------------*/
/*
 * Print usage string
 */
static void es_usage(void)
{
    printf("Usage:\n");
    printf("security_test -h\n");    
    printf("security_test -k\n");
    printf("security_test -e\n");
    printf("security_test -d\n");
    printf("security_test -E\n");
    printf("security_test -D\n");
    printf("\n");
    printf("-k	get random key and initial vector\n");
    printf("-e	set algorithm,mode,argument to run encrypt function\n");
    printf("-d	set algorithm,mode,argument to run decrypt function\n");
    printf("-E	set algorithm,mode,argument, generate random key and IV to encrypt data\n");
    printf("-D	set algorithm,mode,argument, generate random key and IV to decrypt data\n");
    printf("\n");
}

/******************************* MAIN ********************************/
int main(int argc, char **argv)
{
    esreq wrq;
    int descript = -1, i = 0, j = 0, *tmp_addr;
    int test_length = 0, IV_normal_length = 0;

    /* just for ioctl()ling it */
    descript = open("/dev/security", O_RDWR);
    if (descript < 0) {
        printf("%s: open device fail\n", argv[0]);
        exit(1);
    }

    /* No argument : show the driver name + info */
    if (argc == 1) {
        printf("GM Security driver\n");
        exit(0);
    }

    /* Special case for help... */
    if ((!strncmp(argv[1], "-h", 9)) || (!strcmp(argv[1], "--help"))) {
        es_usage();
        exit(0);
    }

    /* Check argument number */
    if (!(argc == 2)) {
        printf("Command argument number error\n");
        exit(0);
    }

    tmp_addr = malloc(4096);
    if (tmp_addr == NULL)
        printf("Malloc DataIn buffer failed\n");
    wrq.DataIn_addr = (unsigned int)tmp_addr;        

    tmp_addr = malloc(4096);
    if (tmp_addr == NULL)
        printf("Malloc DataOut buffer failed\n");
    wrq.DataOut_addr = (unsigned int)tmp_addr;     
     
		printf("Data In virtual addr = 0x%x, out = 0x%x\n",wrq.DataIn_addr, wrq.DataOut_addr);

    tmp_addr = malloc(32);
    if (tmp_addr == NULL)
        printf("Malloc Key_backup buffer failed\n");
    wrq.Key_backup = (unsigned int)tmp_addr;
    
    tmp_addr = malloc(16);
    if (tmp_addr == NULL)
        printf("Malloc IV_backup buffer failed\n");
    wrq.IV_backup = (unsigned int)tmp_addr;
	
//==========================================
//command: security_test -k 
//==========================================
  
    wrq.algorithm = Algorithm_AES_256;        
    wrq.mode = CBC_mode;
    
    /* data need 8 times => Algorithm_DES / Algorithm_Triple_DES
    * data need 16 times => Algorithm_AES_128
    * data need 24 times => Algorithm_AES_192
    * data need 32 times => Algorithm_AES_256    
    */     
    test_length = 4064;//AES 256 data need 32 times, 127 x 32 = 4064
    IV_normal_length = 16;
    
    if ((argc == 2) && (!strcmp(argv[1], "-k"))) {
        if (ioctl(descript, ES_GETKEY, &wrq) < 0)
            printf("get random key ioctl fail\n");

        printf("get random key length = %d, IV length = %d\n", wrq.key_length, wrq.IV_length);
        goto just_gen;
    } else {
/* ========================== test pattern ===========================================*/    	
        if ((argc == 2) && ((!strcmp(argv[1], "-e")) || (!strcmp(argv[1], "-E")))) {
      	
            *(unsigned int *)(wrq.DataIn_addr)      = 0xe2bec16b;
            *(unsigned int *)(wrq.DataIn_addr + 4)  = 0x969f402e;
            *(unsigned int *)(wrq.DataIn_addr + 8)  = 0x117e3de9;
            *(unsigned int *)(wrq.DataIn_addr + 12) = 0x2a179373;
            *(unsigned int *)(wrq.DataIn_addr + 16) = 0x578a2dae;
            *(unsigned int *)(wrq.DataIn_addr + 20) = 0x9cac031e;
            *(unsigned int *)(wrq.DataIn_addr + 24) = 0xac6fb79e;
            *(unsigned int *)(wrq.DataIn_addr + 28) = 0x518eaf45;

            *(unsigned int *)(wrq.DataIn_addr + 32) = 0x461cc830;
            *(unsigned int *)(wrq.DataIn_addr + 36) = 0x11e45ca3;
            *(unsigned int *)(wrq.DataIn_addr + 40) = 0x19c1fbe5;
            *(unsigned int *)(wrq.DataIn_addr + 44) = 0xef520a1a;
            *(unsigned int *)(wrq.DataIn_addr + 48) = 0x45249ff6;
            *(unsigned int *)(wrq.DataIn_addr + 52) = 0x179b4fdf;
            *(unsigned int *)(wrq.DataIn_addr + 56) = 0x7b412bad;
            *(unsigned int *)(wrq.DataIn_addr + 60) = 0x10376ce6;
        }
        if ((argc == 2) && ((!strcmp(argv[1], "-d")) || (!strcmp(argv[1], "-D")))) {
            *(unsigned int *)(wrq.DataIn_addr)      = 0x044c8cf5;
            *(unsigned int *)(wrq.DataIn_addr + 4)  = 0xbaf1e5d6;
            *(unsigned int *)(wrq.DataIn_addr + 8)  = 0xfbab9e77;
            *(unsigned int *)(wrq.DataIn_addr + 12) = 0xd6fb7b5f;
            *(unsigned int *)(wrq.DataIn_addr + 16) = 0x964efc9c;
            *(unsigned int *)(wrq.DataIn_addr + 20) = 0x8d80db7e;
            *(unsigned int *)(wrq.DataIn_addr + 24) = 0x7b779f67;
            *(unsigned int *)(wrq.DataIn_addr + 28) = 0x7d2c70c6;

            *(unsigned int *)(wrq.DataIn_addr + 32) = 0x6933f239;
            *(unsigned int *)(wrq.DataIn_addr + 36) = 0xcfbad9a9;
            *(unsigned int *)(wrq.DataIn_addr + 40) = 0x63e230a5;
            *(unsigned int *)(wrq.DataIn_addr + 44) = 0x61142304;
            *(unsigned int *)(wrq.DataIn_addr + 48) = 0xe205ebb2;
            *(unsigned int *)(wrq.DataIn_addr + 52) = 0xfce99bc3;
            *(unsigned int *)(wrq.DataIn_addr + 56) = 0x07196cda;
            *(unsigned int *)(wrq.DataIn_addr + 60) = 0x1b9d6a8c;
        }

        wrq.key_addr[0] = 0x603deb10;
        wrq.key_addr[1] = 0x15ca71be;
        wrq.key_addr[2] = 0x2b73aef0;
        wrq.key_addr[3] = 0x857d7781;
        wrq.key_addr[4] = 0x1f352c07;
        wrq.key_addr[5] = 0x3b6108d7;
        wrq.key_addr[6] = 0x2d9810a3;
        wrq.key_addr[7] = 0x0914dff4;
                    
        wrq.IV_addr[0] = 0x00010203;
        wrq.IV_addr[1] = 0x04050607;
        wrq.IV_addr[2] = 0x08090a0b;
        wrq.IV_addr[3] = 0x0c0d0e0f;

        //==============================================================================
        //command: security_test -e
        //==============================================================================
        if ((argc == 2) && (!strcmp(argv[1], "-e"))) {
            wrq.data_length = test_length + IV_normal_length;

            if (ioctl(descript, ES_ENCRYPT, &wrq) < 0)
                printf("set key and get encrypt data ioctl fail\n");

            goto just_set;
        }
        //==============================================================================
        //command: security_test -d 
        //==============================================================================
        else if ((argc == 2) && (!strcmp(argv[1], "-d"))) {        	
            wrq.data_length = test_length + IV_normal_length;
            
            if (ioctl(descript, ES_DECRYPT, &wrq) < 0)
                printf("set key and get decrypt data ioctl fail\n");

            goto just_set;
        }
        //==============================================================================
        //command: security_test -E 
        //==============================================================================
        else if ((argc == 2) && (!strcmp(argv[1], "-E"))) {        	
            wrq.data_length = test_length + IV_normal_length;
            
            if (ioctl(descript, ES_AUTO_ENCRYPT, &wrq) < 0)
                printf("build random key and get encrypt data ioctl fail\n");

            goto just_set;
        }
        //==============================================================================
        //command: security_test -D
        //==============================================================================
        else if ((argc == 2) && (!strcmp(argv[1], "-D"))) {
            wrq.data_length = test_length + IV_normal_length;
            
            if (ioctl(descript, ES_AUTO_DECRYPT, &wrq) < 0)
                printf("build random key and get decrypt data ioctl fail\n");

            goto just_set;
            
        } else {
            printf("Command Error\n");
            return 0;
        }
    }

  just_set:
       
    printf("AP DataIn  =\n");
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++)
            printf("0x%08x ", *(unsigned int *)(wrq.DataIn_addr + (i * 4 + j) * 4));
        printf("\n");
    }

    printf("\nAP DataOut =\n");
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++)
            printf("0x%08x ", *(unsigned int *)(wrq.DataOut_addr + (i * 4 + j) * 4));
        printf("\n");
    }
    
  just_gen:
    printf("\nAP keybuf  =\n");
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++)
            printf("0x%08x ", *(unsigned int *)(wrq.Key_backup + (i * 4 + j) * 4));
        printf("\n");
    }
    
    printf("\nAP ivbuf  = ");
    for (i = 0; i < 4; i++)
        printf("0x%08x ", *(unsigned int *)(wrq.IV_backup + i * 4));   
    printf("\n");     
/*
    printf("\nAP key_out_buf  =\n");
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++)
            printf("0x%08x, ", wrq.key_addr[i * 4 + j]);
        printf("\n");
    }

    printf("\nAP iv_out_buf  = ");
    for (i = 0; i < 4; i++)
        printf("0x%08x, ", wrq.IV_addr[i]);
    printf("\n");
*/
    		
    return 0;
}
