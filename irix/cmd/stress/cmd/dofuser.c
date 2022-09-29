#include "stdio.h"
#include "sys/types.h"
#include "sys/utssys.h"

main()
{
	char buf[24*1024];
	int np;

	for (;;) {
		np = utssys("/", F_CONTAINED, UTS_FUSERS, buf);
		if (np < 0) {
			perror("utssys");
			exit(-1);
		}
		printf("np %d\n", np);
	}
}
