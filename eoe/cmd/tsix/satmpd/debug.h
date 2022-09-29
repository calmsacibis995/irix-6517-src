#ifndef DEBUG_HEADER
#define DEBUG_HEADER

#ident "$Revision: 1.1 $"

#ifdef DEBUG_IMPL
unsigned int debug_state = 0;
FILE *debug_fp = stderr;
#else
extern unsigned int debug_state;
extern FILE *debug_fp;
#endif

/* debug states */
#define DEBUG_STARTUP		0x0001
#define DEBUG_FILE_OPEN		0x0002
#define DEBUG_DIR_OPEN		0x0004
#define DEBUG_OPEN_FAIL		0x0008
#define DEBUG_OPENDIR_FAIL	0x0010
#define DEBUG_PROTOCOL		0x0020

#define DEBUG_ALL_OFF		0x0000
#define DEBUG_ALL_ON		0xFFFF

/* debug state macros */
#define set_debug_state(state)	debug_state = (state)
#define add_debug_state(state)	debug_state |= (state)
#define del_debug_state(state)	debug_state &= ~(state)
#define is_debug_state(state)	debug_state == (state)
#define debug_on(state)		((state) & debug_state) == (state)

/* debug functions */
int   parse_debug_opts (const char *);
char *name_debug_opts (void);
void  debug_set_log (const char *);
void  debug_print (const char *, ...);

#endif /* DEBUG_HEADER */
