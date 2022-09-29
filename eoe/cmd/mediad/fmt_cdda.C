//
// fmt_cdda.C -- Compact Disc Digital Audio
//

#include <mntent.h>

#include "Device.H"
#include "Partition.H"
#include "PartitionAddress.H"
#include "SimpleVolume.H"

class FormatDSO;

extern "C"
void
inspect(FormatDSO *, Device *device)
{
    // Does this device support this format?

    if (!device->filesys_cdda())
	return;

    // Distinguish CD-ROM from DAT.

    if (device->has_audio_data() && device->feature_mountable())
    {	
	unsigned secsize = device->sector_size(FMT_CDDA);
	unsigned nsectors = device->capacity();
	PartitionAddress paddr(device->address(),
			       FMT_CDDA,
			       PartitionAddress::WholeDisk);
	Partition *part = Partition::create(paddr,
					    device,
					    secsize,
					    0,
					    nsectors,
					    "cdda");
	const char *fsname = device->dev_name(FMT_CDDA,
					      PartitionAddress::WholeDisk);
	mntent mnt = { (char *) fsname, NULL, "cdda", NULL, 0, 0 };
	(void) SimpleVolume::create(part, mnt, "");
    }
}
