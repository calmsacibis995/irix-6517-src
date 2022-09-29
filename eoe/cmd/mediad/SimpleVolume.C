#include "SimpleVolume.H"

#include <assert.h>
#include <limits.h>
#include <string.h>

#include "Device.H"
#include "Partition.H"

SimpleVolume::SimpleVolume(Partition *part,
			   const mntent& mnt,
			   const char *label)
: Volume(part->address(), mnt, label),
  _part(part)
{ }

SimpleVolume::~SimpleVolume()
{ }

unsigned int
SimpleVolume::npartitions() const
{
    return 1;
}

const Partition *
SimpleVolume::partition(unsigned int index) const
{
    return index == 0 ? _part : NULL;
}

void
SimpleVolume::use_subdir()
{
    char subdir[PATH_MAX];
    sprintf(subdir, "%s/%s", base_dir(), type());
    int partno = partition(0)->address().partition();
    if (partno != PartitionAddress::WholeDisk)
	sprintf(subdir + strlen(subdir), ".partition%d", partno);
    set_sub_dir(subdir);
}

//////////////////////////////////////////////////////////////////////////////

// Extra indirection so we can change the code later...

SimpleVolume *
SimpleVolume::create(Partition *part, const mntent& mnt, const char *label)
{
    // The directory should not be set in the mnt structure.
    // We will set it here.  Build the directory as "/<name>".

    mntent xmnt = mnt;
    assert(xmnt.mnt_dir == NULL);
    Device *dev = part->device();
    char dirbuf[MNTMAXSTR];
    sprintf(dirbuf, "/%s", dev->name());
    xmnt.mnt_dir = dirbuf;
    
    // Instantiate a SimpleVolume and return it.

    SimpleVolume *vol = new SimpleVolume(part, xmnt, label);
    return vol;
}
