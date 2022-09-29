/*
 * interrupt controller (INT2) interrupt mask registers test
 */

#ident	"$Revision: 1.26 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "int2.h"

void scsi_reset(int);		/* for IP22 */

#define	NUMBER_OF_PATTERNS	(sizeof(data_pattern) / sizeof(unsigned char))

#if IP22
static char *int2_testname = "Interrupt mask registers (INT2/3)";
#else
static char *int2_testname = "Interrupt mask registers (INT2)";
#endif
static char *mipscause = "MIPS cause register";
static char *local = "Local I/O";
static int error_count;

static void int2_maskerr(char *, char, volatile unchar *a, unchar, unchar);
static void int2_penderr(char *, char, uint, struct reg_desc *);

extern struct reg_desc _liointr0_desc[], _liointr1_desc[];
extern struct reg_desc cause_desc[];

int 
int2_mask(void)
{
	volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
	volatile unsigned char *lio_isr1 = K1_LIO_1_ISR_ADDR;
	volatile unsigned char *lio_mask0 = K1_LIO_0_MASK_ADDR;
	volatile unsigned char *lio_mask1 = K1_LIO_1_MASK_ADDR;
	static unchar data_pattern[] = {
		0x00, 0xaa, 0xcc, 0xf0,
		(unchar)~0x00, (unchar)~0xaa, (unchar)~0xcc, (unchar)~0xf0
	};
	k_machreg_t old_sr;
	unsigned long i,j;

	msg_printf(VRB, int2_testname);
	msg_printf(VRB, " test\n");

#if IP22
	if (!is_fullhouse())
		scsi_reset(0);			/* fix problem with 3 external scsi devices causing */
						/* this test to fail on R5k Indy. */
#endif

	old_sr = get_SR();
	set_SR(old_sr & ~SR_IE);		/* no interrupts! */

	error_count = 0;

	/*
	 * write different patterns to the interrupt mask registers, then read
	 * back and verify
	 */
	for (i=0,j=1; i < NUMBER_OF_PATTERNS; i++,j=(j+1)%NUMBER_OF_PATTERNS) {
		*lio_mask0 = data_pattern[i];
		*lio_mask1 = data_pattern[j];

		if (*lio_mask0 != data_pattern[i])
			int2_maskerr(local, '0', lio_mask0, data_pattern[i],
				     *lio_mask0);

		if (*lio_mask1 != data_pattern[j])
			int2_maskerr(local, '1', lio_mask1, data_pattern[j],
				     *lio_mask1);
	}

	*lio_mask0 = 0x0;
	*lio_mask1 = 0x0;
	flushbus();

	/*
	 * should have no pending lio 0 interrupts
	 * LIO_GIO_1 & LIO_GIO_0 may be floating
	 */
#if IP20
#define LIO_CARE_0 ~(LIO_GIO_1|LIO_GIO_0)
#endif
#if IP22 || IP26 || IP28
#define LIO_CARE_0 ~(LIO_GIO_1|LIO_GIO_0|LIO_SCSI_1|LIO_SCSI_0|LIO_LIO2)
#endif
	if ((*lio_isr0 & LIO_CARE_0) != 0x0)
		int2_penderr(local,'0',*lio_isr0 & LIO_CARE_0,_liointr0_desc);

	/*
	 * should have no pending lio 1 interrupts
	 * LIO_GIO_2 & LIO_VIDEO may be floating.
	 */
#if IP20
#define LIO_CARE_1 ~(LIO_GIO_2|LIO_VIDEO|LIO_GR1MODE|LIO_AC)
#endif
#if IP22 || IP26 || IP28
#define LIO_CARE_1 ~(LIO_GIO_2|LIO_VIDEO|LIO_AC|LIO_POWER|LIO_HPC3)
#endif
	if ((*lio_isr1 & LIO_CARE_1) != 0x0)
		int2_penderr(local,'1',*lio_isr1 & LIO_CARE_1,_liointr1_desc);

	/*
	 * make sure no unexpected stuff from the FPU
	 */
	set_SR(get_SR()|SR_CU1);
	SetFPSR(0x0);

	/*
	 * MIPS cause register should not show any pending interrupt
	 */
	*(volatile int *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
	*(volatile int *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	flushbus();

	set_SR(get_SR() & ~SR_IMASK);
	set_cause(0);
#if R4000 || R10000
	if ((get_cause() & CAUSE_IPMASK) & ~(CAUSE_IP8|CAUSE_IP6|CAUSE_IP5)) {
#endif
#if TFP
	if ((get_cause() & CAUSE_IPMASK) & ~(CAUSE_IP11|CAUSE_IP6|CAUSE_IP5)) {
#endif
		error_count++;
		msg_printf(VRB, "%s mask: %R\n", mipscause, get_cause(),
			   cause_desc);
	}

	if (error_count)
		sum_error(int2_testname);
	else
		okydoky();

#if IP22 || IP26 || IP28
	*lio_mask1 = LIO_POWER;		/* enable power off */
	set_SR(old_sr);
#endif

	return(error_count);
}

static void
int2_maskerr(char *regname, char number, volatile unchar *address,
	     unchar expected, unchar actual)
{
	error_count++;
	msg_printf(VRB, "%s mask %c: 0x%08x, Expected: 0x%02x, "
		   "Actual: 0x%02x\n", regname, number, address, expected,
		   actual);
}

static void
int2_penderr(char *regname, char number, uint value,
	     struct reg_desc *descriptor)
{
	error_count++;
	if (descriptor)
		msg_printf(VRB, "Unexpected %s %c interrupt pending:  %R\n",
			   regname, number, value, descriptor);
	else
		msg_printf(VRB, "Unexpected %s %c interrupt pending:  0x%x\n",
			   regname, number, value);
}
