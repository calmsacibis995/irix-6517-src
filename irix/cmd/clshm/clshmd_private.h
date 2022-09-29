#ifndef __CLSHMD_PRIVATE_H__
#define __CLSHMD_PRIVATE_H__
/*
 * File: clshmd_private.h
 *
 *	Header file to be included by clshmd only.  Internal definitions.
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

/* states for a remote partition to be in */
#define CLSHMD_UNCONNECTED	0x0
#define CLSHMD_DRV_CONNECTED	0x1
#define CLSHMD_PIPE_CONNECTED	0x2
#define CLSHMD_DAEM_CONNECTED	0x4	
#define CLSHMD_CONNECTED	(CLSHMD_DRV_CONNECTED |		\
                                 CLSHMD_PIPE_CONNECTED | 	\
				 CLSHMD_DAEM_CONNECTED)


/* clshmd_hostname_t
 * -----------------
 * For hostnames that we have asked other daemons for and not got
 * answer yet
 */
typedef struct clshmd_hostname_s {
    int		fd; 			/* file to send answer to */
    partid_t	dst_part;		/* what partition name wanted */
    struct clshmd_hostname_s *next;
} clshmd_hostname_t;

/* to make and parse through info in a key */
#define	KEY_TO_UNIQUE(k)	((k) & 0x00ffffff)
#define	MAKE_KEY(part, u)	(((part) << 24) | (u))
#define MAX_KEY			0x00ffffff

/* clshmd_key_t
 * ------------
 * For keys that we have asked other daemons for and not got answer yet
 */
typedef struct clshmd_key_s {
    pid_in_msg	pid;	/* pid of asking process */
    int		fd;	/* file to send answer on */
    partid_t	dst_part; /* who did we ask for key? */
    struct clshmd_key_s *next;
} clshmd_key_t;


/* clshmd_shmid_t
 * --------------
 * For shmids that we have asked other daemons or the driver for and not 
 * got answer yet
 */
typedef struct clshmd_shmid_s {
    key_t key;		/* key of shmid we want */
    int fd;		/* file to send back answer on when we get it */
    partid_t dst_part;	/* partition to send back to */
    struct clshmd_shmid_s *next;
} clshmd_shmid_t;


/* clshmd_path_t
 * -------------
 * For getting the path of a node that owns a partition in the hwgraph
 */
typedef struct clshmd_path_s {
    shmid_in_msg shmid;		/* shmid want path for */
    int fd;			/* user to send back to */
    struct clshmd_path_s *next;
} clshmd_path_t;


/* clshmd_autormid_t
 * -----------------
 * For requests of autormid to be set up
 */
typedef struct clshmd_autormid_s {
    int fd;			/* user to send back to */
    shmid_in_msg shmid;		/* shmid to autormid */
    struct clshmd_autormid_s *next;
} clshmd_autormid_t;


/* clshmd_free_offsets
 * -------------------
 * Free offsets that we can use to make new shmids with
 */
typedef struct clshmd_free_offsets_s {
    __uint32_t	page_off;	/* offset of start of free space */
    __uint32_t	num_pages;	/* num pages free in a row */
    struct clshmd_free_offsets_s *next;
} clshmd_free_offsets_t;


/* rpart_info_t
 * ------------
 * structure for info about the all other partitions
 */
typedef struct {
    partid_t part_num; 	/* partid for this partition */
    char connect_state;	/* state of connection to partition */
    int daem_fd;	/* file descriptor to the daemon of this
			 * partition */

    int daem_minor_vers; /* minor version number for that daemon */
    /* if the user has a different major version number different than then
     * daemon, then we just abort.  If we have different minor numbers,
     * then whoever has the larger minor number is in charge of making sure
     * that it is compatable with the lesser minor numbers  */

} rpart_info_t;


/* clshmd_shared_area_t
 * --------------------
 * areas that we have mapped to other partitions and our own.  And areas
 * we have mapped from other partitions.  keep all info we need to access
 * it and deal with it in any way.
 */
typedef struct clshmd_shared_area_s {

#   define INCOMPLETE_SHARED	0	/* key known only */
#   define WAITING_SHARED	1	/* waiting for page allocation */
#   define COMPLETE_SHARED	2	/* ready for mmap */
    int state;			/* is this space complete */

    err_in_msg error;		/* error for sending back to user */
    key_in_msg key;		/* key of this space */
    shmid_in_msg shmid;		/* shmid of this space */
    len_in_msg len;		/* len of space */

    pid_in_msg pmo_pid;		/* pid that that pmo handle is part of */
#   define ABSENT_PMO_HANDLE	-1
    pmoh_in_msg pmo_handle;	/* pmo handle for owning nodeid */
#   define ABSENT_PMO_TYPE	-1
    pmot_in_msg pmo_type;	/* pmo type for owning nodeid */
    char *nodepath;		/* path of node that memory is placed on */

    int num_addrs;		/* number of pages for space */
    kpaddr_in_msg *kpaddrs;	/* list of pointer to pages */
    int num_msgs; 		/* number of messages needed to send */
    int *msg_recv;		/* messages received booleans */

#   define AUTO_CLEANUP_ON	0x01	/* set to auto clean up this space */
#   define FIRST_ATTACH_SEEN	0x02	/* we know the area has been 
					 * attached at least once */
    int auto_cleanup;		/* autocleanup flags */

#   define AREA_ASKED_FOR	0x01	/* has partition asked for area */
#   define AREA_ATTACHED	0x02	/* has partition attached this area */
    int parts_state[MAX_PARTITIONS]; /* partitions to send RMID to */
    struct clshmd_shared_area_s *next;
} clshmd_shared_area_t;


#endif /* C || C++ */
#endif /* !__CLSHMD_PRIVATE_H__ */
