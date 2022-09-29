
#ifdef IP21			/* whole file */

#include "libsc.h"
#include "libsk.h"
#include "sys/cpu.h"
#include "sys/loaddrs.h"
#include "sys/EVEREST/evmp.h"

#define EV_REVISION_ALLZERO	0xffffffffffffff00LL

static int parse_margin_args(int, char **);
static void set_voltages(int);
static void set_cpu_voltage(int);

int
margin_voltage_cmd(int argc, char *argv[])
{
	int margin_vector;

	if ((margin_vector = parse_margin_args(argc, argv)) == -1) {
Usage:
		printf("Usage: margin {command} {command}\n");
		printf("\n");
		printf("Command can be one of the following:\n");
		printf("   <nothing>   Set Both Supplies to Nominal.\n");
		printf("   3V=high     Set 3V Supply to Plus Five Percent.\n");
		printf("   3V=low      Set 3V Supply to Minus Five Percent.\n");
		printf("   5V=high     Set 5V Supply to Plus Five Percent.\n");
		printf("   5V=low      Set 5V Supply to Minus Five Percent.\n");
		printf("\n");
		return(1);
	}

	if ((margin_vector & (EV_VCR_LOW3V | EV_VCR_HIGH3V)) ==
						(EV_VCR_LOW3V | EV_VCR_HIGH3V))
		goto Usage;
	else if (margin_vector & EV_VCR_LOW3V)
		printf("Lowering 3V Power to Minus Five Percent...\n");
	else if (margin_vector & EV_VCR_HIGH3V)
		printf("Raising 3V Power to Plus Five Percent...\n");
	else
		printf("Returning 3V Power to Nominal...\n");

	if ((margin_vector & (EV_VCR_LOW5V | EV_VCR_HIGH5V)) ==
						(EV_VCR_LOW5V | EV_VCR_HIGH5V))
		goto Usage;
	else if (margin_vector & EV_VCR_LOW5V)
		printf("Lowering 5V Power to Minus Five Percent...\n");
	else if (margin_vector & EV_VCR_HIGH5V)
		printf("Raising 5V Power to Plus Five Percent...\n");
	else
		printf("Returning 5V Power to Nominal...\n");

	set_voltages(margin_vector);

	return 0;
}

static int
parse_margin_args(int argc, char *argv[])
{
	int margin_vector = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "3V=low")) {
			margin_vector |= EV_VCR_LOW3V;
			continue;
		}
		if (!strcmp(argv[i], "5V=low")) {
			margin_vector |= EV_VCR_LOW5V;
			continue;
		}
		if (!strcmp(argv[i], "3V=high")) {
			margin_vector |= EV_VCR_HIGH3V;
			continue;
		}
		if (!strcmp(argv[i], "5V=high")) {
			margin_vector |= EV_VCR_HIGH5V;
			continue;
		}
		return(-1);
	}
	return(margin_vector);
}

static volatile int cpu_synchronize;

static void
set_voltages(int margin_vector)
{
	unsigned int cpu;

	for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {
		if (cpu == cpuid())
			continue;

		if ((MPCONF[cpu].mpconf_magic != MPCONF_MAGIC) ||
		    (MPCONF[cpu].virt_id != cpu))
			continue;

		cpu_synchronize = 1;
		us_delay(10000);
		launch_slave(cpu, set_cpu_voltage, margin_vector, 0, 0, 0);
		while (cpu_synchronize != 0)
			;
		us_delay(10000);
	}


	us_delay(10000);
	set_cpu_voltage(margin_vector);
	us_delay(10000);

	printf("...Done\n");
}

static void
set_cpu_voltage(int margin_vector)
{
	/* Attempt to make sure that this board does indeed
	 * have a voltage control register.
	 * Note that the voltage control register also incorporates
	 * the revision register.
	 */
	machreg_t revision = read_revision();

	if ((revision & EV_REVISION_ALLZERO) == 0 &&
	    (revision & EV_IP21REV_MASK) >= EV_IP21REV_VCR)
		write_vcr((long)margin_vector);
	else
		printf(
		"Cpu %d, board revision must be >= 7 for Voltage Margining\n"
		"Revision register read was 0x%lx\n",
		cpuid(), revision);
	cpu_synchronize = 0;
}

static void
print_board_revision(int cpu)
{
	machreg_t revision = read_revision();

	/* Print out the board revision. Since the revision register
	 * did not appear on the board until rev 6, attempt to
	 * disambiguate whether we are reading garbage EAROM
	 * or the actual revision register.
	 */
	if ((revision & EV_REVISION_ALLZERO) == 0 &&
	    (revision & EV_IP21REV_MASK) >= EV_IP21LOWREV)
		printf("Cpu %d, Board Revision %ld\n",
			cpu, (revision & EV_IP21REV_MASK) >> EV_IP21REV_SHFT);
	else
		printf("Cpu %d Board Revision < 6 "
			" (Revision register read was 0x%lx)\n",
			cpu, revision);
	cpu_synchronize = 0;
}

int
board_revision_cmd(void)
{
	unsigned int cpu;

	for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {
		if (cpu == cpuid()) {
			print_board_revision(cpu);
			continue;
		}

		if ((MPCONF[cpu].mpconf_magic != MPCONF_MAGIC) ||
		    (MPCONF[cpu].virt_id != cpu))
			continue;

		cpu_synchronize = 1;
		us_delay(10000);
		launch_slave(cpu, print_board_revision, cpu, 0, 0, 0);
		while (cpu_synchronize != 0)
			;
		us_delay(10000);
	}

	return 0;
}

#endif	/* IP21 */
