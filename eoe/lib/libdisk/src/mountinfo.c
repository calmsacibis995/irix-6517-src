
/* Copyright 1995, Silicon Graphics, Inc.
 * All Rights Reserved.
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

/* Overview:
 *
 * The 'mnt' and 'dsk' routines were designed to implement a standard method
 * of handling the following problems:
 *
 * - A standard method for generating disk/partition device names
 * - Testing for mounted overlapping partitions
 * - Testing if mounting a new partition or logical volume
 *   will conflict with any existing mounted/claimed partitions
 * - Determining if any blk/chr partition device entries are missing
 * - Determining which partitions in a system are unused
 * - A standard, thread safe, method for listing all disks from inventory
 *
 * The 'mnt' routines are initialized by calling:
 *  mnt_check_state_t *check_state;
 *  mnt_check_init(&check_state);
 *
 * To close the routines and free associated memory call:
 *  mnt_check_end(check_state);
 *
 * The following routines are provided:
 *
 *  mnt_find_existing_mount_conficts(check_state);
 *    - build a list of conflicting partition mounts in the current system
 * 
 *  mnt_find_mount_conflicts(check_state, "/devicename");
 *    - check if mounting "/devname" causes overlapping mount conflicts
 *
 *  mnt_find_dev_mount_conflicts(check_state, dev_t);
 *    - same as above but uses dev_t of partition/lv to check
 *
 *  mnt_find_unmounted_partitions(check_state);
 *    - build a list of all useable unmounted partitions in the system
 *
 *  mnt_find_missing_dev_entries(check_state);
 *    - build a list of existing devices that are missing chr or blk entries
 *
 * The following list walking routines are provided to traverse the
 * the results of each of the 'mnt_find*' commands:
 *
 *  mnt_plist_walk_reset(check_state);
 *    - reset walk pointer to 1st item in list
 *
 *  mnt_plist_walk_next(check_state);
 *    - move to next item in the list
 *
 *  mnt_plist_walk_clear(check_state);
 *    - remove all entries from list (automatically called from mnt_find*)
 *
 * Each mnt_plist routine returns a pointer to a mnt_plist_return_t
 * This structure is filled in from internal tables each time 'plist_walk_next'
 * is called.  This allows our internal structures to be hidden and modified
 * in the future without affecting user code.  While conviently locating
 * all relevant info in the mnt_plist_return_t.
 *
 * The 'dsk' routines provide a thread safe wrapper around the getinvent
 * routine which only inspects disk devices.
 *
 *  dsk_scan_state_t *scan_state;
 *  dsk_inventory_init(&scan_state);
 *    - Initializes the scan routines
 *
 *  dsk_inventory_next(scan_state);
 *    - moves to the next disk in the inventory
 * 
 *  dsk_inventory_end(scan_state);
 *    - frees all associated memory
 *
 * FUTURE WORK:
 * RAID failover support needs to be added (if failover
 * occurs during data collection we may give erroneous results).
 *
 * -San 8/95
 */

/*********************************************************************/

#include <mountinfo.h>
#include <unistd.h>
#include <sys/uuid.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>     /* PATH_MAX & friends */
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/errno.h>
#include <sys/swap.h>   /* swapctl routines */
#include <dirent.h>     /* SYSV dirent */
#include <mntent.h>     /* mtab/fstab routines */
#include <sys/major.h>  /* major deviceno typedefs */
#include <ustat.h>      /* check for filesystem off of dev */
#include <ctype.h>
#include <sys/syssgi.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <diskinfo.h>
#include <sys/dkio.h>
#include <sys/statvfs.h>
#include <sys/iograph.h>


/*********************************************************************/
/* internal prototypes */

static mnt_partition_table_t *new_mnt_partition_table(void);
static mnt_device_entry_t *new_mnt_device_entry(void);
static mnt_check_state_t *new_mnt_check_state(void);
static mnt_plist_return_t *new_mnt_plist_return(void);
static mnt_plist_t *new_mnt_plist(void);
static mnt_plist_entry_t *new_mnt_plist_entry(void);

static int mnt_test_for_overlap(int, int, int, int);
static void mnt_copy_ret_info(mnt_plist_return_t *,
			      mnt_plist_entry_t *);
static int mnt_register_partition(mnt_check_state_t *,
				   mnt_partition_table_t *, int, int, int);
static int mnt_register_overlap(mnt_check_state_t *,
				 mnt_partition_table_t *, int, int);
static int mnt_find_lvol_mount_conflicts(mnt_check_state_t *, dev_t);
static int mnt_find_lvol_mounted_partitions(mnt_check_state_t *, dev_t);
static int mnt_ptable_check_conflicts(mnt_check_state_t *,
				      mnt_partition_table_t *, int);
static int mnt_ptable_check_mounts(mnt_check_state_t *,
				   mnt_partition_table_t *);
static int mnt_mark_dev_mounted(mnt_check_state_t *, dev_t, int);
#ifdef NOT_YET
static int mnt_unmark_dev_mounted(mnt_check_state_t *, dev_t);
#endif
static int mnt_mark_dev_claimed(mnt_check_state_t *, dev_t, int, dev_t);

static xlv_tab_subvol_t *xlv_get_subvol_space(void);
static xlv_tab_vol_entry_t *xlv_get_vol_space(void);
static int xlv_free_subvol_space(xlv_tab_subvol_t *);
static int xlv_free_vol_space(xlv_tab_vol_entry_t *);

static int mnt_check_xlv_subvol(mnt_check_state_t *, xlv_tab_subvol_t *,
				dev_t);
static int mnt_check_xlv_claims(mnt_check_state_t *);
static int mnt_check_swap_mounts(mnt_check_state_t *);
static char *mnt_find_mount_opt(char *, char *);
static int mnt_check_fstab_mounts(mnt_check_state_t *);
static int mnt_check_mtab_mounts(mnt_check_state_t *);
static int mnt_load_partition_table(mnt_partition_table_t *);

/* note: this should be put back after we come up with a generic
   block dev_t to char dev_t conversion routine XXXsbarr */
static
int
mnt_find_dev_mount_conflicts(mnt_check_state_t *, dev_t);

/*********************************************************************/
/* internal static vars */

static int mnt_overlap_sequence_num = 0;

/*********************************************************************/
/* mem alloc routines, pseudo c++ style
 * 
 * Creates a new structure of the given type and initializes it
 * before returning a pointer to the new object.
 */

static
mnt_partition_table_t *
new_mnt_partition_table(void)
{
    mnt_partition_table_t *tmp_ptable;
    int which_part;

    if ((tmp_ptable = malloc(sizeof(mnt_partition_table_t))) == NULL) {
	fprintf(stderr, "new_mnt_partition_table(): malloc failed\n");
	exit(1);
    }

    tmp_ptable->dev_entry = NULL;
    tmp_ptable->next = NULL;
    tmp_ptable->ptable_loaded = 0;

    for (which_part = 0; which_part < NPARTAB; which_part++) {
	tmp_ptable->ptable[which_part].device = 0;
	tmp_ptable->ptable[which_part].start_lbn = 0;
	tmp_ptable->ptable[which_part].end_lbn = 0;
	tmp_ptable->ptable[which_part].devptr = NULL;
    }

    return tmp_ptable;
}

static
mnt_device_entry_t *
new_mnt_device_entry(void)
{
    mnt_device_entry_t *newent;

    /* make a new device entry */
    if ((newent = malloc(sizeof(mnt_device_entry_t))) == NULL) {
	fprintf(stderr, "new_mnt_device_entry(): malloc failed\n");
	exit(1);
    }

    newent->valid = 0;
    newent->mounted = 0;
    newent->dev_missing = MNT_DEV_MISSING_NONE;
    newent->ptable_loaded = 0;
    newent->pathname[0] = '\0';
    newent->owner = MNT_OWNER_NONE;
    newent->owner_dev = 0;
    newent->part = NULL;
    newent->next = NULL;

    return newent;
}

static
mnt_check_state_t *
new_mnt_check_state(void) 
{
    mnt_check_state_t *overlap_state;

    /* allocate & setup overlap checking state */
    if ((overlap_state = malloc(sizeof(mnt_check_state_t))) == NULL) {
	puts("new_mnt_check_state(): unable to malloc()");
	exit(1);
    }

    overlap_state->magic = MNT_CHECK_STATE_MAGIC;
    overlap_state->partition_list = NULL;
    overlap_state->device_list = NULL;
    overlap_state->conflict_list = NULL;

    return overlap_state;
}

static
mnt_plist_return_t *
new_mnt_plist_return(void)
{
    mnt_plist_return_t *new_ret_entry;

    if ((new_ret_entry = malloc(sizeof(mnt_plist_return_t))) == NULL) {
	fprintf(stderr, "mnt_plist_return(): unable to malloc()");
	exit(1);
    }

    new_ret_entry->version = MNT_PLIST_RETURN_VER;
    new_ret_entry->pathname = NULL;
    new_ret_entry->dev = 0;
    new_ret_entry->dev_missing = 0;
    new_ret_entry->ptable = NULL;
    new_ret_entry->part_num = 0;
    new_ret_entry->start_lbn = 0;
    new_ret_entry->end_lbn = 0;
    new_ret_entry->owner = 0;
    new_ret_entry->owner_dev = 0;
    new_ret_entry->mounted = 0;
    new_ret_entry->cause = MNT_CAUSE_NONE;

    return new_ret_entry;
}

static
mnt_plist_t *
new_mnt_plist(void)
{
    mnt_plist_t *new_conf;

    if ((new_conf = malloc(sizeof(mnt_plist_t))) == NULL) {
	fprintf(stderr, "mnt_plist(): unable to malloc()");
	exit(1);
    }

    new_conf->done = 0;
    new_conf->count = 0;
    new_conf->causes = 0;
    new_conf->head = NULL;
    new_conf->walk = NULL;
    new_conf->ret = new_mnt_plist_return();

    return new_conf;
}

static
mnt_plist_entry_t *
new_mnt_plist_entry(void)
{
    mnt_plist_entry_t *new_conf;

    if ((new_conf = malloc(sizeof(mnt_plist_entry_t))) == NULL) {
	fprintf(stderr, "mnt_plist_entry(): unable to malloc()");
	exit(1);
    }

    new_conf->sequence = 0;
    new_conf->part = 0;
    new_conf->ptable = NULL;
    new_conf->cause = MNT_CAUSE_NONE;
    new_conf->next = NULL;

    return new_conf;
}

/*********************************************************************/

/* mnt_test_for_overlap()
 *
 * This routine determines if two ranges (s1-e1, s2-e2) overlap. 
 * The algorithm tests to see if either one of the endpoints of 
 * one region lay within the other region.  If so, the regions
 * overlap and 1 is returned.  The only special case is when one
 * region encompasses the other, in this case we must test the endpoints
 * of the smaller region, not the larger one.  This assumes that all
 * regions are >= 0.
 *
 * ARGUMENTS:
 *  s1 - start of region 1
 *  e1 - end of region 1
 *  s2 - start of region 2
 *  e2 - end of region 2
 *
 * RETURN VALUE:
 *  0 no overlap
 *  1 regions overlap
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   8/10/95 sbarr
 */

static
int
mnt_test_for_overlap(int s1, int e1, int s2, int e2)
{
    int st, et;

    /* check if 1 encompasses 2 */
    if ((s1 < s2) && (e1 > e2)) {
	/* swap */
	st = s1;
	et = e1;
	s1 = s2;
	e1 = e2;
	s2 = st;
	e2 = et;
    }

    /* 1 does not encompass 2 */
    if ((s1 >= s2) && (s1 <= e2)) {
	/* overlap */
	return 1;
			   
    } else if ((e1 >= s2) && (e1 <= e2)) {
	/* overlap */
	return 1;
    }
    return 0;
}

/* mnt_copy_ret_info()
 *
 * Collect all relevant partition info into a structure for user
 * inspection.  We do this to hide the internal layout of our
 * conflict detection structures, and hopefully allow us to modify
 * things later without breaking user code.
 *
 * ARGUMENTS:
 *  part - pointer to returned-to-user partition description struct
 *  item - pointer to partition entry to copy info from
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   9/25/95 sbarr
 */

static
void
mnt_copy_ret_info(mnt_plist_return_t *part, mnt_plist_entry_t *item)
{
    mnt_partition_entry_t *pte;
    mnt_device_entry_t *dev_entry;

    pte = &item->ptable[item->part];
    dev_entry = pte->devptr;

    part->sequence = item->sequence;
    part->pathname = dev_entry->pathname;
    part->dev = pte->device;
    part->dev_missing = dev_entry->dev_missing;
    part->ptable = item->ptable;
    part->part_num = item->part;
    part->start_lbn = pte->start_lbn;
    part->end_lbn = pte->end_lbn;
    part->owner = dev_entry->owner;
    part->owner_dev = dev_entry->owner_dev;
    part->mounted = dev_entry->mounted;
    part->cause = item->cause;
}

/* mnt_plist_walk_clear()
 *
 * Clear out any previous conflict list entries.  Reset
 * sequence number to 0.
 *
 * ARGUMENTS:
 *  check_state - pointer to a valid mnt_check_state_t
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   8/26/95 sbarr
 */

void
mnt_plist_walk_clear(mnt_check_state_t *check_state)
{
    mnt_plist_t *clist;
    mnt_plist_entry_t *centry;
    mnt_plist_entry_t *tmp;

    if (!check_state)
	return;

    /* reset the sequence number */
    mnt_overlap_sequence_num = 0;

    clist = check_state->conflict_list;
    if (!clist)
	return;

    centry = check_state->conflict_list->head;

    while (centry) {
	tmp = centry->next;
	free(centry);
	centry = tmp;
    }

    check_state->conflict_list->head = 0;
    check_state->conflict_list->walk = 0;
    check_state->conflict_list->count = 0;
    check_state->conflict_list->done = 0;
    check_state->conflict_list->causes = 0;

    /* leave conflict_list & ret pointers intact */
}

/* mnt_plist_walk_next()
 *
 * Move to next partition in conflict list.  Copy relevant info
 * into a user inspectable structure.
 *
 * ARGUMENTS:
 *  check_state - pointer to a valid mnt_check_state_t
 *
 * RETURN VALUE:
 *  pointer to user inspectable mnt_plist_return_t
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   8/10/95 sbarr
 */

mnt_plist_return_t *
mnt_plist_walk_next(mnt_check_state_t *check_state)
{
    mnt_plist_t *clist;

    if (!check_state)
	return NULL;

    clist = check_state->conflict_list;

    if (!clist)
	return NULL;
    if (!clist->head)
	return NULL;
    if (clist->done)
	return NULL;

    if (!clist->walk) {
	clist->walk = clist->head;
    } else {
	clist->walk = clist->walk->next;
    }

    if (!clist->walk) {
	clist->done = 1;
	return NULL;
    }

    /* copy all info into struct here */
    mnt_copy_ret_info(clist->ret, clist->walk);

    return clist->ret;
}

/* mnt_plist_walk_reset()
 *
 * Reset the conflict list walk pointer to the beginning of
 * the conflict list.
 *
 * ARGUMENTS:
 *  check_state - pointer to a valid mnt_check_state_t
 *
 * RETURN VALUE:
 *  void
 *
 * CREATED:         LAST MODIFIED:
 * 8/10/95 sbarr    8/10/95 sbarr
 */

void
mnt_plist_walk_reset(mnt_check_state_t *check_state)
{
    if (check_state->conflict_list) {
	check_state->conflict_list->walk = NULL;
	check_state->conflict_list->done = 0;
    }
}

/* mnt_plist_walk_count()
 *
 * Return the number of items in the plist.
 *
 * ARGUMENTS:
 *  check_state - pointer to a valid mnt_check_state_t
 *
 * RETURN VALUE:
 *  number of items in walk list
 *
 * CREATED:         LAST MODIFIED:
 * 8/26/95 sbarr    8/27/95 sbarr
 */

int
mnt_plist_walk_count(mnt_check_state_t *check_state)
{
    if (check_state)
	if (check_state->conflict_list)
	    return check_state->conflict_list->count;
    return 0;
}

/* mnt_register_partition()
 *
 * Add a partition to the conflict list.  If sequence is
 * set, then roll the sequence number forward one.
 *
 * ARGUMENTS:
 *  check_state - pointer to a valid mnt_check_state_t
 *  ptable - pointer to disk paritition table
 *  part - the actual partition number
 *  sequence - flag, 1 = bump sequence number, 0 = leave the same
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   9/25/95 sbarr
 */

static
int
mnt_register_partition(mnt_check_state_t *check_state,
		       mnt_partition_table_t *ptable,
		       int part,
		       int sequence,
		       int cause)
{
    mnt_plist_entry_t *new_entry;

    /* demand load partition table into memory (if not already there) */
    if (mnt_load_partition_table(ptable) == -1)
	return -1;

    if (!check_state->conflict_list) {
	check_state->conflict_list = new_mnt_plist();
    }

    new_entry = new_mnt_plist_entry();

    /* increment sequence number */
    if (sequence) {
	mnt_overlap_sequence_num++;
    }

    new_entry->sequence = mnt_overlap_sequence_num;
    new_entry->ptable = ptable->ptable;
    new_entry->part = part;
    new_entry->cause = cause;

    /* OR all causes together */
    check_state->conflict_list->causes |= cause;

    new_entry->next = check_state->conflict_list->head;
    check_state->conflict_list->head = new_entry;
    check_state->conflict_list->count++;

    return 0;
}

/* mnt_register_overlap()
 *
 * Convienence function.  Registers two overlapping partitions at once.
 *
 * ARGUMENTS:
 *  check_state - pointer to a valid mnt_check_state_t
 *  ptable - pointer to disk paritition table
 *  part1 - the actual partition number of 1st overlapping partition
 *  part2 - the actual partition number of 2nd overlapping partition
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   10/24/95 sbarr
 */

static
int
mnt_register_overlap(mnt_check_state_t *check_state,
		     mnt_partition_table_t *ptable,
		     int part1,
		     int part2)
{
    if (-1 == mnt_register_partition(check_state, ptable,
				     part1, 1, MNT_CAUSE_OVERLAP)) {
	return -1;
    }
    if (-1 == mnt_register_partition(check_state, ptable,
				     part2, 0, MNT_CAUSE_OVERLAP)) {
	/* should never get here */
	return -1;
    }
    return 0;
}

/* mnt_causes_test()
 *
 * See if a specific cause is present in the plist
 *
 * ARGUMENTS:
 *  check_state - valid mnt_check_state_t
 *  cause - the cause (or causes) to test for
 *
 * RETURN VALUE:
 *  1 if present
 *  0 if not
 */

int mnt_causes_test(mnt_check_state_t *check_state, int cause)
{
    if (!check_state)
	return 0;
    if (!check_state->conflict_list)
	return 0;
    if (cause & check_state->conflict_list->causes)
	return 1;
    return 0;
}

/* mnt_string_cause_show_description()
 *
 * Display a list of all conflict causes.  
 *
 * ARGUMENTS:
 *  check_state - valid check_state pointer
 *  fp - file pointer to write output to
 *  prefix - prefix to display before each line of output (':' appended) 
 */

void
mnt_causes_show(mnt_check_state_t *check_state,
		FILE *fp,
		char *prefix)
{
    char p[103];
    char ostr[200];
    char count = 'a';
    int causebits;
    int cause_walk;
    int cause_count = 0;

    /* get causes */
    if (!check_state)
	return;
    if (!check_state->conflict_list)
	return;
    causebits = check_state->conflict_list->causes;

    for (cause_walk = 1; cause_walk < MNT_CAUSE__END; cause_walk <<= 1) {
	if (cause_walk & causebits)
	    cause_count++;
    }

    p[0] = '\0';

    if (prefix) {
	strncpy(p, prefix, 20);
	strcat(p, ": ");
    }

    if (cause_count > 1)
	strcat(p, "%c. ");

    if (causebits & MNT_CAUSE_MOUNTED) {
	strcpy(ostr, p);
	strcat(ostr, "partitions in use detected on device\n");
	fprintf(fp, ostr, count++);
    }
    if (causebits & MNT_CAUSE_OVERLAP) {
	strcpy(ostr, p);
	strcat(ostr, "overlapping partitions in use on device\n");
	fprintf(fp, ostr, count++);
    }
    if (causebits & MNT_CAUSE_NODEV) {
	strcpy(ostr, p);
	strcat(ostr, "device is missing entry in /dev/rdsk or /dev/dsk\n");
	fprintf(fp, ostr, count++);
    }
    if (causebits & MNT_CAUSE_UNUSED) {
	strcpy(ostr, p);
	strcat(ostr, "available partitions found (be cautious of overlaps)\n");
	fprintf(fp, ostr, count++);
    }
    if (causebits & MNT_CAUSE_MULTIMOUNT) {
	strcpy(ostr, p);
	strcat(ostr, "attempt to use partition that is already in use\n");
	fprintf(fp, ostr, count++);
    }
    if (causebits & MNT_CAUSE_LVOL_OWNED) {
	strcpy(ostr, p);
	strcat(ostr, "partition already in use by xlv logical volume\n");
	fprintf(fp, ostr, count++);
    }
}

/* mnt_string_cause()
 *
 * Convert a cause value into the appropriate string for display
 */

char *
mnt_string_cause(int cause)
{
    switch (cause) {
    case MNT_CAUSE_NONE:
	return "none";
    case MNT_CAUSE_MOUNTED:
	return "already in use";
    case MNT_CAUSE_OVERLAP:
	return "overlap";
    case MNT_CAUSE_NODEV:
	return "no /dev entry";
    case MNT_CAUSE_UNUSED:
	return "unused";
    case MNT_CAUSE_MULTIMOUNT:
	return "already in use";
    case MNT_CAUSE_LVOL_OWNED:
	return "part of xlv vol";
    default:
	break;
    };

    return "[unknown cause]";
}

/* mnt_string_owner()
 *
 * Covert the specified owner value into a display string 
 */

char *
mnt_string_owner(int owner)
{
    static char *ownerlist[] = {
	"",
	"xlv",
	"xfs",
	"swap",
	"efs",
	"rawdata",
	/* don't forget to add new entries here - placeholders for now */
	"?"
    };
    static int count = sizeof(ownerlist)/sizeof(char *);

    if (owner > count)
	return "[out of range]";

    return ownerlist[owner];
}

/* mnt_plist_show()
 *
 * Convience function to print all partitions listed in the
 * conflict list.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  prefix - prefix to display before each output line
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   9/25/95 sbarr
 */

void
mnt_plist_show(mnt_check_state_t *check_state, FILE *fp, char *prefix)
{
    mnt_plist_return_t *entry;
    int max_width = 0;
    int width;

    if (!mnt_plist_walk_count(check_state)) {
	puts("No entries.");
	return;
    }

    if (!prefix)
	prefix = "-";

    /* calc max_width of pathname */
    mnt_plist_walk_reset(check_state);
    while ((entry = mnt_plist_walk_next(check_state)) != NULL) {
	width = (int)strlen(entry->pathname);
	if (width > max_width)
	    max_width = width;
    }

    /* show pathnames */
    mnt_plist_walk_reset(check_state);

    fprintf(fp, "%s: %-*.*s seq owner   state\n",
	   prefix, max_width + 1, max_width + 1, "devname");
    while ((entry = mnt_plist_walk_next(check_state)) != NULL) {

	fprintf(fp, "%s: %-*.*s %3d %-7.7s %s\n",
	       prefix,
	       max_width + 1,
	       max_width + 1,
	       entry->pathname,
	       entry->sequence,
	       mnt_string_owner(entry->owner),
	       mnt_string_cause(entry->cause));

    }
}

/*********************************************************************/

/* mnt_load_partition_table()
 * 
 * Fill in the partition information for a specific ptable from disk.
 * 
 * ARGUMENTS:
 *  ptable - pointer to ptable containing valid pathname entry and dev info
 *
 * RETURN VALUE:
 *  0 if no errors encountered
 *  -1 if failure
 *
 * CREATED:        LAST MODIFIED:
 * 10/23/95 sbarr  10/23/95 sbarr
 */

static
int
mnt_load_partition_table(mnt_partition_table_t *ptable)
{
    int fd;
    int first_lbn;
    int nblks;
    int which_part;
    struct volume_header vh;
    int ret;
    mnt_partition_entry_t *pentry;

    if (ptable->ptable_loaded)
	return(0);

    if (!ptable->dev_entry) 
	return(-1);

    /*
     * Attempt to read the disk header.
     * If the open call fails this may be a disconnected device
     * or possibly one of the alternate paths for a SCSI raid device
     * either way, add this device to the table and mark it invalid.
     */

    /* open the vh */
    if ((fd = open(ptable->dev_entry->pathname, 0)) == -1) {
	/* open failed, mark device invalid */
	ptable->dev_entry->valid = 0;
	return -1;
    }

    /* pull disk header / partition info */
    ret = ioctl(fd, DIOCGETVH, (caddr_t)&vh);
    close(fd);
    if (ret < 0) {
	/* vol header ioctl failed, mark this dev invalid */
	ptable->dev_entry->valid = 0;
	return -1;
    }

    /* we've made it this far, mark the entry valid */
    ptable->dev_entry->valid = 1;
    ptable->ptable_loaded = 1;

    /* copy out vh partition info - fill in valid entry info */
    for (which_part = 0; which_part < NPARTAB; which_part++) {

	pentry = &ptable->ptable[which_part];

	/* flag that the partition table was loaded from disk */
	pentry->devptr->ptable_loaded = 1;

	first_lbn = vh.vh_pt[which_part].pt_firstlbn;
	nblks = vh.vh_pt[which_part].pt_nblks;

	pentry->start_lbn = first_lbn;

	/*
	 * if VH then we already know the device is good,
	 * skip the checks
	 */
	if (which_part == DSK_PART_VH) {
	    pentry->end_lbn = first_lbn + nblks - 1;
	    continue;
	}

	/* if negative partition size, make it zero, skip namegen */ 
	if (nblks <= 0) {

	    /* ignore zero length partitions */
	    pentry->end_lbn = first_lbn;
	    pentry->devptr->valid = 0;

	} else {

	    /* test if chr & block names stat */

	    pentry->end_lbn = first_lbn + nblks - 1;

	    /*
	     * We used to stat the filename here to see if it really
             * exists, but since the advent of the HWG, the dev is
             * guaranteed to have the correct char/block entries,
             * so we don't have to stat the devices again.
	     * mismatched block/char devs are a thing of the past.
             */

	    pentry->devptr->valid = 1;
	}
    }
    return 0;
}


/* mnt_check_init_doscan()
 * 
 * This is the main workhorse function for the mnt lib.  For each
 * disk in inventory, it generates a volume header name.  Reads
 * the partition info for each disk into our internal partition table.
 * For valid partitions, and associated device description record is
 * created.
 *
 * During the scan, valid partitions without /dev/[r]dsk entries have
 * /var/tmp devices created for them and each partition is marked as
 * missing a valid dev entry.
 * 
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *
 * RETURN VALUE:
 *  0 if no errors encountered
 *  -1 if failure
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    9/10/96 sbarr
 */

/* ARGSUSED */
static
int
diskscan(
	const char          *diskname,
	const struct stat64 *statp,
	inventory_t         *inv,
	int                 inv_count,
	void                *ourdata)
{
	mnt_check_state_t *overlap_state = ourdata;
	int which_part;
	char ndevpathname[DSK_DEVNAME_MAX];
	char aliasname[DSK_DEVNAME_MAX];
	int aliaslen;
	char diskdir[DSK_DEVNAME_MAX];
	dev_t dev;
	mnt_device_entry_t *tmp_deventry;
	mnt_device_entry_t *tmp_deventry2;
	mnt_partition_table_t *tmp_ptable;
	mnt_partition_entry_t *pentry;
	struct stat64 sbuf;
	char *p;
	DIR *dp;
	struct dirent64 *direntp;

	/* make a new device entry and add this entry to head of list */
	tmp_deventry = new_mnt_device_entry();
	tmp_deventry->next = overlap_state->device_list;
	overlap_state->device_list = tmp_deventry;

	/* copy devicename (use an alias if possible) */
	aliaslen = sizeof(aliasname);
	if (diskpath_to_alias2((char *)diskname, aliasname, &aliaslen, inv, inv_count))
		strcpy(tmp_deventry->pathname, aliasname);
	else
		strcpy(tmp_deventry->pathname, diskname);

	/* we're guaranteed that this vh exists since it's in the hwgraph */

	/* gen a fresh partition table */
	tmp_ptable = new_mnt_partition_table();

	tmp_ptable->dev_entry = tmp_deventry;

	/* add new ptable entry to head of our state list */
	tmp_ptable->next = overlap_state->partition_list;
	overlap_state->partition_list = tmp_ptable;

	/* since we're trying to avoid reading the volume headers
         * until we absolutely have to: we create all potential devices
         * and load the partition tables with dev_t's (inferred from
         * the stat on the vh entry) but leave the rest of the ptable
         * unfilled.
         */

	dev = statp->st_rdev;
	for (which_part = 0; which_part < NPARTAB; which_part++) {

		pentry = &tmp_ptable->ptable[which_part];

		/* special processing for vh, since we already have stat */
		if (which_part == DSK_PART_VH) {
			pentry->device = dev;
			tmp_deventry->part = pentry;
			pentry->devptr = tmp_deventry;
			pentry->devptr->valid = 1;
			continue;
		}

		/* make a new device entry */
		tmp_deventry2 = new_mnt_device_entry();
		tmp_deventry2->valid = 0;

		/* add this entry to head of list */
		tmp_deventry2->next = overlap_state->device_list;
		overlap_state->device_list = tmp_deventry2;

		/* we'll fill these in later, if the partition exists */
		pentry->device = 0;
		tmp_deventry2->pathname[0] = '\0';

		/* point the device entry to the appropriate ptable entry */
		tmp_deventry2->part = pentry;
		/* point the partition table entry back to the dev */
		pentry->devptr = tmp_deventry2;
	}

	/* fill in the correct dev_t's */

	/* put together the correct path for the volume partition */
	strcpy(diskdir, diskname);
	if ((p = strstr(diskdir, "/" EDGE_LBL_VOLUME_HEADER)) == NULL)
		return 0;
	*p = '\0';

	/* get dev_t for vol path */
	strcpy(ndevpathname, diskdir);
	strcat(ndevpathname, "/" EDGE_LBL_VOLUME "/" EDGE_LBL_CHAR);
	if (stat64(ndevpathname, &sbuf) == -1)
		return 0;

	pentry = &tmp_ptable->ptable[DSK_PART_VOL];
	pentry->devptr->valid = 1;

	/* convert to alias */
	aliaslen = sizeof(aliasname);
	if (diskpath_to_alias2(ndevpathname, aliasname, &aliaslen, inv, inv_count))
		strcpy(pentry->devptr->pathname, aliasname);
	else
		strcpy(pentry->devptr->pathname, ndevpathname);

	pentry->device = sbuf.st_rdev;
	
	/* get dev_t's for remaining partitions */
	strcpy(ndevpathname, diskdir);
	strcat(ndevpathname, "/" EDGE_LBL_PARTITION);

	/* cobble together disk dir path */
	if ((dp = opendir(ndevpathname)) == NULL)
		return 0;

	/* walk the disk partition directory to find live partitions */

	for (;;) {
		char space[sizeof(struct dirent64)+NAME_MAX+1];
		struct dirent64 *dirent = (struct dirent64 *)space;

		if (readdir64_r(dp, dirent, &direntp) != 0)
			break;
		if (direntp == NULL)
			break;

		/* skip '.' and '..' */
		if (strcmp(direntp->d_name, ".") == 0)
			continue;
		if (strcmp(direntp->d_name, "..") == 0)
			continue;

		/* get partition number */
		which_part = atoi(direntp->d_name);

		/* verify it's valid */
		if (which_part < 0 || which_part > 15)
			continue;
		strcpy(ndevpathname, diskdir);
		strcat(ndevpathname, "/" EDGE_LBL_PARTITION "/");
		strcat(ndevpathname, direntp->d_name);
		strcat(ndevpathname, "/" EDGE_LBL_CHAR);
		if (stat64(ndevpathname, &sbuf) == -1)
			continue;

		pentry = &tmp_ptable->ptable[which_part];
		pentry->devptr->valid = 1;

		/* convert to alias */
		aliaslen = sizeof(aliasname);
		if (diskpath_to_alias2(ndevpathname, aliasname, &aliaslen, inv, inv_count))
			strcpy(pentry->devptr->pathname, aliasname);
		else
			strcpy(pentry->devptr->pathname, ndevpathname);

		pentry->device = sbuf.st_rdev;
	}
	closedir(dp);

	return 0;
}

/* mnt_check_init()
 * 
 * Allocate a check_state, call doscan() and then run the partition
 * claim checks.  Each partition is checked for ownership from lv,xlv,
 * swap, fstab ('ignore' entries for raw partitions), and mtab.
 * 
 * ARGUMENTS:
 *  check_state - pointer to mnt_check_state_t pointer (to be allocated)
 *
 * RETURN VALUE:
 *  0 if no errors encountered
 *  -1 failure
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    7/31/96 sbarr
 */

int
mnt_check_init(mnt_check_state_t **overlap_state)
{
    int ret;

    /* allocate & setup overlap checking state */
    *overlap_state = new_mnt_check_state();

    /* scan drives */
    if ((ret = dsk_walk_drives((dsk_cbfunc)diskscan, *overlap_state)) != 0) 
	return ret;

    /* register ownership of discovered partitions */
    mnt_check_xlv_claims(*overlap_state);
    mnt_check_swap_mounts(*overlap_state);
    mnt_check_fstab_mounts(*overlap_state);
    mnt_check_mtab_mounts(*overlap_state);

    return 0;
}

/* mnt_check_end()
 * 
 * Free all associated memory structures and remove and /var/tmp entries.
 * 
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *
 * RETURN VALUE:
 *  0 if no errors encountered
 *  -1 failure
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    8/22/95 sbarr
 */

int
mnt_check_end(mnt_check_state_t *check_state)
{
    /* run through state and free all list entries & state */
    mnt_device_entry_t *dev_walk;
    mnt_device_entry_t *dev_tmp;
    mnt_partition_table_t *pt_walk;
    mnt_partition_table_t *pt_tmp;
    mnt_plist_t *clist;

    if (!check_state)
        return -1;
    if (check_state->magic != MNT_CHECK_STATE_MAGIC)
        return -1;

    /* free conflict list entries */
    mnt_plist_walk_clear(check_state);

    /* free device list & remove dev entries from /var/tmp */
    dev_walk = check_state->device_list;
    while (dev_walk) {
	dev_tmp = dev_walk->next;
#ifdef OLD_SCHEME
	/* remove /var/tmp dev entries */
	if (dev_walk->dev_missing) {
	    unlink(dev_walk->pathname);
	}
#endif /* OLD_SCHEME */
	free(dev_walk);
	dev_walk = dev_tmp;
    }

    /* free partition list */
    pt_walk = check_state->partition_list;
    while (pt_walk) {
	pt_tmp = pt_walk->next;
	free(pt_walk);
	pt_walk = pt_tmp;
    }

    /* free conflict list */
    clist = check_state->conflict_list;
    if (clist) {
	if (clist->ret) {
	    free(clist->ret);
	}
	free(clist);
    }

    free(check_state);

    return 0;
}

/*********************************************************************/

/* mnt_find_missing_dev_entries()
 *
 * Check the partition list for any partitions missing blk or char
 * device entries.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   10/24/95 sbarr
 */

int
mnt_find_missing_dev_entries(mnt_check_state_t *check_state)
{
    mnt_partition_table_t *pt_walk;
    int which_part;
    int count = 0;

    /* clear out conflict list */
    mnt_plist_walk_clear(check_state);

    pt_walk = check_state->partition_list;

    for (;pt_walk != NULL; pt_walk = pt_walk->next) {

	if (-1 == mnt_load_partition_table(pt_walk))
	    continue;

	for (which_part = 0; which_part < NPARTS; which_part++) {

	    if (!pt_walk->ptable[which_part].devptr) {
		continue;
	    }
	    if (pt_walk->ptable[which_part].devptr->dev_missing) {
		if (!mnt_register_partition(check_state, pt_walk, which_part,
					   1, MNT_CAUSE_NODEV)) {
		    count++;
		}
	    }
	}
    }
    return count;
}

/* mnt_find_unmounted_partitions()
 *
 * Search through partition list and tag all partitions that are unmounted.
 * This routine is strict, all XLV and LV 'claimed' partitions are considered
 * 'mounted'.  Only non-overlapping non-claimed partitions are listed.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *
 * RETURN VALUE:
 *  none
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   10/29/95 sbarr
 */

int
mnt_find_unmounted_partitions(mnt_check_state_t *check_state)
{
    mnt_partition_table_t *pt_walk;
    int which_part;
    int which_part2;
    int s1, s2, e1, e2;
    int count = 0;
    int part_status[NPARTS];
    int p1_tag, p2_tag;

    /* clear out conflict list */
    mnt_plist_walk_clear(check_state);

    for (pt_walk = check_state->partition_list;
	 pt_walk != NULL;
	 pt_walk = pt_walk->next) {

	/* demand load partition table info */
	if (mnt_load_partition_table(pt_walk) == -1)
	    continue;

	/* clear out status flags */
	for (which_part = 0; which_part < NPARTS; which_part++)
	  part_status[which_part] = 0;

	for (which_part = 0; which_part < NPARTS; which_part++) {

	    if (!pt_walk->ptable[which_part].devptr) {
		part_status[which_part] = 1;
		continue;
	    }

	    if (!pt_walk->ptable[which_part].devptr->valid) {
		part_status[which_part] = 1;
		continue;
	    }

	    if  (which_part == DSK_PART_VOL || which_part == DSK_PART_VH) {
		part_status[which_part] = 1;
		continue;
	    }

	    s1 = pt_walk->ptable[which_part].start_lbn;
	    e1 = pt_walk->ptable[which_part].end_lbn;

	    if (s1 == e1) {
		part_status[which_part] = 1;
		continue;
	    }

	    p1_tag = 0;
	    if (pt_walk->ptable[which_part].devptr->mounted ||
		pt_walk->ptable[which_part].devptr->owner_dev) {
		part_status[which_part] = 1;
		p1_tag = 1;
	    }

	    for (which_part2 = 0; which_part2 < NPARTS; which_part2++) {
		if (which_part2 == which_part)
		    continue;
		if (part_status[which_part2])
		    continue;

		if (which_part2 == DSK_PART_VOL || which_part2 == DSK_PART_VH) {
		    part_status[which_part2] = 1;
		    continue;
		}

		if (!pt_walk->ptable[which_part2].devptr) {
		    part_status[which_part2] = 1;
		    continue;
		}
		if (!pt_walk->ptable[which_part2].devptr->valid) {
		    part_status[which_part2] = 1;
		    continue;
		}

		s2 = pt_walk->ptable[which_part2].start_lbn;
		e2 = pt_walk->ptable[which_part2].end_lbn;

		if (s2 == e2) {
		    part_status[which_part2] = 1;
		    continue;
		}

		p2_tag = 0;
		if (pt_walk->ptable[which_part2].devptr->mounted ||
		    pt_walk->ptable[which_part2].devptr->owner_dev) {
		    part_status[which_part2] = 1;
		    p2_tag = 1;
		}

		/* if they're both tagged, skip the overlap check */
		if ((p1_tag || p2_tag) && !(p1_tag && p2_tag)) {
		    if (mnt_test_for_overlap(s1, e1, s2, e2)) {

			part_status[which_part] = 1;
			part_status[which_part2] = 1;
		    }
		}
	    }
	}

	for (which_part = 0; which_part < NPARTS; which_part++) {
	    if (part_status[which_part] == 0) {
		/* ptable already loaded */
		mnt_register_partition(check_state, pt_walk,
				     which_part, 1, MNT_CAUSE_UNUSED);
		count++;
	    }
	}

    }

    return count;
}

/* mnt_ptable_check_conflicts()
 * 
 * Takes a partition table list and checks for overlapping
 * mounted entries.  Non-mounted entries are ignored.  This
 * routine is strict, it considers xlv/lv owned
 * partitions as mounted regardless of mount state.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *
 * RETURN VALUE:
 *  number of overlapping partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   10/23/95 sbarr
 */

static
int
mnt_ptable_check_conflicts(mnt_check_state_t *check_state, mnt_partition_table_t *ptable_p, int mntflag)
{
    int which_part;
    int which_part2;
    mnt_partition_entry_t *pentry;
    mnt_partition_entry_t *pentry2;
    int s1, s2, e1, e2;
    int count = 0;

    /* demand load partition info */
    if (mnt_load_partition_table(ptable_p) == -1)
	return 0;

    for (which_part = 0; which_part < NPARTS; which_part++) {

	pentry = &ptable_p->ptable[which_part];

	/* invalid or 0 partitions may not have a devptr */
	if (!pentry->devptr)
	    continue;

	/* if not mounted and not owned by xlv/lv, then skip */
	if (!pentry->devptr->mounted && !pentry->devptr->owner_dev)
	    continue;


	if (mntflag) {
	    /* if this partition is claimed by more than 1 device, flag it */
	    if (pentry->devptr->mounted > 1) {
		mnt_register_partition(check_state, ptable_p,
				       which_part, 1, MNT_CAUSE_MULTIMOUNT);
		count++;
	    }
	}

	s1 = pentry->start_lbn;
	e1 = pentry->end_lbn;
	if (s1 == e1)
	    continue;

	for (which_part2 = which_part + 1; which_part2 < NPARTS; which_part2++) {
	    pentry2 = &ptable_p->ptable[which_part2];

	    if (!pentry2->devptr)
		continue;

	    /* if not mounted and not owned by xlv/lv, then skip */
	    if (!pentry2->devptr->mounted && !pentry2->devptr->owner_dev)
		continue;

	    s2 = pentry2->start_lbn;
	    e2 = pentry2->end_lbn;
	    if (s2 == e2)
		continue;

	    if (mnt_test_for_overlap(s1, e1, s2, e2)) {
		mnt_register_overlap(check_state, ptable_p,
				     which_part, which_part2);
		count++;
	    }
	}
    }
    return count;
}

/* mnt_ptable_check_mounts()
 * 
 * Takes a partition table list and checks for mounted entries.
 * This routine is strict, it considers xlv/lv owned
 * partitions as mounted regardless of mount state.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *
 * RETURN VALUE:
 *  number of mounted partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 9/25/95 sbarr   2/12/96 sbarr
 */

static
int
mnt_ptable_check_mounts(mnt_check_state_t *check_state, mnt_partition_table_t *ptable_p)
{
    int which_part;
    mnt_partition_entry_t *pentry;
    int count = 0;
    int cause;

    for (which_part = 0; which_part < NPARTS; which_part++) {

	pentry = &ptable_p->ptable[which_part];

	/* invalid or 0 partitions may not have a devptr */
	if (!pentry->devptr)
	    continue;

	/* if not mounted and not owned by xlv/lv, then skip */
	cause = 0;
	if (pentry->devptr->mounted)
	    cause = MNT_CAUSE_MOUNTED;

	if (pentry->devptr->owner_dev)
	    cause = MNT_CAUSE_LVOL_OWNED;

	if (cause) {

	    /* demand load partition info */
	    if (!mnt_register_partition(check_state, ptable_p,
					which_part, 1, cause)) {
		count++;
	    }
	}
    }
    return count;
}

/* mnt_find_existing_mount_conflicts()
 * 
 * Scans the partition table list and checks each table
 * for overlapping mounted entries.  Non-mounted entries are
 * ignored.  This routine is strict, it considers xlv/lv owned
 * partitions as mounted regardless of mount state.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *
 * RETURN VALUE:
 *  number of overlapping partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   9/25/95 sbarr
 */

int
mnt_find_existing_mount_conflicts(mnt_check_state_t *check_state)
{
    mnt_partition_table_t *pt_walk;
    int count = 0;

    /* clear out conflict list */
    mnt_plist_walk_clear(check_state);

    pt_walk = check_state->partition_list;

    for (pt_walk = check_state->partition_list;
	 pt_walk != NULL;
	 pt_walk = pt_walk->next) {

	count += mnt_ptable_check_conflicts(check_state, pt_walk, 1);
    }

    return count;
}

/* mnt_find_lvol_mount_conflicts()
 * 
 * Scans each partition table in mem checking if any of the partitions
 * in the logical volume (specified by the dev_t) overlap any mounted
 * partitions.  Also checks to see if any partitions claimed by the
 * volume dev_t are already mounted (possible for raw partition entries
 * in fstab or if someone puts a filesystem on a partition from an
 * unmounted logical volume.  Of course, the later case should never
 * happen after this lib gets put into use.
 * 
 * Obvious assumption: logical volumes (lv/xlv) encompass multiple
 * real partitions.
 *
 * NOTE: this is an internal function called from mnt_find_dev_mount_conflicts
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  dev - dev_t of the logical volume
 *
 * RETURN VALUE:
 *  number of overlapping/pre-mounted partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   10/23/95 sbarr
 */

static
int
mnt_find_lvol_mount_conflicts(mnt_check_state_t *check_state, dev_t dev)
{
    mnt_partition_table_t *pt_walk;
    mnt_partition_entry_t *pentry;
    mnt_partition_entry_t *pentry2;
    int which_part;
    int which_part2;
    int s1, s2, e1, e2;
    int count = 0;
    int found1, found2;

    pt_walk = check_state->partition_list;

    for (;pt_walk != NULL; pt_walk = pt_walk->next) {
	for (which_part = 0; which_part < NPARTS; which_part++) {

	    found1 = 0;
	    pentry = &pt_walk->ptable[which_part];

	    if (!pentry->devptr)
		continue;

	    if (!pentry->devptr->mounted
		&& (pentry->devptr->owner_dev != dev)) 
		continue;

	    if (pentry->devptr->owner_dev == dev) {
		found1 = 1;

		/*
		 * Is this partition already mounted?
		 * (possible that it's claimed as a raw partition in fstab)
		 */

		if (pentry->devptr->mounted) {
		    if (!mnt_register_partition(check_state, pt_walk,
						which_part, 1,
						MNT_CAUSE_MULTIMOUNT)) {
			count++;
		    }
		}
	    }

	    /* demand load partition info
	     * not optimal location but will have to do
	     */
	    if (mnt_load_partition_table(pt_walk) == -1)
		continue;

	    s1 = pentry->start_lbn;
	    e1 = pentry->end_lbn;
	    if (s1 == e1)
		continue;

	    /* inner loop through remaining partitions */
	    for (which_part2 = which_part + 1;
		 which_part2 < NPARTS;
		 which_part2++) {

		found2 = 0;
		pentry2 = &pt_walk->ptable[which_part2];

		if (!pentry2->devptr)
		    continue;

		if (!pentry2->devptr->mounted
		    && (pentry2->devptr->owner_dev != dev)) 
		    continue;

		if (pentry2->devptr->owner_dev == dev)
		    found2 = 1;

		/*
		 * one out of the two being tested have to belong to the
		 * logical volume
		 */

		if (!found1 && !found2)
		    continue;

		s2 = pentry2->start_lbn;
		e2 = pentry2->end_lbn;
		if (s2 == e2)
		    continue;

		if (mnt_test_for_overlap(s1, e1, s2, e2)) {

		    mnt_register_overlap(check_state, pt_walk,
					 which_part, which_part2);
		    count++;
		}
	    }
	}
    }
    return count;
}

/* mnt_find_mount_conflicts()
 * 
 * Wrapper interface to mnt_find_dev_mount_conflicts.  Takes a
 * pathname instead of a dev_t.  Stats the pathname and passes
 * the rdev to mnt_find_dev_mount_conflicts().
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  devname - pathname of the device
 *
 * RETURN VALUE:
 *  number of overlapping partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 8/21/95 sbarr   8/29/95 sbarr
 */

int
mnt_find_mount_conflicts(mnt_check_state_t *check_state, char *devname)
{
    /* stat the devname - use devt for clal to find_dev_mount */
    struct stat64 sbuf;
    char *newpath;

    if ((newpath = findrawpath(devname)) == NULL)
	    newpath = devname;

    if (stat64(newpath, &sbuf) == -1) {
	/* unable to stat dev */
	return -1;
    }

    if (sbuf.st_rdev == 0) {
	return 0;
    }

    return mnt_find_dev_mount_conflicts(check_state, sbuf.st_rdev);
}

/* mnt_find_dev_mount_conflicts()
 * 
 * Given a dev_t this routine searches each partition table looking
 * for overlapping mounted partitions.  If the dev_t is an xlv or lv
 * volume, the check is send to lvol_check.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  dev - dev_t of the device to check
 *
 * RETURN VALUE:
 *  number of overlapping partitions detected
 *
 * NOTE:
 *  'dev' must be a char dev_t
 *
 * CREATED:        LAST MODIFIED:
 * 8/10/95 sbarr   10/23/95 sbarr
 */

static
int
mnt_find_dev_mount_conflicts(mnt_check_state_t *check_state, dev_t dev)
{
    mnt_partition_table_t *pt_walk;
    mnt_partition_entry_t *pentry;
    mnt_partition_entry_t *pentry2;
    int found;
    int which_part;
    int which_part2;
    int s1, s2, e1, e2;
    int count = 0;
    int dest_registered = 0;

    if (!dev)
	return 0;

    /* clear out any previous walk state */
    mnt_plist_walk_clear(check_state);

    /* if logical vol, then scan for owner_dev */
    if (mnt_is_logical_vol(dev)) {
	return mnt_find_lvol_mount_conflicts(check_state, dev);
    }

    pt_walk = check_state->partition_list;

    found = 0;
    for (;pt_walk != NULL; pt_walk = pt_walk->next) {

	for (which_part = 0; which_part < NPARTS; which_part++) {

	    pentry = &pt_walk->ptable[which_part];
	    if (pentry->device == dev) {

		if (pentry->devptr) {
		    found = 1;
		    break;
		} else {
		    /* should never happen, but just in case */
		    fprintf(stderr, "No info available for specified device (%d/%d)\n",
			    major(dev), minor(dev));
		    return -2;
		}
	    }
	}
	if (found)
	    break;
    }

    if (!found) {
	return -1;
    }

    /* demand load partition info */
    if (mnt_load_partition_table(pt_walk) == -1)
	return 0;

    /*
     * partition already mounted?
     * note: have to check since raw partitions may not give a dev busy
     */
    if (pentry->devptr->mounted >= 1) {

	mnt_register_partition(check_state, pt_walk, which_part, 1,
			       MNT_CAUSE_MOUNTED);
	count++;
	/* pass through to check overlaps as well */
    } else if (pentry->devptr->owner_dev) {

	/* strict processing of lv/xlv ownership - assume mounted */
	mnt_register_partition(check_state, pt_walk, which_part, 1,
			       MNT_CAUSE_LVOL_OWNED);
	count++;
	/* pass through to check overlaps as well */
    }

    s1 = pentry->start_lbn;
    e1 = pentry->end_lbn;

    /* now check the found partition for overlaps in it's partition table */

    for (which_part2 = 0; which_part2 < NPARTS; which_part2++) {

	if (which_part2 == which_part)
	    continue;

	pentry2 = &pt_walk->ptable[which_part2];

	if (!pentry2->devptr)
	    continue;

	/* if not mounted or not owned by xlv/lv then skip */
	if (!pentry2->devptr->mounted && !pentry2->devptr->owner_dev)
	    continue;

	s2 = pentry2->start_lbn;
	e2 = pentry2->end_lbn;
	if (s2 == e2)
	    continue;

	if (mnt_test_for_overlap(s1, e1, s2, e2)) {
	    if (!dest_registered) {
		mnt_register_partition(check_state, pt_walk,
				       which_part, 0, MNT_CAUSE_OVERLAP);
		dest_registered = 1;
	    }
	    mnt_register_partition(check_state, pt_walk,
				       which_part2, 0, MNT_CAUSE_OVERLAP);
	    count++;
	}
    }
    return count;
}

/* mnt_find_mounted_partitions()
 * 
 * Wrapper interface to mnt_find_dev_mount_conflicts.  Takes a
 * pathname instead of a dev_t.  Stats the pathname and passes
 * the rdev to mnt_find_dev_mount_conflicts().
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  devname - pathname of the device
 *
 * RETURN VALUE:
 *  number of overlapping partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 9/21/95 sbarr   9/25/95 sbarr
 */

int
mnt_find_mounted_partitions(mnt_check_state_t *check_state, char *devname)
{
    /* stat the devname - use devt for clal to find_dev_mount */
    struct stat64 sbuf;

    if (stat64(devname, &sbuf) == -1) {
	/* unable to stat dev */
	return -1;
    }
    if (sbuf.st_rdev == 0) {
	return 0;
    }

    return mnt_find_dev_mounted_partitions(check_state, sbuf.st_rdev);
}

/* mnt_find_dev_mounted_partitions()
 * 
 * This routine uses a dev_t to locate the partiton table that the
 * partition belongs to.  Then the entire partition table is checked
 * for overlapping mounted partitions.  dev is _not_ considered a
 * mounted partition (in contrast to mnt_find_dev_mount_conflicts()).
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  dev - dev_t of the device to check
 *
 * RETURN VALUE:
 *  number of overlapping partitions detected
 *
 * CREATED:        LAST MODIFIED:
 * 9/21/95 sbarr   9/25/95 sbarr
 */

int
mnt_find_dev_mounted_partitions(mnt_check_state_t *check_state, dev_t dev)
{
    mnt_partition_table_t *pt_walk;
    mnt_partition_entry_t *pentry;
    int found;
    int which_part;
    int count = 0;

    if (!dev)
	return 0;

    /* clear out any previous walk state */
    mnt_plist_walk_clear(check_state);

    /* if logical vol, then scan for owner_dev */
    if (mnt_is_logical_vol(dev)) {
	return mnt_find_lvol_mounted_partitions(check_state, dev);
    }

    pt_walk = check_state->partition_list;

    found = 0;
    for (;pt_walk != NULL; pt_walk = pt_walk->next) {

	for (which_part = 0; which_part < NPARTS; which_part++) {

	    pentry = &pt_walk->ptable[which_part];
	    if (pentry->device == dev) {

		if (pentry->devptr) {
		    found = 1;
		    break;
		} else {
		    /* should never happen, but just in case */
		    fprintf(stderr, "No info available for specified device (%d/%d)\n",
			    major(dev), minor(dev));
		    return -2;
		}
	    }
	}
	if (found)
	    break;
    }

    if (!found)
	return -1;

    count = mnt_ptable_check_mounts(check_state, pt_walk);

    return count;
}

/* mnt_find_lvol_mounted_partitions()
 * 
 * Find all partitions owned by the specified logical volume (lv/xlv).
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state from mnt_*_init
 *  dev - dev_t of the logical volume
 *
 * RETURN VALUE:
 *  number of partitions found
 *
 * CREATED:        LAST MODIFIED:
 * 9/25/95 sbarr   10/24/95 sbarr
 */

static
int
mnt_find_lvol_mounted_partitions(mnt_check_state_t *check_state, dev_t dev)
{
    mnt_partition_table_t *pt_walk;
    mnt_partition_entry_t *pentry;
    int which_part;
    int count = 0;

    pt_walk = check_state->partition_list;

    for (;pt_walk != NULL; pt_walk = pt_walk->next) {
	for (which_part = 0; which_part < NPARTS; which_part++) {

	    pentry = &pt_walk->ptable[which_part];

	    if (!pentry->devptr)
		continue;

	    if (pentry->devptr->owner_dev != dev)
		continue;

	    if (pentry->devptr->mounted > 0) {

		/* demand load partition info */
		if (!mnt_register_partition(check_state, pt_walk,
					    which_part, 1,
					    MNT_CAUSE_MOUNTED)) {
		    count++;
		}
	    }
	}
    }
    return count;
}

/*********************************************************************/

/* mnt_check_and_mark_mounted()
 *
 * External interface to allow programs to mark partitions mounted.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state structure
 *  fname - pathname of block or raw device to check
 *
 * RETURN VALUE:
 *  -1 error
 *  0  no registered partitions match
 *  otherwise returns a count of all partitions reserved
 */

int mnt_check_and_mark_mounted(mnt_check_state_t *check_state, char *fname)
{
    int count;
    int owner = MNT_OWNER_UNKNOWN;
    int ret;
    dev_t rdev;
    char *pathname;
    struct stat64 sbuf;

    pathname = findrawpath(fname);
    if (!pathname)
	pathname = fname;

    if (stat64(pathname, &sbuf) == -1)
	return -2;

    rdev = sbuf.st_rdev;

    /* XXXsbarr: this needs to change when we go hwgraph */
    if (major(rdev) == XLV_MAJOR) {
	owner = MNT_OWNER_XLV;
    } else {
        if (has_xfs_fs(pathname))
            owner = MNT_OWNER_XFS;
	else if (has_efs_fs(pathname))
	    owner = MNT_OWNER_EFS;
    }

    ret = mnt_find_dev_mount_conflicts(check_state, rdev);
    if (ret == -1)
	return -2;
    if (ret > 0)
	return -1;

    count = mnt_mark_dev_mounted(check_state, rdev, owner);
    return count;
}

/* mnt_mark_dev_mounted()
 *
 * given a dev_t run down the partition list and mark the appropriate dev_t
 * as mounted.  Returns the number of partitions marked mounted
 * This routine also group marks logical volumes.
 *
 * NOTE: It's assumed that a logical vol dev_t is never entered in our
 * tables as a single partition.  If you change this semantic, you'll
 * have to change this code as well.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *  dev - device to mark claimed
 *  owner - who the owner should be attributed to
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 san      8/28/95 san
 */

static
int
mnt_mark_dev_mounted(mnt_check_state_t *check_state, dev_t dev, int owner)
{
    mnt_device_entry_t *walk_dev;
    mnt_partition_entry_t *pentry;
    int return_status = 0;

    for (walk_dev = check_state->device_list;
	 walk_dev;
	 walk_dev = walk_dev->next) {

	if ((pentry = walk_dev->part) != NULL) {

	    if (dev == pentry->device) {

		/* check if owned but not mounted */
		if (walk_dev->owner == owner) {
		    /* only mark same owner as mounted once */
		    if (!walk_dev->mounted) {
			walk_dev->mounted++;
			return_status++;
		    }
		} else {
		    /* not same owner, if none - claim it */
		    if (!walk_dev->mounted) {
			walk_dev->owner = owner;
		    }
		    walk_dev->mounted++;
		    return_status++;
		}

	    } else if (dev == walk_dev->owner_dev) {

		/* if not pentry match - see if this is a logical vol match */
		walk_dev->mounted++;
		return_status++;
	    }
	}
    }

    return return_status;
}

/* mnt_unmark_dev_mounted()
 *
 * given a dev_t run down the partition list and change the matching dev_t
 * to unmounted.  Returns the number of partitions marked unmounted
 * This routine also group unmarks logical volumes.
 *
 * NOTE: It's assumed that a logical vol dev_t is never entered in our
 * tables as a single partition.  If you change this semantic, you'll
 * have to change this code as well.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *  dev - device to unmark
 *
 * RETURN VALUE:
 *  number of partitions unmarked
 *
 * CREATED:        LAST MODIFIED:
 * 9/13/95 sbarr   9/14/95 sbarr
 */

#ifdef NOT_YET

static
int
mnt_unmark_dev_mounted(mnt_check_state_t *check_state, dev_t dev)
{
    mnt_device_entry_t *walk_dev;
    mnt_partition_entry_t *pentry;
    int return_status = 0;

    for (walk_dev = check_state->device_list;
	 walk_dev;
	 walk_dev = walk_dev->next) {

	if ((pentry = walk_dev->part) != NULL) {

	    if (dev == pentry->device) {

		/* note: we loose secondary owners */
		if (walk_dev->mounted) {
		    walk_dev->mounted--;
		    walk_dev->owner = MNT_OWNER_DEL;
		    return_status++;
		}

	    } else if (dev == walk_dev->owner_dev) {

		/* if not pentry match - see if this is a logical vol match */
		if (walk_dev->mounted) {
		    walk_dev->mounted--;
		    walk_dev->owner = MNT_OWNER_DEL;
		    return_status++;
		}
	    }
	}
    }

    return return_status;
}

#endif

/* mnt_mark_dev_claimed()
 *
 * given a dev_t run down the partition list and mark the appropriate dev_t
 * as claimed by a logical vol.  Returns the number of partitions
 * marked claimed.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *  dev - device to mark claimed
 *  owner - who the owner should be attributed to
 *  owner_dev - dev_t of the owning device (logical vol)
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    8/28/95 sbarr
 */

static
int
mnt_mark_dev_claimed(mnt_check_state_t *check_state,
		     dev_t dev,
		     int owner,
		     dev_t owner_dev)
{
    mnt_device_entry_t *walk_dev;
    mnt_partition_entry_t *pentry;
    int return_status = 0;

    walk_dev = check_state->device_list;
    while (walk_dev) {
	if ((pentry = walk_dev->part) != NULL) {
	    if (dev == pentry->device) {

		/* skip dev if same owner marks twice */
		if (walk_dev->owner != owner) {
		    walk_dev->owner = owner;
		    walk_dev->owner_dev = owner_dev;
		    return_status++;
		}
	    }
	}
	walk_dev = walk_dev->next;
    }

    return return_status;
}

/*********************************************************************/
/* xlv convenience functions */

static
xlv_tab_subvol_t *
xlv_get_subvol_space( void )
{
    int                     i, plexsize;
    xlv_tab_plex_t          *plex;
    xlv_tab_subvol_t        *subvol;

    subvol = malloc(sizeof(xlv_tab_subvol_t));
    bzero(subvol, sizeof(xlv_tab_subvol_t));
    plexsize = sizeof(xlv_tab_plex_t) +
      (sizeof(xlv_tab_vol_elmnt_t) * (XLV_MAX_VE_PER_PLEX-1));
    for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
	plex = malloc( plexsize );
	bzero(plex, plexsize );
	plex->max_vol_elmnts = XLV_MAX_VE_PER_PLEX;
	subvol->plex[i] = plex;
    }
    return( subvol );
}
static
xlv_tab_vol_entry_t *
xlv_get_vol_space(void)
{
    xlv_tab_vol_entry_t *vol;

    vol = malloc(sizeof(xlv_tab_vol_entry_t));
    if (vol == NULL)
	return (0);

    bzero(vol, sizeof(xlv_tab_vol_entry_t));

    if ((NULL == (vol->log_subvol = xlv_get_subvol_space())) ||
	(NULL == (vol->data_subvol = xlv_get_subvol_space())) ||
	(NULL == (vol->rt_subvol = xlv_get_subvol_space()))) {
	xlv_free_vol_space(vol);
	return (0);
    }
        
    return (vol);
}

static
int
xlv_free_subvol_space(xlv_tab_subvol_t *subvol) 
{
    int i;

    for ( i = 0; i < XLV_MAX_PLEXES; i++ ) {
	if (subvol->plex[i])
	  free(subvol->plex[i]);
    }
    free(subvol);
    return (0);
}

static
int
xlv_free_vol_space(xlv_tab_vol_entry_t *vol)
{
    if (vol == NULL)
      return (0);

    if (vol->log_subvol)
	xlv_free_subvol_space(vol->log_subvol);
    if (vol->data_subvol)
	xlv_free_subvol_space(vol->data_subvol);
    if (vol->rt_subvol)
	xlv_free_subvol_space(vol->rt_subvol);
    free(vol);
    return (0);
}

/*
 * If the logical device is a xlv striped volume, then it returns the 
 * stripe unit and stripe width information.
 * Input parameters:	the logical volume
 *			the subvolume type - (XLV_SUBVOL_TYPE_RT or
 *					      XLV_SUBVOL_TYPE_DATA) 
 * Output parameters:	the stripe unit and width in 512 byte blocks
 */
void
xlv_get_subvol_stripe(char *fs, int type, int *sunit, int *swidth)
{
        xlv_attr_cursor_t cursor;
        xlv_tab_subvol_t *subvolp;
        xlv_attr_req_t  req;
        xlv_tab_plex_t *plex, *prev_plex;
        xlv_getdev_t getdev;
        char *rawfile;
        int i, j, fd;

        /* convert to raw device */
        if ((rawfile = findrawpath(fs)) == NULL) 
		return;

        if ( (fd = open(rawfile, O_RDONLY)) < 0) 
               	return; 

        if (ioctl(fd, DIOCGETXLVDEV, &getdev) < 0) 
                return;

        if (syssgi(SGI_XLV_ATTR_CURSOR, &cursor) < 0) 
                return;
        

        if (type == XLV_SUBVOL_TYPE_RT && getdev.rt_subvol_dev) {
                cursor.subvol = minor(getdev.rt_subvol_dev) - 1;
        } else if (type == XLV_SUBVOL_TYPE_DATA && getdev.data_subvol_dev) {
                cursor.subvol = minor(getdev.data_subvol_dev) - 1;
        } else 
                return;

        /* allocate space */
        subvolp = xlv_get_subvol_space();

        if (NULL == subvolp) 
                return;

        req.attr = XLV_ATTR_SUBVOL;
        req.ar_svp = subvolp;
        req.cmd = XLV_ATTR_CMD_QUERY;

        /* get the corresponding subvol */
        if (syssgi(SGI_XLV_ATTR_GET, &cursor, &req) < 0) {
                xlv_free_subvol_space(subvolp);
                return;
	}

        plex = subvolp->plex[0];

	if (plex->vol_elmnts[0].type != XLV_VE_TYPE_STRIPE) {
		xlv_free_subvol_space(subvolp);
		return;
	}

	/* 1st plex; check if all volume elements are identical stripes */
	for (j = 1; j < plex->num_vol_elmnts; j++) {
       		if ((plex->vol_elmnts[j].type != XLV_VE_TYPE_STRIPE) ||
		    (plex->vol_elmnts[0].grp_size != 
		     plex->vol_elmnts[j].grp_size) ||
		    (plex->vol_elmnts[0].stripe_unit_size !=
		     plex->vol_elmnts[j].stripe_unit_size)) {
			xlv_free_subvol_space(subvolp);
			return;
       	 	}
	}		

	/*
	 * check that all volume elements in all the plexes are identical
	 */
	for (i = 1; i < subvolp->num_plexes; i++) {

		prev_plex = plex;
		plex = subvolp->plex[i];

		/*
		 * Compare 1st volume element in previous plex with 
		 * current plex
		 */
		if ((plex->vol_elmnts[0].type != XLV_VE_TYPE_STRIPE) || 
		    (prev_plex->vol_elmnts[0].grp_size !=
		     plex->vol_elmnts[0].grp_size) ||
                    (prev_plex->vol_elmnts[0].stripe_unit_size !=
                     plex->vol_elmnts[0].stripe_unit_size)) {
			xlv_free_subvol_space(subvolp);
			return;
		}

		/*
		 * Compare 1st volume element in current plex with other
		 * elements in the plex
		 */
		for (j = 1; j < plex->num_vol_elmnts; j++) {
       		 	if ((plex->vol_elmnts[j].type != XLV_VE_TYPE_STRIPE) ||
			    (plex->vol_elmnts[0].grp_size != 
			     plex->vol_elmnts[j].grp_size) ||
			    (plex->vol_elmnts[0].stripe_unit_size !=
			     plex->vol_elmnts[j].stripe_unit_size)) {
				xlv_free_subvol_space(subvolp);
				return;
       	 		}
		}

	}

        /* 512 byte values */
        *sunit = plex->vol_elmnts[0].stripe_unit_size;
        *swidth = *sunit * plex->vol_elmnts[0].grp_size;

        /* Free up the memory */
        xlv_free_subvol_space(subvolp);
        close(fd);
}


/*********************************************************************/

/* mnt_check_xlv_subvol()
 *
 * Take a pointer to an xlv subvol structure.  Break apart each
 * volume element and register each individual partition (and
 * failover path) as claimed.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *  subvol - pointer to an xlv subvol structure (contains Vol Elements)
 *  dev - the xlv dev to attribute ownership to
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    10/11/96 sbarr
 */

static
int
mnt_check_xlv_subvol(mnt_check_state_t *check_state,
		     xlv_tab_subvol_t *subvol,
		     dev_t dev)
{
    /*  we need to talk to xlv and find what partitions it owns */
    /* tag the owner field XLV */
    xlv_tab_plex_t     *plexp;
    xlv_tab_vol_elmnt_t *vep;
    xlv_tab_disk_part_t *dp;
    int count = 0;
    int which_path;
    int p, ve, d;
    char path[MAXDEVNAME];
    int len = sizeof(path);
    dev_t pdev;
    struct stat64 sbuf;

    /* check each vol for device */
    for (p = 0; p < subvol->num_plexes; p++) {
	plexp = subvol->plex[p];

	for (ve = 0; ve < plexp->num_vol_elmnts; ve++) {
	    vep = &plexp->vol_elmnts[ve];

	    for (d = 0; d < vep->grp_size; d++) {
		dp = &vep->disk_parts[d];

		for (which_path = 0; which_path < dp->n_paths; which_path++) {
		    /* we mark any failover paths as mounted as well */

		    /* XLV dev_t's are block devices, but we need the
                     * equivalent char dev for the mount overlap code.
                     * Back in 6.2 they were the same, so there was no
                     * cost associated with their conversion, but now that
                     * they're different, we have to do an explicit stat.
                     */
		    pdev = dp->dev[which_path];
		    if (dev_to_rawpath(pdev, path, &len, &sbuf) == NULL)
			    continue;

		    if (mnt_mark_dev_claimed(check_state,
					     sbuf.st_rdev,
					     MNT_OWNER_XLV,
					     dev) == 0) {
			count++;
		    }
		}
	    }
	}
    }
    return count;
}


/* mnt_check_xlv_claims()
 *
 * Talk to the xlv driver and have it return a pointer to each
 * volume it's managing.  Send each subvol to mnt_check_xlv_subvol
 * for processing of individual partitions.
 *
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    10/30/95 sbarr
 */

static
int
mnt_check_xlv_claims(mnt_check_state_t *check_state)
{
    /* talk to the xlv device driver.  Read each volume - mark all subvols
       associated with vol as from the same device for our purposes */

    xlv_attr_cursor_t cursor;
    xlv_tab_vol_entry_t *vol;
    xlv_attr_req_t req;
    long status;
    int count;
    dev_t dev;
    uint_t uustatus;

    if ((vol = xlv_get_vol_space()) == NULL) {
	fprintf(stderr, "mnt_check_xlv_claims(): can't malloc()\n");
	exit(1);
    }

    if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
	/* no xlv_cursor -> no xlv vols */
	/* actually xlv not loaded? */
	return(0);
    }

    req.attr = XLV_ATTR_VOL;
    req.ar_vp = vol;

    status = 0;
    count = 0;
    while (status == NULL) {
	status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
	if (status < 0) {
	    if (ENFILE == oserror()) {
		/* no more entries */
		break;
	    }
	    if (ENOENT == oserror()) {
		/* xlv loaded but no vols */
		break;
	    }
	    if (EFAULT == oserror()) {
		fprintf(stderr, "libdisk: Possible xlv revision mismatch:");
		break;
	    }
	    /* otherwise fail silently */
	    break;
	} else {
	    dev = 0;

	    /*
	     * break apart subvols here
	     *
	     * if empty uuid then the subvol isn't active.
	     * assume if no data subvol then no log or rt vols
	     * (is this a valid assumption? What about rt + log?)
	     */

	    /* check data subvolume */
	    if (!vol->data_subvol)
		break;

	    /* if uuid is empty, skip this vol */
	    if (B_TRUE == uuid_is_nil(&vol->data_subvol->uuid, &uustatus))
		break;

	    dev = vol->data_subvol->dev;
	    count += mnt_check_xlv_subvol(check_state, vol->data_subvol, dev);

	    /* check log subvolume */
	    if (vol->log_subvol) {
		if (B_FALSE ==
		    uuid_is_nil(&vol->log_subvol->uuid, &uustatus)) {
		    count += mnt_check_xlv_subvol(check_state,
						  vol->log_subvol,
						  dev);
		}
	    }

	    /* check real time subvolume */
	    if (vol->rt_subvol) {
		if (B_FALSE ==
		    uuid_is_nil(&vol->rt_subvol->uuid, &uustatus)) {
		    count += mnt_check_xlv_subvol(check_state,
						  vol->rt_subvol,
						  dev);
		}
	    }
	}
    }
    xlv_free_vol_space(vol);

    return count;
}

/* mnt_check_swap_mounts()
 *
 * Run through each swap entry looking for local swap devices.
 * Mark each local device found as 'mounted'.
 * 
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 sbarr    8/29/95 sbarr
 */

static
int
mnt_check_swap_mounts(mnt_check_state_t *check_state)
{
    swaptbl_t *swp_table;
    swapent_t *se;
    char *pathnames;
    char *path_walk;
    struct stat64 sbuf;
    int num_swap;
    int which_swap;
    int return_value = 0;

    /* get the number of swap devices */
    if ((num_swap = swapctl(SC_GETNSWP, NULL)) <= 0) {
	return 0;
    }

    /* allocate enough space for swap entries */
    swp_table = malloc(sizeof(swaptbl_t) + sizeof(swapent_t) * num_swap);
    if (swp_table == NULL) {
	fprintf(stderr, "malloc() failed: mnt_check_swap_mounts\n");
	exit(1);
    }

    swp_table->swt_n = num_swap;
    if ((pathnames = malloc(num_swap * PATH_MAX)) == NULL) {
	fprintf(stderr, "malloc() failed: mnt_check_swap_mounts\n");
	exit(1);
    }

    se = swp_table->swt_ent;
    path_walk = pathnames;
    for (which_swap = 0; which_swap < num_swap; which_swap++) {
	se->ste_path = path_walk;
	se++;
	path_walk += PATH_MAX;
    }

    /* possible race condition here, swap entry could be added or
     * deleted between the time when we get the number of swap entries
     * and then go to get the info.
     */

    if ((num_swap = swapctl(SC_LIST, swp_table)) < 0) {
	perror("swapctl(SC_LIST)");
	return(1);
    }

    for (which_swap = 0; which_swap < num_swap; which_swap++) {
	if (stat64(swp_table->swt_ent[which_swap].ste_path, &sbuf) == -1) {
	    continue;
	}

	if ((sbuf.st_mode & S_IFMT) == S_IFCHR
	    || (sbuf.st_mode & S_IFMT) == S_IFBLK) {
	    if (mnt_mark_dev_mounted(check_state, sbuf.st_rdev, MNT_OWNER_SWAP)) {
		return_value++;
	    }
	}
    }

    free(swp_table);
    free(pathnames);

    return return_value;
}

static
char *
mnt_find_mount_opt(char *s, char *target)
{
	char *lasttok = NULL;
    char *str = strtok_r(s, ",", &lasttok);
    unsigned long len = strlen(target);

    while (str != NULL) {
	if (!strncmp(str, target, len)) {
	    return (str);
	}
	str = strtok_r(NULL, ",", &lasttok);
    }
    return NULL;
}

/* mnt_check_fstab_mounts()
 *
 * Search through fstab for partitions marked as 'rawdata',  
 * mark each device specified as 'mounted'.
 *
 * This allows us a way of allocating raw partitions that will be
 * treated as mounted (for instance Oracle raw partitions).
 * 
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 san      9/8/95 san
 */

static
int
mnt_check_fstab_mounts(mnt_check_state_t *check_state)
{
    /* note: getmntent() is not thread safe, uses static area for return val */
    FILE *fp;
    struct mntent *mount_entry;
    char *opt;
    struct stat64 sbuf;
    int return_value = 0;
    char *mntfile = MNTTAB; /* /etc/fstab */
    char *rawdevname;

    if ((fp = setmntent(mntfile, "r")) == NULL) {
	return -1;
    }

    while ((mount_entry = getmntent(fp)) != NULL) {

	/*
	 * mark mounted partitions with fstype 'rawdata' (raw dev optional)
	 * this allows and admin to reserve raw partitions for oracle &
         * friends and the system will attempt to prevent it from being
	 * inadvertantly clobbered.
         *
         * fstab is checked for RAWDATA entries.  If a 'raw=rawdevicename'
	 * entry exists, then use the rawdevicename.  If not, use
         * the filesysname entry.  Either one should point to a valid
	 * raw device.
         *
         * To avoid user confusion, we accept both raw and block devs
 	 */

	if (strcmp(mount_entry->mnt_type, MNTTYPE_RAWDATA) != 0)
	    continue;

	rawdevname = mount_entry->mnt_fsname;
	opt = mnt_find_mount_opt(mount_entry->mnt_opts, MNTOPT_RAW);
	if (opt)
	    rawdevname = opt+4;

	if (!rawdevname)
	    continue;

	/* check raw entry, stat & mark this dev used */
	if (stat64(rawdevname, &sbuf) != -1) {

	    /* verify that it's a char or block device */
	    if ((sbuf.st_mode & S_IFMT) == S_IFCHR
		|| (sbuf.st_mode & S_IFMT) == S_IFBLK) {

		/* mark as used */
		mnt_mark_dev_mounted(check_state, sbuf.st_rdev, MNT_OWNER_RAW);
		return_value++;
	    }
	}
    }

    endmntent(fp);

    return return_value;
}

/* mnt_check_mtab_mounts()
 *
 * Assuming mtab actually has an accurate picture of what's mounted
 * on the system, we use this to find the associated devices and
 * mark partitions as mounted.
 * 
 * ARGUMENTS:
 *  check_state - pointer to valid check_state.
 *
 * RETURN VALUE:
 *  number of partitions marked
 *
 * CREATED:        LAST MODIFIED:
 * 8/1/95 san      9/8/95 san
 */

static
int
mnt_check_mtab_mounts(mnt_check_state_t *check_state)
{
    /* note: getmntent() is not thread safe, uses static area for return val */
    FILE *fp;
    struct mntent *mount_entry;
    char *opt;
    struct stat64 sbuf;
    int return_value = 0;
    char *mntfile = MOUNTED;  /* /etc/mtab */
    char *rawpath;
    dev_t blk_dev;
    dev_t raw_dev;
    struct statvfs64 statvfsbuf;

    /* walk through /etc/mtab */

    if ((fp = setmntent(mntfile, "r")) == NULL) {
	return -1;
    }

    while ((mount_entry = getmntent(fp)) != NULL) {

	/* check for xfs and efs filesystems */

	if (!strcmp(mount_entry->mnt_type, MNTTYPE_XFS)
	    || !strcmp(mount_entry->mnt_type, MNTTYPE_EFS)) {

	    /* see if we can stat the device */
	    if (stat64(mount_entry->mnt_fsname, &sbuf) == -1)
		continue;

	    blk_dev = sbuf.st_rdev;

	    /* if mtab 'raw=' entry exists, save time and use it */

	    opt = mnt_find_mount_opt(mount_entry->mnt_opts, MNTOPT_RAW);
	    if (opt) {
		rawpath = opt+4;
	    } else {
		/* if 'raw=' not specified, use findrawpath to dig up dev */
		rawpath = findrawpath(mount_entry->mnt_fsname);
	    }

	    /* see if we can stat the device */
	    if (stat64(rawpath, &sbuf) == -1)
		    continue;

	    /* verify it's a char device */
	    if ((sbuf.st_mode & S_IFMT) != S_IFCHR)
		    continue;

	    raw_dev = sbuf.st_rdev;

	    /* stat the filesystem mount point */
	    if (stat64(mount_entry->mnt_dir, &sbuf) == -1)
		continue;

	    /* if raw device dev doesn't match mountpoint then skip */
	    if (blk_dev != sbuf.st_dev)
		continue;

	    if (statvfs64(mount_entry->mnt_dir, &statvfsbuf) != -1) {
		    int fstype = MNT_OWNER_UNKNOWN;

		    if (strcmp(statvfsbuf.f_basetype, "xfs") == 0)
			    fstype = MNT_OWNER_XFS;
		    else if (strcmp(statvfsbuf.f_basetype, "efs") == 0)
			    fstype = MNT_OWNER_EFS;

		/* mark partition used - auto tags all xlv & lv vols */
		mnt_mark_dev_mounted(check_state, raw_dev, fstype);
		return_value++;
	    }
	}
    }
    endmntent(fp);

    return return_value;
}
