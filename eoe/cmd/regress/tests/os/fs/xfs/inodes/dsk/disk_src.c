#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdarg.h>

#include "dsk.h"

#define STAT	0
#define CREAT	1
#define MCHOICE	2
#define MFILES	75

/*
 * dsearch exercises the directory search mechanism of unix systems.
 * it is called by the disk test program. dsearch assumes that it is 
 * invoked with its current directory the parent directory
 * of the hand created directory that is distributed with the benchmark.
 * it assumes that in this directory is a file "dirlist" that provides a list
 * of file names under the current directory, along with a list of names to
 * search for.  some of these names are to be stat'ed while some are to be
 * creat'ed
 */

void scramble(char *list[], int num);
unsigned long mrand(void);
void cl_list(char *[MCHOICE][MFILES]);
void errdump(char *str, ...);

int dsearch( char *argv )
{
	FILE	*fp;
	int		fd;
	struct stat stbuf;
	char	errbuf[80];
	int		index;
	char	*flist[MCHOICE][MFILES]; /* the list of target files */

	if (debug > 3)
		fprintf(stderr, "dsearch: argv = %s\n", argv);

/*	if (argv && chdir(argv) == -1) { */
	if (chdir("fakeh") == -1) {
		perror("dsearch");
		errdump("dsearch: directory %s is inaccessible\n", argv);
		return(-1);
	}
	if((fp = fopen("dirlist","r")) == NULL)  {
		errdump("dsearch(): file 'dirlist' is inaccessable\n");
		chdir("..");
		return(-1);
	}
	if(get_list(fp,flist) < 0)  {
		errdump("dsearch(): file 'dirlist' is corrupted\n");
 		chdir("..");
		cl_list(flist);
		return(-1);
	}
	scramble(flist[STAT],MFILES);
	scramble(flist[CREAT],MFILES);
	for(index=0; index < MFILES; index++)  {
		if(flist[STAT][index] != NULL)  {
			if (stat(flist[STAT][index],&stbuf) < 0) {
				perror("stat() in dsearch()");
				sprintf(errbuf,"dsearch(): can't stat '%s'\n",
					flist[STAT][index]);
				errdump(errbuf);
				chdir("..");
				cl_list(flist);
				return(-1);
			}
		}
		if(flist[CREAT][index] != NULL)  {
			if ((fd = creat(flist[CREAT][index],0666)) < 0) {
				perror("creat() in dsearch()");
				sprintf(errbuf,"dsearch():can't creat '%s'\n",
					flist[CREAT][index]);
				errdump(errbuf);
				chdir("..");
				cl_list(flist);
				return(-1);
			}
			close(fd);
			unlink(flist[CREAT][index]);
		}
	}
	cl_list(flist);
	chdir("..");
	return(0);
}

get_list(FILE *file, char *list[MCHOICE][MFILES])
{
#define MYBUF 160
	char	buff[MYBUF];
	int		s_index=0, c_index=0;
	int		i;
	char	*tmp, *malloc();

	/* initialize array */
	for (i=0; i<MFILES; i++)
		list[STAT][i] = list[CREAT][i] = NULL;

	while(fgets(buff,MYBUF - 1,file) != NULL)  {
	if(buff[0] != 's' && buff[0] != 'c')  {
		continue;
	}
	buff[strlen(buff)-1] = '\0';	/* eliminate trailing new line */
	if((tmp = malloc(strlen(buff)+1+6)) == NULL)  {
		cl_list(list);
		return(-1);
	}
	strcpy(tmp, buff + 2);
	switch(buff[0])  {
	case 's':
		list[STAT][s_index++] = tmp;
		break;
	case 'c':		/* unique name to CREAT; Tin Le */
		sprintf(tmp,"%s%04d",(buff+2),c_index);
		list[CREAT][c_index++] = tmp;
		break;
	default:			/* this cannot be */
		errdump("getlist(): Deadly error encountered\n");
		cl_list(list);
		return(-1);
	}
	}
	return(1);
}

#define MSCR 5

void scramble(char *list[], int num)
{
	register int	i, scount;
	register char	*tmp;
	int		rnum;

	for(scount=0; scount<MSCR; scount++)  {
		for(i=0; i<num; i++)  {
			rnum = mrand() % num;
			tmp = list[i];
			list[i] = list[rnum];
			list[rnum] = tmp;
		}
	}
}


unsigned long mrand(void)
{
	return((unsigned long)rand());
}

void cl_list(char *list[MCHOICE][MFILES])
{
	int index;

	for(index=0; index < MFILES; index++)  {
	if(list[STAT][index] != NULL)  {
		free(list[STAT][index]);
	}
	if(list[CREAT][index] != NULL)  {
		free(list[CREAT][index]);
	}
	}
}

void errdump(char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	fprintf(stderr, str, va_arg(ap, char *));
	va_end(ap);
}

disk_src( char *argv )
{
	return( dsearch("disk_src") );
}
