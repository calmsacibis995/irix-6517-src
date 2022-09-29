#include <stdio.h>
#include <pwd.h>

#include "sub_pws.h"

main(int argc, char **argv)
{
	pthread_t 	th1,th2;

	pthread_create(&th1,
			NULL,
			(void *)sub_getpwent,
			(void *)NULL);
	pthread_create(&th2,
			NULL,
			(void *)sub_getpwent,
			(void *)NULL);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
}


