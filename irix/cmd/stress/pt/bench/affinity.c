/* Purpose: test intelligence of kernel-level and pthread schedulers wrt
 * memory affinity.
 * 
 * We create a number of threads. Each thread gets a set of memory blocks,
 * which it spends its life accessing. Some of these blocks might be shared.
 * 
 * We have the option of using pthreads, or sproc's; we also have the option of
 * using the mmci interfaces to place our memory.
 * 
 * NB: this test must be run on an SN0 machine, or any other supporting the
 * mmci interfaces.
 * 
 * TODO: Allow touch_block to read, write, or both.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/mman.h>
#include <sys/sysmp.h>
#include <fcntl.h>
#include <invent.h>
#include <mutex.h>
#include "hrn.h"
#include "sthr.h"

HRN_DECLARE_LIBCAP(HRN_LIBCAP_ALL);

/* command-line parameters */
#define DEFAULT_NBLOCKS 32
static int	nblocks = DEFAULT_NBLOCKS;	/* total number of blocks */
static int	blocks_per_thread = 1;		/* number of blocks / thread */
static int	do_mlds = 1;			/* use mmci stuff */
static int	blocksz = 0x4000;		/* block size */
static int	nthreads = 1;
static int 	sertouch;
static int	blocksync;
static int	nmlds;
static char*	policy_name = "PlacementRoundRobin";/* placement policy name */
static topology_type_t tplgy = TOPOLOGY_FREE;

typedef struct {
	char*		b_mem;
	sthr_mutex_t	b_mtx;
} memblock_t;

/* incoming parameters to each thread. */
typedef struct {
	memblock_t	**t_mem;	/* this thread's blocks */
	pmo_handle_t	*t_mlds;	/* this thread's mlds */
	pmo_handle_t	t_mldset;
} thread_args_t;

/* internal prototypes */
static void threadbody (void* arg);
static int memblock_init(memblock_t* b, char* mem);
static void memblocks_destroy(memblock_t* b);
static void memblock_destroy(memblock_t* b);
static void touch_block(memblock_t* b);
static int print_mem(memblock_t* block);
static memblock_t* targs_init (hrn_args_t* ha, thread_args_t* targs, char* mem);
static void targs_destroy(thread_args_t* targs);
static void print_mld(pmo_handle_t mld);
static void mlds_destroy(void);

static sthr_barrier_t	bar;

/* mmci globals */
static pmo_handle_t	*mlds, mldset;


/*
 * Obtain an appropriately sized region of memory; divvy it up among the
 * threads; start the threads.
 */
int hrn_main (hrn_args_t* ha)
{
	int 		ii, zerofd;
	sthr_t 		*threads;
	void		*bigregion;	/* a big region of memory */
	thread_args_t 	*targs;
	memblock_t	*mem;

	/* allocate threads and thread args */
	if (ha->a_nthreads)
		nthreads = ha->a_nthreads;
	targs =	NEWV (thread_args_t, nthreads);
	threads = NEWV (sthr_t, nthreads);
	
	/* map a large-enough region of memory */
	ChkInt(zerofd = open("/dev/zero", O_RDWR), >= 0);
	bigregion = mmap(0, nblocks * blocksz,
			 PROT_READ | PROT_WRITE, MAP_PRIVATE,
			 zerofd, 0);

	trc_info("mmap: 0x%p\n", bigregion);
	if (bigregion == MAP_FAILED) {
		perror("mmap");
		exit(-1);
	}

	/* set up thread args, mlds, etc. */
	mem = targs_init(ha, targs, bigregion);

	/* create threads */
	sthr_barrier_init(&bar, nthreads+1);
	for (ii = 0; ii < nthreads; ii++) {
		trc_info("arg: 0x%p\n", targs + ii);
		ChkInt(hrn_barrier_start (&bar, threads+ii, 0, 
					  threadbody, targs + ii), == 0);
	}
	sthr_barrier_join(&bar);
	
	/* join all threads */
	for (ii = 0; ii < nthreads; ii++) {
		sthr_join(threads +ii);
	}

	/* 
	 * Now print out memory; we do this to prevent interfering with memory
	 * placement by touching memory first from the main thread.
	 */
	for (ii = 0; ii < nblocks; ii++) {
		print_mem(mem+ii);
	}

	targs_destroy(targs);
	memblocks_destroy(mem);
	mlds_destroy();
	free(threads);

	sthr_barrier_destroy(&bar);

	ChkInt(munmap (bigregion, nblocks * blocksz), == 0);
	return 0;
}

/*
 * Each thread expresses an affinity for its assigned block of memory,
 * and touches its pages repeatedly.
 */
static void threadbody (void* arg)
{
	thread_args_t*	args = (thread_args_t*) arg;
	policy_set_t	policy_set;
	pmo_handle_t 	pm;
	int		ii;
	char		accstr[256];

	/* cache values of globals in registers */
	register blocks_per_thread_ = blocks_per_thread;
	register blocksz_ = blocksz;
	register nthreads_ = nthreads;
	register blocksync_ = blocksync;

	trc_info("awake; args == 0x%p\n", arg);
	if (do_mlds) {
		/* It's not clear that this is the right thing to do; there 
		 * are more than one mld that we have an affinity for. Oh
		 * well... */
		process_mldlink(getpid(), args->t_mlds[0], RQMODE_ADVISORY);
	}


	/* access our different blocks, one after another. */
	START_TIMER;
	if (sertouch) {
		for (ii = 0; ii < blocks_per_thread_; ii++) {
			int jj;
			char str[256];

			sprintf(str, "b0x%p", args->t_mem[ii]->b_mem);
			if (blocksync_)
				ChkInt(sthr_mutex_lock(&args->t_mem[ii]->b_mtx),
				       == 0);
			START_TIMER;
			for (jj=0; jj < 10000; jj++)  {
				touch_block(args->t_mem[ii]);
			}
			END_TIMER(str);
			if (blocksync_)
			ChkInt( sthr_mutex_unlock(&args->t_mem[ii]->b_mtx), 
				== 0);
		}
	}
	else {
		for (ii = 0; ii < 10000; ii++) {
			touch_block(args->t_mem[ii%blocks_per_thread_]);
		}
	}
	END_TIMER("touch");

}

/* 
 * Intialize a memblock
 */
static int memblock_init(memblock_t* b, char* mem)
{
	b->b_mem = mem;
	return sthr_mutex_init(&b->b_mtx);
}

/* 
 * destroy a memblock 
 */
static void memblock_destroy(memblock_t* b)
{
	sthr_mutex_destroy(&b->b_mtx);
}

/* 
 * Touch a block. This reads and writes the entire block 
 */
static void touch_block(memblock_t* b)
{
	memmove(b->b_mem, b->b_mem+1, blocksz-1);
}

/* 
 * Print out the node an mld was placed on 
 */
static void print_mld(pmo_handle_t mld)
{
	dev_t node_dev;
	char devname[128];
	int len = 128;
	
	node_dev = __mld_to_node(mld);
	trc_info("mld: 0x%08x; dev: %s\n", mld,
	    dev_to_devname(node_dev, devname, &len));
}

/* 
 * Fill in the argument structure for each thread; return memblocks 
 */
static memblock_t* targs_init (hrn_args_t* ha, thread_args_t* targs, char* mem)
{
	int ii, bcount=0;
	memblock_t	*memblocks;
	
	/* 
	 * we want one mld for each node in the system, or one for each
	 * thread, whichever is least
	 */
	if (do_mlds) {
		int 		numnodes;
		pmo_handle_t	mldset;
		policy_set_t	policy;
		pmo_handle_t	pm;

		if (! nmlds) {
			numnodes = sysmp(MP_NUMNODES);
			nmlds = (numnodes > nthreads)? nthreads : numnodes;
		}

		mlds = NEWV(pmo_handle_t, nmlds);

		for (ii=0; ii < nmlds; ii++) {
			mlds[ii] = mld_create(0, blocksz);
			trc_info("mlds[%d]: 0x%08x\n", ii, mlds[ii]);
		}

		/* place the mlds in an mldset; attach a policy module */
		mldset = mldset_create (mlds, nmlds);
		mldset_place(mldset, tplgy, 0, 0, RQMODE_ADVISORY);
		/* show where the mlds were placed */
		for (ii = 0; ii < nmlds; ii++) {
			trc_info("ii: %d\n");
			print_mld(mlds[ii]);
		}

		/* cover the entire region with one policy module */
		pm_filldefault(&policy);
		policy.placement_policy_name = policy_name;
		policy.placement_policy_args = (void*)mldset;
		pm = pm_create(&policy);

		pm_attach(pm, mem, nblocks * blocksz);
	}

	/* now the memblocks-- each block of memory gets one */
	memblocks = NEWV(memblock_t, nblocks);
	for (ii = 0; ii < nblocks; ii++) {
		memblock_init(memblocks+ii,
			      mem + ii * blocksz);
	}

	/* 
	 * do some preliminary sanity-checking on our settings-- make no more
	 * memory blocks than we need to 
	 */
	if (nthreads * blocks_per_thread < nblocks)
		nblocks = nthreads * blocks_per_thread;

	/* now we start handing out blocks to threads round-robin */
	trc_info("init_pages: nthreads == %d\n", nthreads);
	for (ii=0; ii < nthreads; ii++) {
		int jj;
		targs[ii].t_mem = NEWV(memblock_t*, blocks_per_thread);
		targs[ii].t_mlds = NEWV(pmo_handle_t, blocks_per_thread);

		for (jj=0; jj < blocks_per_thread; jj++, bcount++) {
			bcount %= nblocks;
			trc_vrb("bcount: %d\n", bcount);
			targs[ii].t_mem[jj] = &memblocks[bcount];
			if (do_mlds)
				targs[ii].t_mlds[jj] = mlds[bcount];
		}
	}

	return memblocks;
}

/*
 * Deallocate all mmci-related resources
 */
static void mlds_destroy(void)
{
	int 	ii;
	if (do_mlds) {
		for (ii=0; ii < nmlds; ii++) {
			mld_destroy(mlds[ii]);
		}
	}
	free(mlds);

	mldset_destroy(mldset);
}

/*
 * Deallocate all resources hanging off of targs
 */
static void targs_destroy(thread_args_t* targs)
{
	int ii;

	for (ii=0; ii < nthreads; ii++) {
		free(targs[ii].t_mem);
		free(targs[ii].t_mlds);
	}
	free(targs);
}

static void memblocks_destroy(memblock_t* b)
{
	int 	ii;
	for (ii=0; ii < nblocks; ii++) {
		memblock_destroy(b+ii);
	}

	free(b);
}


/* 
 * Print out placement information for the given block. Assumes this
 * block has already been faulted in elsewhere.
 */
static int print_mem(memblock_t* block)
{
	pm_pginfo_t  pg;
	char buf[256];
	int len = 256;

	if ( __pm_get_page_info(block->b_mem,
				blocksz, &pg, 1) == -1)
		return -1;

	dev_to_devname(pg.node_dev, buf, &len);
	trc_info("block 0x%p on %s\n", block->b_mem, buf);

	return 0;
}

DEF_PARSER(p_domain, nblocks = atoi(arg))
DEF_PARSER(p_mlds, do_mlds = 0)
DEF_PARSER(p_blocksz, blocksz = atoi(arg))
DEF_PARSER(p_blocks_per_thread, blocks_per_thread = atoi(arg))
DEF_PARSER(p_sertouch,  sertouch =1)
DEF_PARSER(p_blocksync,  blocksync = 1)
DEF_PARSER(p_nmlds, nmlds = atoi(arg))
DEF_PARSER(p_policy, policy_name = arg)

/* cmd-line wack */
hrn_option_t hrn_options[] = {
	{ "D:", "set size of shared domain to #\n",
		"this sets the size, in pages, of the domain that the\n"
		"threads in the test access.\n",
		p_domain },
	{ "B:", "set block size to #\n",
		"This sets the size of the blocks used in the test.\n",
		p_blocksz },
	{ "T:", "use # blocks per thread\n",
		"This sets the number of pages each thread is allocated\n",
		p_blocks_per_thread },
	{ "N", "Disable MLD usage\n",
		"By default, this test uses the mmci interfaces to specify \n"
		"NUMA placement parameters. This option turns off the mmci \n"
		"calls.",
		p_mlds },
	{ "S", "Serialize block usage\n",
		"By default, each thread will spread its accesses over time\n"
		"evenly over its set of blocks. If this option is specified,\n"
		"each thread will first use its first block, then its second,\n"
		"etc., never repeating a block.\n",
		p_sertouch },
	{ "X", "Make memory usage exclusive\n",
		"make threads lock a block before using it.\n",
		p_blocksync },
	{ "M:", "set number of mld's\n",
		"explicitly sets the number of mlds used. By default, we use\n"
		"one mld for each node in the system,\n",
		p_nmlds },
	{ "P:", "set placement policy to #\n",
		"This option takes as an argument the name of the placement \n"
		"policy to use, as described in the mmci(5) manpage\n",
		p_policy },
	{0,0,0,0}
};

hrn_doc_t hrn_doc = {
"Affinity",
"Measure sensitivity to memory affinity",
"for each thread\n"
"	allocate blocks_per_thread blocks from memory pool;\n"
"\n"
"\neach thread {\n"
"	for each block for this thread {\n"
"		read and write entire block;\n"
"	}\n"
"}\n"
"\nThe command-line options offer many variations on this basic structure.\n",
"Time for each block access; total time per thread.\n"
};

