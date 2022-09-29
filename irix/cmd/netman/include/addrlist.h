#ifndef ADDRLIST_H
#define ADDRLIST_H
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Address List service include file
 *
 *	$Revision: 1.2 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/time.h>
#include <protocols/ether.h>
#include <histogram.h>

/* Protocol Version */
#define AL_VERSION	1

/* Packet types */
#define AL_ENDOFDATA	0
#define AL_DATATYPE	1

/* Sizes */
#define AL_NAMELEN	30
#define AL_MAXENTRIES	92

struct alkey {
	struct etheraddr	alk_paddr;
	char			alk_name[AL_NAMELEN];
	unsigned long		alk_naddr;
};

struct alentry {
	struct alkey		ale_src;
	struct alkey		ale_dst;
	struct counts		ale_count;
};

struct alpacket {
	unsigned short		al_version;
	unsigned short		al_type;
	unsigned long		al_source;
	struct timeval		al_timestamp; 
	unsigned int		al_nentries;
	struct alentry		al_entry[AL_MAXENTRIES];
};

#endif /* !ADDRLIST_H */
