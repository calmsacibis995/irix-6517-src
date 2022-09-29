#include <sys/unistd.h>
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
#include <signal.h>
#include <sys/stat.h>
#include <invent.h>

#define BUF_SIZE 4096
#define NUMB_SLICES 16

struct scsi_ha_op	scsiioctl;

char ctrl_c_msg[64];
int pinned = 0;

#define USAGE "scsiadminswap\n\t-u (unplug)\n\t -p (plug)\n\t-d device number\n\t-b bus number\n\t[-t timeout val (default 300 secs)]"


void plug_unit(int bus, int device, int timeout);

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

/* ignore a ctrl-c otherwise if the scsi bus is quiesce and we
 * exit, we may never unquisce the bus and bad things will happen.
 */

static void
sig_int_handler()
{
	if (strlen(ctrl_c_msg) > 1) {
		printf("%s\n", ctrl_c_msg);
	}
	else {
		if (pinned)
			if (plock(UNLOCK) != 0) {
				perror("warning could not unpin mmeory");
			}
		exit(0);
	}
}

int 
checkforquiesce(int bus)
{

	inventory_t *inventlist;
	int	found=0;
	int	returnval=0;

	while (found == 0) {
		inventlist = (inventory_t *)getinvent();
		if (inventlist != NULL) {

			if ( (inventlist->inv_class == INV_DISK) && 
			(inventlist->inv_type == INV_SCSICONTROL) &&
			((int)inventlist->inv_controller) == bus) {

				found = 1;
				if( (inventlist->inv_state == INV_WD93)   ||
				    (inventlist->inv_state == INV_WD93A)   ||
				    (inventlist->inv_state == INV_WD93B) ||
				    (inventlist->inv_state == INV_WD95A)   ||
				    (inventlist->inv_state == INV_SCIP95)   ||
				    (inventlist->inv_state == INV_ADP7880) )
					returnval = 0;
				else 
					returnval = 1;

			}
		} else {
			returnval = 0;
			found = 1;
		}
	}

	return(returnval);

}

main(int argc, char **argv)
{
	int err_val = 0;
	int bus = -1;
	int device = -1;
	struct dsreq *dsp;
	int c, i;
	int plug = -1;
	int force = 0;
	int unplug = -1;
	char err_str[64];
	volatile int state = 0;
	int fd, temp_fd, temp1_fd;
	int timeout = -1;
	int in_progress_timeout = 10; /* default */
	char q_device[128];
	char unplug_device[128];
	fd_set fdset;
	struct timeval timeout_val;
	int rtn_val;
	struct sigaction act;
	char sys_str[128];
	char temp_str[128];
	char search_str[128];
	char buf[BUF_SIZE];
	FILE *ptr;


	strcpy(ctrl_c_msg, "");

	act.sa_flags = 0;
	act.sa_handler = sig_int_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);

	while ((c = getopt(argc, argv, "b:d:fpt:u")) != -1) {
		switch (c) {
		case 'b':
			bus = atoi(optarg);
			break;
		case 'd':
			device = atoi(optarg);
			break;
		case 'f':
			force = 1;
			break;
		case 'p':
			plug = 1;
			if (unplug != -1) {
				printf("%s\n", USAGE);
				exit(-1);
			}
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
		default:
			printf("%s\n", USAGE);
			exit(-1);
		}
	}

	if ((bus == -1) ||((plug == -1) && (unplug == -1))) {
		printf("%s\n", USAGE);
		exit(-1);
	}
	if ((unplug != -1) &&  (device == -1)) {
	  	printf("%s\n", USAGE);
		exit(-1);
	}

	/* check to see if this scsi processor is supported for quiescebus */
	if (checkforquiesce(bus)==0){
		printf("This scsi controller does not support quiescing bus.\n");
		exit(-1);
	}


	if ((plug == 1) && (unplug == -1)) {
		plug_unit(bus, device, timeout);
		exit(0);
	}

	/* check in three places for whether anybody is currently
	   using this disk */

	/* check first in mtab */



	sprintf(sys_str, "grep /dev/dsk/dks%dd%d /etc/mtab", bus, device);
	if ((ptr = popen(sys_str, "r")) != NULL) {
		while (fgets(buf, BUF_SIZE, ptr) != NULL) {
			printf("/dev/dsk/dks%dd%d is currently mounted\n", bus, device);
			if (! force) {
				pclose(ptr);
				exit(0);
			}
		}
	}
	pclose(ptr);

	temp_fd = 0;  /* eliminate silly compiler warning */
	if (dup2(STDERR_FILENO, temp_fd) != -1) {

		close(STDERR_FILENO);
		temp1_fd = open("/dev/null", O_WRONLY);
		
		for (i=0; i<NUMB_SLICES; i++) {
			sprintf(sys_str, "fuser /dev/rdsk/dks%dd%ds%d", 
				bus, device, i);
			
			if ((ptr = popen(sys_str, "r")) != NULL) {
				while (fgets(buf, BUF_SIZE, ptr) != NULL) {
					printf(" /dev/rdsk/dks%dd%ds%d in use\n",
					       bus, device, i);
					if (! force) {
						pclose(ptr);
						exit(0);
					}
				}
			}
			pclose(ptr);
		}
		close(temp1_fd);
		if (dup2(temp_fd, STDERR_FILENO) == -1) {
			printf("could not re-establish stderr\n");
			exit(0);
		}
	}
	else {
		printf("warning: unable to check raw disk usage\n");
	}

	ptr = fopen("/sbin/xlv_mgr", "r");
	if (ptr != NULL) {
		fclose(ptr);
		sprintf(search_str, "/dev/dsk/dks%dd%d\n", bus, device);
		sprintf(sys_str, "/sbin/xlv_mgr -Rc \"show kernel\"");
		if ((ptr = popen(sys_str, "r")) != NULL) {
			while (fgets(buf, BUF_SIZE, ptr) != NULL) {
				if (strstr(buf, search_str) != NULL) {
					printf("/dev/dsk/dks%dd%d as an xlv volume\n", bus, device);
					if (! force) {
						pclose(ptr);
						exit(0);
					}
				}
			}
		}
		pclose(ptr);
	}


	/* now check if anybody currently is using through a raw access */
	
	pinned = 1;
	/* make sure we aren't swapped */
	if (plock(PROCLOCK) != 0) {
		/* special case most probable cause of failure and
		   print clear warning message */
		if (errno == EPERM) {
			printf("Must be super user to perform admin swap\n");
		}
		perror("could not perform pindown");
		exit(-1);
	}

	sprintf(q_device, "/dev/scsi/sc%dd%dl0", bus, device);
	if((dsp = dsopen(q_device, O_RDONLY)) == NULL) {
		/* if open fails, try pre-pending /dev/scsi */
		char buf[256];
		strcpy(buf, "/dev/scsi/");
		if((strlen(buf) + strlen(q_device)) < sizeof(buf)) {
			strcat(buf, q_device);
			dsp = dsopen(buf, O_RDONLY);
		}
		if(!dsp) {
			fflush(stdout);
			sprintf(err_str, "cannot open %s", q_device);
			perror(err_str);
			err_val = -1;
			goto cleanup_exit;
		}
	}

	printf("spinning down drive\n");
	/* perform spin down */
	if(startunit1b(dsp, 0, 0)) {
		printf("error: could not perform spin down\n");
		err_val = -1;
		goto cleanup_exit;
	}
	dsclose(dsp);

	sleep(5);
	printf("spin down complete\n");

	strcpy(ctrl_c_msg, "The bus is quiesced, exiting now will cause problems - ctrl-c ignored\n");
	printf("The bus is quiescing, exiting now will cause problems - ctrl-c ignored\n");
	printf("Please wait for bus to quiesce.\n"); 
	
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
		scsiioctl.sb_arg = (timeout ) * HZ;
		
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
	/*
	 * There currently is a printf statememt indicating this.  However, it seems
	 * that it (spin down command) returns (prints this message) very quickly -
	 * probably too quick for it actually to have physically stopped.  After
	 * talking with Radek we decided that it makes sense to add a couple second
	 * delay before printing out the message.  This change will go into kudzu.
	 */
	printf("bus is quiesced\n");
	}



	FD_ZERO(&fdset);
	FD_SET(STDIN_FILENO, &fdset);
	timeout_val.tv_sec = 300;
	timeout_val.tv_usec = 0;

	strcpy(temp_str, q_device);
	temp_str[strlen(q_device) - 3] = '\0';
	sprintf(unplug_device, "%starget/%d", temp_str, device);

	printf("%s ready to be unplugged\n", unplug_device);
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

	strcpy(ctrl_c_msg, "");
	if (plug == 1) {
		plug_unit(bus, device, timeout);
	}
	else {
		/* now scan the bus.  things might have changed */
		printf("probing the bus and updating the hw graph - please be patient (2 minutes)\n");
		if (ioctl(fd, SOP_SCAN, &scsiioctl) != 0) {
			printf("%s:  ", q_device);
			printf("probe failed: %s\n", strerror(errno));
			exit(1);
		}
		
		close(fd);
		
		printf("reconfiguring /dev (may take a while on large configurations)\n");
		sprintf(sys_str, "ioconfig -f /hw/scsi_ctlr/%d/target", bus);
		system(sys_str);
	}

      cleanup_exit:
	if (plock(UNLOCK) != 0) {
		perror("warning could not unpin mmeory");
		err_val = -1;
	}
	
	pinned = 0;
	if (err_val != -1)
		printf("scsiadminswap successful\n");
	return(err_val);
}

void
plug_unit(int bus, int device, int timeout)
{
	char sys_str[128];
	fd_set fdset;
	struct timeval timeout_val;
	struct stat statret = {0};
	 char q_device[128];
	int rtn_val;
	int err_val = 0;
	volatile int state = 0;
	int fd;
	int in_progress_timeout = 10; /* default */

	/* check to make sure the drive has not been part of the hinv */
	sprintf(sys_str,"/hw/rdisk/dks%dd%dvol",bus, device);
	stat(sys_str, &statret);
	
	if (statret.st_blksize > 0) {
		printf("drive %d is already configed into the system on bus %d\n", device, bus); 
		goto return_func;
	} else {
		printf("drive %d is ready to be plugged into bus %d\n", device, bus);
	}
	    	

	printf("Please wait for bus to quiesce.\n"); 
	
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
		goto cleanup_return;
	    }
	}

	while (state != QUIESCE_IS_COMPLETE) {
		scsiioctl.sb_opt = in_progress_timeout * HZ;
		scsiioctl.sb_arg = (timeout ) * HZ;
		
		if (ioctl(fd, SOP_QUIESCE, &scsiioctl) != 0) {
			printf("%s:  ", q_device);
			printf("quiesce failed: %s\n", strerror(errno));
			err_val = -1;
			goto cleanup_return;
		}
		
		do {
			scsiioctl.sb_addr = (uintptr_t)&state;
			
			if (ioctl(fd, SOP_QUIESCE_STATE, &scsiioctl) != 0) {
				printf("%s:  ", q_device);
				printf("poll failed: %s\n", strerror(errno));
				err_val = -1;
				goto cleanup_return;
			}
			if(state == NO_QUIESCE_IN_PROGRESS) {
				/* timer must have popped */
				printf("Failed to quiesce bus in alloted time (%d seconds)\n",in_progress_timeout);
				exit(-1);
			}
		} while  (state != QUIESCE_IS_COMPLETE);

	printf("bus is quiesced\n");

	}




	FD_ZERO(&fdset);
	FD_SET(STDIN_FILENO, &fdset);
	if (timeout != -1)
		timeout_val.tv_sec = timeout;
	else
		timeout_val.tv_sec = 300;
	timeout_val.tv_usec = 0;


	printf("\n -  Hit enter once drive is inserted - \n");

	rtn_val = select(1, &fdset, NULL, NULL, &timeout_val);

	if (ioctl(fd, SOP_UN_QUIESCE, &scsiioctl) != 0) {
		printf("%s:  ", q_device);
		printf("unquiesce failed: %s\n", strerror(errno));
		exit(1);
	}
	printf("bus now unquiesced: %s\n",q_device);

	/* Since there is a delay in the spin up of the drive based on the id * 10 sec */
	/* we need to insert an sleep */

	printf("\n -  wait for drive to spin up in %d sec \n", device*10); 
	sleep( device * 10);
	
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

	printf("reconfiguring /dev (may take a while on large configurations)\n");
	sprintf(sys_str, "ioconfig -f /hw/scsi_ctlr/%d", bus);
	system(sys_str);

return_func:

	return;

 cleanup_return:
	if (plock(UNLOCK) != 0) {
		perror("warning could not unpin mmeory");
		err_val = -1;
	}
	
	pinned = 0;
	if (err_val != -1)
		printf("scsiadminswap successful\n");
	return;

}
