#include <sys/types.h>
#include <sys/mman.h>
#include "stdlib.h"
#include "fcntl.h"
#include "ieee1394.h"

main(argc,argv)
int argc;
char **argv;
{
    self_id_db_t    tree_db;
    ioctl_quadread_t a;
    int fd,s,us,them;
    long long readaddr;
    if((argc != 2)&&(argc != 3)) {
        printf("Usage %s destination_offset\n",argv[0]);
	printf("Example: %s 0xfffff000040c\n", argv[0]);
        exit(1);
    }
    fd=open("/hw/ieee1394/ieee1394",O_RDWR);
    if(fd<0) {
        printf("Can't open /hw/ieee1394/ieee1394\n");
        exit(1);
    }

    if (argc == 2) {
      ioctl(fd,FW_GET_TREE,&tree_db);
      us=tree_db.our_phy_ID;
      if(us==0)them=1;else them=0;
    }
    else them = atoi(argv[2]);

    readaddr = strtoull(argv[1], 0, 0);

    a.destination_id = them;
    a.destination_offset = readaddr;
    a.quadlet_data = 0xdeadbead;
    a.seqnum=tree_db.bus_reset_sequence_number;
    a.status=0xdeadfeed;

    s=ioctl(fd,FW_QUADREAD,&a);

    printf("destination_id     0x%x\n",   a.destination_id);
    printf("destination_offset 0x%llx\n", a.destination_offset);
    printf("quadlet_data       %d, 0x%x\n",   a.quadlet_data, 
					      a.quadlet_data);
    printf("seqnum             %lld\n",   a.seqnum);
    printf("status             0x%x\n",   a.status);
    exit(0);
}

