
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.11 $"

/* Subroutines dealing with the logical volume labels in the headers of
 * disks which are part of Logical Volumes.
 *
 * General convention note: apart from functions returning a pointer, 
 * all others return 0 for OK and -1 for error.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/lv.h>
#include "lvutils.h"

extern int errno;
extern char *malloc();
extern struct volmember memhead;
extern struct logvol    *loghead;
extern int readonly;
extern char *progname;

char *pathtovhpath();
char *pathtolabname();
struct partition_table *getvhpart();
struct headerinfo *vh_incore();
void purgefds();

/* checkvolpaths: goes through all the pathnames for a volume doing
 * accesscheck on each one. 
 */

checkvolpaths(vol)
register struct logvol *vol;
{
register int i;
int error = 0;

	for (i = 0; i < vol->tabent->ndevs; i++)
	{
		if (accesscheck(vol->vmem[i]) < 0) 
		{
			error = -1;
		}
	}
	if (error) 
		vol->status |= L_BADPATHNAME;
	return (error);
}

/* getvolheads: goes through all the pathnames for a volume getting the
 * volume headers for each one. 
 */
		
getvolheads(vol)
register struct logvol *vol;
{
register int i;
int error = 0;

	for (i = 0; i < vol->tabent->ndevs; i++)
	{
		if (getdvh(vol->vmem[i]) < 0) 
		{
			error = -1;
		}
	}
	return (error);
}

/* getlvlabs: goes through all the pathnames for a volume getting the
 * logical volume labels for each one. We detect two possible partial
 * cases:
 *  a) every member up to a given one has a label, and none of the ones after
 *     it do. This is an 'OK' case when extending an existing volume.
 *  b) Labels present/missing in any less orderly pattern. This is an indication
 *     of some problem.
 *
 */
		
getlvlabs(vol)
register struct logvol *vol;
{
register vmp *vmem;
register int i;
int labelsfound = 0;
int volpresent = 0;
int endseen = 0;
int noncontig = 0;
int gotlabel;


	for (i = 0; i < vol->tabent->ndevs; i++)
	{
		if (gotlabel = (getlabel(vol->vmem[i]) == 0)) 
		{
			labelsfound++;
			vol->hilabel = i;
		}
		if ((i == 0) && gotlabel) volpresent = 1;
		if (volpresent && !gotlabel) endseen = 1;
		if (endseen && gotlabel) noncontig = 1;
		if (!volpresent && gotlabel) noncontig = 1;
	}
	vol->labels = labelsfound;
	if (labelsfound == vol->tabent->ndevs)
	{
		vol->status |= L_GOTALL_LABELS;
		return (0);
	}
	if (noncontig) vol->status |= L_BADPARTIAL;
	else if (volpresent) vol->status |= L_GOODPARTIAL;

	return (-1);
}

/* check_dev_connections():
 * goes thru all known member devices checking that the actual current
 * device of the member is the same as the device recorded in its label
 * when it was initialized. If not, something is connected in the wrong
 * place; this MUST be corrected before any further logical volume actions..
 */

check_dev_connections(abort)
int abort;
{
register struct volmember *vmem;
int misconnect = 0;

	for (vmem = memhead.next; 
	     (vmem && (vmem != &memhead)); 
	     vmem = vmem->next)
	{
		if (!(vmem->status & GOT_LABEL)) continue;
		if (vmem->status & 
		(BAD_LABEL | CANT_ACCESS | WRONG_DEVTYPE | WRONG_DEV)) continue;
		if (vmem->dev != vmem->lvlab->mydev)
		{
			fprintf(stderr,"%s: device currently connected as ",
				progname);
			printpath(vmem->dev, 1, 0);
			fprintf(stderr,"\n was initialized when connected as ");
			printpath(vmem->lvlab->mydev, 1, 0);
			fprintf(stderr,"\naccording to its logical volume label.\n");
			misconnect = 1;
		}
	}
	if (misconnect) 
	{
		fprintf(stderr,"\nIf this device is part of a current logical volume, either the disk ID\n");
		fprintf(stderr,"must be corrected, or the volume remade, before that volume can be used.\n");
		if (abort)
			exit(-1);
	}
}

/* vol_labels_check: goes through all the members of a volume checking whether 
 * they do, according to their logical volume labels, constitute a 
 * consistent volume. 
 *
 * 'find_typical_label()' is called first if necessary to locate a
 * concensus label for the volume.
 * Members whose labels don't agree have their status marked to indicate
 * the nature of the problem.
 *
 * Note at this point we are checking just that the labels are consistent
 * among themselves: verifying that the constituted volume is the
 * one claimed in an lvtab entry is done by vol_to_tab_check().
 *
 */
		
vol_labels_check(vol)
register struct logvol *vol;
{
register struct volmember *vmem;
register int i, ndevs;
register struct lvlabel *lvlab;
register int labsize;
int error = 0;

	if ((vol->lvlab == NULL) && find_typical_label(vol))
		return (-1);		   /* typical_label couldn't find even
					    * ONE decent label... obviously
					    * no consistent volume present!
					    */

	lvlab = vol->lvlab;
	labsize = vol->labsize;
	ndevs = lvlab->lvdescrip.l_ndevs;

	for (i = 0; i < ndevs; i++)
	{
		vmem = vol->vmem[i];
		if (!vmem || (vmem->status != (GOT_HEADER | GOT_LABEL)))
		{
			vol->status |= L_MISSDEV;
			error = -1;
			continue;
		}
		if (vmem_to_lab_check(vmem, lvlab, labsize, 1))
		{
			vmem->status |= BAD_LABEL;
			error = -1;
		}
		else
			vol->goodlabels++;
	}
	if (ndevs != vol->labels) 
		error = -1;

	/* If ndevs for the volume is not yet set (ie we're working from
	 * disk labels rather than an lvtab entry), it should be set to
	 * the max of ndevs & labels_found, to ensure that the whole
	 * volume will be displayed on error...
	 */

	if (!vol->ndevs)
		vol->ndevs = (ndevs > vol->labels) ? ndevs : vol->labels;
	if (!error)
		vol->status |= L_GOODLABELS;
	return (error);

}

/* find_typical_label(): this scans the labels known for the members of a
 * volume to find a 'typical' label. (All of the info in the labels of a
 * consistent volume is replicated with the exception of the one field
 * recording the dev of the device to which the label refers).
 *
 * However, there is always the possibility of corrupted labels, or
 * misconnected devices! So what we must do is scan all the available
 * labels looking for a majority concensus.
 */

find_typical_label(vol)
register struct logvol *vol;
{
register struct volmember *vmem;
register struct lvlabel *curlab;
int curlabsize;
int ptype;
int bestmatchcount = 0;
int i, j, labels, matches;

	/* How many members should we scan? Depending on where this is 
	 * called from, the highest plausible member may be indicated 
	 * by ndevs, labels or hilabel. Take the max.
	 */

	labels = vol->labels;
	labels = (vol->ndevs > labels) ? vol->ndevs : labels;
	labels = ((vol->hilabel + 1) > labels) ? vol->hilabel + 1 : labels;
	vol->lvlab = NULL;
	for (i = 0; i < labels; i++)
	{
		vmem = vol->vmem[i];
		if (!vmem || (vmem->status != (GOT_HEADER | GOT_LABEL)))
			continue;
		
		curlab = vmem->lvlab;
		curlabsize = vmem->labsize;
		ptype = partype(vmem);
		for (j = 0, matches = 0; j < labels; j++)
		{
			vmem = vol->vmem[j];
			if (!vmem || (vmem->status != (GOT_HEADER | GOT_LABEL)))
				continue;

			if (vmem_to_lab_check(vmem, curlab, curlabsize, 0) == 0)
				matches++;
		}
		if (matches > bestmatchcount)
		{
			bestmatchcount = matches;
			vol->lvlab = curlab;
			vol->labsize = curlabsize;
			vol->ptype = ptype;
		}
		if (matches == vol->labels) break; /* all agree, no need to
						    * go on! 
						    */
	}
	if (vol->lvlab) 
		return (0);
	else
		return (-1);
}

/* vmem_to_lab_check(): checks if the label of the given volmember is
 * consistent with the given test label. If 'mark' is set and they are not
 * consistent, the vmem status is marked with a value indicating the problem.
 */

vmem_to_lab_check(vmem, lvlab, labsize, mark)
register struct volmember *vmem;
register struct lvlabel *lvlab;
int labsize;
int mark;
{
register int i;
register char *name, *volid;
register char *firstpath;
register struct lv *lvp, *tlvp;
register int comparesize, ndevs;

	name = lvlab->volname;
	volid = lvlab->volid;
	lvp = &lvlab->lvdescrip;
	ndevs = lvp->l_ndevs;

	comparesize = LVSIZE(ndevs);

	if (strlen(name) && !streq(name, vmem->lvlab->volname))
	{
		if (mark)
			vmem->status |= WRONG_VOLUME;
		return (-1);
	}
	if (!streq(volid, vmem->lvlab->volid))
	{
		if (mark)
			vmem->status |= WRONG_VOLUME;
		return (-1);
	}
	if (vmem->labsize != labsize)
	{
		if (mark)
			vmem->status |= BAD_LABEL;
		return (-1);
	}
	tlvp = &vmem->lvlab->lvdescrip;
	if (bcmp((caddr_t)lvp, (caddr_t)tlvp, comparesize))
	{
		if (mark)
			vmem->status |= BAD_LABEL;
		return (-1);
	}
	return (0);
}

/* vol_devs_check: goes through volume members checking that the partition type 
 * is PTYPE_LVOL, and that the partition size agrees with the label info.
 *
 * It is assumed that 'find_typical_label()' has been called to locate a
 * concensus label for the volume. (If that's not available we have no
 * volume info to work from, so error out at once).
 */

vol_devs_check(vol)
register struct logvol *vol;
{
register struct volmember *vmem;
register struct lvlabel *lvlab;
register struct lv *lvp;
register struct physdev *pd;
register int i, ptype, psize;
register int ndevs;
int error = 0;

	if ((lvlab = vol->lvlab) == NULL)
		return (-1);	
	lvp = &lvlab->lvdescrip;
	ndevs = lvp->l_ndevs;
	pd = lvp->pd;

	for (i = 0; i < ndevs; i++, pd++)
	{
		vmem = vol->vmem[i];
		if (!vmem || (vmem->status != (GOT_LABEL | GOT_HEADER)))
		{
			vol->status |= L_MISSDEV;
			error = -1;
			continue;
		}

		if (vmem->dev != pd->dev)
		{
			vmem->status |= BAD_LABEL;
				error = -1;
		}

		ptype = partype(vmem);
		switch (ptype)
		{
		case PTYPE_LVOL  :
		case PTYPE_RLVOL :  break;

		case PTYPE_VOLHDR :
		case PTYPE_VOLUME :

			{
				vmem->status |= ILLEGAL_PTYPE;
				error = -1;
				break;
			}
		default :
			{
				vmem->status |= WRONG_PTYPE;
				error = -1;
				break;
			}
		}; /* end ptype switch */

	/* Now the size check. This depends on striping: if not striping, the
	 * size in the physdev should be exactly the partition size. But if
	 * striping, it should have been rounded down to a multiple of the
	 * granularity.
	 */

		psize = partsize(vmem);	
		if (lvp->l_stripe > 1)
			psize -= (psize % lvp->l_gran);
		if (psize != pd->size)
		{
			vmem->status |= WRONG_PSIZE;
			error = -1;
		}
	}
	if (!error)
		vol->status |= L_DEVSCHEKD;
	return (error);
}

/* vol_to_tab_check(): checks that the volume found from the disk labels
 * agrees with the parameters given in the lvtab entry.
 * 
 * We compare with the 'typical' label stored for the volume. (Inconsistencies
 * within the volume labels will have been detected earlier).
 *
 * If 'partial' is set, a smaller # of devs in the on-disk volume than
 * the lvtab is acceptable. (A greater # is always an error, of course).
 *
 * Also checks the striping geometry specified in the lvtab entry
 * against the on-disk label, and if a human-assigned name is present in the
 * lvtab entry, that must agree with the name in the labels.
 */

vol_to_tab_check(vol, partial)
register struct logvol *vol;
{
register struct lv *lvp;
register struct lvtabent *lvt;
register char *name;
register unsigned ndevs;

	lvt = vol->tabent;
	if (name = vol->tabent->volname)
	{
		if (!streq(name, vol->lvlab->volname))
		{
			fprintf(stderr,"%s: volume name '%s' in lvtab entry\n",
				progname, name);
			fprintf(stderr,"disagrees with name '%s' in on-disk labels\n", vol->lvlab->volname);
		 	return (-1);
		}
	}

	lvp = &vol->lvlab->lvdescrip;
	ndevs = lvp->l_ndevs;

	if (ndevs > lvt->ndevs)
	{
		fprintf(stderr,"%s: Number of devs in lvtab entry %s is greater \n", progname, vol->tabent->devname);
		fprintf(stderr,"than number %d in on-disk labels.\n", ndevs);
		return (-1);
	}
	else if ((ndevs < lvt->ndevs) && !partial)
	{
		fprintf(stderr,"%s: Number of devs in lvtab entry %s is less \n", progname, vol->tabent->devname);
		fprintf(stderr,"than number %d in on-disk labels.\n", ndevs);
		return(-1);
	}

	if (lvp->l_stripe != lvt->stripe)
	{
		fprintf(stderr,"%s: stripes specified in lvtab entry for %s \n", progname, lvt->devname);
		fprintf(stderr,"don't agree with on-disk label\n");
		return (-1);
	}
	if (lvt->gran && (lvt->gran != lvp->l_gran))
	{
		fprintf(stderr,"%s: step specified in lvtab entry for %s \n", progname, lvt->devname);
		fprintf(stderr,"doesn't agree with on-disk label\n");
		return (-1);
	}
	if (lvp->l_stripe > 1)
	{
		if ((lvp->l_gran == 0) || (lvp->l_gran > MAXLVGRAN))
		{
			fprintf(stderr,"%s: bad step in label for %s\n",
				progname, lvt->devname);
			return (-1);
		}
	}
	return (0);
}

/* makelabels(): for each member of the vol, make the appropriate new Logical
 * Volume label to go in its header. It is assumed that accesscheck has
 * succeeded: ie all the member devs are of correct type & accessible.
 * It is also assumed that buildlv has succeeded: ie the logvol struct
 * now possesses a correct struct lv. It is also assumed that getvolheads has 
 * succeeded: ie the header info for each member is in core.
 * Also that we have a volid: either generated for a new volume, or obtained
 * from the earlier part if extending.
 * All we do here is allocate the memory for each label, copy in the lv struct,
 * and put in the name (if present), volid, dev and magic number.
 */

void
makelabels(vol)
register struct logvol *vol;
{
register struct volmember *vmem;
register struct lvlabel *lvlab;
register int i;
int labelsize;
int dev_bsize;

	vmem = vol->vmem[0];
	dev_bsize = vmem->vhinfo->dev_bsize;
	labelsize = sizeof (struct lvlabel) + ((vol->ndevs -1) * sizeof (struct
						physdev));

	/* The labelsize to write on the header must be a multiple of the
	 * device blocksize.
	 */

	labelsize = roundblk(labelsize, dev_bsize);

	for (i = 0; i < vol->ndevs; i++)
	{
		vmem = vol->vmem[i];
		if ((lvlab = (struct lvlabel *)malloc(labelsize)) == NULL)
		{
			fprintf(stderr,"%s: can't allocate memory!\n", progname);
			exit(1);
		}
		lvlab->lvmagic = LV_MAGIC;
		lvlab->mydev = vmem->dev;
		bzero(lvlab->volname, MAXVNAMELEN);
		if (vol->name)
			strcpy(lvlab->volname, vol->name);
		bzero(lvlab->volid, MAXVNAMELEN);
		strcpy(lvlab->volid, vol->volid);
		bcopy(vol->lv, &lvlab->lvdescrip, LVSIZE(vol->ndevs));
		vmem->newlvlab = lvlab;
		vmem->newlabsize = labelsize;
	}
}

/* buildlv(): from the member structs and the given striping info, construct 
 * the struct lv for the volume. It is assumed that checkvolpaths & 
 * getvolheads have succeeded previously: ie all the members have valid 
 * pathnames, are accessible, and have their volume headers in core.
 * At this point, the only error we can encounter is to find, if striping,
 * that members which are supposed to be part of a stripegroup are NOT
 * the same size. Note that a requirement in earlier versions that tracksize be
 * identical has been relaxed.
 */

buildlv(vol, stripes, gran)
register struct logvol *vol;
int stripes, gran;
{
register struct volmember *vmem;
register struct headerinfo *vhinfo;
register struct lv *lv;
register struct physdev *pd;
register int i, j, stripegroups, groupsize, groupstart;
int member = 0;
int total = 0;
int error = 0;

	if ((lv = (struct lv *)malloc(LVSIZE(vol->ndevs))) == NULL)
	{
		fprintf(stderr,"%s: can't allocate memory!\n", progname);
		exit(1);
	}
	lv->l_flags = 0;
	lv->l_ndevs = vol->ndevs;
	lv->l_stripe = stripes;
	lv->l_align = vol->vmem[0]->vhinfo->trksize;

	if (stripes > 1)
	{
		if (gran) lv->l_gran = gran;
		else gran = lv->l_gran = lv->l_align;

		stripegroups = vol->ndevs / stripes;

		for (i = 0; i < stripegroups; i++)
		{
			groupsize = partsize(vol->vmem[member]);
			groupstart = total;
			for (j = 0; j < stripes; j++, member++)
			{
				pd = &lv->pd[member];
				vmem = vol->vmem[member];
				vhinfo = vmem->vhinfo;
				if (groupsize != partsize(vmem))
				{
					error = 1;
					vmem->status |= WRONG_PSIZE;
				}
				pd->dev = vmem->dev;
				pd->size = groupsize - (groupsize % gran);
				pd->start = groupstart + (j * gran);
				total += pd->size;
			}
		}
	}
	else
	{
		for (member = 0; member < vol->ndevs; member++)
		{
			pd = &lv->pd[member];
			vmem = vol->vmem[member];
			pd->dev = vmem->dev;
			pd->size = partsize(vmem);
			pd->start = total;
			total += pd->size;
		}
	}		
	lv->l_size = total;
	vol->lv = lv;
	if (error) 
		return (-1);
	else
		return (0);
}

/* comparelv(): compares two (in-core) lv structures. 
 * Used to check an on-disk label against a newly constructed one. 
 * If partial is set, it is acceptable for the on-disk label to be a subset 
 * of the constructed one (for the case where mklv is extending a 
 * logical volume).
 */

comparelv(newlv, disklv, partial)
register struct lv *newlv, *disklv;
int partial;
{
register struct physdev *npd, *dpd;
register int i, ndevs;

	ndevs = disklv->l_ndevs;
	if (!partial)
	{
		if (newlv->l_ndevs != ndevs) return (-1);
		if(bcmp((caddr_t)newlv, (caddr_t)disklv, LVSIZE(ndevs)))
			return (-1);
	}
	else
	{
		if (newlv->l_stripe != disklv->l_stripe) return (-1);
		if (newlv->l_gran != disklv->l_gran) return (-1);
		if (newlv->l_align != disklv->l_align) return (-1);
		for (i = 0, npd = &newlv->pd[0], dpd = &disklv->pd[0]; 
		     i < ndevs; 
		     i++, npd++, dpd++)
		{
			if (npd->dev != dpd->dev) return (-1);
			if (npd->size != dpd->size) return (-1);
			if (npd->start != dpd->start) return (-1);
		}
	}
	return (0);
}

/* getdvh: reads the volume header for the disk on which the given member
 * resides. It is assumed that 'pathname' of the member is set!!
 *
 * We use the IOCTL to get the header from the driver rather than reading
 * directly from the disk because of the problem discovered with some
 * older SCSI disks that had bogus header info; the SCSI driver
 * sanity-checks it for us. Note that membfd() always opens the raw device.
 */

getdvh(vmem)
register struct volmember *vmem;
{
register struct headerinfo *vhinfo;
register struct volume_header *vh;
register int vfd;

	/* if member status indicates a problem with the pathname 
	 * there is no point in proceeding 
	 */

	if (vmem->status & BAD_NAME) return (-1);

	/* It is legal for more than one partition of a disk to be part
	 * of a logical volume (or even of different volumes, though this
	 * is not encouraged)!
	 * Thus, the volume header for this partition may already be 
	 * in core. We do NOT want multiple copies, since this would
	 * create inconsistencies if writing...
	 */

	if (vhinfo = vh_incore(vmem->pathname)) 
	{
		vmem->vhinfo = vhinfo;
		vmem->status |= GOT_HEADER;
		return (0);
	}

	/* So this is the first access to this header. Allocate a 
	 * headerinfo structure for it.
	 */

	vmem->vhinfo = vhinfo = 
		(struct headerinfo *)malloc(sizeof(struct headerinfo));
	if (!vhinfo) 
	{
		fprintf(stderr,"%s: can't allocate memory!\n", progname);
		exit(1);
	}

	vhinfo->vfd = -1;  	/* obviously not open yet! */
	vh = &vhinfo->vh;	/* for speed & legibility */

	if ((vfd = membfd(vmem)) < 0) return (-1);

	if(ioctl(vfd, DIOCGETVH, (caddr_t)vh) == -1) 
	{
		close(vfd);
		free((caddr_t)vhinfo);
		vmem->vhinfo = NULL;
		return (-1);
	}

	vhinfo->firstfileblk = 2; /* match fx and dvhtool */
	vhinfo->trksize = 128;  /* reasonable default, and that's all this
		is used for */
	vhinfo->dev_bsize = vh->vh_dp.dp_secbytes;
	if(vhinfo->dev_bsize == 0)	/* a lot of older disks had their
					/* secbytes set to 0, and it wasn't 
					 * noticed because nothing used it! 
					 */
		vhinfo->dev_bsize = DEV_BSIZE;

	vhinfo->vfd = vfd;
	vmem->status |= GOT_HEADER;
	vmem->ptype = partype(vmem);
	return 0;
}

/* Check whether the volume header for the given pathname is already in
 * core, return a pointer to the relevent headerinfo if so. All volmember 
 * structs in use are linked on to a circular list rooted at memhead. 
 * We traverse this looking for any entry whose header partition evalates 
 * to the same as that of the current pathname.
 */

struct headerinfo *
vh_incore(pname)
register char *pname;
{
register struct volmember *vmem;
char *head;
char headname[MAXDEVPATHLEN];

	/* First, find the pathname of the header for the given pathname.
	 * pathtovhpath() returns a pointer to static data, so we must
	 * save it. 
	 */

	head = pathtovhpath(pname);
	strncpy(headname, head, strlen(head));
	headname[strlen(head)] = 0x00;
	
	vmem = memhead.next;

	while (vmem && (vmem != &memhead))
	{
		if (streq(headname, pathtovhpath(vmem->pathname)) &&
		    vmem->vhinfo)
			return (vmem->vhinfo);
		else vmem = vmem->next;
	}
	return (NULL);
}

/* putdvh: writes the modified disk volume header to disk for the given member.
 * This is called from writelabel, so we may assume that the volume
 * header IS in core and its file descriptor is open.
 * Also, since the logical volume tools work from existing headers and
 * change only the logvol label files in the volume directory (& the 
 * partition type tags), much of the checking that dvhtool does is not 
 * needed here.
 */

putdvh(vmem)
register struct volmember *vmem;
{
register struct volume_header *vh;
register int i, vfd, dev_bsize;
int error = 0;

	vh = &vmem->vhinfo->vh;
	vfd = vmem->vhinfo->vfd;
	dev_bsize = vmem->vhinfo->dev_bsize;

	/*
	 * calculate vh_csum
	 */
	vh->vh_magic = VHMAGIC;
	vh->vh_csum = 0;
	vh->vh_csum = vh_checksum(vh);

	/* write new volhdr to disk, and to driver.  Now goes *ONLY* to
	 * sector 0 on drive, unlike old days when it went to first sector
	 * of each track in cylinder 0.  We never used the spares, and needed
	 * additional space in the volhdr... */
	if(ioctl(vfd, DIOCSETVH, (caddr_t)vh) == 0 && lseek(vfd, 0, 0) == 0 &&
		write(vfd, (caddr_t)vh, dev_bsize) == dev_bsize)
			return 0;
	fprintf(stderr,"%s: write of disklabel for %s failed: %s\n", progname,
		vmem->pathname, strerror(errno));
	return -1;
}


/* perform checks on the given pathname to verify that it really is a
 * block device and that device is accessible..
 *
 */

accesscheck(vmem)
register struct volmember *vmem;
{
struct stat s;
int fd;
int error = 0;

	if ((fd = open(vmem->pathname, (readonly) ? O_RDONLY : O_RDWR)) < 0)
	{
		vmem->status |= CANT_ACCESS;
		error = -1;
	}
	if (fstat(fd, &s) < 0)
	{
		close(fd);
		vmem->status |= CANT_ACCESS;
		error = -1;
	}
	if ((s.st_mode & S_IFMT) != S_IFBLK)
	{
		close(fd);
		vmem->status |= WRONG_DEVTYPE;
		error = -1;
	}
	close(fd);
	if (!error)
		vmem->dev = s.st_rdev;
	return (error);
}


/*
 * delete a volume directory entry from the given header.
 */
vd_delete(dvh_file, vh)
register char *dvh_file;
register struct volume_header *vh;
{
	register int i;

	for (i = 0; i < NVDIR; i++) {
		if (vh->vh_vd[i].vd_nbytes
		    && streq(vh->vh_vd[i].vd_name, dvh_file)) {
			/* different programs look at different things... */
			vh->vh_vd[i].vd_nbytes = 0;
			vh->vh_vd[i].vd_lbn = -1;
			*vh->vh_vd[i].vd_name = '\0';
			sortdir(vh->vh_vd); /* keep in sorted order just for 
					     * appearances, and in case any 
					     * older programs counted on it. 
					     */
			break;
		}
	}
}

/*
 *  Write the new logical volume label for the given member.
 *  It is assumed that the new label has already been created, and that the
 *  member pathname is set.
 *  Side effects: it pushes the volume header out to disk too, and sets the
 *  partition tag to PTYPE_LVOL or PTYPE_RLVOL dpending on 'raw'.
 */

writelabel(vmem, raw)
register struct volmember *vmem;
int raw;
{
	register char *labelname;
	register struct volume_header *vh;
	register int i;
	register int empty;
	register int bytes;
	register int vfd;
	register int dev_bsize;
	daddr_t lbn;
	unsigned offset;
	register int part;
	struct partition_table *pt;

	if (!vmem->vhinfo)
	{
		if (getdvh(vmem) < 0) return (-1);
	}
	else if ((vfd = membfd(vmem)) < 0) return (-1);
	
	vh = &vmem->vhinfo->vh;
	dev_bsize = vmem->vhinfo->dev_bsize;

	/* delete first in case replacing */

	labelname = pathtolabname(vmem->pathname);
	(void)vd_delete(labelname, vh);

	/* Now some belt&braces safety: some older versions of dvhtool
	 * appear to leave bogus zero-length directory entries when asked
	 * to delete an entry. This confuses the allocation code; we'll do a
	 * cleaning pass here to force any such semi-allocated entries
	 * to be properly clear!
	 */

	for (i = 0; i < NVDIR; i++) 
	{
		if (vh->vh_vd[i].vd_nbytes == 0)
		{
			vh->vh_vd[i].vd_lbn = -1;
			*vh->vh_vd[i].vd_name = '\0';

		}
	}
	sortdir(vh->vh_vd); /* keep in sorted order just for appearances */

	/* Now allocate a directory slot. */

	empty = -1;
	for (i = 0; i < NVDIR; i++) 
	{
		if (empty < 0 && vh->vh_vd[i].vd_nbytes == 0)
		{
			empty = i;
			break;
		}
	}
	if (empty == -1) {
		fprintf(stderr,"%s: Can't write label for %d, disk header directory full\n",progname, vmem->pathname);
		return (-1);
	}

	bytes = roundblk(vmem->newlabsize, dev_bsize); /* round up to blks */

	/* since logvol labels are small, we should not have any problem
	 * finding contiguous space for them; we don't need to go through
	 * all the excruciating copy-off/copy-on stuff that dvhtool has to
	 * do in order to handle large objects...
	 */

	if((lbn = vh_contig(vmem, bytes)) == -1) 
	{
		fprintf(stderr,"%s: Can't write label for %d, disk header partition full\n",progname, vmem->pathname);
		return(-1);
	}

	offset = lbn * dev_bsize;
	lseek(vfd, offset, 0);
	if (write(vfd, (caddr_t)vmem->newlvlab, bytes) != bytes)
	{
		fprintf(stderr,"%s: write of label for %s failed\n", 
			progname, vmem->pathname);
		return (-1);
	}

	strcpy(vh->vh_vd[empty].vd_name, labelname);
	vh->vh_vd[empty].vd_nbytes = bytes;
	vh->vh_vd[empty].vd_lbn = lbn;
	sortdir(vh->vh_vd); /* keep in sorted order just for appearances,
		and in case any older programs counted on it. */

	vh->vh_pt[pathtopart(vmem->pathname)].pt_type = 
		raw ? PTYPE_RLVOL: PTYPE_LVOL;
	if (putdvh(vmem) < 0) return (-1);
	return(0);
}

/*
 * copy in a logical volume label from volume header for the given member. 
 * It is assumed that (at least) the pathname of the member is set.
 */

getlabel(vmem)
struct volmember *vmem;
{
register char *labelname;
register struct volume_header *vh;
register struct lvlabel *lvlp;
register struct physdev *pd;
register int i, bytes, offset, vfd, dev_bsize;

	labelname = pathtolabname(vmem->pathname);

	if (!vmem->vhinfo)
	{
		if (getdvh(vmem) < 0) return (-1);
	}
	if ((vfd = membfd(vmem)) < 0) return (-1);
	
	vh = &vmem->vhinfo->vh;
	dev_bsize = vmem->vhinfo->dev_bsize;

	for(i = 0; i < NVDIR; i++) {
		if (vh->vh_vd[i].vd_nbytes
		    && streq(vh->vh_vd[i].vd_name, labelname)) {
			break;
		}
	}
	if(i == NVDIR)
	{
		return (-1);
	}
	bytes = roundblk(vh->vh_vd[i].vd_nbytes, dev_bsize);
	offset = vh->vh_vd[i].vd_lbn * dev_bsize; 
	if ((vmem->lvlab = (struct lvlabel *)malloc(bytes)) == NULL)
	{
		fprintf(stderr,"%s: can't allocate memory!\n", progname);
		exit(1);
	}
	(void)lseek(vfd, offset, 0); 
	if (read(vfd, (caddr_t)vmem->lvlab, bytes) != bytes)
	{
		free((caddr_t)vmem->lvlab);
		vmem->lvlab = NULL;
		close(vfd);
		vmem->vhinfo->vfd = -1;
		vmem->status |= UNREADABLE_LABEL;
		return (-1);
	}

	vmem->status |= GOT_LABEL;
	vmem->labsize = bytes;

	/* Now some internal consistency checks to try to detect
	 * corrupted labels. Magic number must be right, ndevs must be
	 * reasonable, the system-assigned id may not be null, the
	 * 'own device' indicated by mydev must be one of the devs in
	 * the logical volume, and the volid (and volname if present)
	 * must be of legal length.
	 */

	if (vmem->lvlab->lvmagic != LV_MAGIC)
	{ 
		vmem->status |= BAD_LABEL;
		return (-1);
	}
	if (vmem->lvlab->lvdescrip.l_ndevs > MAXLVDEVS)
	{ 
		vmem->status |= BAD_LABEL;
		return (-1);
	}
	if (strlen(vmem->lvlab->volid) == 0)
	{ 
		vmem->status |= BAD_LABEL;
		return (-1);
	}
	for (i = 0, pd = vmem->lvlab->lvdescrip.pd; 
	    i < vmem->lvlab->lvdescrip.l_ndevs; 
	    i++, pd++)
	{
		if (vmem->lvlab->mydev == pd->dev)
		{
			vmem->devindex = i;
			break;
		}
	}
	if (i == vmem->lvlab->lvdescrip.l_ndevs) /* didn't find a match */
	{ 
		vmem->status |= BAD_LABEL;
		return (-1);
	}
	if (strlen(vmem->lvlab->volid) >= MAXVNAMELEN)
	{ 
		vmem->status |= BAD_LABEL;
		return (-1);
	}
	if (strlen(vmem->lvlab->volname) >= MAXVNAMELEN)
	{ 
		vmem->status |= BAD_LABEL;
		return (-1);
	}
	return (0);
}

/* partype: return the partition type currently set for the member */

partype(vmem)
struct volmember *vmem;
{
register struct volume_header *vh;
register int vfd;

	if (!vmem->vhinfo)
	{
		if (getdvh(vmem) < 0) return (-1);
	}
	vh = &vmem->vhinfo->vh;

	return(vh->vh_pt[pathtopart(vmem->pathname)].pt_type); 
}

/* partsize: return the partition size (in blocks) for the member */

partsize(vmem)
struct volmember *vmem;
{
register struct volume_header *vh;
register int vfd;

	if (!vmem->vhinfo)
	{
		if (getdvh(vmem) < 0) return (-1);
	}
	vh = &vmem->vhinfo->vh;

	return(vh->vh_pt[pathtopart(vmem->pathname)].pt_nblks); 
}


vh_checksum(vhp)
register struct volume_header *vhp;
{
	register int csum;
	register int *ip;

	csum = 0;
	for (ip = (int *)vhp; ip < (int *)(vhp+1); ip++)
		csum += *ip;
	return(-csum);
}

/*	figure out which partition is the volume header.
 */

struct partition_table *
getvhpart(vh)
register struct volume_header *vh;
{
	struct partition_table *vhpt = NULL, *pt;
	register int i;

	pt = vh->vh_pt;
	for (i = 0; i < NPARTAB; i++, pt++) {
		if(!vhpt && pt->pt_nblks && pt->pt_type == PTYPE_VOLHDR) {
			vhpt = pt;
			break;
		}
	}
	if(!vhpt)
		fprintf(stderr,"%s: no entry for volume header in partition table\n",
			progname);
	return vhpt;
}


/*	used to sort the volume directory on lbn.  Sort so that zero and
 *	negative #'s sort to highest position.  These are invalid lbn values,
 *	and indicate an unused slot or a partially completed entry.
 */

lbncmp(v0, v1)
register struct volume_directory *v0, *v1;
{
	if(v0->vd_lbn <= 0)
		return 1;
	else if(v1->vd_lbn <= 0)
		return -1;
	return v0->vd_lbn - v1->vd_lbn;
}

sortdir(v)
struct volume_directory *v;
{
	qsort((char *)v, NVDIR, sizeof(v[0]), lbncmp);
}


/*	Determine if there is a contiguous chunk big enough in the volume 
 *	header. 
 *	Return its lbn if there is, otherwise return -1.  
 *	Note it is assumed that:
 *	a) the volume header info is in core, and
 *	b) the volume directory is sorted by blockno.
 */

vh_contig(vmem, size)
register struct volmember *vmem;
int size;
{
	register struct volume_directory *v, *vd0, *vd1;
	register struct volume_header *vh;
	register struct partition_table *vhpt;
	register int reqblks, fileblks, dev_bsize, filestart, firstfileblk;

	vh = &vmem->vhinfo->vh;
	dev_bsize = vmem->vhinfo->dev_bsize;
	firstfileblk = vmem->vhinfo->firstfileblk;

	reqblks = bytoblks(size, dev_bsize);
	v = vh->vh_vd;
	if(v->vd_lbn <= 0)	/* empty directory: legal tho unlikely! */
		return (firstfileblk);
	for(vd0=v, vd1=vd0+1; vd1->vd_lbn > 0 && vd1 < &v[NVDIR]; vd1++) {
		fileblks = bytoblks(vd0->vd_nbytes, dev_bsize);
		filestart = vd0->vd_lbn;
		if (filestart < firstfileblk) filestart = firstfileblk;
		if((filestart + fileblks + reqblks) <= vd1->vd_lbn)
			return (filestart + fileblks);
		vd0 = vd1;
	}

	/* check for enough between space between last entry and end of
	 *partition 
	 */

	vhpt = getvhpart(vh);
	if (!vhpt)
		return (-1);
	fileblks = bytoblks(vd0->vd_nbytes, dev_bsize);
	if(vhpt->pt_nblks >= (vd0->vd_lbn + fileblks + reqblks))
		return (vd0->vd_lbn + fileblks);
	else
		return (-1);
}


/* round up to dev_bsize multiple */
roundblk(cnt, dev_bsize)
register int cnt, dev_bsize;
{
	cnt = (cnt + dev_bsize -1) / dev_bsize;
	cnt *= dev_bsize;
	return cnt;
}

/* return # of dev_bsize blox needed to hold cnt */

bytoblks(cnt, dev_bsize)
register int cnt, dev_bsize;
{
	return (roundblk(cnt, dev_bsize) / dev_bsize);
}

/**************************************************************************
 * File descriptor S/Rs. We attempt to keep open file descriptors for all
 * volume headers of interest, to avoid opening and closing. However, 
 * since there may be many member devices in a logical volume
 * (and some utilities need to deal with multiple volumes), we may run out.
 * In that case, we scan the member chain closing open descriptors.
 ***************************************************************************/

int nfds = 0;

int
membfd(vmem)
struct volmember *vmem;
{
int vfd;

	if (vmem->vhinfo->vfd > 2) return (vmem->vhinfo->vfd);
	else
	{
		if (nfds > MAXFDS) purgefds();
		if ((vfd = open(pathtovhpath(vmem->pathname),
			       (readonly) ? O_RDONLY : O_RDWR)) < 0)
		{
			vmem->vhinfo->vfd = -1;
			return (-1);
		}
		else 
		{
			vmem->vhinfo->vfd = vfd;
			nfds++;
		}
		return (vfd);
	}
}

void
purgefds()
{
register struct volmember *vmem;

	vmem = memhead.next;

	while (vmem && (vmem != &memhead))
	{
		if (vmem->vhinfo && (vmem->vhinfo->vfd > 2)) 
		{
			close(vmem->vhinfo->vfd);
			vmem->vhinfo->vfd = -1;
			nfds--;
		}
		vmem = vmem->next;
	}
}

/*****************************************************************************/

reply(p)
char *p;
{
char buf[10];

	buf[0] = 0x00;
	printf("%s? (y/n) ",p);
	fflush(stdout);
	fgets(buf, 10, stdin);
	putchar('\n');
	fflush(stdout);
	switch (buf[0]) 
	{

		case 'y':
		case 'Y': return (0);
		default : return (-1);
	};
}

/* printbadvol(): prints volume in a form similar to an lvtab entry.
 * Members with problems get a note appended to their pathnames.
 * Print is done on the file pointer passed in as efile.
 * 'new' tells if this is a new vol being mklv'd, if so we don't complain
 * about missing labels.
 */

printbadvol(vol, efile, new)
register struct logvol *vol;
FILE *efile;
int new;
{
register struct volmember *vmem;
register struct lv *lv;
register struct lvlabel *lvlab;
register struct lvtabent *tabent;
register struct headerinfo *vhinfo;
register unsigned ndevs, i;
int rogues = 0;
int members = 0;

	ndevs = vol->ndevs;
	if (!ndevs) 
	{
		ndevs = vol->labels;
		for(i = 0; i < ndevs; i++)
			if (vol->vmem[i])
				members++;
		if (!members)
			return 0;
	}
		
	lvlab = vol->lvlab;
	tabent = vol->tabent;

	if (lvlab)
		fprintf(efile,"\n# Volume id:  %s\n", lvlab->volid);
		
	if (tabent)
	{
		fprintf(efile,"%s:", tabent->devname);
		if (tabent->volname && strlen(tabent->volname))
			fprintf(efile,"%s:", tabent->volname);
		else
			fprintf(efile,":");
		if (tabent->stripe > 1)
		{
			fprintf(efile,"stripes=%d:", tabent->stripe);
			if (tabent->gran) 
				fprintf(efile,"step=%d:", tabent->gran);
		}
		fprintf(efile,"devs= \\\n");
	}
	else if (lvlab)
	{
		lv = &lvlab->lvdescrip;
		fprintf(efile,"lv?:");
		if (lvlab->volname && strlen(lvlab->volname))
			fprintf(efile,"%s:", lvlab->volname);
		else
			fprintf(efile,":");
		if (lv->l_stripe > 1)
		{
			fprintf(efile,"stripes=%d:", lv->l_stripe);

		/* to decide whether to print a 'step', we need to know
		 * whether the granularity differs from the track size; so
		 * we need volume header info for the first device. 
		 * If that's not present, we'll print the step anyway,
		 * to be conservative. 
		 */
			if (vol->vmem[0] && vol->vmem[0]->vhinfo)
				vhinfo = vol->vmem[0]->vhinfo;
			else
				vhinfo = NULL;
			if (!vhinfo || (lv->l_gran != vhinfo->trksize))
				fprintf(efile,"step=%d:", lv->l_gran);
		}
		fprintf(efile,"devs= \\\n");
	}
	else
	{
		fprintf(efile,"\n# The following devices may be or have been part of a logical volume\n");
		fprintf(efile,"# but there is insufficient information to recover its identity.\n");
	}


	for(i = 0; i < ndevs; i++)
	{
		fprintf(efile,"\t");
		vmem = vol->vmem[i];
		if (vmem)
			fprintf(efile,"%s", vmem->pathname);
		if (!vmem)
		{
			fprintf(efile,"     <MEMBER MISSING> ");
			goto nextdev;
		}
		if (vmem->status & WRONG_VOLUME)
		{
			fprintf(efile,"     <NOT A MEMBER OF THIS VOLUME> ");
			goto nextdev;
		}
		if (vmem->status & BAD_NAME)
		{
			fprintf(efile,"     <ILLEGAL PATHNAME> ");
			goto nextdev;
		}
		if (vmem->status & CANT_ACCESS)
		{
			fprintf(efile,"     <CAN'T ACCESS> ");
			goto nextdev;
		}
		if (!(vmem->status & GOT_HEADER))
		{
			fprintf(efile,"     <CAN'T READ DISK HEADER> ");
			goto nextdev;
		}
		if (vmem->status & ILLEGAL_PTYPE)
		{
			fprintf(efile,"     <ILLEGAL PARTITION TYPE> ");
			goto nextdev;
		}
		if (vmem->status & WRONG_PSIZE)
		{
			fprintf(efile,"     <INCORRECT PARTITION SIZE> ");
			goto nextdev;
		}
		if (vmem->status & WRONG_DEV)
		{
			fprintf(efile,"     <SPECIAL FILE DEV IS WRONG> ");
			goto nextdev;
		}
		if (vmem->status & WRONG_DEVTYPE)
		{
			fprintf(efile,"     <FILE NOT BLOCK SPECIAL> ");
			goto nextdev;
		}
		if (vmem->status & UNKNOWN_DEV)
		{
			fprintf(efile,"     <UNKNOWN DEVICE> ");
			goto nextdev;
		}

		if (new) goto nextdev;

		if (vmem->status & WRONG_PTYPE)
		{
			fprintf(efile,"     <INCORRECT PARTITION TYPE> ");
			goto nextdev;
		}
		if (vmem->status & UNREADABLE_LABEL)
		{
			fprintf(efile,"     <LABEL UNREADABLE> ");
			goto nextdev;
		}
		if (!(vmem->status & GOT_LABEL))
		{
			fprintf(efile,"     <NO LABEL PRESENT> ");
			goto nextdev;
		}
		if (vmem->status & BAD_LABEL)
		{
			fprintf(efile,"     <LABEL CORRUPTED> ");
			goto nextdev;
		}
			
nextdev:
		if ((i + 1) < ndevs)
			fprintf(efile,",\t\\\n");
	}

	/* Finally, if there are any unreclaimed 'rogue' members we should
	 * print notification of this. 
	 */

	if (vol->vrogue)
	{
		vmem = vol->vrogue;
		while (vmem)
		{
			if (!(vmem->status & RECLAIMED_ROGUE))
			{
				if (!rogues)
				{
					fprintf(efile,"\nThe following devices also have labels which claim to belong to this volume\n");
					fprintf(efile,"but their labels are not consistent with the volume.\n");
				}
				fprintf(efile,"\t%s\n", vmem->pathname);
				rogues = 1;
			}
			vmem = vmem->next;
		}
	}			
	fprintf(efile,"\n\n");
	return 1;
}
