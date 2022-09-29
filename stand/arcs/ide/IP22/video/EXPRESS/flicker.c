#include <stdio.h>
#include <fcntl.h>
#include <gl.h>
#include <device.h>
#include <math.h>
#include <poll.h>
#include "/usr/people/iching/bvdroot/usr/src/uts/mips/io/video/cv.h"
#include "/usr/people/iching/bvdroot/usr/src/uts/mips/io/video/EXPRESS/ev1_extern.h"
#include "regs.h"


#define         AB1_RED             0x0
#define         AB1_GREEN           0x8
#define         AB1_BLUE            0x10
#define		NTSC_MODE	    0x1


#define		PIXELS_PER_LINE		768

#define		NTSC_FORMAT	  	0x1
#define		NTSC_LINES		480
#define		NTSC_PIXELS    		640

#define		READ_REG		0x1
#define		WRITE_REG		0x0

static long  *bufptr, pixels;
static long  data,  index;
static long  xll, xur, yll, yur;
static long  line;
static long  dev;
static short val;


struct readbuf_t ab1_dram;

static int rcvbuf[PIXELS_PER_LINE];

int kv_fd;

static int open_kv()
{
        struct video_info vid_info[MAX_VIDEO_BOARDS];
        struct video_attach_board_args attach_args;
        int fd;

        int i;
        int j;
        int c;
        int zz;
        struct video_get_frame_args vframe;

        if ((fd = open("/dev/video.exp",O_RDONLY)) == -1){
                perror("Couldn't open input\n");
                exit(1);
        }
        attach_args.board_type = EXPRESS_TYPE_VIDEO;
        attach_args.board_num = 0;
        attach_args.vaddr = (unsigned long)0;

        if (ioctl(fd,V_ATTACH_BOARD,&attach_args) == -1){
                perror("V_ATTACH_BOARD\n");
                exit(2);
        }

    	if (ioctl(fd,V_INITIALIZE,&attach_args) == -1){
        	perror("V_INIT\n");
        	exit(2);
    	}

	return (fd);
}

static int ev1_reg(int fd, char RdWr, int reg, int val)
{
	int count;
	int myregs[10];

	count = 1;
	myregs[count++] = reg;
	myregs[count++] = val;
	myregs[0] = 2*sizeof(myregs[0]);

	if (RdWr) {
                if (ioctl(fd, V_GET_REG,myregs) == -1){
                        perror("V_GET_REG\n");
                        exit (0);
                }
                printf("Read cc1_reg :: 0x%x\n", myregs[2]);
		return(myregs[2]);
	} else {
                if (ioctl(fd, V_SET_REG,myregs) == -1){
                        perror("V_SET_REG\n");
                        exit (0);
                }

	}
	return(0);
}

Gfx_2_Vid_A(int fd, int fk, int av) 
{
 	ev1_reg(fd, WRITE_REG, csc0,    0x3A);		
 	ev1_reg(fd, WRITE_REG, ccind36, 1);
	ev1_reg(fd, WRITE_REG, abdir5,  1);

	ev1_reg(fd, WRITE_REG, abind0,  0);
	ev1_reg(fd, WRITE_REG, abind1,  0);
	ev1_reg(fd, WRITE_REG, abind2,  0);
	ev1_reg(fd, WRITE_REG, abind3,  0);
	ev1_reg(fd, WRITE_REG, abind4,  0x20);

	ev1_reg(fd, WRITE_REG, abind5,  3);
	ev1_reg(fd, WRITE_REG, abind6,  2);
	ev1_reg(fd, WRITE_REG, abind7,  0);
	ev1_reg(fd, WRITE_REG, abind15, 0);
	ev1_reg(fd, WRITE_REG, abind25, 0);
	ev1_reg(fd, WRITE_REG, ccind8,  0);
	ev1_reg(fd, WRITE_REG, ccind9,  127);
	ev1_reg(fd, WRITE_REG, ccind10, 4);
	ev1_reg(fd, WRITE_REG, ccind11, 0);
	ev1_reg(fd, WRITE_REG, ccind12, 240);
	ev1_reg(fd, WRITE_REG, ccind13, 0);
	ev1_reg(fd, WRITE_REG, ccind14, 130);
	ev1_reg(fd, WRITE_REG, ccind18, 0x10);
	ev1_reg(fd, WRITE_REG, ccind26, 0x10);
	ev1_reg(fd, WRITE_REG, ccind1,  10);
	ev1_reg(fd, WRITE_REG, ccind32, 2);
	ev1_reg(fd, WRITE_REG, ccind54, 0);
	ev1_reg(fd, WRITE_REG, ccind55, 0);
	ev1_reg(fd, WRITE_REG, ccind56, 0x40);
	ev1_reg(fd, WRITE_REG, ccind57, 0);
	ev1_reg(fd, WRITE_REG, ccind58, 0);
	ev1_reg(fd, WRITE_REG, ccind0,  0x80);
	ev1_reg(fd, WRITE_REG, ccdir0,  0x48);
}


static int verify_data(int fd)
{
	int line, pixel;

	ev1_reg(fd, WRITE_REG, abdir3, 0);
	ev1_reg(fd, WRITE_REG, abdir4, 0);

	ab1_dram.cmd = AB1_DRAM_READ;
	ab1_dram.buflen = PIXELS_PER_LINE;
	ab1_dram.buffer = (unsigned char *) &rcvbuf[0];

	ev1_reg(fd, WRITE_REG, abdir5, (AB1_RED | NYSC_MODE)); /* NTSC, RED Channel Read */
	ev1_reg(fd, WRITE_REG, abdir6, 0x3);
	for(line=0; line < NTSC_LINES; line++) {
		ab1_dram.ystart = line;
		ab1_dram.xstart = 0;
		if (ioctl(fd, V_DO_MISC,&ab1_dram) == -1){
                	perror("V_DO_MISC.. RED CHANNEL AB1_DRAM_READ .....\n");
                	exit(2);
		}

		for(pixel=0; pixel<NTSC_PIXELS/4; pixel++) {
			printf("RED CHANNEL line= 0x%08X pixel= 0x%x\n" ,line, *(rcvbuf + pixel));
		}
	}

	ev1_reg(fd, WRITE_REG, abdir5, (AB1_GREEN | NTSC_MODE)); /* NTSC, GREEN Channel Read */
	ev1_reg(fd, WRITE_REG, abdir6, 0x3);
	for(line=0; line < NTSC_LINES; line++) {
		ab1_dram.ystart = line;
		ab1_dram.xstart = 0;
		if (ioctl(fd, V_DO_MISC,&ab1_dram) == -1){
                	perror("V_DO_MISC.. GREEN CHANNEL AB1_DRAM_READ .....\n");
                	exit(2);
		}

		for(pixel=0; pixel<NTSC_PIXELS/4; pixel++) {
			printf("GREEN CHANNEL line= 0x%08X pixel= 0x%x\n" ,line, *(rcvbuf + pixel));
		}
	}


	ev1_reg(fd, WRITE_REG, abdir5, (AB1_BLUE | NTSC_MODE)); /* NTSC, BLUE Channel Read */
	ev1_reg(fd, WRITE_REG, abdir6, 0x3);
	for(line=0; line < NTSC_LINES; line++) {
		ab1_dram.ystart = line;
		ab1_dram.xstart = 0;
		if (ioctl(fd, V_DO_MISC,&ab1_dram) == -1){
                	perror("V_DO_MISC.. BLUE CHANNEL AB1_DRAM_READ .....\n");
                	exit(2);
		}

		for(pixel=0; pixel<NTSC_PIXELS/4; pixel++) {
			printf("BLUE CHANNEL line= 0x%08X pixel= 0x%x\n" ,line, *(rcvbuf + pixel));
		}
	}

	return(0);
}


main(int argc, char **argv)
{
	int scratch;
	int fk, av;

	if (argc != 3) {
		printf("Usage: flicker (flicker reduction)(averaging)\n");
		exit (2);
	}
	fk = atoi(argv[1]);
	av = atoi(argv[2]);

	prefposition(0,1279, 0, 1024);
        foreground();
        winopen("FLICKER TEST");
	RGBmode();
	RGBwritemask(0xFF,0xFF,0xFF);
	doublebuffer();

    	qdevice(ESCKEY);
    	qdevice(REDRAW);

	gconfig();
	cpack(0);
        clear();
	swapbuffers();
	clear();
	
	kv_fd = open_kv();
 	ev1_reg(kv_fd, READ_REG, V_CC_VERS_ID, scratch); 

        ev1_reg(kv_fd, WRITE_REG, abdir6, 0x10);       /* First Chip ID 0x10 */
        ev1_reg(kv_fd, WRITE_REG, abdir6, 0x21);       /* second Chip ID 0x21 */
        ev1_reg(kv_fd, WRITE_REG, abdir6, 0x42);       /* Third Chip ID 0x42 */
        ev1_reg(kv_fd, WRITE_REG, abdir2, 0x4);

	 pixels = 1280;	
        /* malloc pixel data buffer */
        if ((bufptr = (long *)malloc(2 * pixels * sizeof(long))) == NULL) {
            fprintf(stderr, "Can't malloc %d long.\n", pixels);
            exit(0);
	}
	
	data = 0;
	for(index=0;index<pixels; index++) {
		*(bufptr + index) = data & 0xFFFFFF;
		*(bufptr + pixels + index) = data & 0xFFFFFF;
		if ((data & 0xFF)  == 0xFF)
			data = 0;
		else
			data += 0x010101;
	}

	xll  = 0; xur = 1280;
	while (1) {
	  switch(dev = qread(&val)) {
		case ESCKEY:
				exit (0);	
				break;
		case REDRAW:
		default:
				for(line=0;line<1024;line++) {
					yll = yur = line;
					lrectwrite(xll,yll,xur,yur, (unsigned long *)(bufptr+line));
				}
				swapbuffers();
        			for(line=0;line<1024;line++) {
                			yll = yur = line;
                			lrectwrite(xll,yll,xur,yur, (unsigned long *)(bufptr+line));
        			}
				Gfx_2_Vid_A(kv_fd, fk, av) ;
 				verify_data(kv_fd);
				break;
	  }
	}
}

