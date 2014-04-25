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

#include "vcap_osd.h"

#define _GNU_SOURCE
#include <getopt.h>

void listitem()
{
    printf("<<Input OSD Mask function test>>\n");
    printf("1. Window(1-8) OSD Control\n");
    printf("2. Window(1-8) OSD Background Transparency)\n");
    printf("3. OSD Window Setting\n");
    printf("q. Exit\n");
    printf("Please input your choice>>");
}

int osd_mask(void)
{
    //int argc = 0;
    fiosdmask_win_t win;
    fiosd_palette_t palette;
    int index;
    //int ret, i = 0, i_rmchar;
    int ret, i = 0;
    fiosd_transparent_t tmp_transparent;
    //char tmp_string[64];
    //int _MAX_DISPLAY_LEN = 64;
    char fosd_dev_name[16];
    //fiosd_font_info_t finfo;
    //int option_index;
    int video_fd[8] = { 0 };
#if 0
    struct option longopts[] = {
        {"ndev", 1, NULL, 'i'},
        {0, 0, 0, 0}
    };
#endif
    for (i = 0; i < 4; i++) {
        snprintf(fosd_dev_name, 16, "/dev/fosd%d%d", i, 0);
        video_fd[i] = open(fosd_dev_name, O_RDWR);
        if (video_fd[i] < 0) {
            printf("fosd%d open fail!", i);
            exit(1);
        }

        palette.index = 0;
        palette.y = 120;
        palette.cb = 128;
        palette.cr = 128;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
        palette.index = 1;
        palette.y = 81;
        palette.cb = 90;
        palette.cr = 239;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
        palette.index = 2;
        palette.y = 210;
        palette.cb = 16;
        palette.cr = 146;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
        palette.index = 3;
        palette.y = 106;
        palette.cb = 202;
        palette.cr = 221;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
        palette.index = 4;
        palette.y = 169;
        palette.cb = 165;
        palette.cr = 16;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
        palette.index = 5;
        palette.y = 144;
        palette.cb = 53;
        palette.cr = 34;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
        palette.index = 6;
        palette.y = 40;
        palette.cb = 239;
        palette.cr = 109;
        ret = ioctl(video_fd[i], FIOSDS_PALTCOLOR, &palette);
        if (ret < 0) {
            printf("FIOSDS_PALTCOLOR %d Fail!", palette.index);
            goto end;
        }
    }

    while (1) {
        listitem();
        if (scanf("%d", &i) == 0) {
            scanf("%*[\n]");
            getchar();
            continue;
        }
        switch (i) {
        case '1':
          win_reentry:
            printf("Win(1-8):");
            scanf("%d", &index);
            if (index < 1 || index > 8) {
                printf("\nWindow index is out of range.!\n");
                goto win_reentry;
            }
            index--;
            printf("\n0-off, 1-on:");
            scanf("%d", &i);
            if (i) {
                ret = ioctl(video_fd[index], FIOSDMASKS_ON, &index);
            } else {
                ret = ioctl(video_fd[index], FIOSDMASKS_OFF, &index);
            }
            if (ret < 0) {
                printf("FIOSDS_[ON/OFF] Fail!");
                goto end;
            }
            break;
        case '2':
          win_reentry1:
            printf("Win(1-8):");
            scanf("%d", &index);
            if (index < 1 || index > 8) {
                printf("\nWindow index is out of range.!\n");
                goto win_reentry1;
            }
            index--;
            printf("\n0-0%%, 1-50%%, 2-75%%, 3-100%%:");
            scanf("%d", &i);
            if (i < 0)
                i = 0;
            if (i > 3)
                i = 3;
            tmp_transparent.windex = index;
            tmp_transparent.level = i;
            ret = ioctl(video_fd[index], FIOSDMASKS_TRANSPARENT, &tmp_transparent);
            if (ret < 0) {
                printf("FIOSDMASKS_TRANSPARENT Fail!\n");
                goto end;
            }
            break;
        case '3':
          win_reentry2:
            printf("Win(1-8):");
            scanf("%d", &index);
            if (index < 1 || index > 8) {
                printf("\nWindow index is out of range.!\n");
                goto win_reentry2;
            }
            index--;
            printf("x:");
            scanf("%d", &i);
            win.windex = index;
            win.x = i;
            printf("y:");
            scanf("%d", &i);
            win.y = i;
            printf("width:");
            scanf("%d", &i);
            win.width = i;
            printf("height:");
            scanf("%d", &i);
            win.height = i;
            printf("Color(0-7):");
            scanf("%d", &i);
            win.color = i;

            ret = ioctl(video_fd[index], FIOSDMASKS_WIN, &win);
            if (ret < 0) {
                printf("FIOSDMASKS_WIN Fail!");
                goto end;
            }
            break;
        case 'q':
        case 'Q':
            for (index = 0; index < 8; index++) {
                ret = ioctl(video_fd[index], FIOSDMASKS_OFF, &index);
            }
            goto end;
            break;

        default:
            break;
        }                       // end of switch
    }                           //end of while

  end:
    for (index = 0; index < 8; index++)
        close(video_fd[index]);
    return 0;
}
