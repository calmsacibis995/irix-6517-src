#include <sys/types.h>
#include <sys/mman.h>
#include "stdlib.h"
#include "fcntl.h"
#include "dvc1394.h"

#define NFRAMES 10

main(argc,argv)
int argc;
char **argv;
{
    int fd,filefd,s,arg,i;
    char buf[144000*NFRAMES];
    if(argc != 1 && argc != 2) {
	printf("Usage %s\n",argv[0]);
	exit(1);
    }
    fd=open("/hw/ieee1394/dvc/dvc1394",O_RDWR);
    if(fd<0) {
	printf("Can't open /hw/ieee1394/dvc/dvc1394\n");
	exit(1);
    }

    s=ioctl(fd,DVC1394_SET_DIRECTION,&arg);
    if(s<=0)perror("DVC1394_SET_DIRECTION");
    printf("s=%d\n",s);
    printf("Done with setting Direction\n");

    filefd=open(argv[1],O_RDONLY);
	if(filefd<0) {
	    printf("Can't open %s\n",argv[1]);
	    exit(0);
        }

    s=read(filefd,buf,144000*NFRAMES);
    if(s!= 144000*NFRAMES)printf("Short read (%d) on %s\n",s,argv[1]);
    while(1) {
	for(i=0;i<10;i++) {
            printf("Putting Frame\n");
            s=ioctl(fd,DVC1394_PUT_FRAME,buf);
            printf("s=%d\n",s);
            printf("Done with putting Frame\n");
	}
    }
}
