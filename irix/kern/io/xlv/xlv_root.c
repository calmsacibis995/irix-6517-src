#ident "$Revision: 1.32 $"

#ifdef USER_MODE
#define _KERNEL 1
#endif

/**************************************************************************
 *									  *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <string.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/bpqueue.h>
#include <sys/mkdev.h>
#include <sys/scsi.h>
#include <sys/invent.h>
#include <sys/iograph.h>

#ifdef USER_MODE

/* Explicitly override the kernel definition of major device numbers. */
#undef major
#define major(x)        (int)(((unsigned)(x)>>L_BITSMINOR) & L_MAXMAJ)

/* Prototypes. */
#include <drv_test.h>

#endif /* USER_MODE */

#include <sys/debug.h>
#include <sys/elog.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/dump.h>
#include <sys/open.h>
#include <sys/splock.h>
#include <sys/sema.h>
#include <ksys/vfile.h>
#include <sys/var.h>
#include <sys/major.h>

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_vh.h>
#include <sys/dvh.h>
#include <sys/kopt.h>
#include "xlv_ioctx.h"
#include "xlv_procs.h"

/*
 * Prototypes
 */
extern int devavail( dev_t );
extern dev_t devparse(register char *cp);

dev_t       xlv_rootdisk = NULL;	/* root disk device - has xlv label */
static char *xlvrootdevname = NULL;	/* root volume name */

#ifndef USER_MODE

#define MAX_VOLS        1
#define MAX_SUBVOLS     3*MAX_VOLS
#define XLV_LAB_NAME    "xlvlab"

#define XLV_TAB_UUID_BOOT_DATA  0
#define XLV_TAB_UUID_BOOT_LOG   1
#define XLV_TAB_UUID_BOOT_RT    2
#define XLV_TAB_UUID_RSVD  	4

#define PTNUM_VOLHDR		8

#define PLEX_SIZE(n) 		(sizeof(xlv_tab_plex_t) + \
				((n - 1)*sizeof(xlv_tab_vol_elmnt_t)))

#define LABEL_SIZE(bsize)	(roundblk(sizeof(xlv_vh_master_t), bsize) + \
				2*roundblk(sizeof(xlv_vh_disk_label_t), bsize))

#define XLV_TAB_SIZE		(sizeof(xlv_tab_t) + 	\
				(XLV_TAB_UUID_RSVD-1)*sizeof(xlv_tab_subvol_t))

#define	MATCH_SV_UUID(v, lep)  \
	(uuid_equal(&lep->vol.volume_uuid, &(v->uuid), &st)	 &&	\
	 uuid_equal(&lep->vol.data_subvol_uuid, &(v->data_subvol->uuid), &st))



/*
 * roundblk()
 *	Rounds count upward to the nearest multiple of dev_bsize.
 *
 * RETURNS:
 *	 rounded value
 */
int
roundblk(int count, int dev_bsize)
{
	count = (count + dev_bsize -1)/ dev_bsize;
	count *= dev_bsize;
	return count;
}

/*
 * xlv_read_dev()
 *	Read the given block from the given device and return the
 * 	data in the given buffer.
 * 
 * RETURNS: 
 *	0 on success 
 * 	1 on error
 */
int 
xlv_read_dev(dev_t ddev, int blkno, char *buf, int len)
{
	int	error;
	buf_t	*bp;

	bp = bread(ddev, blkno, BTOBB(len));

	if (!( error = (bp->b_flags & B_ERROR) ) ) {
		bcopy(bp->b_un.b_addr, buf, len);
	} 

	bp->b_flags |= B_AGE;
	brelse(bp);
	return(error);
}

/*
 * xlv_get_current_label()
 *	By looking at the xlv master label determine which
 *	of the xlv disk labels to use.
 *
 * RETURNS:
 *	pointer to the valid label or NULL.
 */
xlv_vh_disk_label_t *
xlv_get_current_label( xlv_vh_master_t *xlv_master_label, int bsize)
{
	xlv_vh_disk_label_t	*labelp0, *labelp1, *label = NULL;

	/*
	 * Point to primary label.
	 */
	labelp0 = (xlv_vh_disk_label_t *)(((char *)xlv_master_label) + 
			roundblk(sizeof(xlv_vh_master_t), bsize));

	/*
	 * Point to secondary label.
	 */
	labelp1 = (xlv_vh_disk_label_t *)(((char *)labelp0) + 
			roundblk(sizeof(xlv_vh_disk_label_t), bsize));

	/*
	 * Determine the correct label to use.
	 */
	if ((xlv_master_label->vh_seq_number & 0x1) &&
	    (labelp1->header.magic == XLV_VH_MAGIC_NUM)) {
		label = labelp1;
	} else if ((!(xlv_master_label->vh_seq_number & 0x1)) &&
		(labelp0->header.magic == XLV_VH_MAGIC_NUM)) {
		label = labelp0;
	}
	return ( label );
}

/*
 * xlv_add_plex()
 *	Allocate the structures and fill in the information to
 *	add the given disk element as a plex to the given subvolume.
 *
 * RETURNS:
 *	always returns 1.
 */
int
xlv_add_plex( 
	xlv_tab_subvol_t 		*sv,
	xlv_vh_disk_label_entry_t	*lep,
	dev_t				diskdev)
{
	int				plex_size;
	xlv_tab_plex_t			*p;
	xlv_tab_vol_elmnt_t		*ve;
	xlv_tab_disk_part_t		*dp;
	

	/*
	 * Get the plex information for the subvolume
 	 */
	if (sv->plex[lep->plex.seq_num]) {
		return( 1 );
	}

	plex_size =  PLEX_SIZE(lep->plex.num_vol_elmnts);
	p = (xlv_tab_plex_t *) kmem_zalloc (plex_size, KM_NOSLEEP);

	sv->plex[lep->plex.seq_num] = p;
	ve = &(p->vol_elmnts[lep->vol_elmnt.seq_num_in_plex]);
	dp = &(ve->disk_parts[lep->disk_part.seq_in_grp]);

	COPY_UUID(lep->plex.uuid, p->uuid);
	COPY_NAME(lep->plex.name, p->name);

	p->flags	  = lep->plex.flags;
	p->num_vol_elmnts = lep->plex.num_vol_elmnts;
	p->max_vol_elmnts = lep->plex.num_vol_elmnts;

	COPY_UUID(lep->vol_elmnt.uuid, ve->uuid);

	/* 
	 * ### DO NOT COPY the volume element name into
	 * the veu_name field.  veu_name coexists with the
	 * timestamp field which is needed to determine which
	 * plex to mount first
	 * COPY_NAME(lep->vol_elmnt.name, ve->veu_name);
	 */

	ve->type	        = lep->vol_elmnt.type;
	ve->state	        = lep->vol_elmnt.state;
	ve->grp_size	        = lep->vol_elmnt.grp_size;
	ve->end_block_no	= lep->vol_elmnt.end_block_no;
	ve->start_block_no	= lep->vol_elmnt.start_block_no;
	ve->stripe_unit_size	= lep->vol_elmnt.stripe_unit_size;
	ve->veu_timestamp	= lep->vol.subvol_timestamp;

	COPY_UUID(lep->disk_part.uuid, dp->uuid);
	dp->part_size 	= lep->disk_part.size;

	/*
	 * Because the disk may have moved, we need to update the
	 * controller and unit numbers.
	 */

	dp->dev[0]	= diskdev;
	dp->n_paths	= 1;
	dp->active_path	= 0;

	sv->num_plexes++;

	return( 1 );
}

/*
 * xlv_get_xlv_label()
 *	Read the volume header from the disk and it check if an	
 *	xlvlab file is present. If so, read this file and read
 * 	the contents to the caller.
 *
 * RETURNS:
 *	a pointer to a buffer containing the label on success,
 * 	NULL otherwise
 */
char *
xlv_get_xlv_label( dev_t diskdev, int *bsize)
{
	int			lab_sz, vh_size, lab_off, i;
	char			*buf = NULL;
	struct volume_header	*vh;

	/*
	 * Check if the disk device exists.
	 */
	if ( !devavail(diskdev) ) {
		return 0;
	}

	/*
	 * Allocate structure to hold disk volume header
	 */
	vh_size = BBTOB(BTOBB(sizeof(struct volume_header)));
	vh = (struct volume_header *)kmem_zalloc(vh_size, KM_NOSLEEP);

	/*
 	 * Read disk volume header information
 	 * and check for magic number.
 	 */
	if ( (xlv_read_dev( diskdev, 0, (char *)vh, vh_size) == 0) &&
	     (vh->vh_magic == VHMAGIC)) {

		/*
	 	 * Get sector size of physical disk. 
	 	 */
		if ( (*bsize = vh->vh_dp.dp_secbytes) == 0)
			*bsize = DEV_BSIZE;

		lab_sz  = LABEL_SIZE(*bsize);

		/*
	  	 * Scan the disk directory an XLV file.
	  	 */
		for (i = 0; i < NVDIR; i++ ) {

			/*
		 	 * Check if XLV file exists in disk directory.
		 	 */
			if (strcmp(vh->vh_vd[i].vd_name, XLV_LAB_NAME) == 0) {

				buf = (char *)kmem_zalloc(lab_sz, KM_NOSLEEP);

				lab_off = vh->vh_vd[i].vd_lbn;

				/*
			 	 * Read XLV file information.
			 	 */
				if ( xlv_read_dev(diskdev,lab_off,buf,lab_sz)) {

					/*
					 * Error occured reading label.
					 */
					kmem_free(buf, lab_sz);
					buf = NULL;
				}
			}
		}
	}

	kmem_free( vh, vh_size );
	return( buf );
}

/*
 * xscaninvent()
 *
 * walk the hardware inventory, return inventory entry + dev_t
 * when/if the hwg routines support sending the dev_t directly,
 * we should replace this func.
 */

int
xscaninvent(int (*fun)(inventory_t *, dev_t, void *), void *arg)
{
	inventory_t *ie;
	invplace_t iplace = INVPLACE_NONE;
	int rc;

	ie = 0;
	rc = 0;
	while (ie = (inventory_t *)get_next_inventory(&iplace)) {
		rc = (*fun)(ie, iplace.invplace_vhdl, arg);
		if (rc)
			break;
	}
	return rc;
}


/*
 * xlv_scan_invent()
 *	This routine is called for each device in the machine inventory.
 *	If the device is a scsi disk, try to read the volume header and
 *	determine if it contains an XLV volume element. If so, check if
 *	the element belongs to the given subvolume.
 *
 * RETURNS:
 *	always returns 1
 */
int
xlv_scan_invent(inventory_t *ie, dev_t diskdev, void *x)
{
	int 				d, dev_bsize, ret = 0;
	char				*buf;
	uint_t				st;
	uuid_t				lep_uuid;
	xlv_tab_subvol_t 		*sv = x;
	xlv_tab_vol_entry_t		*v = sv->vol_p;
	xlv_vh_disk_label_t		*label;
	xlv_vh_disk_label_entry_t	*lep;
	dev_t                           vhdev;
	dev_t                           partdev;
	char *vhname = EDGE_LBL_VOLUME_HEADER "/" EDGE_LBL_BLOCK;
	char pathname[100];
	
	/*
	 * Determine if this is a disk device.
	 *
	 * Note that only scsi drives are supported.
	 */
	if ((ie->inv_class == INV_DISK) && (ie->inv_type == INV_SCSIDRIVE)) {

	   /* Given the current node, find the handle to the volume header */
	   if (hwgraph_traverse(diskdev, vhname, &vhdev) != GRAPH_SUCCESS)
		   return 0;

           hwgraph_vertex_unref(vhdev);

	   /*
	    * Read the XLV label from the disk.
	    */
	   if ((buf = xlv_get_xlv_label(vhdev, &dev_bsize)) != NULL) {

	      /*
	       * Get the current XLV label.
	       */
	      if (label = xlv_get_current_label((xlv_vh_master_t *)buf,
						dev_bsize)) {

	         /*
		  * Check each partition of the disk 
		  * for XLV subvolume elements.
		  */
		 for (d = 0; d < XLV_MAX_DISK_PARTS; d++){

		    lep = &label->label_entry[d];
		    if (uuid_is_nil (&(lep->disk_part.uuid), &st))
			continue;

		    /*
		     * Must be part of a volume.
		     */
		    if (uuid_is_nil (&(lep->vol.volume_uuid), &st))
			continue;

		    /*
		     * Check if this is part of the root volume.	
		     */

		    if (strcmp(lep->vol.volume_name, xlvrootdevname) == 0) {

		       /*
		 	* Copy geometry information from the 
			* disk label to the in-core xlv_tab 
			* and xlv_tab_vol structures.
		 	*/
		       v->flags             = lep->vol.flag;
		       v->sector_size       = lep->vol.sector_size;

		       if (uuid_is_nil(&v->uuid, &st)) {
		           COPY_UUID(lep->vol.volume_uuid, v->uuid);
		           COPY_NAME(lep->vol.volume_name, v->name);
		       } else {
			   if (!uuid_equal(&v->uuid,&lep->vol.volume_uuid,&st)){
			        ret = 1;
				goto err_out;
			    }
		       }


		      /*
		       * Determine the correct subvolume.
		       * Add the information for this plex to the subvolume.
		       */
		       switch(lep->vol.subvolume_type) {
		           case XLV_SUBVOL_TYPE_DATA:
			       sv = v->data_subvol;
			       COPY_UUID(lep->vol.data_subvol_uuid, lep_uuid);
			       break;
			   case XLV_SUBVOL_TYPE_LOG:
			       sv = v->log_subvol;
		               COPY_UUID( lep->vol.log_subvol_uuid, lep_uuid);
			       break;
			   case XLV_SUBVOL_TYPE_RT:
			       sv = v->rt_subvol;
		               COPY_UUID(lep->vol.rt_subvol_uuid, lep_uuid);
				break;
			    default:
			        break;
		       }

		       /*
			* Copy the uuid of the subvolume.
			* If this is not the first plex, compare the uuid.
		        */
		       if (uuid_is_nil(&sv->uuid, &st)) {
		           COPY_UUID(lep_uuid, sv->uuid);
		       } else {
			   if (!uuid_equal(&sv->uuid, &lep_uuid, &st)) {
			        ret = 1;
				goto err_out;
			   }
		       }

		       /* generate the pathname for the partition we want */
		       SPRINTF_PART(d, pathname);
		       strcat(pathname, "/" EDGE_LBL_BLOCK);
		       /* get the dev_t for the partition */
		       if (hwgraph_traverse(diskdev, pathname, &partdev) == GRAPH_SUCCESS) {
			       xlv_add_plex( sv, lep, partdev);
			       hwgraph_vertex_unref(partdev);
		       } else {
#ifdef DEBUG			     
			       printf("xlv: unable to resolve root plex hwg pathname %s", pathname);
#endif /* DEBUG */
		       }
                     }
	          }
	      }

err_out:
	      /*
	       * Free buf.
	       */
	      kmem_free(buf, LABEL_SIZE(dev_bsize));
	   }
	}	

	/*
	 * XXX: if ret is 1, do we want to return 1?
	 */
	if (ret)
		return 0;
	return 0;
}



/*
 * Compare two volume element's states. Pick the one that should be
 * active. Change any "older" active ve to stale state.
 * This is lifted from cmd/xlv/xlv_assemble/xlv_assemble.c.
 *
 * Return value:
 *	-1 = neither volume element is good
 *	 0 = both volume elmements are good
 *	 1 = vol element 1 is better; ve 2 is stale (from active) or empty
 *	 2 = vol element 2 is better; ve 1 is stale (from active) or empty
 *
 * Never choose volume elements with state "offline" or "incomplete".
 * When comparing a volume element in "offline" or "incomplete" state,
 * always pick the other volume element.
 *
 *	 4   (100) = Pick one depending on timestamp. The one picked is
 *		     active and the other is stale. If the timestamps
 *		     are the same, pick both.
 *	 4+1 (101) = Pick one depending on timestamp. If the timestamps
 *		     are the same, pick vol element 1. The one picked is
 *		     active and the other is stale.
 *	 4+2 (110) = Pick one depending on timestamp. If the timestamps
 *		     are the same, pick vol element 2.
 *	 8+1 (1001) = Pick vol element 1 and mark it active
 *	 8+2 (1010) = Pick vol elmemnt 2 and mark it active
 */
static int
ve_state_cmp(xlv_tab_vol_elmnt_t *vep1, xlv_tab_vol_elmnt_t *vep2)
{
	static int state_array[6][6] = {
	/*      vep2  |   empty  clean  active  stale  offline  incomplete */
	/* vep1|---------------------------------------------------------- */
	/* empty      */ { -1,   8+2,      2,     -1,     -1,         1  },
	/* clean      */ {8+1,     4,    4+1,    8+1,    8+1,         1  },
	/* active     */ {  1,   4+2,    4+1,      1,      1,         1  },
	/* stale      */ { -1,   8+2,      2,     -1,     -1,         1  },
	/* offline    */ { -1,   8+2,      2,     -1,     -1,        -1  },
	/* incomplete */ {  2,     2,      2,      2,     -1,        -1  },
	};

	int	retvalue;
	int	action;

ASSERT(XLV_VE_STATE_EMPTY == 0);
ASSERT(XLV_VE_STATE_CLEAN == 1);
ASSERT(XLV_VE_STATE_ACTIVE == 2);
ASSERT(XLV_VE_STATE_STALE == 3);
ASSERT(XLV_VE_STATE_OFFLINE == 4);
ASSERT(XLV_VE_STATE_INCOMPLETE == 5);

	if (vep1 && vep2)
		action = state_array [(unsigned)vep1->state]
				     [(unsigned)vep2->state];
	else if (!vep1)
		action = state_array [(unsigned)XLV_VE_STATE_EMPTY]
				     [(unsigned)vep2->state];
	else /* !vep2 */
		action = state_array [(unsigned)vep1->state]
				     [XLV_VE_STATE_EMPTY];

	switch (action) {
	case -1:
		retvalue = -1;
		break;
	case 1:
		vep1->state = XLV_VE_STATE_ACTIVE;
		retvalue = 1;
		break;
	case 2:
		vep2->state = XLV_VE_STATE_ACTIVE;
		retvalue = 2;
		break;
	case 4:
		if (vep1->veu_timestamp == vep2->veu_timestamp) {
			vep1->state = XLV_VE_STATE_ACTIVE;
			vep2->state = XLV_VE_STATE_ACTIVE;
			retvalue = 0;
		} else if (vep1->veu_timestamp > vep2->veu_timestamp) {
			vep1->state = XLV_VE_STATE_ACTIVE;
			vep2->state = XLV_VE_STATE_STALE;
			retvalue = 1;
		} else {
			vep1->state = XLV_VE_STATE_STALE;
			vep2->state = XLV_VE_STATE_ACTIVE;
			retvalue = 2;
		}
		break;
	case 4+1:
		if (vep1->veu_timestamp >= vep2->veu_timestamp) {
                        vep1->state = XLV_VE_STATE_ACTIVE;
                        vep2->state = XLV_VE_STATE_STALE;
                        retvalue = 1;
                } else {
                        vep1->state = XLV_VE_STATE_STALE;
                        vep2->state = XLV_VE_STATE_ACTIVE;
                        retvalue = 2;
                }
                break;
	case 4+2:
		if (vep1->veu_timestamp <= vep2->veu_timestamp) {
                        vep1->state = XLV_VE_STATE_STALE;
                        vep2->state = XLV_VE_STATE_ACTIVE;
                        retvalue = 2;
                } else {
                        vep1->state = XLV_VE_STATE_ACTIVE;
                        vep2->state = XLV_VE_STATE_STALE;
                        retvalue = 1;
                }
                break;
	case 8+1:
		vep1->state = XLV_VE_STATE_ACTIVE;
		retvalue = 1;
		break;
	case 8+2:
		vep2->state = XLV_VE_STATE_ACTIVE;
		retvalue = 2;
		break;
	default:
		ASSERT (0);
		retvalue = -1;
		break;
	}

	return (retvalue);

} /* end of ve_state_cmp() */


/*
 * Pick the volume elements in the subvolumes which are "good".
 * Good volume elements are either in "clean" or "active" states.
 * They also have the most recent timestamp. Volume elements
 * that are not "good" should be: empty|stale|offline|incomplete ...
 * Volume elements which are not good will be marked "stale".
 *
 * Return:
 *	0 - No choice made because there is only one plex.
 *	1 - Chose between plex ve's.
 *
 * Note:
 *	If there is only one plex and the ve state is empty then
 *	set the state to active and return 1 so the disk labels
 *	are updated.
 *
 *      This routine is lifted from cmd/xlv/xlv_assemble/xlv_assemble.c.
 *	Unlike the original, this version does not handle more than one
 *      volume element in a plex.
 */
static int
choose_subvol_pieces (xlv_tab_subvol_t *svp)
{
	int			p, insert;
	int			idx;
	int			cmp_count;
	xlv_tab_vol_elmnt_t	*cmp_order[XLV_MAX_PLEXES];
	xlv_tab_vol_elmnt_t	*vep;

	ASSERT (svp->num_plexes > 1);

	cmp_count = 0;
	bzero(cmp_order, sizeof(cmp_order));

	/*
	 * Compare vol element states to find primary copy.
	 *
	 * First determine the order of the comparsion.
	 * Sort by timestamp (which is the second order
	 * criteria when both ve are in a good state).
	 */

	for (p = 0; p < XLV_MAX_PLEXES; p++) {

		if (svp->plex[p] == NULL)
			continue;

		vep = &svp->plex[p]->vol_elmnts[0];

		/*
		 * Find a place for ve in the cmp_order table.
		 */
		for (insert = 0; insert < cmp_count; insert++) {
			if (vep->veu_timestamp >
			    cmp_order[insert]->veu_timestamp)
				break;	/* insert here */
		}
		
		/*
		 * Insert entry -- "insert" is the index of
		 * the new entry. Move all following entries
		 * over one. Start migration from the back.
		 */
		for (idx = cmp_count; idx > insert; idx--) {
			cmp_order[idx] = cmp_order[idx-1];
		}
		cmp_order[insert] = vep;
		cmp_count++;
	}

	/*
	 * Now compare the ve states in the order that we've
	 * sorted them into.
	 */
	if (cmp_count > 1) {
		/*
		 * More than one volume element covers this
		 * address space so find the "best" one.
		 */
		vep = cmp_order[0];
		for (idx = 1; idx < cmp_count; idx++) {
			if (2 == ve_state_cmp(vep, cmp_order[idx]))
				vep = cmp_order[idx];
		}
	}

	ASSERT(vep->state != XLV_VE_STATE_OFFLINE);
	ASSERT(vep->state != XLV_VE_STATE_INCOMPLETE);
	vep->state = XLV_VE_STATE_ACTIVE;

	return(1);

} /* end of choose_subvol_pieces() */


/*
 * xlv_choose_plex()
 *	This routine determines the correct plex to use for the given
 *	subvolume. If all the plexes are CLEAN, then they are all used.
 *	If not, choose 1 randomly. 
 *
 * RETURNS:
 *	0 on success
 *
 */
int
xlv_choose_plex( xlv_tab_subvol_t *sv) 
{
	int 			i, dirty = 0, clean = 0;
	int			cleancount;
	xlv_tab_plex_t 		*plex, *cleanplex[XLV_MAX_PLEXES];
	xlv_tab_vol_elmnt_t	*ve;

	/* Return if there is only 1 plex.
	 */
	ASSERT (sv->num_plexes > 0);

	if ( sv->num_plexes == 1 ) {
		/*
		 * Make sure that the single plex is in the
		 * first position.
		 */
		for (i = XLV_MAX_PLEXES; i > 0; i--)
			if (sv->plex[i]) {
				ASSERT (sv->plex[0] == NULL);
				sv->plex[0] = sv->plex[i];
				sv->plex[i] = NULL;
				break;
			}

		return( 0 );
	}

	/*
	 * Adjust the states of the plexes to take account of their
	 * state and timestamp.
	 */
	i = choose_subvol_pieces (sv);
	ASSERT (i != 0);

	/* Scan all the plexes of this subvolume to determine 
	 * if any of them are dirty.
	 */
	for (i = 0 ; i < sv->num_plexes; i++) {
		plex = sv->plex[i];
		if (!plex) continue;

		ASSERT(plex->num_vol_elmnts == 1);
		ve = &(plex->vol_elmnts[0]);

		/*
		 * Check the state of each plex.
		 */
		if ((ve->state == XLV_VE_STATE_EMPTY) ||
		    (ve->state == XLV_VE_STATE_STALE) ) {
			dirty = 1;
		} else {
			clean = 1;
		}
	}

	/* If there are no plexes which are not online,
	 * the whole subvolume can be started.
	 */
	if ( !dirty ) {
		return( 0 );
	}

	/* None of the plexes in the subvolume are usable.
         */
        if ( !clean ) {
		/*
		 * Force the state to be clean, and let the
		 * code below remove the stale entries and compact the table.
		 */
		ve->state = XLV_VE_STATE_CLEAN;
        }


	/* At least one of the plexes are dirty.
	 * We can only start those plexes that are clean.
	 * 
	 * Scan the list of plexes removing the dirty ones. 
	 * In order that the plex array not be sparse, save
	 * pointers to the clean plexes in the cleanplex array.
	 * After all the dirty plexes have been removed, copy the
	 * the clean ones back into the subvolume structure.
	 */
	cleancount = 0;
	for (i = 0 ; i < XLV_MAX_PLEXES; i++) {
		cleanplex[i] = 0;
	}

	ASSERT( XLV_MAX_PLEXES >= sv->num_plexes );

	for (i = 0 ; i < XLV_MAX_PLEXES; i++) {

		plex = sv->plex[i];
		if (!plex) continue;

		ASSERT( plex );
		ASSERT(plex->num_vol_elmnts == 1);
		ve = &(plex->vol_elmnts[0]);

		/*
	 	 * Delete dirty plexes
	 	 */
		if ((ve->state == XLV_VE_STATE_EMPTY) ||
	    	    (ve->state == XLV_VE_STATE_STALE) ) {
			/*
		 	 * If we a choosing only one plex, 
		 	 * free the remaining ones.
		 	 */
			sv->plex[i] = NULL;
			sv->num_plexes--;

			/*
		 	 * Free plex structure.
		 	 */
			kmem_free(plex, PLEX_SIZE(plex->max_vol_elmnts));
		}  else {
		/* Save clean plexes.
	 	 */
			cleanplex[ cleancount++ ] = plex;
			sv->plex[i] = NULL;
		}

	}

	ASSERT( cleancount == sv->num_plexes );

	/* Copy the clean plexes back into the subvolume 
 	 * structure. 
	 *
	 * NOTE: Normally, xlv will handle sparse plex entries in the
	 * subvolume entry. However, in those cases, num_plexes is set
	 * to the real number (which also includes stale entries).
	 * Here, we are trying to fake out the driver so it does not
	 * do plex revive operations. So we have to ensure that when
	 * we have only one clean plex, it's in the first entry so
	 * that xlvstrategy won't die.
	 */
	for ( i = 0; i < sv->num_plexes; i++) {
		sv->plex[i] = cleanplex[ i ];
		ASSERT( sv->plex[i] );
	}

	return( 0 );
}



/*
 * xlv_boot_startup()
 *	Reads the disk and xlv data structures from the disk and
 *	builds the XLV information necessary to start the logical device.
 *
 *
 *
 * RETURNS:
 *	1 on successfully startup of boot device.
 *	0 on error.
 */
int
xlv_boot_startup( char *devname ) 
{
	int				i, ret = 1;
	int				block_map_size;
	xlv_tab_t	 		*xlv_tab;
	xlv_tab_vol_t	 		*xlv_tab_vol;
	xlv_tab_plex_t			*p;
	xlv_tab_subvol_t 		*sv_data, *sv_log, *sv_rt;
	xlv_tab_vol_entry_t		*v;


	xlvrootdevname	= devname;

	/*	
	 * Allocate space for the xlv_tab_vol structure.
	 */
	xlv_tab_vol = kmem_zalloc(sizeof(xlv_tab_vol_t), KM_NOSLEEP);

	/*
	 * Set the number of vols to 1 immediately.	
	 */
	xlv_tab_vol->num_vols = xlv_tab_vol->max_vols = 1;
	
	/*
	 * Get a pointer to the first volume structure.
	 */
	v = &(xlv_tab_vol->vol[0]);
	v->version = XLV_TAB_VOL_ENTRY_VERS;

	/*
	 * Allocate space for the subvolume structure.
	 * there will be no more than XLV_TAB_UUID_RSVD-1 (3) subvolumes.
	 */
	xlv_tab = kmem_zalloc(XLV_TAB_SIZE, KM_NOSLEEP);

	xlv_tab->max_subvols = XLV_TAB_UUID_RSVD;

	/*
	 * Setup DATA subvolume.
	 */
	sv_data             = &(xlv_tab->subvolume[XLV_TAB_UUID_BOOT_DATA]);
	sv_data->vol_p      = v;
	sv_data->subvol_type= XLV_SUBVOL_TYPE_DATA;
	sv_data->subvol_depth= 0;
	sv_data->num_plexes =0;
	v->data_subvol      = sv_data;
	v->data_subvol->dev = makedev(XLV_MAJOR,XLV_TAB_UUID_BOOT_DATA);

	/*
	 * Setup LOG subvolume.
	 */
	sv_log              = &(xlv_tab->subvolume[XLV_TAB_UUID_BOOT_LOG]);
	sv_log->vol_p       = v;
	sv_log->subvol_type = XLV_SUBVOL_TYPE_LOG;
	sv_log->subvol_depth= 0;
	sv_log->num_plexes  = 0;
	v->log_subvol  	    = sv_log;
	v->log_subvol->dev  = makedev(XLV_MAJOR,XLV_TAB_UUID_BOOT_LOG);

	/*
	 * Setup REALTIME subvolume.
	 */
	sv_rt              = &(xlv_tab->subvolume[XLV_TAB_UUID_BOOT_RT]);
	sv_rt->vol_p       = v;
	sv_rt->subvol_type = XLV_SUBVOL_TYPE_RT;
	sv_rt->subvol_depth= 0;
	sv_rt->num_plexes  = 0;
	v->rt_subvol  	    = sv_rt;
	v->rt_subvol->dev  = makedev(XLV_MAJOR,XLV_TAB_UUID_BOOT_RT);


	/*
	 * Look for the components of this volume.	
	 */
	xscaninvent( xlv_scan_invent, sv_data );

	/*
	 * Check if any DATA plexes were found.
	 */
	if (sv_data->num_plexes == 0) {
		ASSERT(sv_data->plex[0] == NULL);
		ret = 0;
		goto cleanup_label;
	} else {
		/*
		 * Choose plex to use.
		 */
		xlv_tab->num_subvols++;
		xlv_choose_plex( sv_data );
	}

	/*
	 * Check if any LOG plexes were found.
	 */
	if (sv_log->num_plexes == 0) {
		ASSERT(sv_log->plex[0] == NULL);
		sv_log->vol_p = NULL;
		v->log_subvol = NULL;
	} else {
		/*
		 * Choose plex to use.
		 */
		xlv_tab->num_subvols++;
		xlv_choose_plex( sv_log );
	}

	/*
	 * Check if any RT plexes were found.
	 */
	if (sv_rt->num_plexes == 0) {
		ASSERT(sv_rt->plex[0] == NULL);
		sv_rt->vol_p = NULL;
		v->rt_subvol = NULL;
	} else {
		/*
		 * Choose plex to use.
		 */
		xlv_tab->num_subvols++;
		xlv_choose_plex( sv_rt );
	}

	/*
	 * Now compute the derived fields of the xlv_tab.
	 */
	for (i = 0; i < XLV_TAB_UUID_RSVD; i++) {
               	/*
               	 * Assume that the subvol entry is valid 
		 * if there is a non-NULL vol_p pointer.
               	 */
               	if (XLV_SUBVOL_EXISTS(&xlv_tab->subvolume[i])) {
                       	xlv_tab->subvolume[i].subvol_size =
                       		xlv_tab_subvol_size (&xlv_tab->subvolume[i]);

			if ( xlv_tab->subvolume[i].num_plexes > 1 ) {
                       		xlv_tab->subvolume[i].block_map =
                        		xlv_tab_create_block_map(
						&xlv_tab->subvolume[i],
						NULL, NULL);
			}
		}
	}	

		
#ifdef XLV_BOOT_DEBUG
	xlv_print_debug(xlv_tab, xlv_tab_vol);
#endif

	/*
	 * Initialize the xlv driver.
	 */
	xlv_tab_set( xlv_tab_vol, xlv_tab, 0);

	/*
	 * Free block maps.
	 */
	for (i = 0; i < XLV_TAB_UUID_RSVD; i++) {
               	if ((XLV_SUBVOL_EXISTS(&xlv_tab->subvolume[i])) &&
		    (xlv_tab->subvolume[i].block_map)            ) {
				
			block_map_size = sizeof(xlv_block_map_t) +
                       	   (xlv_tab->subvolume[i].block_map->entries-1)*
			   sizeof(xlv_block_map_entry_t);

                       	kmem_free(xlv_tab->subvolume[i].block_map,
				block_map_size);
                       	xlv_tab->subvolume[i].block_map = NULL;
		}
	}

	/*
	 * Free data plex structures.
	 */
	for (i = 0; i < sv_data->num_plexes; i++) {
		if (( p = sv_data->plex[i]) != NULL ) {
			kmem_free(p, PLEX_SIZE(p->max_vol_elmnts));
			sv_data->plex[i] = NULL;
		}
	}

	/*
	 * Free log plex structures.
	 */
	for (i = 0; i < sv_log->num_plexes; i++) {
		if (( p = sv_log->plex[i]) != NULL ) {
			kmem_free(p, PLEX_SIZE(p->max_vol_elmnts));
			sv_log->plex[i] = NULL;
		}
	}

	/*
	 * Free rt plex structures.
	 */
	for (i = 0; i < sv_rt->num_plexes; i++) {
		if (( p = sv_rt->plex[i]) != NULL ) {
			kmem_free(p, PLEX_SIZE(p->max_vol_elmnts));
			sv_rt->plex[i] = NULL;
		}
	}

cleanup_label:

	kmem_free(xlv_tab, XLV_TAB_SIZE);
	kmem_free(xlv_tab_vol, sizeof(xlv_tab_vol_t));

	return( ret );
}

/*
 * xlv_boot_disk()
 *	Determine if the given physical disk has an XLV label.
 *	If so, return the name of the disk, else return NULL.
 */
int
xlv_boot_disk(dev_t pdev) 
{
	int				dev_bsize, part;
	char 				*buf;
	static char			rootvolname[XLV_NAME_LEN];
	dev_t				diskdev;
 	xlv_vh_disk_label_t             *label;
        xlv_vh_disk_label_entry_t       *lep;
	uint_t				st;
	int				is_rootvol;
	scsi_part_info_t                *part_info;
	vertex_hdl_t	vhdl;
	char		*vhname = EDGE_LBL_VOLUME_HEADER "/" EDGE_LBL_BLOCK;
	int		which_level;


	bzero( rootvolname, XLV_NAME_LEN );

	/* Given the current node, find the handle to the volume header */
	vhdl = pdev;

	/* go up 3 edges in the hwg to the 'disk' node */
	for (which_level = 0; which_level < 3; which_level++)
		if ((vhdl = hwgraph_connectpt_get(vhdl)) == GRAPH_VERTEX_NONE)
    			return 0;
		else		
			hwgraph_vertex_unref(vhdl);

	/* get block name for volume header */
	if (hwgraph_traverse(vhdl, vhname, &diskdev) != GRAPH_SUCCESS)
		return 0;
	hwgraph_vertex_unref(diskdev);
#ifdef XLV_BOOT_DEBUG
	{
	char devname[MAXDEVNAME];
	dev_to_name(diskdev, devname, sizeof(devname));
	printf("xlv: checking for root volume on %s\n", devname);
	}
#endif

	/*
	 * Attempt to read the XLV label from the disk.
	 */
	if ((buf = xlv_get_xlv_label(diskdev, &dev_bsize)) != NULL) {

		/*
	 	 * Get the current XLV label.
		 */
	      	if (label = xlv_get_current_label((xlv_vh_master_t *)buf,
						dev_bsize)) {
			vertex_hdl_t	part_vhdl;

	         	/*
		  	 * Check the disk partition
		  	 * for an XLV subvolume element.
		  	 */
			part_vhdl = hwgraph_connectpt_get(pdev);
			part_info =
				scsi_part_info_get(part_vhdl);
			part = SPI_PART(part_info);
			hwgraph_vertex_unref(part_vhdl);
			lep = &label->label_entry[part];

			if (uuid_is_nil(&(lep->disk_part.uuid), &st)) {
				/*
				 * The root device is not XLV.
				 */
				is_rootvol = 0;

			/* root partition is part of an XLV object */
			} else if (uuid_is_nil(&(lep->vol.volume_uuid), &st)) {
				/*
				 * The root device is part of an XLV object,
				 * but it is not a volume object. This is
				 * a potential problem.
				 */
				char *name, *typestr;
				char devname[MAXDEVNAME];

				if (!uuid_is_nil(&(lep->plex.uuid), &st)) {
					name = lep->plex.name;
					typestr = "PLEX";
				} else {
					name = lep->vol_elmnt.name;
					typestr = "VE";
				}
				dev_to_name(pdev, devname, sizeof(devname));
				cmn_err(CE_WARN,
	"Root %s (part %d) is also in standalone XLV %s %s\n",
					devname, part, typestr, name);
				is_rootvol = 0;	/* let boot process continue */

			} else {
				/*
				 * The root device is part of a volume. 
				 *
				 * 2/12/96 Root volume can be plexed but
				 * not striped or concatenated.
				 */
				is_rootvol = 1;
				strcpy(rootvolname, lep->vol.volume_name);
				if (lep->vol_elmnt.grp_size != 1) {
					cmn_err(CE_WARN,
	"Root volume %s is aggregate storage (%d disks); NOT SUPPORTED\n",
						rootvolname,
						lep->vol_elmnt.grp_size);
					is_rootvol = 0;
				}
			}
		}
		kmem_free(buf, LABEL_SIZE(dev_bsize));

		if (is_rootvol) {
			xlv_rootdisk = pdev;
			return( xlv_boot_startup( rootvolname ) );
		}
		/* Disk has an XLV label but it is not a root volume. */
	}

#ifdef XLV_BOOT_DEBUG
	{
	char devname[MAXDEVNAME];
	dev_to_name(diskdev, devname, sizeof(devname));
	printf("xlv: no root volume found on %s\n", devname);
	}
#endif

	/*
	 * The root device is not part of a XLV volume.
	 */
	return( 0 );

} /* end of xlv_boot_disk() */

#ifdef XLV_BOOT_DEBUG
xlv_print_debug(
	xlv_tab_t	 *xlv_tab,
	xlv_tab_vol_t	 *xlv_tab_vol)
{

	printf("BEFORE calling xlv_tab_set \n");
	printf("xlv_tab->num_subvols				= %d \n",
		xlv_tab->num_subvols);
	printf("xlv_tab->max_subvols				= %d \n",
		xlv_tab->max_subvols);
	printf("xlv_tab->subvolume[0].flags			= %x \n",
		xlv_tab->subvolume[0].flags);
	printf("xlv_tab->subvolume[0].subvol_type		= %x \n",
		xlv_tab->subvolume[0].subvol_type);
	printf("xlv_tab->subvolume[0].subvol_depth		= %x \n",
		xlv_tab->subvolume[0].subvol_depth);
	printf("xlv_tab->subvolume[0].subvol_size		= %x \n",
		xlv_tab->subvolume[0].subvol_size);
	printf("xlv_tab->subvolume[0].num_plexes		= %x \n",
		xlv_tab->subvolume[0].num_plexes);
	printf("xlv_tab->subvolume[0].dev			= %x \n",
		xlv_tab->subvolume[0].dev);
	printf("xlv_tab->subvolume[0].vol_p->name		= %s \n",
		xlv_tab->subvolume[0].vol_p->name);
	printf("xlv_tab->subvolume[0].vol_p->flags		= %x \n",
		xlv_tab->subvolume[0].vol_p->flags);
	printf("xlv_tab->subvolume[0].vol_p->sector_size	= %d \n",
		xlv_tab->subvolume[0].vol_p->sector_size);
	if (xlv_tab->subvolume[0].vol_p->data_subvol) {
		printf("data subvolume \n");

	printf("xlv_tab->subvolume[0].vol_p->data_subvol->flags		= %x\n",
		xlv_tab->subvolume[0].vol_p->data_subvol->flags);
	printf("xlv_tab->subvolume[0].vol_p->data_subvol->subvol_type	= %d\n",
		xlv_tab->subvolume[0].vol_p->data_subvol->subvol_type);
	printf("xlv_tab->subvolume[0].vol_p->data_subvol->subvol_depth	= %d\n",
		xlv_tab->subvolume[0].vol_p->data_subvol->subvol_depth);
	printf("xlv_tab->subvolume[0].vol_p->data_subvol->subvol_size	= %d\n",
		xlv_tab->subvolume[0].vol_p->data_subvol->subvol_size);
	printf("xlv_tab->subvolume[0].vol_p->data_subvol->dev		= %d\n",
		xlv_tab->subvolume[0].vol_p->data_subvol->dev);
	printf("xlv_tab->subvolume[0].vol_p->data_subvol->num_plexes	= %d\n",
		xlv_tab->subvolume[0].vol_p->data_subvol->num_plexes);
	}

	if (xlv_tab->subvolume[0].vol_p->log_subvol) {
		printf("log subvolume \n");

	printf("xlv_tab->subvolume[0].vol_p->log_subvol->flags		= %x\n",
		xlv_tab->subvolume[0].vol_p->log_subvol->flags);
	printf("xlv_tab->subvolume[0].vol_p->log_subvol->subvol_type	= %d\n",
		xlv_tab->subvolume[0].vol_p->log_subvol->subvol_type);
	printf("xlv_tab->subvolume[0].vol_p->log_subvol->subvol_depth	= %d\n",
		xlv_tab->subvolume[0].vol_p->log_subvol->subvol_depth);
	printf("xlv_tab->subvolume[0].vol_p->log_subvol->subvol_size	= %d\n",
		xlv_tab->subvolume[0].vol_p->log_subvol->subvol_size);
	printf("xlv_tab->subvolume[0].vol_p->log_subvol->dev		= %d\n",
		xlv_tab->subvolume[0].vol_p->log_subvol->dev);
	printf("xlv_tab->subvolume[0].vol_p->log_subvol->num_plexes	= %d\n",
		xlv_tab->subvolume[0].vol_p->log_subvol->num_plexes);
	}

	if (xlv_tab->subvolume[0].vol_p->rt_subvol) {
		printf("rt subvolume \n");

	printf("xlv_tab->subvolume[0].vol_p->rt_subvol->flags		= %x\n",
		xlv_tab->subvolume[0].vol_p->rt_subvol->flags);
	printf("xlv_tab->subvolume[0].vol_p->rt_subvol->subvol_type	= %d\n",
		xlv_tab->subvolume[0].vol_p->rt_subvol->subvol_type);
	printf("xlv_tab->subvolume[0].vol_p->rt_subvol->subvol_depth	= %d\n",
		xlv_tab->subvolume[0].vol_p->rt_subvol->subvol_depth);
	printf("xlv_tab->subvolume[0].vol_p->rt_subvol->subvol_size	= %d\n",
		xlv_tab->subvolume[0].vol_p->rt_subvol->subvol_size);
	printf("xlv_tab->subvolume[0].vol_p->rt_subvol->dev		= %d\n",
		xlv_tab->subvolume[0].vol_p->rt_subvol->dev);
	printf("xlv_tab->subvolume[0].vol_p->rt_subvol->num_plexes	= %d\n",
		xlv_tab->subvolume[0].vol_p->rt_subvol->num_plexes);
	}



	printf("xlv_tab_vol->num_vols				= %d \n",
		xlv_tab_vol->num_vols);
	printf("xlv_tab_vol->max_vols				= %d \n",
		xlv_tab_vol->max_vols);
	printf("xlv_tab_vol->vol[0].name			= %s \n",
		xlv_tab_vol->vol[0].name);
	printf("xlv_tab_vol->vol[0].sector_size			= %d \n",
		xlv_tab_vol->vol[0].sector_size);
	printf("xlv_tab_vol->vol[0].flags			= %x \n",
		xlv_tab_vol->vol[0].flags);

	if (xlv_tab_vol->vol[0].log_subvol) 	{
	printf("log volume \n");
	printf("xlv_tab_vol->vol[0].log_subvol->dev		= %x \n",
		xlv_tab_vol->vol[0].log_subvol->dev);
	}

	if (xlv_tab_vol->vol[0].data_subvol) 	{
	printf("data volume \n");
	printf("xlv_tab_vol->vol[0].data_subvol->dev		= %x \n",
		xlv_tab_vol->vol[0].data_subvol->dev);
	}

	if (xlv_tab_vol->vol[0].rt_subvol) 	{
	printf("rt volume \n");
	printf("xlv_tab_vol->vol[0].rt_subvol->dev		= %x \n",
		xlv_tab_vol->vol[0].rt_subvol->dev);
	}

}
#endif /* XLV_BOOT_DEBUG */

#endif /* ! USER_MODE */

