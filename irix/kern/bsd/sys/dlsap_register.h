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
#ifndef __SYS_DLSAP_REGISTER_H__
#define __SYS_DLSAP_REGISTER_H__
#ifdef __cplusplus
extern "C" {
#endif

struct ifnet;
struct mbuf;

typedef struct dlsap_family {
	int		dl_family;
	unsigned short	dl_ethersap;
	unsigned short	dl_encap;
	int 		(*dl_infunc)(struct dlsap_family *,struct ifnet*,
					struct mbuf*,void *);
	int		(*dl_outfunc)(void);
	int		(*dl_netisr)(void);
	struct dlsap_family	*dl_next;
	int		reserved;	/* Do not use	*/
	int		ref_count;	/* Do not use	*/
} dlsap_family_t;

#define DLSAP_FAMILY_SIZE	sizeof(dlsap_family_t)

extern int register_dlsap(dlsap_family_t *, dlsap_family_t **);
extern int unregister_dlsap(dlsap_family_t *);
extern dlsap_family_t *dlsap_find(unsigned short, unsigned short);

extern dlsap_family_t *dlsap_families[];

#define DLSAP_BUSY	(-2)

#define DL_NULL_ENCAP	0x00
#define DL_SNAP0_ENCAP	0x01
#define DL_LLC_ENCAP	0x02
#define DL_ETHER_ENCAP	0x04

#ifdef __cplusplus
}
#endif
#endif
