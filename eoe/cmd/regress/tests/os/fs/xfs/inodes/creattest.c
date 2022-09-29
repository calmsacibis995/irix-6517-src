/*

cc -O creattest.c -o creattest
./creattest 0 8000
./creattest 1024 8000
./creattest 2048 8000
./creattest 4096 8000
./creattest 8192 8000
./creattest 16384 8000
./creattest 65536 8000
./creattest 131072 8000

-Case Larsen
ctlarsen@lbl.gov

*/

#ifndef lint
static char vcid[] = "$Id: creattest.c,v 1.1 1994/05/18 09:14:47 clarsen\
 Exp clarsen $";
/*
 * $Log: creattest.c,v $
 * Revision 1.1  1994/08/05 01:01:12  tin
 * Initial revision
 *
 * Revision 1.1  1994/05/18  09:14:47  clarsen
 * Initial revision
 *
 */
#endif /* lint */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#define BUFLEN 8192

main(argc,argv)
     int argc;
     char *argv[];
{
    int i;
    struct timeval start, end;
    char *buf;
    int size = atoi(argv[1]);
    int n_files = atoi(argv[2]);

    if (size>0) {
	buf = malloc(BUFLEN);
    }
    system ("mkdir creat");
    gettimeofday (&start,NULL);
    for (i = 0; i < n_files; i++) {
	char a[36];
	struct stat stb;
	int fd, n;
	
	sprintf (a,"creat/%d",i);
	fd = open(a,O_RDWR|O_CREAT,0644);
	/* Slow on linux.  NOOP on other machines */
	fstat(fd,&stb);
	if (size > 0) {
	    for (n = size; n >0; n-=BUFLEN) {
		write(fd, buf,n > BUFLEN? BUFLEN : n);
	    }
	}
	close(fd);
    }
    gettimeofday(&end,NULL);
    system ("rm -rf creat");
    {
	double t;
	t = (end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec)/1000000.0;
	printf ("%d byte/files:%d files:%f files/second:%f seconds\n",
		size,n_files,n_files/t,t);
    }
    exit(0);
}


