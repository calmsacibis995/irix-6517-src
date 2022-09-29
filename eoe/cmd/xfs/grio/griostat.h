#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/griostat.h,v 1.9 1997/03/13 06:02:45 kanoj Exp $"


/*
 * griostat.h
 *
 * 	Header file for grio:
 *
 *	Guaranteed rate I/O bandwidth and reservation status utility.
 *
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <grio.h>
#include <errno.h>
#include <pwd.h>
#include <stropts.h>
#include <mntent.h>
#include <bstring.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/dir.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sysmp.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dkio.h>
#include <sys/fs/xfs_inum.h>
#include <sys/uuid.h>
#include <sys/syssgi.h>
#include <sys/scsi.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#include "../ggd/include/ggd.h"

#define STR_LEN 	100

/* 
 * Maximum length of a device node (from sys/grio.h).
 */
#define DEV_NAME_LEN	DEV_NMLEN

/*
 * Maximum number of devices supported by a single call to grioview.
 */
#define MAX_NUM_DEVS	512

/*
 * Maximum length of the hardware path from disk to memory (from ggd.h)
 */
#define MAX_DEV_DEPTH	MAXCOMPS

typedef struct device_list {
	dev_t	dev;
	char	name[DEV_NAME_LEN];
} device_list_t;

extern ggd_info_summary_t 	*ggd_info;

extern int convert_devnum_to_nodename(dev_t, char *);
extern int add_to_list(char *, device_list_t [], int *);
extern int show_bandwidth(char *);
extern int show_bandwidth_rot(device_list_t [], int);
extern int item_in_list(char *, device_list_t [], int);
extern int issue_vdisk_ioctl( dev_t, int, char *);
extern int grio_send_commands_to_daemon(int, grio_cmd_t * );
extern int dev_to_alias(dev_t, char *, int *);
extern int show_bandwidth_for_devs(device_list_t [], int);
extern int show_path_for_devs(device_list_t [], int );
extern int show_full(device_list_t [], int );
extern int show_rotation(device_list_t [], int );
extern int add_to_list(char *, device_list_t [], int *);
extern int convert_and_show_bandwidth(device_list_t [], int);
extern int show_file_reservations(device_list_t [], int);
extern int get_ggd_info( void );
extern int get_all_streams( void );
extern int show_all_bandwidth( void );
extern int show_command_stats( void );
extern int delete_streams( stream_id_t [], int);
extern int add_id_to_list( char *, stream_id_t [], int * );
extern int show_ggd_info_nodes( void );
extern int find_proc_name( pid_t, char *, char *);
extern int send_file_resvs_command(dev_t, gr_ino_t, grio_stats_t *);
extern int get_path(char *, device_list_t [], int *);
extern int user_diskname_to_nodename(char *, char *);
extern char *dev_to_rawpath(dev_t, char *, int *, struct stat *);
extern int dev_to_devname(dev_t, char *, int *);
