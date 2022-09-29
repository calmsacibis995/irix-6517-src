/*
 * $Id: malloc-audit.h,v 1.3 1997/01/27 00:02:03 chatz Exp $
 *
 * Define the MALLOC AUDIT wrappers.
 */

#ifdef MALLOC_AUDIT
#ifndef MALLOC_AUDIT_ON
void *_malloc_(size_t size, char *file, int line);
void _free_(void *ptr, char *file, int line);
void *_realloc_(void *ptr, size_t size, char *file, int line);
void *_calloc_(size_t nelem, size_t elsize, char *file, int line);
void *_memalign_(size_t alignment, size_t size, char *file, int line);
void *_valloc_(size_t size, char *file, int line);
void *_valloc_(size_t size, char *file, int line);
char *_strdup_(char *s1, char *file, int line);
void _malloc_reset_(void);
void _malloc_audit_(void);
void _malloc_error_(void);
void _malloc_persistent_(void *ptr, char *file, int line);
#define malloc(x) _malloc_(x, __FILE__, __LINE__)
#define free(x) _free_(x, __FILE__, __LINE__)
#define realloc(x, y) _realloc_(x, y, __FILE__, __LINE__)
#define calloc(x, y) _calloc_(x, y, __FILE__, __LINE__)
#define memalign(x, y) _memalign_(x, y, __FILE__, __LINE__)
#define valloc(x) _valloc_(x, __FILE__, __LINE__)
#define strdup(x) _strdup_(x, __FILE__, __LINE__)
#define _persistent_(x) _malloc_persistent_(x, __FILE__, __LINE__)
#define MALLOC_AUDIT_ON
#endif
/* checks for MALLOC_AUDIT */
#define MALLOC_AUDIT_KEY \
int	_malloc_audit_key = 1;
#define MALLOC_AUDIT_CHECK \
{ extern _malloc_audit_key;\
  if (_malloc_audit_key != 1) {\
    fprintf(stderr, "Error at [%s:%d], -DMALLOC_AUDIT for library, but not for main\n", __FILE__, __LINE__);\
    exit(1);\
    /*NOTREACHED*/\
  }\
}
#else	/* checks for NO MALLOC_AUDIT */
#define MALLOC_AUDIT_KEY \
int	_malloc_audit_key = 0;
#define MALLOC_AUDIT_CHECK \
{ extern _malloc_audit_key;\
  if (_malloc_audit_key != 0) {\
    fprintf(stderr, "Error at [%s:%d], -DMALLOC_AUDIT for main, but not for library\n", __FILE__, __LINE__);\
    exit(1);\
    /*NOTREACHED*/\
  }\
}
#endif
