/*
 * File: xp_shm.h
 * Purpose: Cross partition shared memory similar to the IPC shared memory
 * 		facility.
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
#ifndef _XP_SHM_H_
#define _XP_SHM_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/pmo.h>
#include <sys/types.h>
#include <sys/SN/SN0/arch.h> 	/* for MAX_PARITIONS */

/* version control */
#define CLSHM_USR_MAJOR_VERS	0x01
#define CLSHM_USR_MINOR_VERS	0x01


/* has someone else defined this in sys/ipc.h yet ??? */
#ifndef IPC_AUTORMID
#define IPC_AUTORMID	15
#endif

#define CLSHM_IF_ID_DEFAULT	1
#define CLSHM_IF_PART_PATH	"/hw/clshm/partition/"
#define CLSHM_BEGIN_LOCK_FILE	"/tmp/.clshm_usr_lock_"
#define CLSHM_LOCK_FILE_LEN 	64

/* cross partition shared memory */
extern key_t xp_ftok(const char *path, int id);
extern int xp_shmget(key_t key, size_t size, int shmflg);
extern void *xp_shmat(int shmid, const void *shmaddr, int shmflg);
extern int xp_shmdt(const void *shmaddr);
extern int xp_shmctl(int shmid, int cmd, .../*struct shmid_ds *buf*/);

/* partition helpers */
extern partid_t part_getid(void);
extern int part_getcount(void);
extern int part_getlist(partid_t *part_list, int max_parts);
extern int part_gethostname(partid_t part, char *name, int max_len);
extern partid_t part_getpart(int shmid);

/* mld interface */
extern int part_setpmo(key_t key, pmo_handle_t pmo_handle, 
		       pmo_type_t pmo_type);
extern int part_getnode(int shmid, char *hwpath, int max_len);

/* debugging commands */
extern void set_debug_level(int level);
extern void dump_usr_state(void);
extern void dump_daem_state(void);
extern void dump_drv_state(void);
extern void dump_all_state(void);

#ifdef __cplusplus
}
#endif

#endif /* !_XP_SHM_H_ */
