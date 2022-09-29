/*
 * "libadm.h"
 */

#include <stdio.h>
#include <valtools.h>

extern int	ckquit;

extern void	putprmpt(FILE *, char *, char *[], char *);
extern void	puterror(FILE *, char *, char *);
extern void	puthelp(FILE *, char *, char *);
extern int	getinput(char *);
extern int	puttext(FILE *, char *, int, int);
extern void	printmenu(CKMENU *);
#ifndef __REGEXP_H__
extern char	*compile(char *, char *, char *, int);
extern int	step(char *, char *);
#endif
extern char	*devattr(char *, char *);
extern char	**getdev(char **, char **, int);
extern int	ckstr(char *, char *[], int, char *, char *, char *, char *);
extern int	ckkeywd(char *, char *[], char *, char *, char *, char *);
extern int	_getvol(char *, char *, int, char *, char *);
extern int	pkgnmchk(char *, char *, int);
extern char	*fpkgparam(FILE *, char *);
			       
				
