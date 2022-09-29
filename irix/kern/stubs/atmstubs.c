#include <sys/types.h>
#include <sys/systm.h>
#include "sys/domain.h"
#include "sys/protosw.h"

struct  domain atmarpdomain = {
	0, "atmarp", 0, 0, 0, (struct protosw *)0, (struct protosw *)0, 0
};


new_atmarp_ioctl(int command, caddr_t data)
{
	if (command == 0) {
		return(-1);
	};

	if (data == NULL) {
		return(-1);
	};

	return(0);
}
