#define __UTIL_CFG_C__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file_cfg.h"

inline fcfg_file_t *fcfg_open(const char *name, const char *flag)
{
    fcfg_file_t *cfile;
    cfile = (fcfg_file_t *) fcfg_alloc_mem(sizeof(fcfg_file_t));
    if (cfile == NULL) {
        printf("%s:%d <alloc memory fail!>\n", __FUNCTION__, __LINE__);
        return (NULL);
    }

    cfile->offset = 0;
    cfile->fd = fopen(name, flag);
    if (cfile->fd == NULL) {
        printf("%s:%d <open %s file fail!>\n", __FUNCTION__, __LINE__, name);
        return (NULL);
    }
    return (cfile);
}

void fcfg_close(fcfg_file_t * f)
{
    do {
        fclose(f->fd);
        free(f);
    } while (0);
}

int fcfg_is_delimeter(const char c)
{
    switch (c) {
        case CHAR_RET:
        case CHAR_NULL:
        case CHAR_COMMENT2:
        case CHAR_COMMENT1:
        case CHAR_NEWLINE:
        case CHAR_TAB:
        case CHAR_EQUAL:
        case CHAR_SPACE:
            return (TRUE);
    }
    return (FALSE);
}

/*  Get a line from a file
 *	Limition : There must be a newline charactor within line length 'size'
 */

static int fcfg_readline(fcfg_file_t * cfile, char *line, const int size)
{
    int n;
    char *p1, *p2;
    if (fcfg_read(cfile->fd, line, size) < 1) {
        return (-1);            // End of file
    }
    //n = (int) line + size ;
    if (NULL != (p1 = strchr(line, CHAR_NEWLINE))) {
        *p1 = CHAR_NULL;        // replace newline with NULL char
    } else {
        p1 = line + size;       // newline char not found
    }

    if (NULL != (p2 = strchr(line, CHAR_RET))) {
        *p2 = CHAR_NULL;        // replace the 'return' char with NULL
    } else {
        p2 = line + size;       // 'return' char not found
    }

    if (((line + size) == p1) && ((line + size) == p2)) {
        return (-1);            // neither 'newline' nor 'return' char found
    }
    n = (int)((p1 < p2) ? p1 : p2) - (int)line; // calculate length of the line
    cfile->offset += (n + 1);   // record & move current
    fcfg_lseek(cfile->fd, cfile->offset, 0);    //  read/write pointer in file
    /**  discard comments  **/
    if (NULL != (p1 = strchr(line, CHAR_COMMENT1)))
        *p1 = CHAR_NULL;        // discard words behind '#'
    if (NULL != (p1 = strstr(line, STR_COMMENT2)))
        *p1 = CHAR_NULL;        // discard words behind '//'
//    SEE("%s() line=[%s]\n", __func__,line) ;
    return (n);
}

int fcfg_find_section(const char *section, fcfg_file_t * cfile)
{
    char *s1, *s2;
    char line[LINE_LEN];

    while (fcfg_readline(cfile, line, LINE_LEN) >= 0) {
        if (NULL == (s1 = strchr(line, CHAR_LSQR))) {
            continue;           // '[' not found
        }
        if (NULL != (s2 = strchr(line, CHAR_RSQR))) {
            *s2 = CHAR_NULL;    // ']' found, replace by NULL char
        }
        if (strcmp(++s1, section)) {    // compare the section name
            continue;           // section name not match
        }
        return (TRUE);          // section name matched
    }
    return (FALSE);    // specified section name not found
}

char *fcfg_find_keystring(const char *keyname, fcfg_file_t * cfile, char *line)
{
    char *s1, *s2;
    int keylen = strlen(keyname);

    while (fcfg_readline(cfile, line, LINE_LEN) >= 0) {

        if (strchr(line, CHAR_LSQR)) {
            if (strchr(line, CHAR_RSQR)) {
                return ((char *)NULL);
            }
        }
        //  search the keyname in this line
        if (NULL == (s1 = strstr(line, keyname))) {
            continue;           // keyname is not a substring of this line
        }
        //  determine the end of the candidate keyname (s1)
        if (!fcfg_is_delimeter(s1[keylen])) {
            continue;           // s1 is a super set of keyname, not exactly match.
        }
        //  keyname matched, looking for the equal sigh '=' after keyname
        if (NULL != (s2 = strchr(s1 + keylen, CHAR_EQUAL))) {
            for (++s2; is_not_digit(*s2); ++s2) ;       // skip non-digit charactors next to '='.
        }
        return (s2);            // return the value string
    }
    return ((char *)NULL);
}

int fcfg_check_section(const char *section, fcfg_file_t *cfile)
{
    int ret;
    fcfg_reset(cfile);
    ret = fcfg_find_section(section, cfile);
    return ret;
}

int fcfg_getint(const char *section, const char *keyname, int *keyvalue, fcfg_file_t * cfile)
{
    char *keystr;
    char line[LINE_LEN];

    fcfg_reset(cfile);
    if (!fcfg_find_section(section, cfile)) {
        goto no_section_exit;            // section not found
    }

    if (NULL == (keystr = fcfg_find_keystring(keyname, cfile, line))) {
        goto err_exit;            // keyname not found
    }

    *keyvalue = fcfg_StrToLong(keystr, NULL, 0);     // convert the string of key value

    return (1);
no_section_exit:    
    return (0);
err_exit: 
    return (-1);
}
