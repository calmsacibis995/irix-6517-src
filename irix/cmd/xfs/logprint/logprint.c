#ident	"$Revision: 1.27 $" 

#include <sys/var.h>
#define _KERNEL 1
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#undef _KERNEL
#include <sys/uuid.h>
#include <sys/vnode.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <strings.h>
#include <stdlib.h>
#include <bstring.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>

#include "sim.h"

extern void
xfs_log_print_trans(dev_t	log_dev,
		    daddr_t	blk_offset,
		    int		num_bblks,
		    int		print_block_start,
		    uint	flags);
void
usage(void)
{
	printf("USAGE: xfs_logprint [options...] <volume-name|device-name|logfile-name>\n");
	printf("-e		exit when error found in log\n");
	printf("-f 		the log is a file\n");
/*	-l only used for debugging */
	printf("-n		don't try and interpret log data\n");
	printf("-o		print buffer data in hex\n");
	printf("-s <start blk>	block # to start printing\n");
	printf("-t		print out transactional view\n");
	printf("	-b	in transactional view, extract buffer info\n");
	printf("	-i	in transactional view, extract inode info\n");
	printf("	-q	in transactional view, extract quota info\n");
	printf("-D		print only data; no decoding\n");
	exit(1);
}


int recover_only = 0;
int print_data = 0;
int print_only_data = 0;
int print_inode = 0;
int print_quota = 0;
int print_buffer = 0;
int print_transactions = 0;

void
main(int argc, char **argv)
{
	sim_init_t	si;
	char		*dfile = NULL;
	uint		flags = 0;
	int		c;
	int		is_file = 0;
	char		*vfile = NULL;
	int		print_start = -1;

	xlog_debug = 1;
	bzero(&si, sizeof(si));
	while ((c = getopt(argc, argv, "befiqnors:tD")) != EOF) {
		switch (c) {
			case 'D': {
				print_only_data++;
				print_data++;
				break;
			}
			case 'b': {
				print_buffer++;
				break;
			}
			case 'd': {
				dfile = optarg;
				if (vfile) {
		fprintf(stderr, "xfs_logprint: -d incompatible with -v\n");
					exit(1);
				}
				break;
			}
			case 'e': {
				flags |= XFS_LOG_PRINT_EXIT;
				break;
			}
			case 'f': {
				is_file = 1;
				break;
			}
			case 'i': {
				print_inode++;
				break;
			}
			case 'q': {
				print_quota++;
				break;
			}
			case 'n': {
				flags |= XFS_LOG_PRINT_NO_DATA;
				break;
			}
			case 'o': {
				print_data++;
				break;
			}
			case 's': {
				print_start = atoi(optarg);
				break;
			}
			case 't': {
				print_transactions++;
				break;
			}
			case 'v': {
				vfile = optarg;
				if (dfile) {
		fprintf(stderr, "xfs_logprint: -v incompatible with -d\n");
					exit(1);
				}
				break;
			}
			case '?': {
				usage();
			}
	        }
	}

	if (argc - optind != 1)
		usage();

	si.volname = si.dname = NULL;

	if (is_file) {
		si.disfile = 1;
		si.logname = argv[optind];
		si.lisfile = 1;
	} else {
		si.volname = argv[optind];
	}

	si.notvolok = 1;
	si.isreadonly = XFS_SIM_ISREADONLY;
	si.notvolmsg = "You should never see this message.\n";
	if (!xfs_sim_init(&si))
		exit(1);
	xfs_sim_logstat(&si);
	if (print_transactions)
		xfs_log_print_trans(si.logdev, si.logBBstart,
				    si.logBBsize, print_start, flags);
	else
		xfs_log_print(NULL, si.logdev, si.logBBstart, si.logBBsize,
			      print_start, flags);
	exit(0);
} /* main */
