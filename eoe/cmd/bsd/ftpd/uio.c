#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#define	ENOUGH	(64<<10)
#define	min(a,b)	((a) < (b) ? (a) : (b))
extern	int debug, odirect, big_pipe;

/* keep this a function */
int	max(int a, int b)
{
    return (a > b ? a : b);
}

/*
 * For sockets, return ENOUGH.  Let the disk open it up if it wants to.
 */
ssize_t
blksize(int fd)
{
    int	bsize = odirect ? odirect : ENOUGH;
    struct	stat sbuf;

    if ((fstat(fd, &sbuf) == -1) || !S_ISREG(sbuf.st_mode)) {
	/* here if fd is a socket */
	if (debug) {
	    syslog(LOG_DEBUG,
		    "blksize=%d\n", big_pipe ? big_pipe : ENOUGH);
	}
	return (big_pipe ? big_pipe : ENOUGH);
    }
    if (sbuf.st_size) {
	bsize = min(bsize, sbuf.st_size);
    }
    if (odirect) {
	struct	dioattr dio;
	int	flags;


	flags = fcntl(fd, F_GETFL, 0);
	if (!(flags & FDIRECT)) {
	    flags |= FDIRECT;
	    fcntl(fd, F_SETFL, flags);
	}
	if (fcntl(fd, F_DIOINFO, &dio) == -1) return (bsize);

	bsize = min(bsize, dio.d_maxiosz);

    }
    if (!bsize) {	/* XXX - any way for this to happen? */
	bsize = ENOUGH;
    }
    return (bsize);
}

ssize_t
splice(int from, int to)
{


    int	a, b, tmp, flags;
    size_t moved = 0;
    short to_isreg   = 0;
    short from_isreg = 0;
    struct stat sbuf;
    struct dioattr dio;
    short toggle_to_fdirect   = 0;
    short toggle_from_fdirect = 0;

    int	bsize = max(blksize(from), blksize(to));
    char *buf =  (char *) valloc(bsize);
    
    /* uuhh?? why is this here??? */
    /* fchmod(bsize, 0); */

    if (!buf)
	return (-1);
	
    if (odirect) {

	if (fstat(from, &sbuf) == -1) {
		free(buf);
		return (-1);
	}
	    
	if (S_ISREG(sbuf.st_mode)) {
	    fcntl(from, F_DIOINFO, &dio);
	    from_isreg = 1;
	}
	else {

	    if (fstat(to, &sbuf) == -1) {
		free(buf);
		return(-1);
	    }

            if (S_ISREG(sbuf.st_mode)) {
                if (fcntl(to, F_DIOINFO, &dio) == -1) {
			free(buf);
			return(-1);
		}
                to_isreg = 1;
	    }
	}
    }

    /*
     * We want to get all the bytes from either fd before moving to the
     * other fd.  Otherwise we screwup XFS something awful.
     */
    
     
    if (odirect && from_isreg &&
	    (sbuf.st_size == bsize) && (bsize % dio.d_miniosz)) {

	toggle_from_fdirect = 1;
	flags = fcntl(from, F_GETFL, 0);
	flags &= ~FDIRECT;
	fcntl(from, F_SETFL, flags);
    }

    for ( ;; ) {


	for (a = 0; a < bsize; a += tmp) {

	    tmp = read(from, buf + a, bsize - a);
	    if (tmp == -1) {
		free(buf);
		return (-1);
	    }
	    if (!tmp) {
		break;
	    }
	}

	if (odirect && to_isreg && (a % dio.d_miniosz))  {
	    toggle_to_fdirect = 1;
	}

	for (b = 0; b < a; b += tmp) {
	   if (toggle_to_fdirect) {
		flags = fcntl(to, F_GETFL, 0);
		flags &= ~FDIRECT;
		fcntl(to, F_SETFL, flags);
	    }

	    tmp = write(to, buf + b, a - b);
	    if (tmp == -1) {
		free(buf);
		return (-2);
	    }
	}
	 
	if (!a) {
	    break;
	}
	moved += a;
    }



    if (odirect) {
	/* lets restore the original state */
	if (toggle_from_fdirect) {
	    flags |= FDIRECT;
	    fcntl(from, F_SETFL, flags);
	}
	else if (toggle_to_fdirect) {
	    flags |= FDIRECT;
	    fcntl(to, F_SETFL, flags);
	}
    }  

    free(buf);
    if (!moved && (a == -1 || b == -1))  return (-1);

    return (moved);
}

