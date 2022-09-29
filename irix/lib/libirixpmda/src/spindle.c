/*
 * --- Disk spindle statistics --
 * This is mostly ripped out of sardc.
 * Mark Goodwin, markgw@sgi.com Mon Nov 29 13:27:21 EST 1993
 *
 * Upgraded to use libdisk for 6.5.
 * Tim Shimmin, tes@sgi.com Thu Jun 17 14:47:03 EST 1999
 */

#ident "$Id: spindle.c,v 1.45 1999/08/26 07:31:48 tes Exp $"

#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/var.h>
#include <sys/sema.h>
#include <sys/iobuf.h>
#include <sys/stat.h>
#include <sys/elog.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/file.h>
#include <sys/flock.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <dslib.h>
#include <diskinfo.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#ifdef PCP_SPINDLE_TESTING
#undef PCP_DEBUG /* so I can compile separately */
#endif
#include "./spindle.h"
#include "./cluster.h"

#ifdef PCP_SPINDLE_TESTING
#define NDEVS 1 /* for testing */
#else
/* Taken from sa.h */
/* Allow for 16 SCSI and 8 IPI buses w/4 drives/bus */
#define NDEVS 96        
#endif

static int ndisk = 0; /* number of s_diosetup entries */
static struct disk_iostat_info *s_diosetup = NULL;

int _pmSpindleCount = 0; /* number of s_stats entries */
static spindle *s_stats = (spindle *)NULL;

static int initialized=0;
static int controller_initialized=0;

static int _pmControllerCount = 0;
static controller *c_stats = NULL;
static int *controller_disk_include = NULL;


spindle *
spindle_stats(int *inclusion_map)
{
    spindle *s = s_stats;
    int	i;
    struct iotime iotim;


    if (!initialized) {
	__pmNotifyErr(LOG_WARNING, "spindle_stats: spindle_stats_init() has not been called");
	return NULL;
    }

    for (i=0; i < _pmSpindleCount; i++) {

	
	if (inclusion_map != NULL && inclusion_map[i] == 0) {
	    /* do not collect stats for this drive */
#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_DISK)
	    	__pmNotifyErr(LOG_WARNING, 
			     "spindle_stats: inclusion_map is null for %d\n",
	    		     i);
#endif
	    continue;
	}

	
	if (dkiotime_neoget(s[i].s_hwgname, &iotim) == -1) {
#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_DISK)
	    	__pmNotifyErr(LOG_ERR, 
			     "spindle_stats: dkiotime_neoget failed for %s: %s\n",
	    		     s[i].s_hwgname, strerror(oserror()));
#endif
	    continue;
	}

	s[i].s_ops = iotim.io_cnt;
	s[i].s_wops = iotim.io_wops;
	s[i].s_bops = iotim.io_bcnt;
	s[i].s_wbops = iotim.io_wbcnt;
	s[i].s_act = iotim.io_act;
	s[i].s_resp = iotim.io_resp;
	s[i].s_qlen = iotim.io_qc;
	s[i].s_sum_qlen = iotim.ios.io_misc;
    }

    return s;
}

/*
 * Allocate another element.
 */
struct disk_iostat_info *
sgi_dio_alloc(void)
{
    static struct disk_iostat_info *block;
    static struct disk_iostat_info *curr;
    static int i = NDEVS; /* ensure allocation happens initially */
    struct disk_iostat_info *ds;

    /*
     * Check to see if we're past the end of allocated elements.
     */
    if (i == NDEVS) {
	/*
	 * Allocate another block of iostats
	 */
	block = (struct disk_iostat_info *)
			calloc(1, NDEVS * sizeof(struct disk_iostat_info));
	if (!block) {
	    __pmNotifyErr(LOG_ERR, "sgi_dio_alloc: calloc failed for %lu disks: %s", 
			ndisk, pmErrStr(-oserror()));
	    return NULL;
	}
	i = 0;
    }

    ds = &block[i++];
    if (s_diosetup)
	curr->next = ds;
    else
	s_diosetup = ds;
    curr = ds;

    ndisk++;

    return ds;
}

/*
 * Free the table for resets
 *
 * Free the strings: diskname, devicename for each entry
 * Free the blocks
 * 
 */

void
free_spindle_table(void)
{
    int i = 0;
    struct disk_iostat_info *ds, *block, *next;

    block = s_diosetup;

    for (ds = s_diosetup; ds != NULL; ds = next) {
	/* need to free strings */
	if (ds->diskname)
	    free(ds->diskname);
	if (ds->devicename)
	    free(ds->devicename);

	next = ds->next;

	i++;
	if (i == NDEVS) {
	    /* need to free block of records */
	    free(block);
	    block = next; 
	    i = 0;
	}
    }
    _pmSpindleCount = 0; ndisk = 0;
    s_diosetup = NULL;
    if (s_stats) {
	free(s_stats);
	s_stats = NULL;
    }
}

void
create_spindle_table(void)
{
    unsigned int ctlr;
    spindle *disk;
    struct stat stat_buf;
    char *str, *ptr;
    struct disk_iostat_info *ds;

    _pmSpindleCount = 0; ndisk = 0;

    /* use the libdisk routine to construct the iostat table */
    sgi_neo_disk_setup(sgi_dio_alloc);
    
    s_stats = (spindle *)calloc(1, ndisk * sizeof(spindle));
    if (s_stats == NULL) {
	__pmNotifyErr(LOG_ERR, "create_spindle_table: calloc failed: %s", 
                      pmErrStr(-oserror()));
	free_spindle_table(); 
	return;
    }

    /* fill in the data */
    for (ds = s_diosetup, disk = s_stats; ds != NULL; ds = ds->next, disk++) {

	disk->s_hwgname = ds->devicename;

	/* external instance name */
	disk->s_dname = ds->diskname;

	/* internal instance id */
        if (stat(ds->devicename, &stat_buf) == -1) {
	    __pmNotifyErr(LOG_ERR, "create_spindle_table: stat(%s) failed: %s", 
			  ds->devicename, pmErrStr(-oserror()));
	    continue;
	}
	/* use major/minor numbers for internal id */
	disk->s_id = (unsigned int) stat_buf.st_rdev;

	/* WARNING: this code segment is hwgname naming convention specific
	 *  
	 * need to process the hwg name to determine stuff:
	 * - fibre channel or direct scsi disk
	 * - controller number
	 */
	if (strncmp(ds->diskname, "dks", 3) == 0) {
	    /* direct disk */
	    str = ds->diskname + 3; /* skip over "dks" */	
	    ctlr = strtol(str, &ptr, 10);
	    if (ptr == str) {
		__pmNotifyErr(LOG_ERR, "create_spindle_table: extract ctlr failed: %s", 
			  ds->diskname);
		continue;
	    }
	    disk->s_ctlr = ctlr;
	    disk->s_type = SPINDLE_DIRECT_DISK;
	}
	else if ((str = strstr(ds->devicename, "vol/c")) != NULL) {
	    /* fibre channel disk */
	    str += 5; /* skip over "vol/c" */
	    ctlr = strtol(str, &ptr, 10);
	    if (ptr == str) {
		__pmNotifyErr(LOG_ERR, "create_spindle_table: extract ctlr failed: %s", 
			  ds->diskname);
		continue;
	    }
	    disk->s_ctlr = ctlr;
	    disk->s_type = SPINDLE_FC_DISK;
	} 
	else {
	    /* don't know it, so ignore it */
	    __pmNotifyErr(LOG_WARNING, "create_spindle_table: unrecognised device name format: %s",
                          ds->devicename);
	    continue;
	}

	_pmSpindleCount++;
    }/*for*/

}

int
spindle_stats_init(int reset)
{
    if (initialized) {
	if (reset) {
	    /* rescan */
	    initialized = 0;
	    free_spindle_table();
	}
	else {
	    __pmNotifyErr(LOG_WARNING, "spindle_stats_init: called more than once!");
	    return _pmSpindleCount; /* not a fatal error */
	}
    }

    initialized++;
    create_spindle_table();

#ifdef PCP_DEBUG
    /* print out spindle table */
    if (pmIrixDebug & DBG_IRIX_DISK) {
	int i;

	for (i=0; i < _pmSpindleCount; i++) {
	   spindle *s = s_stats + i;
	    __pmNotifyErr(LOG_DEBUG, 
		     "spindle_stats: %d: <id = %u> <type = %d> <ctlr = %u> "
		     "<dname = %s> <hwgname = %s>\n",
		     i, s->s_id, s->s_type, s->s_ctlr, s->s_dname, s->s_hwgname); 
	}
    }
#endif

    /* success */
    return _pmSpindleCount;
}



static int
drive_controller(spindle *drive, int drivenumber)
{
    int sts;
    int i;
    controller *c;
    controller *newc;
    static int max_controllers = 0;

    c = c_stats;
    for (i=0; i < _pmControllerCount; i++) {
	if (c[i].c_ctlr == drive->s_ctlr) {
	    c[i].c_ndrives++;
	    c[i].c_drives = (int *)realloc(c[i].c_drives, c[i].c_ndrives * sizeof(int));
	    if (c[i].c_drives == NULL)
		goto MALLOC_FAIL;
	    c[i].c_drives[c[i].c_ndrives-1] = drivenumber;
	    /* successfully DONE */
	    return 0;
	}
    }

    /* we haven't seen this controller before */
    _pmControllerCount++;
    while (_pmControllerCount >= max_controllers) {
	max_controllers += 4;
	c_stats = (controller *)realloc(c_stats, max_controllers * sizeof(controller));
	if (c_stats == NULL)
	    goto MALLOC_FAIL;
    }
    newc = &c_stats[_pmControllerCount-1];

    newc->c_id = drive->s_ctlr;
    newc->c_ctlr = drive->s_ctlr;

    if (drive->s_type == SPINDLE_FC_DISK) {
	sprintf(newc->c_dname, "fc%d", drive->s_ctlr);
    }
    else { /* (drive->s_type == SPINDLE_DIRECT_DISK) */
	sprintf(newc->c_dname, "dks%d", drive->s_ctlr);
    }

    newc->c_ndrives = 1;
    if ((newc->c_drives = (int *)malloc(sizeof(int))) == NULL)
	goto MALLOC_FAIL;
    newc->c_drives[0] = drivenumber;

    /* success */
    return 0;

MALLOC_FAIL:
    sts = oserror();
    __pmNotifyErr(LOG_ERR, "controller_stats_init: malloc failed: %s", pmErrStr(-sts));
    return -sts;
}

int
controller_stats_init(int reset)
{
    int i, e;

    if (!initialized) {
	__pmNotifyErr(LOG_ERR, "controller_stats_init: spindle_stats_init() has not been called");
	return 0;
    }

    if (controller_initialized) {
	if (reset) {
	    /* rescan */
	    if (controller_disk_include != NULL)
		free(controller_disk_include);
	    controller_disk_include = NULL;
	    for(i=0; i < _pmControllerCount; i++) {
		free(c_stats[i].c_drives);
	    }
	    _pmControllerCount = 0;
	}
	else {
	    __pmNotifyErr(LOG_WARNING, "controller_stats_init: disk controller stats already initialized");
	    return _pmControllerCount;
	}
    }

    for (i=0; i < _pmSpindleCount; i++) {
	if ((e = drive_controller(&s_stats[i], i)) < 0)
	    goto FAIL;
    }

    controller_disk_include = (int *)malloc(_pmSpindleCount * sizeof(int));
    if (controller_disk_include == NULL) {
	e = -oserror();
	goto FAIL;
    }


#ifdef PCP_DEBUG
    /* print out controller table */
    if (pmIrixDebug & DBG_IRIX_DISK) {
	int i,j;
	char drives[1024], drive_num[20];

	*drives = '\0';

	for (i=0; i < _pmControllerCount; i++) {
	    controller *c = c_stats + i;
	    *drives = '\0';
	    for (j=0; j < c->c_ndrives; j++) {
		sprintf(drive_num, "%d ", c->c_drives[j]);
		strcat(drives, drive_num);
	    }
	    __pmNotifyErr(LOG_DEBUG, 
		     "controller_stats: %d: <id = %u> <ctlr = %u> "
		     "<dname = %s> <n_drives = %d> <drives: %s>\n",
		     i, c->c_id, c->c_ctlr, c->c_dname, c->c_ndrives, drives); 
	}
    }
#endif

    /* success */
    controller_initialized = 1;
    return _pmControllerCount;

FAIL:
    _pmControllerCount = 0;
    __pmNotifyErr(LOG_WARNING, "controller_stats_init: disk controller stats are not available: %s", pmErrStr(e));
    return e;
}

controller *
controller_stats(int *inclusion_map)
{
    spindle	*sps;
    controller	*cntrls;
    int		i, j;
    int		d;

    if (!initialized) {
	__pmNotifyErr(LOG_WARNING, "controller_stats: spindle_stats_init() has not been called");
	return NULL;
    }

    if (!controller_initialized) {
	/* Controller stats may not have been initialised because there
	   are no disks */
	if (_pmSpindleCount > 0)
	    __pmNotifyErr(LOG_ERR, 
			  "controller_stats: controller_stats_init() has not been called");
	return NULL;
    }

    /*
     * NOTE: one controller_stats_init has succeeded
     * do not expect controller_stats ever to fail.
     */

    cntrls = c_stats;

    if (inclusion_map != NULL) {
	/* only collect stats for disks on ``interesting'' controllers */
	for (i=0; i < _pmControllerCount; i++) {
	    for (j=0; j < cntrls[i].c_ndrives; j++) {
		controller_disk_include[cntrls[i].c_drives[j]] = inclusion_map[i];
	    }
	}
	/* get associated spindle stats */
	sps = spindle_stats(controller_disk_include);
    }
    else {
	/* get all spindle stats */
	sps = spindle_stats(NULL);
    }

    for (i=0; i < _pmControllerCount; i++) {
	if (inclusion_map != NULL && inclusion_map[i] == 0)
	    continue;
	cntrls[i].c_ops = cntrls[i].c_wops = cntrls[i].c_bops = 
	cntrls[i].c_wbops = cntrls[i].c_act = cntrls[i].c_resp = 
	cntrls[i].c_qlen = cntrls[i].c_sum_qlen = 0;

	for (j=0; j < cntrls[i].c_ndrives; j++) {
	    d = cntrls[i].c_drives[j];
	    cntrls[i].c_ops += sps[d].s_ops;
	    cntrls[i].c_wops += sps[d].s_wops;
	    cntrls[i].c_bops += sps[d].s_bops;
	    cntrls[i].c_wbops += sps[d].s_wbops;
	    cntrls[i].c_act += sps[d].s_act;
	    cntrls[i].c_resp += sps[d].s_resp;
	    cntrls[i].c_qlen += sps[d].s_qlen;
	    cntrls[i].c_sum_qlen += sps[d].s_sum_qlen;
	}
    }

    return cntrls;
}

#ifdef PCP_SPINDLE_TESTING

static void
dump_spindles(spindle *sp)
{
    int i;
    for (i=0; i < _pmSpindleCount; i++) {
	printf("disk = %d, %d, %s\n", sp[i].s_id, sp[i].s_type, sp[i].s_dname);
    }
}

static void
dump_controllers(controller *ctrl)
{
    int i,j;
    for (i=0; i < _pmControllerCount; i++) {
	printf("ctrl = %d, %s\n", ctrl[i].c_id, ctrl[i].c_dname);
	for (j=0; j < ctrl[i].c_ndrives; j++) {
	    printf("  drive[%d] = %d\n", j, ctrl[i].c_drives[j]);
        }
    }
}

/*
 * Iterate with resets so the code can be tested and purified
 */

void
main(int argc, char *argv[])
{
    int sts, reset;
    int i;
    spindle *sp;
    controller *ctrl;

    reset = 0;
    for (i = 0; i < 10; i++) {
	sts = spindle_stats_init(reset);
	sts = controller_stats_init(reset);

	sp = spindle_stats(NULL);
	dump_spindles(sp);

	ctrl = controller_stats(NULL);
	dump_controllers(ctrl);

	reset = 1;
    }
}

#endif
