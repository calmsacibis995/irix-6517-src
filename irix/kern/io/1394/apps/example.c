#include "dvc1394.h"
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bstring.h>


/* -------------------- libdvc1394.c --------------------------- */

#define ROUNDTOPAGE(x) (((x)+4095)& ~4095)

dvc1394_state_t *
dvc1394openinputport(int nbufs, int debug_level)
{
    int fd;
    dvc1394_buf_t *bufs;
    int mmapsize;
    dvc1394_state_t *state;

    fd=open("/hw/ieee1394/dvc/dvc1394",O_RDWR);
    if(fd<0)perror("/hw/ieee1394/dvc/dvc1394");
    if(fd==-1)return 0;
    
    ioctl(fd,DVC1394_SET_DEBUG_LEVEL,&debug_level);

    mmapsize=ROUNDTOPAGE(nbufs*sizeof(dvc1394_buf_t));
    bufs=(dvc1394_buf_t *)
        mmap(0, mmapsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if(bufs==(dvc1394_buf_t*)MAP_FAILED){
        close(fd);
        return 0;
    }

    ioctl(fd,DVC1394_SET_DIRECTION,1);

    state=(dvc1394_state_t *)malloc(sizeof(dvc1394_state_t));
    state->fd=fd;
    state->bufs=bufs;
    state->nbufs=nbufs;
    state->mmapsize=mmapsize;
    return state;
}

void
dvc1394closeport(dvc1394_state_t *state)
{
    close(state->fd);
    free(state);
}

void
dvc1394setchannel(dvc1394_state_t *state, int channel)
{
    /* Doesn't do anything yet */
    ioctl(state->fd,DVC1394_SET_CHANNEL,&channel);
}

void
dvc1394getframe(dvc1394_state_t *state, long long **framep, int *validflag)
{
    int bufnum;
    dvc1394_buf_t *bufp;
    bufnum=ioctl(state->fd,DVC1394_GET_FRAME,0);
    bufp= &(state->bufs[bufnum]);
    *framep = &(bufp->frame[0]);
    *validflag=bufp->valid;
}

void
dvc1394releaseframe(dvc1394_state_t *state)
{
    ioctl(state->fd,DVC1394_RELEASE_FRAME,0);
}

int
dvc1394getfilled(dvc1394_state_t *state)
{
    int s;
    s=ioctl(state->fd,DVC1394_GET_FILLED,0);
    return s;
}

int
dvc1394getfd(dvc1394_state_t *state)
{
    return state->fd;
}

int
dvc1394isntsc(dvc1394_state_t *state)
{
    int bufnum;
    dvc1394_buf_t *bufp;

    bufnum=ioctl(state->fd,DVC1394_GET_FRAME,0);
    bufp= &(state->bufs[bufnum]);
    return bufp->ntsc;
}

/* ----------- Example program dvc1394record.c ----------------- */

#define NDVCBUFS 30 /* 30 frames of buffering - 1 second */

int expected_ntsc=2;
#define dwrite write

main(argc,argv)
int argc;
char **argv;
{
    dvc1394_state_t *dvc1394state;
    long long *framep;
    int validflag;

    int framenumber=0,maxframenumber=0,ntsc;
    int outfd,s;
    int nfilled;
    fd_set rdfds;
    struct timeval timeout;
    int debug_level;

    if (argc == 1) {
	printf("Usage: %s debug_level no_of_frames\n", argv[0]); 
	printf("E.g. : %s    0           60       \n", argv[0]);
	exit(1);
    }

    debug_level=atoi(argv[1]);
    if ((debug_level < 0) || (debug_level >3)) {
	printf("debug_level must be 0 - 3\n");
	exit(1);
    }

    /* Open a dvc port */

    dvc1394state = dvc1394openinputport(NDVCBUFS, debug_level);
    if(dvc1394state==0){
        printf("Can't open a DVC 1394 port\n");
        exit(1);
    }
    dvc1394setchannel(dvc1394state, 0x3f); 

    /* Open an output file */

    outfd=open("dvc.out",O_RDWR|O_CREAT|O_TRUNC,0777);

    /* Parse args */

    maxframenumber=0;
    if(argc==3)maxframenumber=atoi(argv[2]);

    /* Record frames */

    FD_ZERO(&rdfds);
    while(1) {

        /* Block until a frame arrives.  Time out if one doesn't
           arrive soon */

        FD_SET(dvc1394getfd(dvc1394state),&rdfds);
        timeout.tv_sec=10;
        timeout.tv_usec=0;
        s=select(1+dvc1394getfd(dvc1394state),&rdfds,0,0,&timeout);
        if(s==0) {
            printf("No 1394 frames arrived in the last 10 seconds\n");
            exit(1);
        }

        /* Get the frame */
        dvc1394getframe(dvc1394state, &framep, &validflag);

        /* Check if it's NTSC or PAL */

        /* printf("Checking if it's NTSC or PAL\n"); */
        ntsc=dvc1394isntsc(dvc1394state);
        if(ntsc != expected_ntsc) {
            expected_ntsc=ntsc;
            /* printf("%s\n",ntsc?"NTSC":"PAL"); */
        }

        /* If the frame was garbled, don't write it */

        if(validflag) {
            if(ntsc) 
                dwrite(outfd,framep,120000);
            else
                dwrite(outfd,framep,144000);
        } else {
            printf("Frame %d invalid\n",framenumber);
        }

        /* Release the frame */

        dvc1394releaseframe(dvc1394state);
        framenumber++;
        if(framenumber == maxframenumber && maxframenumber != 0)
            break;
    }

    dvc1394closeport(dvc1394state);
    exit(0);
}
