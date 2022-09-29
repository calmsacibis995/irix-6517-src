/* rpssio.c -- handles RPSS io functionality.  Derived from old saio/lib/saio.c
 * and modified to meet ARCS (was RPSS) spec.
 */

#include <arcs/io.h>
#include <arcs/time.h>
#include <arcs/hinv.h>
#include <arcs/eiob.h>
#include <arcs/folder.h>
#include <arcs/cfgtree.h>
#include <arcs/errno.h>

#include <saio.h>
#include <libsc.h>
#include <libsk.h>

#ifdef SN0
#include <pgdrv.h>
#endif

#ident "$Revision: 1.93 $"

#ifdef NETDBX
#include <saioctl.h>
#include <net/in.h>
#include <net/ei.h>
#include <net/socket.h>

extern struct eiob *new_eiob();
extern struct eiob *get_eiob(ULONG);
extern void free_eiob(struct eiob *);
extern char netaddr_default[];


#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

/*
 * Extensions for bringing udp to user layer.
 */
LONG
Bind(ULONG fd, u_short pno)
{
	struct eiob *io = get_eiob(fd);
	int errflg;
	extern int _ifioctl(IOBLOCK *);
	struct in_addr myiaddr;
	char *cp;

	if (io == NULL) {
		printf("Bind: Cannot find ioblock for fd = %d\n", fd);
		return(EBADF);
	}

	/*
	io->iob.FunctionCode = FC_IOCTL;
	io->iob.IoctlCmd = (IOCTL_CMD)NIOCBIND;
	io->iob.IoctlArg = (IOCTL_ARG)htons(pno);

	dprintf(("Trying bind for fd = %d, port = %d, devstrat = 0x%x\n",
		fd, pno, io->devstrat));
	errflg = (*io->devstrat)(io->dev, &(io->iob));
	dprintf(("Bind Done for fd = %d, port = %d, errflg = %d\n",
			fd, pno, errflg));
	*/


	io->iob.IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCBIND;
	io->iob.IoctlArg = (IOCTL_ARG)htons(pno);


	errflg = _ifioctl(&(io->iob));

	dprintf(("Bind Done for fd = %d, port = %d, errflg = %d\n",
			fd, pno, errflg));

       myiaddr.s_addr = 0;
        if ((cp = getenv("netaddr")) != NULL && *cp != '\0') {
		/*
		 * If Internet address is set in the environment, then
		 * configure the interface for that IP address.
		 */
		myiaddr = inet_addr(cp);
		if (!strcmp(cp, netaddr_default)) {
			printf("%s%s%s",
			"Warning: 'netaddr' is set to the default address ", netaddr_default,
			".\nUse 'setenv' to reset it to an Internet address on your network.\n");
			return(errflg);
		}
		if (ntohl(myiaddr.s_addr) == -1) {
			printf("$netaddr is not a valid Internet address\n");
			return(errflg);
		}
		io->iob.FunctionCode = FC_IOCTL;
		io->iob.IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCSIFADDR;
		io->iob.IoctlArg = (IOCTL_ARG)&myiaddr;
		if ((errflg = _ifioctl(&(io->iob))) < 0) {
			printf("Unable to set interface address\n");
		}

		return(errflg);
	}
	else {
		printf("netaddr variable not set\n");
		return(errflg);
	}
}

#define CEI(x)	((struct ether_info *)(x->iob.DevPtr))

LONG
GetSource(ULONG fd, struct sockaddr_in *dstaddr)
{
	struct eiob *io = get_eiob(fd);

	if (io == NULL)
		return(EINVAL);

	dstaddr->sin_addr.s_addr = CEI(io)->ei_srcaddr.sin_addr.s_addr;
	dstaddr->sin_port = CEI(io)->ei_srcaddr.sin_port;
	dstaddr->sin_family = CEI(io)->ei_srcaddr.sin_family;

	return(ESUCCESS);
}

LONG
BindSource(ULONG fd, struct sockaddr_in dstaddr)
{
	struct eiob *io = get_eiob(fd);
	char	*gateaddr;
	struct in_addr 		gateiaddr;
	struct sockaddr_in	gatesaddr;

	if (io == NULL)
		return(EINVAL);

	if ((gateaddr = getenv("gateaddr")) != NULL && *gateaddr != '\0') {
		gateiaddr = inet_addr(gateaddr);
		if (ntohl(gateiaddr.s_addr) == -1) {
			dprintf(("$gateaddr not valid\n"));
			gateaddr = "192.48.165.1";
		}
	} else gateaddr = "192.48.165.1";

	CEI(io)->ei_dstaddr.sin_addr.s_addr = dstaddr.sin_addr.s_addr;
	CEI(io)->ei_dstaddr.sin_port = dstaddr.sin_port;
	CEI(io)->ei_dstaddr.sin_family = dstaddr.sin_family;

	gateiaddr.s_addr = 0;
	gateiaddr = inet_addr(gateaddr);
	bzero(&gatesaddr, sizeof(struct sockaddr_in));
	gatesaddr.sin_family = AF_INET;
	gatesaddr.sin_addr.s_addr = *(u_int *)&gateiaddr;

	CEI(io)->ei_gateaddr = gatesaddr;

	dprintf(("DST address set to: %s, port = %d\n",
		inet_ntoa(CEI(io)->ei_dstaddr.sin_addr),
			CEI(io)->ei_dstaddr.sin_port));
	return ESUCCESS;
}
#endif /* NETDBX */


LONG
Open(CHAR *filename, OPENMODE flags, ULONG *fd)
{
	register struct eiob *io;
	int rc = EACCES;
	int errflg;

	*fd = 0xffffffff;	/* make sure fd is invalid on error */

	if (!filename)
		return(EINVAL);

	if ((io = new_eiob()) == NULL)
		return(EMFILE);

	io->fsstrat = 0;		/* must init before parse_path() */
	io->iob.Flags = 0;

#ifdef SN0
	io->iob.Controller = io->iob.Unit = 0 ;
#ifdef SN_PDI
	if (errflg = sn_parse_path(filename, io)) {
		rc = errflg ;
		goto bad ;
	}
#else
	if (errflg = kl_parse_path(filename, io)) {
			rc = errflg ;
			goto bad ;
	}
#endif
#else /* !SN0 */
	if (errflg=parse_path(filename, io)) {
		rc = errflg;
		goto bad;
	}
#endif /* SN0 */
	/*  If console(X) protocol is used, make sure device can operate
	 * as console.  Devices advertising ConsoleIn/Out will recognize
	 * both F_CONSx flags.
	 */
	if ((io->iob.Flags & (F_CONS|F_ECONS)) &&
		((io->dev->Flags & (ConsoleIn|ConsoleOut)) == 0)) {
		rc = EINVAL;
		goto bad;
	}

	/* make sure opened device has a driver, and device init passed */
	if ((io->devstrat == NULL) || (io->dev->Flags & Failed)) {
		rc = ENODEV;
		goto bad;
	}

	/*  Check if openmode matches the component's Flags, and set
	 * iob.Flags if the mode is valid.
	 */
	switch (flags) {
	case OpenReadOnly:
		if ((io->dev->Flags & Input) == 0)
			goto bad;
		io->iob.Flags |= F_READ;
		break;

	case OpenWriteOnly:
		if ((io->dev->Flags & Output) == 0)
			goto bad;
		io->iob.Flags |= F_WRITE;
		break;

	case OpenReadWrite:
		if ((io->dev->Flags & (Output|Input)) != (Output|Input))
			goto bad;
		io->iob.Flags |= F_READ | F_WRITE;
		break;

	case CreateWriteOnly:
		if ((io->dev->Flags & Output) == 0)
			goto bad;
		io->iob.Flags |= F_WRITE|F_CREATE;
		break;

	case CreateReadWrite:
		if ((io->dev->Flags & (Output|Input)) != (Output|Input))
			goto bad;
		io->iob.Flags |= F_READ|F_WRITE|F_CREATE;
		break;

	case SupersedeWriteOnly:
		if ((io->dev->Flags & Output) == 0)
			goto bad;
		io->iob.Flags |= F_WRITE|F_SUPERSEDE;
		break;

	case SupersedeReadWrite:
		if ((io->dev->Flags & (Output|Input)) != (Output|Input))
			goto bad;
		io->iob.Flags |= F_READ|F_WRITE|F_SUPERSEDE;
		break;

	case OpenDirectory:
		if ((io->dev->Flags & Input) == 0)
			goto bad;
		io->iob.Flags |= F_READ|F_DIR;
		break;

	case CreateDirectory:
		if ((io->dev->Flags & Output) == 0)
			goto bad;
		io->iob.Flags |= F_WRITE|F_CREATE|F_DIR;
		break;

	default:			/* unknown open mode! */
		rc = EINVAL;
		goto bad;
	}

	/*  If we want to open a device directly (no filename given), then
	 * Open() is invalid in filesystem modes (create, supersede, or
	 * directory).
	 */
	if ((!io->filename) && (io->iob.Flags&(F_DIR|F_SUPERSEDE|F_CREATE))) {
		rc = ENOTDIR;
		goto bad;
	}

	/* Devices are assumed to be character, non-filesystem devices
	 * unless marked otherwise.  Device open function gets a crack at
	 * changing these if the driver must.
	 */
	switch (io->dev->Type) {
	case DiskController:
	case TapeController:
	case CDROMController:
	case WORMController:
	case DiskPeripheral:
	case FloppyDiskPeripheral:
	case TapePeripheral:
		io->iob.Flags |= F_BLOCK | F_FS;
		break;

	case NetworkPeripheral:
	case NetworkController:
		io->iob.Flags |= F_FS;
		break;

	case PointerPeripheral:
		io->iob.Flags |= F_MS;
		break;
	};

	io->iob.FunctionCode = FC_OPEN;

	errflg = (*io->devstrat)(io->dev, &io->iob);
	if (errflg) {
		rc = io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
		goto bad;
	}

	/*
	 * Now that device is open, try to find the real filesystem type
	 * if and only if a path name is given.  The fs_search function
	 * will set io->fsstrat if a matching filesys is found. If
	 * io->fsstrat is set before getting here, then the parser
	 * interpreted a filesystem type in the pathname, and the file
	 * systems do not need to be searched.
	 */
	io->fsb.Device = io->dev;
	io->fsb.DeviceStrategy = io->devstrat;
	io->fsb.Filename = (CHAR *)io->filename;
	io->fsb.IO = &io->iob;

	if (io->fsstrat) {
		/* must have a filename if listed a filesystem */
		if (!io->filename) {
			rc = EINVAL;
			goto bad1;
		}
		/* make sure the filesystem given is valid */
		io->fsb.FunctionCode = FS_CHECK;
		if ((rc=(*io->fsstrat)(&io->fsb)) != ESUCCESS)
			goto bad1;
	}
	else if (io->filename) {
		if (io->iob.Flags & F_FS){
			rc = fs_search (io);
			if (rc != ESUCCESS) {
#if defined(DEBUG) || defined(SN0)
				printf("No file system found for \"%s\".\n",
				       io->filename);
#endif
				rc = ENXIO;
				goto bad1;
			}
			
		}
		else {
			rc = EINVAL;
			goto bad1;
		}
	}

	if (io->fsstrat) {
		io->fsb.FunctionCode = FS_OPEN;
		errflg = (*io->fsstrat)(&io->fsb);
		if (errflg) {
			rc = io->iob.ErrorNumber;
			goto bad1;
		}
	}

	io->iob.Offset.hi = 0;
	io->iob.Offset.lo = 0;
	*fd = io->fd;
	return(ESUCCESS);

bad1:
	io->iob.FunctionCode = FC_CLOSE;
	(void)(*io->devstrat)(io->dev, &io->iob);
bad:
	free_eiob(io);
	return(rc);
}

LONG
Read(ULONG fd, void *buf, ULONG cnt, ULONG *n)
{
	register struct eiob *io = get_eiob(fd);
	int retval = 0;
	long resid;
#ifdef NETDBX
	int  readany = 0;
#endif /* NETDBX */

	if (io == NULL || (io->iob.Flags&F_READ) == 0)
		return(EBADF);
	if (io->iob.Flags&F_DIR)
		return(EISDIR);

	_scandevs();

#ifdef NETDBX
	readany = (io->iob.Flags & F_READANY);
#endif /* NETDBX */
	if (io->fsstrat) {
		io->iob.Address = buf;
		io->iob.Count = cnt;

		/*
		 * fs routines take the current file offset and perform
		 * the appropriate transfer by calling the device strategy
		 * routines directly.  By convention, StartBlock indicates the
		 * current block in the buffer pointed to by Buffer (if
		 * one exists).
		 *
		 */
		io->fsb.FunctionCode = FS_READ;
		if ((*io->fsstrat)(&io->fsb)) {
			*n = 0;
			return io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
		}

		*n = io->iob.Count;
		return ESUCCESS;
	}
#ifdef NETDBX
	else for ( resid = cnt; (readany && resid == cnt) || (!readany && resid); ) {
#else
	else for ( resid = cnt; resid; ) {
#endif /* NETDBX */
		long alignoff;
		unsigned long effcnt;

		io->iob.FunctionCode = FC_READ;

		/*
		 * If a block device offset is not block aligned, or
		 * the amount to transfer is less than a block,
		 * then transfer a single block to an intermediate
		 * buffer first.
		 */
		if ((io->iob.Flags & F_BLOCK) &&
			((alignoff = io->iob.Offset.lo % BLKSIZE) ||
						    resid < BLKSIZE) ) {
			/* XXX The following code assumes Offset.hi = 0. */
			char alignbuf[BLKSIZE];

			/* Clamp transfer to not cross block boundary */
			if ( alignoff+resid >= BLKSIZE )
				effcnt = BLKSIZE-alignoff;
			else
				effcnt = resid;

			/* NEEDS_WORK: can overflow StartBlock currently */
			io->iob.StartBlock =(io->iob.Offset.lo/BLKSIZE) +
					    (io->iob.Offset.hi*BLKSIZEHIGH);
			io->iob.Address = alignbuf;
			io->iob.Count = BLKSIZE;

			retval = (*io->devstrat)(io->dev, &io->iob);

			if (retval != ESUCCESS)
				break;

			bcopy(alignbuf+alignoff, buf, effcnt);

			buf = (char *)buf+effcnt;
			resid -= effcnt;
			addulongtolarge(effcnt, &io->iob.Offset);
			continue;
		}

		/*
		 * Clamp a block transfer to an whole block count
		 */
		if ( io->iob.Flags & F_BLOCK ) {
			/* NEEDS_WORK: can overflow StartBlock currently */
			io->iob.StartBlock =(io->iob.Offset.lo/BLKSIZE) +
					    (io->iob.Offset.hi*BLKSIZEHIGH);
			if (resid % BLKSIZE)
				io->iob.Count = resid - (resid % BLKSIZE);
			else
				io->iob.Count = resid;
		} else
			io->iob.Count = resid;
		io->iob.Address = buf;
		retval = (*io->devstrat)(io->dev, &io->iob);
		if (retval != ESUCCESS)
			break;
		addulongtolarge(io->iob.Count, &io->iob.Offset);
		buf = (char *)buf+io->iob.Count;
		resid -= io->iob.Count;
	}

	if (retval) {
		*n = 0;
		return io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
	}

#ifdef NETDBX
	*n = readany ? (cnt - resid) : cnt;
#else
	*n = cnt;
#endif /* NETDBX */
	return ESUCCESS;
}

/*
 * write -- standalone write
 */
LONG
Write(ULONG fd, void *buf, ULONG cnt, ULONG *n)
{
	register struct eiob *io = get_eiob(fd);
	int retval;

	if (io == NULL || (io->iob.Flags&F_WRITE) == 0)
		return(EBADF);
	if (io->iob.Flags&F_DIR)
		return(EISDIR);

	_scandevs(); 

	io->iob.Address = buf;
	io->iob.Count = cnt;

	if (io->fsstrat) {
		io->fsb.FunctionCode = FS_WRITE;
		retval = (*io->fsstrat)(&io->fsb);
	}
	else {
		if (io->iob.Flags & F_BLOCK) {
			if (io->iob.Offset.lo % BLKSIZE) {
				printf("offset not on block boundry\n");
				return EIO;
			}
		}
		io->iob.StartBlock = (io->iob.Offset.lo/BLKSIZE) +
			(io->iob.Offset.hi*BLKSIZEHIGH);
		io->iob.FunctionCode = FC_WRITE;
		retval =(*io->devstrat)(io->dev, &io->iob);
		if (retval == ESUCCESS)
			addulongtolarge(io->iob.Count, &io->iob.Offset);
	}

	if (retval) {
		*n = 0;
		retval = io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
	}
	else {
		*n = io->iob.Count;
		retval = 0;
	}

	return (retval);
}

LONG
Close(ULONG fd)
{
	register struct eiob *io = get_eiob(fd);
	int retval;

	if (io == NULL || io->iob.Flags == 0)
		return(EBADF);

	retval = 0;
	if (io->fsstrat) {
		io->fsb.FunctionCode = FS_CLOSE;
		retval = (*io->fsstrat)(&io->fsb);
	}

	io->iob.FunctionCode = FC_CLOSE;
	retval |= (*io->devstrat)(io->dev, &io->iob);

	if (retval)
		retval = io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;

	free_eiob(io);
	return (retval);
}

/*
 * Seek(fd, off, how) -- set file offset
 */
LONG
Seek(ULONG fd, LARGEINTEGER *off, SEEKMODE how)
{
	register struct eiob *io = get_eiob(fd);
	LONG sh;
	ULONG sl;
	LARGE newoff;

	if (io == NULL || io->iob.Flags == 0)
		return(EBADF);

	switch (how) {
	case SeekAbsolute:
		sh = off->hi;
		sl = off->lo;
		break;

	case SeekRelative:
		addlarge(&io->iob.Offset, off, &newoff);
		sh = newoff.hi;
		sl = newoff.lo;
		break;
	default:
		return(EINVAL);
	}

	/* guard against invalid offset (negative) */
	if (sh >= 0) {
		io->iob.Offset.hi = sh;
		io->iob.Offset.lo = sl;
	}
	
	off->hi = io->iob.Offset.hi;
	off->lo = io->iob.Offset.lo;
	return(ESUCCESS);
}

/*
 * GetDirectoryEntry -- standalone directory read
 */
LONG
GetDirectoryEntry(ULONG fd, DIRECTORYENTRY *buf, ULONG cnt, ULONG *rcnt)
{
	register struct eiob *io = get_eiob(fd);
	int retval;

	if (io == NULL || (io->iob.Flags&F_READ) == 0 || io->fsstrat == 0)
		return(EBADF);
	if ((io->iob.Flags&F_DIR) == 0)
		return(ENOTDIR);

	_scandevs();

	/* On calling fs strat routine, i_ma contains user buffer
	 * address, and i_cc contains the number of entries
	 * requested.  Fs strat routine returns the number of
	 * entries actually read, setting errno on error.
	 */
	io->iob.Address = buf;
	io->iob.Count = cnt;
	io->iob.ErrorNumber = ESUCCESS;		/* just in case */
	io->fsb.FunctionCode = FS_GETDIRENT;
	retval = (*io->fsstrat)(&io->fsb);
	*rcnt = io->iob.Count;

	return(retval ? io->iob.ErrorNumber : ESUCCESS);
}

TIMEINFO *
GetTime(void)
{
	static TIMEINFO t;

	cpu_get_tod(&t);
	return(&t);
}

static SYSTEMID _sysid;

void
_init_systemid(void)
{
	char *buf;
	strcpy(_sysid.VendorId,"SGI");
	bzero (_sysid.ProductId, sizeof(_sysid.ProductId));
	buf = getenv ("eaddr");
	if (buf) {
	    _sysid.ProductId[0] = buf[6];
	    _sysid.ProductId[1] = buf[7];
	    _sysid.ProductId[2] = buf[9];
	    _sysid.ProductId[3] = buf[10];
	    _sysid.ProductId[4] = buf[12];
	    _sysid.ProductId[5] = buf[13];
	    _sysid.ProductId[6] = buf[15];
	    _sysid.ProductId[7] = buf[16];
	}
}

SYSTEMID *
GetSystemId(void)
{
	return(&_sysid);
}

LONG
GetReadStatus(ULONG fd)
{
	register struct eiob *io = get_eiob(fd);
	int rc;

	if (io == NULL || (io->iob.Flags&F_READ) == 0)
		return(EBADF);
	if (io->iob.Flags&F_DIR)
		return(EISDIR);

	_scandevs();

	if (io->fsstrat) {
		io->fsb.FunctionCode = FS_GETREADSTATUS;
		rc = (*io->fsstrat)(&io->fsb);
	}
	else {
		io->iob.FunctionCode = FC_GETREADSTATUS;
		/* NEEDS_WORK: can overflow StartBlock currently */
		io->iob.StartBlock = (io->iob.Offset.lo/BLKSIZE) +
			(io->iob.Offset.hi*BLKSIZEHIGH);
		rc = (*io->devstrat)(io->dev, &io->iob);
	}

	/* rc != 0 implies a error as recorded by the driver */
	return(rc ? io->iob.ErrorNumber : ESUCCESS);
}

LONG
Mount(CHAR *path, MOUNTOPERATION op)
{
	register struct eiob *io;
	long rc;

	if ((io = new_eiob()) == NULL)
		return(EMFILE);
	if (parse_path(path, io)) {
		free_eiob(io);
		return(EINVAL);
	}

	io->iob.FunctionCode = FC_MOUNT;
	io->iob.Count = op;
	rc = (*io->devstrat)(io->dev, &io->iob);

	rc = rc ? io->iob.ErrorNumber : ESUCCESS;

	free_eiob(io);
	return(rc);
}

LONG
SetEnvironmentVariable(CHAR *name, CHAR *value)
{
	return(_setenv(name, value, 0));
}

CHAR *
GetEnvironmentVariable(CHAR *name)
{
	return((CHAR *)getenv((const char *)name));
}

/*
 * GetFileInformation - Return info about a file or disk, like fstat
 */
LONG
GetFileInformation(ULONG fd, FILEINFORMATION *info)
{
	register struct eiob *io = get_eiob(fd);
	int rc;

	if (io == NULL)
		return(EBADF);

	_scandevs();

	io->iob.Address = info;
	io->iob.Count = sizeof *info;
	io->iob.ErrorNumber = ESUCCESS;

	if (io->fsstrat) {
		io->fsb.FunctionCode = FS_GETFILEINFO;
		rc = (*io->fsstrat)(&io->fsb);
	}
	else {
		io->iob.FunctionCode = FC_GETFILEINFO;
		rc = (*io->devstrat)(io->dev, &io->iob);
	}

	return(rc ? io->iob.ErrorNumber : ESUCCESS);
}

/*ARGSUSED*/
LONG
SetFileInformation(ULONG fd, ULONG flags, ULONG mask)
{
	_scandevs();

	return EINVAL;
}
