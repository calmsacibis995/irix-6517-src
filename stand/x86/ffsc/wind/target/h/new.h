/* new.h - run-time support C++ operator new */

/*
modification history
--------------------
01a,19oct92,srh  written.
*/

#ifndef __INCnewh
#define __INCnewh

#include <stddef.h>

extern void (*set_new_handler (void(*)()))();

void *operator new(size_t, void*);

#endif				/* __INCnewh */
