/*
 * clshmd.c
 * --------
 * clshm daemon to communicate information between partitions for
 * cross partition shared memory.
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
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/partition.h>
#include <sys/syssgi.h>
#include <sys/param.h>
#include <sys/pmo.h>

#include <sys/socket.h>
#include <sys/procfs.h>
#include <dirent.h>
#include <paths.h>
#include <sys/mac.h>
#include <sys/un.h>

#include <sys/SN/clshm.h>
#include <sys/SN/clshmd.h>
#include "clshmd_private.h"

extern	int	errno;


/* paths for hardware devices we need to use */
#define XP_ADMIN_PATH   "/hw/xplink/admin"
#define	CLSHMCTL_PATH	"/hw/clshm/"


/* should be fixed in partition.c -- so pActivate and pActivateHB will
 * return !PI_F_ACTIVE from SYSSGI_PARTOP_PARTMAP, but at least return 
 * the partid !!!!!! */
#define PARTMAP_FIXES_THIS
#ifdef PARTMAP_FIXES_THIS
#define MAX_DISCOVERY_TRIES 100
#endif


#define	CLSHMD_DEBUG
/* comment this next line out for debug messages */
#undef CLSHMD_DEBUG

#ifdef CLSHMD_DEBUG
int	clshmd_debug = 10;
#define	dprintf(lvl, x) if (clshmd_debug >= lvl) printf x
#else   /* ! CLSHMD_DEBUG */
#define dprintf(lvl, x)
#endif  /* ! CLSHMD_DEBUG */


/* malloc with a check */
static void *check_malloc(int num_bytes) 
{
    void *x;

    x = malloc(num_bytes);
    if(x == NULL) {
	fprintf(stderr, "clshmd: out of memory\n");
	exit(1);
    }
    return(x);
}

#define check_free(x) free(x)


/* return if socket is closed from recv_from_usr */
#define CLSHMD_SOCKET_CLOSED	-5


/* HELPER FUNCTIONS */

/* debugging functions */
static  void 	xdump(char *bytes, int num_bytes);
static 	void  	dump_daemon_state(void);

/* sending messages */
static	int	send_to_usr(int fd, clshm_msg_t *msg);
static	int	send_to_daem(partid_t dest, clshm_msg_t *msg);
static 	int 	send_to_drv(clshm_msg_t *msg);

/* receiving messages */
static  int	recv_from_usr(int fd, clshm_msg_t *msg);
static	int	recv_from_daem(int fd, clshm_msg_t *msg);
static	int	recv_from_drv(clshm_msg_t *msg);

/* processing messages */
static	int	process_usr_msg(int fd, clshm_msg_t *msg);
static	int	process_driver_msg(clshm_msg_t *msg);
static	void	process_daem_msg(clshm_msg_t *msg);

/* coming up management */
static 	int	init_client_socket(void);
static 	int	is_clshmd_running(void);
static  void 	i_am_coming_up(partid_t dest);
static 	int 	open_part_cl_dev(partid_t part_num);
static  void   	find_new_parts(void);
static 	void 	try_unconnected_parts(void);

/* going down management */
static 	void	close_xp_fds(void);
static 	void	unmap_shared_kernel_region(void);
static 	void 	clean_up_dead_part(partid_t part_num);
static  int 	check_for_closed_fd(partid_t part_num);
static 	void 	delete_unfinished_business(int fd, partid_t part_num);

/* dealing with really owned local structures */
static	int	set_local_hostname(char buf[MAX_MSG_LEN]);
static 	int	insert_free_page_range(int page_off, int num_pages);
static 	int	get_free_page_range(int num_pages);

/* manipulating internal structures */
static 	clshmd_shmid_t *make_new_shmid(int fd, key_in_msg key, 
				       partid_t part_num);
static	clshmd_shared_area_t *make_new_area(key_in_msg key, len_in_msg len);
static 	void 	free_area(clshmd_shared_area_t *area);
static 	clshmd_shared_area_t *find_new_area(key_in_msg key, len_in_msg len);
static 	void 	move_area_to_waiting(clshmd_shared_area_t *new_area,
				     int len);
static 	clshmd_shared_area_t *put_msg_in_area(clshm_msg_t *msg);

/* communication helpers */
static 	int 	inform_drv_of_need_shmid(clshmd_shared_area_t *new_area, 
					 partid_t part_num);
static 	int 	send_area_to_daem(clshmd_shared_area_t *area, 
				  clshm_msg_t *msg);
static 	void 	send_shmid_to_taker(clshm_msg_t *msg, shmid_in_msg shmid, 
				    err_in_msg error, int to_user, int fd);
static 	void	send_pmo_response_to_user(int fd, clshm_msg_t *msg, 
					  err_in_msg err);

/* removing areas */
static 	clshmd_shared_area_t *remove_local_shmid_area(shmid_in_msg shmid);
static 	int 	remove_driver_shmid_area(clshmd_shared_area_t *area);
static 	void 	remove_remote_shmid_area(clshmd_shared_area_t *area);
static 	int 	auto_remove_shmid_area(clshmd_shared_area_t *area);


/* globals */
char	*usage = "clshmd <unit number>  <num_pages>  <timeout in msecs>";
uint	gunit;		/* which clshm unit -- should be 0 */
uint 	gnum_pages;	/* number of pages mapped and shared with driver */
uint	gtimeout;	/* timeout for main select of daemon */	
uint	gshared_pages;	/* number of pages that the partition can share */
uint	gpage_size;	/* size of the page the a partition can share */

char	*gmapped_region;      		/* shared reg b/w clshmd & drvr */

int	gClientSock;			/* socket to listen for users */
int	gdrv_fd;			/* driver file descriptor */
int	gdrv_minor_vers;		/* minor version number of driver */
int	guser_fds[CLSHMD_SOCKET_MAX];	/* sockets to connected users */

partid_t	gown_part_id;			/* my part id */
rpart_info_t	gremote_parts[MAX_PARTITIONS];	/* remote part info */

clshmd_hostname_t	*gpart_hostnames = NULL;/* hostnames not finished */
clshmd_key_t		*gkey_list = NULL;	/* keys not finished */
int			gunique_key_count = 0;	/* current key to return */
clshmd_shmid_t		*gshmid_list = NULL;	/* shmids not finished */
clshmd_path_t		*gnode_path = NULL;	/* nodepaths not finished */
clshmd_autormid_t	*gautormid_list = NULL;	/* autormids not finished */
clshmd_free_offsets_t	*gfree_offsets = NULL;	/* offsets free for pages */
clshmd_shared_area_t	*gshared_area = NULL;	/* areas users are using */

/* dump_daemon_state
 * -----------------
 * dump the daemon state for a user.  Dumps everything we have access to
 */
static void dump_daemon_state(void)
{
    int 			i;
    clshmd_hostname_t		*hn;
    clshmd_key_t		*key;
    clshmd_shmid_t		*shmid;
    clshmd_free_offsets_t	*fo;
    clshmd_shared_area_t	*sa;

    dprintf(0, ("clshmd: DUMPING DAEMON STATE:\n\n"
		"\tGENERAL GLOBALS:\n"
		"\tgunit = %d, gnum_pages = %d, gtimeout = %d, "
		"gshared_pages = %d, gpage_size = %d\n"
		"\tgmapped_region = 0x%x, gown_part_id = %d, "
		"gunique_key_count = %d\n", gunit, gnum_pages, 
		gtimeout, gshared_pages, gpage_size, gmapped_region, 
		gown_part_id, gunique_key_count));

    dprintf(0, ("\n\tGENERAL FILE DESCRIPTORS:\n"
		"\tgdrv_fd = %d, gClientSock = %d\n", gdrv_fd, gClientSock));
    dprintf(0, ("\tguser_fds: "));
    for(i = 0; i < CLSHMD_SOCKET_MAX; i++) {
	if(guser_fds[i] != -1) {
	    dprintf(0, ("%d ", guser_fds[i]));
	}
    }

    dprintf(0, ("\n\tPER PARTITION INFO:\n"));
    for(i = 0; i < MAX_PARTITIONS; i++) {
	if(gremote_parts[i].connect_state & CLSHMD_DRV_CONNECTED) {
	    dprintf(0, ("\tpart = %d, connect_flag = 0x%x, "
			"daem_fd = %d\n", i,
			gremote_parts[i].connect_state, 
			gremote_parts[i].daem_fd));
	}
    }

    dprintf(0, ("\n\n\tPENDING HOSTNAMES:\n"));
    for(hn = gpart_hostnames; hn != NULL; hn = hn->next) {
	dprintf(0, ("\tfd = %d, dst_part = %d\n", hn->fd, hn->dst_part));
    }

    dprintf(0, ("\n\tPENDING KEYS:\n"));
    for(key = gkey_list; key != NULL; key = key->next) {
	dprintf(0, ("\tpid = %d, fd = %d, dst_part = %d\n", key->pid,
		    key->fd, key->dst_part));
    }

    dprintf(0, ("\n\tPENDING SHMIDS:\n"));
    for(shmid = gshmid_list; shmid != NULL; shmid = shmid->next) {
	dprintf(0, ("\tkey = 0x%x, fd = %d, part = %d\n", shmid->key,
		    shmid->fd, shmid->dst_part));
    }

    dprintf(0, ("\n\tFREE OFFSETS:\n"));
    for(fo = gfree_offsets; fo != NULL; fo = fo->next) {
	dprintf(0, ("\tpage_off = %d, num_pages = %d\n", fo->page_off,
		    fo->num_pages));
    }

    dprintf(0, ("\n\tSHARED AREAS:\n"));
    for(sa = gshared_area; sa != NULL; sa = sa->next) {
	dprintf(0, ("\tstate = %s, key = 0x%x, shmid = 0x%x, "
		    "\n\tlen = %lld, pmo_pid = %d, pmo_handle = 0x%x, "
		    "pmo_type = %d, auto_clean = 0x%x\n", 
		    ((sa->state == INCOMPLETE_SHARED) ? "INCOMPLETE_SHARED" : 
		     ((sa->state == WAITING_SHARED) ? "WAITING_SHARED" : 
		      "COMPLETE_SHARED")), 
		    sa->key, sa->shmid, sa->len, sa->pmo_pid, sa->pmo_handle, 
		    sa->pmo_type, sa->auto_cleanup));
	if(sa->state > INCOMPLETE_SHARED) {
	    dprintf(0, ("\taddrs: "));
	    for(i = 0; i < sa->num_addrs; i++) {
		if(i != 0 && (i % 6) == 0) {
		    dprintf(0, ("\n\t\t"));
		}
		dprintf(0, ("0x%llx ", sa->kpaddrs[i]));
	    }
	    dprintf(0, ("\n\tmsg received of %d: ", sa->num_msgs));
	    for(i = 0; i < sa->num_msgs; i++) {
		if(sa->msg_recv[i]) {
		    dprintf(0, ("%d ", i));
		}
	    }
	}
	dprintf(0, ("\n\tshared partitions <partid>:<state>: "));
	for(i = 0; i < MAX_PARTITIONS; i++) {
	    if(sa->parts_state[i] & AREA_ASKED_FOR) {
		dprintf(0, ("%d:%s ", i, (sa->parts_state[i] & AREA_ATTACHED) 
			    ? "attached" : "not attached"));
	    }
	}
	dprintf(0, ("\n\n"));
    }
    dprintf(0, ("DUMP DAEMON STATE DONE\n"));
}

/* xdump
 * -----
 * Parameters
 * 	bytes:		buffer to dump
 *	num_bytes:	number of bytes to dump
 * 
 * Dump the buffer to standard out
 */
/*ARGSUSED*/
static void xdump(char *bytes, int num_bytes)
{
#ifdef	CLSHMD_XDUMP
    int         i;

    if(! bytes || ! num_bytes)  {
        dprintf(0, ("xdump got ptr or num bytes as 0\n"));
    }
    else  {
        for(i = 0; i < num_bytes; i++)   {
            dprintf(0, ("%d: %x\n", i, bytes[i]));
        }
    }
#endif	/* CLSHMD_XDUMP */
    return;
}


/* send_to_usr
 * -----------
 * Parameters:
 * 	fd:	file to send user message to
 *	msg:	message to send to user
 *	return:	-1 on error and 0 on success
 *	
 * Send a header and then the message to the user.
 */
static int send_to_usr(int fd, clshm_msg_t *msg) 
{
    clshm_msg_hdr_t 	hdr;
    ssize_t		bytes_written;
    size_t 		size;

    dprintf(5, ("clshmd: in send_to_usr\n"));

    /* check parameters */
    if(msg == NULL) {  
	dprintf(1, ("clshmd: got NULL msg ptr in send_to_usr\n"));
	return(-1);
    } else if(fd <= 2 || fd == gClientSock) {  
	dprintf(1, ("clshmd: got bad fd in send_to_usr %d\n", fd));
	return(-1);
    } else if(msg->len > MAX_MSG_LEN)  {
        dprintf(1, ("clshmd: send_to_usr: message too long (%d bytes, "
		    "max %d)\n", msg->len, MAX_MSG_LEN));
	return(-1);
    } else if(msg->dst_part != gown_part_id) {
	dprintf(1, ("clshmd: send_to_usr: message not to local part, to %d\n",
		    msg->dst_part));
	return(-1);
    } else if(msg->src_part >= MAX_PARTITIONS || msg->src_part < 0 ||
	      (msg->src_part != gown_part_id && 
	       gremote_parts[msg->src_part].connect_state != 
	                                           CLSHMD_CONNECTED)) {
	dprintf(1, ("clshmd: send_to_usr: message src is not connected or "
		    "off the part limits, part = %d\n", msg->src_part));
	return(-1);
    }

    /* send off a header */
    size = sizeof(msg->type) + sizeof(msg->src_part) + 
	sizeof(msg->dst_part) + sizeof(msg->dummy) + sizeof(msg->len) +
	msg->len;

    hdr.type = DAEM_TO_USR_HDR;
    hdr.len = size;

    bytes_written = write(fd, &hdr, sizeof(clshm_msg_hdr_t));
    if(bytes_written != sizeof(clshm_msg_hdr_t))  {
	dprintf(1, ("clshmd: Couldn't write %d bytes of hdr; only %d "
		    "written\n", sizeof(clshm_msg_hdr_t), 
		    (int) bytes_written));
	return(-1);
    }
    dprintf(3, ("clshmd: send_to_usr: fd %d, header sent for %d bytes\n",
		fd, hdr.len));
    /* send off the actual message */

    xdump((char *) msg, size);

    bytes_written = write(fd, msg, size);
    if(bytes_written != size)  {
	dprintf(1, ("clshmd: Couldn't write %d bytes of msg; only %d "
		    "written\n", (int) size, (int) bytes_written));
	return(-1);
    } 
    dprintf(3, ("clshmd: send_to_usr: fd %d, sent msg: type %c, src %d, "
		"dst %d, len %d\n", fd, msg->type, msg->src_part, 
		msg->dst_part, msg->len));
    return(fd);
}


/* send_to_daem
 * ------------
 * Parameters:
 * 	dest:	partition to send the message to
 *	msg:	msg to send
 *	return:	errno returns
 *
 * Send the given message to the given partition daemon
 */
static int send_to_daem(partid_t dest, clshm_msg_t *msg)
{
    clshm_msg_hdr_t	hdr;
    char		sendbuf[sizeof(clshm_msg_t) + sizeof(clshm_msg_hdr_t)];
    size_t		msg_len;
    ssize_t		bytes_written;
    int			error = 0;
    int			fd;
    rpart_info_t	*part;

    /* this isn't in the critical path, so we aren't too
    *  concerned about the order of case evaluations etc. */

    dprintf(5, ("clshmd: in send_to_daem\n"));

    /* check parameters */
    if(msg == NULL) {  
	dprintf(1, ("clshmd: got NULL msg ptr in send_to_daem\n"));
	error = EINVAL;
    } else if(msg->len > MAX_MSG_LEN)  {
        dprintf(1, ("clshmd send_to_daem: message too long (%d bytes, "
		    "max %d)\n", msg->len, MAX_MSG_LEN));
        error = EFAULT;
    } else if(dest >= MAX_PARTITIONS || dest < 0 || 
	      msg->dst_part == gown_part_id || dest != msg->dst_part ||
	      !(gremote_parts[dest].connect_state & CLSHMD_PIPE_CONNECTED)) {
	dprintf(1, ("clshmd: send_to_daem: dest part bad: dest %d, msg dest "
		    "%d\n", dest, msg->dst_part));
	error = EINVAL;
    } else if(msg->src_part != gown_part_id) {
	dprintf(1, ("clshmd: send_to_daem: message src is our part, is %d\n",
		    msg->src_part));
	error = EINVAL;
    } else  {
	part = &(gremote_parts[dest]);

	fd = part->daem_fd;

	if(fd <= 2) {  /* can't send to std[in/out/err] */
	    dprintf(1, ("clshmd: got bad fd in send_to_daem %d\n", fd));
	    return(EINVAL);
	}

	/* send header */
	msg_len = sizeof(msg->type) + sizeof(msg->src_part)
		 + sizeof(msg->dst_part) + sizeof(msg->dummy)
		 + sizeof(msg->len) + msg->len;

	hdr.type = DAEM_TO_DAEM_HDR;
	hdr.len = msg_len;
	bcopy(&hdr, sendbuf, sizeof(clshm_msg_hdr_t));

	dprintf(3, ("clshmd: send_to_daem: header sent for %d bytes\n",
		    hdr.len));
	/* send actual message */
	bcopy(msg, sendbuf + sizeof(clshm_msg_hdr_t), msg_len);
	
	xdump((char *) msg, msg_len);

	bytes_written = write(fd, sendbuf, sizeof(clshm_msg_hdr_t) + msg_len);
	if(bytes_written != sizeof(clshm_msg_hdr_t) + msg_len)  {
	    dprintf(1, ("clshmd: Couldn't write %d bytes of hdr+msg; only %d "
			"written, errno = %d\n", 
			sizeof(clshm_msg_hdr_t) + sizeof(clshm_msg_t), 
			(int) bytes_written, errno));
	    error = errno;
   	}
	if(!error) {
	    dprintf(3, ("clshmd: send_to_daem: sent msg: type %c, src %d, "
			"dst %d, len %d\n", msg->type, 
			msg->src_part, msg->dst_part, msg->len));
	}
    }

    return(error);
}


/* send_to_drv
 * -----------
 * Parameters:
 *	msg:	msg to send to driver
 *	return:	-1 for error and 0 for success
 *
 * Wrapper for ioctl call into driver with a daemon message.  Done so
 * we can be symetical and also print out debug info about messages
 */
static int send_to_drv(clshm_msg_t *msg)
{
    int error = 0;

    dprintf(3, ("clshmd: send_to_drv: sent msg: type %c, src %d, "
		"dst %d, len %d\n", msg->type, msg->src_part, 
		msg->dst_part, msg->len));

    if(msg->dst_part != gown_part_id) {
	dprintf(1, ("clshmd: send_to_drv: message not to local part, to %d\n",
		    msg->dst_part));
	error = -1;
    } else if(msg->src_part >= MAX_PARTITIONS || msg->src_part < 0 ||
	      !(gremote_parts[msg->src_part].connect_state & 
		CLSHMD_DRV_CONNECTED)) {
	dprintf(1, ("clshmd: send_to_drv: message src is not connected or "
		    "off the part limits, part = %d\n", msg->src_part));
	error = -1;
    } else if(ioctl(gdrv_fd, CL_SHM_DAEM_MSG, msg) < 0) {
	dprintf(1, ("clshmd: driver sent back error for RMID %d\n", 
		    errno));
	error = -1;
    }
    return(error);
}


/* recv_from_usr
 * -------------
 * Parameters:
 * 	fd:	file descriptor to read the message from
 *	msg:	the msg to fill the incoming message into 
 *	return:	-1 on failure and 0 on success
 *
 * We think there is am message coming in so read it into the msg.
 */
static int recv_from_usr(int fd, clshm_msg_t *msg)
{
    clshm_msg_hdr_t	hdr;
    ssize_t		bytes_read;

    /* check parameters */
    if(msg == NULL) {
	dprintf(1, ("clshmd: got NULL msg ptr in recv_from_usr\n"));
	return(-1);
    }
    if(fd <= 2) {  /* can't recv from std[in/out/err] */
	dprintf(1, ("clshmd: got bad fd in recv_from_usr %d\n", fd));
	return(-1);
    }

    /* read header */
    bytes_read = read(fd, &hdr, sizeof(clshm_msg_hdr_t));
    if(bytes_read == 0) {
	dprintf(5, ("clshmd: usr closed the socket so close it\n"));
	return(CLSHMD_SOCKET_CLOSED);
    } else if(bytes_read < 0 && errno == EWOULDBLOCK) {
	dprintf(5, ("clshmd: usr was going to block!!!\n"));
	return(-1);
    }
    if(bytes_read != sizeof(clshm_msg_hdr_t))  { 
	dprintf(1, ("clshmd: recv_from_usr: Couldn't read %d bytes of hdr; "
		    "only %d read\n", sizeof(clshm_msg_hdr_t), 
		    (int) bytes_read));
	return(-1);
    }
    if(hdr.type != USR_TO_DAEM_HDR)  {
	dprintf(1, ("clshmd: recv_from_usr: expected to get a user hdr type "
		    "msg; got %c\n", hdr.type));
	return(-1);
    }

    if(hdr.len < (sizeof(clshm_msg_t) - MAX_MSG_LEN) || 
       hdr.len > sizeof(clshm_msg_t)) {
	dprintf(1, ("clshmd: recv_from_usr: got a hdr len that is out of "
		    "range: %d\n", hdr.len));
	return(-1);
    }

    dprintf(5, ("clshmd: recv_from_usr: fd %d, Received a header for %d "
		"bytes\n", fd, hdr.len));

    /* we shouldn't need to block since the user should send header and
     * msg in the same write */
    bytes_read = read(fd, msg, hdr.len);

    if(bytes_read != hdr.len)  {
	dprintf(1, ("clshmd: recv_from_usr: expected %d bytes from user; "
		    "got %d\n", hdr.len, bytes_read));
	return(-1);
    }
    if(msg->len > MAX_MSG_LEN)  {
	dprintf(1, ("clshmd: recv_from_usr: message too long "
		    "(%d bytes, max %d)\n", msg->len, MAX_MSG_LEN));
	return(-1);
    }
    if(msg->dst_part >= MAX_PARTITIONS || msg->dst_part < 0 ||
       msg->src_part != gown_part_id) {
	dprintf(1, ("clshmd: recv_from_usr: bad dst %d or src %d part\n",
		    msg->dst_part, msg->src_part));
	return(-1);
    }
    dprintf(3, ("clshmd: recv_from_usr: fd %d, msg type %c, src %d, dst %d, "
		"len %d\n", fd, msg->type, msg->src_part, msg->dst_part, 
		msg->len));

    xdump((char *) msg, hdr.len);

    return (fd);
}


/* recv_from_daem
 * --------------
 * Parameters:
 *	fd:	daem file desc that message is coming in on
 *	msg:	msg structure to fill with the message
 *	return:	errno returns
 *
 * Receive a message from one of the other daemons
 */
static  int
recv_from_daem(int fd, clshm_msg_t *msg)
{
    clshm_msg_hdr_t	hdr;
    ssize_t		bytes_read;
    
    /* make sure parameters are sane */
    if(msg == NULL) {
	dprintf(1, ("clshmd: got NULL msg ptr in recv_from_daem\n"));
	return(EINVAL);
    }
    
    if(fd <= 2) {  /* can't recv from std[in/out/err] */
	dprintf(1, ("clshmd: got bad fd in recv_from_daem %d\n", fd));
	return(EINVAL);
    }

    /* get the hdr first to find expected length */
    bytes_read = read(fd, &hdr, sizeof(clshm_msg_hdr_t));
    if(bytes_read != sizeof(clshm_msg_hdr_t))  { 
	dprintf(1, ("clshmd: Couldn't read %d bytes of hdr; only %d "
		    "read\n", sizeof(clshm_msg_hdr_t), (int) bytes_read));
	return(errno);
    }

    if(hdr.type != DAEM_TO_DAEM_HDR)  {
	dprintf(1, ("clshmd: expected to get a hdr from daem type "
		    "msg; got %c\n", hdr.type));
	return(EINVAL);
    }

    if(hdr.len < (sizeof(clshm_msg_t) - MAX_MSG_LEN) || 
       hdr.len > sizeof(clshm_msg_t)) {
	dprintf(1, ("clshmd: got a hdr len that is out of range: %d\n",
		    hdr.len));
	return(EINVAL);
    }
    dprintf(5, ("clshmd: recv_from_daem: Received a header for "
		"%d bytes\n", hdr.len));

    /* don't assume that fields of msg are contiguous */
    bytes_read = read(fd, msg, hdr.len);
    
    if(bytes_read != hdr.len)  {
	dprintf(1, ("clshmd: expected %d bytes from daemon; "
		    "got %d\n",hdr.len, bytes_read));
	return(errno);
    }
    
    if(msg->len > MAX_MSG_LEN)  {
	dprintf(1, ("clshmd recv_from_daem: message too long "
		    "(%d bytes, max %d)\n", msg->len, 
		    MAX_MSG_LEN));
	return(EFAULT);
    }
    if(msg->dst_part != gown_part_id || msg->src_part >= MAX_PARTITIONS || 
       msg->src_part < 0) {
	dprintf(1, ("clshmd: recv_from_usr: bad dst %d or src %d part\n",
		    msg->dst_part, msg->src_part));
	return(-1);
    }
    dprintf(3, ("clshmd: recv_from_daem: msg type %c, src %d, dst %d, "
		"len %d\n", msg->type, msg->src_part, msg->dst_part, 
		msg->len));
    
    xdump((char *) msg, hdr.len);

    return 0;
}


/* recv_from_drv
 * -------------
 * Parameters:
 *	msg:	msg to put the new message into 
 *	return:	errno returns
 *
 * There is a message from the driver.  Go and read it out of the shared
 * memory segment.
 */
static int recv_from_drv(clshm_msg_t *msg)
{
    volatile __uint32_t *recv_head, *recv_tail, *max_recv, *dummy;
    clshm_msg_t         *msg_area, *curmsg;
    char                *cur_ptr;
    int                 error = 0;

    if(msg == NULL) {
	dprintf(1, ("clshmd: got NULL msg ptr in recv_from_drv\n"));
	error = EINVAL;
    }
    else  {
        recv_head = (__uint32_t *) gmapped_region;
        recv_tail = recv_head + 1;
        max_recv = recv_tail + 1;
        dummy = max_recv + 1;

        dprintf(15, ("clshmd: recv_from_drv: head %x, tail %x, max %x\n",
	    recv_head, recv_tail, max_recv));

        cur_ptr = (char *) dummy;
        msg_area = (clshm_msg_t *) (cur_ptr + sizeof(max_recv));
	dprintf(10, ("clshmd: Two pointers are: %x and %x\n",
	    &(msg_area[*recv_tail]), msg_area + *recv_tail));
        curmsg = &(msg_area[*recv_tail]);
	
	dprintf(10, ("clshmd: receive_from_drv: head %d, tail %d before "
		     "recv msg\n", *recv_head, *recv_tail));
	
    	msg->type = curmsg->type;
    	msg->src_part = curmsg->src_part;
    	msg->dst_part = curmsg->dst_part;
    	msg->len = curmsg->len;

        if(msg->len > MAX_MSG_LEN)  {
            dprintf(1, ("clshmd recv_from_drv: message too long (%d bytes, "
			"max %d)\n", msg->len, MAX_MSG_LEN));
            error = EFAULT;
        } else if(msg->src_part != gown_part_id || 
		  msg->dst_part >= MAX_PARTITIONS || msg->dst_part < 0) {
	    dprintf(1, ("clshmd: recv_from_drv: bad dst %d, or src %d part\n",
			msg->dst_part, msg->src_part));
	    error = EFAULT;
	} else {
    	    bcopy(curmsg->body, msg->body, msg->len);
	}
    	*recv_tail = (*recv_tail + 1) % *max_recv;
    }

    if(!error) {
	dprintf(3, ("clshmd: recv_from_drv: msg type %c, src %d, "
		    "dst %d, len %d\n", msg->type, msg->src_part, 
		    msg->dst_part, msg->len));
    }
    return  error;
}


/* process_usr_msg
 * ---------------
 * Parameters:
 *	fd:	File descriptor that this message came in on.
 *	msg:	The message that came to us
 *	return:	-1 for error, 0 else
 *
 * This is a message that we just received from a user, for all the 
 * different messages we can receive, parse through it and either save
 * the file descriptor to send back to the sender when we have an answer
 * or send back a message right now
 */
static int process_usr_msg(int fd, clshm_msg_t *msg)
{
    clshm_msg_t		local_msg;
    partid_t		dest;
    int			error = 0;
    
    if(msg == NULL) {
	dprintf(1, ("clshmd: process_usr_msg got NULL msg\n"));
	return(-1);
    }
    if(fd <= 2 || fd == gClientSock) {
	dprintf(1, ("clshmd: process_usr_msg got bad fd %d\n", fd));
	return(-1);
    }

    dest = msg->dst_part;
    if(dest >= MAX_PARTITIONS || dest < 0 || msg->src_part != gown_part_id ||
       (dest != gown_part_id && 
	gremote_parts[dest].connect_state != CLSHMD_CONNECTED)) {
	dprintf(1, ("clshmd: process_usr_msg trying to send to not "
		    "fully connect partition: %d, or src %d\n", dest, 
		    msg->src_part));
	/* trying to get info on a downed partition -- tell them so */
	msg->type = DAEM_TO_USR_PART_DEAD;
	msg->len = 0;
	if(send_to_usr(fd, msg) < 0) {
	    dprintf(1, ("clshmd: can't send back to user fd %d\n", fd));
	    return(-1);
	}
	return(0); /* we dealt with it */
    }	

    local_msg.src_part = msg->src_part;
    local_msg.dst_part = msg->dst_part;
    local_msg.len = msg->len;
    bcopy(msg->body, local_msg.body, msg->len);
    switch (msg->type)  {
	
        case USR_TO_DAEM_NEW_USER: {
	    vers_in_msg vers_major;
	    vers_in_msg vers_minor;
	    pgsz_in_msg pgsz;
	    int len;
	    
	    dprintf(5, ("clshmd: USR_TO_DAEM_NEW_USER\n"));

	    if(local_msg.src_part != gown_part_id || 
	       local_msg.dst_part != gown_part_id) {
		dprintf(5, ("clshmd: NEW_USER must be local src and dst\n"));
		local_msg.dst_part = gown_part_id;
		local_msg.src_part = gown_part_id;
	    }

	    /* basically ignor what version they are, let them decide
	     * if they want to play with us. */
	    bcopy(local_msg.body, &vers_major, sizeof(vers_in_msg));
	    len = sizeof(vers_in_msg);
	    bcopy(local_msg.body + len, &vers_minor, sizeof(vers_in_msg));
	    len += sizeof(vers_in_msg);
	    bcopy(local_msg.body + len, &pgsz, sizeof(pgsz_in_msg));
	    
	    /* someday might need to do something with this knowledge
	     * depending on whether or not we have different versions
	     * that the daemon needs to deal with for the user */

	    /* tell the user what is going on here */
	    pgsz = gpage_size;
	    vers_major = CLSHM_DAEM_MAJOR_VERS;
	    vers_minor = CLSHM_DAEM_MINOR_VERS;
	    local_msg.len = (sizeof(vers_in_msg) * 2) + sizeof(pgsz_in_msg);
	    local_msg.type = DAEM_TO_USR_NEW_USER;
	    bcopy(&vers_major, local_msg.body, sizeof(vers_in_msg));
	    len = sizeof(vers_in_msg);
	    bcopy(&vers_minor, local_msg.body + len, sizeof(vers_in_msg));
	    len += sizeof(vers_in_msg);
	    bcopy(&pgsz, local_msg.body + len, sizeof(pgsz_in_msg));
	    if(send_to_usr(fd, &local_msg) < 0) {
		dprintf(1, ("clshmd: error in sending to user fd %d\n", fd));
		error = -1;
	    }
	    break;
	}

        case USR_TO_DAEM_NEED_HOSTNAME: {

	    dprintf(5, ("clshmd: USR_TO_DAEM_NEED_HOSTNAME wants hostname for "
			"part %d\n", local_msg.dst_part));

	    /* if we are the destination, then just return the hostname */
	    if(dest == gown_part_id) {
		if(set_local_hostname(local_msg.body) < 0) {
		    dprintf(1, ("clshmd: error in getting hostname\n"));
		    local_msg.len = 0;
		} else {
		    local_msg.len = strlen(local_msg.body);
		}
		local_msg.type = DAEM_TO_USR_TAKE_HOSTNAME;
		if(send_to_usr(fd, &local_msg) < 0) {
		    dprintf(1, ("clshmd: error in sending to user fd %d\n", 
				fd));
		    error = -1;
		    break;
		}
	    } else {
		/* need to save the user to send back to and ask another
		 * partition to give us its hostname */
		clshmd_hostname_t *new_host, *host;

		new_host = (clshmd_hostname_t *) 
		    check_malloc(sizeof(clshmd_hostname_t));
		new_host->fd = fd;
		new_host->dst_part = dest;
		local_msg.type = DAEM_TO_DAEM_NEED_HOSTNAME;
		local_msg.len = 0;

		for(host = gpart_hostnames; host != NULL; host = host->next) {
		    if(host->dst_part == dest) {
			break;
		    }
		}

		/* if no one else has asked for the hostname, then we 
		 * really have to send off the message */
		if(!host && send_to_daem(dest, &local_msg))  {
		    dprintf(1, ("clshmd: couldn't send NEED_HOSTNAME message "
				"to part %d\n", dest));
		    check_free(new_host);
		    local_msg.type = DAEM_TO_USR_TAKE_HOSTNAME;
		    local_msg.len = 0;
		    local_msg.src_part = local_msg.dst_part;
		    local_msg.dst_part = gown_part_id;
		    if(send_to_usr(fd, &local_msg) < 0) {
			dprintf(1, ("clshmd: error in sending to user fd "
				    "%d\n", fd));
			error = -1;
		    }		    
		    break;
		}
		new_host->next = gpart_hostnames;
		gpart_hostnames = new_host;
	    }
	    break;
	}

        case USR_TO_DAEM_NEED_KEY: {
	    clshmd_key_t *new_key; 
	    clshmd_shared_area_t *new_area;
	    key_in_msg key_err;
	    err_in_msg error_num;
	    char *key_ptr;
	    char *err_ptr;
	    int err_msg_len;

	    dprintf(5, ("clshmd: USR_TO_DAEM_NEED_KEY want key from part "
			"%d\n", local_msg.dst_part));

	    /* set up error key to return if necessary */
	    key_err = CLSHMD_ABSENT_KEY;
	    key_ptr = local_msg.body + sizeof(pid_in_msg);
	    err_ptr = key_ptr + sizeof(key_in_msg);
	    err_msg_len = sizeof(pid_in_msg) + sizeof(key_in_msg) + 
		sizeof(err_in_msg);

	    if(local_msg.len != (sizeof(pid_in_msg))) {
		dprintf(1, ("clshmd: bad parameters in body of NEED_KEY "
			    "message\n"));
		error = -1;
		/* user library screwed up at this point, no answer */
		break;
	    }

	    /* want a key from the local partition, so just make it
	     * and send it back */
	    if(dest == gown_part_id) {
		new_area = make_new_area(CLSHMD_ABSENT_KEY, 0);

		local_msg.type = DAEM_TO_USR_TAKE_KEY;	
		error_num = 0;
		bcopy(&(new_area->key), key_ptr, sizeof(key_in_msg));
		bcopy(&error_num, err_ptr, sizeof(err_in_msg));
		local_msg.len = err_msg_len;
		if(send_to_usr(fd, &local_msg) < 0) {
		    dprintf(1, ("clshmd: couldn't sent TAKE_KEY to user fd "
				"%d\n", fd));
		    error = -1;
		    break;
		}
	    } else {
		/* need it from a not so local partition, so save the
		 * user that we need to send back to and send the other
		 * daemon the request. */
		new_key = (clshmd_key_t *) check_malloc(sizeof(clshmd_key_t));
		bcopy(local_msg.body, &(new_key->pid), sizeof(pid_in_msg));
		new_key->fd = fd;
		new_key->dst_part = dest;
		local_msg.type = DAEM_TO_DAEM_NEED_KEY;
		if(send_to_daem(dest, &local_msg))  {
		    dprintf(1, ("clshmd: couldn't send NEED_KEY message"
				" to part %d\n", dest));
		    /* we had an error, so just say we can't get the key */
		    check_free(new_key);
		    local_msg.type = DAEM_TO_USR_TAKE_KEY;
		    local_msg.len = err_msg_len;
		    error_num = EIO;
		    bcopy(&key_err, key_ptr, sizeof(key_in_msg));
		    bcopy(&error_num, err_ptr, sizeof(err_in_msg));
		    local_msg.src_part = local_msg.dst_part;
		    local_msg.dst_part = gown_part_id;
		    if(send_to_usr(fd, &local_msg) < 0) {
			dprintf(1, ("clshmd: couldn't sent TAKE_KEY to "
				    "user fd %d\n", fd));
			error = -1;
		    }
		    break;
		}
		new_key->next = gkey_list;
		gkey_list = new_key;
	    }	
	    break;
	}

        case USR_TO_DAEM_TAKE_PMO: {
	    key_in_msg key;
	    pid_in_msg pid;
	    pmoh_in_msg pmo_handle;
	    pmot_in_msg pmo_type;
	    char *cpy_ptr;
	    clshmd_shared_area_t *area;

	    dprintf(5, ("clshmd: USR_TO_DAEM_TAKE_PMO\n"));

	    if(local_msg.len != sizeof(key_in_msg) + sizeof(pid_in_msg) +
	                        sizeof(pmoh_in_msg) + sizeof(pmot_in_msg)) {
		dprintf(1, ("clshmd: TAKE_PMO message wrong length %d\n", 
			    local_msg.len));
		error = -1;
		break;
	    }

	    cpy_ptr = local_msg.body;
	    bcopy(cpy_ptr, &key, sizeof(key_in_msg));
	    cpy_ptr += sizeof(key_in_msg);
	    bcopy(cpy_ptr, &pid, sizeof(pid_in_msg));
	    cpy_ptr += sizeof(pid_in_msg);
	    bcopy(cpy_ptr, &pmo_handle, sizeof(pmoh_in_msg));
	    cpy_ptr += sizeof(pmoh_in_msg);
	    bcopy(cpy_ptr, &pmo_type, sizeof(pmot_in_msg));

	    /* parse out the message that we are sent */

	    if(KEY_TO_PARTITION(key) != gown_part_id) {
		dprintf(1, ("clshmd: key to TAKE_PMO is not ours 0x%x\n",
			    key));
		send_pmo_response_to_user(fd, &local_msg, EINVAL);
		break;
	    }

	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->key == key) {
		    break;
		}
	    }
	    if(!area || area->state != INCOMPLETE_SHARED ||
	       area->pmo_pid != CLSHMD_ABSENT_PMO_PID ||
	       area->pmo_handle != CLSHMD_ABSENT_PMO_HANDLE ||
	       area->pmo_type != CLSHMD_ABSENT_PMO_TYPE) {
		dprintf(1, ("clshmd: area already got for TAKE_PMO key "
			    "0x%x\n", key));
		send_pmo_response_to_user(fd, &local_msg, EINVAL);
		break;
	    }

	    area->pmo_pid = pid;
	    area->pmo_handle = pmo_handle;
	    area->pmo_type = pmo_type;
	    
	    send_pmo_response_to_user(fd, &local_msg, 0);
	    
	    break;
	}

        case USR_TO_DAEM_NEED_SHMID: {
	    clshmd_shared_area_t *new_area;
	    len_in_msg len, cpy_len;
	    key_in_msg key;
	    char *cpy_ptr;
	    int offset;
	    
	    dprintf(5, ("clshmd: USR_TO_DAEM_NEED_SHMID want shmid from "
			"part %d\n", local_msg.dst_part));

	    if(local_msg.len != (sizeof(key_in_msg) + 
				 sizeof(len_in_msg))) {
		dprintf(1, ("clshmd: bad parameters in body of NEED_SHMID "
			    "message len = %d\n", local_msg.len));
		/* user library screwed up so no return -1 to kill off 
		 * socket to user */
		error = -1;
		break;
	    }

	    cpy_len = 0;
	    cpy_ptr = local_msg.body;
	    bcopy(cpy_ptr, &(key), sizeof(key_in_msg));
	    cpy_ptr += sizeof(key_in_msg);
	    cpy_len += sizeof(key_in_msg);

	    bcopy(cpy_ptr, &(len), sizeof(len_in_msg));
	    cpy_ptr += sizeof(len_in_msg);
	    cpy_len += sizeof(len_in_msg);

	    /* any errors in what was asked for and just send back a
	     * error message to user */
	    if(len <= 0 || KEY_TO_PARTITION(key) != local_msg.dst_part ||
	       key == CLSHMD_ABSENT_KEY) {
		dprintf(1, ("clshmd: NEED_SHMID len too small: %lld or"
			    " key doesn't match message\n", len));
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID,
				    EINVAL, 1, fd);
		break;
	    }
	    
	    new_area = find_new_area(key, len);

	    if(!new_area || (new_area->state != INCOMPLETE_SHARED && 
			     new_area->len != len)) {
		dprintf(1, ("clshmd: key doesn't match prior len or no "
			    "such key 0x%x\n", key));
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID,
				    EINVAL, 1, fd);
		break;
	    }

	    /* make the shmid and get free range */
	    if(new_area->state == INCOMPLETE_SHARED) {

		move_area_to_waiting(new_area, len);

		if(local_msg.dst_part == gown_part_id) {
		    offset = get_free_page_range(new_area->num_addrs);
		    if(offset < 0) {
			fprintf(stderr, "clshmd: no large enough contigous "
				"space in daemon\n");
			send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID,
					    ENOMEM, 1, fd);
			break;
		    }
		    new_area->shmid = MAKE_SHMID(gown_part_id, offset);
		}

		/* ask another daemon for the shmid */
		if(local_msg.dst_part != gown_part_id) {
		    local_msg.type = DAEM_TO_DAEM_NEED_SHMID;
		    if(send_to_daem(local_msg.dst_part, &local_msg)) {
			dprintf(1, ("clshmd: couldn't send to daemon for "
				    "part %d\n", local_msg.dst_part));
			break;
		    }
		}
	    }

	    /* tell the driver we need this partition if it doesn't
	     * know yet */
	    if(inform_drv_of_need_shmid(new_area, 
					local_msg.dst_part) < 0) {
		dprintf(1, ("clshmd: couldn't inform driver of new shmid "
			    "0x%x\n", new_area->shmid));
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID,
				    errno, 1, fd);
		break;
	    }
	    /* add the dest_part because we request from them */
	    new_area->parts_state[local_msg.dst_part] |= AREA_ASKED_FOR;

	    /* if we already have this shmid, then just send it back */
	    if(new_area->state == COMPLETE_SHARED) {
		/* add the dest_part because we request from them */
		send_shmid_to_taker(&local_msg, new_area->shmid,
				    0, 1, fd);
		break;
	    }

	    /* save the shmid so we can send back a response when we get it */
	    (void) make_new_shmid(fd, key, local_msg.dst_part);

	    break;
	}

        case USR_TO_DAEM_NEED_PATH: {
	    shmid_in_msg shmid;
	    clshmd_path_t *path;
	    clshmd_shared_area_t *area;
	    char *cpy_ptr;

	    dprintf(5, ("clshmd: USR_TO_DAEM_NEED_PATH\n"));

	    /* check parameters */
	    bcopy(local_msg.body, &shmid, sizeof(shmid_in_msg));
	    if(local_msg.len != sizeof(shmid_in_msg) ||
	       SHMID_TO_PARTITION(shmid) != gown_part_id ||
	       local_msg.dst_part != gown_part_id) {
		dprintf(0, ("clshmd: usr sent bad len or shmid for "
			    "NEED_PATH, len %d, shmid 0x%x\n",
			    local_msg.len, shmid));
		error = -1;
		break;
	    }
	    
	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->shmid == shmid) {
		    break;
		}
	    }

	    local_msg.type = DAEM_TO_USR_TAKE_PATH;
	    if(!area) {
		dprintf(5, ("clshmd: can't find shmid for NEED_PATH "
			    "0x%x\n", shmid));
		shmid = CLSHMD_ABSENT_SHMID;
		bcopy(&shmid, local_msg.body, sizeof(shmid_in_msg));
		if(send_to_usr(fd, &local_msg) < 0) {
		    dprintf(5, ("clshmd: can't send TAKE_PATH to user fd "
				"%d\n", fd));
		    error = -1;
		}
		break;
	    }

	    /* we already have the path */
	    if(area->nodepath) {
		cpy_ptr = local_msg.body + sizeof(shmid_in_msg);
		strcpy(cpy_ptr, area->nodepath);
		local_msg.len += strlen(area->nodepath);
		if(send_to_usr(fd, &local_msg) < 0) {
		    dprintf(5, ("clshmd: can't send TAKE_PATH to user fd "
				"%d\n", fd));
		    error = -1;
		}
	    } else {
		/* need to ask for path from driver */
		local_msg.type = DAEM_TO_DRV_NEED_PATH;
		if(send_to_drv(&local_msg)) {
		    dprintf(5, ("clshmd: can't sent NEED_PATH to driver\n"));
		    error = -1;
		    break;
		}
		
		/* save for the return */
		path = (clshmd_path_t *) check_malloc(sizeof(clshmd_path_t));
		path->fd = fd;
		path->shmid = shmid;
		path->next = gnode_path;
		gnode_path = path;
	    }

	    break;
	}

        case USR_TO_DAEM_RMID: {
	    clshmd_shared_area_t *area;
	    shmid_in_msg shmid;
	    err_in_msg err = 0;

	    dprintf(1, ("clshmd: USR_TO_DAEM_RMID\n"));

	    if(local_msg.len != sizeof(shmid_in_msg)) {
		/* usr is screwed up for sending this */
		dprintf(0, ("clshmd: usr sent bad len for RMID %d\n", 
			    local_msg.len));
		error = -1;
		break;
	    }
	    /* remove the shmid on local/remote daemons and drivers */
	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    area = remove_local_shmid_area(shmid);
	    if(!area) {
		dprintf(5, ("clshmd: shmid %x RMID and not found\n",
			    shmid));
		err = EINVAL;
	    } else {
		if(remove_driver_shmid_area(area) < 0) {
		    dprintf(5, ("clshmd: failed in ioctl to RMID\n"));
		    err = EFAULT;
		} 
		remove_remote_shmid_area(area);
		free_area(area);
	    }
	    bcopy(&err, local_msg.body, sizeof(err_in_msg));
	    local_msg.len = sizeof(err_in_msg);
	    local_msg.type = DAEM_TO_USR_RMID;
	    local_msg.src_part = local_msg.dst_part;
	    local_msg.dst_part = gown_part_id;
	    if(send_to_usr(fd, &local_msg) < 0) {
		dprintf(0, ("clshmd: can't send to usr RMID result fd %d\n",
			    fd));
		error = -1;
	    }
	    break;
	}

        case USR_TO_DAEM_AUTORMID: {
	    clshmd_shared_area_t *area;
	    clshmd_autormid_t *autormid;
	    shmid_in_msg shmid;
	    err_in_msg err = 0;
	    partid_t part_num;
	    rpart_info_t *part;

	    dprintf(5, ("clshmd: USR_TO_DAEM_AUTORMID\n"));

	    /* check parameter */
	    if(local_msg.len != sizeof(shmid_in_msg)) {
		/* usr is screwed up for sending this */
		dprintf(0, ("clshmd: usr sent bad len for AUTORMID %d\n",
			    local_msg.len));
		error = -1;
		break;
	    }

	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    part_num = SHMID_TO_PARTITION(shmid);
	   
	    dprintf(5, ("clshmd: AUTORMID shmid %x\n", shmid));

	    /* if the part_num is ok, then take the part address -- if
	     * this is not so it will fall into the following if and 
	     * return an error to the user */
	    if(part_num < MAX_PARTITIONS && part_num >= 0) {
		part = &(gremote_parts[part_num]);
	    }
	    if(part_num != gown_part_id) {

		/* check the shmid for validity */
		if(part_num >= MAX_PARTITIONS || part_num < 0 ||
		   part->connect_state != CLSHMD_CONNECTED) {
		    dprintf(5, ("clshmd: partition of shmid trying to "
				"AUTORMID is not valid part %d\n", part_num));
		    err = EINVAL;
		    bcopy(&err, local_msg.body, sizeof(err_in_msg));
		    local_msg.type = DAEM_TO_USR_AUTORMID;
		    local_msg.len = sizeof(err_in_msg);
		    local_msg.src_part = local_msg.dst_part;
		    local_msg.dst_part = gown_part_id;
		    if(send_to_usr(fd, &local_msg) < 0) {
			dprintf(0, ("clshmd: can't send to AUTORMID to "
				    "user fd %d\n", fd));
			error = -1;

		    }
		    break;
		}
		/* we aren't the partition owner of this shmid, so
		 * tell the other partition about it */
		dprintf(7, ("clshmd: AUTORMID to daem err = %d\n", err));

		bcopy(&err, local_msg.body + sizeof(shmid_in_msg),
		      sizeof(err_in_msg));
		local_msg.len = sizeof(shmid_in_msg) + sizeof(err_in_msg);
		local_msg.type = DAEM_TO_DAEM_AUTORMID;
		local_msg.src_part = gown_part_id;
		local_msg.dst_part = part_num;
		if(send_to_daem(local_msg.dst_part, &local_msg)) {
		    dprintf(0, ("clshmd: can't send AUTORMID to daem part "
				"%d\n", local_msg.dst_part));
		    break;
		}
	    }

	    /* try to find the area locally */
	    for(area = gshared_area; area != NULL; area = area->next) {
		if(shmid == area->shmid) {
		    break;
		}
	    }

	    /* found locally */
	    if(area || part_num == gown_part_id) {

		if(area) {
		    area->auto_cleanup |= AUTO_CLEANUP_ON;

		    /* check to see if we should clean up now */
		    if(part_num == gown_part_id) {
			auto_remove_shmid_area(area);
		    }
		} else {
		    dprintf(1, ("clshmd: can't find area locally for shmid "
				"0x%x AUTORMID\n", shmid));
		    err = EINVAL;
		}

		/* tell user that everything is getting set up */

		dprintf(7, ("clshmd: AUTORMID to usr err = %d\n", err));

		bcopy(&err, local_msg.body, sizeof(err_in_msg));
		local_msg.type = DAEM_TO_USR_AUTORMID;
		local_msg.len = sizeof(err_in_msg);
		local_msg.src_part = local_msg.dst_part;
		local_msg.dst_part = gown_part_id;
		if(send_to_usr(fd, &local_msg) < 0) {
		    dprintf(0, ("clshmd: can't send to AUTORMID to user fd "
				"%d\n", fd));
		    error = -1;
		}
		break;
	    }

	    /* not found locally so wait for the other daemon's reply */
	    
	    dprintf(5, ("clshmd: AUTORMID wait for response\n"));

	    autormid = (clshmd_autormid_t *) 
		check_malloc(sizeof(clshmd_autormid_t));
	    autormid->fd = fd;
	    autormid->shmid = shmid;
	    autormid->next = gautormid_list;
	    gautormid_list = autormid;
	    break;
	}

        case USR_TO_DAEM_DUMP_DAEM_STATE: {
	    dprintf(5, ("clshmd: USR_TO_DAEM_DUMP_DAEM_STATE\n"));
	    dump_daemon_state();
	    break;
	}

        case USR_TO_DAEM_DUMP_DRV_STATE: {
	    dprintf(5, ("clshmd: USR_TO_DAEM_DUMP_DRV_STATE\n"));
	    local_msg.type = DAEM_TO_DRV_DUMP_DRV_STATE;
	    send_to_drv(&local_msg);
	    break;
	}

        default: {
	    dprintf(1, ("clshmd: got message from usrland unknown\n"));
	    error = -1;
	    break;
	}
    }

    dprintf(5, ("clshmd: done processing usr message\n"));
    return(error);
}


/* process_driver_msg
 * ------------------
 * Parameters:
 *	msg:	The message that came to us
 *	return:	1 for to exit, 0 else
 *
 * This is a message that we just received from the driver, for all the 
 * different messages we can receive, parse through it and either send
 * an answer from this to a user or to a daemon depending on why this
 * message came to us (normally in response to a request we sent it)
 */
static int process_driver_msg(clshm_msg_t *msg)
{
    partid_t		dest;
    clshm_msg_t		local_msg;
    int 		exit_status = 0;

    if(msg == NULL || msg->dst_part >= MAX_PARTITIONS || 
       msg->dst_part < 0 || msg->src_part != gown_part_id) {
	dprintf(1, ("clshmd: got NULL or bad msg ptr or bad part numbers in "
		    "process_driver_msg\n"));
	return(1);
    }

    dest = msg->dst_part;

    /* make a copy of the msg, since need to change fields */
    local_msg.src_part = msg->src_part;
    local_msg.dst_part = dest;
    local_msg.len = msg->len;
    bcopy(msg->body, local_msg.body, local_msg.len);
	
    switch (msg->type)  {
        default:  {
	    dprintf(1, ("clshmd: received message of unknown type '%c' "
			"(%d) from driver\n", msg->type, (int) msg->type));
	    break;
	}

	/* the driver is telling us that a partition has died. */
        case DRV_TO_DAEM_PART_DEAD:  {
	    partid_t part_num;

	    part_num = local_msg.dst_part;
	    dprintf(5, ("clshmd: DRV_TO_DAEM_PART_DEAD part %d\n", part_num));

	    if(gremote_parts[part_num].connect_state == CLSHMD_UNCONNECTED) {
		dprintf(1, ("clshmd: driver killing already dead part %d\n",
			    part_num));
		break;
	    }
	    local_msg.type = DAEM_TO_DAEM_PART_DEAD;
	    if(send_to_daem(dest, &local_msg))  {
		dprintf(1, ("clshmd: couldn't send PART_DEAD message to "
			    "part %d\n", dest));
	    }
	    clean_up_dead_part(part_num);
	    gremote_parts[part_num].connect_state = CLSHMD_UNCONNECTED;
	    break;
	}

	/* driver is telling us to just die, so try to clean up as best
	 * we can before we die */
        case DRV_TO_DAEM_CLSHM_DIE_NOW:  {
	    int i;

	    dprintf(1, ("clshmd: DRV_TO_DAEM_CLSHM_DIE_NOW\n"));
	    /* local_msg.src_part = gown_part_id; should already be true. */
	    local_msg.len = 0;
	    for(i = 0; i < MAX_PARTITIONS; i++) {
		if(gremote_parts[i].connect_state != CLSHMD_UNCONNECTED &&
		   gremote_parts[i].daem_fd != -1) {
		    dest = i;
		    local_msg.type = DAEM_TO_DAEM_PART_DEAD;
		    local_msg.dst_part = dest;
		    if(send_to_daem(dest, &local_msg))  {
			dprintf(1, ("clshmd: couldn't send PART_DEAD "
				    "message to part %d\n", dest));
		    }
		    clean_up_dead_part(i);
		    gremote_parts[i].connect_state = CLSHMD_UNCONNECTED;
		}
	    }
	    close_xp_fds();
	    unmap_shared_kernel_region();
	    if(gdrv_fd != 0)  {
		close(gdrv_fd);
		gdrv_fd = -1;
	    }
		
	    /* this will cause the daemon to exit */
	    exit_status = 1;
	    break;
	}


	/* driver is telling us that a new partition has been born */
        case DRV_TO_DAEM_NEW_PART:  {
	    partid_t part_num;

	    dprintf(5, ("clshmd: DRV_TO_DAEM_NEW_PART birth of "
			"part %d; RIP!\n", local_msg.dst_part));

	    /* should be a brand new partition */
	    part_num = local_msg.dst_part;

	    /* if connected at all, kill it!!! */
	    if(gremote_parts[part_num].connect_state) {
		dprintf(1, ("clshmd: got NEW_PART from driver when we "
			    "already knew of partition %d\n", part_num));
		dprintf(5, ("clshmd: already connected, unconnecting "
			    "and reconnecting again %d\n", part_num));
		clean_up_dead_part(part_num);
	    }
	    gremote_parts[part_num].connect_state = CLSHMD_DRV_CONNECTED;

	    /* open connection to other partition */
	    if(open_part_cl_dev(part_num) < 0) {
		dprintf(1, ("clshmd: can't open path to partition %d\n",
			    part_num));
		local_msg.type = DAEM_TO_DRV_PART_DEAD;
		local_msg.src_part = part_num;
		local_msg.dst_part = gown_part_id;
		send_to_drv(&local_msg);
	    }
	    break;
	}

	case DRV_TO_DAEM_TAKE_SHMID: {
	    clshmd_shared_area_t *area;
	    clshmd_shmid_t *prev, *elem;
	    
	    dprintf(5, ("clshmd: DRV_TO_DAEM_TAKE_SHMID\n"));

	    area = put_msg_in_area(&local_msg);

	    if(!area) {
		dprintf(1, ("clshmd: bad return from put_msg_in_area\n"));
		break;
	    }
		    
	    if(area->state == WAITING_SHARED) {
		/* not done yet */
		break;
	    }
	    
	    /* if the area is in a bad state, put_msg_in_area will have
	     * modified the message to send back and placed the error
	     * in the area->error field, so just send to all askers
	     * at this point. */
	    local_msg.src_part = gown_part_id;
	    for(prev = NULL, elem = gshmid_list; elem != NULL; ) {
		if(elem->key == area->key) {
		    if(elem->fd == -1 && elem->dst_part == -1) {
			dprintf(1, ("clshmd: no one to respond to in "
				    "shmid list\n"));
		    }
		    /* only send to users when we are done */
		    if(elem->fd > 0) {
			clshm_msg_t temp_msg;

			dprintf(5, ("clshmd: sending area back to "
				    "user shmid = 0x%x\n", area->shmid));
			bcopy(&local_msg, &temp_msg, sizeof(clshm_msg_t));
			temp_msg.dst_part = gown_part_id;

			/* send_shmid_to_taker expect the src and dest
			 * to be swapped because this is normally used in
			 * an error case to send back an answer to other
			 * daemon -- but both are set to gown_part_id, so
			 * this shouldn't be a problem here!!! */

			send_shmid_to_taker(&temp_msg, area->shmid,
					    area->error, 1, elem->fd);
		    } 
		    /* need to pass all info onto other daemon */
		    if(elem->dst_part >= 0 && 
		       elem->dst_part != gown_part_id) {
			local_msg.dst_part = elem->dst_part;
			if(send_area_to_daem(area, &local_msg) < 0) {
			    dprintf(1, ("clshmd: couldn't sent local msg to "
					"remote deamon %d\n", 
					elem->dst_part));
			    /* couldn't sent to daemon, but don't give up */
			}
		    }
		    if(prev) {
			prev->next = elem->next;
			check_free(elem);
			elem = prev->next;
		    } else {
			gshmid_list = elem->next;
			check_free(elem);
			elem = gshmid_list;
		    }
		} else {
		    prev = elem;
		    elem = elem->next;
		}
	    }
	    break;
	}

        case DRV_TO_DAEM_TAKE_PATH: {
	    shmid_in_msg shmid;
	    clshmd_path_t *prev, *path;
	    clshmd_shared_area_t *area;
	    int len;

	    dprintf(5, ("clshmd: DRV_TO_DAEM_TAKE_PATH\n"));

	    if(local_msg.len > MAX_MSG_LEN) {
		dprintf(0, ("clshmd: bad length for path msg len %d\n",
			    local_msg.len));
		break;
	    }
	    bcopy(local_msg.body, &shmid, sizeof(shmid_in_msg));
	    for(prev = NULL, path = gnode_path; path != NULL; 
		prev = path, path = path->next) {
		if(path->shmid == shmid) {
		    break;
		}
	    }
	    if(!path) {
		dprintf(5, ("clshmd: shmid %x path not found\n", shmid));
		break;
	    }
	    if(prev) {
		prev->next = path->next;
	    } else {
		gnode_path = path->next;
	    }

	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->shmid == shmid) {
		    break;
		}
	    }
	    if(area) {
		len = local_msg.len - sizeof(shmid_in_msg);
		area->nodepath = (char *) check_malloc(len + 1);
		bcopy(local_msg.body + sizeof(shmid_in_msg), 
		      area->nodepath, len);
		area->nodepath[len] = '\0';
	    } else {
		shmid = CLSHMD_ABSENT_SHMID;
		bcopy(&shmid, local_msg.body, sizeof(shmid_in_msg));
		dprintf(5, ("clshmd: can't find area for path shmid: %d\n",
			    shmid));
	    }

	    /* tell the user we the path */
	    local_msg.type = DAEM_TO_USR_TAKE_PATH;
	    if(send_to_usr(path->fd, &local_msg) < 0) {
		dprintf(5, ("clshmd: can't sent path to user fd %d\n",
			    path->fd));
	    }
	    check_free(path);
	    break;
	}

	/* remove the id -- something bad has happened that only the
	 * driver knows about -- probably a error on the shared page. */
        case DRV_TO_DAEM_RMID: {
	    clshmd_shared_area_t *area;
	    shmid_in_msg shmid;

	    dprintf(5, ("clshmd: DRV_TO_DAEM_RMID\n"));

	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    area = remove_local_shmid_area(shmid);
	    if(!area) {
		dprintf(5, ("clshmd: shmid %x RMID and not found\n",
			    shmid));
		break;
	    }
	    remove_remote_shmid_area(area);
	    free_area(area);

	    break;
	}

	/* the driver saw the first attach to a segment */
        case DRV_TO_DAEM_ATTACH: {
	    shmid_in_msg shmid;
	    partid_t dest;
	    clshmd_shared_area_t *area;

	    dprintf(5, ("clshmd: DRV_TO_DAEM_ATTACH\n"));

	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->shmid == shmid) {
		    break;
		}
	    }
	    if(!area) {
		dprintf(0, ("clshmd: drv sent attach for area we don't "
			    "have shmid %x\n", shmid));
		break;
	    }

	    dest = SHMID_TO_PARTITION(shmid);
	    if(dest >= MAX_PARTITIONS || dest < 0) {
		dprintf(0, ("clshmd: daemon sent us shmid with bad partition "
			    "number in it %d\n", dest));
		break;
	    }

	    /* if we own it, just mark it and go on */
	    if(dest == gown_part_id) {
		area->auto_cleanup |= FIRST_ATTACH_SEEN;
		if(!(area->parts_state[gown_part_id] & AREA_ASKED_FOR)) {
		    dprintf(0, ("clshmd: can't attach area that has never "
				"been asked for!!!, shmid %x\n", shmid));
		}
		area->parts_state[gown_part_id] |= AREA_ATTACHED;

	    } else {
		
		/* send to owning partition */
		local_msg.type = DAEM_TO_DAEM_ATTACH;
		local_msg.dst_part = dest;
		if(send_to_daem(dest, &local_msg)) {
		    dprintf(0, ("clshmd: can't send on ATTACH to daemon %d\n",
				dest));
		}
	    }
	    break;
	}

	/* driver saw the last detach */
        case DRV_TO_DAEM_DETACH: {
	    shmid_in_msg shmid;
	    partid_t dest;
	    clshmd_shared_area_t *area;

	    dprintf(5, ("clshmd: DRV_TO_DAEM_DETACH\n"));

	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->shmid == shmid) {
		    break;
		}
	    }
	    /* ok in the case that a RMID message has been seen first */
	    if(!area) {
		dprintf(1, ("clshmd: drv sent detach for area we don't "
			    "have shmid %x\n", shmid));
		break;
	    }

	    /* if we are the owner just set our local area and check to
	     * see if this is the last detach */
	    dest = SHMID_TO_PARTITION(shmid);
	    if(dest >= MAX_PARTITIONS || dest < 0) {
		dprintf(0, ("clshmd: driver sent us bad dest partition %d\n",
			    dest));
		break;
	    }

	    if(dest == gown_part_id) {
		if(!(area->parts_state[dest] == 
		     (AREA_ASKED_FOR|AREA_ATTACHED))) {
		    dprintf(0, ("clshmd: area detached when not attached "
				"or asked for first!!! shmid %x\n", shmid));
		}
		area->parts_state[dest] &= ~AREA_ATTACHED;
		auto_remove_shmid_area(area);
	    } else {
		/* just pass on to the owning partition */
		local_msg.type = DAEM_TO_DAEM_DETACH;
		local_msg.dst_part = dest;
		if(send_to_daem(dest, &local_msg)) {
		    dprintf(0, ("clshmd: can't send on ATTACH to daemon %d\n",
				dest));
		}
	    }
	    break;
	}

    } /* end switch */
    return(exit_status);
}


/* process_daem_msg
 * ----------------
 * Parameters:
 *	msg:	The message that came to us
 *	return:	-1 for error, 0 else
 *
 * This is a message that we just received from another daemon, for all 
 * the different messages we can receive, parse through it and either 
 * send an answer from this to a user or pass it on to the driver
 *  depending on why this message came to us.
 */
static void process_daem_msg(clshm_msg_t *msg)
{
    clshm_msg_t		local_msg;

    if(msg == NULL || msg->dst_part != gown_part_id || 
       msg->src_part >= MAX_PARTITIONS || msg->src_part < 0) {
	dprintf(1, ("clshmd: got NULL or bad msg ptr in process_daem_msg\n"));
	return;
    }
    /* make a copy of the msg, since need to change fields */
    local_msg.src_part = msg->src_part;
    local_msg.dst_part = msg->dst_part;
    local_msg.len = msg->len;
    bcopy(msg->body, local_msg.body, local_msg.len);
    
    switch(msg->type)  {
        default:  {
	    dprintf(1, ("clshmd: ignoring unknown message type %c (%d) in "
			"process_daem_msg\n", msg->type, (int) msg->type));
	    break;
	}
	
	/* another daemon is telling us it is dying */
   	case DAEM_TO_DAEM_PART_DEAD:  {
	    partid_t part_num;

	    part_num = local_msg.src_part;
	    dprintf(5, ("clshmd: DAEM_TO_DAEM_PART_DEAD part %d is dead\n", 
			part_num));

	    if(gremote_parts[part_num].connect_state != CLSHMD_UNCONNECTED) {
		clean_up_dead_part(part_num);
	    } else {
		dprintf(1, ("clshmd: other daemon %d already dead\n",
			    part_num));
	    }
	    break;
	}
	    
	/* another partition is telling us it knows about us */
	case DAEM_TO_DAEM_NEW_PART: {
	    vers_in_msg vers_major, vers_minor;
	    pgsz_in_msg page_size;
	    int len;
	    partid_t part;

	    part = local_msg.src_part;

	    dprintf(5, ("clshmd: DAEM_TO_DAEM_NEW_PART new part %d\n", part));

	    if(gremote_parts[part].connect_state == CLSHMD_UNCONNECTED) {
		dprintf(1, ("clshmd: got connect for new partition "
			    "from partition %d that doesn't exist!!!!\n",
			    part));
		break;
	    }
	    if(gremote_parts[part].connect_state & 
	                                 CLSHMD_DAEM_CONNECTED) {
		dprintf(1, ("clshmd: DAEM_TO_DAEM_NEW_PART called "
			    "when already connected part %d\n",
			    part));
		break;
	    }
	    if(msg->len != (sizeof(vers_in_msg) * 2) + sizeof(pgsz_in_msg)) {
		dprintf(1, ("clshmd: bad message for DAEM_TO_DAEM_NEW"
			    "_PART len = %d\n", msg->len));
		break;
	    }

	    bcopy(msg->body, &vers_major, sizeof(vers_in_msg));
	    len = sizeof(vers_in_msg);
	    bcopy(msg->body + len, &vers_minor, sizeof(vers_in_msg));
	    len += sizeof(vers_in_msg);
	    bcopy(msg->body + len, &page_size, sizeof(pgsz_in_msg));
	    if(vers_major != CLSHM_DAEM_MAJOR_VERS) {
		dprintf(1, ("clshmd: tried to get connect with "
			    "daemon with bad major version number %d\n",
			    vers_major));
		break;
	    } else if(page_size != gpage_size) {
		dprintf(1, ("clshmd: tried to get connect with "
			    "daemon with bad page_length %d\n", page_size));
		break;
	    }
	    gremote_parts[part].connect_state |= CLSHMD_DAEM_CONNECTED;
	    gremote_parts[part].daem_minor_vers = vers_minor;
	    local_msg.type = DAEM_TO_DRV_NEW_PART;
	    local_msg.len = 0;
	    send_to_drv(&local_msg);
	    break;
	}

   	/* another daemon wants our hostname */
        case DAEM_TO_DAEM_NEED_HOSTNAME:  {

	    dprintf(5, ("clshmd: DAEM_TO_DAEM_NEED_HOSTNAME from %d\n",
			local_msg.src_part));

	    if(set_local_hostname(local_msg.body) < 0) {
		dprintf(1, ("clshmd: error in hostname\n"));
		local_msg.len = 0;
	    } else {
		dprintf(1, ("clshmd: Returned hostname %s to daem on "
			    "part %d\n", local_msg.body, 
			    local_msg.src_part));
		local_msg.len = strlen(local_msg.body);
	    }
	    /* return to sender */
	    local_msg.type = DAEM_TO_DAEM_TAKE_HOSTNAME;
	    local_msg.dst_part = local_msg.src_part;
	    local_msg.src_part = gown_part_id;
	    if(send_to_daem(local_msg.dst_part, &local_msg)) {
		dprintf(1, ("clshmd couldn't send to other part %d\n",
			    local_msg.dst_part));
	    }
	    break;
	}

	/* another daemon told us about its hostname. */
   	case DAEM_TO_DAEM_TAKE_HOSTNAME:  {
	    clshmd_hostname_t *host, *prev;

	    dprintf(5, ("clshmd: DAEM_TO_DAEM_TAKE_HOSTNAME from %d\n", 
			local_msg.src_part));

	    if(local_msg.len > MAX_MSG_LEN) {
		local_msg.len = 0;
		dprintf(1, ("clshmd: got bad hostname from other "
			    "partition with len %d\n", local_msg.len));
	    } else {
		char name[MAX_MSG_LEN+1];
		bcopy(local_msg.body, name, local_msg.len);
		name[local_msg.len] = '\0';
		dprintf(5, ("clshmd: Got hostname of %s from part %d\n",
			    name, (int) local_msg.src_part));
	    }

	    /* send it back to the users that wanted to hostname */
	    local_msg.type = DAEM_TO_USR_TAKE_HOSTNAME;
	    for(prev = NULL, host = gpart_hostnames; host != NULL; ) {
		if(local_msg.src_part == host->dst_part) {
		    if(send_to_usr(host->fd, &local_msg) < 0) {
			dprintf(1, ("clshmd: can't send to user fd %d\n",
				    host->fd));
		    }
		    if(prev) {
			prev->next = host->next;
			check_free(host);
			host = prev->next;
		    } else {
			gpart_hostnames = host->next;
			check_free(host);
			host = gpart_hostnames;
		    }
		} else {
		    prev = host;
		    host = host->next;
		}
	    }
	    break;
	}

	/* another daemon wants a key so make it and send it back */
        case DAEM_TO_DAEM_NEED_KEY:  {
	    clshmd_shared_area_t *new_area;

	    dprintf(5, ("clshmd: DAEM_TO_DAEM_NEED_KEY from %d\n",
			local_msg.src_part));

	    if(local_msg.len != (sizeof(pid_in_msg))) {
		dprintf(1, ("clshmd: bad msg body of NEED_KEY len %d\n",
			    local_msg.len));
		/* other daemon really screwed up in this case, so
		 * don't send back an error */
		break;
	    }
	    new_area = make_new_area(CLSHMD_ABSENT_KEY, 0);

	    local_msg.type = DAEM_TO_DAEM_TAKE_KEY;
	    bcopy(&(new_area->key), local_msg.body + sizeof(pid_in_msg),
		  sizeof(key_in_msg));
	    local_msg.len = sizeof(pid_in_msg) + sizeof(key_in_msg);
	    local_msg.dst_part = local_msg.src_part;
	    local_msg.src_part = gown_part_id;
	    if(send_to_daem(local_msg.dst_part, &local_msg)) {
		dprintf(1, ("clshmd couldn't send to remote daemon %d\n",
			    local_msg.dst_part));
	    }
	    break;
	}

        case DAEM_TO_DAEM_TAKE_KEY:  {
	    clshmd_key_t *elem, *prev;
	    pid_in_msg pid;
	    partid_t dest;
	    key_in_msg key;
	    int fd;
	    char *cpy_ptr;
	    int cpy_len;
	    err_in_msg error_num = 0;
	    
	    dprintf(5, ("clshmd: DAEM_TO_DAEM_TAKE_KEY from %d\n", 
			local_msg.src_part));

	    /* check the message we got back */
	    if(local_msg.len != (sizeof(pid_in_msg) + 
				 sizeof(key_in_msg))) {
		dprintf(1, ("clshmd: TAKE_KEY got bad msg len %d\n", 
			    local_msg.len));
		break;
	    }
	    cpy_ptr = local_msg.body;
	    cpy_len = 0;
	    bcopy(cpy_ptr, &(pid), sizeof(pid_in_msg));
	    cpy_ptr += sizeof(pid_in_msg);
	    cpy_len += sizeof(pid_in_msg);

	    bcopy(cpy_ptr, &(key), sizeof(key_in_msg));
	    cpy_ptr += sizeof(key_in_msg);
	    cpy_len += sizeof(key_in_msg);

	    /* find a user that wants this key with the particular pid */
	    dprintf(7, ("clshmd: got key from other daem: 0x%x\n", key));
	    dest = local_msg.src_part;
	    for(prev = NULL, elem = gkey_list; elem != NULL; 
		prev = elem, elem = elem->next) {
		if(elem->pid == pid && elem->dst_part == dest) {
		    break;
		}
	    }
	    if(!elem) {
		dprintf(1, ("clshmd: TAKE_KEY without a NEED_KEY key %x\n",
			    key));
		break;
	    }
	    fd = elem->fd;
	    if(prev) {
		prev->next = elem->next;
	    } else {
		gkey_list = elem->next;
	    }
	    check_free(elem);

	    /* send it back to the user */
	    local_msg.type = DAEM_TO_USR_TAKE_KEY;
	    bcopy(cpy_ptr, &error_num, sizeof(err_in_msg));
	    local_msg.len = cpy_len + sizeof(err_in_msg);
	    if(send_to_usr(fd, &local_msg) < 0) {
		dprintf(1, ("clshmd: can't send to user fd %d\n", fd));
	    }
	    (void) make_new_area(key, 0);
	    break;
	}


        case DAEM_TO_DAEM_NEED_SHMID: {
	    clshmd_shared_area_t *new_area;
	    len_in_msg len, cpy_len;
	    key_in_msg key;
	    char *cpy_ptr;
	    int offset;
	    
	    dprintf(5, ("clshmd: DAEM_TO_DAEM_NEED_SHMID from part %d\n", 
			local_msg.src_part));

	    /* check what we are passed in */
	    if(local_msg.len != (sizeof(key_in_msg) + 
				 sizeof(len_in_msg))) {
		dprintf(1, ("clshmd: bad parameters in body of NEED_SHMID "
			    "message len %d\n", local_msg.len));
		break;
	    }

	    cpy_len = 0;
	    cpy_ptr = local_msg.body;
	    bcopy(cpy_ptr, &(key), sizeof(key_in_msg));
	    cpy_ptr += sizeof(key_in_msg);
	    cpy_len += sizeof(key_in_msg);
	    
	    bcopy(cpy_ptr, &(len), sizeof(len_in_msg));
	    cpy_ptr += sizeof(len_in_msg);
	    cpy_len += sizeof(len_in_msg);
	    
	    if(len <= 0 || KEY_TO_PARTITION(key) != local_msg.dst_part
	       || key == CLSHMD_ABSENT_KEY || 
	       local_msg.dst_part != gown_part_id) {
		dprintf(1, ("clshmd: NEED_SHMID len too small: %lld or "
			    "key %x doesn't match message\n", len, key));
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID, 
				    EINVAL, 0, -1);
		break;	
	    }
	    
	    /* create or find a new area */
	    new_area = find_new_area(key, len);
	    
	    if(!new_area) {
		dprintf(1, ("clshmd: NEED_SHMID asked for non-exist "
			    "key 0x%x\n", key));			    
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID, 
				    EINVAL, 0, -1);
		break;
	    }

	    if(new_area->state != INCOMPLETE_SHARED && new_area->len != len) {
		dprintf(1, ("clshmd: key doesn't match prior len %d\n", 
			    (int) len));
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID, 
				    EINVAL, 0, -1);
		break;
	    }

	    /* we need to make the shmid */
	    if(new_area->state == INCOMPLETE_SHARED) {

		move_area_to_waiting(new_area, len);

		offset = get_free_page_range(new_area->num_addrs);
		if(offset < 0) {
		    send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID, 
					ENOMEM, 0, -1);
		    fprintf(stderr, "clshmd: no large enough contigous "
			    "space in daemon\n");
		    break;
		}
	    
		new_area->shmid = MAKE_SHMID(gown_part_id, offset);
	    }

	    /* tell driver of this NEED_SHMID */
	    if(inform_drv_of_need_shmid(new_area, 
					local_msg.src_part) < 0) {
		send_shmid_to_taker(&local_msg, CLSHMD_ABSENT_SHMID,
				    errno, 0, -1);
		break;
	    }
	    if(!(new_area->parts_state[local_msg.src_part] & 
		 AREA_ASKED_FOR)) {
		new_area->parts_state[local_msg.src_part] |= AREA_ASKED_FOR;
	    }

	    /* if we already have the area, just return it */
	    if(new_area->state == COMPLETE_SHARED) {
		local_msg.dst_part = local_msg.src_part;
		local_msg.src_part = gown_part_id;
		if(send_area_to_daem(new_area, &local_msg) < 0) {
		    dprintf(1, ("clshmd couldn't send to daemon, %d\n",
				local_msg.dst_part));
		}
		break;
	    }

	    /* we need to return this to the caller */
	    (void) make_new_shmid(-1, key, local_msg.src_part);
	    
	    break;
	}
	

	/* the other daemon got back to us on a shmid that we wanted */
        case DAEM_TO_DAEM_TAKE_SHMID: {
	    clshmd_shared_area_t *area;
	    clshmd_shmid_t *prev, *elem;
	    char *cpy_ptr;
	    err_in_msg err;
	    int error = 0;
	    
	    dprintf(5, ("clshmd: DAEM_TO_DAEM_TAKE_SHMID from part %d\n",
			local_msg.src_part));

	    area = put_msg_in_area(&local_msg);
	    
	    if(!area) {
		dprintf(1, ("clshmd: bad return from put_msg_in_area\n"));
		break;
	    }
	    
	    /* put_msg_in_area will have filled out the local_msg with
	     * an error if something was wrong with the message.  We need
	     * to send this on to the driver since it could be along in 
	     * the sequence of message for this shmid */
	    local_msg.type = DAEM_TO_DRV_TAKE_SHMID;
	    cpy_ptr = local_msg.body + sizeof(key_in_msg) + 
		sizeof(len_in_msg);
	    if(send_to_drv(&local_msg) < 0) {
		shmid_in_msg shmid_err;
		
		dprintf(1, ("clshmd: couldn't do TAKE_SHMID to driver\n"));
		error = 1;
		shmid_err = CLSHMD_ABSENT_SHMID;
		err = errno;
		bcopy(&(shmid_err), cpy_ptr, sizeof(shmid_in_msg));
		/* still send this message back to all askers */
		/* so no break here */
	    } else {
		err = area->error;
	    }

	    if(!error && area->state == WAITING_SHARED) {
		/* not done yet */
		break;
	    }

	    /* we are done with the shmid, so send it back to any 
	     * users that want it */
	    local_msg.type = DAEM_TO_USR_TAKE_SHMID;
	    local_msg.len = sizeof(key_in_msg) + sizeof(len_in_msg) +
		sizeof(shmid_in_msg) + sizeof(err_in_msg);

	    cpy_ptr += sizeof(shmid_in_msg);
	    bcopy(&(err), cpy_ptr, sizeof(err_in_msg));

	    for(prev = NULL, elem = gshmid_list; elem != NULL; ) {
		if(elem->key == area->key) {
		    if(elem->fd <= 0) {
			dprintf(1, ("clshmd: bad file descriptor fd %d\n",
				    elem->fd));
			break;
		    }
		    if(send_to_usr(elem->fd, &local_msg) < 0) {
			dprintf(1, ("clshmd: can't send to user fd %d\n",
				    elem->fd));
			break;
		    }
		    if(prev) {
			prev->next = elem->next;
			check_free(elem);
			elem = prev->next;
		    } else {
			gshmid_list = elem->next;
			check_free(elem);
			elem = gshmid_list;
		    }
		} else {
		    prev = elem;
		    elem = elem->next;
		}
	    }
	    break;
	}
	

        case DAEM_TO_DAEM_RMID: {
	    clshmd_shared_area_t *area;
	    shmid_in_msg shmid;

	    dprintf(5, ("clshmd: DAEM_TO_DAEM_RMID from part %d\n", 
			local_msg.src_part));

	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    area = remove_local_shmid_area(shmid);
	    if(!area) {
		dprintf(5, ("clshmd: shmid %x RMID and not found\n",
			    shmid));
		break;
	    }
	    if(remove_driver_shmid_area(area) < 0) {
		dprintf(5, ("clshmd: failed in ioctl to RMID\n"));
	    }

	    /* if we are the owner, then send to all the partitions
	     * that have access to this page */
	    if(SHMID_TO_PARTITION(shmid) == gown_part_id) {
		if(!(area->parts_state[local_msg.src_part] & 
		     AREA_ASKED_FOR)) {
		    dprintf(0, ("clshmd: partition removed from area "
				"without being asked for first!!! shmid "
				"%x\n", shmid));
		}
		area->parts_state[local_msg.src_part] = 0;

		remove_remote_shmid_area(area);
	    } 
	    free_area(area);
	    break;
	}

        case DAEM_TO_DAEM_AUTORMID: {
	    clshmd_shared_area_t *area;
	    clshmd_autormid_t *prev, *armid;
	    shmid_in_msg shmid;
	    err_in_msg err;

	    dprintf(5, ("clshmd: DAEM_TO_DAEM_AUTORMID from part %d\n",
			local_msg.src_part));
	    
	    /* check parameter */
	    if(local_msg.len != sizeof(shmid_in_msg) + sizeof(err_in_msg)) {
		/* usr is screwed up for sending this */
		dprintf(0, ("clshmd: daem sent bad len %d for AUTORMID\n",
			    local_msg.len));
		break;
	    }
	    
	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    bcopy(local_msg.body + sizeof(shmid_in_msg), &(err), 
		  sizeof(err_in_msg));

	    dprintf(5, ("clshmd: AUTORMID shmid %x, err %d\n", shmid, err));

	    /* see if we are receiving an answer or getting asked for 
	     * the AUTORMID to be instanciated. */
	    if(SHMID_TO_PARTITION(shmid) == gown_part_id) {
		
		if(err != 0) {
		    dprintf(0, ("clshmd: daem sent bad err for AUTORMID\n"));
		    break;
		}

		/* find the area locally */
		for(area = gshared_area; area != NULL; area = area->next) {
		    if(shmid == area->shmid) {
			break;
		    }
		}

		/* the area was found */
		if(area) {
		    area->auto_cleanup |= AUTO_CLEANUP_ON;
		    /* check to see if we should clean it up now */
		    auto_remove_shmid_area(area);
		} else {
		    dprintf(5, ("clshmd: can't find area asked for in "
				"AUTORMID shmid %x\n", shmid));
		    err = EINVAL;
		}
	       
		/* send back to asker */

		dprintf(7, ("clshmd: AUTORMID to daem err = %d\n", err));
		bcopy(&err, local_msg.body + sizeof(shmid_in_msg),
		      sizeof(err_in_msg));
		local_msg.len = sizeof(shmid_in_msg) + sizeof(err_in_msg);
		local_msg.type = DAEM_TO_DAEM_AUTORMID;
		local_msg.dst_part = local_msg.src_part;
		local_msg.src_part = gown_part_id;
		if(send_to_daem(local_msg.dst_part, &local_msg)) {
		    dprintf(0, ("clshmd: can't send AUTORMID to daem %d\n",
				local_msg.dst_part));
		    break;
		}		
		
		break;
	    }
		
	    /* hopefully we requested this shmid for autocleanup */
	    for(armid = gautormid_list, prev = NULL; armid != NULL; 
		prev = armid, armid = armid->next) {
		if(armid->shmid == shmid) {
		    break;
		}
	    }

	    /* if we found it, send back to user */
	    if(armid) {
		dprintf(7, ("clshmd: AUTORMID to usr err = %d\n", err));
		bcopy(&err, local_msg.body, sizeof(err_in_msg));
		local_msg.type = DAEM_TO_USR_AUTORMID;
		local_msg.len = sizeof(err_in_msg);
		if(send_to_usr(armid->fd, &local_msg) < 0) {
		    dprintf(0, ("clshmd: can't send to AUTORMID to user fd "
				"%d\n", armid->fd));
		}
		if(prev) {
		    prev->next = armid->next;
		} else {
		    gautormid_list = armid->next;
		}
		check_free(armid);
	    } else {
		/* got back message we weren't waiting for */
		dprintf(0, ("clshmd: got back AUTORMID for strange shmid "
			    "%x\n", shmid));
	    }
	    break;
	}

        case DAEM_TO_DAEM_ATTACH: {
	    shmid_in_msg shmid;
	    clshmd_shared_area_t *area;
	    
	    dprintf(5, ("clshmd: DAEM_TO_DAEM_ATTACH from part %d\n", 
			local_msg.src_part));

	    if(local_msg.len != sizeof(shmid_in_msg)) {
		dprintf(0, ("clshmd: got bad length from daemon "
			    "ATTACH msg len %d\n", local_msg.len));
		break;
	    }
	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    if(SHMID_TO_PARTITION(shmid) != gown_part_id) {
		dprintf(0, ("clshmd: ATTACH from daemon shmid doesn't "
			    "match local shmid %x\n", shmid));
		break;
	    }
	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->shmid == shmid) {
		    break;
		}
	    }
	    if(!area) {
		dprintf(0, ("clshmd: daem sent attach for area we don't "
			    "have shmid %x\n", shmid));
		break;
	    }

	    /* we are the owner so just set our local area */
	    area->auto_cleanup |= FIRST_ATTACH_SEEN;
	    area->parts_state[local_msg.src_part] |= AREA_ATTACHED;
	    break;
	}

        case DAEM_TO_DAEM_DETACH: {
	    shmid_in_msg shmid;
	    clshmd_shared_area_t *area;
	    
	    dprintf(5, ("clshmd: DAEM_TO_DAEM_DETACH from part %d\n", 
			local_msg.src_part));

	    if(local_msg.len != sizeof(shmid_in_msg)) {
		dprintf(0, ("clshmd: got bad length from daemon "
			    "DETACH msg len %d\n", local_msg.len));
		break;
	    }
	    bcopy(local_msg.body, &(shmid), sizeof(shmid_in_msg));
	    if(SHMID_TO_PARTITION(shmid) != gown_part_id) {
		dprintf(0, ("clshmd: DETACH from daemon shmid doesn't "
			    "match local shmid %x\n", shmid));
		break;
	    }
	    for(area = gshared_area; area != NULL; area = area->next) {
		if(area->shmid == shmid) {
		    break;
		}
	    }
	    if(!area) {
		dprintf(0, ("clshmd: daem sent detach for area we don't "
			    "have shmid %x\n", shmid));
		break;
	    }

	    /* we are the owner so just set our local area and check to
	     * see if this is the last detach */
	    area->parts_state[local_msg.src_part] &= ~AREA_ATTACHED;

	    /* see if this is the last detach */
	    auto_remove_shmid_area(area);
	    break;
	}
    }
}



/* init_client_socket
 * ------------------
 * Parameters:
 *	return:	-1 for failure and 0 for success
 *
 * Open up a socket to listen to for users to connect to to this daemon
 * to make requests for shared memroy 
 */
static int init_client_socket(void)
{
    int i;
    struct sockaddr_un      sname ;

    /* open the socket */
    /* make sure the daemon isn't already running */
    if (is_clshmd_running()) {
	fprintf(stderr, "clshmd: appears to already be running\n");
	return(-1);
    }

    gClientSock = socket(PF_UNIX, SOCK_STREAM, 0) ;
    if (gClientSock < 0) {
	return(-1);
    }

    sname.sun_family = AF_UNIX ;
    strcpy(sname.sun_path, CLSHMD_SOCKET_NAME) ;
    
    if (bind(gClientSock, (void *)&sname, sizeof(sname)) < 0) {
	unlink(sname.sun_path) ;
	if (bind(gClientSock, (void *)&sname, sizeof(sname)) < 0) {
	    fprintf(stderr, "clshmd: can't bind to socket %s\n", 
		    CLSHMD_SOCKET_NAME);
	    gClientSock = -1 ;
	    return(-1);
	}
    }
    
    /* give everyone read and write permissions on the file */
    chmod(sname.sun_path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    
    if (listen(gClientSock, CLSHMD_SOCKET_MAX)) {
	gClientSock = -1 ;
	return(-1);
    }

    /* init the user fds */
    for(i = 0; i < CLSHMD_SOCKET_MAX; i++) {
	guser_fds[i] = -1;
   }

    return(0);
}


/* is_clshmd_running
 * -----------------
 * Parameters:
 *	return:	0 for failure and 1 for success 
 *
 * Look for a process running that is named clshmd.  Should only find
 * once since we are a clshmd running process.
 */
static int is_clshmd_running(void)
{
    DIR             *dirp ;
    struct dirent   *dentp ;
    int             pdlen ;
    char            pname[128] ;
    struct prpsinfo info ;
    int             num_clshmd = 0 ;

    if ((dirp = opendir(_PATH_PROCFSPI)) == NULL)
        /* Assume things are OK. Let it fail elsewhere. */
	return(1);

    (void) sprintf(pname, "%s%c", _PATH_PROCFSPI, '/');
    pdlen = strlen(pname);

    while (dentp = readdir(dirp)) {
	int     procfd;         /* fd for /proc/pinfo/nnnnn */
	char   *plblstring;     /* process label string */

	if (strcmp(dentp->d_name, ".") == 0 ||
	    strcmp(dentp->d_name, "..") == 0 ||
	    (dentp->d_name[0] == '.'))
	    continue;
	
	plblstring = NULL;
	(void) strcpy(pname + pdlen, dentp->d_name);
	
	if ((procfd = open(pname, O_RDONLY)) == -1) {
	    mac_free(plblstring);
	    continue;
	}

	if (ioctl(procfd, PIOCPSINFO, (char *) &info) == -1) {
	    (void) close(procfd);
	    continue ;
	}

	(void) close(procfd);
	
	if (strstr(info.pr_fname, "clshmd")) {
	    if(info.pr_zomb) {
		fprintf(stderr, "clshmd: sees clshmd zomb\n");
	    }
	    dprintf(0, ("clshmd: sees pid connected to clshmd: %d\n", 
			info.pr_pid));
	    num_clshmd ++ ;
	}
       
    }

    (void) closedir(dirp);

    return((num_clshmd > 1) ? 1 : 0);
}


/* i_am_coming_up
 * --------------
 * Parameters:
 * 	dest:	partition to tell we are coming up to
 * 	
 * If the partition coming up isn't our own, send it a NEW_PART message.
 * This way we can make sure that the driver thinks that the other
 * partition is alive and we think it is alive and connected to us
 * and talking to us in a sane mannor.
 */
static void i_am_coming_up(partid_t dest)
{
    clshm_msg_t		local_msg;
    int			error;
    pgsz_in_msg		page_size;
    vers_in_msg		major_version, minor_version;

    if(dest != gown_part_id)  {
	page_size = gpage_size;
	local_msg.type = DAEM_TO_DAEM_NEW_PART;
	local_msg.src_part = gown_part_id;
	local_msg.dst_part = dest;
	major_version = CLSHM_DAEM_MAJOR_VERS;
	minor_version = CLSHM_DAEM_MINOR_VERS;
	bcopy(&major_version, local_msg.body, sizeof(vers_in_msg));
	local_msg.len = sizeof(vers_in_msg);
	bcopy(&minor_version, local_msg.body + local_msg.len, 
	      sizeof(vers_in_msg));
	local_msg.len += sizeof(vers_in_msg);
	bcopy(&page_size, local_msg.body + local_msg.len,
	      sizeof(pgsz_in_msg));
	local_msg.len += sizeof(pgsz_in_msg);
	error = send_to_daem(dest, &local_msg);
	if(error)  {
	    dprintf(1, ("clshmd: Couldn't send to clshmd %d with error %d\n", 
			dest, error));
	}
    }
}


/* open_part_cl_dev
 * -----------------
 * Parameters:
 *	part:	partition index and partid for gremote_parts array that we
 *		want to open cl device to
 *	return:	-1 on failure and 0 on success
 *
 * open up the admin clshm to another partition, open it with no delay
 * so that we can continue even in that dummy hasn't openned it yet
 */
static int open_part_cl_dev(partid_t part_num) 
{
    char devname[128];
    rpart_info_t *part;

    part = &(gremote_parts[part_num]);
    dprintf(1, ("clshmd: trying to open socket to new partition %d\n",
		part_num));
    sprintf(devname, "%s/%02x/%s", XP_ADMIN_PATH, part_num, "clshm");
    dprintf(5, ("clshmd: Trying to open %s... (in find_parts); fd is "
		"%d before open\n", devname, 
		part->daem_fd));
    /* need to open exclusive, or others' writes will
     *  interfere with clshmd's reads */
    part->daem_fd = open(devname, O_RDWR | O_EXCL | O_NDELAY);
    if(part->daem_fd < 0)  {
	dprintf(0, ("clshmd: Couldn't open device %s\n", devname));
	perror("clshmd");
	part->daem_fd = -1;
	return(-1);
    }
    dprintf(5, ("clshmd: opened xp dev %s, fd %d\n", devname, part->daem_fd));
    return(0);
}


/* find_new_part
 * --------------
 * Try and discover new partitions that we have not discovered and
 * the driver isn't going to tell us about.
 */
static void find_new_parts(void)
{
    int			i;
    partid_t		part_num;
    part_info_t		part_info[MAX_PARTITIONS];
    uint            	configed_parts;
    rpart_info_t	*part;

    configed_parts = syssgi(SGI_PART_OPERATIONS, SYSSGI_PARTOP_PARTMAP,
			MAX_PARTITIONS, part_info);		

    for(i = 0; i < configed_parts; i++)  {
        part_num = part_info[i].pi_partid;
	if(part_num >= MAX_PARTITIONS || part_num < 0) {
	    dprintf(1, ("clshmd: got bad partition number %d for syssgi!!\n",
			part_num));
	    break;
	}
	part = &(gremote_parts[part_num]);
        if(part_num != gown_part_id && 
	   part->connect_state != CLSHMD_CONNECTED && part->daem_fd < 0) {
	    if(part->connect_state == CLSHMD_UNCONNECTED) {
		part->connect_state = CLSHMD_DRV_CONNECTED;
	    }
	    if(open_part_cl_dev(part_num) < 0) {
		dprintf(1, ("clshmd: can't open device to partition "
			    "number %d\n", part_num));
	    } 
        } else if(part_num == gown_part_id && 
		  part->connect_state == CLSHMD_UNCONNECTED) {
	    part->connect_state = CLSHMD_DRV_CONNECTED;
	}
    }
}


/* try_unconnected_parts
 * ----------------------
 * We had decided that the driver will tell us about any new partitions,
 * so we just poll the ones that the driver has told us about but that
 * daemons haven't tried to contact us yet
 */
static void try_unconnected_parts(void)
{
    int i;

    for(i = 0; i < MAX_PARTITIONS; i++)  {
	if(gremote_parts[i].connect_state == CLSHMD_DRV_CONNECTED &&
	   gremote_parts[i].daem_fd < 0 && i != gown_part_id) {
	    if(open_part_cl_dev(i) < 0) {
		dprintf(1, ("can't open device to partition number %d\n", i));
	    } 
        }
    }
}


/* close_xp_fds
 * ------------
 */
static void close_xp_fds(void)
{
    int		i;

    for(i = 0; i < MAX_PARTITIONS; i++)  {
	if(gremote_parts[i].daem_fd != -1) { 
	    close(gremote_parts[i].daem_fd);
	    gremote_parts[i].daem_fd = -1;
	}
    }
}


/* unmap_shared_kernel_region
 * --------------------------
 */
static void unmap_shared_kernel_region(void)
{
    if(gmapped_region)   {
	if(munmap(gmapped_region, gpage_size * gnum_pages))  {
	    dprintf(1, ("clshmd: error during unmapping of shared region "
			"at exit\n"));
	}
    }
}


/* clean_up_dead_part
 * -------------------
 * Parameters:
 *	part:	partition index of part that died
 *
 * We have detected that another partition has gone away.  Do what we can
 * to clean up any state that we have that might be associated with this
 * dead partition.
 */
static void clean_up_dead_part(partid_t part_num) 
{
    clshmd_shared_area_t *area, *prev, *next;
    clshm_msg_t msg;
    int removed;

    /* set the connectivity state */
    if(part_num == gown_part_id) {
	dprintf(0, ("clshmd: can't kill our own partition!!!\n"));
	return;
    }
    if(gremote_parts[part_num].connect_state == CLSHMD_UNCONNECTED) return;
    gremote_parts[part_num].connect_state &= ~(CLSHMD_DAEM_CONNECTED | 
						 CLSHMD_PIPE_CONNECTED);

    dprintf(5, ("clshmd: in clean_up_dead_part\n"));

    /* go through and see if we are expecting any answers from that 
     * partition and free up those resources and tell user that they 
     * aren't going to get a reply if they are waiting
     */
    delete_unfinished_business(-1, part_num);

    /* delete all areas that are on that partition -- telling the
     * driver that these resources are going away.
     */
    for(area = gshared_area, prev = NULL; area != NULL; ) {
	removed = 0;
	if(KEY_TO_PARTITION(area->key) == part_num && 
	   area->state != INCOMPLETE_SHARED) {
	    check_free(area->kpaddrs);
	    check_free(area->msg_recv);

	    msg.type = DAEM_TO_DRV_RMID;
	    msg.src_part = part_num;
	    msg.dst_part = gown_part_id;
	    msg.len = sizeof(shmid_in_msg);
	    bcopy(&(area->shmid), msg.body, sizeof(shmid_in_msg));
	    send_to_drv(&msg);

	    if(prev) {
		prev->next = area->next;
		check_free(area);
		area = prev->next;
	    } else {
		gshared_area = area->next;
		check_free(area);
		area = gshared_area;
	    }
	    removed = 1;
	} else {

	    /* if that partition has references to areas on this partition
	     * say that that is no longer so
	     */
	    area->parts_state[part_num] = 0;

	    if(SHMID_TO_PARTITION(area->shmid) == gown_part_id) {
		next = area->next;
		if(auto_remove_shmid_area(area)) {
		    removed = 1;
		    area = next;
		}
	    } 
	}

	if(!removed) {
	    prev = area;
	    area = area->next;
	}
    }

    /* tell the driver that this partition is dead for us */
    msg.type = DAEM_TO_DRV_PART_DEAD;
    msg.src_part = part_num;
    msg.dst_part = gown_part_id;
    msg.len = 0;
    send_to_drv(&msg);
    check_for_closed_fd(part_num);
}


/* check_for_closed_fd
 * -------------------
 * Parameters:
 * 	part:	partition to check the admin file is open for
 *	return: 0 for way closed, and 1 for opened and then closed
 */
static int check_for_closed_fd(partid_t part_num)
{
    if(gremote_parts[part_num].daem_fd == -1)  {
	return 0;
    } else {
	close(gremote_parts[part_num].daem_fd);
	gremote_parts[part_num].daem_fd = -1;
	return 1;
    }
}


/* delete_unfinished_business
 * --------------------------
 * Parameters:	
 *	fd:	fd for user, if user is the unfinished business
 *	part:	part if a partition went down
 *
 * If someone dies and there is unfinished business for that partition or
 * user (fd), then clean up the mess that it left behind, but just saying
 * that we no longer are requesting this information.
 */
static void delete_unfinished_business(int fd, partid_t part_num)
{
    clshmd_key_t *key_elem, *key_prev;
    clshmd_shmid_t *shmid_elem, *shmid_prev;
    clshmd_path_t *path_elem, *path_prev;
    clshmd_hostname_t *host_elem, *host_prev;
    clshmd_autormid_t *auto_elem, *auto_prev;
    clshm_msg_t msg;
    int cpy_len;
    char *cpy_ptr;

    /* free up all the hostname requests pending */
    for(host_elem = gpart_hostnames, host_prev = NULL; host_elem != NULL;) {
	if((fd > 0 && host_elem->fd == fd) ||
	   (part_num != -1 && host_elem->dst_part == part_num)) {
	    if(host_elem->fd > 0) {
		msg.type = DAEM_TO_USR_TAKE_HOSTNAME;
		msg.src_part = host_elem->dst_part;
		msg.dst_part = gown_part_id;
		msg.len = 0;
		send_to_usr(host_elem->fd, &msg);
	    }
	    if(host_prev) {
		host_prev->next = host_elem->next;
		check_free(host_elem);
		host_elem = host_prev->next;
	    } else {
		gpart_hostnames = host_elem->next;
		check_free(host_elem);
		host_elem = gpart_hostnames;
	    }
	} else {
	    host_prev = host_elem; 
	    host_elem = host_elem->next;
	}
    }

    /* clean up all the pending keys for this fd or partition */
    for(key_elem = gkey_list, key_prev = NULL; key_elem != NULL;) {
	if((fd > 0 && key_elem->fd == fd) ||
	   (part_num != -1 && key_elem->dst_part == part_num)) {
	    if(key_elem->fd > 0) {
		/* sent user error message */
		key_in_msg key = CLSHMD_ABSENT_KEY;
		err_in_msg err = EIO;
		msg.type = DAEM_TO_USR_TAKE_KEY;
		msg.src_part = key_elem->dst_part;
		msg.dst_part = gown_part_id;
		cpy_ptr = msg.body;
		cpy_len = 0;
		bcopy(&(key_elem->pid), cpy_ptr, sizeof(pid_in_msg));
		cpy_ptr += sizeof(pid_in_msg);
		cpy_len += sizeof(pid_in_msg);
		bcopy(&(key), cpy_ptr, sizeof(key_in_msg));
		cpy_ptr += sizeof(key_in_msg);
		cpy_len += sizeof(key_in_msg);
		bcopy(&(err), cpy_ptr, sizeof(err_in_msg));
		cpy_len += sizeof(err_in_msg);
		msg.len = cpy_len;
		send_to_usr(key_elem->fd, &msg);
	    }
	    if(key_prev) {
		key_prev->next = key_elem->next;
		check_free(key_elem);
		key_elem = key_prev->next;
	    } else {
		gkey_list = key_elem->next;
		check_free(key_elem);
		key_elem = gkey_list;
	    }
	} else {
	    key_prev = key_elem; 
	    key_elem = key_elem->next;
	}
    }

    /* free up all the shmid requests pending */
    for(shmid_elem = gshmid_list, shmid_prev = NULL; shmid_elem != NULL;) {
	if((fd > 0 && shmid_elem->fd == fd) ||
	   (part_num != -1 && shmid_elem->dst_part == part_num)) {
	    if(shmid_elem->fd > 0) {
		key_in_msg key = shmid_elem->key;
		shmid_in_msg shmid = CLSHMD_ABSENT_SHMID;
		len_in_msg len = 0;
		err_in_msg err = EIO;
		msg.type = DAEM_TO_USR_TAKE_KEY;
		msg.src_part = shmid_elem->dst_part;
		msg.dst_part = gown_part_id;
		cpy_ptr = msg.body;
		cpy_len = 0;
		bcopy(&(key), cpy_ptr, sizeof(key_in_msg));
		cpy_ptr += sizeof(key_in_msg);
		cpy_len += sizeof(key_in_msg);
		bcopy(&(len), cpy_ptr, sizeof(len_in_msg));
		cpy_ptr += sizeof(len_in_msg);
		cpy_len += sizeof(len_in_msg);
		bcopy(&(shmid), cpy_ptr, sizeof(shmid_in_msg));
		cpy_ptr += sizeof(shmid_in_msg);
		cpy_len += sizeof(shmid_in_msg);
		bcopy(&(err), cpy_ptr, sizeof(err_in_msg));
		cpy_len += sizeof(err_in_msg);
		msg.len = cpy_len;
		send_to_usr(shmid_elem->fd, &msg);

		/* do something with area that we might not really want
		 * any longer ????? -- might not be able to do it just 
		 * in case we loose track of other partition, but it
		 * isn't really dead ? */
	    }
	    if(shmid_prev) {
		shmid_prev->next = shmid_elem->next;
		check_free(shmid_elem);
		shmid_elem = shmid_prev->next;
	    } else {
		gshmid_list = shmid_elem->next;
		check_free(shmid_elem);
		shmid_elem = gshmid_list;
	    }
	} else {
	    shmid_prev = shmid_elem; 
	    shmid_elem = shmid_elem->next;
	}
    }

    /* free up all the path requests pending */
    for(path_elem = gnode_path, path_prev = NULL; path_elem != NULL;) {
	if((fd > 0 && path_elem->fd == fd) ||
	   (part_num != -1 && part_num == gown_part_id)) {
	    if(path_elem->fd > 0) {
		shmid_in_msg shmid = CLSHMD_ABSENT_SHMID;
		msg.type = DAEM_TO_USR_TAKE_PATH;
		msg.src_part = gown_part_id;
		msg.dst_part = gown_part_id;
		bcopy(&(shmid), msg.body, sizeof(shmid_in_msg));
		msg.len = sizeof(shmid_in_msg);
		send_to_usr(path_elem->fd, &msg);
	    }
	    if(path_prev) {
		path_prev->next = path_elem->next;
		check_free(path_elem);
		path_elem = path_prev->next;
	    } else {
		gnode_path = path_elem->next;
		check_free(path_elem);
		path_elem = gnode_path;
	    }
	} else {
	    path_prev = path_elem; 
	    path_elem = path_elem->next;
	}
    }


    /* free up all the autormid requests pending */
    for(auto_elem = gautormid_list, auto_prev = NULL; auto_elem != NULL;) {
	if((fd > 0 && auto_elem->fd == fd) ||
	   (part_num != -1 && 
	    SHMID_TO_PARTITION(auto_elem->shmid) == part_num)) {
	    if(auto_elem->fd > 0) {
		err_in_msg err = EIO;

		msg.type = DAEM_TO_USR_AUTORMID;
		msg.src_part = SHMID_TO_PARTITION(auto_elem->shmid);
		msg.dst_part = gown_part_id;
		msg.len = sizeof(err_in_msg);
		bcopy(&err, msg.body, sizeof(err_in_msg));
		send_to_usr(auto_elem->fd, &msg);
	    }
	    if(auto_prev) {
		auto_prev->next = auto_elem->next;
		check_free(auto_elem);
		auto_elem = auto_prev->next;
	    } else {
		gautormid_list = auto_elem->next;
		check_free(auto_elem);
		auto_elem = gautormid_list;
	    }
	} else {
	    auto_prev = auto_elem; 
	    auto_elem = auto_elem->next;
	}
    }
}


/* set_local_hostname
 * ------------------
 * Parameters:
 * 	buf:	buffer to put the hostname into 
 *	return:	-1 on failure and 0 on success
 *
 * get the local hostname for the hostname requests for other daemons
 * as well as users.
 */
static int set_local_hostname(char buf[MAX_MSG_LEN])
{
    char *hostname, *end_of_hostname;
    int max_dom_len;

    dprintf(5, ("clshmd: got into set_local_host\n"));
    bzero(buf, MAX_MSG_LEN);
    hostname = buf;
    if(gethostname(hostname, MAX_MSG_LEN)) {
	perror("gethostname");
	return(-1);
    }
    dprintf(5, ("clshmd: after gethostname length = %d\n", 
		strlen(hostname)));
    max_dom_len = MAX_MSG_LEN - (1 + strlen(hostname));
    end_of_hostname = strchr(hostname, '\0');
    *end_of_hostname = '.';
    end_of_hostname++;
    if(getdomainname(end_of_hostname, max_dom_len)) {
	perror("getdomainname");
	return(-1);
    }
    dprintf(5, ("clshmd: after getdomainname length = %d\n", 
		strlen(hostname)));
    return(0);
}


/* insert_free_page_range
 * ----------------------
 * Parameters:
 *	page_off:	offset of page set to insert
 *	num_pages:	number of pages to insert
 *	returns:	page_off of page inserted into (largest contiguous
 *			page range that we create with this insert)
 *
 * Insert the page range into a link list of ranges that include the page
 * offset and the number of pages available.  Keeps the list so that the
 * largest continuous ranges are placed together.
 */
static int insert_free_page_range(int page_off, int num_pages)
{
    clshmd_free_offsets_t *elem, *prev, *new_elem;
    int offset = -1, place_in_front = 0;

    for(prev = NULL, elem = gfree_offsets; elem != NULL; 
	prev = elem, elem = elem->next) {

	if(prev) {
	    if(page_off < elem->page_off) {
		if(prev->page_off + prev->num_pages > page_off ||
		   page_off + num_pages > elem->page_off) {
		    dprintf(1, ("clshmd overlap in insert_free_page_range\n"));

		} else if (prev->page_off + prev->num_pages == page_off &&
			   page_off + num_pages == elem->page_off) {
		    /* fit to connect prev and current elements */
		    offset = prev->page_off;
		    prev->num_pages += num_pages + elem->num_pages;
		    prev->next = elem->next;
		    check_free(elem);

		} else if (prev->page_off + prev->num_pages == page_off) {
		    /* connected to end of previous element */
		    offset = prev->page_off;
		    prev->num_pages += num_pages;

		} else if (page_off + num_pages == elem->page_off) {
		    /* connected to beginning of element */
		    offset = page_off;
		    elem->page_off = page_off;
		    elem->num_pages += num_pages;

		} else {
		    /* somewhere in between prev and current */
		    offset = page_off;
		    new_elem = (clshmd_free_offsets_t *) 
			check_malloc(sizeof(clshmd_free_offsets_t));
		    new_elem->page_off = page_off;
		    new_elem->num_pages = num_pages;
		    prev->next = new_elem;
		    new_elem->next = elem;
		}
		break;
	    } else if(page_off == elem->page_off) {
		dprintf(1, ("clshmd overlap in insert_free_page_range\n"));
		break;
	    }

	} else {
	    if(page_off < elem->page_off) {
		/* this goes right at the beginning of the list */
		if(page_off + num_pages > elem->page_off) {
		    /* overlap */
		    dprintf(1, ("clshmd: overlap in "
				"insert_free_page_range\n"));
		    
		} else if(page_off + num_pages == elem->page_off) {
		    /* prepend to given elem */
		    offset = page_off;
		    elem->page_off = page_off;
		    elem->num_pages += num_pages;

		} else {
		    /* new elem at the front */
		    place_in_front = 1;
		}

		break;
	    }
	}
    }

    /* we are to place a new element at the very beginning of the list */
    if(place_in_front || gfree_offsets == NULL) {
	offset = page_off;
	new_elem = (clshmd_free_offsets_t *) 
	    check_malloc(sizeof(clshmd_free_offsets_t));
	new_elem->page_off = page_off;
	new_elem->num_pages = num_pages;
	new_elem->next = gfree_offsets;
	gfree_offsets = new_elem;
    }

    return(offset);
}


/* get_free_page_range
 * -------------------
 * Parameters:
 *	num_pages:	number of pages we need continuous
 *	returns:	the page offset from 0 - num pages available
 *
 * Given the number of pages we need, find the first range that has
 * the number of pages continuous that we need.  Return the offset.  
 * The offset is somewhere between 0 and the max number of pages we
 * have available.
 */
static int get_free_page_range(int num_pages)
{
    clshmd_free_offsets_t *elem, **last;
    int offset = -1;

    for(last = &(gfree_offsets), elem = *last; elem != NULL; 
	last = &(elem->next), elem = *last) {
	if(elem->num_pages > num_pages) {
	    /* found a chunk larger than what we need */
	    offset = elem->page_off;
	    elem->num_pages -= num_pages;
	    elem->page_off += num_pages;
	    break;
	} else if(elem->num_pages == num_pages) {
	    /* found a chunk exactly the length we need */
	    offset = elem->page_off;
	    (*last) = elem->next;
	    check_free(elem);
	    break;
	}
    }
    return(offset);
}


/* make_new_shmid
 * --------------
 * Parameters:
 * 	fd:	file of user that asked (-1 if no user asked)
 *	key:	key associated with this new shmid
 *	part:	partition that asked for this shmid (-1 for no part asking)
 *	return:	pointer to the new shmid created
 *
 * A new shmid structure is created and placed on the list of shmids that
 * have been asked for but not delivered.  This might be because we are
 * waiting for a remote daemon reply or a driver reply to give us the shmid
 * associated with the key in the shmid structure.
 */
static clshmd_shmid_t * make_new_shmid(int fd, key_in_msg key, 
				       partid_t part_num)
{
    clshmd_shmid_t *new_shmid;

    new_shmid = (clshmd_shmid_t *) check_malloc(sizeof(clshmd_shmid_t));
    new_shmid->fd = fd;
    new_shmid->key = key;
    new_shmid->dst_part = part_num;
    new_shmid->next = gshmid_list;
    gshmid_list = new_shmid;
    return(new_shmid);
}


/* make_new_area
 * -------------
 * Parameters:
 *	key:	key of this new area
 *	len:	length of this new area
 *	return:	return the new area
 *
 * Create a new area with the specified parameters.  If the key is 
 * the absent key, then create a key for the current partition.
 */
static clshmd_shared_area_t *make_new_area(key_in_msg key, len_in_msg len)
{
    int i;
    clshmd_shared_area_t *new_area;

    new_area = (clshmd_shared_area_t *) 
	check_malloc(sizeof(clshmd_shared_area_t));
    if(key == CLSHMD_ABSENT_KEY) {
	new_area->key = MAKE_KEY(gown_part_id, gunique_key_count);
	gunique_key_count++;
	if(gunique_key_count > MAX_KEY) {
	    dprintf(0, ("clshmd: exceeded max key\n"));
	    /* DO SOMETHING HERE ????? */
	    gunique_key_count = 0;
	}
	dprintf(7, ("clshmd: key made locally: 0x%x\n", new_area->key));
    } else {
	new_area->key = key;
	dprintf(7, ("clshmd: key made remotely: 0x%x\n", new_area->key));
    }
    new_area->error = 0;
    new_area->shmid = CLSHMD_ABSENT_SHMID;
    new_area->state = INCOMPLETE_SHARED;
    new_area->len = len;
    new_area->pmo_pid = CLSHMD_ABSENT_PMO_PID;
    new_area->pmo_handle = CLSHMD_ABSENT_PMO_HANDLE;
    new_area->pmo_type = CLSHMD_ABSENT_PMO_TYPE;
    new_area->nodepath = NULL;
    new_area->auto_cleanup = 0;
    for(i = 0; i < MAX_PARTITIONS; i++) {
	new_area->parts_state[i] = 0;
	new_area->state = 0;
    }
    new_area->next = gshared_area;
    gshared_area = new_area;
    return(new_area);
}


/* free_area
 * ---------
 * Parameters:
 *	area:	the area to free
 *	
 * Free the area, amking sure that we set the pages that the area says
 * it owns as free if it is in our own partition.
 */
static void free_area(clshmd_shared_area_t *area)
{
    partid_t owner;
    int page_off, num_pages;

    owner = SHMID_TO_PARTITION(area->shmid);
    if(owner == gown_part_id) {
	page_off = SHMID_TO_OFFSET(area->shmid);
	num_pages = area->num_addrs;
	if(insert_free_page_range(page_off, num_pages) != page_off) {
	    dprintf(5, ("clshmd: didn't remove the correct page offset\n"));
	}
    }
    if(area->nodepath) {
	check_free(area->nodepath);
    }
    check_free(area->kpaddrs);
    check_free(area->msg_recv);
    check_free(area);
}


/* find_new_area
 * -------------
 * Parameters:
 *	key:	key of area to find a new one of
 *	len:	length of new area
 *	return:	the area that has been created for found with the same 
 *		key that was passed in
 *
 * If the key is already in our list, then return that shared area.  Else
 * create a new one with the given length and all else unknown.
 */
static clshmd_shared_area_t *find_new_area(key_in_msg key, len_in_msg len) 
{
    clshmd_shared_area_t *new_area;

    for(new_area = gshared_area; new_area != NULL; 
	new_area = new_area->next) {
	if(key == new_area->key) break;
    }
    
    if(!new_area) {
	if(KEY_TO_PARTITION(key) == gown_part_id) {
	    dprintf(1, ("clshmd: key %x not registered and looking"
			" for it on local partition\n", key));
	    return(NULL);
	}
	new_area = make_new_area(key, len);
    }
    dprintf(7, ("clshmd: find_new_area: key = 0x%x\n", new_area->key));
    return(new_area);
}


/* move_area_to_waiting
 * --------------------
 * Parameters:	
 *	new_area:	area to move to WAITING_SHARED
 *	
 * The area is in the INCOMPLETE_SHARED state, so move it to the 
 * WAITING_SHARED state and allocate all the different arrays for
 * the area 
 */
static void move_area_to_waiting(clshmd_shared_area_t *new_area, int len)
{
    new_area->len = len;

    new_area->state = WAITING_SHARED;
    new_area->num_addrs = (len / gpage_size) + ((len % gpage_size) ? 1 : 0);
    new_area->kpaddrs = (kpaddr_in_msg *) 
	check_malloc((sizeof(kpaddr_in_msg) * new_area->num_addrs));
    bzero(new_area->kpaddrs, (sizeof(kpaddr_in_msg) * new_area->num_addrs));

    new_area->num_msgs = (new_area->num_addrs / ADDRS_PER_MSG) +
	((new_area->num_addrs % ADDRS_PER_MSG) ? 1 : 0);
    new_area->msg_recv = (int *) 
	check_malloc((sizeof(int) * new_area->num_msgs));
    bzero(new_area->msg_recv, (sizeof(int) * new_area->num_msgs));
}


/* put_msg_in_area
 * ---------------
 * Parameters:
 *	msg:	Msg that we received that has info about an area.
 *	return:	The area that we put the message into.
 *
 * Find the area that this message corresponds to that has a list 
 * of pointers in it.  If we find the area and everythign is good
 * then we just place this message in the area.  Else, we return
 * with an error -- NULL
 */
static clshmd_shared_area_t *put_msg_in_area(clshm_msg_t *msg)
{
    __uint32_t cpy_len;
    int i, iaddr;
    char *cpy_ptr;
    key_in_msg key;
    len_in_msg len;
    msgnum_in_msg msg_num;
    clshmd_shared_area_t *area;

    /* get into to find area that we are putting into */
    cpy_len = 0;
    cpy_ptr = msg->body;
    bcopy(cpy_ptr, &key, sizeof(key_in_msg));
    cpy_ptr += sizeof(key_in_msg);
    cpy_len += sizeof(key_in_msg);
    
    bcopy(cpy_ptr, &len, sizeof(len_in_msg));
    cpy_ptr += sizeof(len_in_msg);
    cpy_len += sizeof(len_in_msg);
    
    for(area = gshared_area; area != NULL; area = area->next) {
	dprintf(7, ("clshmd: looking at key 0x%x\n", area->key));
	if(area->key == key) {
	    break;
	}
    }

    if(!area) {
	dprintf(1, ("clshmd got area answer when not asking for it key = "
		    "0x%x\n", key));
	return(NULL);
    }

    /* get info to put info into actual area */
    bcopy(cpy_ptr, &(area->shmid), sizeof(shmid_in_msg)); 
    cpy_len += sizeof(shmid_in_msg);
    cpy_ptr += sizeof(shmid_in_msg);

    /* make sure that everyone will get sent back bad stuff because we 
     * have failed for this shmid */
    if(area->shmid == CLSHMD_ABSENT_SHMID || area->len != len) { 
	dprintf(1, ("clshmd got back bad shmid answer, so sending back "
		    "error\n"));
	if(area->shmid == CLSHMD_ABSENT_SHMID) {
	    bcopy(cpy_ptr, &(area->error), sizeof(err_in_msg));
	    if(!area->error) {
		area->error = EIO;
	    }
	} else {
	    area->shmid = CLSHMD_ABSENT_SHMID;
	    cpy_ptr -= sizeof(shmid_in_msg);
	    bcopy(&(area->shmid), cpy_ptr, sizeof(shmid_in_msg));
	    cpy_ptr += sizeof(shmid_in_msg);
	    area->error = EIO;
	}
	/* set back to INCOMPLETE_SHARED state cause we failed */
	area->num_addrs = 0;
	check_free(area->kpaddrs);
	area->num_msgs = 0;
	check_free(area->msg_recv);
	area->state = INCOMPLETE_SHARED;

	bcopy(&(area->error), cpy_ptr, sizeof(err_in_msg));
	cpy_len += sizeof(err_in_msg);
	msg->len = cpy_len;

	return(area);
    }

    bcopy(cpy_ptr, &msg_num, sizeof(msgnum_in_msg));
    cpy_ptr += sizeof(msgnum_in_msg);
    cpy_len += sizeof(msgnum_in_msg);
    

    if(area->state != WAITING_SHARED ||
       msg_num > area->num_msgs || area->msg_recv[msg_num]) {
	dprintf(1, ("clshmd: bad message number for this area\n"));
	/* just ignor -- some other daemon is all screwed up.  so 
	 * hopefully this is just a dump.  So ignor!!! */
	/* Might want to send off RMIDS or something ??? */
	return(NULL);
    }
    
    /* place the message in the area */
    area->msg_recv[msg_num] = 1;

    for(i = 0, iaddr = msg_num * ADDRS_PER_MSG;
	i < ADDRS_PER_MSG && iaddr < area->num_addrs; i++, iaddr++) {
	bcopy(cpy_ptr, &(area->kpaddrs[iaddr]), sizeof(kpaddr_in_msg));
	cpy_ptr += sizeof(kpaddr_in_msg);
	cpy_len += sizeof(kpaddr_in_msg);
    }
	
    if(cpy_len != msg->len) {
	dprintf(1, ("clshmd: bad len for getting kpaddrs: expected %d, "
		    "got %d\n", msg->len, cpy_len));
	return(NULL);
    }

    area->state = COMPLETE_SHARED;
    for(msg_num = 0; msg_num < area->num_msgs; msg_num++) {
	if(!area->msg_recv[msg_num]) {
	    area->state = WAITING_SHARED;
	    break;
	}
    }
    return(area);
}


/* inform_drv_of_need_shmid
 * ------------------------
 * Parameters:
 *	new_area:	The area that we need to tell the driver that
 *			we want access to
 *	part:		The partition that wants access to this area
 *	return:		-1 for failure and 0 for success
 *
 * This is called when a new partition is asking for a shmid from an
 * area.  If the area is owned by the local driver, we need to tell the
 * driver that a new partition wants to share the area, so we can open
 * up the walls to that partition.  This should be called before, the
 * new area is added to the partition.
 */
static int inform_drv_of_need_shmid(clshmd_shared_area_t *new_area, 
				    partid_t part_num)
{
    clshm_msg_t msg;
    int cpy_len;
    char *cpy_ptr;

    if(SHMID_TO_PARTITION(new_area->shmid) == gown_part_id) {
	/* find if we have already registered this partition for this area */
	if(!(new_area->parts_state[part_num] & AREA_ASKED_FOR)) {
	    /* send a message to the driver */
	    dprintf(5, ("clshmd: we are sending a NEED_SHMID to driver for "
			"partition %d, shmid 0x%x\n", part_num, 
			new_area->shmid));
	    msg.type = DAEM_TO_DRV_NEED_SHMID;
	    cpy_len = 0;
	    cpy_ptr = msg.body;
	    bcopy(&(new_area->key), cpy_ptr, sizeof(key_in_msg));
	    cpy_len += sizeof(key_in_msg);
	    cpy_ptr += sizeof(key_in_msg);
	    bcopy(&(new_area->len), cpy_ptr, sizeof(len_in_msg));
	    cpy_len += sizeof(len_in_msg);
	    cpy_ptr += sizeof(len_in_msg);
	    bcopy(&(new_area->shmid), cpy_ptr, sizeof(shmid_in_msg));
	    cpy_len += sizeof(shmid_in_msg);
	    cpy_ptr += sizeof(shmid_in_msg);
	    bcopy(&(new_area->pmo_pid), cpy_ptr, sizeof(pid_in_msg));
	    cpy_len += sizeof(pid_in_msg);
	    cpy_ptr += sizeof(pid_in_msg);
	    bcopy(&(new_area->pmo_handle), cpy_ptr, sizeof(pmoh_in_msg));
	    cpy_len += sizeof(pmoh_in_msg);
	    cpy_ptr += sizeof(pmoh_in_msg);
	    bcopy(&(new_area->pmo_type), cpy_ptr, sizeof(pmot_in_msg));
	    cpy_len += sizeof(pmot_in_msg);
	    msg.len = cpy_len;
	    msg.src_part = part_num;
	    msg.dst_part = gown_part_id;
	    return(send_to_drv(&msg));
	}
    }
    return(0);
}


/* send_area_to_daem
 * -----------------
 * Parameters:
 *	area:	Area to send to asker
 *	msg: 	msg to send to asker -- asker is msg contents
 *	return:	-1 for failure and 0 for success
 * 
 * The area might have too many pointers for pages to go in one message 
 * in which case multiple messages will be sent.  If we need to send to 
 * another daemon, then the dest_part should be set in the message.  
 */
static int send_area_to_daem(clshmd_shared_area_t *area, clshm_msg_t *msg)
{
    __uint32_t cpy_len, msg_len;
    int iaddr, i;
    msgnum_in_msg msg_num;
    char *cpy_ptr, *msg_ptr;

    /* place all the info into the msg.body */
    msg->type = DAEM_TO_DAEM_TAKE_SHMID;
    msg->src_part = gown_part_id;

    cpy_len = 0;
    cpy_ptr = msg->body;
    bcopy(&(area->key), cpy_ptr, sizeof(key_in_msg));
    cpy_ptr += sizeof(key_in_msg);
    cpy_len += sizeof(key_in_msg);
    
    bcopy(&(area->len), cpy_ptr, sizeof(len_in_msg));
    cpy_ptr += sizeof(len_in_msg);
    cpy_len += sizeof(len_in_msg);
    
    bcopy(&(area->shmid), cpy_ptr, sizeof(shmid_in_msg)); 
    cpy_len += sizeof(shmid_in_msg);
    cpy_ptr += sizeof(shmid_in_msg);

    /* send multiple message for lots of kpaddrs to send */
    iaddr = 0;
    msg_num = 0;
    while(iaddr < area->num_addrs) {
	msg_len = cpy_len;
	msg_ptr = cpy_ptr;
	bcopy(&msg_num, msg_ptr, sizeof(msgnum_in_msg));
	msg_len += sizeof(msgnum_in_msg);
	msg_ptr += sizeof(msgnum_in_msg);

	for(i = 0; i < ADDRS_PER_MSG && iaddr < area->num_addrs; i++) {
	    bcopy(&(area->kpaddrs[iaddr]), msg_ptr, sizeof(kpaddr_in_msg));
	    msg_len += sizeof(kpaddr_in_msg);
	    msg_ptr += sizeof(kpaddr_in_msg);
	    iaddr++;
	}
	msg->len = msg_len;
	if(send_to_daem(msg->dst_part, msg)) {
	    dprintf(1, ("clshmd: couldn't send to daem\n"));
	    /* no need to send RMID since that daemon lost contact
	     * so assume that it is dead. */
	    return(-1);
	} 
	msg_num++;
    }

    return(0);
}


/* send_shmid_to_taker
 * -------------------
 * Parameters:
 * 	msg:		pointer to a message, mostly filled with src, 
 *			dest, and has the key and len already in place.  
 *			This is	something that can be written over in 
 *			this function. -- src and dst will be swapped
 *			before being sent off!!!!!!!!!!
 *	shmid:		shmid to send back
 * 	error:		error to put after the shmid
 *	to_user: 	boolean that says if this is to a user or daem
 *	fd:		file to send to if it is a user
 *
 * Send the shmid back to the person that wants an answer for this shimd. 
 * This is called a lot in error cases, so for the case where we are
 * sending back to the other daemon, the source and destination of the
 * packet will be swapped.  Simply put: this function swaps that src and
 * dst fields of the msg parameter.
 */
static void send_shmid_to_taker(clshm_msg_t *msg, shmid_in_msg shmid, 
				err_in_msg error, int to_user, int fd)
{
    char *cpy_ptr;
    int cpy_len = 0;
    partid_t temp;

    cpy_len = sizeof(key_in_msg) + sizeof(len_in_msg);
    cpy_ptr = msg->body + cpy_len;
    bcopy(&(shmid), cpy_ptr, sizeof(shmid_in_msg));
    cpy_ptr += sizeof(shmid_in_msg);
    cpy_len += sizeof(shmid_in_msg);
    bcopy(&(error), cpy_ptr, sizeof(err_in_msg));
    cpy_len += sizeof(err_in_msg);
    msg->len = cpy_len;
    temp = msg->dst_part;
    msg->dst_part = msg->src_part;
    msg->src_part = temp;

    if(to_user) {
	msg->type = DAEM_TO_USR_TAKE_SHMID;
	if(send_to_usr(fd, msg) < 0) {
	    dprintf(1, ("clshmd: can't send to user TAKE_SHMID\n"));
	}
    } else {
	msg->type = DAEM_TO_DAEM_TAKE_SHMID;
	if(send_to_daem(msg->dst_part, msg)) {
	    dprintf(1, ("clshmd: can't send to remote daem %d\n",
			msg->dst_part));
	}
    }
}


/* send_pmo_response_to_user
 * -------------------------
 * Parameters:
 *	fd:	socket to send message on
 *	msg:	message to fill out and send
 *	err:	error to send back
 *
 * We are sending back the PMO_TAKE response to the user with the 
 * appropriate error code.  
 */
static void send_pmo_response_to_user(int fd, clshm_msg_t *msg, err_in_msg err)
{
    msg->type = DAEM_TO_USR_TAKE_PMO;
    msg->len = sizeof(err_in_msg);
    msg->src_part = gown_part_id;
    msg->dst_part = gown_part_id;
    bcopy(&err, msg->body, sizeof(err_in_msg));
    if(send_to_usr(fd, msg) < 0) {
	dprintf(1, ("clshmd: can't send PMO_TAKE to user fd %d\n", fd));
    }
}


/* remove_local_shmid_area
 * -----------------------
 * Parameters:
 *	shmid:	shmid to remove area of
 *	return:	area that was removed, but not freed
 *
 * Find the area and remove it from the link list, and return it 
 * to the caller
 */
static clshmd_shared_area_t *remove_local_shmid_area(shmid_in_msg shmid)
{
    clshmd_shared_area_t *prev, *elem;

    for(prev = NULL, elem = gshared_area; elem != NULL; 
	prev = elem, elem = elem->next) {
	if(shmid == elem->shmid) {
	    break;
	}
    }

    if(!elem) {
	dprintf(5, ("clshmd: shared area not found for removal for shmid %x\n",
		    shmid));
	return(NULL);
    }

    if(elem->state != COMPLETE_SHARED) {
	dprintf(5, ("clshmd: shared area for shmid %x, removed when not "
		    "COMPLETE_SHARED\n", shmid));
    }

    if(prev) {
	prev->next = elem->next;
    } else {
	gshared_area = elem->next;
    }

    return(elem);
}


/* remove_driver_shmid_area
 * ------------------------
 * Parameters:
 * 	area:	area that is being removed
 *	return:	-1 for failure, 0 for success
 *
 * To remove an area from being alive, send a message to the driver to
 * kill this area as well.
 */
static int remove_driver_shmid_area(clshmd_shared_area_t *area)
{
    clshm_msg_t local_msg;

    local_msg.type = DAEM_TO_DRV_RMID;
    local_msg.src_part = SHMID_TO_PARTITION(area->shmid);
    local_msg.dst_part = gown_part_id;
    local_msg.len = sizeof(shmid_in_msg);
    bcopy(&(area->shmid), local_msg.body, sizeof(shmid_in_msg));
    return(send_to_drv(&local_msg));
}


/* remove_remote_shmid_area
 * ------------------------
 * Parameters:
 *	area:	Area to remove from remote partitions
 *	
 * Send messages to remote partitions that we know need to know that
 * this area has been removed.
 */
static void remove_remote_shmid_area(clshmd_shared_area_t *area)
{
    clshm_msg_t local_msg;
    int i;

    local_msg.type = DAEM_TO_DAEM_RMID;
    local_msg.src_part = gown_part_id;
    local_msg.len = sizeof(shmid_in_msg);
    bcopy(&(area->shmid), local_msg.body, sizeof(shmid_in_msg));
    for(i = 0; i < MAX_PARTITIONS; i++) {
	if(area->parts_state[i] & AREA_ASKED_FOR && i != gown_part_id) {
	    local_msg.dst_part = i;
	    if(send_to_daem(i, &local_msg)) {
		dprintf(5, ("clshmd: can't send to partition %d\n", i));
	    }
	}
    }
}


/* auto_remove_shmid_area
 * ----------------------
 * Parameters:
 *	area:	area to check if we need to remove
 *	return:	1 means it was removed and 0 means it wasn't
 *
 * Check to see if the area is ready to be auto cleaned up at this
 * point.  And if it is, remove it.
 */
static int auto_remove_shmid_area(clshmd_shared_area_t *area)
{
    int i, ret = 0;
    shmid_in_msg shmid;

    shmid = area->shmid;
    if((SHMID_TO_PARTITION(shmid) == gown_part_id) &&
       (area->auto_cleanup & AUTO_CLEANUP_ON) && 
       (area->auto_cleanup & FIRST_ATTACH_SEEN)) {
	for(i = 0; i < MAX_PARTITIONS; i++) {
	    if(area->parts_state[i] == (AREA_ASKED_FOR|AREA_ATTACHED)) {
		break;
	    }
	}
	
	/* we should clean it up now */
	if(i == MAX_PARTITIONS) {
	    area = remove_local_shmid_area(shmid);
	    if(!area) {
		dprintf(5, ("clshmd: shmid %x AUTORMID and not found\n", 
			    shmid));
		return(ret);
	    }
	    if(remove_driver_shmid_area(area) < 0) {
		dprintf(5, ("clshmd: failed in ioctl to AUTORMID\n"));
	    }
	    remove_remote_shmid_area(area);
	    free_area(area);
	    ret = 1;
	}
    }
    return(ret);
}



/* MAIN !!!! */
int main(int argc, char *argv[])
{
    int			i, j, error, running;
    struct  timeval 	tv;
    char		devname[32];
    clshm_config_t	cfg;
    volatile __uint32_t	*rcv_head, *rcv_tail, *max_recv;
    uint		min_pages;
    uint		max_msgs;
    clshm_msg_t		msg;
    fd_set		fd_r_var, fd_w_var, fd_x_var;
    int			num_rdy_fds;
    char		*moving_hand;
    int			num_regions;
    char		r_fd_set, w_fd_set, x_fd_set;
    int			fd;
    pgsz_in_msg 	pgsz;
    vers_in_msg 	vers_major, vers_minor;
    rpart_info_t	*part;

#ifdef PARTMAP_FIXES_THIS
    int			daem_loops = 0;
#endif

    if(argc != 2)  {
	fprintf(stderr, "clshmd: too few args; need 2\n");
	exit(1);
    }

    /* get parameters */
    gunit = (uint) atoi(argv[1]);
    dprintf(5, ("clshmd: unit %d\n", gunit));

    /* if we try to write to a socket that is closed, really want to 
     * ignor that since we don't really trust our buddy users to not 
     * just kill their pipe at any time. */
    signal(SIGPIPE, SIG_IGN);

    /* need space for:
     * head/tail/max 
     * MAX_OUTSTAND_MSGS messages 
     * for each (recv/send) region
     */
    
    /* open the control device that we are going to map */
    num_regions = 1;

    sprintf(devname, "%s%d/ctl", CLSHMCTL_PATH, gunit);

    gdrv_fd = open(devname, O_RDWR | O_EXCL);
    if(gdrv_fd < 2)  {
	fprintf(stderr, "clshmd: couldn't open %s\n", devname);
	perror("clshmd");
	fprintf(stderr, "clshmd: clshm may not be turned on or may not be "
		"configured correctly\n");
	exit(1);
    }
    
    if(ioctl(gdrv_fd, CL_SHM_GET_CFG, &cfg) < 0)  {
        perror("clshmd: couldn't get current configuration");
        exit(1);
    }
    
    gpage_size = cfg.page_size;
    gshared_pages = cfg.max_pages;
    gdrv_minor_vers = cfg.minor_version;
    gnum_pages = cfg.clshmd_pages;
    gtimeout = cfg.clshmd_timeout;

    if(cfg.major_version != CLSHM_DAEM_MAJOR_VERS) {
	fprintf(stderr, "clshmd: incompat major version of driver %d, "
		"daemon %d\n", cfg.major_version, CLSHM_DAEM_MAJOR_VERS);
	exit(1);
    }

    /* find out the min number of pages we need to map between us and
     * the driver */
    min_pages = num_regions * ((MAX_OUTSTAND_MSGS * sizeof(clshm_msg_t)
				+ OHEAD_UINTS * sizeof(__uint32_t) + 
				(gpage_size-1))/gpage_size);
    if(gnum_pages < min_pages)  {
	fprintf(stderr, "clshmd: started up with too few pages to "
		"communicate with driver; need at least %d & have %d, "
		"this can be set with clshmctl\n", 
		min_pages, gnum_pages);
	exit(1);
    }

    /* insert the number of free pages that we have access to map
     * shared memory across partitions */
    if(insert_free_page_range(0, gshared_pages) < 0) {
	fprintf(stderr, "clshmd: can't insert the free pages into a free "
		"list\n");
	exit(1);
    }

    dprintf(5, ("clshmd: Going to mmap, fd is %d, need %d bytes \n", 
		gdrv_fd, gpage_size * gnum_pages));

    /* parameters are ok; mmap some space */
    gmapped_region = mmap(NULL, 
			  gpage_size * gnum_pages, 
			  PROT_READ | PROT_WRITE, MAP_SHARED, gdrv_fd, 0);
    if((void *) gmapped_region == MAP_FAILED)  {
	perror("clshmd mmap");
	exit(1);
    }

    dprintf(5, ("mapped_region set up, addr 0x%x\n", gmapped_region));

    moving_hand = gmapped_region;
    bzero(moving_hand, gpage_size * gnum_pages);

    /* OHEAD_UINTS and 2 more ptrs gone from each region */
    max_msgs = (gpage_size * (gnum_pages/num_regions)
		- OHEAD_UINTS * sizeof(__uint32_t))
	/ sizeof(clshm_msg_t);

    /* set up the moving hands for the message passing from the driver */
    rcv_head = (__uint32_t *) moving_hand;
    moving_hand += sizeof(rcv_head);
    rcv_tail = (__uint32_t *) moving_hand;
    moving_hand += sizeof(rcv_tail);
    max_recv = (__uint32_t *) moving_hand;
    moving_hand += sizeof(max_recv);
    moving_hand += sizeof(__uint32_t);
    /* msg area */
    moving_hand += max_msgs * sizeof(clshm_msg_t);
    *max_recv = max_msgs;
    *rcv_head = *rcv_tail = 0;

    /* set the timeout structure */
    tv.tv_sec = (long) gtimeout/1000;	/* gtimeout is in msec */
    tv.tv_usec = (long) (gtimeout % 1000) * 1000;

    dprintf(10, ("clshmd: Timeout: %d secs, %d usecs\n", 
		 tv.tv_sec, tv.tv_usec));

    /* init all partition info that we have access to */
    for(i = 0; i < MAX_PARTITIONS; i++)   {
	gremote_parts[i].part_num = i;
	gremote_parts[i].daem_fd = -1;
	gremote_parts[i].connect_state = CLSHMD_UNCONNECTED;
    }

    gown_part_id = (partid_t) 
	syssgi(SGI_PART_OPERATIONS, SYSSGI_PARTOP_GETPARTID);

    dprintf(5, ("clshmd: OK, own part is %d\n", gown_part_id));

    /* find new partitions and init all the sockets */
    find_new_parts();
    if(init_client_socket() < 0) {
	dprintf(1, ("clshmd: can't open socket to user land\n"));
	exit(1);
    }

    /* tell the driver we are ready to go */
    vers_major = CLSHM_DAEM_MAJOR_VERS;
    vers_minor = CLSHM_DAEM_MINOR_VERS;
    pgsz = gpage_size;
    msg.type = DAEM_TO_DRV_NEW_PART;
    msg.src_part = gown_part_id;
    msg.dst_part = gown_part_id;
    bcopy(&vers_major, msg.body, sizeof(vers_in_msg));
    msg.len = sizeof(vers_in_msg);
    bcopy(&vers_minor, msg.body + msg.len, sizeof(vers_in_msg));
    msg.len += sizeof(vers_in_msg);
    bcopy(&pgsz, msg.body + msg.len, sizeof(pgsz_in_msg));
    msg.len += sizeof(pgsz_in_msg);
    if(send_to_drv(&msg) < 0) {
	fprintf(stderr, "clshmd: couldn't contact driver, check versions "
		"of driver and daemon\n");
	exit(1);
    }


    /* be the daemon */
    dprintf(0, ("clshmd: becoming daemon\n"));
    running = 1;
    while(running)  {
	/* is there local work? */
	if(*rcv_head != *rcv_tail)  {
	    /* we've got work from the kernel; go, do it! */
	    dprintf(10, ("clshmd: Got work from kernel, head %d tail %d\n",
			 *rcv_head, *rcv_tail));
	    error = recv_from_drv(&msg);
	    if(error)  {
		dprintf(1, ("clshmd: error in receiving message from "
			    "driver; currrent msg\n"));
	    }
	    else {
		dprintf(15, ("clshmd: Got a healthy message; will process\n"));
		if(process_driver_msg(&msg) != 0) {
		    running = 0;
		    continue;
		}
	    }
	}
    

	/* set the daem_fd bits first -- for the parts we are
	 * interested in as well as the user sockets and the connection
	 * socket for new users to connect */
	FD_ZERO(&fd_r_var);
	FD_ZERO(&fd_w_var);
	FD_ZERO(&fd_x_var);

        for(i = 0; i < MAX_PARTITIONS || i < CLSHMD_SOCKET_MAX; i++)   {
	    if(i < MAX_PARTITIONS && 
	       (gremote_parts[i].connect_state & CLSHMD_DRV_CONNECTED) &&
	       gremote_parts[i].daem_fd != -1)   {
		/* if we have opened admin devs to this partition */
	        FD_SET(gremote_parts[i].daem_fd, &fd_r_var);
	        FD_SET(gremote_parts[i].daem_fd, &fd_w_var);
	        FD_SET(gremote_parts[i].daem_fd, &fd_x_var);
	    }
	    if(i < CLSHMD_SOCKET_MAX && guser_fds[i] != -1) {
		FD_SET(guser_fds[i], &fd_r_var);
		FD_SET(guser_fds[i], &fd_w_var);
		FD_SET(guser_fds[i], &fd_x_var);
	    }
	}

	FD_SET(gClientSock, &fd_r_var);
	FD_SET(gClientSock, &fd_w_var);
	FD_SET(gClientSock, &fd_x_var);

        /* 
	 * check all the fds to see if there's any part ready,
	 * or any message to be read
	 */
	num_rdy_fds  = select(getdtablehi(), &fd_r_var, &fd_w_var, 
			      &fd_x_var, &tv);


	/* loop through all the user sockets as well as the partition
	 * daemon sockets */
	for(i = 0; num_rdy_fds > 0 && 
		(i < MAX_PARTITIONS || i < CLSHMD_SOCKET_MAX); i++)   {
	    dprintf(20, ("clshmd: watch_connect: looking at index %d\n", i));

	    r_fd_set = w_fd_set = x_fd_set = 0;

	    /* check other partition daems for activitiy */
	    if(i < MAX_PARTITIONS) {
		part = &(gremote_parts[i]);

		if((part->connect_state & CLSHMD_DRV_CONNECTED) && 
		   part->daem_fd != -1) {

		    if(FD_ISSET(part->daem_fd, &fd_w_var)) {
			w_fd_set = 1;
			if(part->daem_fd != -1 && 
			   !(part->connect_state & CLSHMD_PIPE_CONNECTED)) {
			    /* this is a new connection */
			    part->connect_state |= CLSHMD_PIPE_CONNECTED;
    
			    if(FD_ISSET(part->daem_fd, &fd_x_var))  {
				FD_CLR(part->daem_fd, &fd_x_var);
			    }
			    dprintf(1, ("clshmd: i_am_coming_up() called for "
					"part %d: flags 0x%x\n", 
					i, part->connect_state));
			    i_am_coming_up(i);
			}
			FD_CLR(part->daem_fd, &fd_w_var);
		    }	/* if write fd was set */

		    if(FD_ISSET(part->daem_fd, &fd_x_var)) {
			x_fd_set = 1;
			FD_CLR(part->daem_fd, &fd_x_var);
			/* don't read from this socket either! */
			FD_CLR(part->daem_fd, &fd_r_var);
			dprintf(1, ("Exceptions on part %d; socket closed\n", 
				    i));
			clean_up_dead_part(i);
		    }

		    if(FD_ISSET(part->daem_fd, &fd_r_var))  {
			r_fd_set = 1;
			if(recv_from_daem(part->daem_fd, &msg))  {
			    dprintf(1, ("Error while receiving message from "
					"part %d; ignoring the message\n", 
					i));
			} else  {
			    dprintf(3, ("clshmd: received a msg from part "
					"%d\n", i));
			    process_daem_msg(&msg);
			}
			FD_CLR(part->daem_fd, &fd_r_var);
		    }
		}
	    }
	    if(r_fd_set || w_fd_set || x_fd_set)  {
		num_rdy_fds--;
	    }
	    
	    r_fd_set = w_fd_set = x_fd_set = 0;
	    
	    /* check user file desc for activity */
	    if(i < CLSHMD_SOCKET_MAX && guser_fds[i] != -1) {
		if(FD_ISSET(guser_fds[i], &fd_w_var)) {
		    w_fd_set = 1;
		    FD_CLR(guser_fds[i], &fd_w_var);
		    /* should just mean that it's ok to write: ignor */
		}
		if(FD_ISSET(guser_fds[i], &fd_x_var)) {
		    x_fd_set = 1;
		    FD_CLR(guser_fds[i], &fd_x_var);
		    close(guser_fds[i]);
		    dprintf(1, ("clshmd: we got to the except for fd %d\n", 
				guser_fds[i]));
		    guser_fds[i] = -1;
		    /* shouldn't really be here */
		}
		if(FD_ISSET(guser_fds[i], &fd_r_var)) {
		    r_fd_set = 1;
		    FD_CLR(guser_fds[i], &fd_r_var);
		    if((fd = recv_from_usr(guser_fds[i], &msg)) 
		       != guser_fds[i]) {
			if(fd == CLSHMD_SOCKET_CLOSED) {	
			    dprintf(3, ("clshmd: user land socket closed\n"));
			} else {
			    dprintf(1, ("clshmd: Error while receiving "
					"message from usr land; closing user "
					"socket\n"));
			}
			delete_unfinished_business(guser_fds[i], -1);
			close(guser_fds[i]);
			guser_fds[i] = -1;
		    } else {
			dprintf(3, ("clshmd: received a msg from usrland\n"));
			if(process_usr_msg(guser_fds[i], &msg) < 0) {
			    dprintf(1, ("clshmd: user died in process msg, so "
					"kill the user\n"));
			    close(guser_fds[i]);
			    guser_fds[i] = -1;
			}
		    }   
		}
	    
		if(r_fd_set || w_fd_set || x_fd_set)  {
		    num_rdy_fds--;
		}
	    }
	} /* for loop through daemon and user sockets */

	r_fd_set = w_fd_set = x_fd_set = 0;

	/* check client socket for new connects */
	if(num_rdy_fds > 0) {
	    if(FD_ISSET(gClientSock, &fd_w_var)) {
		w_fd_set = 1;
		FD_CLR(gClientSock, &fd_w_var);
		dprintf(1, ("clshmd: shouldn't really be here\n"));
	    }
	    if(FD_ISSET(gClientSock, &fd_x_var))  {
		x_fd_set = 1;
		FD_CLR(gClientSock, &fd_x_var);
		dprintf(1, ("clshmd: user connect socket got except: "
			    "dying\n"));
		running = 0;
		continue;
	    }
	    if(FD_ISSET(gClientSock, &fd_r_var))  {
		r_fd_set = 1;
		dprintf(1, ("clshmd: will read from client socket\n"));

		if((fd = accept(gClientSock, 0, 0)) < 0) {
		    dprintf(5, ("clshmd: can't accept from socket\n"));
		} else if(fcntl(fd, F_SETFL, FNDELAY) < 0) {
		    dprintf(1, ("clshmd: can't set connection to user as "
				"non-blocking\n"));
		} else {
		    dprintf(3, ("clshmd: new user connection\n"));
		    for(j = 0; j < CLSHMD_SOCKET_MAX; j++) {
			if(guser_fds[j] == -1) break;
		    }
		    if(j == CLSHMD_SOCKET_MAX) {
			dprintf(1, ("clshmd: can we get this many open?\n"));
		    } else {
			guser_fds[j] = fd;
		    }
	        }
	        FD_CLR(gClientSock, &fd_r_var);
 	    }

	    if(r_fd_set || w_fd_set || x_fd_set)  {
		num_rdy_fds--;
	    }
	}

	/* if we didn't read up on all the file descriptors, why!!! */
	if(num_rdy_fds != 0) {
	    char *fd_w_ptr, *fd_r_ptr, *fd_x_ptr;

	    dprintf(1, ("clshmd: num_rdy_fds = %d\n", num_rdy_fds));
	    fd_r_ptr = (char *) &(fd_r_var);
	    fd_w_ptr = (char *) &(fd_w_var);
	    fd_x_ptr = (char *) &(fd_x_var);
	    for(i = 0; i < sizeof(fd_set); i++) {
		dprintf(5, ("clshmd: byte %d: fd_r_var == Ox%x\n", i, 
			    *fd_r_ptr));
		dprintf(5, ("clshmd: byte %d: fd_x_var == Ox%x\n", i, 
			    *fd_x_ptr));
		dprintf(5, ("clshmd: byte %d: fd_w_var == Ox%x\n", i, 
			    *fd_w_ptr));
		fd_r_ptr++; fd_w_ptr++; fd_x_ptr++;
	    }

	}


#ifdef PARTMAP_FIXES_THIS
	/* FIX THIS IN PARTITION CODE */
	if(daem_loops < MAX_DISCOVERY_TRIES) {
	    find_new_parts();
	    daem_loops++;
	} else {
#endif
	    try_unconnected_parts();
#ifdef PARTMAP_FIXES_THIS
	}
#endif

    }
    /* always return error since should never really exit */
    return(1);
}

