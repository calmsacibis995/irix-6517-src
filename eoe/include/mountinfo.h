
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

#if !defined(MOUNTINFO_H)
#define MOUNTINFO_H

#if defined(__cplusplus)
extern "C" {
#endif

/* mountinfo.h
 *
 * Header for disk volume/partition check routines
 */

#include <diskinvent.h>
#include <stdio.h>

/* test if dev indicates a logical volume */
#define mnt_is_logical_vol(d) (major(d) == XLV_MAJOR)

/* the part check struct:
   Each partition with a list pointer to track all partitions on a device
   */

typedef struct mnt_partition_entry_s {
    dev_t device;          /* device we're associated with */
    int start_lbn;         /* starting logical block number */
    int end_lbn;           /* ending logical block */
    struct mnt_device_entry_s *devptr;  /* point back to our device entry */
} mnt_partition_entry_t;

typedef struct mnt_partition_table_s {
    struct mnt_device_entry_s *dev_entry;  /* vol header entry */
    char ptable_loaded;                    /* flag ptable loaded? */
    mnt_partition_entry_t ptable[NPARTAB];
    struct mnt_partition_table_s *next;
} mnt_partition_table_t;

#define MNT_OWNER_NONE 0
#define MNT_OWNER_XLV  1
#define MNT_OWNER_XFS  2
#define MNT_OWNER_SWAP 3
#define MNT_OWNER_EFS  4
#define MNT_OWNER_RAW  5
#define MNT_OWNER_DEL     6
#define MNT_OWNER_UNKNOWN 6

#define MNT_DEV_MISSING_NONE  0x0
#define MNT_DEV_MISSING_BLOCK 0x1
#define MNT_DEV_MISSING_RAW   0x2
#define MNT_DEV_MISSING_BOTH  0x3
#define MNT_DEV_MISMATCH      0x4

typedef struct mnt_device_entry_s {
    char valid;                          /* is this entry a valid device */
    char mounted;                        /* is this device mounted (in use) */
    char dev_missing;                    /* char or block dev missing */
    char ptable_loaded;                  /* has the part table been loaded? */
                                         /* if char dev missing, pathname points to tmp */
                                         /* this file needs to be removed */
    char pathname[DSK_DEVNAME_MAX];      /* pathname to raw dev */
    int owner;
    dev_t owner_dev;
    mnt_partition_entry_t *part;         /* pointer to the partition info */
    struct mnt_device_entry_s *next;
} mnt_device_entry_t;

/* conflict collections */
typedef struct mnt_plist_s {
    int done;
    int count;
    int causes;
    struct mnt_plist_entry_s *head;
    struct mnt_plist_entry_s *walk;
    struct mnt_plist_return_s *ret;
} mnt_plist_t;

typedef struct mnt_plist_entry_s {
    int sequence;
    mnt_partition_entry_t *ptable;
    int part;
    int cause;
    struct mnt_plist_entry_s *next;
} mnt_plist_entry_t;

#define MNT_CAUSE_NONE       0x00  
#define MNT_CAUSE_MOUNTED    0x01  /* partition already mounted */
#define MNT_CAUSE_OVERLAP    0x02  /* partitions overlap */
#define MNT_CAUSE_NODEV      0x04  /* no /dev/rdsk /dev/dsk entry */
#define MNT_CAUSE_UNUSED     0x08  /* unallocated partition */
#define MNT_CAUSE_MULTIMOUNT 0x10  /* multiple owners */
#define MNT_CAUSE_LVOL_OWNED 0x20  /* owned by logical vol */
#define MNT_CAUSE__END       0x40  /* last entry */

#define MNT_PLIST_RETURN_VER 1
typedef struct mnt_plist_return_s {
    int version;                /* which version of this struct */
    int sequence;               /* sequence number for this conflict */
    char *pathname;             /* device pathname */
    dev_t dev;                  /* our dev_t */
    int dev_missing;            /* chr/blk dev missing flags */
    mnt_partition_entry_t *ptable;  /* owner ptable */
    int part_num;               /* which partition in table */
    int start_lbn;              /* starting block */
    int end_lbn;                /* ending block */
    int owner;                  /* which type of dev owns us? */
    char mounted;               /* is partition mounted?  Multiple? */
    dev_t owner_dev;            /* dev_t of device that owns us */
    int cause;                  /* what caused this entry */
} mnt_plist_return_t;

#define MNT_CHECK_STATE_MAGIC 0xb1eed
typedef struct mnt_check_state_s {
    int magic;
    mnt_partition_table_t *partition_list;
    mnt_device_entry_t *device_list;
    mnt_plist_t *conflict_list;
} mnt_check_state_t;

/* prototypes */

int mnt_causes_test(mnt_check_state_t *, int);
void mnt_causes_show(mnt_check_state_t *, FILE *, char *);
void mnt_plist_walk_clear(mnt_check_state_t *);
mnt_plist_return_t *mnt_plist_walk_next(mnt_check_state_t *);
void mnt_plist_walk_reset(mnt_check_state_t *);
int mnt_plist_walk_count(mnt_check_state_t *);
void mnt_plist_show(mnt_check_state_t *, FILE *, char *);
int mnt_find_missing_dev_entries(mnt_check_state_t *);
int mnt_find_unmounted_partitions(mnt_check_state_t *);
int mnt_find_existing_mount_conflicts(mnt_check_state_t *);
int mnt_find_mount_conflicts(mnt_check_state_t *, char *);
/* XXXsbarr put this back when we have a generic char dev_t to block dev_t
   routine
   int mnt_find_dev_mount_conflicts(mnt_check_state_t *, dev_t);
*/
int mnt_find_mounted_partitions(mnt_check_state_t *, char *);
int mnt_find_dev_mounted_partitions(mnt_check_state_t *, dev_t);
int mnt_check_init(mnt_check_state_t **);
int mnt_check_end(mnt_check_state_t *);
int mnt_check_and_mark_mounted(mnt_check_state_t *, char *);

char *mnt_string_cause(int);
char *mnt_string_owner(int);

#if defined(__cplusplus)
}
#endif

#endif
