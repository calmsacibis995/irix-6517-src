/* ARCS stubs for calling through the SPB firmware switch conviently.
 */
#ident "$Revision: 1.31 $"

#include <arcs/spb.h>
#include <arcs/io.h>
#include <arcs/time.h>
#include <arcs/dirent.h>
#include <arcs/hinv.h>
#include <arcs/time.h>
#include <libsc.h>

LONG
Open(CHAR *filename, OPENMODE mode, ULONG *fd)
{
	return(__TV->Open(filename,mode,fd));
}

LONG
Close(ULONG fd)
{
	return(__TV->Close(fd));
}

LONG
Read(ULONG fd, void *buf, ULONG n, ULONG *cnt)
{
	return(__TV->Read(fd,buf,n,cnt));
}

LONG
GetReadStatus(ULONG fd)
{
	return(__TV->GetReadStatus(fd));
}

LONG
Write(ULONG fd, void *buf, ULONG n, ULONG *cnt)
{
	return(__TV->Write(fd,buf,n,cnt));
}

LONG
Seek(ULONG fd, LARGEINTEGER *off, SEEKMODE wence)
{
	return(__TV->Seek(fd,off,wence));
}

LONG
Mount(CHAR *path, MOUNTOPERATION op)
{
	return(__TV->Mount(path,op));
}

LONG
SetEnvironmentVariable(CHAR *name, CHAR *value)
{
	return(__TV->SetEnvironmentVariable(name,value));
}

CHAR
*GetEnvironmentVariable(CHAR *name)
{
	return(__TV->GetEnvironmentVariable(name));
}

LONG
GetDirectoryEntry(ULONG fd, DIRECTORYENTRY *buf, ULONG n, ULONG *cnt)
{
	return(__TV->GetDirEntry(fd,buf,n,cnt));
}

COMPONENT *
GetPeer(COMPONENT *p)
{
	return(__TV->GetPeer(p));
}

COMPONENT *
GetChild(COMPONENT *p)
{
	return(__TV->GetChild(p));
}

COMPONENT *
GetParent(COMPONENT *p)
{
	return(__TV->GetParent(p));
}

LONG
GetConfigurationData(void *data, COMPONENT *p)
{
	return(__TV->GetConfigData(data,p));
}

COMPONENT *
AddChild(COMPONENT *cur, COMPONENT *tmp, void *cfgd)
{
	return(__TV->AddChild(cur,tmp,cfgd));
}

LONG
DeleteComponent(COMPONENT *c)
{
	return(__TV->DeleteComponent(c));
}

COMPONENT *
GetComponent(CHAR *path)
{
	return(__TV->GetComponent(path));
}

LONG
SaveConfiguration(void)
{
	return(__TV->SaveConfiguration());
}

SYSTEMID *
GetSystemId(void)
{
	return(__TV->GetSystemId());
}

TIMEINFO *
GetTime(void)
{
	return(__TV->GetTime());
}

ULONG
GetRelativeTime(void)
{
	return(__TV->GetRelativeTime());
}

LONG
GetFileInformation(ULONG fd, FILEINFORMATION *info)
{
	return(__TV->GetFileInformation(fd, info));
}

LONG
SetFileInformation(ULONG fd, ULONG flags, ULONG mask)
{
	return(__TV->SetFileInformation(fd, flags, mask));
}

VOID
FlushAllCaches(void)
{
#if _MIPS_SIM != _ABI64
	/* ok, ok, this is a hack but there is no FlushAllCaches() entry point
	 * in the original revision of the IP20 prom and we're not about to 
	 * replace those in the field. */
	void ip20promhack_cacheflush(void);

	if (!__TV->FlushAllCaches)
		ip20promhack_cacheflush();
	else
#endif
		__TV->FlushAllCaches();
}

/*  Old IP20 proms had intermediate arcs trasfer vectors, which
 * need to be swizzled so we can run on it.
 */
void
updatetv(void)
{
#if _MIPS_SIM != _ABI64			/* ARCS64 does not have this problem */
	if ((sgivers()) < 1 && (SPB->TVLength < (34*4))) {
		oldFirmwareVector_1 *oldtv = (oldFirmwareVector_1 *)
			SPB->TransferVector;
		FirmwareVector *tv = SPB->TransferVector;

		oldtv1_2_latest(SPB,oldtv,tv);
	}
#endif
}
