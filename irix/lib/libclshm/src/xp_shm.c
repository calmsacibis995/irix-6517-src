/*
 * File: xp_shmem.c
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/partition.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <ulocks.h>
#include <strings.h>

#include <sys/SN/clshm.h>
#include <sys/SN/clshmd.h>
#include "xp_shm.h"

#define XP_SHM_DEBUG
#undef XP_SHM_DEBUG

#ifdef XP_SHM_DEBUG
static int 	g_debug_flag = 0; 	/* debug level for dprintfs */

/* debug printing macro */
#define dprintf(level, x)    	\
if (g_debug_flag >= level)  {  	\
    printf x;                	\
    fflush(stdout);          	\
}
#else
#define dprintf(level, x)
#endif

/* defines */
#define CLSHM_PATH 		"/hw/clshm/0/"


/* struct: addr_list_t
 * -------------------
 * To keep track of the shmaddrs that we currently have registered,
 * for debugging purposes mostly.
 */
typedef struct addr_list_s {
    struct addr_list_s *next;
    void *shmaddr;
    size_t size;
} addr_list_t;


/* struct: shmid_list_t
 * --------------------
 * List of key to shmid maps.  This way if we do a shmget again, we don't
 * have to go to the daemon to ask again if we have this cached.  If someone
 * kills this area with a RMID, then it will fail later on when we try to 
 * do an attach.
 */
typedef struct shmid_list_s {
    struct shmid_list_s *next;
    key_t key;
    int shmid;
    size_t size;
} shmid_list_t;


/* Globals */
static int 	g_shm_fd = -1;		/* fd for shared memory maps */
static int 	g_daem_sock = -1;	/* socket to daemon */
static int	g_pagesize = -1;	/* page size that we map in daem */

static shmid_list_t 	*g_shmid_list = NULL;	/* list of known shmids */
static addr_list_t     	*g_addr_list = NULL;	/* list of shmaddrs */
static usptr_t 		*g_lock_file = NULL;	/* lock file used */
static ulock_t		g_lock = NULL;		/* lock for daemon com */


/* if the user has a different major version number different than then
 * daemon, then we just abort.  If we have different minor numbers,
 * then whoever has the larger minor number is in charge of making sure
 * that it is compatable with the lesser minor numbers  */
static int 	g_daem_minor_vers = -1;	/* daem version to check */


/* helper function prototypes */
static void xdump(char *bytes, int num_bytes);
static int first_init(void);
static int open_clshm_fd(char *devend, int flags);
static shmid_list_t *find_key_shmid_pair(key_t key, int shmid);
static int exchange_with_daem(int fd, clshm_msg_t *msg) ;
static int send_to_daem(int fd, clshm_msg_t *msg);
static int recv_from_daem(int fd, clshm_msg_t *msg);


/* xdump
 * -----
 * Parameters
 * 	bytes:		buffer to dump
 *	num_bytes:	number of bytes to dump
 * 
 * Dump the buffer to standard out
 */
/* ARGSUSED */
static void xdump(char *bytes, int num_bytes)
{
#ifdef XP_SHM_DEBUG
    int         i;

    if(! bytes || ! num_bytes)  {
        dprintf(20, ("xdump got ptr or num bytes as 0\n"));
    }
    else  {
        for(i = 0; i < num_bytes; i++)   {
            dprintf(20, ("%d: %x\n", i, bytes[i]));
        }
    }
#endif
    return;
}


/* set_debug_level
 * ---------------
 * Parameters:
 *	level:	The debug level to set this puppy to!
 *
 * Sets the debug level to whatever the usr wants for debugging
 */
/* ARGSUSED */
void set_debug_level(int level)
{
#ifdef XP_SHM_DEBUG
    g_debug_flag = level;
#endif
}


/* dump_usr_state
 * --------------
 * Dump the user state to the current context
 */
void dump_usr_state(void)
{
    shmid_list_t *shmid;
    addr_list_t *addr;

    dprintf(0, ("xp_shmem: DUMP USER STATE:\n\n"));
    dprintf(0, ("\tGENERAL GLOBALS:\n"
		"\tg_shm_fd = %d, g_debug_flag = %d, g_daem_sock = %d\n", 
		g_shm_fd, g_debug_flag, g_daem_sock));
    dprintf(0, ("\n\tSHMID LIST:\n"));
    for(shmid = g_shmid_list; shmid != NULL; shmid = shmid->next) {

#if (_MIPS_SIM == _ABI64)
	dprintf(0, ("\tshmid = 0x%x, key = 0x%x, size = %lld\n", shmid->shmid,
#else /* (_MIPS_SIM == _ABIN32) */
	dprintf(0, ("\tshmid = 0x%x, key = 0x%x, size = %d\n", shmid->shmid,
#endif

		    shmid->key, shmid->size));
    }
    dprintf(0, ("\n\tSHMADDR LIST:\n"));
    for(addr = g_addr_list; addr != NULL; addr = addr->next) {
#if (_MIPS_SIM == _ABI64) 
	dprintf(0, ("\taddr = 0x%llx, size = %lld\n", addr->shmaddr, 
#else /* (_MIPS_SIM == _ABIN32)  */
	dprintf(0, ("\taddr = 0x%x, size = %d\n", addr->shmaddr, 
#endif
		    addr->size));
    }
    dprintf(0, ("DUMP USER STATE DONE\n"));
}


/* dump_daem_state
 * ---------------
 * Tell the daemon to dump state
 */
void dump_daem_state(void)
{
    clshm_msg_t msg;

    if(first_init() < 0) {
	dprintf(5, ("xp_shmem: can't first_init\n"));
	return;
    }

    msg.type = USR_TO_DAEM_DUMP_DAEM_STATE;
    msg.src_part = part_getid();
    msg.dst_part = msg.src_part;
    msg.len = 0;

    if(ussetlock(g_lock) < 0) {
	dprintf(1, ("xp_shmem: can't set lock\n"));
	return;
    }
    if(send_to_daem(g_daem_sock, &msg) < 0) {
	dprintf(0, ("xp_shmem: can't dump daemon info!!!!\n"));
    } else {
	sginap(100);
    }
    if(usunsetlock(g_lock) < 0) {
	dprintf(1, ("xp_shmem: can't unset lock\n"));
    }
}


/* dump_drv_state
 * --------------
 * Tell the daemon to tell the driver to dump state
 */
void dump_drv_state(void)
{
    clshm_msg_t msg;

    if(first_init() < 0) {
	dprintf(5, ("xp_shmem: can't first_init\n"));
	return;
    }

    msg.type = USR_TO_DAEM_DUMP_DRV_STATE;
    msg.src_part = part_getid();
    msg.dst_part = msg.src_part;
    msg.len = 0;

    if(ussetlock(g_lock) < 0) {
	dprintf(1, ("xp_shmem: can't set lock\n"));
	return;
    }
    if(send_to_daem(g_daem_sock, &msg) < 0) {
	dprintf(0, ("xp_shmem: can't dump daemon info!!!!\n"));
    } else {
	sginap(100);
    }
    if(usunsetlock(g_lock) < 0) {
	dprintf(1, ("xp_shmem: can't unset lock\n"));
    }
}


/* dump_all_state
 * --------------
 * Call all the functions to dump state all the way to the driver.
 */
void dump_all_state(void)
{
    sginap(100);
    dump_usr_state();
    dump_daem_state();
    dump_drv_state();
}


/* first_init
 * ----------
 * Needs to be called before we can really do anything with the daemon or
 * shared memory in general.  It gets info from the daemon and opens the
 * appropriate socket for communications, lock for synch, and gets info
 * for version checking.
 */
static int first_init(void)
{
    static int initted = 0;
    clshm_msg_t msg;
    vers_in_msg vers_minor, vers_major;
    pgsz_in_msg page_size;
    struct sockaddr_un sname;
    char lock_file[CLSHM_LOCK_FILE_LEN];
    char *ptr;
    pid_t pid;
    int len, error;

    if(initted) return(0);

    /* initialize the locking for the xp_shmem */
    if(!strcpy(lock_file, CLSHM_BEGIN_LOCK_FILE)) {
	goto first_init_error;
    }

    if((pid = getpid()) == -1) {
	goto first_init_error;
    }

    ptr = &(lock_file[strlen(lock_file)]);
    sprintf(ptr, "%d", pid);
    if(usconfig(CONF_ARENATYPE, US_SHAREDONLY) == -1) {
	goto first_init_error;
    }   

    g_lock_file = usinit(lock_file);
    if(!g_lock_file) {
	goto first_init_error;
    }

    g_lock = usnewlock(g_lock_file);
    if(!g_lock) {
	goto first_init_error;
    }
    usinitlock(g_lock);

    /* open up communication with the daemon */
    if((g_daem_sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	goto first_init_error;
    }

    sname.sun_family = AF_UNIX ;

    if(!strcpy(sname.sun_path, CLSHMD_SOCKET_NAME)) {
	goto first_init_error;
    }

    if (connect(g_daem_sock, (caddr_t)&sname, sizeof(sname)) < 0) {
	goto first_init_error;
    }

    /* check version info with daemon */
    msg.type = USR_TO_DAEM_NEW_USER;
    msg.len = sizeof(vers_in_msg) * 2;
    vers_major = CLSHM_USR_MAJOR_VERS;
    vers_minor = CLSHM_USR_MINOR_VERS;
    bcopy(&vers_major, msg.body, sizeof(vers_in_msg));
    bcopy(&vers_minor, msg.body + sizeof(vers_in_msg), sizeof(vers_in_msg));
    msg.src_part = part_getid();
    msg.dst_part = msg.src_part;
    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
	goto first_init_error;
    }
    if(msg.len != (sizeof(vers_in_msg) * 2) + sizeof(pgsz_in_msg)) {
	errno = EIO;
	goto first_init_error;
    }
    bcopy(msg.body, &vers_major, sizeof(vers_in_msg));
    len = sizeof(vers_in_msg);
    bcopy(msg.body + len, &vers_minor, sizeof(vers_in_msg));
    len += sizeof(vers_in_msg);
    bcopy(msg.body + len, &page_size, sizeof(pgsz_in_msg));
    if(vers_major != CLSHM_USR_MAJOR_VERS) {
        dprintf(0, ("xp_shmem: major version incompatable with daemon\n"
		    "\tlibrary version = %d, daemon version = %d\n", 
		    CLSHM_USR_MAJOR_VERS, vers_major));
        errno = EPROTO;
	goto first_init_error;
    }
    if(g_daem_minor_vers == -1) {
	g_daem_minor_vers = vers_minor;
    }
    g_pagesize = page_size;

    initted = 1;
    return(0);

first_init_error:
    error = errno;
    if(g_daem_sock != -1) {
	close(g_daem_sock);
	g_daem_sock = -1;
    }
    if(g_lock_file) {
	usdetach(g_lock_file);
	g_lock_file = NULL;
    }
    errno = error ? error : EFAULT;
    return(-1);
}


/* open_clshm_fd
 * -------------
 * Parameters:
 *      devend: "shm", or "ctl" for kind of deviced ending
 *              in the hwgraph
 *      flags:  flags to open with
 *      return: the file descriptor for the device
 *
 * Open the given clshm device.
 */
static int open_clshm_fd(char *devend, int flags) 
{
    char devname[32];
    int fd;

    sprintf(devname, "%s%s", CLSHM_PATH, devend);
    dprintf(5, ("xp_shmem: Trying to open %s\n", devname));
    fd = open(devname, flags);
    if(fd < 3) {
        dprintf(5, ("xp_shmem: open_clshm_fd: Couldn't open %s\n", devname));
    }
    return(fd);
}


/* xp_ftok
 * -------
 * Parameters:
 *	path:	should be /hw/clshm/partition/<partid> or NULL for this part
 *	id:	should be 1
 *	return:	-1 for failure and 0 for success
 *
 * Parse the path and make sure it is a partition.  Send and receive a
 * message from the daemon for the key for this new segment that will
 * be returned.
 */
key_t xp_ftok(const char *path, int id)
{
    partid_t partid;
    key_in_msg key;
    clshm_msg_t msg;
    pid_in_msg pid;
    err_in_msg error_num;
    char *cpyptr;
    int len;

    len = (int) strlen(CLSHM_IF_PART_PATH);
    partid = part_getid();
    if(id != CLSHM_IF_ID_DEFAULT || (path != NULL &&
				     strncmp(path, CLSHM_IF_PART_PATH, len))) {
	dprintf(1, ("xp_ftok: bad params\n"));
	errno = EINVAL;
	return(-1);
    }
    
    /* check that the path is in the hwgraph */
    if(path != NULL && strlen(path) != len) {
	if(strlen(path + len) != 2) {
	    dprintf(1, ("xp_ftok: bad path digit length\n"));
	    errno = EINVAL;
	    return(-1);
	}
	if(!isdigit(*(path + len))) {
	    dprintf(1, ("xp_ftok: path partition bad first digit\n"));
	    errno = EINVAL;
	    return(-1);
	}
	partid = ((*(path + len)) - '0') * 16;
	len++;
	if(!isdigit(*(path + len))) {
	    dprintf(1, ("xp_ftok: path partition bad second digit\n"));
	    errno = EINVAL;
	    return(-1);
	}
	partid += (*(path + len)) - '0';
    }
    
    if(first_init() < 0) {
	dprintf(5, ("xp_ftok: can't first_init\n"));
	return(-1);
    }
    dprintf(5, ("xp_ftok: got past initial stuff\n"));

    /* send to daemon */
    msg.type = USR_TO_DAEM_NEED_KEY;
    msg.src_part = part_getid();
    msg.dst_part = partid;
    msg.len = 0;
    cpyptr = msg.body;

    if(pid = getpid() == -1) {
	dprintf(1, ("xp_ftok: can't get pid\n"));
	return(-1);
    }
    bcopy(&(pid), cpyptr, sizeof(pid_in_msg));
    msg.len += sizeof(pid_in_msg);
    cpyptr += sizeof(pid_in_msg);

    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
	dprintf(1, ("xp_ftok: communication error\n"));
	return(-1);
    } 
    
    /* check out the info returned by the daemon */
    if(msg.type != DAEM_TO_USR_TAKE_KEY) {
	dprintf(1, ("xp_ftok: got back bad message for daemon\n"));
	errno = EIO;
	return(-1);
    }

    if(msg.len != sizeof(pid_in_msg) + sizeof(key_in_msg) + 
       sizeof(err_in_msg)) {
	dprintf(1, ("xp_ftok: got back bad message length from daemon\n"));
	errno = EIO;
	return(-1);
    }
    
    cpyptr = msg.body + sizeof(pid_in_msg);
    bcopy(cpyptr, &key, sizeof(key_in_msg));
    cpyptr += sizeof(key_in_msg);
    bcopy(cpyptr, &error_num, sizeof(err_in_msg));
    if(key == CLSHMD_ABSENT_KEY) {
	key = -1;
	if(!error_num) {
	    errno = EIO;
	} else {
	    errno = error_num;
	}
	dprintf(5, ("xp_ftok: got back bad key\n"));
    } else {
	dprintf(5, ("xp_ftok: success\n"));
    }
    
    return(key);
}


/* find_key_shmid_pair
 * -------------------
 * Parameters:
 *	key:	key to find (or absent key to look for shmid)
 *	shmid:	shmid to find (or absend shmid to look for key)
 *	return:	pointer to shmid_list_t that matches or a new one
 * 
 * Look for the given key or shmid (one of them might be absent if
 * we are only looking for them by one of the two ids.  If they are 
 * not found and both are present, then create a new on and put that 
 * on the list.  But only creates a new one if _both_ are present
 * as parameters.  
 */
static shmid_list_t *find_key_shmid_pair(key_t key, int shmid)
{
    shmid_list_t *elem;

    /* find them in current list */
    for(elem = g_shmid_list; elem != NULL; elem = elem->next) {
	if((shmid != CLSHMD_ABSENT_SHMID && elem->shmid == shmid) ||
	   (key != CLSHMD_ABSENT_KEY && elem->key == key)) {
	    break;
	}
    }

    /* make a new one if applicable */
    if(!elem) {
	if(key != CLSHMD_ABSENT_KEY && shmid != CLSHMD_ABSENT_SHMID) {
	    elem = (shmid_list_t *) malloc(sizeof(shmid_list_t));
	    if(elem) {
		elem->key = key;
		elem->shmid = shmid;
		elem->size = 0;
		elem->next = g_shmid_list;
		g_shmid_list = elem;
	    } else {
		dprintf(5, ("xp_shmem: find_key_shmid_pair: out of memory\n"));
		/* this will return NULL since elem isn't assigned */
	    }
	}
    }
    return(elem);
}


/* xp_shmget
 * ---------
 * Parameters:
 *	key:	key to get shmid for
 *	size:	size to assign to this shmid
 *	shmflg:	IGNORED!
 *	return:	-1 for failure and 0 for success
 *
 * See if we have already seen this key before.  In that case, just
 * return what we have gotten back before.  If not send off message to
 * the daemon for the shmid for this key.  
 */
/* ARGSUSED */
int xp_shmget(key_t key, size_t size, int shmflg)
{
    clshm_msg_t msg;
    shmid_in_msg shmid;
    key_in_msg ret_key;
    len_in_msg ret_len, len;
    err_in_msg error_num;
    int cpy_len;
    char *cpy_ptr;
    shmid_list_t *shmid_elem;

    dprintf(5, ("xp_shmget: started\n"));
    if(first_init() < 0) {	
	dprintf(5, ("xp_shmget: can't first_init\n"));
	return(-1);
    }
    
    /* check params */
    if(key == CLSHMD_ABSENT_KEY || size < 1) {
	dprintf(1, ("xp_shmget: bad key or size\n"));
	errno = EINVAL;
	return(-1);
    }

    /* see if we already have gotten this key/shmid pair */
    shmid_elem = find_key_shmid_pair(key, CLSHMD_ABSENT_SHMID);
    if(shmid_elem) {
	if(shmid_elem->size != size) {
	    dprintf(1, ("xp_shmget: bad size\n"));
	    errno = EINVAL;
	    return(-1);
	}
	return(shmid_elem->shmid);
    }

    /* make a message and send off to daemon for the shmid */
    len = size;
    msg.type = USR_TO_DAEM_NEED_SHMID;
    msg.src_part = part_getid();
    msg.dst_part = KEY_TO_PARTITION(key);
    msg.len = sizeof(key_in_msg) + sizeof(len_in_msg);
    ret_key = key;
    bcopy(&(ret_key), msg.body, sizeof(key_in_msg));
    bcopy(&(len), msg.body + sizeof(key_in_msg), sizeof(len_in_msg));
    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
	dprintf(1, ("xp_shmget: communication error\n"));
	return(-1);
    }
    cpy_ptr = msg.body;
    cpy_len = 0;
    bcopy(cpy_ptr, &(ret_key), sizeof(key_in_msg));
    cpy_ptr += sizeof(key_in_msg);
    cpy_len += sizeof(key_in_msg);
    bcopy(cpy_ptr, &(ret_len), sizeof(len_in_msg));
    cpy_ptr += sizeof(len_in_msg);
    cpy_len += sizeof(len_in_msg);
    bcopy(cpy_ptr, &(shmid), sizeof(shmid_in_msg));
    cpy_ptr += sizeof(shmid_in_msg);
    cpy_len += sizeof(shmid_in_msg);
    bcopy(cpy_ptr, &(error_num), sizeof(err_in_msg));
    cpy_len += sizeof(err_in_msg);

    /* make sure we got something sane from the daemon */
    if(msg.type != DAEM_TO_USR_TAKE_SHMID || cpy_len != msg.len) {
	dprintf(1, ("xp_shmget: bad msg from daem\n"));
	errno = EIO;
	return(-1);
    } else if(ret_key != key || ret_len != size || 
	      shmid == CLSHMD_ABSENT_SHMID) {
	dprintf(1, ("xp_shmget: bad return from daem key = %x/%x, len = "
		    "%d/%d, shmid = %x\n", key, ret_key, (int) size, 
		    (int) ret_len, shmid));
	if(error_num) {
	    errno = error_num;
	} else {
	    errno = EIO;
	}
	return(-1);
    }

    shmid_elem = find_key_shmid_pair(key, shmid);
    if(!shmid_elem) {
	dprintf(1, ("xp_shmget: out of memory\n"));
	errno = ENOMEM;
	return(-1);
    }
    shmid_elem->size = size;

    return(shmid);
}


/* xp_shmat
 * --------
 * Parameters:
 *	shmid:		shmid we want to attach to
 *	shmaddr:	address to attach to (NULL for don't care)
 *	shmflg:		same as sys V flag shmat (see man page)
 *	returns:	NULL for failure and address mapped for success
 *
 * Make sure someone called shmget on this shmid and then convert the
 * shmid into an offset to mmap with.  Turns the shmflg into a flag that
 * can be passed to mmap and then mmaps the addr that we want to share
 */
void *xp_shmat(int shmid, const void *shmaddr, int shmflg)
{
    shmid_list_t *shmid_elem;
    void *map_dst, *map_src;
    addr_list_t *addr_elem;
    int flags, protection;
    off_t off;
    
    dprintf(5, ("xp_shmat: started\n"));
    if(first_init() < 0) {
	dprintf(5, ("xp_shmat: can't first_init\n"));
	return(NULL);
    }

    /* needed to have called xp_shmget first */
    shmid_elem = find_key_shmid_pair(CLSHMD_ABSENT_KEY, shmid);
    if(!shmid_elem) {
	dprintf(1, ("xp_shmat: must call xp_shmget on key first\n"));
	errno = EINVAL;
	return(NULL);
    }

    /* open the shared memory mapping file if not open already */
    if(g_shm_fd == -1) {
        g_shm_fd = open_clshm_fd("shm", O_RDWR);
        if(g_shm_fd < 3) {
            dprintf(0, ("xp_shmat: can't open clshm shm device\n"));
            g_shm_fd = -1;
            return(NULL);
        }
    }

    /* get the info from sys V shmem form to mmap form for the flags */
    flags = MAP_SHARED;
    if(shmaddr != NULL) {
	if(shmflg & SHM_RND) {
	    map_src = (void *) ((__uint64_t) shmaddr - 
				(((__uint64_t) shmaddr) % SHMLBA));
	} else {
	    map_src = (void *) shmaddr;
	}
	flags |= MAP_FIXED;
    } else {
	map_src = NULL;
    }
    protection = PROT_READ;
    if(!(shmflg & SHM_RDONLY)) {
	protection = PROT_WRITE;
    }

    /* offset passed into mmap as shmid << # bits to shift for page */
    off = (__uint32_t) shmid_elem->shmid;
    off *= g_pagesize; /* assume page size mult of 2 */

    map_dst = mmap(map_src, shmid_elem->size, protection, flags, 
		   g_shm_fd, off);

    if(map_dst == MAP_FAILED) {
#if (_MIPS_SIM == _ABI64)
	dprintf(5, ("xp_shmat: mmap failed for addr: 0x%llx, len: %lld, "
#else /* (_MIPS_SIM == _ABIN32) */
	dprintf(5, ("xp_shmat: mmap failed for addr: 0x%x, len: %d, "
#endif
		    "prot: %x, flags %x, fd: %d, \n\t"
		    "offset: 0x%llx, shmid = 0x%x, page size = %d\n", 
		    map_src, shmid_elem->size, protection, flags, 
		    g_shm_fd, off, shmid, g_pagesize));
	return(NULL);
    }

    addr_elem = (addr_list_t *) malloc(sizeof(addr_list_t));
    if(!addr_elem) {
	munmap(map_dst, shmid_elem->size);
	dprintf(1, ("xp_shmat: out of memory\n"));
	errno = ENOMEM;
	return(NULL);
    }
    addr_elem->shmaddr = map_dst;
    addr_elem->size = shmid_elem->size;
    addr_elem->next = g_addr_list;
    g_addr_list = addr_elem;

    return(map_dst);
}


/* xp_shmdt
 * --------
 * Parameters:
 *	shmaddr:	address that we mapped
 *	return:		-1 for failure and 0 for success
 *
 * Check to see if we have it cached locally.  If so, then remove it
 * locally.  If not, then we can't detach it.  Basically just munmap it.
 */
int xp_shmdt(const void *shmaddr)
{
    addr_list_t *elem, *prev;

    dprintf(5, ("xp_shmdt: started\n"));
    if(first_init() < 0) {
	dprintf(5, ("xp_shmdt: can't first_init\n"));
	return(-1);
    }

    /* see if we have this address registered */
    for(prev = NULL, elem = g_addr_list; elem != NULL; 
	prev = elem, elem = elem->next) {
	if(elem->shmaddr == shmaddr) {
	    if(prev != NULL) {
		prev->next = elem->next;
	    } else {
		g_addr_list = elem->next;
	    }
	    break;
	}
    }
    if(!elem) {
	dprintf(5, ("xp_shmdt: can't find address trying to detach\n"));
	errno = EINVAL;
	return(-1);
    }

    if(munmap(elem->shmaddr, elem->size) < 0) {
	dprintf(1, ("xp_shmdt: munmap failed\n"));
	return(-1);
    }
    free(elem);
    return(0);
}


/* xp_shmctl
 * ---------
 * Parameters:
 *	shmid:	shmid to send cmd to
 *	cmd:	cmd to send -- only IPC_RMID implemented right now.
 *	return:	-1 on failure and 0 on success
 *
 * Only RMID implemented -- this is sent to the daemon and that shmid
 * is eliminated from the system regardless of who is using it or what
 * is going on with it.  Even if we have not shmgetted it, we can do
 * this call to eliminate it.
 */
int xp_shmctl(int shmid, int cmd, .../* struct shmid_ds *buf */)
{
    int error = 0;

    dprintf(5, ("xp_shmctl: started\n"));
    if(first_init() < 0) {
	dprintf(5, ("xp_shmdt: can't first_init\n"));
	return(-1);
    }

    switch(cmd) {
        case IPC_RMID: {
	    clshm_msg_t msg;	
	    shmid_in_msg msg_shmid;	
	    shmid_list_t *elem, *prev;
	    err_in_msg err;

	    /* see if we have it locally, to delete it */
	    for(prev = NULL, elem = g_shmid_list; elem != NULL;
		prev = elem, elem = elem->next) {
		if(elem->shmid == shmid) {
		    break;
		}
	    }
	    if(!elem) {
		dprintf(5, ("xp_shmctl: failed to find shmid at this addr\n"));
		errno = EINVAL;
		/* still remove even if we don't find cause this might
		 * be a cleanup after a bad death of program!!! */
	    } else {
		if(prev) {
		    prev->next = elem->next;
		} else {
		    g_shmid_list = elem->next;
		}
		free(elem);
	    }

	    /* send the message to the daemon, not expecting a return */
	    msg_shmid = shmid;
	    msg.type = USR_TO_DAEM_RMID;
	    msg.src_part = part_getid();
	    msg.dst_part = SHMID_TO_PARTITION(shmid);
	    msg.len = sizeof(shmid_in_msg);
	    bcopy(&(msg_shmid), msg.body, sizeof(shmid_in_msg));
	    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
		dprintf(1, ("xp_shmctl: can't exchange with daemon\n"));
		error = -1;
	    } else {
		if(msg.len != sizeof(err_in_msg) || 
		   msg.type != DAEM_TO_USR_RMID) {
		    errno = EIO;
		    error = -1;
		    break;
		}
		bcopy(msg.body, &err, sizeof(err_in_msg));
		if(err != 0) {
		    errno = err;
		    error = -1;
		}
	    }
	    
	    break;
	}

        case IPC_AUTORMID: {
	    clshm_msg_t msg;	
	    shmid_in_msg msg_shmid;	
	    err_in_msg err;

	    /* send the message to the daemon, not expecting a return */
	    msg_shmid = shmid;
	    msg.type = USR_TO_DAEM_AUTORMID;
	    msg.src_part = part_getid();
	    msg.dst_part = SHMID_TO_PARTITION(shmid);
	    msg.len = sizeof(shmid_in_msg);
	    bcopy(&(msg_shmid), msg.body, sizeof(shmid_in_msg));
	    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
		dprintf(1, ("xp_shmctl: can't exchange with daemon\n"));
		error = -1;
	    } else {
		if(msg.len != sizeof(err_in_msg) || 
		   msg.type != DAEM_TO_USR_AUTORMID) {
		    dprintf(1, ("xp_shmctl: got back bad message from "
				"daemon for AUTORMID\n"));
		    errno = EIO;
		    error = -1;
		    break;
		}
		bcopy(msg.body, &err, sizeof(err_in_msg));
		if(err != 0) {
		    dprintf(1, ("xp_shmctl: daemon noticed a problem with "
				"what we sent it in AUTORMID\n"));
		    errno = err;
		    error = -1;
		}
	    }
	    break;
	}

        case IPC_STAT:
        case SHM_LOCK:
        case SHM_UNLOCK:
        case IPC_SET: {
	    dprintf(5, ("xp_shmctl: known but not implemented cmd\n"));
	    errno = ENOSYS;
	    error = -1;
	    break;
	}
        default: {
	    dprintf(1, ("xp_shmctl: unknown cmd\n"));
	    errno = EINVAL;
	    error = -1;
	}
    }
    return(error);
}


/* part_getid
 * ----------
 * Parameters:
 *	return:	the partid of the partition we are on.
 *
 * Wrapper for syssgi call to get partition number.
 */
partid_t part_getid()
{
    static partid_t own_part = -1;

    if(own_part == -1) {
	own_part = (partid_t) syssgi(SGI_PART_OPERATIONS, 
				     SYSSGI_PARTOP_GETPARTID);
    }
    return(own_part);
}	


/* part_getcount
 * -------------
 * Parameters:
 *	return:	number of partitions currently up.
 */
int part_getcount()
{
    part_info_t  part_info[MAX_PARTITIONS];

    return((int) syssgi(SGI_PART_OPERATIONS, SYSSGI_PARTOP_PARTMAP,
			MAX_PARTITIONS, part_info));
}


/* part_getlist
 * ------------
 * Parameters:
 *	part_lsit:	allocated list to fill with partition numbers
 *	max_parts:	max partitions we can place in the part list
 *	return:		-1 for failure and # parts placed for success
 *
 * Partitions that are not curretly up are placed in the list as -1
 * for place holders where those partition numbers would have gone.
 */
int part_getlist(partid_t *part_list, int max_parts)
{
    int num_parts, i;
    part_info_t  part_info[MAX_PARTITIONS];

    num_parts = (int) syssgi(SGI_PART_OPERATIONS, SYSSGI_PARTOP_PARTMAP,
			     MAX_PARTITIONS, part_info);
    if(num_parts > max_parts) {
	dprintf(1, ("part_getlist: more partitions than max_parts\n"));
	errno = EINVAL;
	return(-1);
    }
    for(i = 0; i < num_parts; i++) {
	if(part_info[i].pi_flags & PI_F_ACTIVE) {
	    part_list[i] = part_info[i].pi_partid;
	} else {
	    dprintf(5, ("part_getlist: inactive part %d, flags 0x%x\n", 
			part_info[i].pi_partid, part_info[i].pi_flags));
	    part_list[i] = -1;
	}
    }
    return(num_parts);
}


/* part_gethostname
 * ----------------
 * Parameters:
 *	part:		partition to get name of
 *	name:		array already allocated to place in name
 *	max_len:	max length of name array
 *	return:		-1 or error or 0 on success
 *
 * Gets the name from the daemon
 */
int part_gethostname(partid_t part, char *name, int max_len)
{
    clshm_msg_t		msg;
    
    if(!name || max_len < 1) {
	dprintf(1, ("part_gethostname: bad name or max_len passed in\n"));
	errno = EINVAL;
	return(-1);
    }

    /* set up connection first if need be */
    if(first_init() < 0) {
	dprintf(5, ("part_gethostname: can't first_init\n"));
	return(-1);
    }
    
    msg.type = USR_TO_DAEM_NEED_HOSTNAME;
    msg.src_part = part_getid();
    msg.dst_part = part;
    msg.len = 0;

    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
	dprintf(1, ("part_gethostname: communication error\n"));
	return(-1);
    }
	
    /* check the message we got back from the daemon for 
     * correct stuff */
    if(msg.type != DAEM_TO_USR_TAKE_HOSTNAME) {
	dprintf(1, ("part_gethostname: got back bad message from "
		    "daemon\n"));
	errno = EIO;
	return(-1);
    }
    if(!msg.len) {
	dprintf(1, ("part_gethostname: got back no name from "
		    "daemon\n"));
	errno = EIO;
	return(-1);
    }

    if(max_len < msg.len + 1) {
	dprintf(1, ("part_gethastname: max_len passed in not long enough\n"));
	errno = ENOMEM;
	return(-1);
    }

    if(!strncpy(name, msg.body, msg.len)) {
	dprintf(1, ("part_gethostname: strncpy failed\n"));
	return(-1);
    }
    name[msg.len] = '\0';
    return(0);
}


/* part_getpart
 * ------------
 * Parameters:
 *	shmid: 	shmid we want to know what partition owns it.
 *	return:	partid of the partition that owns shmid
 *
 * Extract owning partition from high bits of the shmid.
 */
partid_t part_getpart(int shmid)
{
    shmid_list_t *shmid_elem;

    for(shmid_elem = g_shmid_list; shmid_elem != NULL; 
	shmid_elem = shmid_elem->next) {
	if(shmid_elem->shmid == shmid) {
	    return(SHMID_TO_PARTITION(shmid));
	}
    }
    dprintf(1, ("part_getpart: shmid not registered for this process\n"));
    errno = EINVAL;
    return(-1);
}


/* part_setmld
 * -----------
 * Parameters:
 *	key:		key to associate with pmo stuff
 *	pmo_handle:	handle to associate with key
 *	pmo_type:	type of handle passed in
 *	return:		-1 for error or 0 or success
 *	
 * Associate the given pmo_handle and pmo_type with the given key.  
 * Send this off to the daemon and make sure that no one has done a 
 * xp_shmget before this call.  Need to also make sure that these 
 * parameters are valid (really the shmget has to check this for sure).
 */
int part_setpmo(key_t key, pmo_handle_t pmo_handle, pmo_type_t pmo_type)
{
    clshm_msg_t msg;
    key_in_msg mkey;
    pid_in_msg pmo_pid;
    pmoh_in_msg pmoh;
    pmot_in_msg pmot;
    err_in_msg err;
    pid_t pid;

    /* make sure that the parameters make sense before trying to send
     * them off the the daemon */
    if(KEY_TO_PARTITION(key) != part_getid()) {
	dprintf(1, ("part_setpmo: key not on this partition\n"));
	errno = EINVAL;
	return(-1);
    }

    /* for now we are setting the type to __PMO_MLD cause this is the
     * only type we are accepting */
    if(pmo_type < 0 || pmo_type > __PMO_PMO_NS) {
	dprintf(1, ("part_setpmo: bad pmo_type\n"));
	errno = EINVAL;
	return(-1);
    }

    /* get the pid */
    if((pid = getpid()) <= 0) {
	dprintf(1, ("part_setpmo: can't get pid\n"));
	return(-1);
    }

    if(first_init() < 0) {	
	dprintf(1, ("part_setpmo: can't first init\n"));
	return(-1);
    }

    /* exchange the info with the daemon */
    msg.type = USR_TO_DAEM_TAKE_PMO;
    msg.src_part = part_getid();
    msg.dst_part = msg.src_part;
    pmoh = pmo_handle;
    pmot = pmo_type;
    mkey = key;
    pmo_pid = pid;
    dprintf(5, ("part_setpmo: pid passed in is %d\n", pid));

    bcopy(&mkey, msg.body, sizeof(key_in_msg));
    msg.len = sizeof(key_in_msg);

    bcopy(&pmo_pid, msg.body + msg.len, sizeof(pid_in_msg));
    msg.len += sizeof(pid_in_msg);

    bcopy(&pmoh, msg.body + msg.len, sizeof(pmoh_in_msg));
    msg.len += sizeof(pmoh_in_msg);
    
    bcopy(&pmot, msg.body + msg.len, sizeof(pmot_in_msg));
    msg.len += sizeof(pmot_in_msg);

    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
	dprintf(1, ("part_setpmo: error exchanging with daemon\n"));
	return(-1);
    }
    
    if(msg.len != sizeof(err_in_msg) || msg.type != DAEM_TO_USR_TAKE_PMO) {
	dprintf(1, ("part_setpmo: bad things passed back from daemon\n"));
	errno = EIO;
	return(-1);
    }
    bcopy(msg.body, &err, sizeof(err_in_msg));
    if(err != 0) {
	dprintf(1, ("part_setpmo: got error return from daemon\n"));
	errno = err;
	return(-1);
    }
    return(0);
}


/* part_getnode
 * ------------
 * Parameters:
 *	shmid:		shmid to get the hwpath for
 *	hwpath:		path in the hwgraph that cooresponds to the shmid
 *	max_len:	length of the path array passed into us.
 *	return:		-1 for error or 0 for success.
 *
 * Given the shmid, get the hwgraph path of the first page placed for
 * this shmid.  Since shmid might be mapped across a bunch of different
 * nodes, we only return the first page.  If they are all specified to
 * be on the same node, this will give us the info we really want.
 */
int part_getnode(int shmid, char *hwpath, int max_len)
{
    shmid_in_msg msg_shmid = shmid;
    clshm_msg_t msg;
    int len;

    if(max_len < 1) {
	dprintf(1, ("part_getnode: bad length passed in\n"));
	errno = EINVAL;
	return(-1);
    }
    if(SHMID_TO_PARTITION(shmid) != part_getid()) {
	dprintf(1, ("part_getnode: can only get node for local partition "
		    "owned shmids\n"));
	errno = EINVAL;
	return(-1);
    }

    if(first_init() < 0) {	
	dprintf(1, ("part_getnode: can't first init\n"));
	return(-1);
    }

    msg.type = USR_TO_DAEM_NEED_PATH;
    msg.len = sizeof(shmid_in_msg);
    bcopy(&msg_shmid, msg.body, sizeof(shmid_in_msg));
    msg.src_part = part_getid();
    msg.dst_part = msg.src_part;
    
    if(exchange_with_daem(g_daem_sock, &msg) < 0) {
	dprintf(1, ("part_getnode: error exchanging with daemon\n"));
	return(-1);
    }

    if(msg.len < sizeof(shmid_in_msg) || msg.len > MAX_MSG_LEN) {
	dprintf(1, ("part_getnode: error in response from daemon\n"));
	errno = EFAULT;
	return(-1);
    }

    /* check the shmid returned */
    bcopy(msg.body, &msg_shmid, sizeof(shmid_in_msg));
    if(msg_shmid != shmid) {
	if(msg_shmid == CLSHMD_ABSENT_SHMID) {
	    dprintf(1, ("part_getnode: invalid shmid passed\n"));
	    errno = EINVAL;
	} else {
	    dprintf(1, ("part_getnode: bad shmid returned from daemon\n"));
	    errno = EFAULT;
	}
	return(-1);
    }

    if(msg.len == sizeof(shmid_in_msg)) {
	dprintf(1, ("part_getnode: couldn't get a path name in driver\n"));
	errno = EINVAL;
	return(-1);
    }

    len = (int) (msg.len - sizeof(shmid_in_msg));
    if((len + 1) > max_len) {
	dprintf(1, ("part_getnode: can't fit in size of buffer passed\n"));
	errno = ENAMETOOLONG;
	return(-1);
    }

    bcopy(msg.body + sizeof(shmid_in_msg), hwpath, len);
    hwpath[len] = '\0';

    return(0);
}


/* exchange_with_daem
 * ------------------
 * Parameters:
 *	fd:	socket to daemon
 *	msg:	msg to send to daemon and where the answer comes back in
 *	return:	-1 for failure and 0 for success 
 *
 * Both send and receive from the daem and do so in with a lock around it
 * so that there are no conflicts if multiple threads are created after
 * the init of this library.
 */
static int exchange_with_daem(int fd, clshm_msg_t *msg) 
{
    int error = 0;
    partid_t dst, src;

    if(!msg) {
	dprintf(1, ("xp_shmem: bad msg pointer\n"));
	errno = EFAULT;
	return(-1);
    }

    src = part_getid();
    dst = msg->dst_part;

    if(msg->src_part != src) {
	dprintf(1, ("xp_shmem: src of msg must be local not %d part\n",
		    msg->src_part));
	errno = EIO;
	return(-1);
    }
    if(ussetlock(g_lock) < 0) {
	dprintf(1, ("xp_shmem: can't set lock\n"));
	return(-1);
    }
    dprintf(5, ("xp_shmem: sending to daemon\n"));
    if(send_to_daem(fd, msg) < 0) {
	dprintf(1, ("xp_shmem: couldn't send to daemon\n"));
	error = -1;
    } else {
	dprintf(5, ("xp_shmem: receiving from daemon\n"));
	if(recv_from_daem(fd, msg) < 0) {
	    dprintf(1, ("xp_shmem: couldn't get from daemon\n"));
	    error = -1;
	}
	dprintf(5, ("xp_shmem: done sending and receiving\n"));
    }
    if(usunsetlock(g_lock) < 0) {
	dprintf(1, ("xp_shmem: can't unset lock\n"));
	error = -1;
    }
    /* if the partition we are after is dead, then just tell 
     * the user that things are invalid */
    if(msg->type == DAEM_TO_USR_PART_DEAD) {
	errno = EINVAL;
	error = -1;
    } else if(msg->src_part != dst || msg->dst_part != src) {
	 /* make sure that the daemon is sending from and to who we think!!!*/
	dprintf(1, ("xp_shmem: bad pre src %d/dst %d match for new src %d/"
		    "dst %d\n", src, dst, msg->src_part, msg->dst_part));
	errno = EIO;
	error = -1;
    }

    return(error);
}


/* send_to_daem
 * ------------
 * Parameters:
 *	fd:	file to send the daemon on (daem socket)
 *	msg:	message to send to daemon
 *
 * Send a header and a message all in one buffer so that we know that when
 * the daemon blocks to get our header it doesn't have to also block to get
 * our message as well.
 */
static int send_to_daem(int fd, clshm_msg_t *msg)
{
    clshm_msg_hdr_t	hdr;
    char 		buff[sizeof(clshm_msg_hdr_t) + sizeof(clshm_msg_t)];
    int 		comb_size;

    if(fd == -1 || msg == NULL) {
	dprintf(5, ("xp_shmem: send_to_daem: bad parameters\n"));
	errno = EFAULT;
	return(-1);
    }

    /* place both header and message in one buffer to make sure that
     * the daem can read both the header and message in one go without
     * having to worry about blocking */
    hdr.type = USR_TO_DAEM_HDR;
    hdr.len = (int) (sizeof(msg->type) + sizeof(msg->src_part) + 
	sizeof(msg->dst_part) + sizeof(msg->dummy) + sizeof(msg->len) +
	msg->len);

    comb_size = (int) (sizeof(clshm_msg_hdr_t) + hdr.len);

    bcopy(&hdr, buff, sizeof(clshm_msg_hdr_t));
    bcopy(msg, buff + sizeof(clshm_msg_hdr_t), hdr.len);

    if(write(fd, buff, comb_size) != comb_size) {
	dprintf(5, ("xp_shmem: send_to_daem: trouble writing to daem\n"));
	return(-1);
    }

    xdump(buff, comb_size);

    return(0);
}


/* recv_from_daem
 * --------------
 * Parameters:
 * 	fd:	daem socket
 *	msg:	message to read the stuff into
 *
 * Block until we get a message from the daemon.  We don't know that 
 * the daem will send the header and the message in one go so we have to 
 * block again.  Trust the daemon to never do us wrong, so don't do a
 * timeout and block forever.  If the daemon screws up, we're toast
 * anyways.
 */
static int recv_from_daem(int fd, clshm_msg_t *msg)
{
    clshm_msg_hdr_t	hdr;
    fd_set fd_r_var;
    
    if(fd == -1 || msg == NULL) {
	dprintf(5, ("xp_shmem: recv_from_daem: bad parameters\n"));
	errno = EFAULT;
	return(-1);
    }

    /* wait for daemon to send to us header */
    FD_ZERO(&fd_r_var);
    FD_SET(fd, &fd_r_var);
    if(select(fd+1, &fd_r_var, NULL, NULL, NULL)  <= 0) {
	dprintf(5, ("xp_shmem: recv_from_daem: trouble in select\n"));
	return(-1);
    }
    if(read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	dprintf(5, ("xp_shmem: recv_from_daem: trouble reading header\n"));
	return(-1);
    }

    xdump((char *) &hdr, sizeof(hdr));

    if(hdr.len < sizeof(msg->type) + sizeof(msg->src_part) + 
       sizeof(msg->dst_part) + sizeof(msg->dummy) + sizeof(msg->len)) {
	dprintf(5, ("xp_shmem: recv_from_daem:  header has bad length %d\n", 
		    hdr.len));
	errno = EIO;
	return(-1);
    }

    /* now wait for the actual message */
    FD_ZERO(&fd_r_var);
    FD_SET(fd, &fd_r_var);
    if(select(fd+1, &fd_r_var, NULL, NULL, NULL) <= 0) {
	dprintf(5, ("xp_shmem: recv_from_daem: trouble in select\n"));
	return(-1);
    }
    if(read(fd, msg, hdr.len) != hdr.len) {
	dprintf(5, ("xp_shmem: recv_from_daem: trouble reading msg\n"));
	return(-1);
    }

    xdump((char *) msg, hdr.len);

    return(0);
}

