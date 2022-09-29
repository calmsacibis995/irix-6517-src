/* ARCS stubs for calling through the SPB firmware switch conviently.
 */
#ident "$Revision: 1.2 $"

#include <arcs/spb.h>
#include <arcs/io.h>
#include <arcs/time.h>
#include <arcs/dirent.h>
#include <arcs/hinv.h>
#include <arcs/time.h>
#include <libsc.h>

LONG
Load(CHAR *path, ULONG topaddr, ULONG *execaddr, ULONG *lowaddr)
{
	return(__TV->Load(path,topaddr,execaddr,lowaddr));
}

LONG
Invoke(ULONG execaddr, ULONG stackaddr, LONG argc, CHAR *argv[], CHAR *envp[])
{
	return(__TV->Invoke(execaddr,stackaddr,argc,argv,envp));
}

LONG
Execute(CHAR *path, LONG argc, CHAR *argv[], CHAR *envp[])
{
	/* 
	 * Sash now sets the kernname environ variable for unix, for use
	 * when loading the runtime symbol table for loadable modules.
	 */
	setenv ("kernname", path);
	return(__TV->Execute(path,argc,argv,envp));
}

MEMORYDESCRIPTOR *
GetMemoryDescriptor(MEMORYDESCRIPTOR *current)
{
	return(__TV->GetMemoryDesc(current));
}
