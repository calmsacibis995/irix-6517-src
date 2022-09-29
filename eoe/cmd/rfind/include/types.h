#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>

typedef u_int32_t u_intgen_t;
typedef int32_t intgen_t;

/* darn rpc.h and jdm.c's types.h both define bool_t,
 * one as #define to int, the other as typedef unsigned int32.
 */
#ifndef bool_t
#define bool_t int
#endif

/* darn rpc.h also defines TRUE, FALSE ... */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define BOOL_TRUE	1
#define BOOL_FALSE	0
#define BOOL_UNKNOWN	( -1 )
#define BOOL_ERROR	( -2 )

#endif /* TYPES_H */
