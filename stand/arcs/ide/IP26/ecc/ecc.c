/*
 * ecc.c - IDE ECC checking mechanism test
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/vdma.h"
#include "sys/time.h"
#include "sys/immu.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#define getminutes()    (min=(RT_CLOCK_ADDR)->min&0xff, \
                        (((min&0xf0)>>4)*10|min&0xf))

static bool_t
testsdlr(void)
{
	register bool_t ok = 1;
	unsigned long expect, addr, actual;
	long value, op;
	unsigned long *addrptr = (unsigned long*) PHYS_TO_K1(&addr);
	extern int eccsdlr(unsigned long *addr, long offset, long value);

	msg_printf(DBG, "Testing Memory writes with SDL/SDR instructions\n");

	flush_cache();

	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 0L;
	expect  = 0x8877665544332211L;
	if ((actual = eccsdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG, "ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
		  op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 1L;
	expect  = 0x5588776655443322L;
	if ((actual = eccsdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG, "ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
		  op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 4L;
	expect  = 0x5544332288776655L;
	if ((actual = eccsdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG, "ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
		  op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 6L;
	expect  = 0x5544332211668877L;
	if ((actual = eccsdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG, "ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
		  op, expect, actual);
		ok = 0;
	}
	*addrptr= 0x1122334455667788L;
	value   = 0x8877665544332211L;
	op      = 7L;
	expect  = 0x5544332211667788L;
	if ((actual = eccsdlr(addrptr, op, value)) != expect) {
		msg_printf(DBG, "ecc sdr/sdl (op %ld): expect 0x%lx, actual 0x%lx\n",
		  op, expect, actual);
		ok = 0;
	}
	return ok ? 0 : 1;
}

static bool_t
gio2mc(void)
{
	volatile unsigned long addr, expect, actual;
	volatile unsigned long *addrptr = (unsigned long*) PHYS_TO_K1(&addr);
	int count = 1;
	int i;
	int timeout;
	int min;				/* used timer */
	unsigned int dmactl;
	bool_t err = 0;

	msg_printf(DBG, "Testing GIO to Memory via VDMA\n");

	flushbus();

	dmactl = VDMAREG(DMA_CTL);

	while (count < 9) {

		/* set the dbl word to all */
		*addrptr = 0xffffffffffffffffL;
	
		/* figure out expect results */
		expect = 0xffffffffffffffffL;
		for (i = 0; i < count; i++)
		    expect >>= 8;
	
		/* initialize DMA_CTL */
		/*  Tlim = 512, Slim = 4 */
		/*  no translation, intr disabled */
		/*  cache size = 32 words, page size dependent */
#if _PAGESZ == 16384
		VDMAREG(DMA_CTL) = 0x2000400e | (sizeof(pde_t) >> 3);
#elif _PAGESZ == 4096
		VDMAREG(DMA_CTL) = 0x2000400c | (sizeof(pde_t) >> 3);
#else
#error vdma cant do other than 16K or 4K pages
#endif


		/* clear out the interrupts */
		VDMAREG(DMA_CAUSE) = 0;

		/* load in memory location */
		VDMAREG(DMA_MEMADR) = KDM_TO_MCPHYS(addrptr);

		/* set up DMA_MODE (short burst, fill, DMA write) */
		VDMAREG(DMA_MODE) = VDMA_M_FILL | VDMA_M_INCA | VDMA_M_GTOH;

		/* set zoom to 1 */
		VDMAREG(DMA_STRIDE) = (1<<16);

		/* write X bytes */
		VDMAREG(DMA_SIZE) = (1<<16) | count;

		/* start DMA with writing zeros */
		VDMAREG(DMA_GIO_ADRS) = 0;
		flushbus();

		/* timeout in upto two minutes */
		timeout = (getminutes() + 2) % 60;

		/* wait for DMA to finish */
		while (VDMAREG(DMA_RUN) & VDMA_R_RUNNING) 
			if (getminutes() == timeout) {
				msg_printf(DBG, "DMA never stopped running\n");
				err = 1;
				goto done;
			}

		/* any errors */
		if (VDMAREG(DMA_RUN) != VDMA_R_COMPLETE) {
			msg_printf(DBG, "DMA problems (DMA_RUN 0x%x)\n", VDMAREG(DMA_RUN));
			err = 1;
			goto done;
		}

		/* lets verify the results */
		actual = *addrptr;
		if (actual != expect) {
			msg_printf(DBG, "DMA expected 0x%lx, actual 0x%lx\n", expect, actual);
			err = 1;
			goto done;
		}

		/* lets verify the DMA_ADR changed */
		if (VDMAREG(DMA_MEMADR) != KDM_TO_MCPHYS(addrptr) + count) {
			msg_printf(DBG, "DMA address contains unexpected value\n");
			err = 1;
			goto done;
		}

		count++;
	}

done:
	/* restore DMA_CTL */
	VDMAREG(DMA_CTL) = dmactl;

	return err;
}

/*ARGSUSED*/
bool_t
ecc_test(int argc, char **argv)
{
	register bool_t retval = 0;
	extern int test_ucwrite(void);

	msg_printf(VRB, "ECC test\n");

	/* test memory access from the CPU side */
	retval |= testsdlr();		/* test SDL/SDR */

	/* test memory access from the GIO side */
	retval |= gio2mc();

#ifdef	IP28
	/* uncached write in fast mode */
	if (is_ip28bb()) {
		msg_printf(DBG, "Testing Uncached Write in Fast Mode\n");
		retval |= test_ucwrite();
	}
#endif

	return retval;
}

