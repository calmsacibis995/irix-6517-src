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
    ioctl_quadlock_t a;
    int fd,s,args[25],us,them;
    int arg_value;

    if(argc != 4) {
        printf("Usage %s destination_offset arg_value data_value\n",argv[0]);
	printf("Example: %s 0xfffff0000220 0x9d8 0x0\n", argv[0]);
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

    arg_value = strtoul(argv[2], 0, 0);
    a.destination_id = them;
    a.destination_offset = strtoull(argv[1], 0, 0);
    a.quadlet_arg_value = arg_value;
    a.quadlet_data_value = strtoul(argv[3], 0, 0);
    a.seqnum=tree_db.bus_reset_sequence_number;
    a.status=0xdeadfeed;

    s=ioctl(fd,FW_QUADLOCK,&a);
    if ((s==0) && (a.old_value!= arg_value)) { 
        /* lock went through */
        printf("Compare & Swap failed. Try again: 0x%x, 0x%x.\n", a.old_value,
			arg_value);
        return (-1);
    }
    else printf("Success\n");

    printf("quadwrite ioctl returns %d\n",s);
    printf("status             0x%x\n",a.status);
    exit(0);
}

