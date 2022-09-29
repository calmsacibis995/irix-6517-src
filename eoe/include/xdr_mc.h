#ifndef xdr_mc_included
#define xdr_mc_included

#include <rpc/rpc.h>

#include "mediaclient.h"

/*
 * This file is included by mediad and by libmediaclient.
 * It declares the XDR routines used in mediad's monitor
 * client protocol.
 *
 *	irix/cmd/mediad
 *	irix/lib/libmediaclient
 */

typedef struct xdr_mc_closure {
    bool_t (*volpartproc)(XDR *, u_int *, mc_partition_t ***);
    bool_t (*devpartproc)(XDR *, u_int *, mc_partition_t ***);
    bool_t (*partdevproc)(XDR *, mc_device_t **);
} xdr_mc_closure_t;

#ifdef __cplusplus
extern "C" {
#endif

    /* These routines are defined in xdr_mc.c. */

    extern bool_t xdr_mc_system   (XDR *, mc_system_t *);
    extern bool_t xdr_mc_volume   (XDR *, mc_volume_t *);
    extern bool_t xdr_mc_device   (XDR *, mc_device_t *);
    extern bool_t xdr_mc_partition(XDR *, mc_partition_t *);
    extern bool_t xdr_mc_event    (XDR *, mc_event_t *);

    /* These routines are not strictly XDR-related, but they fell out
       and they're useful. */

    extern unsigned mc_count_pp(const mc_system_t *); /* count part. ptrs */
    extern unsigned mc_count_c(const mc_system_t *); /* count string chars */

#ifdef __cplusplus
}					// end extern "C"
#endif

#endif /* !xdr_mc_included */
