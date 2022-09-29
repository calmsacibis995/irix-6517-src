
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

#ident "$Revision: 1.15 $"

/* lvck: the check utility for logical volumes. */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/dvh.h>
#include <sys/stat.h>
#include <sys/dkio.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/sema.h>
#include <invent.h>
#include <sys/lv.h>
#include <sys/major.h>
#include <sys/sysmacros.h>
#include <sys/scsi.h>
#include "lvutils.h"

extern int errno;
extern int optind;
extern char *optarg;

struct volmember memhead = {0};
struct logvol    *loghead = NULL;
int readonly = 0;
char *progname;
int estatus = 0;
char *fname = "/etc/lvtab";

char *malloc();
char *devtopath();
struct logvol *vol_mem_is_in();
struct logvol *logvalloc();
struct volmember *volmemalloc();
struct logvol *devnametovol();
void check_table(), check_lvname(), check_devname(), check_disks(), check_vol();

main(argc, argv)
int argc;
char **argv;
{
int i;
	progname = argv[0];
	if (getuid() != 0)
	{
		fprintf(stderr,"must be superuser to run %s\n", progname);
		exit(-1);
	}
	
	while ((i = getopt(argc, argv, "dl:")) != EOF)
	{
		switch (i)
		{
		case 'l':
			fname = optarg;
			break;
		case 'd':
			check_disks();
			exit (estatus);
		default:
			goto ucrap;
		};
	}

	argc -= optind;
	if (argc == 0)
		check_table();
	else
	{
		if (!strncmp(argv[optind], "lv", 2))
			check_lvname(argv[optind]);
		else
			check_devname(argv[optind]);
	}

	exit(estatus);

ucrap:
		fprintf(stderr,"usage: lvck [-l lvtabname] [lvname]\n"); 
		fprintf(stderr,"       lvck -d\n");
		fprintf(stderr,"       lvck devname\n");
		exit(1);
}

void
check_table()
{
register struct logvol *vol;
register int i;

	importlvtab(fname);
	vol = loghead;
	while (vol)
	{
		check_vol(vol);
		vol = vol->next;
	}
}

void
check_lvname(name)
char *name;
{
register struct logvol *vol;

	importlvtab(fname);
	vol = devnametovol(name);
	if (!vol)
	{
	    fprintf(stderr, "lvck: %s not found in lvtab\n", name);
	    return;
	}
	else check_vol(vol);
}

void
check_vol(vol)
register struct logvol *vol;
{
int err = 0;

	if (!(vol->status & L_GOODTABENT))
	{
		fprintf(stderr,"\nlvck: bad lvtab entry for %s\n",
			vol->tabent->devname);
		estatus = -1;
		return;
	}
	fprintf(stderr,"\nlvck: checking lvtab entry %s\n", vol->tabent->devname);
	volmemsetup(vol);
	if (checkvolpaths(vol) == -1)
		err++;
	if (getvolheads(vol) == -1)
		err++;
	if (getlvlabs(vol) == -1)
		err++;
	if (err)
		goto badvol;

	/* Now that we have the labels, we can check that the devices are
	 * connected correctly, ie that their current devs agree with
	 * the devs recorded for them in their labels when they were
	 * initialized.
	 */

	check_dev_connections(0);

	vol->ndevs = vol->tabent->ndevs;
	vol_labels_check(vol);
	vol_devs_check(vol);

/* now proceed on basis of status... */

	if (vol->status == 
		(L_GOODTABENT | L_GOTALL_LABELS | L_GOODLABELS | L_DEVSCHEKD))
	{
		if (vol_to_tab_check(vol, COMPLETE_VOL)) 
		{
			estatus = -1;
			return;	
		}
		fprintf(stderr,"lvck: entry %s OK.\n", vol->tabent->devname);
		return;
	}

badvol:
	if (printbadvol(vol, stderr, EXISTING_VOLUME))
		tryfix(vol);
	estatus = -1;
}

void
check_disks()
{
register struct volmember *vmem;
register struct logvol *lvol;
int members;
int member_problems = 0;
int volumes = 0;
int goodvolumes;

	members = find_all_lvpaths();
	if (!members)
	{
		fprintf(stderr, "lvck: no devices found on the system are members of logical volumes\n");
		exit(0);
	}

	/* Now we have a vmem struct for each pathname for which a logical
	 * volume label exists. Next step is to go get the actual label
	 * for each one. Note that normally, there should NOT be any failures 
	 * at this stage; find_all_lvpaths has already been able to access
	 * the relevent disk header partitions. Unlikely cases, though:
	 * there's an entry for the label but we can't read it (eg bad sector),
	 * or it has no id (the label file got trashed somehow).
	 * In either of these cases, we pull the struct out of the vmem list.
	 */

	vmem = memhead.next;
	while (vmem && (vmem != &memhead))
	{
		getlabel(vmem);
		if (!vmem->lvlab || (strlen(vmem->lvlab->volid) == 0))
		{
			member_problems++;
			vmemlinkoff(vmem);
		}
		vmem = vmem->next;
	}

	if (member_problems)
	{
		members -= member_problems;
		if (!members) exit(-1);
	}

	/* Now that we have the labels, we can check that the devices are
	 * connected correctly, ie that their current devs agree with
	 * the devs recorded for them in their labels when they were
	 * initialized.
	 */

	check_dev_connections(0);

	/* Next step is to allocate a logvol struct for each different
	 * volume which appears to be represented (partially or completely)
	 * among the members. We will do this on the basis of the 
	 * system-assigned 'volid' fields, which are reasonably sure to
	 * be unique.
	 */

	vmem = memhead.next;

	while (vmem && (vmem != &memhead))
	{
		if ((lvol = vol_mem_is_in(vmem)) == NULL)
		{
			lvol = logvalloc(MAXLVDEVS);
			lvol->volid = vmem->lvlab->volid;
			volumes++;
		}

		/* Plug the vmem pointer into the appropriate slot of the
		 * volume's vmem array. One check to be done: there should
		 * NOT be anything there already, if there is we have some
		 * kind of duplicate device problem! So link the offending
		 * member on the 'rogue' chain!
		 * Also treat it as a rogue if its devindex is ridiculous.
		 * 
		 * There is obviously a label problem with this member,
		 * mark its status accordingly.
		 */

		if ((vmem->devindex > MAXLVDEVS) ||
		    (lvol->vmem[vmem->devindex]))  /* oops! */
		{
		struct volmember *rvm;

			vmemlinkoff(vmem);
			vmem->status |= BAD_LABEL;
			if ((rvm = lvol->vrogue) == NULL)
			{
				lvol->vrogue = vmem;
				vmem->prev = NULL;
			}
			else
			{
				while (rvm->next) rvm = rvm->next;
				rvm->next = vmem;
				vmem->prev = rvm;
			}
			rvm = vmem;
			vmem = vmem->next;
			rvm->next = NULL;
			continue;
		}
		else
		{
			lvol->vmem[vmem->devindex] = vmem;
			vmem->lvol = lvol;
			lvol->labels++;
			if (vmem->devindex > lvol->hilabel)
				lvol->hilabel = vmem->devindex;
		}
		vmem = vmem->next;
	}

	/* scan, evaluate & display the list */

	lvol = loghead;
	while (lvol)
	{
		if (evaluate_vol(lvol) == 0)
			goodvolumes++;
		lvol = lvol->next;
	}
	if (goodvolumes) printgoodvols();
	volumes -= goodvolumes;
	if (volumes) printbadvols();
}

/* vol_mem_is_in(): for check_disks, runs through all volumes known to
 * date (thru the list rooted at loghead) to see if the given member
 * is a member of an already-known volume. Return pointer to this if so,
 * else NULL.
 */

struct logvol *
vol_mem_is_in(vmem)
register struct volmember *vmem;
{
register struct logvol *lvol;

	lvol = loghead;
	while (lvol)
	{
		if (streq(lvol->volid, vmem->lvlab->volid))
			return (lvol);
		lvol = lvol->next;
	}
	return (NULL);
}

void
check_devname(name)
char *name;
{
register struct volmember *vmem;
register struct lvlabel *lvlab;
struct stat sb;

	if (stat(name, &sb) < 0)
	{
		fprintf(stderr,"lvck: cannot access %s.\n", name);
		return;
	}
	if ((sb.st_mode & S_IFMT) != S_IFBLK)
	{
		fprintf(stderr,"lvck: %s is not a block device.\n", name);
		return;
	}
	if (major(sb.st_rdev) == LV_MAJOR)
	{
		fprintf(stderr,"lvck: %s is a logical volume, not a real disk.\n", name);
		return;
	}

	if (checkformat(name))
	{
		fprintf(stderr,"lvck: Full /dev/dsk/xxx pathname required.\n");
		return;
	}
	vmem = volmemalloc(1, NULL);
	vmem->pathname = name;
	accesscheck(vmem);
	getdvh(vmem);
	getlabel(vmem);

	if (!(vmem->status & GOT_LABEL))
	{
		fprintf(stderr,"lvck: no label found for %s\n", vmem->pathname);
		exit(-1);
	}

	lvlab = vmem->lvlab;

	print_label(vmem);
	if (lvlab->mydev != vmem->dev)
	{
		printf("lvck warning: device mismatch, %s was initialized when connected as ", name);
		printpath(lvlab->mydev, 0, 0);
		putchar('\n');
	}

}

/* find_all_lvpaths(): this attempts to discover every device in the
 * system which appears to be a logical volume member (by virtue of
 * having a logical volume label). Working from the hardware inventory,
 * it marches through every disk on the system. For each one, it gets
 * the volume header, and examines the directory for 'lvlab' files.
 * For each lvlab file found, it deduces the pathname of the actual device
 * special file, allocates a struct vmem (double-linked on the chain rooted 
 * at memhead), and plugs in the deduced pathname.
 * It then checks the pathname to see if it is accessible, of the correct
 * (block) type & has the expected dev: the vmem struct status is set
 * appropriately.
 * It returns the number of members found. As with everything else
 * dealing with device pathnames, it makes GROSS, UGLY, nonportable assumptions
 * about SGI device naming conventions...
 *
 * DHXX: it will need work if we ever use disks of larger physical
 * sector size! It will also need extension if any new types of disk
 * appear.
 */

/* UGLINESS WARNING!! UGLINESS WARNING!! Portable code lovers exit here! */

find_all_lvpaths()
{
inventory_t *inv;
struct volume_header vh;
struct volume_directory *vd;
vmp vmem;
int members = 0;
int vhfd, i, partition;
char vhpathname[MAXDEVPATHLEN];
char mempathbase[MAXDEVPATHLEN];
char *mempathname;
dev_t expected_dev;
int majornum;
struct stat s;
int is_ipi;
int is_jag;
int is_raid;
int is_scsi;

	while (inv = getinvent())
	{
		if (inv->inv_class != INV_DISK) continue;
		bzero(vhpathname, MAXDEVPATHLEN);
		bzero(mempathbase, MAXDEVPATHLEN);
		bzero(&vh, sizeof (vh));
		is_ipi = is_jag = is_raid = is_scsi = 0;
		switch (inv->inv_type) {

		case INV_SCSIDRIVE :
				is_scsi = 1;
				/* check for nonzero lun
				 */
				if ( inv->inv_state ) {
					sprintf(vhpathname,
						"/dev/rdsk/dks%dd%dl%dvh",
						inv->inv_controller, 
						inv->inv_unit,
						inv->inv_state);

					sprintf(mempathbase,
						"/dev/dsk/dks%dd%l%dds",
						inv->inv_controller, 
						inv->inv_unit,
						inv->inv_state);
				} else {
					sprintf(vhpathname,
						"/dev/rdsk/dks%dd%dvh",
						inv->inv_controller, 
						inv->inv_unit);

					sprintf(mempathbase,
						"/dev/dsk/dks%dd%ds",
						inv->inv_controller, 
						inv->inv_unit);
				}
				majornum = DKSC_MAJOR;
				break;

		case INV_VSCSIDRIVE :
				is_jag = 1;
				sprintf(vhpathname,"/dev/rdsk/jag%dd%dvh",
					inv->inv_controller, inv->inv_unit);
				sprintf(mempathbase,"/dev/dsk/jag%dd%ds",
					inv->inv_controller, inv->inv_unit);
				majornum = JAG0_MAJOR + inv->inv_controller;
				break;

		case INV_SCSIRAID :
				is_raid = 1;
				sprintf(vhpathname,"/dev/rdsk/rad%dd%dvh",
					inv->inv_controller, inv->inv_unit);
				sprintf(mempathbase,"/dev/dsk/rad%dd%ds",
					inv->inv_controller, inv->inv_unit);
				majornum = USRAID_MAJOR;
				break;

			default :	continue;
		};

		if ((vhfd = open(vhpathname, O_RDONLY)) < 0)
		{
			fprintf(stderr,"lvck: cannot open volume header %s of disk which inventory claims is present\n", vhpathname);
			continue;
		}
		if (ioctl(vhfd, DIOCGETVH, &vh) < 0)
		{
			fprintf(stderr,"lvck: cannot read volume header %s of disk which inventory claims is present\n", vhpathname);
			close(vhfd);
			continue;
		}
		close(vhfd);
		vd = vh.vh_vd;
		for (i = 0; i < NVDIR; i++, vd++)
		{
			if ((partition = lvlabdecode(vd)) < 0) continue;
			if ((mempathname = malloc(MAXDEVPATHLEN)) == NULL)
			{
				fprintf(stderr,"lvck: can't allocate memory!\n");
				exit(-1);
			}
			bzero(mempathname, MAXDEVPATHLEN);
			sprintf(mempathname,"%s%d", mempathbase, partition);
			vmem = volmemalloc(1, NULL);
			vmem->pathname = mempathname;

		/* Now we check up on the pathname itself. It is possible
		 * for there to be a logical volume label in the disk
		 * header for a device which does not exist in /dev/dsk
		 * on the current system (it hasn't been mknod'ed).
		 * Or, pathologically, a file of that name may exist but have
		 * incorrect maj or min numbers; or even not be a block
		 * special!
		 * 1/22/90: controller/unit mapping to dev is different
		 * for IPI. Sigh.
		 * 3/9/90: SCSI is different too! (Oops, forgot that!)
		 * 9/17/91: So's jag....
		 * 10/20/94: support for scsi luns
		 */
			if (is_scsi || is_raid)
			{
				int ctlr;

				/* convert to internal ctlr format */
				ctlr = SCSI_INT_CTLR(inv->inv_controller);
				expected_dev = makedev(majornum,
				    ((ctlr << DKSC_CTLR_SHIFT) +
				     (inv->inv_unit << DKSC_UNIT_SHIFT) +
				     (inv->inv_state << DKSC_LUN_SHIFT) + 
				     partition) );
			}
			else if (is_ipi || is_jag)
			{
				expected_dev = makedev(majornum, ((inv->inv_unit << 4) + partition));
			}
			else	/* smd or esdi */
			{
				expected_dev = makedev(majornum, ((inv->inv_controller << 6) + (inv->inv_unit << 4) + partition));
			}

			if (stat(mempathname, &s) < 0)
				vmem->status |= CANT_ACCESS;
			else
			{
				if ((s.st_mode & S_IFMT) != S_IFBLK)
					vmem->status |= WRONG_DEVTYPE;
				if (s.st_rdev != expected_dev)
					vmem->status |= WRONG_DEV;
			}
			vmem->dev = expected_dev;
			members++;
		}
	}
	endinvent();
	return (members);
}

/* lvlabdecode(): takes a volume directory entry & determines if it appears
 * to be a logical volume label. If so, return the partition # it refers to.
 * Returns -1 if not.
 */

lvlabdecode(vd)
register struct volume_directory *vd;
{
int partnum;

	if (vd->vd_lbn <= 0) return (-1);
	if (vd->vd_nbytes == 0) return (-1);
	if (strncmp("lvlab", vd->vd_name, 5)) return (-1);
	if (vd->vd_name[7] != 0x00) return (-1);
	if (!isdigit(vd->vd_name[5])) return (-1);
	else partnum = vd->vd_name[5] - '0';
	if (vd->vd_name[6])
	{
		if (!isdigit(vd->vd_name[6])) return (-1);
		else partnum = (partnum * 10) + vd->vd_name[6] - '0';
	}
	if (partnum > NPARTAB) return (-1);
	return (partnum);
}

/* print_label(): bungs out the label of the given member in a form 
 * approximating an lvtab entry; prefacing it with the system-id of the 
 * volume (made to look like an lvtab comment).
 * It is assumed that the vmem struct has a label. However, this can be
 * called from check_device so we may not assume that the label is
 * necessarily correct: sanity checks are needed.
 */

print_label(vmem)
register struct volmember *vmem;
{
register struct lvlabel *lvlab = vmem->lvlab;
register struct headerinfo *vhinfo = vmem->vhinfo;
register struct lv *lv = &lvlab->lvdescrip;
register int i;
char *path;
unsigned ndevs;
int idtrunc = 0;
int nametrunc = 0;
int corrupt = 0;

	/* First, sanity checks. */

	if ((ndevs = lv->l_ndevs) > MAXLVDEVS)
	{
		ndevs = MAXLVDEVS;
		corrupt = 1;
	}
	if (strlen(lvlab->volid) >= MAXVNAMELEN)
	{
		idtrunc = 1;
		lvlab->volid[MAXVNAMELEN] = 0x00;
		corrupt = 1;
	}
	if (strlen(lvlab->volname) >= MAXVNAMELEN)
	{
		nametrunc = 1;
		lvlab->volname[MAXVNAMELEN] = 0x00;
		corrupt = 1;
	}
	if (lvlab->lvmagic != LV_MAGIC)
		corrupt = 1;

	printf("\n# Volume id:   %s\n", lvlab->volid);
	printf("lv?:");
	if (strlen(lvlab->volname))
		printf("%s", lvlab->volname);
	putchar(':');
	if (lv->l_stripe > 1)
	{
		printf("stripes=%d:", lv->l_stripe);
		if (lv->l_gran != vhinfo->trksize)
			printf("step=%d:", lv->l_gran);
	}
	printf("devs=");
	for(i = 0; i < ndevs; i++)
	{
		printpath(lv->pd[i].dev, 0, 0);
		if ((i + 1) < ndevs)
			printf(",\t\\\n\t");
	}
	putchar('\n');

	/* Print notice of any problems. */

	if (corrupt)
		printf("Label is corrupted:\n");
	if (lvlab->lvmagic != LV_MAGIC)
		printf("Bad magic number.\n");
	if (idtrunc)
		printf("Bad id length.\n");
	if (nametrunc)
		printf("Bad name length.\n");
	if (lv->l_ndevs > MAXLVDEVS)
		printf("Illegal number of devices %d\n", lv->l_ndevs);

}

/* evaluate_vol(): (for -d option) goes through volumes deduced
 * from on-disk labels to divide them into OK (all members present & correct),
 * and otherwise. 
 * For volumes where members are missing, we guess the pathnames from the
 * information in the 'typical' label for the volume.
 * 	If there are any 'rogue' members which we couldn't put in their
 *	correct places earlier, we see if they can now be placed.
 *	Otherwise, we fake up vmem structs for the missing members, plug in 
 *	their guessed pathnames & check accessability, readability of 
 * 	disk header etc.
 */

evaluate_vol(vol)
register struct logvol *vol;
{
register struct lvlabel *lvlab;
register struct volmember *tvmem, *rvmem;
register struct lv *lv;
register struct physdev *pd;
register int i, ndevs, firstlabel;
int goodvols = 0;
char *guesspath;

	if (find_typical_label(vol))
		return (-1);		/* no good labels at all */

	lvlab = vol->lvlab;;
	lv = &lvlab->lvdescrip;
	ndevs = lv->l_ndevs;

	/* First we will deal with any missing members,
	 * deducing their pathnames from the good label.
	 */

	for (i = 0, pd = lv->pd; i < ndevs; i++, pd++)
		if (vol->vmem[i])
			continue;
		else
		{
		/* allocate space for the name, and deduce it from the dev */

			if ((guesspath = malloc(MAXDEVPATHLEN)) == NULL)
			{
			    fprintf(stderr,"lvck: malloc failure!\n");
			    exit(-1);
			}
			strcpy(guesspath, devtopath(pd->dev, 0));

		/* If there are any 'rogue' members, see if one of them
		 * matches this pathname: we can 'reclaim' it to its correct
		 * slot in the volume if so.
		 */

			if (rvmem = vol->vrogue)
			{
				while (rvmem)
				{
					if (streq(rvmem->pathname, guesspath))
					{
					    rvmem->status |= RECLAIMED_ROGUE;
					    vol->vmem[i] = rvmem;
					    free(guesspath);
					    break;
					}
					rvmem = rvmem->next;
				}
				if (vol->vmem[i])
					continue;
			}

		/* No reclaimable rogues. So allocate a new vmem struct, plug 
		 * the pathname into it, and plug it into the volume.
		 */

			rvmem = volmemalloc(1, vol);
			vol->vmem[i] = rvmem;
			rvmem->pathname = guesspath;

		/* see if we recognized that dev as a known type */

			if (checkformat(guesspath))
			{
				rvmem->status |= UNKNOWN_DEV;
				continue;
			}
			accesscheck(rvmem);
			getdvh(rvmem);
			if (rvmem->status & GOT_HEADER)
			{
			/* Well, we got the header. May as well try for the
			 * label. If it was OK, & the right disk, it should
			 * have been here already. But maybe there's a
			 * corrupt label, or the wrong disk...
			 */

				getlabel(rvmem);
			}
		}
	vol_labels_check(vol);
	vol_devs_check(vol);
	if ((vol->status == (L_GOODLABELS | L_DEVSCHEKD)) && !vol->vrogue)
	{
		vol->status |= L_GOODEVAL;
		return (0);
	}
	else return (-1);
}

printgoodvols()
{
register struct logvol *vol;
register struct volmember *vmem;

	fflush(stderr);	/* push out any earlier error messages */

	printf("\n# The following logical volumes are present and correct: \n\n\n");
	vol = loghead;
	while (vol)
	{
		if (vol->status & L_GOODEVAL)
		{
			vmem = vol->vmem[0];
			print_label(vmem);
		}
		vol = vol->next;
	}
	printf("\n\n# End of good volumes.\n");
	printf("# *****************************************************************************\n\n");
}

printbadvols()
{
register struct logvol *vol;

	fflush(stderr);	/* push out any earlier error messages */

	printf("\n# The following logical volumes appear to be at least partially present\n");
	printf("# but are not usable.\n\n");
	printf("# This may be due to old logical volume labels left over from previous\n");
	printf("# uses of the disks. Logical volume labels reside in the volume header\n");
	printf("# of the disk: see the NOTE in mklv(1M); obsolete logical volume labels\n");
	printf("# may be removed with dvhtool(1M). \n");

	vol = loghead;
	while (vol)
	{
		if (!(vol->status & L_GOODEVAL))
		{
			if (printbadvol(vol, stdout, EXISTING_VOLUME))
			{
				tryfix(vol);
				printf("# ---------------------------------------------------------------------------\n");
			}
		}
		vol = vol->next;
	}
}

/* tryfix(): determines if damage to a volume seems reasonable to
 * repair by writing new labels; prompts for go-ahead if so.
 *
 * We apply rather stringent criteria for a 'fixable' volume: it must
 * have a majority of members with good, consistent labels, there
 * must be no members with problems other than missing or bad labels,
 * and the physical devices of the members we are considering repairing
 * must match the device specified in the labels. 
 */

tryfix(vol)
register struct logvol *vol;
{
register struct volmember *vmem;
register struct lvlabel *lvlab;
register struct lv *lv;
register struct physdev *pd;
register int i, ndevs, psize;
int repairs = 0;


	if (vol->vrogue) return;
	if ((lvlab = vol->lvlab) == NULL) return;
	lv = &lvlab->lvdescrip;
	if (vol->goodlabels < (vol->labels / 2)) return;
	if (vol->tabent)
	{
		if (strlen(vol->tabent->volname) != strlen(lvlab->volname))
			return;
		if (strlen(lvlab->volname) && 
		    !streq(lvlab->volname, vol->tabent->volname))
			return;
	}
	for (i = 0, pd = lv->pd; i < vol->ndevs; i++, pd++)
	{
		if ((vmem = vol->vmem[i]) == NULL) 
			return;

		switch (vmem->status)
		{
		case (GOT_HEADER | GOT_LABEL) : /* an OK member */ 
						continue;

		case GOT_HEADER 		     	    :
		case (GOT_HEADER | GOT_LABEL | BAD_LABEL )  :
		case (GOT_HEADER | UNREADABLE_LABEL) 	    : 
						/* might be fixable */ 
							break;

		default:	return;
		};

		switch (partype(vmem))
		{
		case PTYPE_LVOL  :
		case PTYPE_RLVOL :	break;
		
		default:	return;
		};

		if (vmem->dev != pd->dev)
			return;

	/* Now the size check. This depends on striping: if not striping, the
	 * size in the physdev should be exactly the partition size. But if
	 * striping, it should have been rounded down to a multiple of the
	 * granularity.
	 */
		psize = partsize(vmem);	
		if (lv->l_stripe > 1)
			psize -= (psize % lv->l_gran);
		if (psize != pd->size)
			return;

		vmem->status |= POSSIBLE_REPAIR;
		repairs++;
	}

	/* We should get here only if there is no un-fixable member.
	 * Sanity check: we should have found at least one repair to do!
	 * (Should never be called with a good vol, but just in case...)
	 */

	if (!repairs)
		return;
	if (reply("Attempt to restore missing labels"))
		return;

	/* Hey ho, here we go.... */

	for (i = 0; i < vol->ndevs; i++)
	{
		vmem = vol->vmem[i];
		if (!(vmem->status & POSSIBLE_REPAIR))
			continue;

	/* Allocate space for the new label. The label for each member is
	 * identical to the 'typical' label held for the volume with the
	 * exception of the 'mydev' field. So copy in the typical label
	 * and set the 'mydev' field.
	 */

		if ((vmem->newlvlab = (struct lvlabel *) malloc(vol->labsize))
			== NULL)
		{
			fprintf(stderr,"lvck: malloc failure!\n");
			exit(-1);
		}
		vmem->newlabsize = vol->labsize;
		bcopy((caddr_t)vol->lvlab, 
		      (caddr_t)vmem->newlvlab, 
		      vol->labsize);
		vmem->newlvlab->mydev = vmem->dev;
		if (writelabel(vmem, (vol->ptype == PTYPE_RLVOL)))
		{
			fprintf(stderr,"lvck: couldn't restore label for %s\n", vmem->pathname);
			exit(-1);
		}
	}
}

