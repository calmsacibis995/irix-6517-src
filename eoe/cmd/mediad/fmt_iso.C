#include <bstring.h>
#include <mntent.h>

#include "Device.H"
#include "Log.H"
#include "Partition.H"
#include "PartitionAddress.H"
#include "SimpleVolume.H"

class FormatDSO;

/*
 *	Volume Partition Descriptor
 */
struct vol_part_desc {
	u_char				type;
	char				id[5];
	u_char				version;
	u_char				unused;
	char				sys_id[32];
	char				vol_id[32];
	u_char				vol_part_loc[8];
	u_long				vol_space_size_lsb;
	u_long				vol_space_size_msb;
	u_char				system_use[1960];
};

extern "C"
void
inspect(FormatDSO *, Device *device)
{
    if (!device->filesys_iso9660())
	return;

    // XXX hack!
    // Work around the "Unexpected blank media" message from audio CDs
    // by skipping them.

    if (device->has_audio_data())
	return;

    char buf[2048];
    static char  iso_ident[] = "CD001";
    static char hsfs_ident[] = "CDROM";

    bzero(buf, sizeof buf);
    int secsize = device->sector_size(FMT_ISO);
    if (secsize > sizeof buf)
    {   Log::info("\"%s\" sector size of %d too large for ISO",
		   device->short_name(), secsize);
	return;				// Give up.
    }
    if (device->read_data(buf, 16, sizeof buf / secsize, secsize) == 0)
    {
	vol_part_desc *vp = (vol_part_desc *) buf;

	if (strncmp(vp->id, iso_ident, strlen(iso_ident)) == 0 ||
	    strncmp(vp->sys_id + 1, hsfs_ident, strlen(hsfs_ident)) == 0 ||
	    strncmp(vp->sys_id + 1, iso_ident, strlen(iso_ident)) == 0)
	{
	    char vol_label[sizeof vp->vol_id + 1];
	    strncpy(vol_label, vp->vol_id, sizeof vol_label);

	    Log::info("device has ISO 9660 filesystem.");
	    PartitionAddress paddr(device->address(), FMT_ISO,
				   PartitionAddress::WholeDisk);
	    int nsectors = device->capacity();
	    Partition *part = Partition::create(paddr, device,
						secsize,
						0,
						nsectors,
						"iso9660");
	    const char *fsname =
		part->device()->dev_name(FMT_ISO, PartitionAddress::WholeDisk);
	    mntent mnt = { (char *) fsname, NULL, "iso9660", NULL, 0, 0 };
	    (void) SimpleVolume::create(part, mnt, vol_label);
	}
    }    
}
