#include "fcntl.h"
main(argc, argv)
int argc;
char **argv;
{
    int fd,s,cmd;
    int arg[2];
    if(argc < 3)usage();
    fd=open("/hw/ieee1394/ieee1394",O_RDWR);
    cmd=strtoul(argv[1], 0, 0);
    if(cmd==1 && argc != 3)usage();
    if(cmd==2 && argc != 4)usage();
    if(cmd != 1 && cmd != 2)usage();
    if(cmd == 1) {
        arg[0]=strtoul(argv[2], 0, 0);
        s=ioctl(fd,cmd,arg);
/*
        printf("fd %d s %d reg %d val 0x%x\n",fd,s,arg[0], arg[1]);
*/
        printf("0x%.8x\n", arg[1]);
    }
    if(cmd == 2) {
        arg[0]=strtoul(argv[2], 0, 0);
        arg[1]=strtoul(argv[3], 0, 0);
        s=ioctl(fd,cmd,arg);
/*
        printf("fd %d s %d reg %d val 0x%x\n",fd,s,arg[0], arg[1]);
*/
    }
    exit(0);
}
usage()
{
    printf("usage: fwioctl 1 addr\n");
    printf("       fwioctl 2 addr val\n");
    exit(1);
}
