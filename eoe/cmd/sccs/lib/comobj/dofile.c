/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sccs:lib/comobj/dofile.c	6.7" */
#ident	"$Revision: 1.7 $"
# include	"../../hdr/defines.h"
# include	<stdio.h>
# include	<dirent.h>
# include	<unistd.h>


static int	nfiles;
char	had_dir;
char	had_standinp;

void
process_dir(dir, func)
char *dir;
int  (*func)();
{
	FILE	*fin;
	int	read_err;
	char	buf[BUFSIZ];
	char	ls[FILESIZE];
	char	str[FILESIZE];

	strcpy(ls, "ls -1 ");
	strcat(ls, dir);
	fin = popen(ls, "r");
	if (fin == NULL){
		return;
	}
	while (fgets(buf, sizeof(buf), fin) != NULL){
		buf[strlen(buf)-1] = '\0';
		sprintf(str, "%s/%s", dir, buf);
		read_err = access(str, R_OK);
		if (!read_err && sccsfile(str)){
			(*func)(str);	
			nfiles++;
		}
	}
	pclose(fin);
	return;
}

void
do_file(p,func)
register char *p;
int (*func)();
{
	extern char *Ffile;
	char str[FILESIZE];
	char ibuf[FILESIZE];
	DIR *iop;
	int	read_err;
	struct dirent64 *dirp;

	if (p[0] == '-') {
		had_standinp = 1;
		while (gets(ibuf) != NULL) {
			if (exists(ibuf) && 
                            (Statbuf.st_mode & S_IFMT) == S_IFDIR) {
				had_dir = 1;
				Ffile = ibuf;
				process_dir(ibuf, func);
			}
			else if (sccsfile(ibuf)) {
				read_err = access(ibuf, R_OK);
				if (!read_err){
					Ffile = ibuf;
					(*func)(ibuf);
					nfiles++;
				}
			}
		}
	}
	else if (exists(p) && (Statbuf.st_mode & S_IFMT) == S_IFDIR) {
		had_dir = 1;
		Ffile = p;
		process_dir(p, func);
	}
	else {
		Ffile = p;
		(*func)(p);
		nfiles++;
	}
}
