#include <sys/param.h>
#include <bstring.h>
#include <diskinfo.h>
#include <sys/dvh.h>
#include <sys/file.h>
#include <sys/fs/efs.h>

#include "Device.H"
#include "Log.H"
#include "Partition.H"
#include "PartitionAddress.H"
#include "SimpleVolume.H"

class FormatDSO;

#define EFS_BLKSIZE 512

extern "C" int valid_efs(struct efs *sblk, int size); // (from libdisk)

static void
create_efs_volume(Device *device, ptinfo *pp)
{
    PartitionAddress paddr(device->address(), FMT_EFS, pp->ptnum);
    Partition *part = Partition::create(paddr,
					device,
					EFS_BLKSIZE,
					pp->pstartblk,
					pp->psize,
					"efs");
    const char *fsname = device->dev_name(FMT_EFS, pp->ptnum);
    mntent mnt = { (char *) fsname, NULL, "efs", NULL, 0, 0 };
    (void) SimpleVolume::create(part, mnt, "");
}

bool has_efs_superblock(Device *dev, const ptinfo *pp)
{
    char sbuf[BBTOB(BTOBB(sizeof (struct efs)))];
    struct efs *sb = (struct efs *) sbuf;
    __uint64_t sectno = pp->pstartblk + EFS_SUPERBB;
    unsigned int nsects = sizeof sbuf / EFS_BLKSIZE;

    if (dev->read_data(sbuf, sectno, nsects, EFS_BLKSIZE) != 0)
    {   Log::critical("can't read sector %lld of device %s: %m",
		      sectno, dev->name());
	return false;
    }
    return valid_efs(sb, pp->psize) == 1;
}

extern "C"
void
inspect(FormatDSO *, Device *device)
{
    // Does this device support this format?

    if (!device->filesys_efs())
	return;

    char buf[2048];
    bzero(buf, sizeof buf);
    int secsize = device->sector_size(FMT_EFS);
    if (secsize != EFS_BLKSIZE)
	return;				// Give up.

    if (device->read_data(buf, 0, sizeof buf / secsize, secsize) != 0)
	return;

    if (valid_vh((volume_header *) buf) == -1)
	return;

    const char *efsname = device->dev_name(FMT_EFS, 8);
    if (strncmp(efsname, "/dev/dsk/", 9))
	return;

    char rawfsname[30];
    sprintf(rawfsname, "/dev/rdsk/%s", efsname + 9);
    Log::debug("EFS inspect: rawfsname = \"%s\"", rawfsname);
    unsigned token = setdiskinfo(rawfsname, "/etc/fstab", O_RDONLY);
    if (token)
    {   for (ptinfo *pp; pp = getpartition(token); )
	    if (pp->psize != 0)
	    {   if (has_efs_superblock(device, pp))
		    create_efs_volume(device, pp);
	    }
	enddiskinfo(token);
    }
}
