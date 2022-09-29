#include "fcntl.h"
#include "dvc1394.h"

main(argc,argv)
int argc;
char **argv;
{
    int fd;
    dvc1394_gain_lose_interest_t interest;

    if (argc != 5) {
       printf("Usage: lose_interest Vendor_ID (Device_ID OR 0) begaddr (endaddr OR 0)\n");
       printf("Example: lose_interest 0x8045 0 0xfffff000040c 0\n");
       exit(1);
    }
    fd=open("/hw/ieee1394/dvc/dvc1394",O_RDWR);
    if(fd<0) {
        printf("Can't open /hw/ieee1394/dvc/dvc1394\n");
        exit(1);
    }

    interest.Vendor_ID = strtoull(argv[1], 0, 0);
    interest.Device_ID = strtoull(argv[2], 0, 0);
    interest.begaddr   = strtoull(argv[3], 0, 0);
    interest.endaddr   = strtoull(argv[4], 0, 0);

    ioctl(fd,DVC1394_LOSE_INTEREST, &interest);

    close(fd);
    exit(0);
}

