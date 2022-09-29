/*
** 1) create a deep directory structure(until getting error)
**    cd to the end, call pwd to traverse all the way
**    back up, cd back
**
** 2) create a directory, create lots of empty files(until getting error)
**    
** 3) If the target directory is a spare file system then unmount 
**    then call fsck -D, remount then repeat step 1 and 2
**
*/
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef DEBUG
#define DIRSIZE	16*1024
#define DIREXTEND	1024
#else
#define DIRSIZE 50
#define DIREXTEND	20
#endif
char *bbuf;
char sbuf[80];
int dirlevel;
char *path = "/usr/tmp/DIRCHK";
int dir_maxlen = DIRSIZE;
int createdir(char *curpwd);
void pwd(char *buf);
void cleanup_dir(void);
void deep(int ndeep);
void cleanup_file(int fnum);
int createfile(char *file);
void wide(int nwide);
void Usage(void);

extern char *sys_errlist[];
int
main(int argc, char **argv)
{
	int c, err = 0;
	int nwide = 0;
	int ndeep = 0;

	while ((c = getopt(argc, argv, "p:f:d:")) != EOF) {
		switch (c) {
		case 'd':
			ndeep = atoi(optarg);
			break;
		case 'f':
			nwide = atoi(optarg);
			break;
		case 'p':
			path = optarg;
			break;
		default:
			err++;
			break;
		}

	}
	if (err) {
		Usage();
		exit(1);
	}
	setbuf(stdout,(char *)0);
	setbuf(stderr,(char *)0);
	wide(nwide);	/* wide() goes first since deep() does execl */
	deep(ndeep);
	return 0;
}

void
Usage(void)
{
	fprintf(stderr,"dirchk [-p directory to put test files][-f # files][-d # dirs]");
}

/*
** create empty files until get error from system
*/
void
wide(int nwide)
{
	char *fname = sbuf;
	int filenum = 0;

	if (chdir(path) < 0) {
           	fprintf(stderr, "dirchk:ERROR:Failed to chdir directory \"%s\"; %s\n", path, sys_errlist[errno]);  
		exit(1);
	}
	fprintf(stdout,"Creating a wide directory at %s\n",path);
	sprintf(fname,"f%x",filenum);
	while (--nwide >= 0 && createfile(fname) == 0) {
		filenum++;
		sprintf(fname,"f%x",filenum);
		if ((filenum % 50) == 0)
			fprintf(stdout,"\rNumber of files created so far is %d",filenum);
	}
	fprintf(stdout,"\nCreated %d files\n",filenum);
	fprintf(stdout,"Remove files under %s\n",path);
	cleanup_file(filenum);
}
	
int
createfile(char *file)
{
	int fd;
#ifdef DEBUG
#define MAXFILES 20
	static int t = 0;
	
	if (t++ > MAXFILES)
		return(-1);
#endif

	if ((fd = creat(file, 0777)) <0) {	
           	fprintf(stderr, "dirchk:  can not create file \"%s\"; %s\n", file, sys_errlist[errno]);  
		return(-1);
	}

	close(fd);
	return(0);
}

void
cleanup_file(int fnum)
{
	char *fname = sbuf;

	while (fnum--) {
		sprintf(fname,"f%x",fnum);
		if (unlink(fname) <0) {
           		fprintf(stderr, "dirchk:ERROR:Failed to unlink file \"%s\"; %s\n", fname, sys_errlist[errno]);  
			fprintf(stderr,"%d files remained to be removed under %s\n",fnum, path);
			exit(1);
		}

	}
}
/*
** create a dir,
** cd into it
** and repeat until error
** do pwd to see where we are, confirm the resullt
** cd back
** 
** 
*/
void
deep(int ndeep)
{
	char *orgpwd, *curpwd;

	if (chdir(path) < 0) {
           	fprintf(stderr, "dirchk:ERROR:Failed to chdir directory \"%s\"; %s\n", path, sys_errlist[errno]);  
		exit(1);
	}
	if ((bbuf = malloc(DIRSIZE)) == NULL) {
		fprintf(stderr,"dirchk: can not malloc %d bytes; %s\n",DIRSIZE, sys_errlist[errno]);
		exit(1);
	}
	pwd(orgpwd = sbuf);
	fprintf(stdout,"Creating a deep directory at %s\n",orgpwd);
	curpwd = bbuf;
	strcpy(curpwd, orgpwd);
	while (createdir(curpwd) == 0 && --ndeep >= 0) {
		/* get bigger buffer */
		if (strlen(bbuf) >= (dir_maxlen -2)) {
			char *newbuf;
#ifdef DEBUG
			static int first = 1;

			if (first) {
				fprintf(stdout,"Overflow buffer, getting new buffer\n");
				first = 0;
			}
			else /* only does it once */
				break;	
#endif
			if ((newbuf=malloc(dir_maxlen + DIREXTEND)) == NULL) {
				fprintf(stderr,"dirchk: can not malloc %d bytes; %s\n",dir_maxlen + DIREXTEND, sys_errlist[errno]);
				exit(1);
			}
			strcpy(newbuf, bbuf);
			free(bbuf);
			bbuf = newbuf;
			curpwd = bbuf;
			dir_maxlen += DIREXTEND;
		}
	}
	fprintf(stdout,"\n%s Directory depth = %d\n",curpwd,dirlevel);
	fprintf(stdout,"Removing all test directories under %s\n",path);
	cleanup_dir();

}

#define RMDIR "/bin/rm"
/*
** cd path
** rm -r -f ./d
*/
void
cleanup_dir(void)
{
	/* cd back */
	if (chdir(path) <0) {
           	fprintf(stderr, "dirchk:ERROR:Failed to chdir directory \"%s\"; %s\n", path, sys_errlist[errno]);  
		fprintf(stderr,"Please manually remove directories under %s\n",path);
		exit(1);
	}
	execl(RMDIR, RMDIR, "-r", "-f", "./d",0);
}

#define PWD	"/bin/pwd"

void
pwd(char *buf)
{
	FILE *fp;
	char *s = buf;

	if( (fp = popen(PWD, "r")) == NULL) {
		fprintf(stderr,"dirchk:ERROR:popen of %s failed\n",PWD);
		exit(1);
	}

	while(fgets(s, BUFSIZ, fp)) 
		s = &buf[strlen(buf) -1];

	pclose(fp);
}

	
/*
** create a new dir
** update curpwd
*/	
#define LOCALDIR	"./d"
int
createdir(char *curpwd)
{
	
	if (access(".",02)) {
		fprintf(stderr, "dirchk: \"%s\": %s\n",curpwd,sys_errlist[errno]);
		exit(1);
	}
	if (setuid(getuid()) || setgid(getgid())) {
		fprintf(stderr, "dirchk:ERROR:Failed to set effective user/group ids to real user/group ids");
		exit(1);
	}
	if ((mkdir(LOCALDIR, 0777)) < 0) {
           	fprintf(stderr, "dirchk:ERROR:Failed to make directory \"%s%s\"; %s\n", curpwd,"/d", sys_errlist[errno]);  
		return(-1);
	}
	if (chdir(LOCALDIR) < 0) {
           	fprintf(stderr, "dirchk:ERROR:Failed to chdir directory \"%s%s\"; %s\n", curpwd,"/d", sys_errlist[errno]);  
		exit(1);
	}
	pwd(curpwd);
	dirlevel++;
	if ((dirlevel % 50) == 0)
		fprintf(stdout,"\rNumber of directories created so far is %d",dirlevel);
	return(0);
}
