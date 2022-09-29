#ifndef __CLSHM_DEV_H__
#define __CLSHM_DEV_H__

/*
 * clshmdev.h
 *
 *	Driver header file for shared-memory across partitions over 
 *      CrayLink.
 *
 *
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */



#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* file defines */
#define EDGE_LBL_CLSHM  	"clshm"
#define EDGE_LBL_CLSHM_PART	"partition"
#define CLSHM_PART_PERMS	0444
#define CLSHM_CTL_DEV_PERMS 	0600
#define CLSHM_SHM_DEV_PERMS 	0666

#define MAX_UNITS		1		/* support 1 unit for now */
#define CLSHM_MAX_CLONES        8       	/* internal use */
#define MAX_CLSHM_PGS	  	0x00ffffff   	/* max offset we have */

/* default init defines */
#define CLSHM_DEF_NUM_PAGES	32		/* default max pages overall */

#define CLSHM_DEF_PAGE_SIZE	NBPP		/* default page size */

/* number of pages shared between daemon and driver by default */
#define CLSHM_DEF_DAEM_PAGES	(((MAX_OUTSTAND_MSGS * sizeof(clshm_msg_t)) \
                                   / CLSHM_DEF_PAGE_SIZE) + 1)

#define CLSHM_DEF_DAEM_TIMEOUT	100  /* default max time to wait before
				      * daemon checks for messages from
				      * driver */

#define CLSHM_MAX_DELETE_PIDS	100  /* random define for deleting broken
				      * pids from internal structures */


/* locking macros */
#define	LOCK(x) 	mutex_lock(&(x), PZERO)
#define	UNLOCK(x)	mutex_unlock(&(x))


/* device defines */
#define IS_CTL_DEV(d_info)	((d_info)->type == CTL_TYPE)
#define IS_SHM_DEV(d_info)	((d_info)->type == SHM_TYPE)

/* constants for removing processes */
#define	THIS_PROCESS	-1
#define	ALL_PROCESSES	0


/* clshm_dev_info_t 
 * ----------------
 * structure saved as the device to be passed into all io calls into driver
 */
typedef struct clshm_dev_info_s  {
    char	type;
#   define  	CTL_TYPE        0
#   define  	SHM_TYPE    	1
    void	*dev_parms;
    __uint32_t	unit;
} clshm_dev_info_t;


/* remote_map_record_t
 * -------------------
 * All the remote pages that we have mapped from a particular partition
 * Saved on a per partition bases
 */
typedef struct remote_map_record_s {
    int		shmid;		/* shmid for this record */
    size_t	len;		/* len of segment */
    off_t	off;		/* offset */
    uint	num_pages;	/* num pages in array */
    paddr_t	*paddrs;     	/* array of physical addresses */
    pfn_t	*pfns;		/* array of physical page numbers */
    uint	num_msgs;	/* num entries in msg_recv array */
    uint	*msg_recv;	/* boolean array of message receives */
    uint	ready;		/* boolean to mean got all messages */
    uint	count;		/* how many maps of page */
    struct remote_map_record_s	*next;
} remote_map_record_t;


/* map_tab_entry_t
 * ---------------
 * A structure that keeps track of on process that has mapped a shared
 * region.  It points to a remote_map_record_t of the pages that it has
 * mapped into the users space
 */
typedef	struct map_tab_entry_s {
    ulong_t		pid; 		/* pid that did the mmap */
    ulong_t		gid;	     	/* to kill forked procs */
    __uint32_t		signal_num;	/* send this signal on error */
    remote_map_record_t *page_info;	/* pointer into remote_parts */
    char		err_handled;	/* err handled on kpaddr? */
    __psunsigned_t	vt_handle;	/* to get vaddr @ recovery */
    struct map_tab_entry_s *next;
} map_tab_entry_t;


/* kernel_page_t
 * -------------
 * Structure keeps track of the info we need for a page that we are
 * letting be mapped on our partition and on others were the pages are
 * allocated on our partition.
 */
typedef	struct kernel_page_s {
    pfd_t	*pfdat;
    paddr_t	kpaddr; /* allocated physical address */
} kernel_page_t;


/* local_map_record_t
 * ------------------
 * Structure for the external info about a page that we are letting ourself
 * and others map into their space.  This is kept in a general link list
 * and _not_ per partition.
 */
typedef struct local_map_record_s {
    int			shmid;		/* shmid of this record */
    size_t		len;		/* length */
    off_t		off;		/* offset */
    cnodeid_t	       	nodeid;		/* node where shmid was placed --
					 * placed on one node */
    kernel_page_t	*pages;     	/* array of pages */
    uint		num_pages;	/* num pages in array */
    struct local_map_record_s *next;
} local_map_record_t;


/* remote_partition_t
 * ------------------
 * Structure for the different info that we are keeping about every
 * partition that we have knowledge of.
 */
typedef struct remote_partition_s {	
#define CLSHM_PART_S_PART_UP		0x1
#define CLSHM_PART_S_DAEM_CONTACT	0x2
    int			state;		/* state of partition */
    partid_t		partid;	       	/* partition */
    map_tab_entry_t 	*map_tab;	/* link list of map tables */
    remote_map_record_t	*rem_page_info;	/* link list of mapped from this
					 * remote partition */
} remote_partition_t;


/* clshm_dev_t
 * -----------
 * General structure that holds all the state of the overall driver
 */
typedef	struct clshm_dev_s  {
    char	clshm_magic[8];	/* "clshm\0" */
    /* main semaphore for open/close/map/ioctl */
    mutex_t  devmutex;

    /* Following defines state of clshmctl devices */
#   define CL_SHM_C_ST_INITTED  (1 << 0)    /* inited */
#   define CL_SHM_C_ST_MMAPPED  (1 << 1)    /* ctl mmap done */
#   define CL_SHM_C_ST_OPENNED	(1 << 2)    /* job open */
#   define CL_SHM_C_ST_EXOPEN	(1 << 3)    /* job openned exclusive */
#   define CL_SHM_C_ST_SHUT	(1 << 4)    /* shutdown pending */
    char    	c_state;

    /* Following fields setup from CL_SHM_SET_CFG ioctl */
    __uint32_t	max_pages;	/* max pgs per job */
    __uint32_t 	page_size;	/* size of a page in bytes */
    __uint32_t 	clshmd_pages;	/* daemon resources */
    __uint32_t  clshmd_timeout; /* daemon timeout */

    void 	*clshmd_ref; 	/* to kill clshmd daemon */
    caddr_t 	clshmd_buff; 	/* for kern-clshmd comm. */
    __psunsigned_t clshmd_ID;  	/* handle from clshmd mmap */

    /* if the user has a different major version number different than then
     * daemon, then we just abort.  If we have different minor numbers,
     * then whoever has the larger minor number is in charge of making sure
     * that it is compatable with the lesser minor numbers  */
    char	daem_minor_vers;	/* version num of clshm s/w */

    /* "map_records" records (in an allocating part) about the
    ** cross-part buffers that have already been allocated
    */
    local_map_record_t	*loc_page_info;	/* link list of mapped by this
					 * remote partition */

#   define	LOCALITY_NOT_IMP        -1
#   define 	UNOCCUPIED_PART_ENTRY	-1   /* 0xff is never a good part */
    remote_partition_t	remote_parts[MAX_PARTITIONS];

}  clshm_dev_t;


#define	SET_MAGIC(dev)  { \
        (dev)->clshm_magic[0] = 'c'; \
        (dev)->clshm_magic[1] = 'l'; \
        (dev)->clshm_magic[2] = 's'; \
        (dev)->clshm_magic[3] = 'h'; \
        (dev)->clshm_magic[4] = 'm'; \
        (dev)->clshm_magic[5] = '\0'; \
	}

#define CHECK_CL_SHM_C(dev) { \
	ASSERT(((dev)->clshm_magic[0] == 'c') &&   \
	       ((dev)->clshm_magic[1] == 'l') &&   \
	       ((dev)->clshm_magic[2] == 's') &&   \
	       ((dev)->clshm_magic[3] == 'h') &&   \
	       ((dev)->clshm_magic[4] == 'm') &&   \
	       ((dev)->clshm_magic[5] == '\0'));   \
	}

#endif /* C || C++ */
#endif /* ! __CLSHM_DEV_H__ */


