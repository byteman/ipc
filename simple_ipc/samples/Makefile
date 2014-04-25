LINUX_SRC=/usr/src/arm-linux-2.6.28/linux-2.6.28-fa
LINUX_INC=$(LINUX_SRC)/include

CROSS_COMPILE=arm-none-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
CPP=$(CC) -E
LD=$(CROSS_COMPILE)ld
AS=$(CROSS_COMPILE)as
MAKE=make
SAMPLE1=liveview
SAMPLE2=record
SAMPLE3=subrecord
SAMPLE4=update
SAMPLE5=snapshot
SAMPLE6=motion
SAMPLE7=mpeg4
SAMPLE8=mjpeg
SAMPLE9=isp-motion
SAMPLE10=update-bitrate
SAMPLE11=cap_osd_string
SAMPLE12=cap_osd_mask
SAMPLE13=capture_raw
SAMPLE14=roi
SAMPLE15=playback
SAMPLE16=2chliveview
SAMPLE17=2CH_PB
SAMPLE18=pip
OUTPUT_DIR=../release
LIB=../lib

HFLAGS = -O3 -Wall -I/$(LINUX_SRC)/../module/include/dvr
HFLAGS +=-I/usr/src/arm-linux-2.6.28/module/include/vcap200_v3
HFLAGS +=-I/usr/src/arm-linux-2.6.28/module/include/security
HFLAGS +=-I/usr/src/arm-linux-2.6.28/module/include/ahb_dma
HFLAGS += -I../inc
HFLAGS +=-I/usr/src/arm-linux-2.6.28/module/include/isp
TARGETS := $(LIB) $(SAMPLE1) $(SAMPLE2) $(SAMPLE3) $(SAMPLE4) $(SAMPLE5) $(SAMPLE6) $(SAMPLE7) $(SAMPLE8) $(SAMPLE9) $(SAMPLE10) $(SAMPLE11) $(SAMPLE12) $(SAMPLE13) $(SAMPLE14) $(SAMPLE15) $(SAMPLE16) $(SAMPLE17) $(SAMPLE18)

.PHONY: $(TARGETS)

all: $(TARGETS)

$(LIB) :
	$(MAKE) -C $(LIB)
	
$(SAMPLE1):
	$(CC) $(HFLAGS) liveview.c -o $(SAMPLE1)
	/bin/cp $(SAMPLE1) $(OUTPUT_DIR)

$(SAMPLE2):
	$(CC) $(HFLAGS) main-bitstream-record.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE2)
	/bin/cp $(SAMPLE2) $(OUTPUT_DIR)

$(SAMPLE3):
	$(CC) $(HFLAGS) sub-bitstream-record.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE3)
	/bin/cp $(SAMPLE3) $(OUTPUT_DIR)

$(SAMPLE4):
	$(CC) $(HFLAGS) update-record-setting.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE4)
	/bin/cp $(SAMPLE4) $(OUTPUT_DIR)

$(SAMPLE5):
	$(CC) $(HFLAGS) snapshot.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE5)
	/bin/cp $(SAMPLE5) $(OUTPUT_DIR)

$(SAMPLE6):
	$(CC) $(HFLAGS) motion-detection.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE6)
	/bin/cp $(SAMPLE6) $(OUTPUT_DIR)

$(SAMPLE7):
	$(CC) $(HFLAGS) mpeg4-record.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE7)
	/bin/cp $(SAMPLE7) $(OUTPUT_DIR)

$(SAMPLE8):
	$(CC) $(HFLAGS) mjpeg-record.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE8)
	/bin/cp $(SAMPLE8) $(OUTPUT_DIR)

$(SAMPLE9):
	$(CC) $(HFLAGS) isp-motion-detection.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE9)
	/bin/cp $(SAMPLE9) $(OUTPUT_DIR)

$(SAMPLE10):
	$(CC) $(HFLAGS) update-bitrate.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE10)
	/bin/cp $(SAMPLE10) $(OUTPUT_DIR)

$(SAMPLE11):
	$(CC) $(HFLAGS) cap_osd_string.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE11)
	/bin/cp $(SAMPLE11) $(OUTPUT_DIR)

$(SAMPLE12):
	$(CC) $(HFLAGS) cap_osd_mask.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE12)
	/bin/cp $(SAMPLE12) $(OUTPUT_DIR)
		
$(SAMPLE13):
	$(CC) $(HFLAGS) capture_raw.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE13)
	/bin/cp $(SAMPLE13) $(OUTPUT_DIR)

$(SAMPLE14):
	$(CC) $(HFLAGS) roi.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE14)
	/bin/cp $(SAMPLE14) $(OUTPUT_DIR)

$(SAMPLE15):
	$(CC) $(HFLAGS) playback.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE15)
	/bin/cp $(SAMPLE15) $(OUTPUT_DIR)

$(SAMPLE16):
	$(CC) $(HFLAGS) 2ch_liveview.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE16)
	/bin/cp $(SAMPLE16) $(OUTPUT_DIR)

$(SAMPLE17):
	$(CC) $(HFLAGS) 2ch_playback.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE17)
	/bin/cp $(SAMPLE17) $(OUTPUT_DIR)

$(SAMPLE18):
	$(CC) $(HFLAGS) pip.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE18)
	/bin/cp $(SAMPLE18) $(OUTPUT_DIR)

clean:
	rm -f  *.o $(SAMPLE1) $(SAMPLE2) $(SAMPLE3) $(SAMPLE4) $(SAMPLE5) $(SAMPLE6) $(SAMPLE7) $(SAMPLE8) $(SAMPLE9) $(SAMPLE10) $(SAMPLE11) $(SAMPLE12) $(SAMPLE13) $(SAMPLE14) $(SAMPLE15) $(SAMPLE16) $(SAMPLE17) $(SAMPLE18)
	