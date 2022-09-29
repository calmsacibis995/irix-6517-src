#ifndef _SPYCMD_H
#define _SPYCMD_H

#ifdef __cplusplus
extern "C" {
#endif
#include "spyBase.h"
#include <sys/types.h>


/* Spy commands
 *
 * Primitive support for building command driven spies.
 *
 */

typedef int (*scmdFunc_t)(spyProc_t*, char*, char**);

typedef struct spySubCmd {
	char*		ssc_name;
	char*		ssc_desc;
	scmdFunc_t	ssc_cmd;
} spySubCmd_t;


typedef struct spyCmdList {
	char*			scl_name;
	char*			scl_desc;
	spySubCmd_t*		scl_cmds;
	struct spyCmdList*	scl_next;
} spyCmdList_t;


#ifdef __cplusplus
}
#endif

#endif /* _SPYCMD_H */
