/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN0_HUB_H__
#define __SYS_SN_SN0_HUB_H__

/* The secret password; used to release protection */
#define HUB_PASSWORD		0x53474972756c6573ull

#define CHIPID_HUB		0
#define CHIPID_ROUTER		1

#define HUB_REV_1_0		1
#define HUB_REV_2_0		2
#define HUB_REV_2_1		3
#define HUB_REV_2_2		4
#define HUB_REV_2_3             5
#define HUB_REV_2_4             6

#define MAX_HUB_PATH		80

#if defined(IP27)

#include <sys/SN/SN0/addrs.h>
#include <sys/SN/SN0/hubpi.h>
#include <sys/SN/SN0/hubmd.h>
#include <sys/SN/SN0/hubio.h>
#include <sys/SN/SN0/hubni.h>
#include <sys/SN/SN0/hubcore.h>

#ifdef SABLE
#define IP27_NO_HUBUART_INT	1
#endif

#else /* ! IP27 */

<< BOMB! SN0 is only defined for IP27 >>

#endif /* defined(IP27) */

/* Translation of uncached attributes */
#define	UATTR_HSPEC	0
#define	UATTR_IO	1
#define	UATTR_MSPEC	2
#define	UATTR_UNCAC	3

#if _LANGUAGE_ASSEMBLY

/*
 * Get nasid into register, r (uses at)
 */
#define GET_NASID_ASM(r)				\
	dli	r, LOCAL_HUB_ADDR(NI_STATUS_REV_ID);	\
	ld	r, (r);					\
	and	r, NSRI_NODEID_MASK;			\
	dsrl	r, NSRI_NODEID_SHFT

#endif /* _LANGUAGE_ASSEMBLY */

#if _LANGUAGE_C

#include <sys/xtalk/xwidget.h>
#include <sys/sema.h>

/* hub-as-widget iograph info, labelled by INFO_LBL_XWIDGET */
typedef struct v_hub_s *v_hub_t;

struct nodepda_s;
int hub_check_pci_equiv(void *addra, void *addrb);
void capture_hub_stats(cnodeid_t, struct nodepda_s *);
void init_hub_stats(cnodeid_t, struct nodepda_s *);

#endif /* _LANGUAGE_C */

#endif /* __SYS_SN_SN0_HUB_H__ */
