/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
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
 *
 *	$Ordinal-Id: io_init.c,v 1.32 1996/12/23 23:49:23 chris Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "merge.h"
#include "nsorterrno.h"
#include <fcntl.h>
#include <sys/cachectl.h>
#include <sys/mman.h>
#include <sys/sysmp.h>

unsigned	Print_task;
sort_t		*OTC_Latest;
sys_t		NsortSys;

/*
 * obtain_aio_fds - obtain file descriptors for the input/output/temp aiocbs
 *
 *	The fileinfo_t passed in already has been opened to the file.
 *	We prefer to open() the file again, rather than duplicating
 *	this file descriptor: unixes have been known to serialize i/o if
 *	the same file table entry is used many times in a process, whereas
 *	if the file is truly opened again this serialization does not occur.
 *	We also open() the file to give each file descriptor a separate kernel
 *	file position entry, so that seeks on one don't affect the others.
 *	XXX require that the dup() cases have only 1 aiocount XXX
 *
 *	The openmode mode is set to -1 if the input file name is not known
 *	(e.g. standard input).
 */
int obtain_aio_fds(sort_t *sort, aiocb_t aiocbs[], int count, 
		   fileinfo_t *file, parse_error_t err)
{
    int	i;
    int	fd;

    /* Open a new file descriptor for each aiocb
     * This needs to be done each time chk_input() starts a new input file.
     */
    for (i = 0; i < count; i++)
    {
	if (i == 0)
	    fd = file->fd;
	else if (file->openmode != -1)
	{
	    if ((fd = open(file->name, file->openmode)) < 0)
	    {
		runtime_error(sort, err, file->name, strerror(errno));
		return (TRUE);
	    }
	}
	else if ((fd = dup(fd)) < 0)
	    die("duplicating aio fd #%d of %s failed: %s", i, file->name, strerror(errno));
	if (aiocbs[i].aio_fildes != -1)
	    close(aiocbs[i].aio_fildes);
	aiocbs[i].aio_fildes = fd;
	/* mapped file reading needs to know the file size, so that
	 * it can easily determine the EOF position in the mapping buffer
	 */
	aiocbs[i].aio_filesize = file->stbuf.st_size;
    }
    return (FALSE);
}

/*
 * close_aio_fds	- close each file descriptor in each aiocb
 *
 * This needs to be done each time chk_input starts a new input file.
 */
void close_aio_fds(sort_t	*sort,
		  aiocb_t	aiocbs[],
		  int		count,
		  char		*name,
		  parse_error_t	err)
{
    int	i;

    for (i = 0; i < count; i++)
    {
	if (aiocbs[i].aio_fildes != -1 && close(aiocbs[i].aio_fildes) < 0)
	    runtime_error(sort, err, name, strerror(errno));
	aiocbs[i].aio_fildes = -1;
    }
}

/*
 * aiocb_alloc - allocate and initialize as 'unused' a number of aiocbs
 *
 */
aiocb_t	*aiocb_alloc(int num_aios)
{
    aiocb_t	*aiocbs;

    aiocbs = (aiocb_t *) calloc(num_aios, sizeof(aiocb_t));
    if (aiocbs == NULL)
	die("can't allocate %d aiocbs", num_aios);
    while (num_aios != 0)
	aiocbs[--num_aios].aio_fildes = -1;
    return (aiocbs);
}

/*
 * init_aio	- prepare for aync reading of the first input file
 *
 *	The aiocbs for the output and any working files are built
 *	in setup_pass1_io() and setup_final_io()
 */
void init_aio(sort_t *sort)
{
    int		i;

    if (!sort->internal)
    {
	IN.aiocb = aiocb_alloc(IN.aio_cnt);
	OUT.aiocb = aiocb_alloc(OUT.aio_cnt);
	if (!sort->api_invoked)
	    obtain_aio_fds(sort, OUT.aiocb, OUT.aio_cnt, OUT.file, NSORT_OUTPUT_OPEN);
    }

    for (i = 0; i < TEMP.n_files; i++)
    {
	TEMP.files[i].aiocb = aiocb_alloc(TEMP.aio_cnt);
	obtain_aio_fds(sort, TEMP.files[i].aiocb, TEMP.aio_cnt, 
		       &TEMP.files[i].fileinfo, NSORT_OUTPUT_OPEN);
    }
}

/* unmap_region -	remove the (single) region used by this sort
 *			from this process's region map
 */
void unmap_region(byte *start, ptrdiff_t size)
{
    int		i;
    region_t	*region;
    int		err;

    if (start == NULL)
	return;

    for (i = 0, region = SYS.regions; i != SYS.n_regions; i++, region++)
    {
	if (region->base == start && region->size >= size)
	{
	    err = munmap(region->base, region->size);
	    if (err)
		die("unmap_region(%08x, %d) failed: %s", region->base,
			region->size, strerror(errno));
	    if (Print_task)
		fprintf(stderr, "unmap_region(%08x, %dK)\n", region->base,
							     region->size / 1024);
	    SYS.n_regions--;
	    SYS.region_kbytes -= region->size / 1024;
	    memmove(region, region + 1, (SYS.n_regions - i) * sizeof(*region));
	    return;
	}
    }
    die("unmap_region: didn't find 0x%x:%d\n", start, size);
}


/*
 * map_io_buf	- allocate a huge chunk of memory for input, items, and output
 *
 *	Reserves a big piece of memory by privately mmap'ing /dev/zero.
 *	The layout of this memory is as listed in the relevent fields
 *	of the sort descriptor indicate, i.e.
 *	    rec_start		- starts the read buffer
 *	    rec_end/refuge		- starts the refugee area
 *	    refuge_end/ll_free	- starts the per-sproc free linelists
 *	    out_buf			- starts the output buffers
 *
 *	Caveat:
 *		The assignment of memory here must correspond with the
 *		calculations performed by calc_mem_use()
 */
void map_io_buf(sort_t *sort)
{
    byte	*begin_region;
    in_run_t	*run;
    ptrdiff_t	i;
    byte	*p;
    ptrdiff_t	size;
    ptrdiff_t	maxsize;
    int		maxregion;
    ptrdiff_t	diff;
    byte	*rp;
    ptrdiff_t	region_size;
    ptrdiff_t	region_cnt;
    ptrdiff_t	size_mapped;
    ptrdiff_t	done_size;
    
    if (sort->check)
    {
	IN.read_size = 1024 * 1024;
	size = IN.aio_cnt * IN.read_size;
    }
    else
	size = sort->end_out_buf - sort->begin_in_run;

    /*
     * The record region must be aligned to a page boundary
     * for the later mmap() and to a dio_mem boundary for reading the input
     */
    size = ROUND_UP_NUM(size, SYS.page_size);
    size = ROUND_UP_NUM(size, IN.dio_mem);

    if (sort->api_invoked)
    {
	/* See if this process already has a large enough region mapped in
	 * Pick the smallest one which is big enough
	 */
	maxsize = 0;
	if (!sort->auto_free && SYS.n_regions != 0)
	{
	    for (i = 0; i != SYS.n_regions; i++)
	    {
		if (SYS.regions[i].size >= size && !SYS.regions[i].busy &&
		    (maxsize == 0 || SYS.regions[i].size < maxsize))
		{
		    maxsize = SYS.regions[i].size;
		    maxregion = i;
		}
	    }
	}
	
	if (maxsize != 0)
	{
	    sort->region = &SYS.regions[maxregion];
	    sort->region->busy = TRUE;
	    p = sort->region->base;
	}
	else
	{
	    /* Allocate a region with one contiguous mmap()
	     * But first, if it appears that are more regions than active sorts
	     * free the idle regions.
	     *  XXX put hysteresis in here, to free after a region is long idle
	     */
	    if (SYS.n_regions >= SYS.n_sorts)
	    {
		for (i = 0; i < SYS.n_regions; )
		    if (SYS.regions[i].busy)
			i++;
		    else
			unmap_region(SYS.regions[i].base, SYS.regions[i].size);
	    }

	    /* Find out where this much address space can be mapped. A client
	     * app could have shared memory segments or mapped files all 
	     * anywhere, so we have to let the kernal decide where to put the
	     * memory.
	     */
	    p = mmap((void *) 0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
		     SYS.zero_fd, (off_t) 0);
	    if (p == (byte *) -1)
		runtime_error(sort, NSORT_MMAP_ZERO_FAILED, size,
							    strerror(errno));

	    SYS.n_regions++;
	    SYS.region_kbytes += size / 1024;
	    SYS.regions = (region_t *)
		realloc(SYS.regions, SYS.n_regions * sizeof(region_t));
	    sort->region = &SYS.regions[SYS.n_regions - 1];
	    sort->region->base = p;
	    sort->region->size = size;
	    sort->region->busy = TRUE;
	}
	SYS.end_touched = p + size;
	SYS.touch_avail = p + size;
    }
    else	/* else this is the sort program we are allocating for */
    {
	/* map everything in one large region
	 */
	if (getenv("SBRK") == 0)
	    p = mmap((void *) 0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
		     SYS.zero_fd, (off_t) 0);
	else
	    p = sbrk(size + SYS.page_size);
	if (p == (byte *) -1)
	    runtime_error(sort, NSORT_MMAP_ZERO_FAILED, size, strerror(errno));
	p = ROUND_UP(p, SYS.page_size);
	
	/* calculate number of regions necessary for swiss cheese mmap()ing
	 */
	region_cnt =
	    ROUND_UP_DIV(size, SYS.max_region_size + sizeof(unsigned));

	/* if there would only be one region, or we don't want to touch
	 * memory in parallel, then leave mapping as is.
	 */
	if (region_cnt <= 1 || sort->touch_memory != FlagTrue)
	{
	    SYS.end_touched = p + size;
	    SYS.touch_avail = p + size;
	}
	else
	{
	    /* unmap the entire the one contiguous region we just mapped,
	     * then "swiss cheese" mmap() it to allow parallel zeroing of
	     * pages.  A one-page hole between regions prevents the 6.3
	     * Irix kernel from collapsing adjacent regions into one big
	     * region, then only allowing serial zeroing in the region.
	     */
	    
	    /* unmap previous mmap()ing
	     */
	    if (munmap(p, size) == -1)
		die("munmap(%x, %x) failed: %s\n", p, size, strerror(errno));

	    /* determine number of bytes needed to hold region_done array
	     */
	    done_size =
		ROUND_UP_COUNT(region_cnt * sizeof(unsigned), SYS.page_size);

	    size_mapped = 0;
	    for (i = 0; i < region_cnt; i++)
	    {
		/* the region size is the minimum of the maximum region
		 * size and the number of bytes left to map
		 */
		region_size = SYS.max_region_size;
		if (region_size > (size - done_size) - size_mapped)
		    region_size = (size - done_size) - size_mapped;

		if (i == 0)
		{
		    /* if first segment, don't make a page hole at beginning
		     */
		    rp = mmap(p, region_size, PROT_READ | PROT_WRITE,
			     MAP_FIXED | MAP_PRIVATE, SYS.zero_fd, (off_t) 0);
		    if (rp != p)
			die("mmap size %x, off %x failed: %s",
			    region_size, 0, strerror(errno));
		}
		/*
		 * else we need to make a page-sized hole at the beginning of
		 * the segment to prevent the 6.3 kernel from collapsing
		 * adjacent segments together, thereby disallowing parallel
		 * allocation of pages between the two segments.
		 *
		 * map the part of the segment past the hole if it exists
		 * (i.e. the segment is bigger than a page).
		 */
		else if (region_size > SYS.page_size)
		{
		    begin_region = p + size_mapped + SYS.page_size;
		    rp = mmap(begin_region, region_size - SYS.page_size,
			      PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE,
			      SYS.zero_fd, (off_t) 0);
		    if (rp != begin_region)
			die("mmap size %x, off %x failed: %s",
			    region_size - SYS.page_size, 
			    size_mapped + SYS.page_size, strerror(errno));
		}
		size_mapped += region_size;
	    }

	    /* map alloc_done array
	     */
	    begin_region = p + size_mapped;
	    SYS.done_array = (mpcount_t *)(p + size_mapped);
	    rp = mmap(begin_region, done_size,
		      PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE,
		      SYS.zero_fd, (off_t) 0);
	    if (rp != begin_region)
		die("done mmap size %x, off %x failed: %s", done_size,
		    size_mapped, strerror(errno));

	    SYS.end_touched = p;
	    SYS.touch_avail = p;
	}
	SYS.n_regions++;
	SYS.region_kbytes += size / 1024;
	SYS.regions = (region_t *)
	    realloc(SYS.regions, SYS.n_regions * sizeof(region_t));
	sort->region = &SYS.regions[SYS.n_regions - 1];
	sort->region->base = p;
	sort->region->size = size;
	sort->region->busy = TRUE;
    }
	

    diff = p - sort->begin_in_run;
    sort->begin_in_run = sort->next_pin = sort->pinned = p;
    if (sort->check)
    {
	sort->end_in_run = p + size;
	sort->end_out_buf = (byte *) ROUND_UP(sort->end_in_run, SYS.page_size);
    }
    else
    {
	for (run = sort->in_run; run < sort->in_run + N_IN_RUNS; run++)
	{
	    run->read_buf += diff;
	    run->ll_free = (item_line_t *)((byte *)run->ll_free + diff);
	    run->refuge_end = (item_line_t *)((byte *)run->refuge_end + diff);
	    run->refuge_next = (item_line_t *)((byte *)run->refuge_next + diff);
	    run->refuge_start = 
		(item_line_t *)((byte *)run->refuge_start + diff);

	    /* Internal sorts have just one run containing all the data */
	    if (sort->internal)
		break;
	}

	if (sort->hash_tables != NULL)
	    sort->hash_tables = (item_t **)((byte *)sort->hash_tables + diff);

	sort->end_in_run += diff;
	
	if (sort->method == SortMethRecord)
	{
	    if (!sort->internal)
		MERGE.rec_bufs += diff;
	    for (i = 0; i < MERGE.merge_cnt; i++)
		MERGE.rob[i].tree = 
		    (node_t *)((byte *)MERGE.rob[i].tree + diff);
	}
	else
	{
	    /* relocate ptr partitions
	     */
	    for (i = 0; i < MERGE.merge_cnt; i++)
	    {
		if (!sort->internal)
		    MERGE.ptr_part[i].ptrs += diff;
		MERGE.ptr_part[i].tree = 
		    (node_t *)((byte *)MERGE.ptr_part[i].tree + diff);
	    }

	    /* relocate output buffers
	     */
	    if (!sort->internal)
	    {
		OUT.buf[0] += diff;

		/* initialize the block headers of the out_blk structs from the
		 * same memory as the output file buffers.  (for record sorts
		 * the out_blk head pointers will vary dynamically)
		 */
		for (i = 0; i < TEMP.out_blk_cnt; i++)
		    TEMP.out_blk[i].head = 
			(blockheader_t *)(OUT.buf[0] + (i * TEMP.blk_size));
	    }
	}

	sort->end_out_buf += diff;
    }

    TIMES.mmap_done = get_time() - TIMES.begin;
}


void touch_mem(sort_t *sort, unsigned sproc_id)
{
    byte	*curr, *next_curr, *p;
    byte	*new, *old, *end;
    unsigned	start_time;
    unsigned	i;
    unsigned	touch_sum = 0;

    /* while the originating process has yet determined the address of the 
     * sort- buffer, take naps.
     */
    while (sort->begin_in_run == NULL)
	ordnap(1, "touch_mem");

    start_time = get_time();
    /* while entire buffer is not yet allocated.
     */
    while ((curr = SYS.touch_avail) < sort->end_out_buf)
    {
	/* figure new allocation position assuming we get the assignment
	 * to allocate the next region.
	 */
	next_curr = curr + SYS.max_region_size;
	if (next_curr >= (byte *)SYS.done_array)
	    next_curr = (byte *)sort->end_out_buf;

	/* if we don't get the assignment, try again.
	 */
	if (uscas(&SYS.touch_avail,
		  (ptrdiff_t)curr, (ptrdiff_t)next_curr, SYS.ua) == 0)
	{
	    continue;
	}

	if (Print_task)
	    fprintf(stderr, "sproc %d touching 0x%08x...\n", sproc_id, curr);

	/* write a word on each page in region to get physical pages allocated
	 */

	/* if first region, there is not a page hole at the beginning
	 */
	if (curr == sort->begin_in_run)
	    p = curr;
	else
	    p = curr + SYS.page_size;

	end = next_curr;
	if (end > (byte *)SYS.done_array)
	    end = (byte *)SYS.done_array;

	while (p < end)
	{
	    touch_sum += *p;
	    p += SYS.page_size;
	}

        /* mark word in done_array that indicates this region has been
         * successfully allocated.
         */
        SYS.done_array[(curr - sort->begin_in_run) / SYS.max_region_size] = 1;

	/* get index of first region not yet covered by end_touched
	 */
        i = (SYS.end_touched - sort->begin_in_run) / SYS.max_region_size;

	/* while index is within the number of regions AND the index's
	 * region has already been touched
	 */
        while (&sort->begin_in_run[i * SYS.max_region_size] <
	       (byte *)SYS.done_array &&
	       SYS.done_array[i])
	{
	    /* while the index's region has been touched but no sproc has
	     * taken responsibility for incrementing SYS.end_touched past
	     * the region.
	     */
	    while (SYS.done_array[i] == 1)
	    {
		/* if can successfully take responsibility for incrementing
		 * SYS.end_touched past the region.
		 */
		if (uscas(&SYS.done_array[i],
			  (ptrdiff_t)1, (ptrdiff_t)2, SYS.ua))
		{
		    /* if this is the first region, don't fill in page hole
		     */
		    if (i == 0)
			break;
		    
		    if (Print_task)
			fprintf(stderr, "sproc %d filling 0x%08x...\n",
				sproc_id,
				&sort->begin_in_run[i * SYS.max_region_size]);

		    /* fill in the hole page
		     */
		    p = mmap(&sort->begin_in_run[i * SYS.max_region_size],
			     SYS.page_size,
			     PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE,
			     SYS.zero_fd, 0);
		    if (p != &sort->begin_in_run[i * SYS.max_region_size])
			die("hole mmap(0x%08x, %08x...) returned %08x",
				&sort->begin_in_run[i * SYS.max_region_size],
				SYS.page_size, p);

		    touch_sum += *p;

		    break;
		}
	    }
            i++;
	}

        /* calculate new value for SYS.end_touched pointer and keep trying to
         * update it until either we are successful, or some other process
         * bumps it up even higher.
         */
        new = sort->begin_in_run + (i * SYS.max_region_size);
	if (new >= (byte *)SYS.done_array)
	    new = sort->end_out_buf;
        while (new > (old = (byte *)SYS.end_touched))
	{
            if (uscas(&SYS.end_touched,
		      (ptrdiff_t)old, (ptrdiff_t)new, SYS.ua))
	    {
		if (new == sort->end_out_buf)
		    TIMES.touch_done = get_time() - TIMES.begin;
                break;
	    }
	}
	
    }

    /* assign the sum somewhere so that touches don't get optimized away
     */
    SYS.touch_sum = touch_sum;
    
    atomic_add(&TIMES.touch_cpu, get_time() - start_time, SYS.ua);
}

void unpin_range(byte *start, byte *end)
{
    ptrdiff_t	npages;
    int		pagesize = getpagesize();

    while (start < end)
    {
	npages = (start - end) / pagesize;
	if (npages > REGION_SIZE)
	    npages = REGION_SIZE;
	if (munpin(start, npages) != 0)
	{
	    break;
	}
	start += npages * pagesize;
    }
}

void prepare_shared(sort_t *sort)
{
    aioinit_t	aioinit;
    int		old;
    int		out_threads;
    int		all_main = TRUE;
    int		i;
    char	tstr[32];

    /* if not single process mode
     */
    if (!sort->single_process)
    {
	memset(&aioinit, 0, sizeof(aioinit));
	
	/* if we are doing synchronous writes, there aren't any output threads.
	 */
	if (OUT.file->io_mode == IOMODE_SERIAL)
	    out_threads = 0;
	else
	    out_threads = OUT.aio_cnt;
	
	/* threads: # of i/o's passable to the kernel
	 * locks: # sprocs which call aio_suspend() (only the i/o sproc does)
	 * numusers:# total sprocs in this sort - (XXX what to do in lib vers?)
	 *				          GET_USERS and take all avail?
	 */
	aioinit.aio_threads = TEMP.aio_cnt * TEMP.n_files + out_threads;
	
	if (IN.aio_cnt > out_threads)
	{
	    for (i = 0; i != IN.n_files; i++)
	    {
		if (!(IN.files[i].io_mode == IOMODE_MAPPED ||
		      IN.files[i].io_mode == IOMODE_SERIAL))
		{
		    all_main = FALSE;
		    break;
		}
	    }
	    if (!all_main)
		aioinit.aio_threads += IN.aio_cnt - out_threads;
	}
	
	aioinit.aio_locks = 1;
	
	aioinit.aio_numusers = 1 + sort->n_sprocs + aioinit.aio_threads;
	
	/* declare enough sprocs for the sum of the io sproc, the sorting 
	 * sprocs, input aio sprocs, and output aio sprocs.
	 */
	if ((old = usconfig(CONF_INITUSERS, aioinit.aio_numusers)) < 0)
	    die("usconfig (CONF_INITUSERS) was %d: %s", old, strerror(errno));

	if (Print_task)
	    fprintf(stderr,
		    "aio_sgi_init:threads %d locks %d numusers %d @ %s\n",
		    aioinit.aio_threads,
		    aioinit.aio_locks,
		    aioinit.aio_numusers, get_time_str(sort, tstr));
	
	aio_sgi_init(&aioinit);
    }

    /* mmap both the input record buf and output bufs.
     */
    map_io_buf(sort);
}

void get_rminfo(void)
{
    struct rminfo rm;

    if (sysmp(MP_SAGET, MPSA_RMINFO, &rm, sizeof(rm)) < 0)
	die("sysmp(MP_SAGET, MPSA_RMINFO) failed: %s", strerror(errno));
    SYS.buffer_cache_sz = rm.chunkpages * SYS.page_size;
    SYS.free_swap = rm.availsmem * (u8) SYS.page_size;
    SYS.free_memory = (rm.freemem * (u8) SYS.page_size) * 0.95;	/* leave 5%? */
    SYS.user_data = (u8) SYS.page_size * (rm.availrmem -
					  (rm.strmem + rm.bufmem + rm.pmapmem));
#if (_MIPS_SZLONG == 32)
    /* A 32-bit program in a 64-bit O.S. might not be able use all mem
     */
    if (SYS.free_memory > (u8) 1 << 31)
	SYS.free_memory = (u8) 1 << 31;
    if (SYS.user_data > (u8) 1 << 31)
	SYS.user_data = (u8) 1 << 31;
    if (SYS.free_swap > (u8) 1 << 31)
	SYS.free_swap = (u8) 1 << 31;
#endif
}

/* sort_alloc	- allocate an new, empty sort structure
 *
 *	Any memory allocated here must not be freed except by sort_free()
 *		OUT.file
 *		sort->statistics (a.k.a. STATS)
 *		TEMP.build
 */
sort_t *sort_alloc(void)
{
    sort_t	*sort;
    int		i;
    extern int	Ordinal_Main;
#if defined(HAS_DPLACEFILE)
    void dplace_file(char *line);
#endif

    if (SYS.page_size == 0)
    {
	Ordinal_Main = getpid();
#if defined(HAS_DPLACEFILE)
	if (access("dplacefile", R_OK) == 0)	/* really, is dplace avail? */
	{
	    dplace_file("dplacefile");
	    SYS.page_size = 4096 * 1024;
	}
	else
#endif
	    SYS.page_size = getpagesize();
	SYS.aio_max = (u2) sysconf(_SC_AIO_MAX);
	SYS.n_processors = (u1) prctl(PR_MAXPPROCS);
	get_rminfo();
	SYS.zero_fd = open("/dev/zero", O_RDWR);
	/* Get shared arena for interprocess synchronization.
	 * using /dev/zero makes a hidden arena: it is not in the filesystem
	 * but is available only to this process and its sprocs
	 */
	SYS.ua = usinit("/dev/zero");
	SYS.max_region_size = (SYS.user_data > 100000 * 1024) ? REGION_SIZE
							      : REGION_SIZE / 2;
    }

    sort = (sort_t *) malloc(sizeof(sort_t));
    memset(sort, 0, sizeof(sort_t));
    TIMES.begin = get_time();
    PARSE.lineno = 1;

    IN_RUN_PARTS = 0;

#if defined(DEBUG1)
    sort->check_ll = getenv("NO_CHECK_LL") == NULL;
#endif

    OUT.file = malloc(sizeof(fileinfo_t));
    memset(OUT.file, 0, sizeof(*OUT.file));
    OUT.file->fd = -1;
    sort->statistics = (sortstat_t *) malloc(sizeof(sortstat_t));
    memset(sort->statistics, 0, sizeof(sortstat_t));

    IN.aio_cnt = 2;
    IN.read_size = (1 << 17);

    TEMP.aio_cnt = 2;
    TEMP.blk_size = (1 << 17);
    TEMP.advance = 1;
    TEMP.build_cnt = 256;
    TEMP.build = (tbuf_t **) malloc(TEMP.build_cnt * sizeof(tbuf_t *));

    MERGE.part_ptr_cnt = MERGE_PART_RECS_MAX;
    MERGE.part_recs = MERGE.part_ptr_cnt;

    OUT.aio_cnt = 2;
    OUT.buf_size = (1 << 17);
    COPY.target_size = (1 << 17);

    sort->touch_memory = FlagUnspecified;

    /* Add this sort to this applications current table of them
     * Reuse the first previously-used-and-freed slot if there is one
     */
    for (i = 0; i < SYS.n_sorts; i++)
	if (SYS.sorts[i] == NULL)
	    break;
    if (i == SYS.n_sorts)
    {
	SYS.n_sorts = i + 1;
	SYS.sorts = (sort_t **) realloc(SYS.sorts,
					SYS.n_sorts * sizeof(sort_t *));
    }

    SYS.sorts[i] = sort;
    sort->nth = i;
    OTC_Latest = sort;
    return (sort);
}

void read_defaults(sort_t *sort)
{
    char	*s;

    if (access("/usr/nsort/nsort.params", R_OK) == 0)
	 process_specfile(sort, "/usr/nsort/nsort.params");

    if (s = getenv("NSORT"))
	parse_string(sort, s, "NSORT environment variable");
}

/* rm_temps	- eradicate all traces of any temporary files used by this sort
 *
 *	This may be called only by the i/o sproc - other sprocs might not be
 *	sharing file descriptors (i.e. their sproc() might not have PR_SFDS)
 */
void rm_temps(sort_t *sort)
{
    unsigned	i;
    extern int Ordinal_Main;

#if 0
    if (getpid() == sort->client_pid)	/* XXX change to != io_sproc?? */
    {
	fprintf(stderr, "rm_temps: call by client %d i/o %d\n",
			sort->client_pid, sort->io_sproc);
	return;
    }
#endif

#if defined(DEBUG1)
    if (sort->n_errors != 0)
	fprintf(stderr, "rm_temps: pid %d has %d to try\n", getpid(), TEMP.n_files);
#endif

    for (i = 0; i < TEMP.n_files; i++)
    {
	/* Complain that we cannot remove the temp file unless it is
	 * already missing. Another nsort sproc may have cleaned it up.
	 */
	if (TEMP.files[i].rm_on_close &&
	    unlink(TEMP.files[i].fileinfo.name) < 0
#if !defined(DEBUG1)
	    && errno != ENOENT
#endif
	    )
	    runtime_error(sort, NSORT_UNLINK_FAILED,
				TEMP.files[i].fileinfo.name,
				strerror(errno));

	TEMP.files[i].rm_on_close = FALSE;
	if (TEMP.files[i].aiocb)
	{
	    close_aio_fds(sort, TEMP.files[i].aiocb,
				TEMP.aio_cnt,
				TEMP.files[i].fileinfo.name,
				NSORT_CLOSE_FAILED);
	    CONDITIONAL_FREE(TEMP.files[i].aiocb);
	}

	CONDITIONAL_FREE(TEMP.files[i].fileinfo.name);
    }
    CONDITIONAL_FREE(TEMP.files);

    TEMP.n_files = 0;
}

/* reset_sort	- restore the fields of a sort to be ready for prepare_sort_def
 *
 *	The items handled here need to be reinitalized when a sort is re-run;
 *	they do not change the sort definition whatsoever.
 */
void reset_sort(sort_t *sort)
{
    unsigned	i;

    if (sort->statistics)
    {
	CONDITIONAL_FREE(STATS.backwards_histogram);
	CONDITIONAL_FREE(STATS.radixcount_histogram);
	CONDITIONAL_FREE(STATS.usages);
	memset(sort->statistics, 0, sizeof(sortstat_t));
    }
    for (i = 0; i != N_IN_RUNS; i++)
    {
	sort->in_run[i].eof = FALSE;
	sort->in_run[i].final_run = FALSE;
	sort->in_run[i].scan_complete = FALSE;
    }
    memset(&TIMES, 0, sizeof(TIMES));
    sort->input_phase = TRUE;
    sort->output_phase = FALSE;
    sort->touch_memory = FlagUnspecified;
    TIMES.begin = get_time();
    IN.reads_issued = 0;
    IN.reads_done = 0;
    IN.begin_run_offset = 0;
    IN.begin_reads_issued = 0;
    IN.eof_remainder = FALSE;
    IN.runs_init = 0;
    IN.runs_read = 0;
    IN.runs_sorted = 0;
    IN.runs_filled = 0;
    IN.runs_write_sched = 0;
    IN.part_avail = 0;
    IN.part_taken = 0;
    IN.part_scanned = 0;
    IN.target_size = sort->in_run_data;
    MERGE.skip_done = FALSE;
    MERGE.merge_avail = 0;
    MERGE.merge_taken = 0;
    MERGE.merge_done = FALSE;
    MERGE.merge_scanned = 0;
    MERGE.tails_done = FALSE;
    MERGE.tails_scanned = 0;
    MERGE.recs_consumed = 0;
    MERGE.width = 0;
    MERGE.end_of_merge = FALSE;
    COPY.bufs_planned = 0;
    TEMP.writes_issued = 0;
    TEMP.writes_done = 0;
    TEMP.writes_final = 0;
    TEMP.blks_filled = 0;
    TEMP.optimal_reads = FALSE;
    OUT.buf = NULL;
    OUT.end_io_output = FALSE;
    OUT.end_sort_output = FALSE;
    OUT.writes_issued = 0;
    OUT.writes_done = 0;
    OUT.rob_writes_issued = 0;
    OUT.rob_writes_done = 0;
    OUT.writes_final = 0;

    sort->n_errors = 0;
}

/*
 * free_resources	- release any sprocs, memory, etc, except those
 *			  which are needed to `keep' the sort definition
 *
 *	This frees all the /dev/zero 'mapped' memory, as well as
 *	all memory which depends on:
 *	- the method of sorting
 *	- the number, type, and/or size of the input records
 *	- the number of sorting sprocs
 */
void free_resources(sort_t *sort)
{
    unsigned	i;

    for (i = 0; i < N_IN_RUNS; i++)
    {
	CONDITIONAL_FREE(sort->in_run[i].partitions);
	CONDITIONAL_FREE(sort->in_run[i].part_lists);
    }

    CONDITIONAL_FREE(TEMP.build);
    CONDITIONAL_FREE(TEMP.out_blk);
    CONDITIONAL_FREE(TEMP.run);

    if (MERGE.rob)
    {
	if (!sort->internal)
	    for (i = 0; i < MERGE.merge_cnt; i++)
		CONDITIONAL_FREE(MERGE.rob[i].rec_outs);
	CONDITIONAL_FREE(MERGE.rob);
    }
    CONDITIONAL_FREE(MERGE.run_list);
    CONDITIONAL_FREE(MERGE.ptr_part);

    CONDITIONAL_FREE(COPY.copy);

    /* Unmap the largest part of memory, which was set up by map_io_buf()
     */
    unmap_region(sort->begin_in_run, sort->end_out_buf - sort->begin_in_run);
    sort->begin_in_run = NULL;
    sort->end_out_buf = NULL;
}

/* free_files	- close all files, discard their names and associated info
 *
 */
void free_files(sort_t *sort)
{
    int	i;

    if (IN.curr_file >= IN.files && IN.curr_file < IN.files + IN.n_files)
	close_aio_fds(sort, IN.aiocb, IN.aio_cnt,
		      IN.curr_file->name, NSORT_CLOSE_FAILED);
    for (i = 0; i < IN.n_files; i++)
    {
	CONDITIONAL_FREE(IN.files[i].name);
    }
    CONDITIONAL_FREE(IN.files);
    CONDITIONAL_FREE(IN.aiocb);
    IN.n_files = 0;
    if (OUT.file->fd != -1)
    {
	close_aio_fds(sort, OUT.aiocb, OUT.aio_cnt, OUT.file->name, NSORT_CLOSE_FAILED);
	CONDITIONAL_FREE(OUT.file->name);
	OUT.file->fd = -1;
    }
    CONDITIONAL_FREE(OUT.aiocb);

    if (TEMP.n_files != 0)
	rm_temps(sort);
}

/* reset_definition	- forget about keys, fields, sorting options, ...
 */
void reset_definition(sort_t *sort)
{
    int	i;

    for (i = 0; i < RECORD.fieldcount; i++)
    {
	if (FIELD_NAMED(RECORD.fields[i].name))
	    CONDITIONAL_FREE(RECORD.fields[i].name);
	if (RECORD.fields[i].flags & FIELD_DERIVED)
	{
	    CONDITIONAL_FREE(RECORD.fields[i].derivation->value);
	    CONDITIONAL_FREE(RECORD.fields[i].derivation);
	}
    }
    CONDITIONAL_FREE(RECORD.fields);

    for (i = 0; i < sort->keycount; i++)
	if (FIELD_NAMED(sort->keys[i].name))
	    CONDITIONAL_FREE(sort->keys[i].name);
    CONDITIONAL_FREE(sort->keys);

    CONDITIONAL_FREE(sort->edits);
    CONDITIONAL_FREE(sort->ovc_init);
    PARSE.lineno = 1;
}

void sort_free(sort_t *sort)
{
    int	context;

    context = sort->nth;
    ASSERT(SYS.sorts[context] == sort);

    free_resources(sort);
    CONDITIONAL_FREE(sort->statistics);

    free_files(sort);

    CONDITIONAL_FREE(OUT.file);

    /* Shrink the sort array size if we can (i.e. this was the last entry)
     */
    SYS.sorts[context] = NULL;
    if ((context + 1) == SYS.n_sorts)
	while (SYS.n_sorts != 0 && SYS.sorts[SYS.n_sorts - 1] == NULL)
	    SYS.n_sorts--;

#if defined(DEBUG1)
    memset(sort, 0xfb, sizeof(*sort));
#endif
    free(sort);
}

void verify_temp_file(sort_t *sort)
{
    /* if no temporary files were specified and we haven't yet created
     * a default one, create default file.
     */
    if (TEMP.n_files == 0)
    {
	add_temp_file(sort, "/tmp");
	TEMP.files[0].aiocb = aiocb_alloc(TEMP.aio_cnt);
	obtain_aio_fds(sort, TEMP.files[0].aiocb, TEMP.aio_cnt, 
		       &TEMP.files[0].fileinfo, NSORT_OUTPUT_OPEN);
    }
}

void suspend_others(void)
{
    int		self = getpid();
    int		i;
    int		pid;

    for (i = 0; i < SYS.n_sprocs_cached; i++)
    {
	if ((pid = SYS.sprocs[i].sproc_pid) != 0 && pid != self)
	    kill(pid, SIGSTOP);
    }
}

void nsort_cleanup(void)
{
    int	i;

    Ordinal_Cleanup = NULL;
    for (i = 0; i != SYS.n_sorts; i++)
    {
	unpin_range(SYS.sorts[i]->begin_in_run, SYS.sorts[i]->end_out_buf);
	rm_temps(SYS.sorts[i]);
    }
}
