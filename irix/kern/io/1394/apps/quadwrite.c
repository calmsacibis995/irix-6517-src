/* iarg[0] dma length
   iarg[1-8] sendbuf initial value
   iarg[9-16] sendbuf final value
   iarg[17-24] rcvbuf final value
*/

#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "fcntl.h"
#include "ieee1394.h"

main(argc,argv)
int argc;
char **argv;
{
    self_id_db_t    tree_db;
    ioctl_quadwrite_t a;
    int fd,s,args[25],us,them;

    if(argc != 3 && argc != 4) {
        printf("Usage %s destination_offset value\n",argv[0]);
	printf("Example: %s 0xfffff0000200 0xff\n", argv[0]);
        exit(1);
    }
    fd=open("/hw/ieee1394/ieee1394",O_RDWR);
    if(fd<0) {
        printf("Can't open /hw/ieee1394/ieee1394\n");
        exit(1);
    }
    s=ioctl(fd,FW_GET_TREE,&tree_db);
    printf("get_tree ioctl returns %d\n",s);
    us=tree_db.our_phy_ID;
    if(us==0)them=1;else them=0;

    if(argc ==4 && argv[3][0]=='x') {
        int i;
        for(i=0;i<=62;i++) {
            a.destination_id = i;
            a.destination_offset = strtoull(argv[1], 0, 0);
            a.quadlet_data = strtoul(argv[2], 0, 0);
            a.seqnum=tree_db.bus_reset_sequence_number;
            a.status=0xdeadfeed;
            s=ioctl(fd,FW_QUADWRITE,&a);
            printf("%3d 0x%x\n",i,a.status);
        }
    exit(0);
    }
    a.destination_id = them;
    if(argc==4) a.destination_id=atoi(argv[3]);
    a.destination_offset = strtoull(argv[1], 0, 0);
    a.quadlet_data = strtoul(argv[2], 0, 0);
    a.seqnum=tree_db.bus_reset_sequence_number;
    a.status=0xdeadfeed;

    s=ioctl(fd,FW_QUADWRITE,&a);
    printf("quadwrite ioctl returns %d\n",s);
    printf("status             0x%x\n",a.status);
    exit(0);
}

