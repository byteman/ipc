
#ifndef  __FILE_CFG_H__
#define  __FILE_CFG_H__ 
  

/* 
 * Set line length in configuration files   
 */
 
#define  FALSE		0
#define  TRUE		1
#define  LINE_LEN	128 //64
#define  RESET		-9999
#define  CHAR_0		'0'
#define  CHAR_9		'9'
#define  CHAR_SPACE	' '
#define  CHAR_NEWLINE	'\n'
#define  CHAR_CR	'\n'
#define  CHAR_RET	'\r'
#define  CHAR_NULL	'\0'
#define  CHAR_TAB	'\t'
#define  CHAR_EQUAL	'='
#define  CHAR_EQUAL	'='
#define  CHAR_LSQR	'['
#define  CHAR_RSQR	']'
#define  CHAR_COMMENT1	'#'
#define  CHAR_COMMENT2	'/'
#define  STR_COMMENT2	"//"
  
/* 
 * Define return error code value 
 */ 
#define ERR_NONE 0       /* read configuration file successfully */ 
#define ERR_NOFILE 2     /* not find or open configuration file */ 
#define ERR_READFILE 3   /* error occur in reading configuration file */ 
#define ERR_FORMAT 4     /* invalid format in configuration file */ 
#define ERR_NOTHING 5     /* not find section or key name in configuration file */ 

/* 
 * Utilitiy macros
 */ 
#define  is_digit(c)		( ((c) >= CHAR_0) && ((c) <= CHAR_9) )
#define  is_not_digit(c)	( ((c) < CHAR_0) || ((c) > CHAR_9) )

/* 
 *   File structure and operations
 */ 
typedef  struct file_s  {
    FILE *fd;
    int	offset;
//    mm_segment_t  fs ; 
}  fcfg_file_t ;

#define  fcfg_reset(f)	do { fcfg_lseek((f)->fd,0,0); (f)->offset = 0; } while(0)
extern fcfg_file_t *fcfg_open( const char*, const char* );
extern void fcfg_close(fcfg_file_t *f);

#define  fcfg_StrToLong	strtol
#define  fcfg_StrToULong strtoul
#define  SEE		printf
//#define  file_close(f)	do { sys_close(f->fd); set_fs(f->fs); kfree(f); } while(0)
#define  fcfg_lseek	fseek
#define  fcfg_read(fd,line,len)	fread(line,len,1,fd)
#define  fcfg_write	
#define  fcfg_alloc_mem(s)	malloc((s))

/* 
 * Read the value of key name in string form 
 */ 
/*
EXT char *getconfigstr(const char* section,		// points to section name
			const char* keyname,	// points to key name
			file_t *cfile);		// points to configuration file
*/
/* 
 * Read the value of key name in integer form 
 */ 
extern int fcfg_getint(const char* section,	// points to section name
			const char* keyname,	// points to key name
			int* keyvalue,		// points to destination address
			fcfg_file_t *cfile);		// points to configuration file
extern int fcfg_check_section(const char *section, fcfg_file_t *cfile);
#endif  // __FILE_CFG_H__



