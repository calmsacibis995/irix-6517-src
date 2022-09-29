/* setup RPSS switchs */
#include <sys/types.h>
#include <arcs/io.h>
#include <arcs/restart.h>
#include <arcs/dirent.h>
#include <arcs/hinv.h>
#include <arcs/pvector.h>
#define __USE_SPB32 1
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/mips_addrspace.h>

#ifdef SN
#include "sys/SN/addrs.h"	/* For TVADDR, PVADDR */
#endif

#ident "$Revision: 1.71 $"

extern int _prom;

static FirmwareVector _fv = {
	Load,
	Invoke,
	Execute,
	Halt,
	PowerDown,
	Restart,
	Reboot,
	EnterInteractiveMode,
	0,				/* reserved1 */
	GetPeer,
	GetChild,
	GetParent,
	GetConfigurationData,
	AddChild,
	DeleteComponent,
	GetComponent,
	SaveConfiguration,
	GetSystemId,
	GetMemoryDescriptor,
	0,				/* reserved2 */
	GetTime,
	GetRelativeTime,
	GetDirectoryEntry,
	Open,
	Close,
	Read,
	GetReadStatus,
	Write,
	Seek,
	Mount,
	GetEnvironmentVariable,
	SetEnvironmentVariable,
	GetFileInformation,
	SetFileInformation,
	FlushAllCaches
};

int sgivers(void) {return(SGIVERS);}
static PrivateVector _pv = {
	ioctl,
	get_nvram_tab,
	load_abs,
	invoke_abs,
	exec_abs,
	fs_register,
	fs_unregister,
	Signal,
	gethtp,
	sgivers,
	cpuid,
	businfo,
	cpufreq,
#ifdef SN0
	kl_inv_find,
	klhinv,
	kl_get_num_cpus,
	sn0_getcpuid,
#endif	/* SN0 */
};

#ifndef SN0
/* Put firmware and private vectors on the SPB page, so they can be changed.
 * 0x1000 - 0x17ff: spb (2K)
 * 0x1800 - 0x1bff: firmware vector (1K)
 * 0x1c00 - 0x1fff: private vector (1K)
 */
#define TVADDR (((__psunsigned_t)SPB)+(0x800))
#define PVADDR (((__psunsigned_t)SPB)+(0xc00))

#endif /* SN0 */

static
void
_setup_spb(void)
{
        extern int _prom;

	SPB->Signature = SPBMAGIC;
	SPB->Length = sizeof(spb_t);
	SPB->Version = SGI_ARCS_VERS;
	SPB->Revision = SGI_ARCS_REV;
	SPB->RestartBlock = 0;
	if (_prom != 3)
		SPB->DebugBlock = 0;
	SPB->AdapterCount = 0;
}

#if _K64PROM32
void
swizzle_SPB(void)
{
	if(SPB->Signature != SPBMAGIC) {
	__int32_t f1, f2, f3, f4;
		f1 = SPB32->TransferVector;
		f2 = SPB32->TVLength;
		f3 = SPB32->PrivateVector;
		f4 = SPB32->PTVLength;

		_setup_spb();

		SPB->TransferVector = (FirmwareVector *)PHYS_TO_K1(f1 & 0x1fffffff);
		SPB->TVLength = f2;
		SPB->PrivateVector = (long *)PHYS_TO_K1(f3 & 0x1fffffff);
		SPB->PTVLength = f4;

		/* leave SPB->GEVector and utlb miss vector random for now */
	}
}
#endif

void
_init_spb(void)
{
	/*  If _prom is set, we are running under prom or dprom, and we
	 * can change the SPB.  Programs such as symmon can be linked with
	 * libsk, but don't need to change the switch because they call
	 * their own i/o routines directly.
	 */

	if (!_prom)
		return;

	bcopy(&_fv,(void *)TVADDR,sizeof(FirmwareVector));
	bcopy(&_pv,(void *)PVADDR,sizeof(PrivateVector));

	_setup_spb();
	SPB->GEVector = (LONG *)exceptnorm;
	SPB->UTLBMissVector = (LONG *)exceptutlb;
	SPB->TVLength = sizeof(FirmwareVector);
	SPB->TransferVector = (FirmwareVector *)TVADDR;
	SPB->PTVLength = sizeof(PrivateVector);
	SPB->PrivateVector = (LONG *) PVADDR;

	return;
}
