#include <arcs/fat.h>
#include <sys/param.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <ctype.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/scsi.h>
#include <sys/dksc.h>
#include <saio.h>
#include <saioctl.h>
#include "scsi.h"

#define CDC_BUF_UNITS 256	/* divisor for buffer full/empty ratios
for most newer drives */

/*	these pages are getting gross enough that the next time
	around, I probably ought to just make an array of 0x40
	of them.... */
struct mode_sense_data current_err_params;
struct mode_sense_data current_acc_params;
struct mode_sense_data current_geom_params;
struct mode_sense_data current_cert_params;
struct mode_sense_data current_blk_params;
struct mode_sense_data current_cach_params;
struct mode_sense_data current_cach2_params;
struct mode_sense_data current_conn_params;
struct err_recov *current_err_page;
struct dev_format *current_acc_page;
struct dev_geometry *current_geom_page;
struct cert_pattern *current_cert_page;
struct block_descrip *current_blk_page;
struct cachectrl *current_cach_page;
struct connparms *current_conn_page;
struct cachescsi2 *current_cach2_page;


extern partnum;

/* len is length of additional data plus 4 byte sense header 
		plus 8 byte block descriptor + 2 byte page header */
#define SENSE_LEN_ADD (8+4+2)

#undef DEBUGMODE

u_char pglengths[ALL+1];	/* lengths of each page for mode select,
	sense.  len of 0 means page not supported */

extern int expert;

scsi_modesense(struct mode_sense_data *addr, u_char pgcode)
{
	struct dk_ioctl_data modesense_data;

	modesense_data.i_page = pgcode;	/* before AND ! */
	pgcode &= ALL;
	if(!pglengths[pgcode])
		return 1;
	modesense_data.i_addr = (caddr_t)addr;

	/* len is length of additional data plus 4 byte sense header 
		plus 8 byte block descriptor + 2 byte page header */
	modesense_data.i_len = pglengths[pgcode] + SENSE_LEN_ADD;
	if(modesense_data.i_len > 0xff)	/* only one byte! (don't want modulo) */
		modesense_data.i_len = 0xff;

	if (gioctl(DIOCSENSE, (char *)&modesense_data) < 0) {
		return 1;
	}
	if(!addr->dk_pages.common.pg_code && (pgcode&ALL))
		addr->dk_pages.common.pg_code = pgcode; /* for Toshiba drives,
			which bogusly don't fill in the pgcode.  This has been
				working because some firmware revs don't look at it... */
	return 0;
}

void
scsi_readcapacity(int *addr)
{
	if(gioctl(DIOCREADCAPACITY, (char *)addr))
		*addr = 0;
}
