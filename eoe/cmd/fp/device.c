/*
 *============================================================================
 *		File:		device.c
 *		Purpose:	This file contains code that handles querying
 *				of disk geometry. 
 *============================================================================
 */
#include "device.h"
#include <sys/conf.h>
#include <invent.h>
#include <sys/iograph.h>

/* Functions defined in libdisk */
extern int valid_vh(struct volume_header *);
extern int vhchksum(struct volume_header *);

/*
 *----------------------------------------------------------------------------
 * External Interface
 *----------------------------------------------------------------------------
 */
int	scsi_query(void);
int	scsi_is_floppy(u_int capacity);
/*
 *----------------------------------------------------------------------------
 * Function prototypes
 *----------------------------------------------------------------------------
 */
static	int 	gioctl(int cmd, void *ptr);
static	u_int	scsi_sector_size(void);
static  int  	scsi_readcapacity(unsigned *addr);
static	int  	scsi_initlabel(void);
static	void	check_geometry(u_int capacity, u_int *h, u_int *s, u_int *c);
static	int     set_vh(struct volume_header *vp);
static	int     get_vh(struct volume_header *vp);
static 	void 	sc_sh_bytes(int from, u_char *to);
static  void 	sc_uint_3bytes(int from, u_char *to);
static  uint 	sc_3bytes_uint(u_char *a);
static  ushort 	sc_bytes_sh(u_char *a);

/*
 *----------------------------------------------------------------------------
 * External variables
 *----------------------------------------------------------------------------
 */
extern	int	debug_flag;
extern	int	verbose_flag;	/* Flag for verbosity */
extern	int	fd;		/* Device descr. */
extern	u_int	heads;		/* Disk Geometry */
extern	u_int	sectors;	/* Disk Geometry */
extern	u_int	cylinders;	/* Disk Geometry */
extern	char	*disk_device;	/* Device name   */

/*
 *----------------------------------------------------------------------------
 * Global variables
 *----------------------------------------------------------------------------
 */
u_int	 capacity = 0;		/* Disk Capacity in sectors */
u_int	 scsi_cap = 0;
struct 	 volume_header	vh;

/*
 *----------------------------------------------------------------------------
 * scsi_query()
 * This routine provides the interface with the rest of the code.
 *----------------------------------------------------------------------------
 */
int	scsi_query(void)
{
	int	error;

	bzero(&vh, sizeof(vh));
	error = scsi_initlabel();
	if (error) return (1);
	check_geometry(capacity, &heads, &sectors, &cylinders);
	return (0);
}

/*
 *----------------------------------------------------------------------------
 * scsi_is_floppy()
 * This routine checks a device's capacity and determines if it's a floppy or
 * floptical, or not...
 *----------------------------------------------------------------------------
 */
int	scsi_is_floppy(u_int capacity)
{
        switch (capacity){
        case    720:    /* FD_FLOP        */
        case    1440:   /* FD_FLOP_35LO   */
        case    2880:   /* FD_FLOP_35     */
        case    2400:   /* FD_FLOP_AT     */
        case    40662:  /* FD_FLOP_35_20M */
                return (1);
        }
        return (0);     /* FD_FLOP_GENERIC */
}

/*
 *----------------------------------------------------------------------------
 * check_geometry()
 * This routine takes a look at the physical geometry information and
 * then either lets them remain as such or fakes values for them.
 *----------------------------------------------------------------------------
 */
static	void    check_geometry(u_int capacity, u_int *h, u_int *s, u_int *c)
{
	if (capacity*SECTOR_SIZE >= 1*GB){
		*h = HMAX_HEADS;
		*s = HMAX_SECTORS;
		*c = capacity/(HMAX_HEADS*HMAX_SECTORS);
	}
	else {
		*h = LMAX_HEADS;
		*s = LMAX_SECTORS;
		*c = capacity/(LMAX_HEADS*LMAX_SECTORS);
	}
#ifdef	DEBUG_GEOMETRY
	if (verbose_flag){
		printf(" mkfp: Geometry: heads 	   = %d\n", *h);
		printf(" mkfp: Geometry: sectors   = %d\n", *s);
		printf(" mkfp: Geometry: cylinders = %d\n", *c);
	}
#endif
	return;
}

/*
 *----------------------------------------------------------------------------
 * scsi_initlabel()
 * This routine reads in the volume header.
 * If this is invalid, get the drive type and set up the defaults.
 *----------------------------------------------------------------------------
 */
static int
scsi_initlabel(void)
{
	int	ret;
	int	part;
        struct  stat    sb;
        struct  partition_table *pt;
        struct  volume_header   vh;

	char	*str;
	char	*pattern = EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR;
	char	devname[MAXDEVNAME];
	int	len1, len2;

        if ((fd = open(disk_device, O_RDWR | O_EXCL)) < 0){
		gerror("open() fail");
                return (errno);
	}
        if (fstat(fd, &sb) < 0){
                close(fd);
                return (errno);
        }
	if (fdes_to_drivername(fd, devname, &len2)) {
		if(strncmp(devname, "dksc", 4) != 0)
			if (verbose_flag)
				printf("mkfp: Device is dksc major\n");
	}
	if (scsi_readcapacity(&capacity) == -1) {
                close(fd);
                return (errno);
        } else if (debug_flag) {
	    printf("device.c:scsi_initlabel: capacity = %d\n", capacity);
	}

	if (verbose_flag){
		printf(" mkfp: Capacity: = %s\n", ground(capacity*SECTOR_SIZE));
	}

	if (get_vh(&vh) == 0){
            if (valid_vh(&vh)){
		  /*
		   * Check that the device specified pertains to
		   * a whole volume device, and not one specific
		   * partition inside the device.
		   */
                  len1 = strlen(pattern);
                  len2 = MAXDEVNAME;

                  dev_to_devname(sb.st_rdev, devname, &len2);
                  len2 = strlen(devname);

                  str = devname + (len2-len1);
                  if (!strcmp(pattern, str)){
                        part = 10;
                        pt = &vh.vh_pt[part];
                  }
                  else  part = -1;

                  if (part != 10) {
                        close(fd);
                        return (ENODEV);
                  }
                  bzero(&vh, sizeof vh);
                  vh.vh_magic          = VHMAGIC;
		  vh.vh_dp.dp_drivecap = capacity;
		  vh.vh_dp.dp_secbytes = scsi_sector_size();
                  pt->pt_nblks         = capacity;
                  pt->pt_firstlbn      = 0;
                  pt->pt_type          = PTYPE_RAW;
                  vh.vh_csum           = -vhchksum(&vh);
		  (void) set_vh(&vh);
            }
        }
        return (0);
}

/*
 *----------------------------------------------------------------------------
 * get_vh()
 * This routine gets volume header from driver
 *----------------------------------------------------------------------------
 */
static	int 	get_vh(struct volume_header *vp)
{
        if( gioctl(DIOCGETVH, vp) < 0 ){
                fprintf(stderr, "no volume header available from driver");
		return (1);
	}
	return (0);
}

/*
 *----------------------------------------------------------------------------
 * set_vh()
 * This routine sets volume header in driver
 *----------------------------------------------------------------------------
 */
static	int 	set_vh(struct volume_header *vp)
{
        vp->vh_magic = VHMAGIC;
        vp->vh_csum = 0;
	vp->vh_csum = -vhchksum(vp);

        if( gioctl(DIOCSETVH, vp) < 0 ){
                fprintf(stderr, "can't set volume header in driver");
		return (1);
	}
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * scsi_readcapacity()
 * This routine is used to read the capacity of the device 
 *-----------------------------------------------------------------------------
 */
static	scsi_readcapacity(unsigned *addr)
{
        if (gioctl(DIOCREADCAPACITY, addr) < 0) {
                *addr = 0;      
                gerror("reading drive capacity");
                return -1;
        } else if (debug_flag) {
	    printf("device.c:scsi_readcapacity: capacity = %d\n", *addr);
	}

        scsi_cap = *addr;       

        return 0;
}

/*
 *----------------------------------------------------------------------------
 * scsi_sector_size()
 *----------------------------------------------------------------------------
 */
static  u_int scsi_sector_size(void)
{
#define	SENSE_LEN_ADD	(8+4+2)

    int				i, maxd, pgnum;
    int				secsize;
    u_char			pglengths[ALL+1];
    u_char			*d;
    struct dk_ioctl_data	sense;
    struct mode_sense_data	sense_data;


    d = (u_char *)&sense_data;

    /*
     * 1st, try to read ALL pages, so's we can figure out
     * whether the device actually has page 3 (device format).
     */
    bzero(pglengths, sizeof(pglengths));

    pglengths[ALL] = sizeof(sense_data) - (1 + SENSE_LEN_ADD);

    sense.i_addr = (void *)&sense_data;
    sense.i_len  = pglengths[ALL] + SENSE_LEN_ADD;
    sense.i_page = ALL|CURRENT;

				/* only one byte! (don't want modulo) */
    if (sense.i_len > 0xff)
	sense.i_len = 0xff;

    bzero(&sense_data, sizeof sense_data);

    if (gioctl(DIOCSENSE, &sense) < 0) {
	if (debug_flag)
	    printf("device.c:scsi_sector_size: bad 1st ioctl, secsz=512\n");

	return 512;
    }

    /*
     * sense_len doesn't include itself;
     * set cd->pglengths[ALL] for completeness
     */
    pglengths[ALL] = maxd = sense_data.sense_len + 1;

    /*
     * Scan through the pages to determine which
     * are actually supported.
     */
    if (sense_data.bd_len > 7)
	i = 4 + sense_data.bd_len;	/* skip header and block desc. */
    else
	i = 4;				/* skip just the header */

    while (i < maxd) {
	pgnum = d[i] & ALL;

	pglengths[pgnum] = d[i+1];

	i += pglengths[pgnum] + 2;	/* +2 for header */
    }

    if (debug_flag > 2) {
	for (i = 0; i < (ALL+1); i++)
		printf("device.c:scsi_sector_size: pg[0x%x]: length/%d\n",
                                                        i, pglengths[i]);
    }

    if (pglengths[3] == 0) {
	if (debug_flag)
		printf("device.c:scsi_sector_size: No page 3!, secsz=512\n");

	return 512;
    }


    sense.i_addr = (void *) &sense_data;
    sense.i_len = sizeof sense_data;
    sense.i_page = 3;

    bzero(&sense_data, sizeof sense_data);

    if (gioctl(DIOCSENSE, &sense) != 0) {
	if (debug_flag)
		printf("device.c:scsi_sector_size: bad 2nd ioctl, secsz=512\n");

	return 512;			/* default sector size */
    }

    if (sense_data.bd_len < 8) {
	if (debug_flag)
		printf("device.c:scsi_sector_size: bad bd_len, secsz=512\n");

	return 512;
    }

    secsize = (sense_data.block_descrip[5] << 16 |
		    sense_data.block_descrip[6] << 8 |
		    sense_data.block_descrip[7]);
    if (debug_flag)
	    printf("device.c:scsi_sector_size: secsz=%d\n", secsize);

    return secsize ? secsize : 512;
}

/*
 *-----------------------------------------------------------------------------
 * ioctl on the disk, with error reporting
 *-----------------------------------------------------------------------------
 */
static	int gioctl(int cmd, void *ptr)
{
        errno = 0;      
        return ioctl(fd, cmd, ptr);
}



