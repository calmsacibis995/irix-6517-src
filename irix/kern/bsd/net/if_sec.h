/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __IF_SEC_H__
#define __IF_SEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.1 $"

#include <sys/mac_label.h>

typedef struct ifsec {
	mac_label * ifs_label_max;	/* dominates all dgrams on if   */
	mac_label * ifs_label_min;	/* dominated by all if dgrams 	*/
	__uint32_t  ifs_doi;		/* domain of interpretation	*/
	u_char	    ifs_authority_max;	/* maximum authority allowed	*/
	u_char	    ifs_authority_min;	/* minimum authority permitted	*/
	u_char	    ifs_reserved;	/* must be zero until defined	*/
	u_char	    ifs_idiom;		/* security idiom (see below)	*/

	struct	mbuf *ifs_lbl_cache;	/* label mapping cache		*/
	uid_t 	ifs_uid;		/* default uid for dacless if */

} ifsec_t;

/* values for ifs_idiom */
#define	IDIOM_MONO		0	/* Monolabel (unlabelled) 	*/
#define IDIOM_BSO_TX		1	/* BSO required for TX, not RX  */
#define IDIOM_BSO_RX		2	/* BSO required for RX, not TX  */
#define IDIOM_BSO_REQ		3	/* BSD required for TX and RX 	*/
#define IDIOM_CIPSO		4	/* CIPSO, Apr 91, tag types 1, 2*/
#define IDIOM_SGIPSO		5	/* SGI's Mint-flavored CIPSO, Apr 91*/
#define IDIOM_TT1		6	/* CIPSO, Apr 91, tag type 1 only */ 
#define IDIOM_CIPSO2		7	/* CIPSO2, Jan 92 		*/
#define IDIOM_SGIPSO2		8	/* SGIPSO2, Jan 92, with UID*/
#define IDIOM_SGIPSOD		9	/* SGIPSO, Apr 91, with UID */
#define IDIOM_SGIPSO2_NO_UID    10	/* SGIPSO2, Jan 92, no UID	*/
#define IDIOM_TT1_CIPSO2        11	/* CIPSO2, Jan 92, tag type 1 only */ 
#define IDIOM_MAX		IDIOM_TT1_CIPSO2

/* #define if_label_max		if_sec.ifs_label_max		*/
/* #define if_label_min		if_sec.ifs_label_min		*/
/* #define if_doi			if_sec.ifs_doi		*/
/* #define if_authority_max	if_sec.ifs_authority_max	*/
/* #define if_authority_min	if_sec.ifs_authority_min	*/
/* #define if_idiom		if_sec.ifs_idiom		*/

#define IFSEC_VECTOR	1	/* ifsec requests are vectored */

#ifdef __cplusplus
}
#endif

#endif	/* __IF_SEC_H_ */












