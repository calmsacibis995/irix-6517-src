#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#define ITERATIONS 2
#define COUNT 0x20
#define DCacheBytes 16384

/* Power On Fast IU test Length of Timeout in Minutes */
#define PON_MIN_LEN		5

extern void fasttest(char *, char *, int);

char Buffer[] = {
	0xff, 0xff, 0xfb, 0x94, 0x00, 0x00, 0x00, 0x01, 
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int
fast_iu_test(void)
{
	int sur1, sur2, sur3, sur4;
	char *SrcData, *DstData;
	int i,j, error = 0;

#if !PROM
	int *DstBuffer, *SrcBuffer;

	DstBuffer = malloc(32*1024);
	SrcBuffer = malloc(1024);

	/* Align SrcData and DstData */
	SrcData = (char *)((((long)(SrcBuffer) + 512) & ~(0x0ff)) + 0x2f);
	DstData = (char *)(((long)(DstBuffer) + 256) & ~(0x0ff));
#else
	SrcData = (char *)PHYS_TO_K0(0x08c0822f);	/* aligned! */
	DstData = (char *)PHYS_TO_K0(0x08c09100);	/* aligned! */
#endif

	bcopy(Buffer,SrcData,COUNT+0x21);

	for (sur1=0; sur1 < 64; sur1++) {
		sur2 = 23 - sur1/8;
		sur3 = 1 << (sur1%8);
		SrcData[sur2] &= ~sur3;
		sur2 = 48 - sur1/8;
		SrcData[sur2] &= ~sur3;

		for (sur4=0; sur4 < 63; sur4++) {
			sur2 = 48 - sur4/8;
			sur3 = 1 << (sur4%8);
			SrcData[sur2] &= ~sur3;

			for (j=0; j < ITERATIONS; j++) {
				fasttest(SrcData,DstData,COUNT);
				bzero(DstData+16*1024,32);

				/* Do the comparison */
				for (i=0; i < COUNT; i++) {
					if (DstData[i] != SrcData[i]) {
						error = 1;
#if !PROM
						msg_printf(DBG,
	"DCache miscompare detected at interation %d, index 0x%x,"
	"expected 0x%x, actual 0x%x\n",j,i,SrcData[i],DstData[i]);
#endif
					}
				}
				if (error) goto done;
			}
			SrcData[sur2]=0xff;
		}
		sur2=23-sur1/8;
		SrcData[sur2]=0xff;
		sur2 = 48-sur1/8;
		SrcData[sur2]=0xff;
	}

done:
#if !PROM
	flush_cache();			/* sync up malloc arena */
	free(SrcBuffer);
	free(DstBuffer);
#endif

	return(error);
}

#define LEDTIME		25

/*  Wait up to PON_MIN_LEN minutes for the IU to heat up.  This test should
 * pass quickly on new IUs (2.4+) someday, but is an issue with most 2.2
 * parts.  Once the parts heat-up they should pass this test ok.
 */
#define getminutes()	(min=(RT_CLOCK_ADDR)->min&0xff,	\
			((((min&0xf0)>>4)*10)+(min&0xf)))

int 
pass_fast_iu_test(void)
{
	unsigned int timeout, min_carry;
	k_machreg_t prid;
	int err = 0;
	char *fastiu_name = "Fast IU";
	int fail = 0;
	TIMEINFO t;
	long *ptr; 
	char *p;
	int len;
	int min;
	static void warmup(long *, int);
#ifdef PROM
	int led = 2, ledcnt = 0;
#endif

	/* If CPU rev 2.2 or less, don't bother with fast iu test */
	if ((prid = tfp_getprid() & 0xff) < 0x22) {
		msg_printf(VRB, "%s test skipped (Processor rev %d.%d).\n", 
		  fastiu_name, (prid>>4), (prid&0xf));
		return 0;
	}

	p = getenv("diagmode");
	if (p++) {
		/* diagmode=xi skip the test */
		if (*p == 'i') {
			msg_printf(VRB, "%s test skipped with diagmode=?i.\n",
				   fastiu_name);
			return 0;
		}

		/* diagmode=xF simulate failed test */

		if (*p == 'F')
			fail = 1;
	}

	msg_printf(VRB, "%s test.\n", fastiu_name);

	len = 1024;
#if PROM
	ptr = (long*) PHYS_TO_K0(0x08c80000);
#else
	ptr = (long*) malloc(sizeof(long)*len);
#endif

	cpu_get_tod(&t);				/* ensure tod going */

	/* ending minute and carry set if will wrap */
	timeout = getminutes() + PON_MIN_LEN;
	min_carry = timeout / 60;

	while (fast_iu_test() || fail) {

		/* determine if hit the PON_MIN_LEN marker */
		if ((getminutes() + (min_carry?60:0)) >= timeout) {
			err = 1;			/* time-out failure */
			break;
		}

#if PROM
		/* time to flip the led? -- suresh test is slow! */
		if ((ledcnt++ % LEDTIME) == 0) {
			led ^= 2;			/* amber/off led */
			ip22_set_cpuled(led);
		}

#endif
		/* attempt to warm up the CPU */
		if (ptr)
			warmup(ptr, len);
	}

#if PROM
	ip22_set_cpuled(LED_AMBER);			/* led back to amber */
#else
	if (ptr)
		free(ptr);
#endif

	/*  If the fast IU test failed in the PROM, we reset the machine
	 * once.  In mfg, this seems to always work as some heaters with
	 * 2.2 IU.
	 *
	 *  Use lucky SEMAPHORE_13 to prevent looping.
	 */
	if (err) {
#if PROM
		volatile int *sema = (int *)PHYS_TO_K1(SEMAPHORE_13);

		/*  If the sema is not set, warm reset the machine.  If this
		 * is the second time through, we may really have a problem
		 * with the heater.
		 */
		if (*sema == 0) {
			/*CONSTCOND*/
			while (1) {
				*(volatile int *)PHYS_TO_K1(CPUCTRL0) |=
					CPUCTRL0_WR_ST;
				flushbus();
				us_delay(1);
			}
		}

		/* Clear sema so the tunes can be played on shutdown.
		 */
		*sema = 0;
#endif

		sum_error(fastiu_name);
	}
	else
		okydoky();

	return err;
}


/*
 * this routine warms up the CPU by writing interleaved 0x5s and 0xas
 * to the cache.
 */
static void
warmup(long *root_ptr, int size)
{
    int i, offset;
    long *ptr;

    for (i = 0; i < 16; i++) 
	for (offset = 0, ptr = root_ptr; offset < size; offset++) {
		if ((offset%2 && i%2) || ((offset%2) == 0 && (i%2) == 0))
			*ptr++ = 0xaaaaaaaaaaaaaaaa;
		else
			*ptr++ = 0x5555555555555555;
    	}
}

#if PROM
int
pon_iu(void) {
	return pass_fast_iu_test();
}

#endif

