/* gcache.c - Gcache diagnostics */
/* :set ts=4 */

#ident "$Revision: 1.28 $"

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <strings.h>
#include "libsc.h"
#include "uif.h"
#include "./cache.h"

#define MAX_ERRORS			16

extern int _sidcache_size;

static int check_error(volatile unsigned long *, const char *);
static void failed(volatile unsigned long *, unsigned long, int, const char *);
static int scache1(void), scache3(void), scache4(void);
static int scache5(void), scache6(void);


/* IDE front end to the GCache tests.  Will run all tests with no args,
 * or tests can be picked indivdually.
 */
int
scache(int argc, char *argv[])
{
	int error = 0, i;
	unsigned long len;

	msg_printf(VRB, "Scache test\n");

	/* Make sure that no cache errors already happened. */
	error = check_error(0,"pre-scache check");
#if 0	/* we really do not want to do this.  (jeffs) */
	if (error) {
		msg_printf(VRB, "pre-scache test FAILED(%d)\n", error);
		return error;
	}
#endif

	if (argc > 2) {
		msg_printf(DBG, "invalid argument\n");
		return -1;
	} else if (argc == 1) {
		error = scache1();
		error += scache3();
		error += scache4();
		error += scache5();
		error += scache6();
	} else {
		for (len = strlen(argv[1]);len;len--)
			if (argv[1][len-1] < '1' || argv[1][len-1] > '6') {
				msg_printf(DBG, "invalid argument\n");
				return -1;
			}
		msg_printf(VRB, "Scache %d test\n", (i = atoi(argv[1])));
		switch(i) {
		case 1:			/* scache 1 and 2 are combined now */
		case 2:
			error = scache1();
			break;
		case 3:
			error = scache3();
			break;
		case 4:
			error = scache4();
			break;
		case 5:
			error = scache5();
			break;
		case 6:
			error = scache6();
			break;
		}
	}

	if (error)
		msg_printf(VRB, "Scache test FAILED(%d)\n", error);
	else
		msg_printf(VRB, "Scache test passed\n");

	return error;
}


/*  Runs the tag power on tests, which test both tag ram chips, not the cache
 * itself.
 */
static int
scache1(void)
{
	volatile long *tcc_gcache = (long *)PHYS_TO_K1(TCC_GCACHE);
	long gcache_orig;
	int error;

	msg_printf(DBG, " Scache1 (pon) test\n");

	__dcache_wb_inval_all();

	gcache_orig = *tcc_gcache;

	run_chandra();

	error  = scache_tag();
	error |= scache_tag2();
	error |= scache_tag3();
	error |= scache_data();
	error |= scache_data2();

	*tcc_gcache = gcache_orig;
	inval_gcache_tags();

	/* check for parity errors after it's all over */
	if (check_error(0,"scache1"))
		error++;

	return error;
}

/*  Runs some simple data through the GCache a set at a time with floating
 * point stores, which go directly from the FPU to the GCache.
 */
static int
scache3(void)
{
	volatile void *dst = (volatile void *)SCACHE_TESTADDR;
	int error=0;
	ulong oldSR;
	void *baddr;

	msg_printf(DBG, " Scache3 test\n");
	__dcache_wb_inval_all();

	/* save status register */
	oldSR = get_SR();

	/* Enable parity interrupts */
	set_SR(oldSR | SR_IBIT10 | SR_IBIT9);

	/* Used to implement this test over each set, changing the TCC_GCACHE
	 * register to only allow the requested set to be tested.  This turns
	 * out to cause cases where the tags can get messed-up (usually in
	 * set 0) we think.  We have seen failures in mfg that could be caused
	 * by this behavior.
	 */
	baddr = fp_ls(0x5555555555555555, dst, 0x200000);
	if (baddr) {
		failed(baddr,0x5555555555555555,-1,"scache3");
		error++;
	}
	baddr = fp_ls(0xaaaaaaaaaaaaaaaa, dst, 0x200000);
	if (baddr) {
		failed(baddr,0xaaaaaaaaaaaaaaaa,-1,"scache3");
		error++;
	}
	baddr = fp_ls(0x5555555555555555, dst, 0x200000);
	if (baddr) {
		failed(baddr,0x5555555555555555,-1,"scache3");
		error++;
	}
	baddr = fp_ls(0xaaaaaaaaaaaaaaaa, dst, 0x200000);
	if (baddr) {
		failed(baddr,0xaaaaaaaaaaaaaaaa,-1,"scache3");
		error++;
	}

	/* check for parity errors after it's all over */
	if (check_error(0,"scache3"))
		error++;

	flush_cache();

	/* restore status register */
	set_SR(oldSR);

	return error;
}

/*  This tests writes 2MB of patterns based on the the address pointer to
 * do an approximation of an address test.  It's impercise as to if it
 * hits off of the cache, and things may be forced out.  It used to be
 * run setting the TCC_GCACHE to disable some sets, but that is risky
 * and seems to cause problems sometimes.
 */
static int
scache4(void)
{
	register unsigned long *fillptr;
	ulong oldSR;
	int error = 0;
	int i,j;

	msg_printf(DBG, " Scache4 test\n");
	__dcache_wb_inval_all();

	/* save status register */
	oldSR = get_SR();

	/* Enable parity interrupts */
	set_SR(oldSR | SR_IBIT10 | SR_IBIT9);

	/* fill memory with inverse current address */
	fillptr = (unsigned long *)SCACHE_TESTADDR;
	for (i = _sidcache_size / (int)sizeof(unsigned long); i > 0; --i) {
		*fillptr =  ~(long)fillptr;
		if (check_error(fillptr++,"scache4")) {
			error++;
			goto err;
		}
	}

	__dcache_wb_all();

	/* now check each entry to valid data */
	fillptr = (unsigned long *)SCACHE_TESTADDR;
	for (i = _sidcache_size / (int)sizeof (unsigned long); i > 0; --i) {
		if (*fillptr != ~(long) fillptr) {
			failed(fillptr, ~(long)fillptr, j, "scache4");

			if (++error == MAX_ERRORS)
				goto err;
		}

		/* check for errors */
		if (check_error(fillptr++,"scache4")) {
  			error++;
			goto err;
		}
	}
		

	/* fill the cache in with the address */
	fillptr = (unsigned long *)SCACHE_TESTADDR;
	for (i = _sidcache_size / (int)sizeof(unsigned long); i > 0; --i) {

		*fillptr = (long)fillptr;

		if (check_error(fillptr++,"scache4")) {
			error++;
			goto err;
		}
	
	}

	/* verify each of the writes */
	fillptr = (unsigned long *)SCACHE_TESTADDR;
	for (i = _sidcache_size / (int)sizeof (unsigned long); i > 0; --i) {
		if (*fillptr != (long) fillptr  ) {
			failed(fillptr, (long)fillptr, j, "scache4");
			if (++error == MAX_ERRORS)
				goto err;
		}
	
		if (check_error(fillptr++,"scache4")) {
			error++;
			goto err;
		}

	}

err:
	/* restore status register */
	set_SR(oldSR);

	return error;
}

/* Walks a bit though each double word in the cache (approx -- does not play
 * with TCC_GCACHE since this is dangerous).
 */
static int
scache5(void)
{
	register unsigned long *fillptr, pattern;
	void *test_base = (void *)(SCACHE_TESTADDR);
	ulong oldSR;
	int i, error=0;

	msg_printf(DBG, " Scache5 test\n");
	__dcache_wb_inval_all();

	/* save status register */
	oldSR = get_SR();

	/* Enable parity interrupts */
	set_SR(oldSR | SR_IBIT10 | SR_IBIT9);

	pattern = 0x1;
	while ( pattern ) {
		fillptr = test_base;

		for (i = 0 ; i < (_sidcache_size/8); i += 32){

			*fillptr = !pattern;
			*(fillptr+8) = !pattern;
			*(fillptr+16) = !pattern;
			*(fillptr+24) = !pattern;

			*fillptr = pattern;
			*(fillptr+8) = pattern;
			*(fillptr+16) = pattern;
			*(fillptr+24) = pattern;

			if (check_error(fillptr,"scache5")) {
				error++;
				goto err;
			}

			fillptr = fillptr + 32;
		}

		__dcache_wb(test_base,_sidcache_size);

		/* Read back and check */
		fillptr = test_base;
		for (i = 0 ; i < (_sidcache_size/8); i += 32){
			if (*fillptr != pattern) {
				failed(fillptr, pattern, -1, "scache5");
				if (++error == MAX_ERRORS)
					goto err;
			}

			if (check_error(fillptr,"scache5")) {
				error++;
				goto err;
			}

			if (*(fillptr+8) != pattern) {
				failed(fillptr+8, pattern, -1, "scache5");
				if (++error == MAX_ERRORS)
					goto err;
			}

			if (check_error(fillptr,"scache5")) {
				error++;
				goto err;
			}


			if (*(fillptr+16) != pattern) {
				failed(fillptr+16, pattern, -1, "scache5");
				error++;
			}

			if (check_error(fillptr,"scache5")) {
				error++;
				goto err;
			}

			if (*(fillptr+24) != pattern) {
				failed(fillptr+24, pattern, -1, "scache5");
				if (++error == MAX_ERRORS)
					goto err;
			}

			if (check_error(fillptr,"scache5")) {
				error++;
				goto err;
			}

			fillptr = fillptr + 32;
		}

		pattern <<=1;

	}

err:
	set_SR(oldSR);

	return error;
}

/* Writes a number of double word patterns to the cache (approx -- does not
 * play with TCC_GCACHE since this is dangerous).
 */
static int
scache6(void)
{
	volatile unsigned long pattern;
	volatile unsigned long *fillptr;
	ulong oldSR;
	volatile int i;
	int j,k,error = 0;

	msg_printf(DBG, " Scache6 test\n");
	__dcache_wb_inval_all();

	/* save status register */
	oldSR = get_SR();

	/* Enable parity interrupts */
	set_SR(oldSR | SR_IBIT10 | SR_IBIT9);

	busy(1);

	/* run thru with 16 patterns */
	pattern = 0;
	for ( pattern = j = 0; j < 16; j++)  {

		fillptr = (unsigned long *)SCACHE_TESTADDR;
		for (i = _sidcache_size / (int)sizeof(unsigned long); i > 0; --i) {
			*fillptr = pattern;

			if (check_error(fillptr,"scache6")) {
				error++;
				goto err;
			}

			fillptr++;
		}

		busy(1);

		fillptr = (unsigned long *)SCACHE_TESTADDR;
		for (i = _sidcache_size / (int)sizeof(unsigned long); i > 0; --i) {

			if ( *fillptr != pattern) {
				failed(fillptr, pattern, k, "scache6");
				if (++error == MAX_ERRORS)
					goto err;
			}

			if (check_error(fillptr,"scache6")) {
				error++;
				goto err;
			}

			fillptr++;
		}

		pattern = pattern + 0x0001000100010001;
		busy(1);
	}

err:
	set_SR(oldSR);

	busy(0);

	return error;
}

static int
check_error(volatile unsigned long *fillptr, const char *name)
{
	volatile long *ptcc_parity = (volatile long *)PHYS_TO_K1(TCC_PARITY);
	long tcc_error = *(volatile long *)PHYS_TO_K1(TCC_ERROR);
	long tcc_parity = *ptcc_parity;
	char *type;

	if ( tcc_parity & ( PAR_SYSAD_SYND_TCC | PAR_DATA_SYND_DB0 | 
	  PAR_DATA_SYND_DB1) ) {
		msg_printf(VRB,
			"ERROR(%s): Teton detected GCache parity error!\n",
			name);
		if (fillptr)
			msg_printf(VRB,"Address 0x%x  Data 0x%x\n",
				   fillptr, *fillptr);
		msg_printf(VRB,"TCC Parity Registor: 0x%08x\n", tcc_parity);
		msg_printf(VRB,"TCC Error Register: 0x%08x\n", tcc_error);

		type = 0;
		switch ((tcc_error & ERROR_TYPE) >> ERROR_TYPE_SHIFT ) {
		case 0:
			msg_printf(VRB,
				"sysAD data parity error (TDB %s%s)\n",
				(tcc_parity & PAR_DATA_SYND_DB0) ? "[0]" : "",
				(tcc_parity & PAR_DATA_SYND_DB1) ? "[1]" : "");
			break;
		case 1:
			type = "sysAD data parity error (TCC)\n";
			break;
		case 2:
			type = "sysCMD parity error\n";
			break;
		case 3:
			type = "unknown sysAD command\n";
			break;
		case 4:
			type = "GCache data parity error\n";
			break;
		case 5:
			type = "unknown T-Bus command\n";
			break;
		default:
			type = "unknown error\n";
			break;
		}
		if (type) msg_printf(VRB,"Error Type: %s",type);

		msg_printf(VRB,"Failing word: 0x%x\nIndex: 0x%x\nSet: 0x%x\n",
		  (tcc_error & ERROR_OFFSET) >> ERROR_OFFSET_SHIFT,
		  (tcc_error & ERROR_INDEX) >> ERROR_INDEX_SHIFT,
		  (tcc_error & ERROR_PARITY_SET) >> ERROR_PARITY_SET_SHIFT);

		/* clear parity information */
		*ptcc_parity = tcc_parity &
			       (PAR_TDB_EN|PAR_TCC_EN|PAR_DATA_TDB_EN);

		return 1;
	}

	return 0;
}


static void
failed(volatile unsigned long *ptr, unsigned long expect, int set,
       const char *test)
{
	unsigned long mask = 0xff;
	unsigned long offset = 0;
	unsigned long actual;
	int *ram;
	char *msg;
	static int even_rams[] = {16,15,11,10,12,6,5,4};
	static int odd_rams[] =  {14,13,8,7,9,3,2,1};

	if ((__psunsigned_t)ptr & 8) 
		ram = odd_rams, msg = "[odd]";
	else
		ram = even_rams, msg = "[even]";
	msg_printf(VRB, "%s: secondary cache data error @ 0x%lx %s\n",
		   test, ptr, msg);
	actual = *ptr;
	msg_printf(VRB, "    Expected: 0x%lx  Actual: 0x%lx\n", expect, actual);
	msg_printf(VRB, "    index: 0x%x  ",
			((__psint_t)ptr&TCC_CACHE_INDEX)>>TCC_CACHE_INDEX_SHIFT);
	if (set != -1)
		msg_printf(VRB, "set: %d", set);
	msg_printf(VRB, "\n");
	do {
		if ((actual & mask) != (expect & mask)) 
			msg_printf(VRB, "    byte: %d  SRAM: S%d\n", offset, ram[offset]);

		mask <<= 8;
		offset++;

	} while(mask);

}
