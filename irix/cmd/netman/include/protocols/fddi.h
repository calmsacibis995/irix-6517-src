#ifndef FDDI_H
#define FDDI_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * FDDI definitions.
 */
#include <sys/types.h>
#include <sys/fddi.h>
#include "protostack.h"

/*
 * FDDI field identifiers are array indices, so don't change them.
 */
#define FDDIFID_DST	3
#define FDDIFID_SRC	4

/*
 * FDDI protocol options
 */
enum fddi_propt { FDDI_PROPT_HOSTBYNAME, FDDI_PROPT_NATIVEORDER };

/*
 * FDDI protocol name and broadcast address.
 */
extern char	  fddiname[];
extern LFDDI_ADDR fddibroadcastaddr;

struct fddiframe {
	ProtoStackFrame		ff_frame;	/* base class state */
	struct lmac_hdr		ff_hdr;
};
#define ff_fs	ff_hdr.mac_bits
#define ff_fc	ff_hdr.mac_fc
#define ff_src	ff_hdr.mac_sa
#define ff_dst	ff_hdr.mac_da

#endif
