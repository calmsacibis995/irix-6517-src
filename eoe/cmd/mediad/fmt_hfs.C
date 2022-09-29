#include <bstring.h>
#include <mntent.h>
#include <string.h>

#include "Device.H"
#include "Log.H"
#include "Partition.H"
#include "SimpleVolume.H"

class FormatDSO;

struct HFS_DriverDescriptorBlock {
    int   ddBlock;			// first block of the driver
    short ddSize;			// size of the driver in blocks
    short ddType;			// system type, has value of 1 for 
					// most models
};

struct HFS_Block0 {
    short sbSig;			// always has value of $4552
    short sbBlkSize;			// block size of the device
    int   sbBlkCount;			// number of blocks on the device
    short sbDevType;			// type code for the device
    short sbDevId;			// ID of the device
    int   sbData;			// reserved
    short sbDrvrCount;			// number of driver descriptor 
					// entries
    HFS_DriverDescriptorBlock sbDescriptors[1];
};

struct HFS_PartitionMapEntry {
    short pmSig;			// always has value of $504D
    short pmSigPad;			// reserved
    int   pmMapBlkCnt;			// number of blocks in map
    int   pmPyPartStart;		// first physical block of partition
    int   pmPartBlkCnt;			// number of blocks in partition
    char  pmPartName[32];		// partition name
    char  pmParType[32];		// partition type
    int   pmLgDataStart;		// first logical block of data area
    int   pmDataCnt;			// number of blocks in data area
    int   pmPartStatus;			// partition status information
    int   pmLgBootStart;		// first logical block of boot code
    int   pmBootSize;			// size in bytes of boot code
    int   pmBootAddr;			// boot code load address
    int   pmBootAddr2;			// additional boot information
    int   pmBootEntry;			// boot code entry point
    int   pmBootEntry2;			// additional boot entry information
    int   pmBootCkSum;			// boot code checksum
    char  pmProcessor[16];		// processor type
    short pmPad[187];			// boot-specific arguments
};

struct HFS_ExtDescriptor {		// extent descriptor
    short xdrStABN;			// first allocation block
    short xdrNumABlks;			// number of allocation blocks
};

typedef HFS_ExtDescriptor HFS_ExtDataRec[3];
    
struct HFS_MDB {			// master directory block

    short	   drSigWord;	// volume signature
    short	   drCrDate[2];	// date and time of volume creation
    short	   drLsMod[2];	// date and time of last modification
    short	   drAtrb;	// volume attributes
    short	   drNmFls;	// number of files in root directory
    short	   drVBMSt;	// first block of volume bitmap
    short	   drAllocPtr;	// start of next allocation search
    short	   drNmAlBlks;	// number of allocation blocks in volume
    int		   drAlBlkSiz;	// size (in bytes) of allocation blocks
    int		   drClpSiz;	// default clump size
    short	   drAlBlSt;	// first allocation block in volume}
    short	   drNxtCNID[2];	// next unused catalog node ID
    short	   drFreeBks;	// number of unused allocation blocks
    char	   drVN[27];	// volume name
    int		   drVolBkUp;	// date and time of last backup
    short	   drVSeqNum;	// volume backup sequence number
    int		   drWrCnt;	// volume write count
    int		   drXTClpSiz;	// clump size for extents overflow file
    int		   drCTClpSiz;	// clump size for catalog file
    short	   drNmRtDirs;	// number of directories in root directory
    int		   drFilCnt;	// number of files in volume
    int		   drDirCnt;	// number of directories in volume
    int		   drFndrInfo[8];// information used by the Finder
    short	   drVCSize;	// size (in blocks) of volume cache
    short	   drVBMCSize;	// size (in blocks) of volume bitmap cache
    short	   drCtlCSize;	// size (in blocks) of common volume cache
    int		   drXTFlSize;	// size of extents overflow file
    HFS_ExtDataRec drXTExtRec;	// extent record for extents overflow file
    int		   drCTFlSize;	// size of catalog file
    HFS_ExtDataRec drCTExtRec;	// extent record for catalog file
};

///////////////////////////////////////////////////////////////////////////////

static void
create_hfs_volume(Partition *part, const HFS_MDB *mdb)
{
    int index = part->address().partition();
    const char *fsname = part->device()->dev_name(FMT_HFS, index);
    const char *volname = mdb->drVN;
    char *opts;
    char label[27];
    char optbuf[12];

    //	Convert a Mac string (chars preceded by length byte)
    //	to a Unix string (null-terminated).

    unsigned char length = (unsigned char) *volname;
    strncpy(label, volname + 1, length);
    label[length] = '\0';

    if (index > 1)
    {	(void) sprintf(optbuf, "partition=%d", index);
	opts = optbuf;
    }
    else
	opts = NULL;
    mntent mnt = { (char *) fsname, NULL, "hfs", opts, 0, 0 };
    Log::info("HFS volume label is \"%s\"", label);
    (void) SimpleVolume::create(part, mnt, label);
}

///////////////////////////////////////////////////////////////////////////////

static void
read_partition_map(Device& device, int secsize, char *block)
{
    //  Read the partition map; count the HFS partitions.
    //  First block is already in block1.

    int nparts = 0;
    int npartmapblocks = 1;
    for (int map_block = 0; map_block < npartmapblocks; map_block++)
    {   if (map_block == 0 ||
	    device.read_data(block, map_block + 1, 1, secsize) == 0)
	{   HFS_PartitionMapEntry *pm, *pmp, *pm_end;
	    pm = (HFS_PartitionMapEntry *) block;
	    pm_end = (HFS_PartitionMapEntry *) (block + secsize);
	    for (pmp = pm; pmp < pm_end; pmp++)
	    {   if (pmp->pmSig != 0x504d && pmp->pmSig != 0x5453)
		    break;
		if (map_block == 0 && pmp == pm)
		    npartmapblocks = pmp->pmMapBlkCnt;
		if (!strcmp(pmp->pmParType, "Apple_HFS"))
		    nparts++;
	    }
	}
    }
    if (nparts > 0)
    {
	// Read the partition map again and create the HFS volumes.
	// If there's just one HFS partition, use WholeDisk for
	// the partition number, otherwise use 1, 2, ...

	int partno = nparts == 1 ? PartitionAddress::WholeDisk : 1;

	for (int map_block = 0; map_block < npartmapblocks; map_block++)
	{   if ( device.read_data(block, map_block + 1, 1, secsize) == 0)
	    {   HFS_PartitionMapEntry *pm, *pmp, *pm_end;
		pm = (HFS_PartitionMapEntry *) block;
		pm_end = (HFS_PartitionMapEntry *) (block + secsize);
		for (pmp = pm; pmp < pm_end; pmp++)
		{   if (pmp->pmSig != 0x504d && pmp->pmSig != 0x5453)
			break;
		    if (!strcmp(pmp->pmParType, "Apple_HFS"))
		    {   char buf[512];
			if (device.read_data(buf, pmp->pmPyPartStart + 2,
					     1, secsize) == 0)
			{   PartitionAddress paddr(device.address(),
						   FMT_HFS, partno);
			    Partition *part = Partition::create(paddr, &device,
								secsize,
								pmp->pmPyPartStart,
								pmp->pmPartBlkCnt,
								"hfs");
			    create_hfs_volume(part, (HFS_MDB *) buf);
			}
			partno++;
		    }
		}
	    }
	}
    }
    Log::info("Device %s has %d HFS partition%s",
	       device.full_name(), nparts, nparts == 1 ? "" : "s");
}

//////////////////////////////////////////////////////////////////////////////
//  This is the code that recognized an HFS disk in old mediad.
//
//	if ((buf[1024] == 0x42 && buf[1025] == 0x44 &&
//	     buf[1038] == 0x00 && buf[1039] == 0x03) ||
//	    (buf[0] == 0x45 && buf[1] == 0x52) ||
//	    (buf[512] == 0x50 && buf[513] == 0x4d &&
//	     buf[1024] == 0x50 && buf[1025] == 0x4d))
//	{
//		// got one...
//	}
// The test "buf[1038..1039] == 0x0003" seems unnecessary, so I
// left it out.  There is a note in Inside Macintosh: Files
// on page 2-60 to the effect that drVBMSt is currently 3,
// but that doesn't seem to be a prerequisite to mounting the filesystem.

bool
all_zeros(char *p, unsigned int nbytes)
{
    for (unsigned int i = 0; i < nbytes; i++)
	if (p[i] != '\0')
	    return false;
    return true;
}

extern "C"
void
inspect(FormatDSO *, Device *device)
{
    // Does this device support this format?

    if (!device->filesys_hfs())
	return;

    char buf[2048];
    bzero(buf, sizeof buf);
    int secsize = device->sector_size(FMT_HFS);
    if (2 * secsize > sizeof buf)
    {   Log::critical("\"%s\" sector size of %d too large for HFS",
		   device->short_name(), secsize);
	return;				// Give up.
    }
    if (device->read_data(buf, 0, sizeof buf / secsize, secsize) == 0)
    {
	//  Look for HFS Block 0 in sector 0.
	//  Or, look for HFS partition map entry in sector 1.

	HFS_Block0 *blk0 = (HFS_Block0 *) buf;
	HFS_PartitionMapEntry *pmp = (HFS_PartitionMapEntry *) (buf + secsize);
	HFS_MDB *mdb = (HFS_MDB *) (buf + 2 * secsize);
	if (blk0->sbSig == 0x4552 &&
	    (pmp->pmSig == 0x504d || pmp->pmSig == 0x5453))
	{
	    // Appears to be a partitioned HFS volume.
	    read_partition_map(*device, secsize, buf + secsize);
	}
	else if (3 * secsize <= sizeof buf && mdb->drSigWord == 0x4244)
	{
	    // Appears to be unpartitioned HFS volume (i.e., floppy)

	    Log::info("device has unpartitioned HFS filesystem.");
	    int nsectors = device->capacity();
	    PartitionAddress paddr(device->address(), FMT_HFS,
				   PartitionAddress::WholeDisk);
	    Partition *part = Partition::create(paddr, device,
						secsize,
						0,
						nsectors,
						"hfs");
	    create_hfs_volume(part, mdb);
	}
	else if (all_zeros(buf, 512))
	{
	    //  N.B., The CD-ROM "A.D.A.M, The Inside Story" has all zeros
	    //  in sector 0 followed by a normal HFS partition map in
	    //  sector 1.  So we accept a disk with all zeros in sector 0,
	    //  if it doesn't have a valid MDB at sector 2.
	    //  I don't know how common disks like that are.

	    read_partition_map(*device,secsize, buf + secsize);
	}
    }
}
