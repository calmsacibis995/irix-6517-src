/* SGI ARCS PrivateVector stubs for calling through the SPB switch.
 */
#ident "$Revision: 1.1 $"

#include <arcs/spb.h>
#include <arcs/pvector.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsc_internal.h>

LONG
load_abs (CHAR *path, ULONG *execaddr)
{
	return(__PTV->LoadAbs(path, execaddr));
}

LONG
invoke_abs (ULONG execaddr, ULONG stackaddr,LONG argc,CHAR *argv[],CHAR *envp[])
{
	return(__PTV->InvokeAbs(execaddr, stackaddr, argc, argv, envp));
}

LONG
exec_abs (CHAR *path, LONG argc, CHAR *argv[], CHAR *envp[])
{
	return(__PTV->ExecAbs(path, argc, argv, envp));
}
