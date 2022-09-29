#include <stdio.h>
#include <stdlib.h>
#include "sys/syssgi.h"
#include "sys/types.h"
	
main()
{	
	if (syssgi(1005) == -1) {
		exit(-1);
	}
}
