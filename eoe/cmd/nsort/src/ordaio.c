/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved         *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 * $Ordinal-Id: ordaio.c,v 1.5 1996/11/05 19:54:08 charles Exp $
 */

#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include <sys/mman.h>
#include <sys/procfs.h>

/*
 *	Ordinal Replacement for aio_read, aio_write, aio_error, and aio_return
 *	This version is intended to behave identically to SGI's, except that
 *	Ordinal's suite permits any process to post a request
 *	(SGI's apparently limits reliable aio_read/aio_writes to the main sproc)
 *
 *	aio_read
 *	aio_write
 *	aio_error
 *	aio_return
 *	aio_suspend	?
 *	aio_cancel	?
 *
 *	Would have liked to have a single AioSproc perform LIO_NOWAIT requests
 *	if lio_listio didn't use so much of the standard aio_* code.
 *
 *	Simplifying conditions:
 *		In the nsort api only the temporary files are read and written.
 *		Temp files are 'local' to a sort, so two different i/o sprocs
 *		will not be attempting to queue i/o's to the same temp file, so
 *		we don't have to worry about the lseek file pointer being
 *		changed by two simulataneous aio requests, and causing the first
 *		request to read/write at the second request's offset, e.g.:
 *			i/o sproc A	i/o sproc B
 *			lseek off1
 *					lseek off2
 *			read (gets off2)
 *					read (gets off2 + A's read size)
 *		
 *		This is important, as SGI does not appear to offset an atomic
 *		'seek-and-read/write' primitive outside of the aio_*&lio_* pkgs.
 *		Thus we do not have to single-thread requests to a file
 *		(doing so would be made more work because several different file
 *		 descriptors can point to the same file & share the same offset)
 *
 *	We still need SMP locking for managing the requests. 
 */

typedef struct aio_state
{
    usptr_t	*arena;		/* shared arena for ordinal aio sprocs */
    usema_t	*sem;		/* aio_daemons sleep here between requests */
    aiocb_t	**aios;		/* circular array of posted aiocb_t *'s */
    mpcount_t	next_post;	/* next place to which an aiocb_t * will go */
    mpcount_t	next_aio;	/* next place from which an aiocb_t * comes  */
    short	n_aios;		/* max # of requests postable (posted's space)*/
    short	n_daemons;	/* # of daemons overall */
    int		*daemons;	/* aio server sprocs */
} aio_state_t;

aio_state_t	AioState;

const char *OrdAioOpcodeNames[] =
{
	"bad aiocmd 0",
	"read",
	"write",
	"nop",
	"map readonly",
	"map copyonwrite",
	"map read/write"
};

/* ordaio_daemon	- handle aio requests in order
 *
 *	This handles
 *		seek and read
 *		seek and write
 *		map a portion of a file into memory read-only
 *		map a portion of a file into memory read-write, private
 *	????	map a portion of a file into memory read-write, shared, autogrow
 *
 */
void ordaio_daemon(aio_state_t *state, size_t stack)
{
    int		ret;
    int		next_aio;
    aiocb_t	*aio;
    void	*mapped;
    byte	*p;
    byte	*p_end;
    int		touch_sum;

#if defined(DEBUG1)
    blockproc(getpid());
#endif

    /* Do prctl(TERMSIG) to exit from the loop when the client app exits
     */
    exit_with_parent();

    for (;;)
    {
	uspsema(state->sem);

	/* Since there is one V() per aio posted, there ``must'' be a aio
	 * for us for perform.  We still need to cas() to find out which one
	 * it is, because several aios could have been posted simulataneously
	 * and each aio_daemon() really ought to pick a different one to do.
	 */
	do
	{
	    next_aio = state->next_aio;
	    if (next_aio == state->next_post)
		die("aio_daemon:no requests:%d", state->next_post);
	    aio = state->aios[next_aio % state->n_aios];
	} while (!uscas(&state->next_aio, next_aio, next_aio+1, state->arena));

	if (aio == NULL)
	    die("ordaio_daemon: aio available, but null");

	if (aio->aio_lio_opcode >= LIO_FIRSTMAP)
	{
	    switch (aio->aio_lio_opcode)
	    {
	      case LIO_MAPRDONLY:
		mapped = (void *) mmap((void *) aio->aio_buf, 
				       aio->aio_nbytes,
				       PROT_READ,
				       MAP_FIXED | MAP_SHARED,
				       aio->aio_fildes,
				       aio->aio_offset);
		break;

	      case LIO_MAPCOPYONWRITE:
		mapped = (void *) mmap((void *) aio->aio_buf, 
				       aio->aio_nbytes,
				       PROT_READ | PROT_WRITE,
				       MAP_FIXED | MAP_PRIVATE,
				       aio->aio_fildes,
				       aio->aio_offset);
		break;

#if 0
	      case LIO_MAPTOUCH:
		mapped = aio->aio_buf;
		break;
#endif
	    }

	    if (mapped != aio->aio_buf)
	    {
		if (errno == ENODEV || errno == ENOSYS)
		    ret = -1;		/* this object can't be mmapped */
		else
		    die("ordaio: map failed %x, %d @ %lld got %x: %s",
			aio->aio_buf, aio->aio_nbytes,
			aio->aio_offset, mapped, strerror(errno));
	    }
	    else
	    {
		if (aio->aio_nbytes > aio->aio_filesize - aio->aio_offset)
		{
		    ret = aio->aio_filesize - aio->aio_offset;
		    /* If the map was so short as to leave a hole in the
		     * address space, map the hole with /dev/zero, avoid faults.
		     */
		    if (ret + SYS.page_size <= aio->aio_nbytes)
		    {
			int	size;
			byte	*start;
			
			size = aio->aio_nbytes-ROUND_UP_NUM(ret,SYS.page_size);
			start = (byte *) aio->aio_buf + aio->aio_nbytes - size;

			if (mmap((void *) start,
				 size,
				 PROT_READ | PROT_WRITE,
				 MAP_FIXED | MAP_PRIVATE,
				 SYS.zero_fd, 0) == (void *) -1)
			    die("ordaio: remap /dev/zero 0x%x %d failed: %s",
				start, size, strerror(errno));
		    }
		}
		else
		    ret = aio->aio_nbytes;
		
		for (p = (byte *) mapped, p_end = p + ret, touch_sum = 0;
		     p < p_end;
		     p += SYS.page_size)
		    touch_sum += *p;

		/* assign the sum so that touches don't get optimized away
		 */
		SYS.touch_sum = touch_sum;
	    }
	}
	else if (aio->aio_lio_opcode == LIO_FSYNC)
	{
	    ret = fsync(aio->aio_fildes);
	    if (ret == 0)
		ret = aio->aio_nbytes;
	}
	else	/* this operation is of the seek-and-(read,write) variety */
	{
#if TRUE || defined(_SGIAPI)
	    /* IRIX 6.2+? has a combined seek-and-read/write system call. This
	     * must match with get_dio_info()'s decision of setting aio_count.
	     */
	    if (aio->aio_lio_opcode == LIO_READ)
		ret = pread(aio->aio_fildes, (void *) aio->aio_buf,
			    aio->aio_nbytes, aio->aio_offset);
	    else
		ret = pwrite(aio->aio_fildes, (void *) aio->aio_buf,
			     aio->aio_nbytes, aio->aio_offset);
#else
	    ret = lseek64(aio->aio_fildes, aio->aio_offset, SEEK_SET);
	    if (ret != -1 || errno == ESPIPE) /* ignore illegal-seek-on-pipe err */
	    {
		if (aio->aio_lio_opcode == LIO_READ)
		    ret = read(aio->aio_fildes, (void *) aio->aio_buf,
						aio->aio_nbytes);
		else
		    ret = write(aio->aio_fildes, (void *) aio->aio_buf,
						 aio->aio_nbytes);
	    }
#endif
	}
#if 1
	if (Print_task)
	    fprintf(stderr, "aiod %d fd %d %s %d bytes @ %lld buf %x returns %d %s\n",
			    getpid(), aio->aio_fildes,
			    OrdAioOpcodeNames[aio->aio_lio_opcode],
			    aio->aio_nbytes, aio->aio_offset, aio->aio_buf,
			    ret, (ret < 0) ? strerror(errno) : "");
#endif
	aio->aio_ret = ret;

	/* Lastly change errno from EINPROGRESS to mark the aio as completed
	 */
	aio->aio_errno = (ret < 0) ? errno : 0;
    }
}

int post_aio(aiocb_t *aio)
{
    aio_state_t	*state = &AioState;
    mpcount_t	next_post;

    aio->aio_ret = -1;
    aio->aio_errno = EINPROGRESS;

    if (state->arena == NULL)
    {
       aioinit_t aioinit;

        memset(&aioinit, 0, sizeof(aioinit));
        /* threads: # of i/o's passable to the kernel
         * locks:   # sprocs which call aio_suspend() (only the i/o sproc does)
         * numusers:# total sprocs in this sort - (XXX what to do in lib vers?)
         */
        aioinit.aio_threads = 3;
        aioinit.aio_locks = 5;
        aioinit.aio_numusers = 1 + 5/* XXX a guess */ + aioinit.aio_threads;
        aio_sgi_init(&aioinit);
    }

    do
    {
	next_post = state->next_post;
	if (next_post == state->next_aio + state->n_aios)
	{
#if defined(DEBUG2)
	    fprintf(stderr, "post_aio: off %d waiting for slot @ %d\n",
			    aio->aio_offset, next_post);
#endif
	    while (next_post == state->next_aio + state->n_aios)
	    {
		ordnap(1, "post_aio");
	    }
#if defined(DEBUG2)
	    fprintf(stderr, "post_aio: off %d wait done slot @ %d next %d\n",
			    aio->aio_offset, next_post, state->next_aio);
#endif
	}
    } while (!uscas(&state->next_post, next_post, next_post + 1, state->arena));
    state->aios[next_post % state->n_aios] = aio;

    /* Wake up exactly one of the the aio_daemon() sprocs to handle this request
     */
    usvsema(state->sem);
    return (0);
}

int ordaio_suspend(aiocb_t * const aiocb[], int count, const timespec_t *lim)
{
    ASSERT(count == 1);
    while (aiocb[0]->aio_errno == EINPROGRESS)
	ordnap(1, "aio_suspend");
    return (0);
}

int ordaio_error(const aiocb_t *aio)
{
    return (aio->aio_errno);
}

ssize_t ordaio_return(aiocb_t *aio)
{
    return (aio->aio_ret);
}

/* ordaio_read	- Initiate an asynchronous read into an aiocb_t
 */
int ordaio_read(aiocb_t *aio)
{
    aio->aio_lio_opcode = LIO_READ;
    return (post_aio(aio));
}

/* ordaio_read	- Initiate an asynchronous write from an aiocb_t
 */
int ordaio_write(aiocb_t *aio)
{
    aio->aio_lio_opcode = LIO_WRITE;
    return (post_aio(aio));
}

/* ord_read	- Synchronously read using an aiocb_t
 *		  The aio_offset is IGNORED!
 *		  Always read from the current position!
 */
int ord_read(aiocb_t *aio)
{
    int	ret;

    ret = read(aio->aio_fildes, (void *) aio->aio_buf, aio->aio_nbytes);
    aio->aio_ret = ret;
    aio->aio_errno = (ret < 0) ? errno : 0;
    return (0);
}

/* ord_write	- Synchronously write using an aiocb_t
 *		  The aio_offset is IGNORED!
 *		  Always writes to the current position!
 */
int ord_write(aiocb_t *aio)
{
    int	ret;

    ret = write(aio->aio_fildes, (void *) aio->aio_buf, aio->aio_nbytes);
    aio->aio_ret = ret;
    aio->aio_errno = (ret < 0) ? errno : 0;
    return (0);
}

/* ordaio_map_rdonly	- 
 *
 */
int ordaio_map_rdonly(aiocb_t *aio)
{
    aio->aio_lio_opcode = LIO_MAPRDONLY;
    return (post_aio(aio));
}

int ordaio_map_copyonwrite(aiocb_t *aio)
{
    aio->aio_lio_opcode = LIO_MAPCOPYONWRITE;
    return (post_aio(aio));
}

int ord_map_copyonwrite(aiocb_t *aio)
{
    void	*mapped;
    int		ret;

    mapped = (void *) mmap((void *) aio->aio_buf, 
			   aio->aio_nbytes,
			   PROT_READ | PROT_WRITE,
			   MAP_FIXED | MAP_PRIVATE,
			   aio->aio_fildes,
			   aio->aio_offset);
    if (mapped != aio->aio_buf)
    {
	if (errno == ENODEV || errno == ENOSYS)
	    ret = -1;		/* this object can't be mmapped */
	else
	    die("ordaio: map failed %x, %d @ %lld got %x: %s",
		aio->aio_buf, aio->aio_nbytes,
		aio->aio_offset, mapped, strerror(errno));
    }
    else if (aio->aio_nbytes > aio->aio_filesize - aio->aio_offset)
    {
	ret = aio->aio_filesize - aio->aio_offset;
	/* If the map was so short as to leave a hole in the
	 * address space, map the hole with /dev/zero, avoid faults.
	 */
	if (ret + SYS.page_size <= aio->aio_nbytes)
	{
	    int		size;
	    byte	*start;
	    
	    size = aio->aio_nbytes - ROUND_UP_NUM(ret, SYS.page_size);
	    start = (byte *) aio->aio_buf + aio->aio_nbytes - size;

	    if (mmap((void *) start,
		     size,
		     PROT_READ | PROT_WRITE,
		     MAP_FIXED | MAP_PRIVATE,
		     SYS.zero_fd, 0) == (void *) -1)
		die("ordaio: remap /dev/zero 0x%x %d failed: %s",
		    start, size, strerror(errno));
	}
    }
    else
	ret = aio->aio_nbytes;
    aio->aio_ret = ret;

    /* Lastly change errno from EINPROGRESS to mark the aio as completed
     */
    aio->aio_errno = (ret < 0) ? errno : 0;

    return (0);
}

int ordaio_fsync(aiocb_t *aio)
{
    /* Every 16MB we flush the output file
     * XXX Tie amount to output file speed, #stripes, ...?
     */
    if ((aio->aio_offset >> 24) != ((aio->aio_offset + aio->aio_nbytes) >> 24))
    {
	aio->aio_lio_opcode = LIO_FSYNC;
	return (post_aio(aio));
    }
    else
    {
	aio->aio_ret = aio->aio_nbytes;
	aio->aio_errno = 0;
	return (0);
    }
}

int ordaio_nop(aiocb_t *aio)
{
    aio->aio_ret = aio->aio_nbytes;
    aio->aio_errno = 0;
    return (0);
}

int ordaio_map_rdwrite(aiocb_t *aio)
{
    aio->aio_lio_opcode = LIO_MAPRDWRITE;
    return (post_aio(aio));
}

int ordaio_cancel(int fd, aiocb_t *aio)
{
    return (AIO_NOTCANCELED);
}

void ordaio_sgi_init(aioinit_t *init)
{
    aio_state_t	*state = &AioState;
    int		i;
    int		aiothreads;

    if (state->arena != NULL)
	return;

    if ((state->arena = usinit("/dev/zero")) == NULL)
	die("ordaio_agi_init: usinit failed: %s", strerror(errno));

    if (init == NULL)
	aiothreads = 5;
    else
	aiothreads = init->aio_threads;
    state->n_daemons = aiothreads;
    state->n_aios = aiothreads * 2;

    state->sem = usnewsema(state->arena, 0);
    state->aios = (aiocb_t **) malloc(state->n_aios * sizeof(aiocb_t **));
    state->next_post = 0;
    state->next_aio = 0;
    state->daemons = (int *) malloc(state->n_daemons * sizeof(int));
    for (i = 0; i != state->n_daemons; i++)
    {
	state->daemons[i] = sprocsp((void (*)(void *arg, size_t)) ordaio_daemon,
				    PR_SADDR | PR_SFDS /*| PR_NOLIBC*/,
				    (void *) state,
				    (caddr_t) NULL, STK_SZ);
	if (state->daemons[i] < 0)
	    die("aio_ordinal_init: can't start aio #%d: %s", i, strerror(errno));
#if defined(DEBUG1)
	/* new proc blocks itself so we can attach */
	unblockproc(state->daemons[i]);
#endif
    }
}

void ordaio_getrusage(struct rusage *total)
{
    struct prusage	prusage;
    char		procname[30];
    int			fd;
    int			i;

    memset(total, 0, sizeof(*total));
    for (i = 0; i != AioState.n_daemons; i++)
    {
	sprintf(procname, "/proc/pinfo/%d", AioState.daemons[i]);
	fd = open(procname, O_RDONLY);
	if (fd < 0)
	    die("ordaio_getrusage: cannot open aio sproc %d",
		AioState.daemons[i], strerror(errno));
	if (ioctl(fd, PIOCUSAGE, &prusage) < 0)
	    die("ordaio_getrusage: cannot PIOCUSAGE aio sproc %d",
		AioState.daemons[i], strerror(errno));
	total->ru_utime.tv_sec += prusage.pu_utime.tv_sec;
	total->ru_utime.tv_usec += prusage.pu_utime.tv_nsec / 1000;
	total->ru_stime.tv_sec += prusage.pu_stime.tv_sec;
	total->ru_stime.tv_usec += prusage.pu_stime.tv_nsec / 1000;
	total->ru_nvcsw += prusage.pu_vctx;
	total->ru_nivcsw += prusage.pu_ictx;
	total->ru_majflt += prusage.pu_majf;
	total->ru_minflt += prusage.pu_minf;
	total->ru_inblock += prusage.pu_inblock;
	total->ru_oublock += prusage.pu_oublock;
	if (total->ru_maxrss < prusage.pu_rss)
	    total->ru_maxrss = prusage.pu_rss;
	close(fd);
    }
}

void print_total_time(sort_t *sort, const char *title)
{
    struct prusage	prusage;
    char		procname[30];
    char		tstr[16];
    int			fd;
    int			i;
    int			selfsproc = -1, selfpid = getpid() ;
    int			user = 0, system = 0;

    for (i = 0; i != SYS.n_sprocs_cached; i++)
    {
	if (selfpid == SYS.sprocs[i].sproc_pid)
	    selfsproc = i;
	sprintf(procname, "/proc/pinfo/%d", SYS.sprocs[i].sproc_pid);
	fd = open(procname, O_RDONLY);
	if (fd < 0)
	{
#if defined(DEBUG2)
	    if (SYS.sprocs[i].sproc_pid != 0)
		fprintf(stderr, "print_total_time: cannot open sproc %d\n",
				SYS.sprocs[i].sproc_pid, strerror(errno));
#endif
	    continue;
	}

	if (ioctl(fd, PIOCUSAGE, &prusage) < 0)
	{
#if defined(DEBUG2)
	    fprintf(stderr, "print_total_time: cannot PIOCUSAGE sproc %d %s\n",
			    SYS.sprocs[i].sproc_pid, strerror(errno));
#endif
	}
	else
	{
	    user += prusage.pu_utime.tv_sec * 100;
	    user += prusage.pu_utime.tv_nsec / 10000000;
	    system += prusage.pu_stime.tv_sec * 100;
	    system += prusage.pu_stime.tv_nsec / 10000000;
	}
	close(fd);
    }

    if (selfsproc == 0)
	strcpy(procname, "main");
    else
	sprintf(procname, "sort %d", selfpid);

    fprintf(stderr, "TIMES (%s): %s u %d; s %d; idle %d; sp %d mp %d, cp %d at %s\n",
		    procname, title, user, system,
		    (get_time() - TIMES.begin) - (user + system),
		    IN.part_avail - IN.part_taken,
		    MERGE.merge_avail - MERGE.merge_taken,
		    COPY.copy_avail - COPY.copy_taken,
		    get_time_str(sort, tstr));
}
