#define MSF	"pointer()"

#include <arcs/io.h>

struct md {
	int state;
	int x,y;
	int maxx,maxy;
	signed char report[5];
};

main()
{
	struct md md;
	ULONG fd,cnt;
	char c;
	int error;

	if (Open(MSF,OpenReadOnly,&fd) != 0) {
		printf("cannot open %s\n",MSF);
		return(1);
	}

	bzero(&md,sizeof(md));
	md.report[0] = 7;
	md.maxx=1023;
	md.maxy=767;

	while (1) {
		if ((error = Read(fd,&c,1,&cnt)) != 0) {
			printf("read error %s %x\n",MSF,error);
			return(1);
		}
		if (cnt != 1) continue;

		if ((c&0xF8)==0x80) {		/* button */
			md.x += md.report[1];
			md.y -= md.report[2];
			if (md.x < 0) md.x = 0;
			if (md.x > md.maxx) md.x = md.maxx;
			if (md.y < 0) md.y = 0;
			if (md.y > md.maxy) md.y = md.maxy;
			printf("(%d,%d)",md.x,md.y);
			if (!(md.report[0] & 0x01))
				printf(" 3");
			if (!(md.report[0] & 0x02))
				printf(" 2");
			if (!(md.report[0] & 0x04))
				printf(" 1");
			printf("\n");

			/* button 1 + 2 down exits */
			if ((md.report[0]&0x06) == 0) {
				printf("done\n");
				Close(fd);
				return;
			}

			md.state = 0;
			md.report[1] = md.report[2] = 0;
		}
		md.report[md.state++] = c;
	}
}
