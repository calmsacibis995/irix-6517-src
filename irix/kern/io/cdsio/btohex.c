/* convert binary bytes on stdin to INTEL .hex format on stdout
 */

#ident "$Revision: 1.1 $"

#include <stdio.h>

static unsigned int addr = 0;

#define MAX_RCD 32
static unsigned char bbuf[MAX_RCD];
static int buflen = 0;
static char allones;

main()
{
	int c;

	while ((c = getc(stdin)) != EOF) {
		bbuf[buflen] = c;
		if (c != 0xff)
			allones = 0;
		if (++buflen >=  MAX_RCD)
			dump_hex();
	}

	if (buflen != 0)
		dump_hex();
	(void)printf(":00000001FF\r\n");

	return 0;
}


dump_hex()
{
	int i;
	unsigned int cksum;

	if (!allones) {
		(void)printf(":%02X%02X%02X00", (buflen & 0xff), 
			((addr>>8) & 0xff), (addr & 0xff));
		cksum = -(buflen + (buflen>>8) + addr + (addr>>8));
		for (i = 0; i < buflen; i++) {
			cksum -= bbuf[i];
			(void)printf("%02X", bbuf[i]);
		}
		(void)printf("%02X\r\n", cksum & 0xff);
	}
	addr += buflen;
	buflen = 0;
	allones = 1;
}
