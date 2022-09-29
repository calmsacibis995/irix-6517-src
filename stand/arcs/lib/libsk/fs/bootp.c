#ident	"lib/libsk/fs/bootp.c:  $Revision: 1.88 $"

/*
 * BOOTP/TFTP standalone bootstrap.
 */

#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/pvector.h>
#include <arcs/signal.h>

#include <sys/param.h>
#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <saio.h>
#include <saioctl.h>
#include <stringlist.h>
#include <setjmp.h>
#include <tpd.h>
typedef	struct in_addr iaddr_t;
#include <protocols/bootp.h>
#include <arpa/tftp.h>
#include <libsc.h>
#include <libsk.h>

#define cei(x)	((struct ether_info *)(x->IO->DevPtr))
#define	cei_if		ei_acp->ac_if
#define	cei_enaddr	ei_acp->ac_enaddr


extern struct string_list environ_str;

# define BOOTPDEBUG 1

#ifdef BOOTPDEBUG
#define dprintf(x)	if (Debug) printf x
#define Dprintf(x,n)	if (Debug>n) printf x
#else  /* !BOOTPDEBUG */
#define dprintf(x)
#define Dprintf(n,x)
#endif /* !BOOTPDEBUG */

/*
 * The BOOTP timeout can be relatively short, since it only does
 * a limited amount of IO.  The TFTP timeout needs to be long,
 * since it may be doing slow things like rewinding cartridge
 * tapes.
 */
#define BOOTP_TIMEOUT	5	/* 5 seconds */
#define TFTP_TIMEOUT	30	/* 30 seconds */
# define BOOTP_TRIES	8	/* 40 seconds */
#define TFTP_TRIES	4

/*
 * Private information maintained by BOOTP/TFTP
 */
int bootp_xid;			/* BOOTP transaction ID */

/* TFTP connection state */

struct tftp_state {
	int ts_blkno;		/* Last ACKed block for current TFTP conn */
	int ts_eof;		/* Final block has been recvd from TFTP */
	char *ts_data;		/* Pointer to TFTP data buffer */
	int ts_offset;		/* offset from ts_data to first unread byte */
	int ts_count;		/* Count of unread bytes in TFTP buffer */
	u_short ts_srvport;	/* Server UDP port number for TFTP */
	u_short ts_myport;	/* Client UDP port number for TFTP */
	struct tp_entry ts_ent;	/* tpd entry (if TPD format file) */
} tftp_st[MAX_IFCS];

struct tftpdmsg {
	struct	tftphdr th;		/* Header */
	char	data[SEGSIZE-1];	/* TFTP SEGSIZE = 512 */
};

int bootp_install(void);
int bootp_strat(FSBLOCK *);

static int _bootp_open(FSBLOCK *);
static int _bootp_close(FSBLOCK *);
static int _bootp_read(FSBLOCK *);
static int _bootp_getrs(FSBLOCK *);

static void _bootpinit(void);
static void mk_bootp(FSBLOCK *, struct bootp *, char *, char *, u_long);
static int bootp_transaction(FSBLOCK *, struct bootp *, struct bootp *);
static int bootp_ok(FSBLOCK *, struct bootp *, int);
static int tftpconnect(FSBLOCK *);
static int tftpread(FSBLOCK *);
static int tftprecv(FSBLOCK *);
static int tftp_ok(FSBLOCK *, struct tftpdmsg *, int);
static void tftpack(FSBLOCK *);
static void tftpabort(FSBLOCK *);
static int get_rand(int, int);
static int _bootp_getfileinfo(FSBLOCK *);

/*
 * bootp_install - register this filesystem to the standalone kernel
 * and initialize any variables
 */
int
bootp_install(void)
{
    _bootpinit();
    return fs_register (bootp_strat, "tftp", DTFS_BOOTP);
}

/*
 * bootp_strat - 
 */
int
bootp_strat(FSBLOCK *fsb)
{
    /* don't support 64 bit offsets (yet?) */
    if (fsb->IO->Offset.hi)
	return fsb->IO->ErrorNumber = EINVAL;

    switch (fsb->FunctionCode) {
    case FS_CHECK:
	return (fsb->Device->Type == NetworkPeripheral) ? ESUCCESS : EINVAL;
    case FS_OPEN:
	return _bootp_open(fsb);
    case FS_CLOSE:
	return _bootp_close(fsb);
    case FS_READ:
	return _bootp_read(fsb);
    case FS_WRITE:
	return fsb->IO->ErrorNumber = EROFS;
    case FS_GETDIRENT:
	return fsb->IO->ErrorNumber = EBADF;
    case FS_GETREADSTATUS:
	return(_bootp_getrs(fsb));
    case FS_GETFILEINFO:
	return _bootp_getfileinfo(fsb);
    default:
	return fsb->IO->ErrorNumber = ENXIO;
    }
}

/*
 * _bootpinit
 */
static void
_bootpinit(void)
{
	register struct tftp_state *tsp;

	bzero((char *)tftp_st, sizeof (tftp_st));
	for (tsp = tftp_st; tsp < &tftp_st[MAX_IFCS]; tsp++)
		tsp->ts_blkno = -1;
}

extern char netaddr_default[];

/*
 * _bootp_open(fsb) -- find an available server for "file"
 */
static int
_bootp_open(FSBLOCK *fsb)
{
	int flags = fsb->IO->Flags;
	char *filename;
	register char *host;	/* name of server */
	register char *file;	/* file name to request from server */
	char *member;		/* name within file for TPD files */
	register char *cp;
	char buf[512];
	struct tftp_state *tsp;
	struct in_addr myiaddr;
	struct bootp xbp;	/* transmit packet */
	struct bootp rbp;	/* receive packet */
	int sverrno = 0;	/* errno value to be returned */

	filename = (char *) fsb->Filename;
	fsb->Buffer = NULL;

	dprintf(("bootpopen: ctlr %d, file %s\n", fsb->IO->Controller, filename));
	
	if (fsb->IO->Controller >= MAX_IFCS) {
		printf("controller number too large: %d\n",
		    fsb->IO->Controller);
		goto errout;
	}

 	tsp = &tftp_st[fsb->IO->Controller];
	tsp->ts_blkno = -1;
	tsp->ts_ent.te_lbn = 0;

	if (flags & F_WRITE) {
		sverrno = EROFS;
		goto errout;
	}
	if (flags & F_DIR) {
		sverrno = EINVAL;
		goto errout;
	}

	ASSERT(filename != NULL);
	if (strlen(filename) > sizeof buf - 2) {
		sverrno = ENAMETOOLONG;
		goto errout;
	}
	strcpy(buf, filename);

	/*
	 * Parse file name to separate host.  If the user wants a pathname
	 * that has a ":" in it, but no host name, then prefix the
	 * name with just ":" which has the desired effect.
	 */
	if ((file = index(buf, ':')) == NULL) {
		host = NULL;
		file = buf;
	} else {
		host = buf;
		*file++ = '\0';
	}

	/*
	 * Now check for a member name within the file name
	 */
#define LEFT_PAREN	0x28	/* so vi can match parens */
#define RIGHT_PAREN	0x29

	if ((member = index(file, LEFT_PAREN)) != NULL) {
		if ((cp = index(member, RIGHT_PAREN)) == NULL) {
			printf("Unmatched parenthesis in file name\n");
			goto errout;
		}
		*member++ = '\0';
		*cp = '\0';
	}

	if (host != NULL && strlen(host) >= sizeof (xbp.bp_sname)) {
		printf("host name too long: %s\n", host);
		goto errout;
	}

	if (strlen(file) >= sizeof (xbp.bp_file)) {
		printf("file name too long: %s\n", file);
		goto errout;
	}

	dprintf(("bootpopen: host %s, file %s %s %s\n",
		host?host:"<NULL>", file,
		member ? ", member " : "",
		member ? member : ""));
	
	/*
	 * Decide whether it is necessary to invoke BOOTP.
	 * BOOTP is not required if all the following conditions
	 * are met:
	 *
	 * 1) the internet address of this system is already known
	 * 2) the internet addresses of the server and gateway
	 *    through which to boot are already known
	 */
	myiaddr.s_addr = 0;
	if ((cp = getenv("netaddr")) != NULL && *cp != '\0') {
		struct in_addr iaddr;

		/*
		 * If Internet address is set in the environment, then
		 * configure the interface for that IP address.
		 */
		myiaddr = inet_addr(cp);
		if (!strcmp(cp, netaddr_default)) {
			printf("%s%s%s",
"Warning: 'netaddr' is set to the default address ", netaddr_default,
".\nUse 'setenv' to reset it to an Internet address on your network.\n");
		}
		if (ntohl(myiaddr.s_addr) == (unsigned)-1) {
			printf("$netaddr is not a valid Internet address\n");
			sverrno = EADDRNOTAVAIL;
			goto errout;
		}
		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCSIFADDR;
		fsb->IO->IoctlArg = (IOCTL_ARG)&myiaddr;
		if (fsb->DeviceStrategy (fsb->Device, fsb->IO) < 0) {
			printf("Unable to set interface address\n");
			goto errout;
		}

		if ((cp = getenv("srvaddr")) == NULL || *cp == '\0') {
			dprintf(("$srvaddr not set\n"));
			goto do_bootp;
		}
		iaddr = inet_addr(cp);
		if (ntohl(iaddr.s_addr) == (unsigned)-1) {
			dprintf(("$srvaddr not valid\n"));
			goto do_bootp;
		}

		cei(fsb)->ei_dstaddr.sin_family = AF_INET;
		cei(fsb)->ei_dstaddr.sin_addr.s_addr = iaddr.s_addr;
		cei(fsb)->ei_dstaddr.sin_port = 0;
		cei(fsb)->ei_gateaddr = cei(fsb)->ei_dstaddr;
		
		if ((cp = getenv("gateaddr")) != NULL && *cp != '\0') {
			iaddr = inet_addr(cp);
			if (ntohl(iaddr.s_addr) == (unsigned)-1) {
				dprintf(("$gateaddr not valid\n"));
				goto do_bootp;
			}
			cei(fsb)->ei_gateaddr.sin_addr.s_addr = iaddr.s_addr;
		}

		dprintf(("skipping BOOTP ...\n"));
		strcpy(cei(fsb)->ei_filename, file);
		strcpy((char*)rbp.bp_sname, getenv("srvaddr"));
		goto after_bootp;
	}

do_bootp:
	cei(fsb)->ei_dstaddr.sin_family = AF_INET;
	cei(fsb)->ei_dstaddr.sin_addr.s_addr = INADDR_BROADCAST;
	cei(fsb)->ei_dstaddr.sin_port = htons((u_short)IPPORT_BOOTPS);
	cei(fsb)->ei_gateaddr = cei(fsb)->ei_dstaddr;

	fsb->IO->FunctionCode = FC_IOCTL;
	fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCBIND;
	fsb->IO->IoctlArg = (IOCTL_ARG)htons((u_short)IPPORT_BOOTPC);
	if (fsb->DeviceStrategy (fsb->Device, fsb->IO) < 0) {
		printf("bootp bind failed\n");
		goto errout;
	}

	/*
	 * Format the BOOTP packet
	 */
	mk_bootp(fsb, &xbp, host, file, myiaddr.s_addr);

	/*
	 * If the IP address of this system is not yet known, then
	 * we must put the interface in promiscuous receive mode
	 * so that it won't drop the BOOTP response message.
	 */
	if (xbp.bp_ciaddr.s_addr == 0)
		cei(fsb)->cei_if.if_flags |= IFF_PROMISC;

	dprintf(("send BOOTP request: ciaddr %s, chaddr %s\n",
		inet_ntoa(xbp.bp_ciaddr), ether_sprintf(xbp.bp_chaddr)));

	if (bootp_transaction(fsb, &xbp, &rbp) < 0) {
		if (Verbose) {
			printf("No server for %s.  \nYour netaddr ", filename);
			printf("environment variable may be set ");
		        printf("incorrectly, or\nthe net may be too ");
		        printf("busy for a connection to be made.\n");
		}
		sverrno = ENOCONNECT;
		if (cei(fsb)->ei_registry > 0)
			_free_socket(cei(fsb)->ei_registry);
		goto errout;
	}

	dprintf(("BOOTP reply from %s(%s): file %s, ",
		rbp.bp_sname, inet_ntoa(rbp.bp_siaddr), rbp.bp_file));
	dprintf(("myiaddr %s, ", inet_ntoa(rbp.bp_yiaddr)));
	dprintf(("giaddr %s\n", inet_ntoa(rbp.bp_giaddr)));

	/*
	 * If the bp_yiaddr field is not zero, then we don't yet
	 * know our own IP address.  Configure the interface with
	 * the new address.
	 */
	if (rbp.bp_yiaddr.s_addr != 0) {
		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCSIFADDR;
		fsb->IO->IoctlArg = (IOCTL_ARG)&rbp.bp_yiaddr;
		if (fsb->DeviceStrategy (fsb->Device, fsb->IO) < 0) {
			printf("Unable to set interface address\n");
			goto errout;
		}
		printf("Setting $netaddr to %s (from server %s)\n",
			inet_ntoa(rbp.bp_yiaddr), rbp.bp_sname);

		SetEnvironmentVariable((CHAR *)"netaddr",
				  (CHAR *)inet_ntoa(rbp.bp_yiaddr));

		/*
		 * We can turn off promiscuous mode now, since we
		 * know who we are.
		 */
		cei(fsb)->cei_if.if_flags &= ~IFF_PROMISC;
	}
        /*
         * Snarf out server and gateway IP addr into the environment.  (So the
         * kernel has these for things like "diskless==2": NFS-mountable
         * s/w installation tools).
         */
        if (rbp.bp_siaddr.s_addr != 0) {
                replace_str("dlserver", inet_ntoa(rbp.bp_siaddr), &environ_str);                dprintf(("dlserver = %s\n", inet_ntoa(rbp.bp_siaddr)));
        }

        if (rbp.bp_giaddr.s_addr != 0) {
		replace_str("dlgate", inet_ntoa(rbp.bp_giaddr), &environ_str);
		dprintf(("dlgate = %s\n", inet_ntoa(rbp.bp_giaddr)));
        }

	/*
	 * Get the server and gateway IP addresses from the message
	 */
	cei(fsb)->ei_dstaddr.sin_family = AF_INET;
	cei(fsb)->ei_dstaddr.sin_addr.s_addr = rbp.bp_siaddr.s_addr;
	cei(fsb)->ei_dstaddr.sin_port = 0;
	cei(fsb)->ei_gateaddr = cei(fsb)->ei_dstaddr;
	if (rbp.bp_giaddr.s_addr != 0)
		cei(fsb)->ei_gateaddr.sin_addr.s_addr = rbp.bp_giaddr.s_addr;
	
	/*
	 * The file name may be NULL, meaning that we requested a
	 * particular server and he didn't have the file.
	 */
	if (strlen((char*)rbp.bp_file) == 0) {
		sverrno = ENOENT;
		if (Verbose)
			printf("File %s not found on server %s\n", file, host);
		goto errout;
	}

	/*
	 * Store the real file name as returned by the server
	 */
	strcpy(cei(fsb)->ei_filename, (char*)rbp.bp_file);

	/*
	 * Store vendor information somewhere that the OS can find it XXX
	 */

after_bootp:
	/*
	 * Call the TFTP connect routine to open the file.
	 */
	if (tftpconnect(fsb) < 0) {
		dprintf(("bootpopen: tftp connect fails\n"));
		goto errout;
	}

	if ( Verbose && member == NULL )
		printf("Obtaining %s from server %s\n", file, rbp.bp_sname);

	/*
	 * No member name given, so we are done.
	 */
	if (member == NULL) {
		ASSERT(tsp->ts_ent.te_lbn == 0);
		return ESUCCESS;
	}

	/*
	 * A member name was specified, check the first block
	 * for a TPD format directory.  If the file is not
	 * in TPD format, then complain and return an error.
	 */
	dprintf(("after connect: tftp count %d, offset %d\n",
		tsp->ts_count, tsp->ts_offset));
	if (!is_tpd((struct tp_dir*)(tsp->ts_data + tsp->ts_offset))) {
		printf("Remote file %s%s%s format not valid: %s\n",
			host ? host : "", host ? ":" : "", file,
			"first block is not a tape directory"); 
		goto errout;
	}

	/*
	 * Search for the requested member in the directory
	 */
	if (!tpd_match(member,
		(struct tp_dir *)(tsp->ts_data + tsp->ts_offset),
		&tsp->ts_ent)) {

		printf("File %s not found in %s%s%s, directory contains:\n",
			member, host ? host : "", host ? ":" : "", file);
		tpd_list((struct tp_dir *)(tsp->ts_data + tsp->ts_offset));
		goto errout;
	}

	if ( Verbose )
	printf("Obtaining %s(%s) from server %s\n", file, member, rbp.bp_sname);


	ASSERT(tsp->ts_ent.te_lbn != 0);
	
	return ESUCCESS;
	
errout:
	if (fsb->Buffer != NULL) {
		_free_iobbuf(fsb->Buffer);
		fsb->Buffer = NULL;
	}
	/*
	 * The interface's close routine will be called by open in saio.c
	 * after the bootp file system open fails
	 */

	/*
	 * Set returned error code
	 */
	fsb->IO->ErrorNumber = (sverrno != 0) ? sverrno : EINVAL;
	
	return fsb->IO->ErrorNumber;
}

/*
 * _bootp_read(fsb) -- read from TFTP
 */
static int
_bootp_read(FSBLOCK *fsb)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];
	register int cc;
	register int offset;
	int savecount;
	int count = fsb->IO->Count;
	char *buf = fsb->IO->Address;
	
	dprintf(("bootpread: ctlr %d, buf %x, count %d\n",
		fsb->IO->Controller, buf, count));

	if (Verbose) start_spinner(30);

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	ASSERT(SEGSIZE == TP_BLKSIZE);	/* so we can mix TFTP blks and tape */
	ASSERT(fsb->Buffer != NULL);
	ASSERT(tsp->ts_offset <= SEGSIZE);

	/*
	 * Compute current logical position in the file
	 */
	offset = ((tsp->ts_blkno - 1 - tsp->ts_ent.te_lbn) * SEGSIZE) +
		 tsp->ts_offset;

	if (offset < (signed) fsb->IO->Offset.lo) {

		while (offset < (signed) fsb->IO->Offset.lo) {
			/*
			 * If we are reading a TPD format file, check that
			 * we are within that file.
			 */
			if (tsp->ts_ent.te_lbn != 0 &&
			    offset >= (signed) tsp->ts_ent.te_nbytes) {
				dprintf(("EOF on seek in TPD file at offset %d, chars %u\n", offset, tsp->ts_ent.te_nbytes));
				fsb->IO->Count = 0;
				if (Verbose) end_spinner();
				return(ESUCCESS);
			}
			if (tsp->ts_count > 0) {
				cc = MIN((signed)fsb->IO->Offset.lo - offset, tsp->ts_count);
				tsp->ts_offset += cc;
				tsp->ts_count -= cc;
				offset += cc;
				continue;
			}
			if ((cc = tftpread(fsb)) <= 0) {
				fsb->IO->Count = cc;
				if (Verbose) end_spinner();
				return(ESUCCESS);
			}
		}

	} else if (offset > (signed)fsb->IO->Offset.lo) {
		dprintf(("Backward seek to %u (current offset is %d)\n",
			fsb->IO->Offset.lo, offset));
		fsb->IO->Count = 0;
		if (Verbose) end_spinner();
		return(EIO);
	}

	/*
	 * If we are reading a TPD format file, restrict the size
	 * of the read to the remaining data in the subfile.
	 */
	if (tsp->ts_ent.te_lbn != 0)
		count = MIN(count, tsp->ts_ent.te_nbytes - offset);

	savecount = count;
	while (count > 0) {
		/*
		 * Copy characters from the io buffer if any
		 */
		if (tsp->ts_count > 0) {
			cc = MIN(count, tsp->ts_count);
			bcopy(tsp->ts_data + tsp->ts_offset, buf, cc);
			tsp->ts_offset += cc;
			tsp->ts_count -= cc;
			fsb->IO->Offset.lo += cc;
			count -= cc;
			buf += cc;
			continue;
		}
		/*
		 * Replenish the buffer
		 */
		if ((cc = tftpread(fsb)) <= 0)
			break;
	}

	fsb->IO->Count = savecount - count;
	if (Verbose) end_spinner();
	return(ESUCCESS);
}

/*
 * _bootp_close(fsb) -- close a BOOTP connection
 */
static int
_bootp_close(FSBLOCK *fsb)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];

	dprintf(("bootpclose: ctlr %d\n", fsb->IO->Controller));
	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);

	/*
	 * Cancel any pending alarms.  Since alarm() does not return
	 * an id like timeout(), all we can do is to blindly turn it off.
	 * This is needed since we may have longjmp'd out of scandevs
	 * in the middle of a bootp.  At this point (7/25/92), networking
	 * is the only code that uses alarms.
	 */
	alarm(0);

	if (fsb->Buffer != NULL) {
		_free_iobbuf(fsb->Buffer);
		fsb->Buffer = NULL;
	}

	/*
	 * If some data blocks have been received, but EOF not reached,
	 * send "abort" to prevent server from retrying
	 */ 
	if (tsp->ts_blkno > 0 && !tsp->ts_eof) {
		tftpabort(fsb);
	}

	return ESUCCESS;
}

/*
 * Format a BOOTP message for transmission
 */
static void
mk_bootp(FSBLOCK *fsb,
    struct bootp *bp,		/* packet buffer */
    char *host,			/* requested server name */
    char *file,			/* boot file name */
    u_long ciaddr)		/* Internet address of client (0 if unknown) */
{
	/*
	 * Construct the BOOTP header.
	 */
	bzero(bp, sizeof *bp);
	bp->bp_op = BOOTREQUEST;
	bp->bp_htype = ARPHRD_ETHER;
	bp->bp_hlen = sizeof (struct ether_addr);
	/*
	 * Select a new BOOTP transaction id
	 */
	bootp_xid = get_rand(1024, 65535);
	bp->bp_xid = bootp_xid;
	bp->bp_ciaddr.s_addr = ciaddr;
	/* set secs field high enough so cisco routers, etc. forward */
	bp->bp_secs = 5;

	bcopy((char *)cei(fsb)->cei_enaddr, bp->bp_chaddr,
		sizeof (struct ether_addr));
	if (host)
		strcpy((char*)bp->bp_sname, host);
	if (file)
		strcpy((char*)bp->bp_file, file);
	bp->vd_clntname[0] = 0;

}

static jmp_buf transact_buf;
static void
trans_handler (void)
{
	longjmp (transact_buf, 1);
}

static int
bootp_transaction(FSBLOCK *fsb,
    struct bootp *xbp,		/* transmit packet buffer */
    struct bootp *rbp)		/* receive packet buffer */
{
	volatile int tries;
	int cc;
	int old_alarm;
	SIGNALHANDLER old_handler;

	/*
	 * save currently running timer
	 */
	old_handler = Signal (SIGALRM, (SIGNALHANDLER)trans_handler);
	old_alarm = alarm(0);

	tries = 0;
	if (setjmp(transact_buf)) {
		dprintf(("BOOTP timeout\n"));
		/* increment to get through reluctant gateways */
		xbp->bp_secs += BOOTP_TIMEOUT;
	}
	if (++tries > BOOTP_TRIES) {
		/*
		 * re-establish previous timer
		 */
		alarm(0);
		Signal (SIGALRM, old_handler);
		alarm(old_alarm);
		return(-1);
	}
	/*
	 * set-up device independent iob parameters
	 */
	fsb->IO->Address = (char *)xbp;
	fsb->IO->Count = sizeof(struct bootp);
	DEVWRITE(fsb);
	alarm(BOOTP_TIMEOUT);
	for (;;) {
		do {
			_scandevs();	/* allow for console interrupts */
			fsb->IO->Address = (char *)rbp;
			fsb->IO->Count = sizeof(*rbp);
		} while ((cc = DEVREAD(fsb)) <= 0);
		
		if (!bootp_ok(fsb, rbp, cc))
			continue;
		/*
		 * re-establish previous timer
		 */
		alarm(0);
		Signal (SIGALRM, old_handler);
		alarm(old_alarm);
		return(0);
	}
	/*NOTREACHED*/
	return 0;		/* to keep the compiler happy */
}

/*
 * Verify a received message is valid BOOTP reply
 */
static int
bootp_ok(FSBLOCK *fsb,
    struct bootp *rbp, 		/* receive packet buffer */
    int count)			/* receive count */
{
	if (count < sizeof (struct bootp)) {
		dprintf(("bootp: msg too short (%d)\n", count));
		return (0);
	}
	if (rbp->bp_op != BOOTREPLY) {
		dprintf(("bootp: not BOOTP reply (%d)\n", rbp->bp_op));
		return (0);
	}
	if (rbp->bp_xid != bootp_xid) {
		dprintf(("bootp: wrong TXID (got %u, expected %d)\n",
			rbp->bp_xid, bootp_xid));
		return (0);
	}
	if (bcmp(rbp->bp_chaddr, (char *)cei(fsb)->cei_enaddr,
		sizeof (struct ether_addr)) != 0) {

		dprintf(("bootp: wrong ether address (%s)\n",
			ether_sprintf(rbp->bp_chaddr)));
		return (0);
	}

	return (1);
}

/*
 * TFTP connect
 *
 * Called from bootpopen to initiate a TFTP pseudo-connection.
 * Sends RRQ (Read ReQuest) packet to the server and waits for the response.
 */
static int
tftpconnect(FSBLOCK *fsb)
{
	struct tftpdmsg msg;	/* max TFTP packet */
	register struct tftphdr *thp = &msg.th;
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];
	register int sendcount;	/* TFTP message size */
	int recvcount;
	int tries;
	register char *cp;

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	fsb->Buffer = (CHAR *) _get_iobbuf();
	tsp->ts_count = 0;
	tsp->ts_data = (char *) fsb->Buffer;
	tsp->ts_offset = 0;
	tsp->ts_eof = 0;

	/*
	 * Initialize the TFTP header for RRQ packet.
	 */
	thp->th_opcode = htons(RRQ);
	sendcount = sizeof *thp;
	strcpy(thp->th_stuff, cei(fsb)->ei_filename);	/* set filename */
	for (cp = (char *)thp->th_stuff ; *cp ; cp++, sendcount++);
	cp++, sendcount++;
	strcpy(cp, "octet");			/* binary transfer mode */
	sendcount += 6;				/* 6 = strlen("octet") + 1 */
	tries = 0;
	
	/*
	 * Set the destination port address to the TFTP server port
	 */
	cei(fsb)->ei_dstaddr.sin_port = htons((u_short)IPPORT_TFTP);

	dprintf(("TFTP: RRQ for file %s to %s\n",
		cei(fsb)->ei_filename,
		inet_ntoa(cei(fsb)->ei_dstaddr.sin_addr)));

	/*
	 * send "RRQ" until a valid block is received or have to give up.
	 */
	for (;;) {
		/*
		 * Choose a different port on each attempt to avoid
		 * confusion with stale replies.
		 */
		if (cei(fsb)->ei_registry >= 0) {
			_free_socket(cei(fsb)->ei_registry);
			cei(fsb)->ei_registry = -1;
		}
		/*
		 * Check for zero bootp_xid in case the BOOTP step
		 * was skipped.
		 */
		if (bootp_xid == 0)
			bootp_xid = get_rand(1024, 65535);
		tsp->ts_myport = ++bootp_xid & 0x3FFF;
		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCBIND;
		fsb->IO->IoctlArg = (IOCTL_ARG)htons(tsp->ts_myport);
		if (fsb->DeviceStrategy (fsb->Device, fsb->IO) < 0) {
			printf("tftp bind failed\n");
			return(-1);
		}

		if (++tries > TFTP_TRIES) {
			dprintf(("TFTP: no response from server\n"));
			fsb->IO->ErrorNumber = ETIMEDOUT;
			return(-1);
		}

		fsb->IO->Count = sendcount;
		fsb->IO->Address = (char *)&msg;
		DEVWRITE(fsb);

		/*
		 * Return if get positive number of bytes or negative
		 * return value indicating an error.
		 */
		if ((recvcount = tftprecv(fsb)) != 0)
			break;
		/*
		 * No data bytes returned.  If the last block has been
		 * received, this means the file was zero length!
		 */
		if (tsp->ts_eof)
			break;
	}
	return(recvcount);
}

/*
 * TFTP read routine
 *
 * Return value is number of new data bytes read.
 */
static int
tftpread(FSBLOCK *fsb)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];
	int recvcount, tries;

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	/*
	 * Connection should have been established by tftpconnect
	 */
	if (tsp->ts_blkno < 0) {
		printf("tftpread: internal error, no connection\n");
		return(-1);
	}

	/*
	 * If EOF has already been received, just return.
	 */
	if (tsp->ts_eof)
		return(0);

	Dprintf(("TFTP: waiting for blk %d\n", tsp->ts_blkno+1),1);
	tries = 0;

	/*
	 * Wait for the next block.  If it doesn't arrive, resend
	 * the last acknowledgement.
	 */
	for (;;) {
		/*
		 * Receive until a good reply comes in or timeout.
		 */
		if ((recvcount = tftprecv(fsb)) != 0)
			break;

		/*
		 * No data bytes returned.  Check for the case in which
		 * the last block contains zero data bytes and just
		 * return instead of retrying.
		 */
		if (tsp->ts_eof)
			break;
		
		if (++tries > TFTP_TRIES) {
			dprintf(("Server timeout ... Giving up.\n"));
			fsb->IO->ErrorNumber = ETIMEDOUT;
			return(-1);
		}

		/*
		 * Poke the server by sending another ACK for the last block
		 */
		tftpack(fsb);
	}
	return (recvcount);	/* return number of data bytes received */
}

/*
 * TFTP receive.
 *
 * Receive messages until we get a valid TFTP reply or time out.
 * ACKs are sent to valid TFTP data blocks.
 *
 * Return value is the actual number of TFTP data bytes received
 * or -1 if error.
 */
static int
tftprecv(FSBLOCK *fsb)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];
	register struct tftpdmsg *rmp = (struct tftpdmsg *)fsb->Buffer;
	register int recvcount;
	int old_alarm;
	SIGNALHANDLER old_handler;

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	/*
	 * save currently running timer
	 */
	old_handler = Signal (SIGALRM, (SIGNALHANDLER)trans_handler);
	old_alarm = alarm(0);

	if (setjmp(transact_buf)) {
		dprintf(("TFTP timeout\n"));
		/*
		 * re-establish previous timer
		 */
		alarm(0);
		Signal (SIGALRM, old_handler);
		alarm(old_alarm);
		/*
		 * let higher level handle the retry
		 */
		return(0);
	}

	alarm(TFTP_TIMEOUT);
	for (;;) {
		fsb->IO->Count = sizeof (*rmp);
		fsb->IO->Address = (char *)rmp;
		do {
			_scandevs();	/* allow for console interrupts */
#ifdef	SN0
			/* Do this again, since _scandevs calling epoll()
			 * could have messed with Function code.
			 */
			fsb->IO->Count = sizeof (*rmp);
			fsb->IO->Address = (char *)rmp;
#endif
		} while ((recvcount = DEVREAD(fsb)) <= 0);
		
		/*
		 * Check the packet
		 */
		recvcount = tftp_ok(fsb, rmp, recvcount);
		if (Verbose >= 1) spinner();

		/*
		 * If it isn't the one we are awaiting, loop again.
		 * Note that we will get zero data bytes
		 * as a legitimate receive on the last block of a file
		 * whose length is a multiple of 512.  When this happens,
		 * the EOF flag will be set.
		 */
		if (recvcount == 0 && !tsp->ts_eof)
			continue;

		/*
		 * cancel our timer and re-establish previous timer
		 */
		alarm(0);
		Signal (SIGALRM, old_handler);
		alarm(old_alarm);
		return(recvcount);
	}
	/*NOTREACHED*/
	return 0;		/* to keep the compiler happy */
}

/*
 * Check the validity of a TFTP message
 *
 * Return values:
 *	0 - not a TFTP packet or a TFTP error that can be retried
 *    < 0 - non-recoverable TFTP error (e.g. abort from server)
 *    > 0 - number of TFTP data bytes in the packet
 */
static int
tftp_ok(FSBLOCK *fsb,
    struct tftpdmsg *rmp,
    int recvcount)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	/*
	 * Would like to use (sizeof (struct tftphdr)) here, but
	 * that doesn't quite work, so ...
	 */
#define TFTP_HDR_SIZE	((int)((long)(rmp->th.th_data) - (long)(rmp)))

	/*
	 * Take a look at the received packet
	 */
	if (recvcount < TFTP_HDR_SIZE) {
		dprintf(("tftprecv: short packet (len %d)\n",
			recvcount));
		return(0);
	}
	if (tsp->ts_blkno > 0 ) {
		if (ntohs(cei(fsb)->ei_srcaddr.sin_port) != tsp->ts_srvport) {
			dprintf(("tftprecv: pkt from server port %u, not %d\n",
				ntohs(cei(fsb)->ei_srcaddr.sin_port),
				tsp->ts_srvport));
			return(0);
		}
		if (cei(fsb)->ei_srcaddr.sin_addr.s_addr !=
		    cei(fsb)->ei_dstaddr.sin_addr.s_addr) {
			dprintf(("tftprecv: pkt from wrong server (%s)\n",
			    inet_ntoa(cei(fsb)->ei_srcaddr.sin_addr)));
			return(0);
		}
	}

	switch (ntohs(rmp->th.th_opcode)) {
	default:
		dprintf(("tftprecv: invalid TFTP opcode 0x%x\n",
			rmp->th.th_opcode));
		return(0);

	case ERROR:
		if ( Verbose )
		printf("\nTFTP error: %s (code %d)\n",
		    rmp->th.th_msg, ntohs(rmp->th.th_code));
		/* We get EBUSY back when a remote tape is not rewound. */
		if (strncmp(rmp->th.th_msg,"Device or resource busy",23) == 0 )
			fsb->IO->ErrorNumber = EBUSY;
		else
			fsb->IO->ErrorNumber = ECONNABORTED;
		return (-1);

	case DATA:
		break;
	}
	
	if (tsp->ts_blkno < 0) {	/* receiving 1st block */
		tsp->ts_srvport = ntohs(cei(fsb)->ei_srcaddr.sin_port);
		cei(fsb)->ei_dstaddr.sin_port = htons(tsp->ts_srvport);
		/*
		 * Reset our idea of the server's address since
		 * the server may have multiple interfaces and then
		 * replied to us via a different interface.  This will
		 * happen sometimes when going through gateways to servers
		 * that are also gateways.
		 */
		if (cei(fsb)->ei_dstaddr.sin_addr.s_addr !=
					cei(fsb)->ei_srcaddr.sin_addr.s_addr) {
			cei(fsb)->ei_dstaddr.sin_addr.s_addr =
					cei(fsb)->ei_srcaddr.sin_addr.s_addr;
			dprintf(("TFTP server address changed to: %s",
		       inet_ntoa(cei(fsb)->ei_dstaddr.sin_addr)));
		}
		tsp->ts_blkno = 0;
		tsp->ts_eof = 0;
	}

	/*
	 * Be careful with the block number.  The th_block field
	 * in the header is declared as a signed short, which would
	 * limit the size of files transferred with TFTP to 16 megabytes.
	 * Fortunately the server code treats the block number as
	 * an unsigned short, which allows the transfer of files
	 * up to 32 megabytes in length.
	 */
	if ((u_short)ntohs(rmp->th.th_block) != tsp->ts_blkno+1) {
		dprintf(("tftprecv: got blk %u, expect %d\n",
			(u_short)rmp->th.th_block, tsp->ts_blkno+1));
		/*
		 * Resend the last ack at this point in an attempt
		 * to get back in sync with the server.
		 */
		tftpack(fsb);
		return (0);
	}

	/*
	 * Increment block number
	 */
	tsp->ts_blkno++;

	/*
	 * Compute actual count of TFTP data bytes received
	 */
	recvcount -= TFTP_HDR_SIZE;
	tsp->ts_data = rmp->th.th_data;
	tsp->ts_offset = 0;
	tsp->ts_count = recvcount;

	Dprintf(("tftprecv: got blk nbr %d, %d bytes\n",
		tsp->ts_blkno, recvcount),1);
	
	if (recvcount < SEGSIZE)
		/*
		 * This is the last block of the file.
		 */
		tsp->ts_eof = 1;

	/*
	 * Acknowledge this block now to get some overlap with the server.
	 */
	tftpack(fsb);

	/*
	 * Note that recvcount may be 0 in the case that we
	 * have just received the last block of a file that has
	 * length divisible by 512 bytes.
	 */
	return(recvcount);

#undef TFTP_HDR_SIZE
}

/*
 * TFTP send acknowledgement routine.
 *
 * Format and transmit an acknowledgement for the most recently
 * received block.
 */
static void
tftpack(FSBLOCK *fsb)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];
	struct tftphdr msg;	/* TFTP packet */
	register struct tftphdr *thp = &msg;

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	/*
	 * It is an error if we are acknowledging a block
	 * number less than 1.
	 */
	if (tsp->ts_blkno < 1) {
		printf("tftpack: called for bogus block %d\n", tsp->ts_blkno);
		return;
	}

	/*
	 * Initialize the TFTP header for ACK.
	 */
	thp->th_opcode = htons(ACK);
	thp->th_block = htons(tsp->ts_blkno);
	Dprintf(("TFTP send ACK for blk %d\n", tsp->ts_blkno),1);

	fsb->IO->Count = sizeof *thp;
	fsb->IO->Address = (char *)thp;
	DEVWRITE(fsb);
}

/*
 * TFTP error send routine.
 *
 * Used when we don't intend to read all the blocks of the file.
 * Sends a TFTP error packet to stop the server from retrying.
 */
static void
tftpabort(FSBLOCK *fsb)
{
	struct tftphdr msg;	/* TFTP packet */
	register struct tftphdr *thp = &msg;

	ASSERT(0 <= fsb->IO->Controller && fsb->IO->Controller < MAX_IFCS);
	/*
	 * Initialize the TFTP header for ERROR packet.
	 */
	thp->th_opcode = htons(ERROR);
	thp->th_code = htons(ENOSPACE);
	dprintf(("tftpabort: TFTP send ERROR packet\n"));

	/*
	 * Send "ERROR" only once, since it will not be acknowledged.
	 */
	fsb->IO->Count = sizeof *thp;
	fsb->IO->Address = (char *)thp;
	DEVWRITE(fsb);
}

/*
 * Make up a sort-of-random number.
 *
 * Note that this is not a general purpose routine, but is geared
 * toward the specific application of generating an integer in the
 * range of 1024 to 65k.
 */
static int
get_rand(int lb, int ub)		/* upper and lower bounds */
{
	register int secs;
	register int rand;

	/*
	 * Read the real time clock which returns the number of
	 * seconds from some epoch.
	 */
	secs = get_tod();

	/*
	 * Combine different bytes of the word in various ways
	 */
	rand  = (secs & 0xff00) >> 8;
	rand *= (secs & 0xff0000) >> 16;
	rand += (secs & 0xff);
	rand *= (secs & 0xff000000) >> 24;
	rand >>= (secs & 7);
	if (rand < 0)
		rand *= -1;

	/*
	 * Now fit the range
	 */
	ub -= lb;
	rand = rand % ub;
	rand += lb;

	dprintf(("get_rand returns %d\n", rand));

	return (rand);
}

/*  If the offset if unchanged by Seek(), and there are bytes buffered
 * then return ESUCCESS (ie read of 1 byte will return immediately),
 * else return EAGAIN signifying that we'll have to go out to the
 * network for more bytes of the EOF.
 */
static int
_bootp_getrs(FSBLOCK *fsb)
{
	register struct tftp_state *tsp = &tftp_st[fsb->IO->Controller];
	int offset;

	offset = ((tsp->ts_blkno - 1 - tsp->ts_ent.te_lbn) * SEGSIZE) +
		tsp->ts_offset;

	if ((offset == fsb->IO->Offset.lo) && (tsp->ts_count > 0)) {
		fsb->IO->Count = tsp->ts_count;
		return(ESUCCESS);
	}

	return(EAGAIN);
}

static int
_bootp_getfileinfo(FSBLOCK *fsb)
{
	register struct tftp_state *tsp;
	FILEINFORMATION fi;

	bzero(&fi, sizeof fi);

	tsp = &tftp_st[fsb->IO->Controller];

	if ( tsp->ts_ent.te_lbn ) {
		strncpy(fi.FileName, tsp->ts_ent.te_name, 31);
		fi.EndingAddress.lo = tsp->ts_ent.te_nbytes;
	} else
		strncpy(fi.FileName, cei(fsb)->ei_filename, 31);

	fi.CurrentAddress.lo  = fsb->IO->Offset.lo;
	fi.Attributes         = ReadOnlyFile;
	fi.Type               = fsb->Device->Type;

	if ( fsb->IO->Flags & F_DIR )
		fi.Attributes |= DirectoryFile;

	bcopy(&fi, fsb->IO->Address, sizeof fi);

	return ESUCCESS;
}
