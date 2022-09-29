#include "fcntl.h"
#include "ieee1394.h"

main(argc,argv)
int argc;
char **argv;
{
    int fd;

    fd=open("/hw/ieee1394/ieee1394",O_RDWR);
    if(fd<0) {
        printf("Can't open /hw/ieee1394/ieee1394\n");
        exit(1);
    }

    ioctl(fd,FW_PRINTINTERESTTABLE);

    exit(0);
}

