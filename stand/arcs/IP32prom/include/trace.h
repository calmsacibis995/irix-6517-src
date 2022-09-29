#ifndef _trace_h_
#define _trace_h_


/* LED codes for trace */

#define DARK_LED  0
#define RED_LED   1
#define GREEN_LED 2
#define AMBER_LED 3


/*
 * Trace codes
 *
 * Trace codes are sent to led's, parallel port, or whatever to indicate
 * what sections of code the firmware and diagnostics are executing.  They
 * are mostly of value before the console is available.
 */

/* Serial loader trace codes */

#define  SL_RESET       1
#define  SL_CACHE_INV	2
#define  SL_PATTERNS	3
#define  SL_FIND_FW	4
#define  SL_LOAD_FW	5
#define  SL_CACHE_ERR	6
#define  SL_EXCEPTION   7


/* POST trace codes */

#define  POST1_ENTRY    10

#define  POST2_ENTRY    11

#define  POST3_ENTRY    12

/*
 * Define trace routines.
 *
 * Used by assy as well as C code
 */
#ifdef _LANGUAGE_ASSEMBLY

/* Primitive trace interface */
#define TRACE(x) li a0,TRACE_CODE(x,RED_LED); jal trace


#else
void sltrace(int code);		/* Primitive sloader trace rtn for debugging */
void trace(int code);		/* "Higher level" trace rtn */
void _errputs(char*);		/* Define here to avoid warnings */
#endif

#define TRACE_CODE(x,y) (x*4+y)

#endif
