#include <bstring.h>
#include <mntent.h>
#include <string.h>

#include "Device.H"
#include "Log.H"
#include "Partition.H"
#include "SimpleVolume.H"
#include "fmt_dos.H"


/*
 *-----------------------------------------------------------------------------
 * A Note on DOS partitions:
 * =========================
 * - Partitions with index = 1, 2, 3, 4 refer to primary partitions. These have
 *   partition table entries that reside in the very first sector on disk.
 * - Partitions with index = 5, 6, 7... refer to logical partitions. These are
 *   part of an extended partition. There can be only one extended partition.
 * - Partitions with index = 0 imply partitions that occupy entire devices. 
 *   Typical examples of these are floppies and flopticals; They have boot
 *   sectors in the very first sector.
 *-----------------------------------------------------------------------------
 */

class	FormatDSO;

/*
 *-----------------------------------------------------------------------------
 * create_dos_volume()
 * This routine instantiates a SimpleVolume 
 * for a single mountable dos partition. 
 *-----------------------------------------------------------------------------
 */
static	void	create_dos_volume(Partition *part, char *label)
{
	char	*opts;
	char	optbuf[12];
	int 	index = part->address().partition();
    	const 	char *fsname = part->device()->dev_name(FMT_DOS, index);

	if (index >= 1){
		(void) sprintf(optbuf, "partition=%d", index);
		opts = optbuf;
	}
	else	opts = NULL;
	mntent	mnt = { (char *) fsname, NULL, "dos", opts, 0, 0 };
	Log::debug("DOS volume label is \"%s\"", label);
    	(void) SimpleVolume::create(part, mnt, label);	
	return;	
}

/*
 *-----------------------------------------------------------------------------
 * trim_label()
 * This routine takes a 11 char string representing a label and converts it
 * into a null-terminated string, without trailing spaces.
 *-----------------------------------------------------------------------------
 */
static char  	*trim_label(char *label)
{
        char    *t;
        static  char trimmed[12];

        strncpy(trimmed, label, 11);
        for (t = trimmed+10; t >= trimmed; t--){
                if (*t != ' '){
                        *++t = '\0';
                        return (trimmed);
                }
        }
        *trimmed = '\0';
        return (trimmed);
}

/*
 *-----------------------------------------------------------------------------
 * read_label()
 * This routine is used to read in a dos label.
 *-----------------------------------------------------------------------------
 */
static  void    read_label(Device &device, int start_sect, char *bootsect,
                           int secsize, char *label)
{
        int     error;
        u_char  fatcount;
        u_short fatsize;
        u_short reservecount;
        u_int   label_sect;
        dfile_t fentry;
        char    buff[2048], *lptr = label;

        strcpy(lptr, "");
        fatsize  = FAT_SIZE(bootsect);
        fatcount = FAT_COUNT(bootsect);
        reservecount = RESERVE_COUNT(bootsect);
        label_sect   = (start_sect+reservecount+fatcount*fatsize);

        error = device.read_data(buff, label_sect, 1, secsize);
        if (error == 0){
                memcpy(&fentry, buff, sizeof fentry);
                if (fentry.df_attribute == (ATTRIB_ARCHIVE | ATTRIB_LABEL)){
                        strncpy(lptr, fentry.df_name, FILE_LEN);
                        strncpy(lptr+FILE_LEN, fentry.df_ext, EXTN_LEN);
			strcpy(label, trim_label(label));
                        return;
                }
        }
        return;
}

/*
 *-----------------------------------------------------------------------------
 * read_logical_partn()
 * This routine looks at all the logical partitions that are within an extended
 * partition and creates a dos volume for each one of them.
 *-----------------------------------------------------------------------------
 */
static  void  read_logical_partn(Device &device, int secsize, u_int extnd_sect)
{
        int     partn;
        int     error;
        pte_t   *p1, *p2;
	char	label[27];
        char    buf[2048];
        char    bootsect[2048];
        u_int   numbr_sect;
        u_int   data_sect;
	u_int	this_sect;
        u_int   part_sect;
        u_int   next_sect = 0;
        Partition       *part;
        
        partn = 5;
        part_sect = extnd_sect;
        do {
                error = device.read_data(buf, part_sect, 1, secsize);
                if (error == 0){
                        p1 = offset(buf, 0);
                        p2 = offset(buf, 1);    
                        next_sect = PARTN_START_SECT(p2);
			this_sect = PARTN_START_SECT(p1);
                        numbr_sect= PARTN_NUMBR_SECT(p1);
                        data_sect = part_sect+this_sect;
                        part_sect = extnd_sect+next_sect;
			

                        /*
                         * We've found a logical partition.
                         * We need to check if it's a dos volume.
                         * We need to create a dos volume for it.
                         */
                        error = device.read_data(bootsect, data_sect, 
						 1, secsize);
                        if (error == 0){
                                if (MAGIC_DOS(bootsect)){
					read_label(device, data_sect, bootsect, 
						   secsize,label);
                                        PartitionAddress paddr(device.address(),                                                               FMT_DOS, partn);
                                        part = Partition::create(paddr, 
                                                                 &device,
                                                                 secsize,
                                                                 data_sect,
                                                                 numbr_sect,
                                                                 "dos");
                                        create_dos_volume(part, label);
                                }
                        }
                }
                partn++;
        } while (next_sect);
        return;
}

/*
 *-----------------------------------------------------------------------------
 * read_primary_partn()
 * This routine looks at all the partitions within a device (primary/logical)
 * and creates a SimpleVolume for each mountable dos partition.
 *-----------------------------------------------------------------------------
 */
static	void	read_primary_partn(Device &device, int secsize, char *buf)
{
	int	partn;
	pte_t	*p;
	int	error;
	u_int	start_sect;
	u_int	numbr_sect;
	u_int	extnd_sect;
	char	label[27];
	char	bootsect[2048];
	Partition	*part;

	for (partn = 1; partn <= 4; partn++){
		/*
		 * Look at each primary partition.
		 * If primary partition is non-extended, create dos volume.
		 * If primary partition is extended, call read_logical_partn.
		 */
		p = offset(buf, partn-1);
		if (p->type != EXTENDED_PARTITION){
			/*
			 * We've found a non-extended partition.	
		 	 * We need to check if it's a dos volume.
			 */
			start_sect = PARTN_START_SECT(p);
			numbr_sect = PARTN_NUMBR_SECT(p);
			error = device.read_data(bootsect, start_sect, 1, secsize);
			if (error == 0){
				if (MAGIC_DOS(bootsect)){
					read_label(device, start_sect, 
						   bootsect, secsize,label);
					PartitionAddress paddr(device.address(),
							       FMT_DOS, partn);
					part = Partition::create(paddr, 
								 &device,
								 secsize,
								 start_sect,
								 numbr_sect,
							         "dos");
					create_dos_volume(part, label);
				}
			}
		}
		else {
			/* 
			 * This is an extended partition.
			 * Look through this for logical partitions.
			 */
			extnd_sect = PARTN_START_SECT(p);
			read_logical_partn(device, secsize, extnd_sect);
		}
	}
	return;
}

extern "C"
void    inspect(FormatDSO *, Device *device)
{
	// Does this device support this format?

	if (!device->filesys_dos())
		return;

        int     error;
        int     nsectors;
        int     secsize = device->sector_size(FMT_DOS);
	char	label[27];
        char    buf[2048];
        Partition       *part;

        error = device->read_data(buf, 0, 1, secsize);
        if (error == 0){
                if (MAGIC_DOS(buf)){
                        /*
                         * This is an unpartitioned DOS device
			 * Such devices are floppies and flopticals.
 			 * Their sector count is stored at offset: 0x13.
			 * 
                         */
			read_label(*device, 0, buf, secsize,label);
                        nsectors = SECTOR_COUNT(buf);
                        PartitionAddress paddr(device->address(), FMT_DOS,
                                               PartitionAddress::WholeDisk);
                        part = Partition::create(paddr, device, secsize,
                                                 0, nsectors, "dos");
                        create_dos_volume(part, label);
                }
                else if (MAGIC_PART(buf)){
                        /*
                         * This is a partitioned DOS device
                         */
                        read_primary_partn(*device, secsize, buf);
                }

        }
        return;
}

