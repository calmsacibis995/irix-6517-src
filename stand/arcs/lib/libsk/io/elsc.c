#ifdef SN0

#include <sys/types.h>
#include <ksys/elsc.h>
#include <libsc.h>
#include <sys/SN/arch.h>

/*
 * Most of the ELSC support is in libkl/io/elsc.c
 * This part isn't in the prom
 */

static elsc_t *elsc_array[MAXCPUS];

void
set_elsc(elsc_t *e)
{
	cpuid_t cpunum = cpuid();

	/* The master CPU may call us before CPU numbers are set up */
	if (cpunum == -1)
		cpunum = 0;

	elsc_array[cpuid()] = e;
}

elsc_t *
get_elsc(void)
{
	return elsc_array[cpuid()];
}

#endif /* SN0 */
