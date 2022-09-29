/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1996-1998, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN1_SNAC_H__
#define __SYS_SN_SN1_SNAC_H__

/* The secret password; used to release protection */
#define SNAC_PASSWORD		0x53474972756c6573ull

#if defined(IP33)

#include <sys/SN/SN1/addrs.h>
#include <sys/SN/SN1/snacpi.h>
#include <sys/SN/SN1/snacmd.h>
#include <sys/SN/SN1/snacio.h>
#include <sys/SN/SN1/snacni.h>
#include <sys/SN/SN1/snaclb.h>


#else /* ! IP33 */

<< BOMB! SN1 is only defined for IP33 >>

#endif /* defined(IP33) */

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

#endif /* __SYS_SN_SN1_SNAC_H__ */
