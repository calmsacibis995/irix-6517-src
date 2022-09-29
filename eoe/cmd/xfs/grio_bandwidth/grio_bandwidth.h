#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio_bandwidth/RCS/grio_bandwidth.h,v 1.6 1997/04/29 22:09:05 kanoj Exp $"

/* 
 * grio_bandwidth.h
 */ 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <bstring.h>
#include <time.h>
#include <grio.h>
#include <dslib.h>
#include <invent.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysmp.h>
#include <limits.h>
#include <ulocks.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/iobuf.h>
#include <sys/stat.h>
#include <sys/sema.h>
#include <sys/scsi.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <sys/dksc.h>
#include <sys/times.h>
#include <sys/invent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/schedctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/iograph.h>

/*
 * Global defines
 */
#define DEFAULT_SAMPLETIME	10*60		/* 10 minutes */
#define DISK_ID_SIZE		28
#define	READ_FLAG		0x01
#define	WRITE_FLAG		0x10
#define LINE_LENGTH		512
#define GRIODISKS		"/etc/grio_disks"

#define	TICK_TIME_SIZE		50
#define MAX_IOS			20000

#define MAX_RAIDS_ON_SYSTEM     32
#define LINE_LENGTH             512
#define FILE_NAME_LEN           64
#define NUM_DEVS_PER_CTLR       16
#define MAX_NUM_DEVS            2*NUM_DEVS_PER_CTLR
#define MAX_NUM_PROCS           20*MAX_NUM_DEVS
#define DISK_ID_SIZE            28
#define MAX_NUM_SCSI_UNITS	200
#define IOCONFIG_CTLR_NUM_FILE  "/etc/ioconfig.conf"	/* from ioconfig */

/*
 * define local data structures
 */
typedef struct system_raid_info {
	int	sig1;
	int	sig2;
	int	spa_ctlr;
	int	spb_ctlr;
	int	spa_unit;
	int	spb_unit;
} system_raid_info_t;

typedef struct disk_bw_arg_list {
	char	devicename[DEV_NMLEN];
	int	iosize;
	int	rw;
	int	verbose;
	int	sampletime;
	int	silent;
	int	index;
	int	addparams;
} disk_bw_arg_list_t;

typedef struct device_name {
	char	devicename[DEV_NMLEN];
	int	scsi_ctlr;
	int	scsi_unit;
	int	scsi_lun;
} device_name_t;

extern char 	*dev_to_rawpath(dev_t, char *, int *, struct stat *);
extern int 	measure_raid_bw(int, int, int, int, int, int);
extern int 	measure_controller_bw(char *, int, int, int, int, int);
extern int 	system_failed(int, int);
extern int 	measure_multiple_disks_io(int, device_name_t *, int, int, 
					int , int, int, int, int, int);
extern int 	remove_nodename_params_from_config_file(char *);
extern int 	add_nodename_params_to_config_file(char *, int, int);
extern int user_diskname_to_nodename(char *user_diskname, char *nodename);
extern void nodename_to_volname(char *nodename, char *volname);
extern int	get_maxioops_for_raid_controller(int, int);

extern int *child_status_average;
extern int *child_status_worst;

