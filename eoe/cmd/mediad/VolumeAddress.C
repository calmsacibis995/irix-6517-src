#include "VolumeAddress.H"

#include <assert.h>
#include <devconvert.h>
#include <diskinvent.h>
#include <stdio.h>
#include <sys/conf.h>
#include <sys/dir.h>

#include "DeviceAddress.H"
#include "PartitionAddress.H"
#include "operators.H"

VolumeAddress::VolumeAddress(const VolumeAddress& that)
: _refcount(0)
{
    if (that._refcount == 0)
    {
	//  That is a VolumeAddress, not a derived class.
	//  Share its rep.

	_rep = that._rep;
	if (_rep)
	    _rep->_refcount++;
    }
    else
    {
	//  That is a derived class.  Clone it.
	
	_rep = that.clone();
    }	
}

VolumeAddress&
VolumeAddress::operator = (const VolumeAddress& that)
{
    if (this != &that)
    {   if (_rep && !--_rep->_refcount)
	    delete _rep;
	_rep = that._rep;
	if (_rep)
	    _rep->_refcount++;
    }
    return *this;
}

VolumeAddress::~VolumeAddress()
{
    assert(!_rep || _rep->_refcount > 0);
    if (_rep && !--_rep->_refcount)
	delete _rep;
}

VolumeAddress *
VolumeAddress::clone() const
{
    return new VolumeAddress(*this);
}

const PartitionAddress *
VolumeAddress::partition(unsigned index) const
{
    if (_rep)
	return _rep->partition(index);
    else
	return NULL;
}

bool
VolumeAddress::operator == (const VolumeAddress& that) const
{
    if (this == &that)
	return true;
    if (_rep)
	return *_rep == that;
    if (that._rep)
	return *this == *that._rep;
    for (int i = 0; ; i++)
    {   const PartitionAddress *a = partition(i);
	const PartitionAddress *b = that.partition(i);
	if (!a || !b)
	    return !a && !b;
	if (a->partition() != b->partition())
	    return false;
	if (a->device() != b->device())
	    return false;
    }
}

bool
VolumeAddress::overlaps(const VolumeAddress& that) const
{
    if (_rep == that._rep)
	return true;
    if (!_rep || !that._rep)
	return false;
    const PartitionAddress *p;
    for (int i = 0; p = partition(i); i++)
	if (p->overlaps(that))
	    return true;
    return false;
}

const PartitionAddress *
VolumeAddress::as_PartitionAddress() const
{
    return _rep ? _rep->as_PartitionAddress() : NULL;
}

PartitionAddress *
VolumeAddress::as_PartitionAddress()
{
    return _rep ? _rep->as_PartitionAddress() : NULL;
}

static bool match_scsi_dev(dev_t dev, int *ctlrp, int *unitp, int *lunp)
{
    static struct SCSI_Dev {
	dev_t dev;
	int ctlr, unit, lun;
    } *devs;
    static int ndevs;
    if (!devs)
    {
	// devs is not initialized.  Read /dev/scsi, count all entries,
	// and store them away.

	DIR *D = opendir("/dev/scsi");
	direct *dp;
	int ctlr, unit, lun;

	if (!D)
	    return false;
	ndevs = 0;
	while (dp = readdir(D))
	    if (sscanf(dp->d_name, "sc%dd%dl%d", &ctlr, &unit, &lun) == 3)
		ndevs++;
	closedir(D);
	D = opendir("/dev/scsi");
	if (!D)
	    return false;
	devs = new SCSI_Dev[ndevs];
	for (int i = 0; i < ndevs; )
	{   if (!(dp = readdir(D)))
		break;
	    if (sscanf(dp->d_name, "sc%dd%dl%d", &ctlr, &unit, &lun) != 3)
		continue;
	    struct stat status;
	    char fullpath[100];
	    sprintf(fullpath, "/dev/scsi/%s", dp->d_name);
	    if (stat(fullpath, &status) != 0)
		continue;
	    devs[i].dev = status.st_rdev;
	    devs[i].ctlr = ctlr;
	    devs[i].unit = unit;
	    devs[i].lun = lun;
	    i++;
	}
	closedir(D);
    }

    // Look at all SCSI devices, find the match.

    for (SCSI_Dev *p = devs; p < devs + ndevs; p++)
	if (p->dev == dev)
	{   *ctlrp = p->ctlr;
	    *unitp = p->unit;
	    *lunp = p->lun;
	    return true;
	}
    return false;
}

VolumeAddress::VolumeAddress(const char *path, FormatIndex fmt, int partno)
: _refcount(0), _rep(NULL)
{

    // Map the device to a /dev/scsi device.

    char scsipath[MAXDEVNAME];
    int len = sizeof scsipath;
    if (filename_to_scsiname(path, scsipath, &len) != NULL)
    {
	// This is a scsi device.
	// Find out which /dev/scsi device we found.

	struct stat status;
	if (stat(scsipath, &status) != 0)
	    return;
	int ctlr, unit, lun;
	if (!match_scsi_dev(status.st_rdev, &ctlr, &unit, &lun))
	    return;
    
	// If this is a disk, we need to figure out which partition it
	// hits.

	int partition = partno;
	if (partition == UnknownPartition)
	{   partition = path_to_partnum((char *) path);
	    if (partition == -1)
		partition = WholeDisk;
	}

	// Build the DeviceAddress and the PartitionAddress

	_rep = new PartitionAddress(DeviceAddress(ctlr, unit, lun),
				    fmt, partition);
    }
    else if (!strcmp(path, "/dev/pfd/pfd.2dd") ||
	     !strcmp(path, "/dev/pfd/pfd.2hd"))
    {
	// This is a parallel device.

	int partition = partno;
	if (partition == UnknownPartition)
	    partition = WholeDisk;
	_rep = new PartitionAddress(DeviceAddress(DeviceAddress::Parallel()),
				    fmt, partition);
    }
//  else (_rep == NULL) => invalid VolumeAddress
}
