#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "rcspriv.h"
#include "rcsbase.h"

void clearslot(unsigned long bits[], int bitno);
#define STREAM_MTIME(x)	((x) ? (x) : time(NULL))
#define STREAM_MODE(x)	((x) ? (x) : 0444) 

#define NUMSTATWORDS \
	(TEMPNAMES + sizeof(unsigned long)*8 - 1)/(sizeof(unsigned long)*8)

static char nocopynames[TEMPNAMES][MAXPATHLEN];	
static unsigned long nocopy[NUMSTATWORDS] = {0}; /* non-copied files */
static unsigned long tmpbits[NUMSTATWORDS] = {0};  /* temp file bits */

int
rcsReadInData(readMethod, inputStream, conn, opts, handle_ptr)
	int readMethod(RCSSTREAM *inputStream, void **ptr, size_t nbytes);
	RCSSTREAM *inputStream;
	RCSCONN conn;
	struct rcsStreamOpts *opts;
	RCSTMP *handle_ptr;
{
	/* Takes an input stream, and its read method.
	 * Makes a copy of the complete stream, and hands back
         * a "handle" to this snapshot copy.
	 *
	 * Return value is zero on success; negative error code.
	 *
	 * If opts->fname is provided, the input stream is a file,
	 * which the library may choose to read as needed, rather
	 * than immediately.  It is strictly advisory; the library
	 * may still choose to make an immediate copy, using the
	 * read method.
	 *
	 * The virtue of this parameter is that one less copy
	 * of the data may be made (esp. when the application
	 * is linked directly with the server code).
	 * to be made. 
	 *
	 * 
	 *  opts->mtime - the "modification time" of the stream.
	 *		This may represent the time that the stream
	 *		data were created/changed, or the current
	 *		time.  A value of zero is interpreted as
	 *		the current time.
	 *
	 *		This time may be used for such tasks as
	 *		setting the time of revision created from the
	 *		input stream, and setting the time of the RCS
	 *		archive file (if so desired and indicated using
	 *		appropriate option flags).
	 *
	 *		If fname is used, the file's st_mtime is used
	 *		as a safety check to ensure that a file has
	 *		not changed during checkin.
	 *
	 * opts->mode -	the "mode" of the stream.  The r/x bits of the
	 *		mode are used by RCS to set the corresponding
	 *		bits of a new archive file.  (For example,
	 *		shell scripts should be executable.)
	 *
	 *		The w bits of the mode are used to infer whether
	 *		the stream was created/modified by the application,
	 *		or is merely a reflection of a version already
	 *		in the archive (which has been checked out, read-only)
	 *
	 *		This latter use (checking w bits) of the mode
	 *		generally does not apply to the library.  In the
	 *		RCS commands, a checkout (output stream) may
	 *		ask the user whether to overwrite a writable file.
	 * 
	 * If fname is used, then other struct stat fields have significance:
	 * st_size -    used internally as a simple filter to see whether
	 *		the file being checked in might have changed in
	 *		the middle of the operation.  Not used unless
	 *		"fname" set.
	 *
	 * st_ino, st_dev - used to verify that the file (if "fname" is
	 *		    set) is not the same file as the archive file.
	 *
	 */



/*	Handles:
 *	    0        - TEMPNAMES-1 represent temp files (pre-read)
 *	    TMPNAMES - 2*TEMPNAMES-1 represent regular files (read linked)
 *
 *	When referencing maketemp() and related functions in rcsfnms.c,
 *	one must add TMPRESERVED as an offset to the handle (because the
 *      first few slots are reserved).
 *
 *	When referencing the non-copied filenames in "nocopynames", one
 *	must substract TEMPNAMES from the handle (because these handles
 *	are already incremented by TEMPNAMES to avoid numeric conflicts
 *	with the temp file handles).
 *
 *	For example:
 *	    handle 3 maps to mktemp(3 + TMPRESERVED)
 *	    handle 100 maps to nocopynames[100 - TEMPNAMES]
 *		(assuming 100 >= TEMPNAMES)
 */

	int handle;
	const char *cptr;
	void *vptr;
	int fd;
	size_t nbytes;

	/* No need to perform sigsetjmp, since no core library routine
	 * is called.
	 */
	if (opts->fname) {
		/* pass back a handle mapped to a range above the temps */
		handle = findslot(nocopy, TEMPNAMES);
		if (handle < 0) return handle;
		strcpy(nocopynames[handle], opts->fname);
		*handle_ptr = (RCSTMP)(handle + TEMPNAMES);
		return 0;
	}

	fd = rcsMakeTmpHdl(handle_ptr, &cptr, opts->mode);

#define MAXREAD (1 << (sizeof(size_t) * 8 - 2))
	while ((nbytes = readMethod(inputStream, &vptr, MAXREAD)) > 0) {
	  if (fd >= 0) {
	    if (write(fd, vptr, nbytes) < nbytes) {
		close(fd);
		clearslot(tmpbits, *(int *)handle_ptr);
		unlink(cptr);
		return ERR_BAD_FILEWRITE;
	    }
	  }
	}
	if (fd < 0) return ERR_BAD_FILEOPEN;

	if (opts->mtime) {	/* "now" overridden */
	    struct utimbuf times;
	    times.actime = time(NULL);
	    times.modtime = opts->mtime;
	    if (utime(cptr, &times) < 0) {
		close(fd);
		clearslot(tmpbits, *(int *)handle_ptr);
		unlink(cptr);
		return ERR_BAD_UTIME;
	    }
	}
	close(fd);
	return 0;
}


	int
rcsInitStreamOpts(opts)
	struct rcsStreamOpts *opts;
{
	/* Initialize default values for stream */
	if (!opts) return ERR_APPL_ARGS;

	memset(opts, '\0', sizeof(*opts));
	return 0;
}


/* Utilities for managing file handles on server side */
int findslot(unsigned long bits[], int maxbit)
{
	/* given a bit vector, and the number of elements, finds an
	 * available slot, sets the appropriate bit, and returns the
	 * bit index.  (For instance, if we allow 32 slots, then maxbit
	 * is 32.)
	 */
	int i, j;
	unsigned long ul;
	int bitno;

	int topword = (maxbit + sizeof(unsigned long)*8 - 1)/
			(sizeof(unsigned long)*8);

	for (i = 0; i < topword; i++) {
	    if (bits[i] != ~(unsigned long)0) {
		for (j = 0, ul = 1; j < sizeof(unsigned long) * 8; j++,ul<<=1) {
		    if (!(bits[i] & ul)) break;
		}
		if (j < sizeof(unsigned long) * 8) break;
	    }
	}

	bitno = i * sizeof(unsigned long) *8  + j;
	if (bitno >= maxbit) return ERR_OUT_OF_FILES;

	bits[i] |= 1 << j;
	return bitno;
}

void
clearslot(unsigned long bits[], int bitno)
{
	int wordno = bitno / (sizeof(unsigned long) * 8);
	bitno -= wordno * sizeof(unsigned long) * 8;
	bits[wordno] &= ~(unsigned long)((unsigned long)1 << bitno);
}

int
rcsMapTmpHdlToName(handle, name)
	RCSTMP handle;
	const char **name;
{
	/* Given a handle, return a read-only copy of the file that
	 * the handle belongs to.
	 *
	 * Returns: 0 - success
	 *	    ERR_APPL_ARGS - invalid handle (no file mapped, perhaps
	 *			    because application closed the handle)
	 */
	const char *nm;
	
	if ((int)handle < 0 || (int)handle > 2*TEMPNAMES)
		return ERR_APPL_ARGS;

	if ((int)handle < TEMPNAMES) {	/* true temp file */
	    *name = tpnames[(int)handle + TMPRESERVED];
	    return 0;
	}

	/* Just a file name copy */
	*name = nocopynames[(int)handle - TEMPNAMES];
	return 0;
}

int
rcsMakeTmpHdl(hdl, name, mode)
	RCSTMP *hdl;		/* returned handle */
	const char **name;	/* name of temp file (if name is not NULL) */
	mode_t mode;		/* mode of new file */
{
	/* create a temp file handle, optionally passing back name of
	 * temp file.  (Can otherwise be retrieved via rcsMapTmpHdlToName())
	 *
	 * Returns open file descriptor (>=0), or error code (< 0)
	 */
	int handle;
	int fd;
	const char *cptr;

	handle = findslot(tmpbits, TEMPNAMES);
	if (handle < 0) return handle;
	cptr = maketemp(handle + TMPRESERVED);

	fd = open(cptr, O_WRONLY|O_CREAT, STREAM_MODE(mode));
	if (fd < 0) {clearslot(tmpbits, handle); return ERR_BAD_FILEOPEN;}
	*hdl = (RCSTMP)handle;
	if (name) *name = cptr;
	return fd;
}

int
rcsFreeTmpHdl(hdl)
	RCSTMP hdl;
{
	/* Internal utility for unlinking file associated with a temp
	 * handle.  It assumes the caller provides a valid tmp handle,
	 * so it does no checking here.  (Used by internal code, when
	 * it discovers an error and wants to discard work, and by
	 * the API rcsCloseTmp(), which does the requisite checking.)
	 */
	if ((int)hdl >= TEMPNAMES) return -1;
	clearslot(tmpbits, (int)hdl);
	un_link(tpnames[(int)hdl + TMPRESERVED]);
	return 0;
}


