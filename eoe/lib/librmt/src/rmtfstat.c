#ident	"$Header: /proj/irix6.5.7m/isms/eoe/lib/librmt/src/RCS/rmtfstat.c,v 1.9 1997/08/06 23:34:40 prasadb Exp $"

#include "rmtlib.h"

#include <errno.h>
#include <sgidefs.h>
#include <sys/types.h>
#include <sys/stat.h>

static int _rmt_fstat(int, char *);

/*
 *	Get file status.  Looks just like fstat(2) to caller.
 */
 
int rmtfstat (fildes, buf)
int fildes;
struct stat *buf;
{
	if (isrmt (fildes))
	{
		return (_rmt_fstat (fildes - REM_BIAS, (char *)buf));
	}
	else
	{
		int i;
		i = fstat(fildes, buf);
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
		return i;
#else
		if (i == 0 || errno != EOVERFLOW)
			return i;
		else {
			struct stat64 st64;
			fstat64(fildes, &st64);
			buf->st_dev = st64.st_dev;
			buf->st_ino = (ino_t)st64.st_ino;
			buf->st_mode = st64.st_mode;
			buf->st_nlink = st64.st_nlink;
			buf->st_uid = st64.st_uid;
			buf->st_gid = st64.st_gid;
			buf->st_rdev = st64.st_rdev;
			buf->st_size = (off_t)st64.st_size;
			buf->st_atim = st64.st_atim;
			buf->st_mtim = st64.st_mtim;
			buf->st_ctim = st64.st_ctim;
			buf->st_blksize = st64.st_blksize;
			buf->st_blocks = (blkcnt_t)st64.st_blocks;
			bcopy(st64.st_fstype, buf->st_fstype, _ST_FSTYPSZ);
			return 0;
		}
#endif
	}
}

static int
_rmt_fstat(int fildes, char *arg)
{
	char buffer[ BUFMAGIC ];
	int rc, cnt, adj_rc;

	if (server_version == -1) {
		/* server doesn't know about this command
		** just return -1 and set errno and hope
		** that user program can handle that
		*/
		setoserror( EOPNOTSUPP );
		return(-1);
	}
		
	sprintf( buffer, "Z%d\n", fildes );

	/*
	 *	grab the status and read it directly into the structure
	 *	this assumes that the status buffer is (hopefully) not
	 *	padded and that 2 shorts fit in a long without any word
	 *	alignment problems, ie - the whole struct is contiguous
	 *	NOTE - this is probably NOT a good assumption.
	 */

	if (_rmt_command(fildes, buffer) == -1 ||
	    (rc = _rmt_status(fildes)) == -1)
		return(-1);

	/* adjust read count to prevent overflow */

	adj_rc = (rc > sizeof(struct stat)) ? sizeof(struct stat) : rc ;
	rc -= adj_rc;

	for (; adj_rc > 0; adj_rc -= cnt, arg += cnt)
	{
		cnt = read(READ(fildes), arg, adj_rc);
		if (cnt <= 0)
		{
abortit:
			_rmt_abort(fildes);
			setoserror( EIO );
			return(-1);
		}
	}

	/* handle any bytes we didn't know what to do with */
	while (rc-- > 0)
		if (read(READ(fildes), buffer, 1) <= 0)
			goto abortit;

	return(0);
}
