/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"IP30prom/main_rprom.c:  $Revision: 1.8 $"

/*
 * main_rprom.c -- main line rprom code
 *		   and the rprom IO i/f
 *		   very condensed version of arcsio and fs
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>

/* arcsio stub */
#include <arcs/folder.h>
#include <arcs/cfgtree.h>
#include <arcs/eiob.h>
#include <arcs/errno.h>

#include <saio.h>


#include <libsc.h>
#include <libsk.h>
#include <sys/RACER/sflash.h>

#ifdef VERBOSE
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

/*
 * PROMPT is different for debugging version so
 * it's easy to tell which monitor you're talking to
 * when debugging monitor with monitor
 */

#ifdef ENETBOOT
#define	PROMPT	">>> "
#ifdef SYMMON
int _prom=3;            /* bake _prom flag with true, 3 == dprom.DBG */
#else
int _prom=2;            /* bake _prom flag with true, 2 == dprom */
#endif /* SYMMON */
#else
#define	PROMPT	">> "
int _prom=1;            /* bake _prom flag with true. 1 == prom */
#endif /* ENETBOOT */

char* getversion(void);

#define MAX_RETRYS		10

#define RPROM_DEV_DVH_HD	1
#define RPROM_DEV_DVH_CDROM	2
#define RPROM_DEV_BOOTP		3

LONG fs_dev_open(COMPONENT *p, LONG *fd);
LONG fs_dev_read(ULONG fd, void *buf, ULONG cnt, ULONG *n);
LONG fs_dev_close(ULONG fd);
LONG get_IP30prom(COMPONENT *p);

static int get_work_buf(void);
static int program_flash(__psunsigned_t, int);

static __psunsigned_t ld_buf;
static int ld_sz;

/*
 * for libsc scsaio.c
 */
int Debug, Verbose, _udpcksum;

#define CR 0x0d
#define NL 0x0a

/* externals */

extern COMPONENT *net_comp; 
extern int scan_dev_ok;		/* scan successfully loaded IP30prom.bin */

/*
 * main -- called from csu after prom data area has been cleared
 */
void
main(void)
{
    TIMEINFO tod, *t;
    __psunsigned_t bufp;
    ULONG cnt;
    int retrys;
    /*REFERENCED*/
    int rv;
    void scan_dev_cb(LONG (*func)(COMPONENT *));

    cpu_get_tod(&tod);	/* initializes the dallas part */

    printf("RPROM Main\n");
    rv = flash_init();
    DPRINTF(("flash_init=%d\n", rv));

#if RPROM_DEBUG
    printf("%d/%d/%d %d:%d:%d\n",
	   tod.Month, tod.Day, tod.Year,
	   tod.Hour, tod.Minutes, tod.Seconds);
    
    flash_print_prom_info(SFLASH_RPROM_HDR_ADDR,
			  (u_short *)SFLASH_RPROM_ADDR);
#endif

    get_work_buf();

    /* hack to get around the scsi sensing problem */
    us_delay(10000 * 1000);

    while (1) {
	scan_dev_cb(get_IP30prom);
	if (scan_dev_ok)
	    break;

	/*
	 * if there was a net device, try it last
	 */
	if (net_comp && get_IP30prom(net_comp) == ESUCCESS) {
	    break;
	}
	if ( ++retrys == MAX_RETRYS) {
	    DPRINTF(("Failed to get the file\n"));
	    goto fail_loop;
	}
	/* try again after a pause */
	us_delay(2000 * 1000);
    }
    rv = program_flash(ld_buf, ld_sz);
    if (rv == ESUCCESS) {
	printf("Fprom recovery successful\n");
	flash_pds_log("Fprom recovery successful");
	if (jumper_off()) {
	    printf("Power off system and re-install passwd jumper\n");
	    while (1) {
		ip30_set_cpuled(LED_AMBER);
		us_delay(1000 * 1000);
		ip30_set_cpuled(LED_GREEN);
		us_delay(1000 * 1000);
	    }
	}
    } else {
fail_loop:
	/* failure */
	printf("Fprom recovery failed\n");
	flash_pds_log("Fprom recovery failed");
	while (1) {
	    ip30_set_cpuled(LED_AMBER);
	    us_delay(2 * 1000 * 1000);
	    ip30_set_cpuled(LED_OFF);
	    us_delay(2 * 1000 * 1000);
	}
    }
    cpu_hardreset();
}

static int
program_flash(__psunsigned_t bin_buf, int bin_sz)
{
    int _fl_prom(uint16_t *, int , int , int);
    char *fbuf;
    int rv;

    rv = _fl_prom((uint16_t *)(bin_buf + SFLASH_RPROM_SIZE),
		  0,
		  SFLASH_FPROM_SEG,
		  bin_sz - SFLASH_RPROM_SIZE); 
    fbuf = (char *)(SFLASH_FPROM_HDR_ADDR) + FLASH_HEADER_SIZE;
    if (!rv && flash_prom_ok(fbuf, SFLASH_FPROM_HDR_ADDR)) {
	DPRINTF(("fprom programed OK\n"));
#if RPROM_DEBUG
	flash_print_prom_info(SFLASH_FPROM_HDR_ADDR, fbuf);
#endif
	return(ESUCCESS);
    }
    return(~ESUCCESS);
}

int (*bootp_fs_strat)();
int (*dvh_fs_strat)();

/******/
/* fs */
/******/
/* fs init is still required */

int
fs_register (int (*func)(), char *name, int type)
{
    if (strcmp(name, "tftp")  == 0) {
        bootp_fs_strat = func;
        return(ESUCCESS);
    }
    if (strcmp(name, "dvh") == 0) {
        dvh_fs_strat = func;
        return(ESUCCESS);
    }
    return(EINVAL);
}

/*
 * fs_init - initialize filesys data structures from table in conf.c
 */
void
fs_init(void)
{
    extern int (*_fs_table[])(void);
    int i;

    /* search the filesys initialization array and call each
     * install function found there
     */

    for (i = 0; _fs_table[i]; i++)
	(*_fs_table[i])();
}

int
set_fs_dev(COMPONENT *c, struct eiob *iob)
{
    ULONG key;
    cfgnode_t *p = (cfgnode_t *)c;
    cfgnode_t *adap, *ctrl;

    /*  Fill in Controller, and Unit. */
    iob->iob.Unit = p->comp.Key;
    ctrl = p->parent;
    if (ctrl) {
	adap = ctrl->parent;
	if (adap && (adap->comp.Type == SCSIAdapter)) {
	    iob->iob.Controller = adap->comp.Key;
	    iob->iob.Unit = ctrl->comp.Key;
	} else {
	    iob->iob.Controller = ctrl->comp.Key;
	}
    }

    if (p->comp.Type == NetworkPeripheral) {
	iob->fsstrat = bootp_fs_strat;
	iob->filename = "flash/IP30prom.bin";	/*XXX */

    } else {
	iob->iob.Partition = 8; /*XXX*/

	iob->fsstrat = dvh_fs_strat;
	iob->filename = "/IP30prom";	/*XXX*/
    }

    iob->dev = (COMPONENT *)p;
    iob->devstrat = p->driver;

    return(ESUCCESS);

}

LONG
fs_dev_open(COMPONENT *periph, LONG *fd)
{
    struct eiob *io;
    int rc = EACCES;
    int errflg;

    DPRINTF(("fs_dev_open\n"));
    *fd = -1;		/* make sure fd is invalid on error */

    if ((io = new_eiob()) == NULL)
	return(EMFILE);
    
    io->fsstrat = 0;
    io->iob.Flags = 0;

    if (rc=set_fs_dev(periph, io)) {
	rc = EINVAL;
	goto bad;
    }

    /* make sure opened device has a driver, and device init passed */
    if ((io->devstrat == NULL) || (io->dev->Flags & Failed)) {
	rc = ENODEV;
	goto bad;
    }

    /* only do OpenReadOnly mode */
    if ((io->dev->Flags & Input) == 0) {
	goto bad;
    }
    io->iob.Flags |= (F_READ | F_FS);
    if (periph->Type == DiskPeripheral)
	io->iob.Flags |= F_BLOCK;

    io->iob.FunctionCode = FC_OPEN;
    errflg = (*io->devstrat)(io->dev, &io->iob);
    if (errflg) {
	rc = io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
	goto bad;
    }

    io->fsb.Device = io->dev;
    io->fsb.DeviceStrategy = io->devstrat;
    io->fsb.Filename = (CHAR *)io->filename;
    io->fsb.IO = &io->iob;

    if (io->fsstrat) {
	/* must have a filename if listed a filesystem */
	if (!io->filename) {
	     rc = EINVAL;
	     goto bad1;
	}
	/* make sure the filesystem given is valid */
	io->fsb.FunctionCode = FS_CHECK;
	if ((rc=(*io->fsstrat)(&io->fsb)) != ESUCCESS)
	    goto bad1;

	if (io->fsstrat) {
	    io->fsb.FunctionCode = FS_OPEN;
	    errflg = (*io->fsstrat)(&io->fsb);
	    if (errflg) {
		rc = io->iob.ErrorNumber;
		goto bad1;
	    }
	}

	io->iob.Offset.hi = 0;
	io->iob.Offset.lo = 0;

	*fd = io->fd;
	return(ESUCCESS);

    } else {
	rc = ENXIO;
	goto bad1;
    }

bad1:
    io->iob.FunctionCode = FC_CLOSE;
    (void)(*io->devstrat)(io->dev, &io->iob);
bad:
    free_eiob(io);
    return(rc);
}

#define READ_SIZE	(16 * 1024)
LONG
fs_dev_read(ULONG fd, void *buf, ULONG cnt, ULONG *n)
{
    struct eiob *io = get_eiob(fd);
    char *cptr, *buf_lim;
    ULONG total;

    DPRINTF(("fs_dev_read\n"));
    if (io == NULL || (io->iob.Flags&F_READ) == 0 || !io->fsstrat)
	return(EBADF);

    _scandevs();

    cptr = (char *)buf;
    total = 0;
    buf_lim = cptr + cnt - READ_SIZE;

    do {
	io->iob.Address = cptr;
	io->iob.Count = READ_SIZE;

	io->fsb.FunctionCode = FS_READ;
	if ((*io->fsstrat)(&io->fsb)) {
	    return io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
	}
	total += io->iob.Count;
	cptr += io->iob.Count;
	if (cptr > buf_lim) {
	    return ENOMEM;
	}

    } while (io->iob.Count);

    *n = total;
    return ESUCCESS;
}

LONG
fs_dev_close(ULONG fd)
{
    register struct eiob *io = get_eiob(fd);
    int retval;

    DPRINTF(("fs_dev_close\n"));
    if (io == NULL || io->iob.Flags == 0  || !io->fsstrat)
	return(EBADF);

    retval = 0;

    io->fsb.FunctionCode = FS_CLOSE;
    retval = (*io->fsstrat)(&io->fsb);

    io->iob.FunctionCode = FC_CLOSE;
    retval |= (*io->devstrat)(io->dev, &io->iob);

    if (retval)
	retval = io->iob.ErrorNumber ? io->iob.ErrorNumber : EIO;
    
    free_eiob(io);
    return (retval);
}

static int
get_work_buf(void)
{
    extern void __dcache_wb_inval(void *, int);
    extern MEMORYDESCRIPTOR *mem_getblock(void);
    extern void mem_list(void);
    MEMORYDESCRIPTOR *m;
    __psunsigned_t bufp;

    if (!(m = mem_getblock())) {
	DPRINTF(("No free memory descriptors"));
	mem_list();
	return(ENOMEM);
    }
    DPRINTF(("  memblock base=%#x count=%ld\n",
    		arcs_ptob(m->BasePage), arcs_ptob(m->PageCount)));
    bufp = arcs_ptob(m->BasePage);
    bufp = PHYS_TO_K0(bufp);
    __dcache_wb_inval((void *)bufp, SFLASH_MAX_SIZE);
    bufp = K0_TO_K1(bufp);
    DPRINTF(("work buf @ 0x%x\n", bufp));

    ld_buf = bufp;
    return(ESUCCESS);

}

LONG
get_IP30prom(COMPONENT *c)
{
    LONG fd;
    LONG err;
    ULONG cnt;
    int _fl_check_bin_image(char *, int); 

    err = fs_dev_open(c, &fd);
    if (err) {
	DPRINTF(("Open error %d\n", err));
	goto bail;
    }
    err = fs_dev_read(fd, (void *)ld_buf, SFLASH_MAX_SIZE, &cnt);
    if (err != ESUCCESS) {
	DPRINTF(("Read error  %d\n", err));
	goto bail;
    }
    err = fs_dev_close(fd);
    if (err != ESUCCESS) {
	DPRINTF(("Close error %d\n", err));
    }
    DPRINTF(("%d bytes read\n", cnt));
    DPRINTF(("sum -r = %d\n", flash_cksum_r((void *)ld_buf, cnt, 0)));

    /* IP30PROM_BIN_FORMAT is 1 */
    if (_fl_check_bin_image((char*)ld_buf, cnt) != 1) {
	DPRINTF(("Bad IP30prom file\n"));
	err = EBADF;
	goto bail;
    }

    ld_sz = cnt;
    return(ESUCCESS);

bail:
    return(err);
}

