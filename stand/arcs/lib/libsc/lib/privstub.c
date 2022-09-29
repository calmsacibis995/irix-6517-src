/* SGI ARCS PrivateVector stubs for calling through the SPB switch.
 */
#ident "$Revision: 1.25 $"

#include <arcs/spb.h>
#include <arcs/pvector.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsc_internal.h>

LONG
ioctl(ULONG fd, LONG cmd, LONG arg)
{
	return(__PTV->Ioctl(fd,cmd,arg));
}

LONG
get_nvram_tab (char *addr, int size)
{
	return(__PTV->GetNvramTab(addr,size));
}

int
fs_register(int (*func)(), char *name, int type)
{
	return(__PTV->FsReg(func,name,type));
}

int
fs_unregister(char *name)
{
	return(__PTV->FsUnReg(name));
}

SIGNALHANDLER
Signal(LONG sig, SIGNALHANDLER handler)
{
	return(__PTV->Signal(sig,handler));
}

int
sgivers(void)
{
	/* pre-sgivers() call? */
	if (SPB->PTVLength <= (ULONG)PTVOffset(sgivers))
		return(0);
	return(__PTV->sgivers());
}

int
cpuid(void)
{
	/*
	 * Older UP proms don't have this entry.  Since they're
	 * all UP, we can just return 0 for cpuid.
	 */
	if (SPB->PTVLength <= (ULONG)PTVOffset(cpuid)) {
		return(0);
	}
	return(__PTV->cpuid());
}


int
cpufreq(int cpuid)
{
	/*
	 * Older UP proms don't have this entry point. In the past,
	 * we did this by returning the value stored in the cpufreq
	 * environment variable, so we do that if we can't find an
	 * entry point.
	 */

	if (SPB->PTVLength <= (ULONG)PTVOffset(cpufreq)) {
		int freq;

		if (*atob(getenv("cpufreq"), &freq))
			return -1;
		else
			return freq;
	}

	return(__PTV->cpufreq(cpuid));
}
	 
void
businfo(LONG verbose)
{
	/*
	 * Older proms don't have this entry. Since only
	 * everest needs this currently, just return if it
	 * isn't available.
	 */
	if (SPB->PTVLength > (ULONG)PTVOffset(businfo))
		__PTV->businfo(verbose);
} 
		
/* libsc isatty() which uses ioctl to call libsk version.
 */
int
isatty(ULONG fd)
{
	int arg;

	ioctl(fd,TIOCISATTY,(long)&arg);
	return(arg);
}
