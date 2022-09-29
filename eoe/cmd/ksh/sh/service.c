/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:sh/service.c	1.4.4.1"

/*
 * UNIX shell
 *
 * S. R. Bourne
 * Rewritten by David Korn
 * AT&T Bell Laboratories
 *
 */

#include	<errno.h>
#include	"defs.h"
#include	"jobs.h"
#include	"sym.h"
#include	"builtins.h"
#include	"history.h"

#define MAXDEPTH (32*sizeof(int))	/* maximum levels of recursion */

extern int		gscan_some();
#ifdef SUID_EXEC
    extern char		*utos();
#endif /* SUID_EXEC */

static char		*prune();
static char		*execs();
static int		canexecute();
#ifndef VFORK
    static void		 exscript();
#endif /* VFORK */
#ifdef VPIX
    static int		suffix_offset;
    extern char		*suffix_list[];
    static int		 dospath();
#endif /* VPIX */

static struct namnod	*tracknod;
static const char	*xecmsg;
static int		 xecrval;
static char		**xecenv;
static int		pruned;

/* make sure PWD is set up correctly */

/*
 * Return the present working directory
 * Invokes /bin/pwd if flag==0 and if necessary
 * Sets the PWD variable to this value
 */

char *path_pwd(flag)
int flag;
{
	register char *cp;
	register char *dfault = (char*)e_dot;
	register int count = 0;
	extern MSG	e_crondir;
	if(sh.pwd)
		return(sh.pwd);
	while(1) 
	{
		/* try from lowest to highest */
		switch(count++)
		{
			case 0:
				cp = nam_strval(PWDNOD);
				break;
			case 1:
				cp = nam_strval(HOME);
				break;
			case 2:
				cp = "/";
				break;
			case 3:
				cp = (char*)e_crondir;
				if(flag) /* skip next case when non-zero flag */
					++count;
				break;
			case 4:
			{
#ifdef apollo
				char buff[PATH_MAX+1];
				extern char *getwd();
				cp=getwd(buff+1);
#else
				/* even restricted shells can pwd */
				optflag savflags = opt_flags;
				unsigned savestates = st.states;
				off_option(RSHFLG);
				st.states &= ~(EXECPR|READPR);
				sh_eval((char*)e_setpwd);
				opt_flags = savflags;
				st.states = savestates;
#endif	/* apollo */
				cp = nam_strval(PWDNOD);
				if(*cp=='/')
					dfault = cp;
				break;
			}
			case 5:
				return(dfault);
		}
		if(cp && *cp=='/' && test_inode(cp,e_dot))
			break;
	}
	if(count>1)
		nam_fputval(PWDNOD,cp);
	nam_ontype(PWDNOD,N_FREE|N_EXPORT);
	sh.pwd = PWDNOD->value.namval.cp;
	return(cp);
}

/*
 * given <s> return a colon separated list of directories to search on the stack
 * This routine adds names to the tracked alias list, if possible, and returns
 * a reduced path string for tracked aliases
 */

char *path_get(s)
const char *s;
/*@
 	assume s!=NULL;
	return path satisfying path!=NULL;
@*/
{
	register char *path;
	register char *sp = sh.lastpath;
	if(strchr(s,'/'))
		return(NULLSTR);
	path = nam_fstrval(PATHNOD);
	if(path==NULL)
		path = (char*)e_defpath;
	path = stakcopy(path);
	if(sp || ((tracknod=nam_search(s,sh.track_tree,0)) &&
		nam_istype(tracknod,NO_ALIAS)==0 &&
		(sp=nam_strval(tracknod))))
	{
		path = prune(path,sp);
		pruned++;
	}
	return(path);
}

int	path_open(name,path)
register const char *name;
register char *path;
/*@
	assume name!=NULL;
 @*/
{
	register int n;
	struct stat statb;
	if(strchr(name,'/'))
	{
		if(is_option(RSHFLG))
			sh_fail(name, gettxt(_SGI_DMMX_e_restricted,e_restricted),ERROR);
	}
	else
	{
		if(path==NULL)
			path = (char*)e_defpath;
		path = stakcopy(path);
	}
	do
	{
		path=path_join(path,name);
		if((n = open(path_relative(stakptr(OPATH)),O_RDONLY)) >= 0)
		{
			if(fstat(n,&statb)<0 || S_ISDIR(statb.st_mode))
			{
				close(n);
				n = -1;
			}
		}
	}
	while( n<0 && path);

	/* Insure n is greater than ERRIO */
	if( n >= 0 && n <= ERRIO )
	{
		int newfd,save1,save2;
		if((newfd=getdtablehi()) > ERRIO)
		{
			n = io_renumber(n,newfd);
		}
		else
		{
			if((save1 = dup(n)) <= ERRIO) /* Could be 1 or 2 */
			{
				if(save1 == -1)	/* Bad dup */
				{
					close(n);
					n = -1;
				}
				else if((save2 = dup(n)) == ERRIO) /* Could be 2 */
				{
					if(save2 == -1)	/* Bad dup */
					{
						close(n);
						n = -1;
					}
					else
					{
						n = io_renumber(n,getdtablehi());
						close(save1);
						close(save2);
					}
				}
				else	
				{
					n = io_renumber(n,save2);
					close(save1);
				}
			}
			else	n = io_renumber(n,save1);
		}
	}
	return(n);
}

#ifdef VPIX
/*
 * This routine returns 1 if first directory in path is also in
 * the DOSPATH variable, 0 otherwise
 */

static int dospath(path)
char *path;
{
	register char *dp = nam_fstrval(DOSPATHNOD);
	register char *sp = path;
	register int c;
	int match = 1;
	int pwd=0;	/* set for in preset working directory */
	if(dp==0 || *sp==0)
		return(0);
	if(sp==0)
		return(0);
	if(*sp==':')
	{
		sp = path = path_pwd(1);
		pwd++;
	}
	if(pwd && *dp==':')
		return(1);
	while(1)
	{
		if((c= *dp++)==0 || c == ':')
		{
			if(match==1 && (*sp==0 || *sp==':'))
				return(1);
			if(c==0)
				return(0);
			if(pwd && (*dp==':' || *dp==0))
				return(1);
			match = 1;
			sp = path;
		}
		else if(match)
		{
			if(*sp++ != c)
				match = 0;
		}
	}
	/* NOTREACHED */
}
#endif /* VPIX */

/*
 *  set tracked alias node <np> to value <sp>
 */

void path_alias(np,sp)
register struct namnod *np;
register char *sp;
/*@
	assume np!=NULL;
@*/
{
	if(sp==NIL)
		nam_free(np);
	else
	{
		register char *vp = np->value.namval.cp;
		register int n = 1;
		register int nofree = nam_istype(np,N_FREE);
		nam_offtype(np,~NO_PRINT);
		if(vp==0 || (n=strcmp(sp,vp))!=0)
			nam_putval(np,sp);
		nam_typeset(np,T_FLAG|N_EXPORT);
		if(nofree && n==0)
			nam_ontype(np,N_FREE);
	}
}


/*
 * given a pathname return the base name
 */

char	*path_basename(name)
register const char *name;
/*@
	assume name!=NULL;
	return x satisfying x>=name && *x!='/';
 @*/
{
	register const char *start = name;
	while (*name)
		if ((*name++ == '/') && *name)	/* don't trim trailing / */
			start = name;
	return ((char*)start);
}

/*
 * do a path search and track alias if requested
 * if flag is 0, or if name not found, then try autoloading function
 * if flag==2, returns 1 if name found on FPATH
 * returns 1, if function was autoloaded.
 */

int	path_search(name,flag)
register const char *name;
/*@
	assume name!=NULL;
	assume flag==0 || flag==1 || flag==2;
@*/
{
	register struct namnod *np;
	register int fno;
	if(flag)
	{
		/* if not found on pruned path, rehash and try again */
		while((sh.lastpath=path_absolute(name))==0 && pruned)
			nam_ontype(tracknod,NO_ALIAS);
	}
	if(flag==0 || sh.lastpath==0)
	{
		register char *path;
		int savestates;
		path = nam_fstrval(FPATHNOD);
		if(path && (fno=path_open(name,path))>=0)
		{
			if(flag==2)
			{
				close(fno);
				return(1);
			}
			sh.un.fd = fno;
			st.exec_flag--;
			savestates = st.states;
			st.states |= NOLOG;
			sh_eval(NIL);
			st.states = savestates;
			st.exec_flag++;
			if((np=nam_search(name,sh.fun_tree,0))&&np->value.namval.ip)
				return(1);
		}
		return(0);
	}
	else
	{
		if((np=tracknod) || (is_option(HASHALL) &&
			(np=nam_search(name,sh.track_tree,N_ADD)))
		  )
			path_alias(np,sh.lastpath);
	}
	return(0);
}


/*
 * do a path search and find the full pathname of file name
 */

char	*path_absolute(name)
register const char *name; 
/*@
	assume name!=NULL;
	return x satisfying x && *x=='/';
@*/
{
	register int	f;
	register char *path;
#ifdef VPIX
	char **suffix = 0;
	char *top;
#endif /* VPIX */
	pruned = 0;
	path = path_get(name);
	do
	{
		if(sh.trapnote&SIGSET)
			sh_exit(SIGFAIL);
#ifdef VPIX
		if(suffix==0)
		{
			if(dospath(path))
				suffix = suffix_list;
			path=path_join(path,name);
			if(suffix)
				top = stakptr(suffix_offset);
		}
		if(suffix)
		{
			sh_copy(*suffix,top);
			if(**suffix==0)
				suffix = 0;
			else
				suffix++;
		}
#else
		path=path_join(path,name);
#endif /* VPIX */
		f = canexecute(stakptr(OPATH));
	}
#ifdef VPIX
	while(f<0 && (path||suffix));
#else
	while(f<0 && path);
#endif /* VPIX */
	if(f<0)
		return(0);
	/* check for relative pathname */
	if(*stakptr(OPATH)!='/')
		path_join(path_pwd(1),(char*)stakfreeze(1)+OPATH);
	return((char*)stakfreeze(1)+OPATH);
}

/*
 * returns 0 if path can execute
 * sets xecmsg to e_exec if file is found but can't be executable
 */
#undef S_IXALL
#ifdef S_IXUSR
#   define S_IXALL	(S_IXUSR|S_IXGRP|S_IXOTH)
#else
#   ifdef S_IEXEC
#	define S_IXALL	(S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6))
#   else
#	define S_IXALL	0111
#   endif /*S_EXEC */
#endif /* S_IXUSR */

static int canexecute(path)
register char *path;
/*@
	assume path!=NULL;
@*/
{
	struct stat statb;
	path = path_relative(path);
	if(stat(path,&statb) < 0)
	{
		if(errno!=ENOENT) {
			xecmsg = gettxt(_SGI_DMMX_e_exec,e_exec);
			xecrval = EXEBAD;
		}
		return(-1);
	}
	xecmsg = gettxt(_SGI_DMMX_e_exec,e_exec);
	xecrval = EXEBAD;
	if(!S_ISDIR(statb.st_mode))
	{
		if((statb.st_mode&S_IXALL)==S_IXALL)
			return(0);
 		return(sh_access(path,X_OK));
	}
	return(-1);
}

#ifndef INT16
/*
 * Return path relative to present working directory
 */

char *path_relative(file)
register const char *file;
/*@
	assume file!=NULL;
	return x satisfying x!=NULL;
@*/
{
	register char *pwd;
	register char *fp = (char*)file;

	/* can't relpath when sh.pwd not set */
	if(!(pwd=sh.pwd))
		return(fp);
	while(*pwd==*fp)
	{
		if(*pwd++==0)
			return((char*)e_dot);
		fp++;
	}
	if(*pwd==0 && *fp == '/')
	{
		while(*++fp=='/');
		if(*fp)
			return(fp);
		return((char*)e_dot);
	}
	return((char*)file);
}
#endif /* INT16 */

char *path_join(path,name)
register char *path;
const char *name;
/*@
	assume path!=NULL;
	assume name!=NULL;
@*/
{
	/* leaves result on top of stack */
	register char *scanp = path;
	register int c;
	stakseek(OPATH);
	/* eliminate . and ./ */
	if(*scanp=='.')
	{
		if((c= *++scanp)==0 || c==':')
			path = scanp;
		else if(c=='/')
			path = ++scanp;
		else
			scanp--;
	}
	while((c = *scanp) && c!=':')
	{
		stakputc(c);
		scanp++;
	}
	if(scanp!=path)
	{
		if(scanp[-1]!='/')
			stakputc('/');
		/* position past ":" unless a trailing colon after pathname */
		if(*scanp && *++scanp==0)
			scanp--;
	}
	else
		while(*scanp == ':')
			scanp++;
	path=(*scanp ? scanp : 0);
	stakputs(name);
#ifdef VPIX
	/* make sure that there is room for suffixes */
	suffix_offset = staktell();
	stakputs(*suffix_list);
	*stakptr(suffix_offset) = 0;
#endif /*VPIX */
	return(path);
}


void	path_exec(at,local)
char *	at[];
struct argnod *local;		/* local environment modification */
/*@
	assume at!=NULL && *at!=NULL;
@*/
{
	register const char *path = e_nullstr;
	register char **t = at;
	xecmsg=gettxt(_SGI_DMMX_e_found,e_found);
	xecrval = FNDBAD;
#ifdef VFORK
	if(local)
		nam_scope(local);
	xecenv=env_gen();
#else
	env_setlist(local,N_EXPORT);
	xecenv=env_gen();
#endif	/* VFORK */
	if(strchr(t[0],'/'))
	{
		/* name containing / not allowed for restricted shell */
		if(is_option(RSHFLG))
			sh_fail(t[0],gettxt(_SGI_DMMX_e_restricted,e_restricted),ERROR);
	}
	else
		path=path_get(*t);
#ifdef VFORK
	if(local)
		nam_unscope();
#endif	/* VFORK */
	/* leave room for inserting _= pathname in environment */
	xecenv--;
	while(path=execs(path,t));
	sh_fail(*t,xecmsg,xecrval);
}

/*
 * This routine constructs a short path consisting of all
 * Relative directories up to the directory of fullname <name>
 */
static char *prune(path,fullname)
register char *path;
const char *fullname;
/*@
	assume path!=NULL;
	return x satisfying x!=NULL && strlen(x)<=strlen(in path);
@*/
{
	register char *p = path;
	register char *s;
	int n = 1; 
	const char *base;
	char *inpath = path;
	if(fullname==NULL  || *fullname != '/' || *path==0)
		return(path);
	base = path_basename(fullname);
	do
	{
		/* a null path means current directory */
		if(*path == ':')
		{
			*p++ = ':';
			path++;
			continue;
		}
		s = path;
		path=path_join(path,base);
		if(*s != '/' || (n=strcmp(stakptr(OPATH),fullname))==0)
		{
			/* position p past end of path */
			while(*s && *s!=':')
				*p++ = *s++;
			if(n==0)
			{
				*p = 0;
				return(inpath);
			}
			*p++ = ':';
		}
	}
	while(path);
	/* if there is no match just return path */
	path = nam_fstrval(PATHNOD);
	if(path==NULL)
		path = (char*)e_defpath;
	strcpy(inpath,path);
	return(inpath);
}

#ifdef XENIX
/*
 *  This code takes care of a bug in the XENIX exec routine
 *  Contributed by Pat Wood
 */
static ex_xenix(file)
char *file;
{
	struct stat stats;
	register int fd;
	unsigned short magic;
	if((fd = open(file,O_RDONLY)) == -1) /* can't read, so can't be shell prog */
		return(1);
	read(fd, &magic, sizeof(magic));
	if(magic == 01006) /* magic for xenix executable */
	{
		close(fd);
		return(1);
	}
	fstat(fd, &stats);
	close(fd);
	errno = ENOEXEC;
	if(!geteuid())
	{
		if(!(stats.st_mode & 0111))
			errno = EACCES;
		return(0);
	}
	if((geteuid() == stats.st_uid))
	{
		if(!(stats.st_mode & 0100))
			errno = EACCES;
		return(0);
	}
	if((getegid() == stats.st_gid))
	{
		if(!(stats.st_mode & 0010))
			errno = EACCES;
		return(0);
	}
	if(!(stats.st_mode & 0001))
		errno = EACCES;
	return(0);
}
#endif	/* XENIX */

static char *execs(ap,t)
char *	ap;
register char **t;
/*@
	assume ap!=NULL;
	assume t!=NULL && *t!=NULL;
@*/
{
	register char *p, *prefix;
	prefix=path_join(ap,t[0]);
	xecenv[0] =  stakptr(0);
	*stakptr(0) = '_';
	*stakptr(1) = '=';
	p=stakptr(OPATH);
	p_flush();
#ifdef VPIX
	if(dospath(ap))
	{
		char **suffix;
		char *savet = t[0];
		t[0] = p;
		t[-2] = e_vpix+1;
		t[-1] = "-c";
		suffix = suffix_list;
		while(**suffix)
		{
			char *vp;
			sh_copy(*suffix++,stakptr(suffix_offset));
			if(canexecute(p)>=0)
			{
				stakfreeze(1);
				if(p=nam_fstrval(VPIXNOD))
					stakputs(p);
				else
					stakputs(e_vpixdir);
				stakputs(e_vpix);
				execve(stakptr(0), &t[-2] ,xecenv);
				switch(errno)
				{
					case ENOENT:
						sh_fail(vp,gettxt(_SGI_DMMX_e_found,e_found),FNDBAD);
					default:
						sh_fail(vp,gettxt(_SGI_DMMX_e_exec,e_exec),EXEBAD);
				}
			}
		}
		t[0] = savet;
		*stakptr(suffix_offset) = 0;
	}
#endif /* VPIX */
	if(sh.trapnote&SIGSET)
		sh_exit(SIGFAIL);
/* 	p = path_relative(p); Bug #594441 */
#ifdef XENIX
	if(ex_xenix(p))
#endif	/* XENIX */
	execve(p, &t[0] ,xecenv);
	switch(errno)
	{
#ifdef apollo
		/* 
  		 * On apollo's execve will fail with eacces when
		 * file has execute but not read permissions. So,
		 * for now we will pretend that EACCES and ENOEXEC
 		 * mean the same thing.
 		 */
		case EACCES:
#endif /* apollo */
		case ENOEXEC:
#ifdef VFORK
		{
			/* this code handles the !# interpreter name convention */
			char iname[PATH_MAX+1];
#ifdef SUID_EXEC
			/* check if file cannot open for read or script is setuid/setgid  */
			static char name[] = "/tmp/euidXXXXXXXXXXX";
			register int n;
			register uid_t euserid;
			struct stat statb;
			if((n=open(p,O_RDONLY)) >= 0)
			{
				if(fstat(n,&statb)==0)
				{
					if((statb.st_mode&(S_ISUID|S_ISGID))==0)
						goto openok;
				}
				close(n);
			}
			if((euserid=geteuid()) != sh.userid)
			{
				strncpy(name+9,utos((long)getpid(),10),sizeof(name)-10);
				/* create a suid open file with owner equal effective uid */
				if((n=creat(name,04100)) < 0)
					goto fail;
				unlink(name);
				/* make sure that file has right owner */
				if(fstat(n,&statb)<0 || statb.st_uid != euserid)
					goto fail;
				if(n!=10)
				{
					close(10);
					fcntl(n, F_DUPFD,10);
					close(n);
				}
			}
			*--t = p;
			execve(e_suidexec,t,xecenv);
	fail:
			sh_fail(p, gettxt(_SGI_DMMX_e_open,e_open),ERROR);
	openok:
			close(n);
#endif /*SUID_EXEC */
			/* get name returns the interpreter name */
			if(get_shell(p, iname)<0)
				sh_fail(p, gettxt(_SGI_DMMX_e_exec,e_exec),EXEBAD);
			t--;
			t[0] = iname;
			execve(iname, t, xecenv);
			if(sh_access(iname,F_OK)==0) {
				xecmsg=gettxt(_SGI_DMMX_e_exec,e_exec);
				xecrval = EXEBAD;
			}
			sh_fail(iname, xecmsg, xecrval);
		}
#else
			exscript(p,t);
#endif	/* VFORK */
#ifdef ENAMETOOLONG
		case ENAMETOOLONG:
			xecmsg = gettxt(_SGI_DMMX_e_longname,e_longname);
			xecrval = ERROR;
			return(prefix);
#endif /* ENAMETOOLONG */
		case ENOMEM:
			sh_fail(p,gettxt(_SGI_DMMX_e_toobig,e_toobig),ERROR);

		case E2BIG:
			sh_fail(p,gettxt(_SGI_DMMX_e_arglist,e_arglist),ERROR);

		case ETXTBSY:
			sh_fail(p,gettxt(_SGI_DMMX_e_txtbsy,e_txtbsy),ERROR);
#ifdef ELIBACC
		case ELIBACC:
			sh_fail(p, gettxt(_SGI_DMMX_e_libacc,e_libacc),ERROR);

		case ELIBBAD:
			sh_fail(p, gettxt(_SGI_DMMX_e_libbad,e_libbad),ERROR);

		case ELIBSCN:
			sh_fail(p, gettxt(_SGI_DMMX_e_libscn,e_libscn),ERROR);

		case ELIBMAX:
			sh_fail(p, gettxt(_SGI_DMMX_e_libmax,e_libmax),ERROR);
#endif /* ELIBACC */

#ifdef sgi
		/*
		 * We recognize the magic number, but can't exec it (e.g.
		 * MIPS II on a R3000).
		 */
		case EBADRQC:
			sh_fail(p, gettxt(_SGI_DMMX_e_badmagic,e_badmagic),ERROR);
			break;
#endif /* sgi */

		default:
			if(sh_access(p,F_OK)==0) {
				xecmsg=gettxt(_SGI_DMMX_e_exec,e_exec);
				xecrval = EXEBAD;
			}
		case ENOENT:
			return(prefix);
	}
	/*NOTREACHED*/
}

/*
 * File is executable but not machine code.
 * Assume file is a Shell script and execute it.
 */


static void exscript(p,t)
register char *p;
register char *t[];
/*@
	assume p!=NULL;
	assume t!=NULL && *t!=NULL;
@*/
{
#ifdef _OPTIM_
	st.flags.i[_LOW_] = 0;
	st.flags.i[_HIGH_] &= (HASHALL|EMACS|GMACS|VIRAW|EDITVI)>>16;
#else
	off_option(~(HASHALL|EMACS|GMACS|VIRAW|EDITVI));
#endif /* _OPTIM_ */
	sh.comdiv=0;
	sh.bckpid = 0;
	st.ioset=0;
	/* clean up any cooperating processes */
	if(sh.cpipe[INPIPE]>0)
		io_pclose(sh.cpipe);
	if(sh.cpid)
		io_fclose(COTPIPE);
	arg_clear(); /* remove for loop junk */
	io_clear((struct fileblk*)0); /* remove open files */
	job_clear();
	if(input>0 && input!=F_STRING)
		io_fclose(input);
	st.states = 0;
	p_flush();
	st.standout= 1;
#ifdef SUID_EXEC
	/* check if file cannot open for read or script is setuid/setgid  */
	{
		static char name[] = "/tmp/euidXXXXXXXXXX";
		register int n;
		register uid_t euserid;
		char *savet;
		struct stat statb;
		if((n=open(p,O_RDONLY)) >= 0)
		{
			if(fstat(n,&statb)==0)
			{
				if((statb.st_mode&(S_ISUID|S_ISGID))==0)
					goto openok;
			}
			close(n);
		}
		else if((euserid=geteuid()) != sh.userid)
		{
			strncpy(name+9,utos((long)getpid(),10),sizeof(name)-10);
			/* create a suid open file with owner equal effective uid */
			if((n=creat(name,04100)) < 0)
				goto fail;
			unlink(name);
			/* make sure that file has right owner */
			if(fstat(n,&statb)<0 || statb.st_uid != euserid)
				goto fail;
			if(n!=10)
			{
				close(10);
				fcntl(n, F_DUPFD, 10);
				close(n);
			}
		}
		else goto fail;
		savet = *--t;
		*t = p;
		execve(e_suidexec,t,xecenv);
	fail:
		/*
		 *  The following code is just for compatibility
		 *  It should be replaced with the line sh_fail(p,e_exec);
		 */
		if((n=open(p,O_RDONLY)) < 0)
			sh_fail(p, gettxt(_SGI_DMMX_e_open,e_open),ERROR);
		*t++ = savet;
		close(10);

	openok:
		input = n;
	}
#else
	input = io_fopen(p);
#endif /* SUID_EXEC */
	hist_close();
#ifdef ACCT
	preacct(p) ;  /* reset accounting */
#endif	/* ACCT */
	/* remove locals */
	gscan_some(env_nolocal,sh.var_tree,N_EXPORT,0);	/* local variables*/
	gscan_some(env_nolocal,sh.alias_tree,N_EXPORT,0);/* local aliases*/
	gscan_some(env_nolocal,sh.fun_tree,N_EXPORT,0);	/* local functions*/
	/* set up new args */
	arg_set(t);
	nam_ontype(L_ARGNOD, N_INDIRECT);
	nam_offtype(SHELLNOD,~N_RESTRICT);
	nam_offtype(PATHNOD,~N_RESTRICT);
	sh.lastarg = sh_heap(p);
	/* save name of calling command */
	sh.readscript = st.cmdadr;
	st.cmdadr = sh_heap(t[0]);
	LONGJMP(sh.subshell,1);
}

/*
 * The following routine is used to execute shell functions and command subs
 * when com!=NULL $* is saved and restored
 */

void sh_funct(t,com,execflg,envlist)
union anynode *t;
register char *com[];
register int execflg;
struct argnod *envlist;
/*@
	assume t!=NULL;
@*/
{
	/* execute user defined function */
	register char *trap;
	jmp_buf retbuf;
	jmp_buf *savreturn = sh.freturn;
	struct dolnod	*argsav=0;
	int mode;
	struct dolnod *savargfor;
	struct fileblk *savstandin = st.standin;
	struct sh_scoped savst;
	struct sh_optget Optget;
	savst = st;
	sv_optget(&Optget);
	st.loopcnt = 0;
	if(com)
	{
		nam_scope(envlist);
		if(execflg&EXECPR)
			on_option(EXECPR);
		else
			off_option(EXECPR);
		execflg &= ~EXECPR;
		/* Reset st.cmdadr to function name */
		if(!xpg_compliant())
			st.cmdadr = com[0];
		sig_funset(0);
		argsav = arg_new(com,&savargfor);
	}
	sh.freturn = (jmp_buf*)retbuf;
	mode = SETJMP(retbuf);
	if(mode==0)
	{
		st.states |= FUNCTION;
		if(st.fn_depth++ > MAXDEPTH)
			LONGJMP(*sh.freturn,3);
		else
			sh_exec(t,execflg);
	}
	if(--st.fn_depth==1 && mode==3)
		sh_fail(com[0],gettxt(_SGI_DMMX_e_recursive,e_recursive),ERROR);
	sh.freturn = savreturn;
	if(com)
	{
		nam_unscope();
		arg_reset(argsav,savargfor);
		trap = st.trapcom[0];
		st.trapcom[0] = 0;
		sig_funset(1);
	}
	else
	{
		/* remember signals that occur for processing later */
		savst.trapflg[SIGINT] = st.trapflg[SIGINT];
		savst.trapflg[SIGTERM] = st.trapflg[SIGTERM];
		savst.trapflg[SIGHUP] = st.trapflg[SIGHUP];
	}
	io_clear(savstandin);
	st = savst;
	rs_optget(&Optget);
	if(com)
	{
		if(sh.exitval > SIGFAIL)
			sh_fault(sh.exitval-SIGFAIL);
		if(trap)
		{
			if(xpg_compliant()){
				int savexit = sh.exitval;
				sh_eval(trap);
				sh.exitval = savexit;
				free(trap);
			}
			else {
				sig_clear(0);
				st.trapcom[0] = sh_heap(trap);
				sig_ontrap(0);
			}
		}
	}
	if(mode == 2) {	/* for return call */
		if(!xpg_compliant())
			st.states &= ~ERRFLG;
	}
	if(mode > 2)
	{
		/* mode==4 for exit command */
		if(mode==4 && st.fn_depth<=0)
		{
			st.states &= ~(PROMPT|PROFILE|BUILTIN|FUNCTION|LASTPIPE);
			sh_exit(sh.exitval);
		}
		LONGJMP(*sh.freturn,mode);
	}
}

#ifdef LSTAT
/*
 * Given an absolute pathname, find physical pathname by resolving links 
 * path_phys returns 1, if successful, 0 for recursive link or overflow
 * path must be an array of at least PATH_MAX characters
 * The resulting path is canonicalized
 * Coded by David Korn
 *          ulysses!dgk
 */

path_physical(path)
register char *path;
/*@
	assume path!=NULL;
@*/
{
#ifdef apollo
	/* 
	 * This code has been added to figure out where we
	 * really are instead performing operations on the
	 * path string. This is much faster the walking through
	 * the path doing readlink(s). Note: code has been added in
	 * SYSCD(builtin.c) to look at the errno value in case getwd() fails.
	 */
	extern char *getwd();
	char realpath[PATH_MAX+1];				
	 
	if (!getwd(realpath))	
		return(0);
	strcpy(path, realpath);
	return(1);
#else /* apollo */
	register char *cp = path;
	char buffer[PATH_MAX+1];
	register char *savecp;
	int depth = 0;
	int c;
	int n;
	while(*cp)
	{
		/* skip over '/' */
		savecp = cp+1;
		while(*cp=='/')
			cp++;
		/* eliminate multiple slashes */
		if(cp > savecp)
			cp = strcpy(savecp,cp);
		/* check for .. */
		if(*cp=='.')
		{
			switch(cp[1])
			{
			case 0: case '/':
				/* eliminate /. */
				cp--;
				strcpy(cp,cp+2);
				continue;
			case '.':
				if(cp[2]=='/' || cp[2]==0)
				{
					/* backup, but not past root */
					savecp = cp+2;
					cp--;
					while(cp>path && *--cp!='/');
					strcpy(cp,savecp);
					continue;
				}
				break;
			}
		}
		savecp = cp;
		/* go to end of component */
		while(*cp && *cp!='/')
			cp++;
		c = *cp;
		*cp = 0;
		n = readlink(path,buffer,PATH_MAX);
		*cp = c;
		if(n>0)
		{
			if(++depth > MAXDEPTH)
				return(0);
			strcpy(buffer+n,cp);
			if(*buffer=='/')
				cp = strcpy(path,buffer);
			else
			{
				/* check for path overflow */
				cp = savecp;
				if((strlen(buffer)+(cp-path)) > PATH_MAX)
					return(0);
				strcpy(cp,buffer);
			}
		}
	}
	if(cp==path)
		*++cp = 0;
	else while(--cp > path && *cp=='/')
		/* eliminate trailing slashes */
		*cp = 0;
	return(1);
#endif /* apollo */
}
#endif /* LSTAT */

#ifdef ACCT
#   include <sys/acct.h>

static int compress();

static struct acct sabuf;
static struct tms buffer;
static clock_t	before;
static char *SHACCT; /* 0 environment variable SHACCT not set so never acct
		ptr to SHACCT value if set, so acct if shell procedure*/
static shaccton; /* 0 implies do not write record on exit
			  1 implies write acct record on exit
		*/
/*
 *	initialize accounting, i.e., see if SHACCT variable set
 */
void initacct()
{

	SHACCT = nam_strval(ACCTNOD);
}
/*
* suspend accounting unitl turned on by preacct()
*/
void suspacct()
{
	shaccton=0;
}

int preacct(cmdname)
char	*cmdname;
{
	if(SHACCT)
	{
		sabuf.ac_btime = time((time_t *)0);
		before = times(&buffer);
		sabuf.ac_uid = getuid();
		sabuf.ac_gid = getgid();
		strncpy(sabuf.ac_comm, (char*)path_basename(cmdname),
			sizeof(sabuf.ac_comm));
		shaccton = 1;
	}
}
void	doacct()
{
	int	fd;
	clock_t	after;

	if(shaccton)
	{
		after = times(&buffer);
		sabuf.ac_utime = compress(buffer.tms_utime + buffer.tms_cutime);
		sabuf.ac_stime = compress(buffer.tms_stime + buffer.tms_cstime);
		sabuf.ac_etime = compress( (time_t)(after-before));
		fd = open( SHACCT , O_WRONLY | O_APPEND | O_CREAT,RW_ALL);
		write(fd, &sabuf, sizeof( sabuf ));
		close( fd);
	}
}
 
/*
 * Produce a pseudo-floating point representation
 * with 3 bits base-8 exponent, 13 bits fraction.
 */
static int compress(t)
register time_t t;
{
	register int exp = 0, rund = 0;

	while (t >= 8192)
	{
		exp++;
		rund = t&04;
		t >>= 3;
	}
	if (rund)
	{
		t++;
		if (t >= 8192)
		{
			t >>= 3;
			exp++;
		}
	}
	return((exp<<13) + t);
}
#endif	/* ACCT */

