#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtioctl.c,v 1.5 1995/03/15 17:55:47 tap Exp $"

#include <errno.h>

#include "rmtlib.h"

#ifdef RMTIOCTL
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/param.h>
#endif

#ifdef RMTIOCTL
static int _rmt_ioctl(int, int, char *);
#endif

/*
 *	Do ioctl on file.  Looks just like ioctl(2) to caller.
 */
 
int rmtioctl (fildes, request, arg)
int fildes, request, arg;
{
	if (isrmt (fildes))
	{
#ifdef RMTIOCTL
		return (_rmt_ioctl (fildes - REM_BIAS, request, (char *)arg));
#else
		setoserror( EOPNOTSUPP );
		return (-1);		/* For now  (fnf) */
#endif
	}
	else
	{
		return (ioctl (fildes, request, arg));
	}
}


/*
 *	_rmt_ioctl --- perform raw tape operations remotely
 */

#ifdef RMTIOCTL
static int
_rmt_ioctl(int fildes, int op, char *arg)
{
	char c;
	int rc, cnt, ssize;
	char buffer[BUFMAGIC];
	char *p, *omtget;
	short mt_type;

/*
 *	MTIOCOP is the easy one. nothing is transfered in binary
 */

	if (op == MTIOCTOP)
	{
		sprintf(buffer, "I%d\n%d\n", ((struct mtop *) arg)->mt_op,
			((struct mtop *) arg)->mt_count);
		if (_rmt_command(fildes, buffer) == -1)
		{
			return(-1);
		}
		return(_rmt_status(fildes));
	}

#ifdef BSD
/*
 *	we can only handle 2 ops, if not the other one, punt
 */

	if (op != MTIOCGET)
	{
		setoserror( EINVAL );
		return(-1);
	}
#endif

#ifdef sgi
/*
 * SGI has two (2) other ioctl's that we must handle:
 *   MTSCSIINQ		SCSI inquiry command
 *   MTIOCGETBLKSIZE	get blocksize
 */

	switch( op )
	{
	   case MTIOCGET: 

		/*
		 *	grab the status and read it directly into the structure
		 *  Since the data is binary data, and the other machine might
		 *  The original code could overwrite the space it malloced;
		 *  must be careful to not do that!
		 *  NOTE: the original /etc/rmt did NOT support a newline after
		 *  the S command, and Sun still does not.  Neither does the
		 *  current bsd source, all the way through the tahoe release.
		 *  So do NOT add the \n to this!  The sgi rmt command will
		 *  work either way.  Olson, 4/91
		 */
		if (_rmt_command(fildes, "S") == -1 ||
		    (rc = _rmt_status(fildes)) == -1)
			return(-1);
		
		ssize = rc;

		/*
		 * if server is of the old version, then the mtget struct
		 * that it returns is not the same as the one that we knows
		 * so lets convert it into something that we can use
		 * (note this assumption may no longer be true either, since
		 * sizeof old_mtget is 16, but some Sun machines return
		 * 20 bytes of data, at least one running 4.0.3 does...
		 */
		if (server_version == -1) {
			p = omtget = (char *)malloc(sizeof(struct old_mtget));
			if (p == (char *)0) {
				_rmt_abort(fildes);
				setoserror( ENOMEM );
				return(-1);
			}
			if(sizeof(struct old_mtget) < ssize)
				ssize = sizeof(struct old_mtget);
		} else {
			if(sizeof(struct mtget) < ssize)
				ssize = sizeof(struct mtget);
			p = arg;
		}
		

		rc -= ssize;
		for (; ssize > 0; ssize -= cnt, p += cnt)
		{
			cnt = read(READ(fildes), p, ssize);
			if (cnt <= 0)
			{
abortit:
				_rmt_abort(fildes);
				setoserror( EIO );
				return(-1);
			}
		}

		/* handle any bytes we didn't know what to do with */
		while(rc-- > 0)
			if(read(READ(fildes), &c, 1) <= 0)
				goto abortit;

		/*
		 *	now we check for byte position. mt_type is a small
		 *	integer field (normally) so we will check its
		 *	magnitude. if it is larger than
		 *	256, we will assume that the bytes are swapped and
		 *	go through and reverse all the bytes
		 */
		if (server_version == -1)  
			p = omtget;
		else
			p = arg;

		mt_type = ((struct mtget *) p)->mt_type;

	
		if (mt_type >= 256) {
			/* assume that we need to swap byte */
			for (cnt = 0; cnt < rc; cnt += 2)
			{
				c = p[cnt];
				p[cnt] = p[cnt+1];
				p[cnt+1] = c;
			}
		}

		/* 
		 * now mtgetp has the correct (byte-swapped if needed)
		 * data, if server is of old version then lets convert
		 * the data into something that we can use
		 * else all done
		 */
		if (server_version == -1) {
			struct mtget *newp = (struct mtget *)arg;
			struct old_mtget *oldp = (struct old_mtget *)omtget;

			newp->mt_type = oldp->mt_type;
			newp->mt_dsreg = oldp->mt_dsreg;
			newp->mt_erreg = oldp->mt_erreg;
			newp->mt_resid = oldp->mt_resid;
			newp->mt_fileno = oldp->mt_fileno;
			newp->mt_blkno = oldp->mt_blkno;

			/* 
			 * dsreg has the HW specific bits set and it is
			 * different bet. tape driver, that is why
			 * dposn is invented in the newer version so that 
			 * the code that deciphers dposn can be generic
			 * old version doesn't know about dposn, so just
			 * set it to 0 to get consistent result
			 */
			newp->mt_dposn = 0;
		}

		return(0);

	   case MTSCSIINQ:
		if (server_version == -1) {
			/* server doesn't know about this command
			** just return -1 and set errno and hope
			** that user program can handle that
			*/
			setoserror( EOPNOTSUPP );
			return(-1);
		}
		if (_rmt_command(fildes, "Q\n") == -1 ||
		    (rc = _rmt_status(fildes)) == -1)
			return(-1);

		for (; rc > 0; rc -= cnt, arg += cnt)
		{
			cnt = read(READ(fildes), arg, rc);
			if (cnt <= 0)
			{
				_rmt_abort(fildes);
				setoserror( EIO );
				return(-1);
			}
		}
		return(0);

	   case MTIOCGETBLKSIZE:
		if (server_version == -1) {
			/* server doesn't know about this command
			** so just default to use 400 of 512 bytes block
			** this is the default value that our tape driver
			** return
			*/
			*(int *)arg = 400;
			return(0);
		}
		if (_rmt_command(fildes, "B\n") == -1 ||
		    (rc = _rmt_status(fildes)) == -1)
			return(-1);

		for (; rc > 0; rc -= cnt, arg += cnt)
		{
			cnt = read(READ(fildes), arg, rc);
			if (cnt <= 0)
			{
				_rmt_abort(fildes);
				setoserror( EIO );
				return(-1);
			}
		}
		return(0);
#endif

	   default:
		setoserror( EINVAL );
		return(-1);
	}
  }
#endif /* RMTIOCTL */

