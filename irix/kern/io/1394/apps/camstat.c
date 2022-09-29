#include <sys/types.h>
#include <sys/mman.h>
#include "stdlib.h"
#include "fcntl.h"
#include "ieee1394.h"
#include "signal.h"

self_id_db_t    tree_db;
int fd;
int us,them;

int catch(){}

main(argc,argv)
int argc;
char **argv;
{
    int s,q;
    long long readaddr, base;

/*
    sigset(SIGINT,catch);
*/
    fd=open("/hw/ieee1394/ieee1394",O_RDWR);
    if(fd<0) {
	printf("Can't open /dev/fw\n");
	exit(1);
    }
    ioctl(fd,FW_GET_TREE,&tree_db);
    us=tree_db.our_phy_ID;
    if(us==0)them=1;else them=0;
    base=0xfffff0f00000;

    printf(    "000 Initialize		0x%x\n",quadread(base+0x000));
    printf(    "100 V_FORMAT_INQ	0x%x\n",quadread(base+0x100));
    printf(    "180 V_MODE_INQ		0x%x\n",quadread(base+0x180));
    printf(    "200 V_RATE_INQ_0_0	0x%x\n",quadread(base+0x200));
    printf(    "204 V_RATE_INQ_0_1	0x%x\n",quadread(base+0x204));
    printf(    "208 V_RATE_INQ_0_2	0x%x\n",quadread(base+0x208));
    printf(    "20C V_RATE_INQ_0_3	0x%x\n",quadread(base+0x20C));
    printf(    "210 V_RATE_INQ_0_4	0x%x\n",quadread(base+0x210));
    printf(    "214 V_RATE_INQ_0_5	0x%x\n",quadread(base+0x214));
    printf(    "400 BASIC_FUNC_INQ	0x%x\n",quadread(base+0x400));

    printf(    "404 FEATURE_HI_INQ	0x%x\n",q=quadread(base+0x404));
	printf("\t");
	if(!(q&0x80000000))printf("No ");
	printf("Brightness\n");
	printf("\t");
	if(!(q&0x40000000))printf("No ");
	printf("Exposure\n");
	printf("\t");
	if(!(q&0x20000000))printf("No ");
	printf("Sharpness\n");
	printf("\t");
	if(!(q&0x10000000))printf("No ");
	printf("White_Balance\n");
	printf("\t");
	if(!(q&0x08000000))printf("No ");
	printf("Hue\n");
	printf("\t");
	if(!(q&0x04000000))printf("No ");
	printf("Saturation\n");
	printf("\t");
	if(!(q&0x02000000))printf("No ");
	printf("Gamma or ON/OFF\n");
	printf("\t");
	if(!(q&0x01000000))printf("No ");
	printf("Shutter\n");
	printf("\t");
	if(!(q&0x00800000))printf("No ");
	printf("Gain\n");
	printf("\t");
	if(!(q&0x00400000))printf("No ");
	printf("Iris\n");
	printf("\t");
	if(!(q&0x00200000))printf("No ");
	printf("Focus\n");
    printf(    "408 FEATURE_LO_INQ	0x%x\n",q=quadread(base+0x408));
	printf("\t");
	if(!(q&0x80000000))printf("No ");
	printf("Zoom\n");
	printf("\t");
	if(!(q&0x40000000))printf("No ");
	printf("Pan\n");
	printf("\t");
	if(!(q&0x20000000))printf("No ");
	printf("Tilt\n");
    printf(    "600 CUR_V_FRM_RATE	0x%x\n",q=quadread(base+0x600));
    printf(    "604 CUR_V_FRM_MODE	0x%x\n",q=quadread(base+0x604));
    printf(    "608 CUR_V_FRM_FORMAT	0x%x\n",q=quadread(base+0x608));
    printf(    "60C ISO_CHANNEL & SPEED	0x%x\n",q=quadread(base+0x60C));
    printf(    "610 CAMERA_POWER	0x%x\n",q=quadread(base+0x610));
    printf(    "614 ISO_EN		0x%x\n",q=quadread(base+0x614));

    printf(    "828 FOCUS  		0x%x\n",q=quadread(base+0x828));
    printf(    "880 ZOOM  		0x%x\n",q=quadread(base+0x880));
}

int
quadread(long long readaddr)
{
    ioctl_quadread_t a;
    int s;


    sginap(1);
    a.destination_id = them;
    a.destination_offset = readaddr;
    a.quadlet_data = 0xdeadbead;
    a.seqnum=tree_db.bus_reset_sequence_number;
    a.status=0xdeadfeed;

    s=ioctl(fd,FW_QUADREAD,&a);

if(s!=0){
    printf("\n");
    perror("");
    printf("\n");
}

#ifdef LATER
    printf("destination_id     0x%x\n",   a.destination_id);
    printf("destination_offset 0x%llx\n", a.destination_offset);
    printf("quadlet_data       0x%x\n",   a.quadlet_data);
    printf("seqnum             %lld\n",   a.seqnum);
    printf("status             0x%x\n",   a.status);
#endif
    return a.quadlet_data;
}
