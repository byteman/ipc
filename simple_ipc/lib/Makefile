INCLUDE_PATH = -I../inc/
COPS = $(INCLUDE_PATH) -D ARM_ASM -O3 -finline-functions -march=armv4 -mtune=arm9tdmi -msoft-float
CROSS_COMPILE=arm-none-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
AS=$(CROSS_COMPILE)as
AR=$(CROSS_COMPILE)ar

CSRC = gmavi.c favc_motiondet.c fv_motiondet.c file_cfg.c
SLSRC = 

COBJS = ${CSRC:%.c=%.o}
SOBJS = ${SLSRC:%.s=%.o}
LOPTS = -g

TARGET = libipc.a

$(TARGET) : $(COBJS) $(SOBJS) 
	$(AR) -r libipc.a *.o
	#cp libipc.a ../

%.o: %.c
	$(CC) -c $(COPS) -I. -o $@ $<

%.o : %.s
	$(CC) -c $(COPS) -I. -o $@ $<

clean :
	/bin/rm -f *.o
	/bin/rm -f libipc.a

