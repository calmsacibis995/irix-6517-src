/*
 * issue cmds through the sable disk interface
 *
 * $Id: simdisk.c,v 1.1 1997/06/05 23:39:23 philw Exp $
 */

#ifdef SIMHIM

#include "stdio.h"
#include "fcntl.h"
#include "stdlib.h"
#include "bstring.h"
#include "sys/scsi.h"
#include "sys/mace.h"
#include "dslib.h"
#include "sys/mips_addrspace.h"
#include "sys/IPMHSIM.h"

#include "../../../../IP32/include/DBCsim.h"

/*
 * From adp78.h
 */
typedef struct sg_element {
	char	*data_ptr;
	int	data_len;
} SG_ELEMENT;

#define SG_LAST_ELEMENT		0x80000000

extern void osm_swizzle(unsigned char *, uint);

static int verbose_disk=0;
static int disk_targ=-1;

static int	init_disk(int);
static int	do_testunitready(unsigned char *, SG_ELEMENT *);
static int	do_inquiry(unsigned char *, SG_ELEMENT *);
static int	do_modesense(unsigned char *, SG_ELEMENT *);
static int	do_startstop(unsigned char *, SG_ELEMENT *);
static int	do_g1read(unsigned char *, SG_ELEMENT *);
static int	do_g1write(unsigned char *, SG_ELEMENT *);

/*
 * Entry point into the sable disk world.  Calls other functions to
 * do the actual work for every type of command.
 *
 * Returns 0 if command was successful, -1 otherwise.
 * hacked in 1 for selection timeout.
 * XXX This is actually wrong.  It does not look at the controller number.
 */
int
do_devscsi(int targ, unsigned char *cdbbuf, SG_ELEMENT *sgp)
{
	int rval;

	if (init_disk(targ)) {
		return -1;
	}

	if (targ != disk_targ) {
		return 1;
	}

	if (verbose_disk)
		DPRINTF("do_devscsi: cmd is %x %x %x %x\n",
			cdbbuf[0], cdbbuf[1], cdbbuf[2], cdbbuf[3]);

	switch(cdbbuf[0]) {
	case G0_TEST:
		rval = do_testunitready(cdbbuf, sgp);
		break;

	case G0_INQU:
		rval = do_inquiry(cdbbuf, sgp);
		break;

	case G0_MSEN:
		rval = do_modesense(cdbbuf, sgp);
		break;

	case G0_STOP:
		rval = do_startstop(cdbbuf, sgp);
		break;

	case G1_READ:
		rval = do_g1read(cdbbuf, sgp);
		break;

	case G1_WRIT:
		rval = do_g1write(cdbbuf, sgp);
		break;

	default:
		DPRINTF("do_devscsi: unsupported command 0x%x\n",
		       cdbbuf[0]);
		rval = -1;
		break;
	}

	return rval;
}

/*
 * Copy the data from the sglist into a single local buffer.
 * Then take the values out of the cdb to make a dslib call.
 *
 * returns 0 on success, -1 on error.
 */
static int
do_g1write(unsigned char *cdb, SG_ELEMENT *sgp)
{
	int rval, blocks, lun;
	unsigned int blocknum, size=0, transfered, native;
	SG_ELEMENT *tsgp=sgp;
	char *write_buf_ptr;

	blocks = (cdb[7] << 8) | cdb[8];
	blocknum = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
	lun = 0;

	/* calculate the size of the temporary write buffer needed */
	while (!(tsgp->data_len & SG_LAST_ELEMENT)) {
		size = size + sgp->data_len;
		tsgp++;
	}
	size += (sgp->data_len & (~SG_LAST_ELEMENT));

	if (verbose_disk)
		DPRINTF("g1write: block num: 0x%x blocks: 0x%x (0x%x bytes)\n",
			blocknum, blocks, size);

	/*
	 * copy the data from the sg list into the sable data area.
	 */
	tsgp = sgp;
	write_buf_ptr = (char *) MHSIM_DISK_DATA;
	while (!(tsgp->data_len & SG_LAST_ELEMENT)) {
		bcopy((char *) ((int) tsgp->data_ptr & (~PCI_NATIVE_VIEW)),
		      write_buf_ptr,
		      tsgp->data_len);
		write_buf_ptr += tsgp->data_len;
		tsgp++;
	}
	bcopy((char *) ((int) tsgp->data_ptr & (~PCI_NATIVE_VIEW)),
	      write_buf_ptr,
	      (sgp->data_len & (~SG_LAST_ELEMENT)));

	/*
	 * Poke sable disk registers
	 */
	*(int *)MHSIM_DISK_DISKNUM = disk_targ;
	*(int *)MHSIM_DISK_SECTORNUM = blocknum;
	*(int *)MHSIM_DISK_SECTORCOUNT = blocks;
	*(int *)MHSIM_DISK_OPERATION = MHSIM_DISK_WRITE;

	rval = *(int *)MHSIM_DISK_STATUS;
	transfered = *(int *)MHSIM_DISK_BYTES_TRANSFERRED;
	if (verbose_disk)
		DPRINTF("g1write: status 0x%x bytes transfered 0x%x\n",
			rval, transfered);
	if (rval || transfered != size) {
		return -1;
	}

	return 0;
}

/*
 * Take the values out of the cdb so we can call the dslib entry
 * point.  Have the data from the read go into a local buffer.
 * Then copy the buffer into the memory pieces pointed to by the
 * sglist.
 *
 * returns 0 on success, -1 on error.
 */
static int
do_g1read(unsigned char *cdb, SG_ELEMENT *sgp)
{
	int rval, blocks, lun;
	unsigned int blocknum, size=0, transfered, native;
	SG_ELEMENT *tsgp=sgp;
	char *read_buf_ptr;

	blocks = (cdb[7] << 8) | cdb[8];
	blocknum = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
	lun = 0;

	/*
	 * Use the first sg element only to see if reader wants
	 * native view or hardware swapped view.  Actually, for read and
	 * write, the disk driver should always pass the data back big
	 * endien.  (no swapping of disk data)
	 */
	if (((int) tsgp->data_ptr) & PCI_NATIVE_VIEW) {
		if (verbose_disk)
			DPRINTF("native view, no hardware swapping requested\n");
		native = 1;
	} else {
		if (verbose_disk)
			DPRINTF("hardware swapping requested\n");
		native = 0;
	}

	/* calculate the size from the sg list */
	while (!(tsgp->data_len & SG_LAST_ELEMENT)) {
		size += tsgp->data_len;
		tsgp++;
	}
	size += (tsgp->data_len & (~SG_LAST_ELEMENT));

	if (verbose_disk)
		DPRINTF("g1read: block num: 0x%x blocks: 0x%x (0x%x bytes)\n",
			blocknum, blocks, size);

	/*
	 * Poke sable disk registers
	 */
	*(int *)MHSIM_DISK_DISKNUM = disk_targ;
	*(int *)MHSIM_DISK_SECTORNUM = blocknum;
	*(int *)MHSIM_DISK_SECTORCOUNT = blocks;
	*(int *)MHSIM_DISK_OPERATION = MHSIM_DISK_READ;

	rval = *(int *)MHSIM_DISK_STATUS;
	transfered = *(int *)MHSIM_DISK_BYTES_TRANSFERRED;
	read_buf_ptr = (char *) MHSIM_DISK_DATA;
	if (verbose_disk)
		DPRINTF("g1read: status 0x%x bytes transfered 0x%x\n",
			rval, transfered);
	if (rval || transfered != size) {
		return -1;
	}

	/* copy the data into where the sg list wants them. */
	tsgp = sgp;
	while (!(tsgp->data_len & SG_LAST_ELEMENT)) {
		if (verbose_disk)
			DPRINTF("g1read: from 0x%x to 0x%x size 0x%x\n",
				read_buf_ptr,
				((int) tsgp->data_ptr & (~PCI_NATIVE_VIEW)),
				tsgp->data_len);

		bcopy(read_buf_ptr,
		      (char *) ((int) tsgp->data_ptr & (~PCI_NATIVE_VIEW)),
		      tsgp->data_len);
		read_buf_ptr += tsgp->data_len;
		tsgp++;
	}

	if (verbose_disk)
		DPRINTF("g1read: from 0x%x to 0x%x size 0x%x\n",
			read_buf_ptr,
			((int) tsgp->data_ptr & (~PCI_NATIVE_VIEW)),
			(tsgp->data_len & (~SG_LAST_ELEMENT)));
	bcopy(read_buf_ptr,
	      (char *) ((int) tsgp->data_ptr & (~PCI_NATIVE_VIEW)),
	      (tsgp->data_len & (~SG_LAST_ELEMENT)));

	return 0;
}

#define MIN(a,b)	(((a) < (b)) ? (a) : (b))

static char inquiry_data[] = {0, 0, 2, 2, 0x8f, 0, 0, 0x12,
			     'S', 'A', 'B', 'L', 'E', 0, 0, 0,
			     'D', 'I', 'S', 'K', 0, 0, 0, 0};
/*
 * Swizzled version of inquiry data.  The swizzling is done dynamically.
 */
static char inquiry_data_sw[] = {0, 0, 2, 2, 0x8f, 0, 0, 0x12,
			         'S', 'A', 'B', 'L', 'E', 0, 0, 0,
			         'D', 'I', 'S', 'K', 0, 0, 0, 0};
static int inquiry_data_len = (sizeof(inquiry_data)/sizeof(char));


static int
do_inquiry(unsigned char *cdb, SG_ELEMENT *sgp)
{
	int native, copy_len, data_left;
	char *data_ptr;

	if (verbose_disk)
		DPRINTF("do_inquiry: filling sg list at 0x%x len 0x%x\n",
			sgp->data_ptr, sgp->data_len);

	if ((int) sgp->data_ptr & PCI_NATIVE_VIEW) {
		if (verbose_disk)
			DPRINTF("native view, no hardware swapping requested\n");
		native = 1;
	} else {
		if (verbose_disk)
			DPRINTF("hardware swapping requested\n");
		native = 0;
	}

	if (inquiry_data_sw[0] == 0)
		osm_swizzle(inquiry_data_sw, inquiry_data_len);

	if (native)
		data_ptr = inquiry_data_sw;
	else
		data_ptr = inquiry_data;

	/*
	 * Copy the inquiry data to the sg list.
	 */
	data_left = inquiry_data_len;
	while ((!(sgp->data_len & SG_LAST_ELEMENT)) &&
	       (data_left > 0)) {
		copy_len = MIN(data_left, sgp->data_len);
		bcopy(data_ptr,
		      (char *) ((int) sgp->data_ptr & (~PCI_NATIVE_VIEW)),
		      copy_len);
		sgp++;
		data_left -= copy_len;
		data_ptr += copy_len;
	}
	copy_len = MIN(data_left, (sgp->data_len & (~SG_LAST_ELEMENT)));
	if (copy_len > 0)
		bcopy(data_ptr,
		      (char *) ((int) sgp->data_ptr & (~PCI_NATIVE_VIEW)),
		      copy_len);
	
	return 0;
}

static char modesense_data[] = {0x87, 0, 0x10, 8, 0, 0, 0, 0, 0, 0, 2, 0,
	    0x81, 0xa, 0xc0, 0x28, 0x30, 0, 0, 0, 0x18, 0, 0xff, 0xff,
	    0x82, 0xe, 0x80, 0x80, 0, 0xa, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0x83, 0x16, 0, 5, 0, 5, 0, 0, 0, 5, 0, 68, 2, 0, 0, 1, 0, 0xa, 0, 0x15, 0x40, 0, 0,
	    0x84, 0x16, 0, 0xf, 0x98, 0x5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0x87, 0xa, 0, 0x28, 0x30, 0, 0, 0, 0, 0, 0xff, 0xff,
	    0x88, 0x12, 0x90, 0xff, 0xff, 0, 0, 0xff, 0xff, 0xff, 0xff, 0, 3, 0, 0, 0, 0, 0, 0,
	    0x8a, 0xa, 0x2, 0x10, 0, 0, 0, 0, 0, 0, 0, 0,
	    0x80, 0x2, 0, 0,
	    0, 0};			/* last 2 are for padding during swizzling */
/*
 * swizzled version of mode sense data.  The swizzling is done dynamically.
 */
static char modesense_data_sw[] = {0x87, 0, 0x10, 8, 0, 0, 0, 0, 0, 0, 2, 0,
	    0x81, 0xa, 0xc0, 0x28, 0x30, 0, 0, 0, 0x18, 0, 0xff, 0xff,
	    0x82, 0xe, 0x80, 0x80, 0, 0xa, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0x83, 0x16, 0, 5, 0, 5, 0, 0, 0, 5, 0, 68, 2, 0, 0, 1, 0, 0xa, 0, 0x15, 0x40, 0, 0,
	    0x84, 0x16, 0, 0xf, 0x98, 0x5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0x87, 0xa, 0, 0x28, 0x30, 0, 0, 0, 0, 0, 0xff, 0xff,
	    0x88, 0x12, 0x90, 0xff, 0xff, 0, 0, 0xff, 0xff, 0xff, 0xff, 0, 3, 0, 0, 0, 0, 0, 0,
	    0x8a, 0xa, 0x2, 0x10, 0, 0, 0, 0, 0, 0, 0, 0,
	    0x80, 0x2, 0, 0,
	    0, 0};			/* last 2 are for padding during swizzling */
static int modesense_len = (sizeof(modesense_data)/ sizeof(char));

static int
do_modesense(unsigned char *cdb, SG_ELEMENT *sgp)
{
	int native, copy_len, data_left;
	char *msen_data_ptr;

	if (verbose_disk) {
		DPRINTF("do_modesense: %x %x %x %x %x %x\n", cdb[0], cdb[1], cdb[2],
			cdb[3], cdb[4], cdb[5]);
		DPRINTF("do_modesense: filling sg list at 0x%x len 0x%x (data len 0x%x)\n",
			sgp->data_ptr, sgp->data_len, modesense_len);
	}

	if ((int) sgp->data_ptr & PCI_NATIVE_VIEW) {
		if (verbose_disk)
			DPRINTF("native view, no hardware swapping requested\n");
		native = 1;
	} else {
		if (verbose_disk)
			DPRINTF("hardware swapping requested\n");
		native = 0;
	}

	if (modesense_data_sw[0] == 0x87)
		osm_swizzle(modesense_data_sw, modesense_len);

	if (native)
		msen_data_ptr = modesense_data_sw;
	else
		msen_data_ptr = modesense_data;

	/*
	 * Copy the mode sense data to the sg list.
	 */
	data_left = modesense_len;
	while ((!(sgp->data_len & SG_LAST_ELEMENT)) &&
	       (data_left > 0)) {
		copy_len = MIN(data_left, sgp->data_len);
		bcopy(msen_data_ptr,
		      (char *) ((int) sgp->data_ptr & (~PCI_NATIVE_VIEW)),
		      copy_len);
		sgp++;
		data_left -= copy_len;
		msen_data_ptr += copy_len;
	}
	copy_len = MIN(data_left, (sgp->data_len & (~SG_LAST_ELEMENT)));
	if (copy_len > 0)
		bcopy(msen_data_ptr,
		      (char *) ((int) sgp->data_ptr & (~PCI_NATIVE_VIEW)),
		      copy_len);

	return 0;
}

static int
do_testunitready(unsigned char *cdb, SG_ELEMENT *sgp)
{
	int rval;

	if (verbose_disk)
		DPRINTF("testunitready\n");

/*	rval = testunitready00(dsp); */
	rval = 0;

	return rval;
}

static int
do_startstop(unsigned char *cdb, SG_ELEMENT *sgp)
{
	int rval;

	if (cdb[4] & 1)
		DPRINTF("Spinning up disk %d\n", disk_targ);
	else
		DPRINTF("Spinning down disk %d\n", disk_targ);

	return 0;
}


static int
init_disk(int targ)
{

	if (disk_targ == -1)
		disk_targ = targ;

	return 0;
}

#endif /* SIMHIM */


