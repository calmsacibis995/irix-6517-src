#include <sys/types.h>
#include <sys/lock.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/scsi.h>
#include <dslib.h>
#include <sys/param.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/mount.h>

struct scsi_ha_op	scsiioctl;


#define USAGE "scsihotswap\n\t-u (unplug)    | -p (plug)\n\t-d scsi/device | dsk/device\n\t-b bus\n\t-f fsname | -n (no un/mount)\n\t-s fstype (only for plug)\n\t[-x (disk is part of an xlv volume)]\n\t[-t timeout val (default 300 secs)]"


void plug_unit(int bus, char device[128], char fsname[128], 
	       char fstype[16], int unmount, int timeout, int xlv_vol);

/**********************/
/* from scsicontrol.c */

int
startunit1b(struct dsreq *dsp, int startstop, int vu)
{
  fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), 0x1b, 0, 0, 0, (uchar_t)startstop, B1(vu<<6));
  filldsreq(dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 90; /* 90 seconds */
  return(doscsireq(getfd(dsp), dsp));
}

/**********************/

main(int argc, char **argv)
{
	int err_val = 0;
	int bus = -1;
	char device[128];
	char fsname[128];
	char xlv_vol_list[128];
	char fstype[16];
	int xlv_vol = -1;
	struct dsreq *dsp;
	int c;
	int plug = -1;
	int unplug = -1;
	char sys_str[64];
	char err_str[64];
	volatile int state = 0;
	int fd;
	int timeout = -1;
	int in_progress_timeout = 10; /* default */
	int quiesce_time_usr = 20; /* this is what the user wanted */
	int unmount = 1;
	char q_device[128];
	fd_set fdset;
	struct timeval timeout_val;
	int rtn_val;

	strcpy(device, "");
	strcpy(fsname, "");
	strcpy(xlv_vol_list, "");
	while ((c = getopt(argc, argv, "b:d:f:nps:t:ux")) != -1) {
		switch (c) {
		case 'b':
			bus = atoi(optarg);
			break;
		case 'd':
			strcpy(device, optarg);
			break;
		case 'f':
			strcpy(fsname, optarg);
			break;
		case 'n':
			unmount = 0;
			break;
		case 'p':
			plug = 1;
			if (unplug != -1) {
				printf("%s\n", USAGE);
				exit(-1);
			}
			break;
		case 's':
			strcpy(fstype, optarg);
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		case 'u':
			unplug = 1;
			if (plug != -1) {
				printf("%s\n", USAGE);
				exit(-1);
			}
			break;
		case 'x':
			xlv_vol = 1;
			break;
		default:
			printf("%s\n", USAGE);
			exit(-1);
		}
	}

	if ((bus == -1) ||
	    (strlen(device) < 1) ||
	    ((plug == -1) && (unplug == -1)) ||
	    ((unmount) && (strlen(fsname) < 1))) {
		printf("%s\n", USAGE);
		exit(-1);
	}

	if (plug == 1) 
		plug_unit(bus, device, fsname, fstype, unmount, timeout, xlv_vol);

	/* make sure we aren't swapped */
	if (plock(PROCLOCK) != 0) {
		/* special case most probable cause of failure and
		   print clear warning message */
		if (errno == EPERM) {
			printf("Must be super user to perform hot swap\n");
		}
		perror("could not perform pindown");
		exit(-1);
	}

	if (unmount) {
		if (umount(fsname) != 0) {
			sprintf(err_str, "umount of %s failed", fsname);
			perror(err_str);
			err_val = -1;
			goto cleanup_exit;
		}
		printf("unmount of %s successful\n", fsname);
	}

	if (xlv_vol != -1) {
		printf("performing xlv shutdown\n");
		/* at some point we may want suspend activity
		   only a select list of xlv volumes */
		if (strlen(xlv_vol_list) > 0) {
			sprintf(sys_str, "xlv_shutdown -n %s\n", xlv_vol_list);
			system(sys_str);
		}
		else
			system("xlv_shutdown");
	}

	if((dsp = dsopen(device, O_RDONLY)) == NULL) {
		/* if open fails, try pre-pending /dev/scsi */
		char buf[256];
		strcpy(buf, "/dev/scsi/");
		if((strlen(buf) + strlen(device)) < sizeof(buf)) {
			strcat(buf, device);
			dsp = dsopen(buf, O_RDONLY);
		}
		if(!dsp) {
			fflush(stdout);
			sprintf(err_str, "cannot open %s", device);
			perror(err_str);
			err_val = -1;
			goto cleanup_exit;
		}
	}

	/* perform spin down */
	if(startunit1b(dsp, 0, 0)) {
		printf("error: could not perform spin down\n");
		err_val = -1;
		goto cleanup_exit;
	}
	dsclose(dsp);

	printf("spin down complete\n");

	sprintf(q_device, "/hw/scsi_ctlr/%d/bus", bus);
	/* perform quiesce */
	if((fd = open(q_device, O_RDONLY)) == -1) {
	    /* if open fails, try pre-pending /dev/scsi */
	    char buf[256];
	    strcpy(buf, "/dev/scsi/");
	    if((strlen(buf) + strlen(q_device)) < sizeof(buf)) {
		strcat(buf, q_device);
		fd = open(buf, O_RDONLY);
	    }
	    if(fd == -1) {
		printf("%s:  ", q_device);
		fflush(stdout);
		perror("cannot open");
		err_val = -1;
		goto cleanup_exit;
	    }
	}

	while (state != QUIESCE_IS_COMPLETE) {
		scsiioctl.sb_opt = in_progress_timeout * HZ;
		scsiioctl.sb_arg = (quiesce_time_usr ) * HZ;
		
		if (ioctl(fd, SOP_QUIESCE, &scsiioctl) != 0) {
			printf("%s:  ", q_device);
			printf("quiesce failed: %s\n", strerror(errno));
			err_val = -1;
			goto cleanup_exit;
		}
		
		do {
			scsiioctl.sb_addr = (uintptr_t)&state;
			
			if (ioctl(fd, SOP_QUIESCE_STATE, &scsiioctl) != 0) {
				printf("%s:  ", q_device);
				printf("poll failed: %s\n", strerror(errno));
				err_val = -1;
				goto cleanup_exit;
			}
			if(state == NO_QUIESCE_IN_PROGRESS) {
				/* timer must have popped */
				printf("Failed to quiesce bus in alloted time (%d seconds)\n",in_progress_timeout);
				exit(-1);
			}
		} while  (state != QUIESCE_IS_COMPLETE);
	}
	printf("bus is quiesced\n");
	FD_ZERO(&fdset);
	FD_SET(STDIN_FILENO, &fdset);
	timeout_val.tv_sec = 300;
	timeout_val.tv_usec = 0;

	printf("%s ready to be unplugged\n", q_device);
	printf("\n -  Hit enter once drive removed - \n");
	rtn_val = select(1, &fdset, NULL, NULL, &timeout_val);

	
	if (rtn_val == 0) {
		timeout_val.tv_sec = 10;
		timeout_val.tv_usec = 0;
		printf("\n%d secs left to remove drive\n",
		       timeout_val.tv_sec);
		printf("\n -  Hit enter once drive is removed - \n");
		select(1, &fdset, NULL, NULL, &timeout_val);
	}


	if (ioctl(fd, SOP_UN_QUIESCE, &scsiioctl) != 0) {
		printf("%s:  ", q_device);
		printf("unquiesce failed: %s\n", strerror(errno));
		exit(1);
	}
	printf("bus now unquiesced: %s\n",q_device);

	/* now scan the bus.  things might have changed */
	printf("probing the bus and updating the hw graph - please be patient (2 minutes)\n");
	if (ioctl(fd, SOP_SCAN, &scsiioctl) != 0) {
		printf("%s:  ", q_device);
		printf("probe failed: %s\n", strerror(errno));
		exit(1);
	}

	close(fd);

	printf("reconfiguring /dev\n");
	system("ioconfig -f /hw");


      cleanup_exit:
	if (plock(UNLOCK) != 0) {
		perror("warning could not unpin mmeory");
		err_val = -1;
	}

	if (err_val != -1)
		printf("unplug successful\n");
	return(err_val);
}

void
plug_unit(int bus, char device[128], char fsname[128], 
	  char fstype[16], int unmount, int timeout, int xlv_vol)
{
	char sys_str[128];
	fd_set fdset;
	struct timeval timeout_val;
	int rtn_val;

	if (strlen(fstype) < 1) {
		printf("%s\n", USAGE);
		exit(-1);
	}

	printf("%s ready to be plugged in\n", device);
	FD_ZERO(&fdset);
	FD_SET(STDIN_FILENO, &fdset);
	if (timeout != -1)
		timeout_val.tv_sec = timeout;
	else
		timeout_val.tv_sec = 300;
	timeout_val.tv_usec = 0;


	printf("\n -  Hit enter once drive is inserted - \n");
	rtn_val = select(1, &fdset, NULL, NULL, &timeout_val);

	
	if (rtn_val == 0) {
		timeout_val.tv_sec = 10;
		timeout_val.tv_usec = 0;
		printf("\n%d secs left to insert drive ctrl-c to abort\n",
		       timeout_val.tv_sec);
		printf("\n -  Hit enter once drive is inserted - \n");
		select(1, &fdset, NULL, NULL, &timeout_val);
	}

	sprintf(sys_str, "scsiha -p %d", bus);

	printf("probing the bus and updating the hw graph - please be patient (2 minutes)\n");

	system(sys_str);
	

	printf("reconfiguring /dev\n");
	system("ioconfig -f /hw");

	if (xlv_vol == 1) {
		printf("performing xlv assemble\n");
		system("xlv_assemble");
	}

	sleep(1);

	if (unmount) {
		sprintf(sys_str, "mount -t %s %s %s",fstype, device, fsname);
		system(sys_str);
		sleep(1);
		sprintf(sys_str, "umount %s\n", fsname);
		system(sys_str);
		sleep(1);
		printf("mounting %s\n", fsname);
		sprintf(sys_str, "mount -t %s %s %s",fstype, device, fsname);
		system(sys_str);

	}

	exit(0);

}
