/*      @(#)util.h	1.1 88/03/07 4.0NFSSRC SMI   */

#define EOS '\0'

#ifndef NULL 
#	define NULL ((char *) 0)
#endif


#define MALLOC(object_type) ((object_type *) calloc(1,sizeof(object_type)))

#define FREE(ptr)	(0!=(ptr) ? free((char *) ptr) : 0)

#define STRCPY(dst,src) \
	(dst = malloc((unsigned)strlen(src)+1), (void) strcpy(dst,src))

#define STRNCPY(dst,src,num) \
	(dst = malloc((unsigned)(num) + 1),\
	(void)strncpy(dst,src,num),(dst)[num] = EOS) 

extern char *malloc();
extern char *alloca();

char *getline();
void fatal();


