/* wordexp.c
 *
 *      perform word expansion
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */


#ident "$Revision: 1.3 $"

#include <wordexp.h>
#ifdef __STDC__
	#pragma weak wordexp = _wordexp
	#pragma weak wordfree = _wordfree
#endif

/*
 * wordexp(3) -- as defined in POSIX 1003.2.
 */

#include "synonyms.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stropts.h>
#include <limits.h>
#include <sys/wait.h>

typedef char Char;

#define RD_END		0
#define WR_END		1
#define SHELL_PATH	"/sbin/sh"
#define SHELL   	"sh"
/* 
 * Following shell return values correspond with the values defined
 * in defs.h for the ksh.
 */
#define SH_ERROR	1
#define SH_SYNBAD	2
#define SH_CMDBAD	124
#define SH_CTXBAD	125

#define WRDE_ABORTED	1		


static
int	wrde_expand(wordexp_t *pwordexp, char *pipebuf, int total_cnt)
{
	register char **pathv;
	register size_t i;
	int x,len,nargs;
	size_t newsize;
	Char *copy,*pb;

	pb = pipebuf;
	/* Scan for number of args - 1 null terminates each arg */
	for(nargs=0,x=0;x<total_cnt;x++)
	{
		if(*pb++ == NULL)
			++nargs;
	}
	newsize = sizeof(*pathv) * (nargs + pwordexp->we_wordc + pwordexp->we_offs + 1);
	pathv = pwordexp->we_wordv ? 
		    realloc((void *)pwordexp->we_wordv, newsize) :
		    malloc(newsize);
	if (pathv == NULL)
	{
		free(pipebuf);
		return(WRDE_NOSPACE);
	}

	if (pwordexp->we_wordv == NULL && pwordexp->we_offs > 0) 
	{
		/* first time around -- clear initial we_offs items */
		for (i = 0;i < pwordexp->we_offs; ++i)
			*(pathv+i) = NULL;
	}
	pwordexp->we_wordv = pathv;

	pb = pipebuf;
	while(total_cnt)
	{
		len = (int) strlen(pb)+1;	/* plus null byte */
		if((copy=malloc(len)) == NULL)
		{
			free(pipebuf);
			return(WRDE_NOSPACE);
		}
		strcpy(copy,pb);
		pathv[pwordexp->we_offs + pwordexp->we_wordc++] = copy;
		total_cnt -= len;
		pb += len;

	}
	free(pipebuf);
	pathv[pwordexp->we_offs + pwordexp->we_wordc] = NULL;
	return(0);
}
/* Free allocated data belonging to a wordexp_t structure. */
void
wordfree(wordexp_t *pwordexp)
{
	register size_t i;
	register Char **pp;

	if(pwordexp->we_wordv != NULL)
	{
		pp = pwordexp->we_wordv + pwordexp->we_offs;
		for (i = pwordexp->we_wordc; i--; ++pp)
			if (*pp)
				free(*pp);
		free(pwordexp->we_wordv);
	}
}
static void
wrde_closepipe(int *p)
{
	close(p[RD_END]);
	close(p[WR_END]);
}

int
wordexp(const char *words, wordexp_t *pwordexp, int flags)
{
	int ppid;
	int status;
	int outpipe[2];
	int errpipe[2];
	int cur_cnt,total_cnt;
	Char *pipebuf = NULL;
	int msgcnt=0;
	extern char     **environ;

	if(flags & WRDE_REUSE)
		wordfree(pwordexp);

	if(pipe(outpipe) != 0 || pipe(errpipe) != 0)
	{
		if(flags & WRDE_SHOWERR)
			perror("Wordexp: Cannot creat pipe");
		return(WRDE_ABORTED);
	}
	if((ppid=fork()) == 0 ) /* Child */
	{
		Char shflags[6];
		Char shcmd[LINE_MAX];
		close(1);
		dup(outpipe[WR_END]);
		close(2);
		dup(errpipe[WR_END]);

		strcpy(shflags,"-wc");
		if(flags & WRDE_NOCMD)
			strcat(shflags,"P");
		if(flags & WRDE_UNDEF)
			strcat(shflags,"u");
		sprintf(shcmd,"echo %s",words);

		execle(SHELL_PATH, SHELL, shflags, shcmd, (Char *)0, environ);

		if(flags & WRDE_SHOWERR)
			perror("Wordexp: Cannot exec");
		return(WRDE_ABORTED);
	}
	else if(ppid == -1)
	{
		if(flags & WRDE_SHOWERR)
			perror("Wordexp: Cannot fork");
		return(WRDE_ABORTED);
	}
	else	/* Parent */
	{
		while (waitpid(ppid, &status, 0) < 0 && errno == EINTR);

		/* Determine if there is standard error data to read */
		if(flags & WRDE_SHOWERR)
		{
			while((msgcnt=ioctl(errpipe[RD_END],I_NREAD,&cur_cnt)) > 0 && cur_cnt)
			{
				pipebuf = (Char *)malloc((size_t)cur_cnt);
				if(pipebuf == NULL)
				{
					wrde_closepipe(errpipe);
					return(WRDE_NOSPACE);
				}
				if(read(errpipe[RD_END],pipebuf,cur_cnt) != cur_cnt)
				{
					if(flags & WRDE_SHOWERR)
						perror("Wordexp: Cannot read from pipe");
					free(pipebuf);
					wrde_closepipe(errpipe);
					return(WRDE_ABORTED);
				}
				if(write(2,pipebuf,cur_cnt) == -1)
				{
					if(flags & WRDE_SHOWERR)
						perror("Wordexp: Cannot write to stderr");
					free(pipebuf);
					wrde_closepipe(errpipe);
					return(WRDE_ABORTED);
				}
				free(pipebuf);
			}
			if(msgcnt == -1)
			{
				if(flags & WRDE_SHOWERR)
					perror("Wordexp: ioctl INREAD");
				wrde_closepipe(errpipe);
				return(WRDE_ABORTED);
			}
		}
		wrde_closepipe(errpipe);

		switch(WEXITSTATUS(status))
		{
			case 0:	break;
			case SH_ERROR:	
				wrde_closepipe(outpipe);
				if(flags & WRDE_UNDEF)
					return(WRDE_BADVAL);
				return(WRDE_ABORTED);
			case SH_SYNBAD:
				wrde_closepipe(outpipe);
				return(WRDE_SYNTAX);
			case SH_CMDBAD:
				wrde_closepipe(outpipe);
				return(WRDE_CMDSUB);
			case SH_CTXBAD:
				wrde_closepipe(outpipe);
				return(WRDE_BADCHAR);
			default:
				wrde_closepipe(outpipe);
				return(WRDE_ABORTED);
		}
		pipebuf = NULL;
		cur_cnt = total_cnt = 0;

		while((msgcnt=ioctl(outpipe[RD_END],I_NREAD,&cur_cnt)) > 0 && cur_cnt)
		{
			pipebuf = pipebuf ?
				  realloc(pipebuf,total_cnt+cur_cnt) :
				  malloc(cur_cnt);
			if(pipebuf == NULL)
			{
				wrde_closepipe(outpipe);
				return(WRDE_NOSPACE);
			}
			if(read(outpipe[RD_END],pipebuf+total_cnt,cur_cnt) != cur_cnt)
			{
				if(flags & WRDE_SHOWERR)
					perror("Wordexp: Cannot read from pipe");
				free(pipebuf);
				wrde_closepipe(outpipe);
				return(WRDE_ABORTED);
			}
			total_cnt += cur_cnt;
		}
		wrde_closepipe(outpipe);
		if(msgcnt == -1)
		{
			if(flags & WRDE_SHOWERR)
				perror("Wordexp: ioctl INREAD");
			if(pipebuf)
				free(pipebuf);
			return(WRDE_ABORTED);
		}
		if(!(flags & WRDE_APPEND)) {
			pwordexp->we_wordc = 0;
			pwordexp->we_wordv = NULL;
			if (!(flags & WRDE_DOOFFS))
				pwordexp->we_offs = 0;
		}
		return(wrde_expand(pwordexp,pipebuf,total_cnt));
	}
}
