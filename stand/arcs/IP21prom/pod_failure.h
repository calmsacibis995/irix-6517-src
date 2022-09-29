/***********************************************************************\
* pod_failure.h -							*
*	Contains definitions for structures used by the prom to keep	*
*	track of diagnostic results.					*
\***********************************************************************/

#if _LANGUAGE_C

typedef struct pod_scmsg_s {
    unsigned char dv_code;
    char *short_msg;
    char *long_msg;
} pod_scmsg_t;

#endif /* _LANGUAGE_C */

/* These are bits to store in the processor's DIAGVALREG which
 * is a bit vector of which diagnostics have run and/or failed.
 */

#define PODV_DCACHE_RAN			0x0001
#define PODV_ICACHE_RAN			0x0002
#define PODV_SCACHE_RAN			0x0004
#define PODV_SCACHEWB_RAN		0x0008
#define PODV_BUSTAGS_RAN		0x0010

#define PODV_DCACHE_FAILED		0x0100
#define PODV_ICACHE_FAILED		0x0200
#define PODV_SCACHE_FAILED		0x0400
#define PODV_SCACHEWB_FAILED		0x0800
#define PODV_BUSTAGS_FAILED		0x0100

/* Macros for extracting diagval info. */

#define EVDIAG_DIAGMASK		0xff
#define EVDIAG_DIAGCODE(_x)	((char)(_x & EVDIAG_DIAGMASK))

#include <sys/EVEREST/evdiag.h>

