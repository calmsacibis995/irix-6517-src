
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
#ifndef __GIO_GIOBR_PRIVATE_H__
#define __GIO_GIOBR_PRIVATE_H__

/*
 * giobr_private.h -- private definitions for giobr
 * only the giobr driver (and its closest friends)
 * should ever peek into this file.
 */

#ident "sys/GIO/giobr_private: $Revision: 1.1 $"


#include <sys/GIO/gioio_private.h>

/*
 * All GIO providers set up PIO using this information.
 */
struct giobr_piomap_s {
	struct gioio_piomap_s	bp_pp;		/* generic stuff */

#define	bp_flags	bp_pp.pp_flags		/* GIOBR_PIOMAP flags */
#define	bp_dev		bp_pp.pp_dev		/* associated gio card */
#define	bp_slot		bp_pp.pp_slot		/* which slot the card is in */
#define	bp_space	bp_pp.pp_space		/* which address space */
#define	bp_gioaddr	bp_pp.pp_gioaddr	/* starting offset of mapping */
#define	bp_mapsz	bp_pp.pp_mapsz		/* size of this mapping */
#define	bp_kvaddr	bp_pp.pp_kvaddr		/* kernel virtual address to use */

	iopaddr_t		bp_xtalk_addr;	/* corresponding xtalk address */
	xtalk_piomap_t		bp_xtalk_pio;	/* corresponding xtalk resource */
};

/*
 * All GIO providers set up DMA using this information.
 */
struct giobr_dmamap_s {
	struct gioio_dmamap_s	bd_pd;
#define	bd_flags	bd_pd.pd_flags		/* GIOBR_DMAMAP flags */
#define	bd_dev		bd_pd.pd_dev		/* associated gio card */
#define	bd_slot		bd_pd.pd_slot		/* which slot the card is in */
	struct giobr_soft_s    *bd_soft;    	/* giobr soft state backptr */
	xtalk_dmamap_t		bd_xtalk;	/* associated xtalk resources */
	/* XXX- add fields for RRB management? */
};

/*
 * All GIO providers set up interrupts using this information.
 */

struct giobr_intr_s {
	struct gioio_intr_s	bi_pi;
#define	bi_flags	bi_pi.pi_flags		/* GIOBR_INTR flags */
#define	bi_dev		bi_pi.pi_dev		/* associated gio card */
#define	bi_line		bi_pi.pi_line		/* which GIOBR interrupt line */
	struct giobr_soft_s    *bi_soft;	/* shortcut to soft info */
	intr_func_t	bi_func;		/* handler function (when connected) */
	intr_arg_t	bi_arg;			/* handler parameter (when connected) */
};

#endif /* __GIO_GIOBR_PRIVATE_H__ */
