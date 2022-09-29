#include "dsk.h"

creat_clo( char *argv )
{
	register int n;
	register int  j;
	char fn4[80];

	n = 10000;

	if ( *argv )
		sprintf(fn4,"%s/%s", argv, TMPFILE2);
	else
		sprintf(fn4,"%s",TMPFILE2);
	mktemp(fn4);

	while (n--)  {
		if( (j = creat(fn4,0777))==-1 || close(j)==-1) {
			fprintf(stderr, "creat/close error\n");
		}
	}
	unlink(fn4);
	return(j);
}
