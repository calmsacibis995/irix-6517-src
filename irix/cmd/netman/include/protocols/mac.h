#ifndef MAC_H
#define MAC_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * MAC definitions.
 */
#include <sys/types.h>
#include <sys/fddi.h>
#include "protocols/fddi.h"

typedef struct {
	u_char	bcn_type;
	u_char	bcn_pad[3];
} beacon;

typedef union {
	u_long	clm;		/* clame */
	beacon	bcn;		/* beacon */
} MAC;

struct macframe {
	ProtoStackFrame		mf_frame;	/* base class state */
	MAC 			mf_mac;
};
#define	mf_clm	mf_mac.clm
#define mf_bcn	mf_mac.bcn.bcn_type
#define mf_pad	mf_mac.bcn.bcn_pad

#define MAC_SUBFC_BEACON	0x2
#define MAC_SUBFC_CLM		0x3

/*
 * MAC protocol name.
 */
extern char	macname[];

#endif
