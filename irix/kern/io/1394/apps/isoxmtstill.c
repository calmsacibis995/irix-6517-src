#include <sys/types.h>
#include <sys/mman.h>
#include "stdlib.h"
#include "fcntl.h"
#include "dvc1394.h"


main(argc,argv)
int argc;
char **argv;
{
    int fd,filefd,s,arg;
    char buf[144000];
    if(argc != 1 && argc != 2) {
	printf("Usage %s\n",argv[0]);
	exit(1);
    }
    fd=open("/hw/ieee1394/dvc/dvc1394",O_RDWR);
    if(fd<0) {
	printf("Can't open /hw/ieee1394/dvc/dvc1394\n");
	exit(1);
    }

    printf("Setting Direction\n");
    arg=2;
    s=ioctl(fd,DVC1394_SET_DIRECTION,&arg);
    if(s<=0)perror("DVC1394_SET_DIRECTION");
    printf("s=%d\n",s);
    printf("Done with setting Direction\n");

    if(argc==2) {
    filefd=open(argv[1],O_RDONLY);
	if(filefd<0) {
	    printf("Can't open %s\n",argv[1]);
	    exit(0);
        }
    s=read(filefd,buf,144000);
    if(s!= 144000)printf("Short read (%d) on %s\n",s,argv[1]);
    printf("Loading Frame\n");
    s=ioctl(fd,DVC1394_LOAD_FRAME,buf);
    printf("s=%d\n",s);
    printf("Done with Loading Frame\n");
    }
    sleep(5);
    exit(0);
}
