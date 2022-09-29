#include <sys/param.h>
#include <bstring.h>
#include <diskinfo.h>
#include <sys/dvh.h>
#include <sys/file.h>

#include "Device.H"
#include "Log.H"
#include "Partition.H"
#include "PartitionAddress.H"
#include "SimpleVolume.H"

class FormatDSO;

// The XFS include files containing internal details such as the
// definition of the super block aren't public.  We must know the
// size of the super block, so (re)define it here.

const int XFS_SB_SIZE = 512;		// size of an XFS super block

// The first parameter to valid_xfs is a super block pointer
// but since we don't have the definition pass it as a char *.

extern "C" int valid_xfs(char *sblk, long long size);	// (from libdisk)

static void
create_xfs_volume(Device *device, ptinfo *pp)
{
    PartitionAddress paddr(device->address(), FMT_XFS, pp->ptnum);
    Partition *part = Partition::create(paddr,
					device,
    					device->sector_size(FMT_XFS),
					pp->pstartblk,
					pp->psize,
					"xfs");
    const char *fsname = device->dev_name(FMT_XFS, pp->ptnum);
    mntent mnt = { (char *) fsname, NULL, "xfs", NULL, 0, 0 };
    (void) SimpleVolume::create(part, mnt, "");
}

bool has_xfs_superblock(Device *dev, const ptinfo *pp)
{
    char sbuf[XFS_SB_SIZE];
    __uint64_t sectno = pp->pstartblk;
    unsigned int nsects = 1;

    if (dev->read_data(sbuf, sectno, nsects, XFS_SB_SIZE) != 0)
    {   Log::critical("can't read sector %lld of device %s: %m",
		      sectno, dev->name());
	return false;
    }
    return valid_xfs(sbuf, pp->psize) == 1;
}

extern "C"
void
inspect(FormatDSO *, Device *device)
{
    // Does this device support this format?

    if (!device->filesys_xfs())
	return;

    char buf[2048];
    bzero(buf, sizeof buf);
    int secsize = device->sector_size(FMT_XFS);

    if (device->read_data(buf, 0, sizeof buf / secsize, secsize) != 0)
	return;

    if (valid_vh((volume_header *) buf) == -1)
	return;

    const char *xfsname = device->dev_name(FMT_XFS, 8);
    if (strncmp(xfsname, "/dev/dsk/", 9))
	return;

    char rawfsname[30];
    sprintf(rawfsname, "/dev/rdsk/%s", xfsname + 9);
    Log::debug("XFS inspect: rawfsname = \"%s\"", rawfsname);
    unsigned token = setdiskinfo(rawfsname, "/etc/fstab", O_RDONLY);
    if (token)
    {   for (ptinfo *pp; pp = getpartition(token); )
	    if (pp->psize != 0)
	    {   if (has_xfs_superblock(device, pp))
		    create_xfs_volume(device, pp);
	    }
	enddiskinfo(token);
    }
}
