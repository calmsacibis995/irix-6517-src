/*************************************************************************
 *                                                                       *
 *              Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                       *
 * These coded instructions, statements, and computer programs  contain  *
 * unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 * are protected by Federal copyright law.  They  may  not be disclosed  *
 * to  third  parties  or copied or duplicated in any form, in whole or  *
 * in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                       *
 *************************************************************************/

#ifndef __SNIF_H__
#define __SNIF_H__

extern struct mbuf *mblk_to_mbuf(mblk_t *, int, int, int);
extern mblk_t *mbuf_to_mblk(struct mbuf *, int);
extern struct ifnet *ifunit(char *);
extern struct rawif *rawif_lookup(const char *);

#define SNIF_ID		41
#define MAX_MAC_LEN	6

#ifdef _MP_NETLOCKS
#define SNIF_INITLOCK(snifp)	initnlock(&(snifp)->snif_lock, "snif_lock")
#define SNIF_LOCK(snifp,s)	s = splockspl((snifp)->snif_lock, splimp)
#define SNIF_UNLOCK(snifp,s)	spunlockspl((snifp)->snif_lock, s)
#endif /* _MP_NETLOCKS */

typedef struct {
	ulong	len;
	caddr_t	addr[MAX_MAC_LEN];
} mac_address_t;

typedef struct saps {
	struct saps 	*next;
	dlsap_family_t	*dp;
} snif_sap_t;

#define SNIF_SAP_SIZE	sizeof(snif_sap_t)

typedef struct {
	queue_t		*rq;
	struct ifnet	*ifp;
	ushort		mtu;
	ushort		id;
	toid_t		wdogid;
	ulong		queuing;
	ulong		priority;
	ulong		media;
	mac_address_t	factory_mac;
	mac_address_t	current_mac;
	snif_sap_t	*saps;
#ifdef _MP_NETLOCKS
	lock_t		snif_lock;
#endif /* _MP_NETLOCKS */
} snif_dev_t;

#define SNIF_DEV_SIZE	sizeof(snif_dev_t)

extern char *iftab[];

extern snif_devcnt;

#endif
