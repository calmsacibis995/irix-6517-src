/*
 *      xbstat.c
 *      Tests the memory-mapping functions of the xbmon driver.
 *
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <curses.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/slotnum.h>
#include <sys/xbmon.h>

#define XB_TRUE 		1
#define XB_FALSE 		0
#define XB_STAT_VERSION 	1

int interrupt_received = 0;

static char	*pname;

char *devname = "/dev/xbmon";

void
interrupt(void)
{
	interrupt_received++;
}

void main(int argc, char *argv[])
{
        int             devfd;
        int             idx;
	int		forever = 1;
        xb_vcntr_block_t *blk;
        xb_vcounter_t    *vc;
        long		duration = 0;   
        time_t          start_time,now;
	char		line_buf[132];
	int		c, i;
	int		err = 0;
	__uint32_t	this_version;
	__uint32_t	slotnum;
	char		*slottype;

	pname = argv[0];
	while ((c = getopt(argc, argv, "d:t:")) != EOF) {
		switch (c) {
		case 'd':
		        devname = optarg;
			break;
		case 't':
			if ((duration = strtol(optarg, (char **) 0, 0)) <= 0)
				err++;
			break;
		case '?':
			err++;
			break;
		}	
	}

	if (err) {
		  fprintf(stderr, "usage: %s [-d devname] [-t sec]\n", pname);
		  exit(1);
	}

	for (i = optind; i < argc; i++) {
		fprintf(stderr, "%s: extra argument %s ignored\n", pname, argv[i]);
	}

	if (duration != 0) forever = 0;

	signal(SIGINT, interrupt);


        if( (devfd = open(devname, O_RDONLY)) < 0 ){
                perror(devname);
                exit(1);
        }

        if( ioctl(devfd, XB_GET_VERSION , &this_version) < 0 ){
                perror("ioctl(XB_GET_VERSION)");
                exit(1);
        }

	if (this_version != XB_STAT_VERSION) {
		fprintf(stderr,"Mismatched DRVR & API versions\n");
		exit(1);
	}

        if( ioctl(devfd, XB_ENABLE_MPX_MODE,0 ) < 0 ){
                perror("ioctl(XB_ENABLE_MPX_MODE)");
                exit(1);
        }

        if( ioctl(devfd, XB_START_TIMER, 0) < 0 ){
                perror("ioctl(XB_START_TIMER)");
                exit(1);
        }


        blk = (xb_vcntr_block_t *)
                mmap(0,sizeof(xb_vcntr_block_t),PROT_READ,MAP_PRIVATE,devfd,0);

        if (blk == NULL) {
                perror("Could not map device memory");
                exit(1);
        }

        time(&start_time);
	
        initscr();
	clear();
	refresh();

	move(0,0);
	addstr("Slot  Flags       Source         Destination      RcRtry  TxRtry");
	move(1,0);
	addstr("----  -----  ----------------  ----------------   ------  ------");

	for (;;) {
            if (interrupt_received)  break;
            time(&now);
            if (!forever && (now > (start_time + duration)))  break;
            
            for (idx = 0, vc = &(blk->vcounters[0]); idx < XB_MON_LINK_MASK+1; vc++,idx++) {
		move(idx+2,0);

		/* convert port numbers to slot numbers */
                slotnum = idx+8;
                if(ioctl(devfd, XB_GET_SLOTNUM, &slotnum) < 0) {
		    perror("ioctl(XB_GET_SLOTNUM)");
		    endwin();
		    munmap((caddr_t) blk,sizeof(xb_vcntr_block_t));
		    exit(1);
		} 
		if(SLOTNUM_GETCLASS(slotnum) == SLOTNUM_INVALID_CLASS) {
                    slotnum = idx+8;
                    slottype = "  ";
		} else {
                    switch(SLOTNUM_GETCLASS(slotnum)) {
                    case SLOTNUM_NODE_CLASS:
                    case SLOTNUM_KNODE_CLASS:
                        slottype = " N";
                        break;
                    case SLOTNUM_XTALK_CLASS:
                        slottype = "IO";
                        break;
                    default:
                        slottype = "  ";
                        break;
                    }
                    slotnum = SLOTNUM_GETSLOT(slotnum);
                }
                
		sprintf(line_buf,"%s%-2.d  %3llu    %16llu  %16llu   %6llu  %6llu\n",
			slottype, slotnum,
			vc->flags,vc->vsrc, vc->vdst, vc->crcv, vc->cxmt);
		addstr(line_buf);
            }
	    refresh();      
	    sginap(100); /* read vcounters and refresh display every second */
        }

 	endwin(); 

	munmap((caddr_t) blk,sizeof(xb_vcntr_block_t));

        if( ioctl(devfd, XB_STOP_TIMER, 0) < 0 ){
                perror("ioctl(XB_STOP_TIMER)");
                exit(1);
        }
        close(devfd);

        exit(0);
} /* main() */


