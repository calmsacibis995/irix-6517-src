/* Global definitions for localedef */

#ifndef _LOCALEDEF_H_
#define _LOCALEDEF_H_

#include <stdio.h>
#include <locale.h>

/* Files for ATT processing */
extern FILE *att_file [];
extern FILE *iconv_spec_file;

/* Booleans indicating what has been defined */
extern int has_content [];

/* Name of the locale being created */
extern char * locale_name;

/* Escape character defined */
extern char escape_char;

/* Memory allocation error handling routine */
void exit_malloc_error ( );

/* Function to process the copy directive */
FILE * process_symlink_copy ( int category, const char * from );

/* Write to the chrtbl source file characters that are always
   automatically included in a character class */
void write_posix_ctype ( FILE * );

/* Messaging */
#include "localedef_msg.h"
#include "mkmsgs.h"
#include <nl_types.h>

/* Catalog id */
#include <nl_types.h>
extern nl_catd catid;

#define GETMSG( id ) ( CATGETS ( catid, id ))


/* Debug macros */
#ifdef YACC_DEBUG
#define YDBG_PRINTF(x)    printf x
#else
#define YDBG_PRINTF(x)    
#endif

#ifdef LEX_DEBUG
#define LDBG_PRINTF(x)    printf x
#else
#define LDBG_PRINTF(x)    
#endif


#endif /* _LOCALEDEF_H_ */
