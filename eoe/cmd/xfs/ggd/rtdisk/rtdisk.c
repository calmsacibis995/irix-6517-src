#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/rtdisk/RCS/rtdisk.c,v 1.3 1994/11/29 00:25:44 tap Exp $"

/*
 * Guaranteed Rate I/O Utility
 *
 * Currently 3 options are supported:
 *	-p	print out the current scsi modes for the given drive
 *	-n	change the scsi modes to disable retry on the given drive
 *	-r	change the scsi modes to enable retry on the given drive
 *
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <bstring.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/dvh.h>
#include <sys/scsi.h>
#include <sys/dkio.h>
#include <sys/dksc.h>

int scsi_print_err_settings( int, char *);
extern int scsi_setup( int, char *);
extern int scsi_modesense( int, char *, int, u_char);
extern int scsi_set_disk_to_retry( int, char * );
extern int scsi_set_disk_to_no_retry( int, char * );

extern u_char pglengths[];

void
usage(void)
{
	printf("usage: rtdisk [-prn] [-d diskname]\n");
}

/*
 * Main routine for rtdisk program.
 */
main( int argc, char **argv)
{
	extern char *optarg;
	int		c, fd, pflag, nflag, rflag, ret;
	char		*diskname;

	nflag = pflag = rflag = 0;

	/*
	 * Parse args
	 */
	while  (( c = getopt(argc, argv, "d:npr")) != EOF)  {
		switch(c) {
			case 'd':
				diskname = strdup(optarg);
				break;
			case 'n':
				nflag = 1;
				break;
			case 'p':
				pflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
			default:
				usage();
				exit(-1 );
		}
	}

	if ((nflag + pflag + rflag) > 1 ) {
		printf("can only specify one of -n -p -r options \n");
		exit( -1 );
	}

	if ((nflag + pflag + rflag) == 0 ) {
		printf("must specify one of -n -p -r options \n");
		exit( -1 );
	}

	if ( ( fd = open(diskname, O_RDONLY) ) < 0 ) {
		printf("open of %s failed.\n",diskname);
		exit( -1 );
	}

	if ( pflag )  {
		ret = scsi_print_err_settings( fd, diskname );
	} else if ( nflag ) {
		ret = ioctl(fd, DIOCNORETRY, 1);
		if (ret) {
			printf("DIOCNORETRY ioctl failed \n");
		}

		ret = ioctl(fd, DIOCNOSORT, 1);
		if (ret) {
			printf("DIOCNOSORT ioctl failed \n");
		}

		ret = scsi_set_disk_to_no_retry( fd, diskname );
		ret = scsi_set_disk_to_no_retry( fd, diskname );
		if (ret) {
			printf("Could not set scsi select data to no retry\n");
		}
	} else if ( rflag ) {
		ret = ioctl(fd, DIOCNORETRY, 0);
		if (ret) {
			printf("DIOCNORETRY ioctl failed \n");
		}

		ret = ioctl(fd, DIOCNOSORT, 0);
		if (ret) {
			printf("DIOCNOSORT ioctl failed \n");
		}

		ret = scsi_set_disk_to_retry( fd, diskname );
		ret = scsi_set_disk_to_retry( fd, diskname );
		if (ret) {
			printf("Could not set scsi select data to retry\n");
		}
	}
	return( ret );
}

int
scsi_print_err_settings( int fd, char *diskname)
{
        struct mode_sense_data  err_params_sen;
        struct mode_sense_data  err_params_sel;
        struct err_recov        *err_page;
        int                     len, ret;

        ret = scsi_setup( fd, diskname );

        if (ret != 0) {
                return( ret );
        }

        bzero( &err_params_sen, sizeof(err_params_sen) );
        bzero( &err_params_sel, sizeof(err_params_sel) );

        len = (int)pglengths[ERR_RECOV];
        ret = scsi_modesense( fd, (char*)(&err_params_sen),
		len, ERR_RECOV|CURRENT);
        if ( ret != 0 ) {
                printf("Could not get scsi error sense data for %s\n",
                        diskname);
                return ( ret );
        }

        err_page = (struct err_recov *)
                   (err_params_sen.block_descrip + 
		    ((ulong_t)err_params_sen.bd_len) );
        printf("Error page bits: 0x%x \n", err_page->e_err_bits);

        printf("E_DCR = %x,", (err_page->e_err_bits & E_DCR)  >> 0 );
        printf(" E_DTE = %x,", (err_page->e_err_bits & E_DTE)  >> 1 );
        printf(" E_PER = %x,", (err_page->e_err_bits & E_PER)  >> 2 );
        printf(" E_RC = %x,", (err_page->e_err_bits & E_RC)   >> 4 );
        printf(" E_TB = %x,", (err_page->e_err_bits & E_TB)   >> 5 );
        printf(" E_ARRE = %x,", (err_page->e_err_bits & E_ARRE) >> 6 );
        printf(" E_AWRE = %x\n\n", (err_page->e_err_bits & E_AWRE) >> 7 );

	printf("Error correction %s.\n",
		(err_page->e_err_bits & E_DCR) ? "disabled" : "enabled");

	printf("%s data transfer on error.\n",
		(err_page->e_err_bits & E_DTE) ? "Disable" : "Enable");

	printf("%s report recovered errors.\n",
		(err_page->e_err_bits & E_PER) ? "Do" : "Don't");

	printf("%s delay for error recovery.\n",
		(err_page->e_err_bits & E_RC) ? "Don't" : "Do");

	printf("%s transfer bad blocks.\n",
		(err_page->e_err_bits & E_TB) ? "Do" : "Don't");

	printf("%s auto bad block reallocation (read).\n",
		(err_page->e_err_bits & E_ARRE) ? "Do" : "No");

	printf("%s auto bad block reallocation (write).\n",
		(err_page->e_err_bits & E_AWRE) ? "Do" : "No");

	return( 0 );

}
