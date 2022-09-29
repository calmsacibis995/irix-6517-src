/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:sh/builtin.c	1.5.5.2"

/*
 *  builtin routines for the shell
 *
 *   David Korn
 *   AT&T Bell Laboratories
 *   Room 3C-526B
 *   Murray Hill, N. J. 07974
 *   Tel. x7975
 *
 */

#include	<errno.h>
#include	"defs.h"
#include	"history.h"
#include	"builtins.h"
#include	"jobs.h"
#include	"sym.h"

#define NOT_USED(x)	(&x,1)	/* prevents not used messages */

#ifdef _sys_resource_
#   ifndef included_sys_time_
#	include <sys/time.h>
#   endif
#   include	<sys/resource.h>/* needed for ulimit */
#   define	LIM_FSIZE	RLIMIT_FSIZE
#   define	LIM_DATA	RLIMIT_DATA
#   define	LIM_STACK	RLIMIT_STACK
#   define	LIM_CORE	RLIMIT_CORE
#   define	LIM_CPU		RLIMIT_CPU
#   define	INFINITY	RLIM64_INFINITY
#   ifdef RLIMIT_RSS
#	define	LIM_MAXRSS	RLIMIT_RSS
#   endif /* RLIMIT_RSS */
#else
#   ifdef VLIMIT
#	include	<sys/vlimit.h>
#   endif /* VLIMIT */
#endif	/* _sys_resource_ */
#ifdef UNIVERSE
    static int att_univ = -1;
#   ifdef _sys_universe_
#	include	<sys/universe.h>
#	define getuniverse(x)	(setuniverse(flag=setuniverse(0)),\
				(--flag>=0?strncpy(x,univ_name[flag],TMPSIZ):0),\
				flag)
#   else
#	define univ_number(x)	(x)
#   endif /* _sys_universe_ */
#endif /* UNIVERSE */

#ifdef ECHOPRINT
#   undef ECHO_RAW
#   undef ECHO_N
#endif /* ECHOPRINT */

#define DOTMAX	32	/* maximum level of . nesting */
#define PHYS_MODE	H_FLAG
#define LOG_MODE	N_LJUST

/* This module references these external routines */
#ifdef ECHO_RAW
    extern char		*echo_mode();
#endif	/* ECHO_RAW */
extern int		gscan_all();
extern char		*utos();
extern char		*u64tos();
extern void		ltou();

static int	b_common();
static int	b_unall();
static int	flagset();
static int	sig_number();
static int	scanargs();
#ifdef JOBS
    static void	sig_list();
#endif	/* JOBS */
static int	argnum;
static int 	aflag;
static int	newflag;
static int	echon;
static int	scoped;

int    b_exec(argn,com)
char **com;
{
        st.ioset = 0;
        if(*++com)
                b_login(argn,com);
	return(0);
}

int    b_login(argn,com)
char **com;
{
	NOT_USED(argn);
	if(is_option(RSHFLG))
		sh_cfail(gettxt(_SGI_DMMX_e_restricted,e_restricted));
	else
        {
#ifdef JOBS
		if(job_close() < 0)
			return(1);
#endif /* JOBS */
		/* force bad exec to terminate shell */
		st.states &= ~(PROFILE|PROMPT|BUILTIN|LASTPIPE);
		sig_reset(0);
		hist_close();
		sh_freeup();
		if(st.states&RM_TMP)
			rm_files(io_tmpname);
		path_exec(com,(struct argnod*)0);
		sh_done(0);
        }
	return(1);
}

int    b_pwd(argn,com)
char **com;
{
	register int flag=0;
	register char *a1 = com[1];
	NOT_USED(argn);
#if defined(LSTAT) || defined(FS_3D)
	while(a1 && *a1=='-'&& 
		(flag = flagset(a1,~(PHYS_MODE|LOG_MODE))))
	{
		if(flag&LOG_MODE)
			flag = 0;
		com++;
		a1 = com[1];
	}
#endif /* LSTAT||FS_3D */
	if(*(a1 = path_pwd(0))!='/')
		sh_cfail(gettxt(_SGI_DMMX_e_pwd,e_pwd));
#if defined(LSTAT) || defined(FS_3D)
	if(flag)
	{
#   ifdef FS_3D
		if(umask(flag=umask(0)),flag&01000)
			mount(".",a1=stakalloc(PATH_MAX),10|(PATH_MAX)<<4);
#   else
		a1 = stakcopy(a1);
#   endif /* FS_3D */
#ifdef LSTAT
		path_physical(a1);
#endif /* LSTAT */
	}
#endif /* LSTAT||FS_3D */
	p_setout(st.standout);
	p_str(a1,NL);
	return(0);
}

#ifndef ECHOPRINT
int    b_echo(argn,com)
int argn;
char **com;
{
	register int r;
	char *save = *com;
#   ifdef ECHO_RAW
	/* This mess is because /bin/echo on BSD is archaic */
#     ifdef UNIVERSE
	if(att_univ < 0)
	{
		int flag;
		char universe[TMPSIZ];
       		if(getuniverse(universe) >= 0)
			att_univ = (strcmp(universe,"att")==0);
	}
	if(att_univ>0)
		*com = (char*)e_minus;
	else
#     endif /* UNIVERSE */
	*com = echo_mode();
	r = b_print(argn+1,com-1);
	*com = save;
	return(r);
#   else
#	ifdef ECHO_N
	    /* same as echo except -n special */
	{
	    struct namnod  *np;
	    np = nam_search("_XPG",sh.var_tree,N_NULL);
	    if(!np || np && *(np->value.namval.cp) == '0') {
		    echon = 1;
		    return(b_print(argn,com));
	    } else {
		    *com = (char*)e_minus;
		    r = b_print(argn+1,com-1);
		    *com = save;
		    return(r);
	    }
	}
#	else
	    /* equivalent to print - */
	    *com = (char*)e_minus;
	    r = b_print(argn+1,com-1);
	    *com = save;
	    return(r);
#	endif /* ECHO_N */
#   endif	/* ECHO_RAW */
}
#endif /* ECHOPRINT */

int    b_print(argn,com)
int argn;
char **com;
{
	register int fd;
	register char *a1 = com[1];
	register char *cmd = com[0];
	register int flag = 0;
	register int r=0;
	const char *msg = gettxt(_SGI_DMMX_e_file,e_file);
	int raw = 0;
	NOT_USED(argn);
	argnum =  1;
	while(a1 && *a1 == '-')
	{
		int c = *(a1+1);
		com++;
		/* echon set when only -n is legal */
		if(echon)
		{
			if(strcmp(a1,"-n")==0)
				c = 0;
			else
			{
				com--;
				break;
			}
		}
		newflag = flagset(a1,~(N_FLAG|R_FLAG|P_FLAG|U_FLAG|S_FLAG|N_RJUST));
		flag |= newflag;
		/* handle the -R flag for BSD style echo */
		if(flag&N_RJUST)
			echon = 1;
		if(c==0 || newflag==0)
			break;
		a1 = com[1];
	}
	echon = 0;
	argnum %= 10;
	if(flag&(R_FLAG|N_RJUST))
		raw = 1;
	if(flag&S_FLAG)
	{
		/* print to history file */
		if(!hist_open())
			sh_cfail(gettxt(_SGI_DMMX_e_history,e_history));
		fd = hist_ptr->fixfd;
		st.states |= FIXFLG;
		goto skip;
	}
	else if(flag&P_FLAG)
	{
		fd = COTPIPE;
		msg = gettxt(_SGI_DMMX_e_query,e_query);
	}
	else if(flag&U_FLAG)
		fd = argnum;
	else	
		fd = st.standout;
	if(r = !fiswrite(fd))
	{
		if(fd==st.standout)
			return(r);
		sh_cfail(msg);
	}
skip:
	p_setout(fd);
	if(echo_list(raw,com+1))
	{	
		if(is_option(WORDEXP))
			p_char(0);
		else if((flag&N_FLAG)==0)
			newline();
	}
	if(flag&S_FLAG)
		hist_flush();
	return(r);
}

int    b_let(argn,com)
register int argn;
char **com;
{
	register int r;
	if(argn < 2)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
	while(--argn)
		r = !sh_arith(*++com);
	return(r);
}

int    b_command(argn,com,execflg)
register char **com;
{
	register const char *a1;
	register struct namnod *np;
	register int c,vflg,Vflg,pflg;
	register int spc_exec = 0;
	register int cmdoff;
	extern char *optarg;
	struct sh_optget Optget;

	vflg = Vflg = pflg = 0;

	if(argn == 1)
		sh_fail(*com,gettxt(_SGI_DMMX_e_nargs,e_nargs),ERROR);
		/* NOTREACHED */
	sv_optget(&Optget);
	while ((c = optget(argn, com, "vVp")) != EOF) {
		switch (c) {
			case 'v':
				if(Vflg || pflg) {
					rs_optget(&Optget);
					sh_fail(*com,gettxt(_SGI_DMMX_e_combo,e_combo),ERROR);
					/* NOTREACHED */
				}
				++vflg;
				break;
			case 'V':
				if(vflg || pflg) {
					rs_optget(&Optget);
					sh_fail(*com,gettxt(_SGI_DMMX_e_combo,e_combo),ERROR);
					/* NOTREACHED */
				}
				++Vflg;
				break;
			case 'p':
				if(vflg || Vflg) {
					rs_optget(&Optget);
					sh_fail(*com,gettxt(_SGI_DMMX_e_combo,e_combo),ERROR);
					/* NOTREACHED */
				}
				++pflg;
				break;
			case '?':
				rs_optget(&Optget);
				sh_fail(*com,gettxt(_SGI_DMMX_e_option,e_option),ERROR);
				/* NOTREACHED */
		}
	}
	if(((vflg || Vflg) && (argn-opt_index) != 1) || (pflg && (argn-opt_index) < 1)) {
		rs_optget(&Optget);
		sh_fail(*com,gettxt(_SGI_DMMX_e_nargs,e_nargs),ERROR);
		/* NOTREACHED */
	}
	cmdoff = opt_index;
	rs_optget(&Optget);

	if(!vflg && !Vflg) {
		struct fileblk	fb;
		int i,flags;
		union anynode *t;
		char inbuf[IOBSIZE+1];
		char build[IOBSIZE+1];
		char sav_path[PATH_MAX+1];
		struct ionod *saviotemp = st.iotemp;
		struct slnod *saveslp = st.staklist;
		struct namnod* np;

		if(pflg)
		{
			if(nam_fstrval(PATHNOD)) {
				strcpy(sav_path,nam_fstrval(PATHNOD));
				nam_fputval(PATHNOD,(char *)e_defpath);
			}
			else pflg=0;
		}

		io_push(&fb);
		/* 
		 * Reconstruct input line from **com args skipping the
		 * command "command" and any options.
		 * Surround each **com arg with single quotes to prevent
		 * further expansion and seperate with spaces.
		 */
		inbuf[0] = 0;
		strcpy(inbuf,com[cmdoff]);	/* Command name */
		for(i=(cmdoff+1); com[i]; ++i) {
			sprintf(build," '%s' ",com[i]);
			strcat(inbuf,build);
		}
		io_sopen(inbuf);
		st.exec_flag++;
		t = sh_parse(NL,NLFLG|MTFLG);
		st.exec_flag--;
		p_setout(ERRIO);
		io_pop(0);
		np = nam_search(path_basename(com[cmdoff]),sh.fun_tree,0);
		if(np && nam_istype(np,BLT_SPC))
			st.states |= CMD_SPC;
		flags = st.states&(ERRFLG|MONITOR);
		if(execflg)
			flags |= 1;
		st.states |= NOFUNC;

		sh_exec(t,flags);

		if(np && nam_istype(np,BLT_SPC))
			st.states &= ~CMD_SPC;
		st.states &= ~NOFUNC;
		sh_freeup();
		st.iotemp = saviotemp;
		st.staklist = saveslp;
		if(pflg)
			nam_fputval(PATHNOD,sav_path);
		return(sh.exitval);
	}
	p_setout(st.standout);

	/* Check for special builtin execables */
	np = nam_search(path_basename(com[cmdoff]),sh.fun_tree,0);

	if(np && nam_istype(np,BLT_EXEC)==BLT_EXEC) {
		++spc_exec;
		strcpy(com[cmdoff],path_basename(com[cmdoff]));
	}
	/* Utilities, regular builtin utilities, command names with slash */
	/* and implementation provided functions */

	if(!spc_exec && path_search(com[cmdoff],2)==0) {
		a1 = sh.lastpath;
		sh.lastpath = 0;
		if(a1) {
			p_str(a1,0);
			if(Vflg)
				p_str(gettxt(_SGI_DMMX_is_pathfound,is_pathfound),0);
			p_char(NL);
			return(0);
		}
	}
	/* built-ins and functions */

	if(np=nam_search(com[cmdoff],sh.fun_tree,N_NULL)) {
		if(is_abuiltin(np) && nam_istype(np,BLT_SPC)==BLT_SPC) {
			p_str(com[cmdoff],0);
			if(Vflg)
				p_str(gettxt(_SGI_DMMX_is_spc_builtin,is_spc_builtin),0);
		}
		else if(is_abuiltin(np)) {
			p_str(com[cmdoff],0);
			if(Vflg)
				p_str(gettxt(_SGI_DMMX_is_builtin,is_builtin),0);
		}
		else if(nam_istype(np,N_EXPORT)) {
			p_str(com[cmdoff],0);
			if(Vflg)
				p_str(gettxt(_SGI_DMMX_is_xfunction,is_xfunction),0);
		} else {
			p_str(com[cmdoff],0);
			if(Vflg)
				p_str(gettxt(_SGI_DMMX_is_function,is_function),0);
		}
		p_char(NL);
		return(0);
	}
	/* reserved words */

	if(sh_lookup(com[cmdoff],tab_reserved)) {
		p_str(com[cmdoff],0);
		if(Vflg)
			p_str(gettxt(_SGI_DMMX_is_reserved,is_reserved),0);
		p_char(NL);
		return(0);
	}
	/* aliases */

	if(np=nam_search(com[cmdoff],sh.alias_tree,N_NULL)) {
		if(Vflg)
			p_str(gettxt(_SGI_DMMX_e_aliasspc,e_aliasspc),0);
		env_prnamval(np,0);
		return(0);
	}
	if(vflg)
		return(127);
	p_setout(ERRIO);
	p_str(com[cmdoff],0);
	p_str(gettxt(_SGI_DMMX_e_found,e_found),NL);
	return(1);
}

/*
 * The following few builtins are provided to set,print,
 * and test attributes and variables for shell variables,
 * aliases, and functions.
 * In addition, typeset -f can be used to test whether a
 * function has been defined or to list all defined functions
 * Note readonly is same as typeset -r.
 * Note export is same as typeset -x.
 */

int    b_readonly(argn,com)
register char **com;
{
	register int pflag=0;
	register int c;
	struct sh_optget Optget;

	aflag = '-';
	argnum = scoped = 0;
	if(argn > 1 && *com[1] == '-')
	{
		sv_optget(&Optget);
		while ((c = optget(argn, com, "p")) != EOF)
		{
			switch (c) 
			{
				case 'p':
					++pflag;
					break;
				case '?':
					rs_optget(&Optget);
					sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
					/* NOTREACHED */
			}
		}
		if(pflag && argn > opt_index)
		{
			rs_optget(&Optget);
			sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
			/* NOTREACHED */
		}
		com += (opt_index-1);
		rs_optget(&Optget);
	}
	return(b_common(com,R_FLAG,sh.var_tree,(pflag?P_FLAG:0)));
}

int    b_export(argn,com)
register char **com;
{
	register int pflag=0;
	register int c;
	struct sh_optget Optget;

	aflag = '-';
	argnum = scoped = 0;
	if(argn > 1 && *com[1] == '-')
	{
		sv_optget(&Optget);
		while ((c = optget(argn, com, "p")) != EOF)
		{
			switch (c) 
			{
				case 'p':
					++pflag;
					break;
				case '?':
					rs_optget(&Optget);
					sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
					/* NOTREACHED */
			}
		}
		if(pflag && argn > opt_index)
		{
			rs_optget(&Optget);
			sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
			/* NOTREACHED */
		}
		com += (opt_index-1);
		rs_optget(&Optget);
	}
	return(b_common(com,X_FLAG,sh.var_tree,(pflag?P_FLAG:0)));
}
	
int	b_type(argn,com)
register char **com;
{
	NOT_USED(argn);
	if(com[1] == 0)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
	if(strcmp(com[1],"--")==0)
		++com;
	p_setout(st.standout);
	return(sh_whence(com,V_FLAG));
}


int	b_hash(argn,com)
register char **com;
{
	int c;
	struct sh_optget Optget;

	sv_optget(&Optget);
	scoped = 0;
	if(com[1] && *com[1] == '-')
	{
		while ((c = optget(argn, com, "r")) != EOF)
		{
			switch (c) 
			{
				case 'r':
					gscan_all(nam_free,sh.track_tree);
					rs_optget(&Optget);
					return(0);
				case '?':
					rs_optget(&Optget);
					sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
			}
		}
		com += (opt_index-1);
	}
	rs_optget(&Optget);
	aflag = '-';
	return(b_common(com,T_FLAG,sh.track_tree,0));
}



int    b_alias(argn,com)
register char **com;
{
	register int flag = 0;
	register struct Amemory *troot;
	NOT_USED(argn);
	argnum = scoped = 0;
	if(com[1])
	{
		if( (aflag= *com[1]) == '-' || aflag == '+')
		{
			com += scanargs(com,~(T_FLAG|N_EXPORT));
			flag = newflag;
		}
	}
	else
		aflag = 0;
	if(flag&T_FLAG)
		troot = sh.track_tree;
	else
		troot = sh.alias_tree;
	return(b_common(com,flag,troot,0));
}


int    b_typeset(argn,com)
register char **com;
{
	register int flag = 0;
	struct Amemory *troot;
	NOT_USED(argn);
	argnum = scoped = 0;
	if(com[1])
	{
		if( (aflag= *com[1]) == '-' || aflag == '+')
		{
			com += scanargs(com,~(N_LJUST|N_RJUST|N_ZFILL
				|N_INTGER|N_LTOU |N_UTOL|X_FLAG|R_FLAG
				|F_FLAG|T_FLAG|N_HOST
				|N_DOUBLE|N_EXPNOTE));
			flag = newflag;
		}
	}
	else
		aflag = 0;
	/* G_FLAG forces name to be in newest scope */
	if(st.fn_depth)
		scoped = G_FLAG;
	if((flag&N_INTGER) && (flag&(N_LJUST|N_RJUST|N_ZFILL|F_FLAG)))
		sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
	else if(flag&F_FLAG)
	{
		if(flag&~(N_EXPORT|F_FLAG|T_FLAG|U_FLAG))
			sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
		troot = sh.fun_tree;
		flag &= ~F_FLAG;
	}
	else
		troot = sh.var_tree;
	return(b_common(com,flag,troot,0));
}

static int     b_common(com,flag,troot,pflag)
register int flag,pflag;
char **com;
struct Amemory *troot;
{
	register int fd;
	register char *a1;
	register int type = 0;
	int r = 0;
	fd = st.standout;
	p_setout(fd);
	if(troot==sh.alias_tree)
		/* env_namset treats this value specially */
		type = (V_FLAG|G_FLAG);
	if(aflag == 0)
	{
		if(type)
			env_scan(fd,0,troot,0,pflag);
		else
			gscan_all(env_prattr,troot);
		return(0);
	}
	if(com[1])
	{
		while(a1 = *++com)
		{
			register unsigned newflag;
			register struct namnod *np;
			unsigned curflag;
			if(st.subflag && (flag || strchr(a1,'=')))
				continue;
			if(troot == sh.fun_tree)
			{
				/*
				 *functions can be exported or
				 * traced but not set
				 */
				if(flag&U_FLAG)
					np = env_namset(a1,sh.fun_tree,P_FLAG|V_FLAG);
				else
					np = nam_search(a1,sh.fun_tree,0);
				if(np && is_abuiltin(np))
					np = 0;
				if(np && ((flag&U_FLAG) || !isnull(np)))
				{
					if(flag==0)
					{
						env_prnamval(np,0);
						continue;
					}
					if(aflag=='-')
						nam_ontype(np,flag|N_FUNCTION);
					else if(aflag=='+')
						nam_offtype(np,~flag);
				}
				else
					r++;
				continue;
			}
			np = env_namset(a1,troot,(type|scoped));
			/* tracked alias */
			if(troot==sh.track_tree && aflag=='-')
			{
				nam_ontype(np,NO_ALIAS);
				path_alias(np,path_absolute(np->namid));
				continue;
			}
			if(flag==0 && aflag!='-' && strchr(a1,'=') == NULL)
			{
				/* type==0 for TYPESET */
				if(type && (isnull(np) || !env_prnamval(np,0)))
				{
					p_setout(ERRIO);
					p_str(a1,':');
					p_str(gettxt(_SGI_DMMX_e_alias,e_alias),NL);
					r++;
					p_setout(st.standout);
				}
				continue;
			}
			curflag = namflag(np);
			if (aflag == '-')
			{
				newflag = curflag;
				if(flag&~NO_CHANGE)
					newflag &= NO_CHANGE;
				newflag |= flag;
				if (flag & (N_LJUST|N_RJUST))
				{
					if (flag & N_LJUST)
						newflag &= ~N_RJUST;
					else
						newflag &= ~N_LJUST;
				}
				if (flag & N_UTOL)
					newflag &= ~N_LTOU;
				else if (flag & N_LTOU)
					newflag &= ~N_UTOL;
			}
			else
			{
				if((flag&R_FLAG) && (curflag&R_FLAG))
					sh_fail(np->namid,gettxt(_SGI_DMMX_e_readonly,e_readonly),ERROR);
				newflag = curflag & ~flag;
			}
			if (aflag && (argnum>0 || (curflag!=newflag)))
			{
				if(type)
					namflag(np) = newflag;
				else
					nam_newtype (np, newflag,argnum);
				nam_newtype (np, newflag,argnum);
			}
		}
	}
	else
		env_scan(fd,flag,troot,aflag=='+',(pflag?flag:0));
	return(r);
}

/*
 * The removing of Shell variable names, aliases, and functions
 * is performed here.
 * Unset functions with unset -f
 * Non-existent items being deleted give non-zero exit status
 */

int    b_unalias(argn,com)
char **com;
{
	register int c;
	struct sh_optget Optget;

	sv_optget(&Optget);
	while ((c = optget(argn, com, "a")) != EOF)
	{
		switch (c) 
		{
			case 'a':
				gscan_all(nam_free,sh.alias_tree);
				rs_optget(&Optget);
				return(0);
			case '?':
				rs_optget(&Optget);
				sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
		}
	}
	com += (opt_index-1);
	argn -= (opt_index-1);
	rs_optget(&Optget);
	return(b_unall(argn,com,sh.alias_tree));
}

int    b_unset(argn,com)
register char **com;
{
	struct Amemory *troot;
	register int c;
	struct sh_optget Optget;

	sv_optget(&Optget);
	troot = sh.var_tree;	/* default tree */
	while ((c = optget(argn, com, "fv")) != EOF)
	{
		switch (c) 
		{
			case 'f':
				troot = sh.fun_tree;
				break;
			case 'v':
				troot = sh.var_tree;
				break;
			case '?':
				rs_optget(&Optget);
				sh_fail(*com,gettxt(_SGI_DMMX_e_option,e_option),ERROR);
				/* NOTREACHED */
		}
	}
	com += (opt_index-1);
	argn -= (opt_index-1);
	rs_optget(&Optget);
	return(b_unall(argn,com,troot));
}

static int	b_unall(argn,com,troot)
register int argn;
char **com;
struct Amemory *troot;
{
	register char *a1;
	register struct namnod *np;
	register struct slnod *slp;
	int r = 0;
	if(st.subflag)
		return(0);
	if(argn < 2)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
	
	while(a1 = *++com)
	{
		np=env_namset(a1,troot,P_FLAG);
		if(np && !isnull(np))
		{
			if(troot==sh.var_tree)
			{
				if (nam_istype (np, N_RDONLY))
					sh_fail(np->namid,gettxt(_SGI_DMMX_e_readonly,e_readonly),ERROR);
				else if (nam_istype (np, N_RESTRICT))
					sh_fail(np->namid,gettxt(_SGI_DMMX_e_restricted,e_restricted),ERROR);
#ifdef apollo
				{
					short namlen;
					namlen =strlen(np->namid);
					ev_$delete_var(np->namid,&namlen);
				}
#endif /* apollo */
			}
			else if(is_abuiltin(np))
			{
				r = 1;
				continue;
			}
			else if(slp=(struct slnod*)(np->value.namenv))
			{
				/* free function definition */
				stakdelete(slp->slptr);
				np->value.namenv = 0;
			}
			nam_free(np);
		}
		else if(isnull(np) && troot!=sh.alias_tree)
			r = 0;
		else	r = 1;
	}
	return(r);
}


int	b_dot(argn,com)
char **com;
{
	register char *a1 = com[1];
	st.states &= ~MONITOR;
	if(a1)
	{
		register int flag;
		jmp_buf retbuf;
		jmp_buf *savreturn = sh.freturn;
#ifdef POSIX
		/* check for function first */
		register struct namnod *np;
		np = nam_search(a1,sh.fun_tree,0);
		if(np && !np->value.namval.ip)
		{
			if(!nam_istype(np,N_FUNCTION))
				np = 0;
			else
			{
				path_search(a1,0);
				if(np->value.namval.ip==0)
					sh_fail(a1,gettxt(_SGI_DMMX_e_found,e_found),FNDBAD);
			}
		}
		if(!np)
#endif /* POSIX */
		if((sh.un.fd=path_open(a1,path_get(a1))) < 0)
			sh_fail(a1,gettxt(_SGI_DMMX_e_found,e_found),FNDBAD);
		else
		{
			if(st.dot_depth++ > DOTMAX)
				sh_cfail(gettxt(_SGI_DMMX_e_recursive,e_recursive));
			if(argn > 2)
				arg_set(com+1);
			st.states |= BUILTIN;
			sh.freturn = (jmp_buf*)retbuf;
			flag = SETJMP(retbuf);
			if(flag==0)
			{
#ifdef POSIX
				if(np)
					sh_exec((union anynode*)(funtree(np))
					,(int)(st.states&(ERRFLG|MONITOR)));
				else
#endif /* POSIX */
				sh_eval((char*)0);
			}
			st.states &= ~BUILTIN;
			sh.freturn = savreturn;
			st.dot_depth--;
			if(flag && flag!=2)
				sh_exit(sh.exitval);
		}
	}
	else
		sh_cfail(gettxt(_SGI_DMMX_e_argexp,e_argexp));
	return(sh.exitval);
}

int	b_times()
{
	struct tms tt;
	times(&tt);
	p_setout(st.standout);
	p_time(tt.tms_utime,' ');
	p_time(tt.tms_stime,NL);
	p_time(tt.tms_cutime,' ');
	p_time(tt.tms_cstime,NL);
	return(0);
}
		

/*
 * return and exit
 */

int	b_ret_exit(argn,com)
register char **com;
{
	register int flag;
	register int isexit = (**com=='e');
	NOT_USED(argn);
	if(st.subflag)
		return(0);
	flag = ((com[1]?atoi(com[1]):sh.oldexit)&EXITMASK);
	if(st.fn_depth>0 || (!isexit && (st.dot_depth>0||(st.states&PROFILE))))
	{
		sh.exitval = flag;
		LONGJMP(*sh.freturn,isexit?4:2);
	}
	/* force exit */
	st.states &= ~(PROMPT|PROFILE|BUILTIN|FUNCTION|LASTPIPE);
	sh_exit(flag);
	return(1);
}
		
/*
 * null command
 */
int	b_null(argn,com)
register char **com;
{
	NOT_USED(argn);
	return(**com=='f');
}
		
int	b_continue(argn,com)
register char **com;
{
	NOT_USED(argn);
	if(!st.subflag && st.loopcnt)
	{
		st.execbrk = st.breakcnt = 1;
		if(com[1])
			st.breakcnt = atoi(com[1]);
		if(st.breakcnt > st.loopcnt)
			st.breakcnt = st.loopcnt;
		else
			st.breakcnt = -st.breakcnt;
	}
	return(0);
}
		
int	b_break(argn,com)
register char **com;
{
	NOT_USED(argn);
	if(!st.subflag && st.loopcnt)
	{
		st.execbrk = st.breakcnt = 1;
		if(com[1])
			st.breakcnt = atoi(com[1]);
		if(st.breakcnt > st.loopcnt)
			st.breakcnt = st.loopcnt;
	}
	return(0);
}
		
int	b_trap(argn,com)
char **com;
{
	register char *a1 = com[1];
	register int sig;
	NOT_USED(argn);
	if(a1)
	{
		register int	clear;
		char *action = a1;
		if(st.subflag)
			return(0);
		if(strcmp(a1,"--")==0) {
			if(!com[2]) {
				sig_list(-1);
				return(0);
			}
			++com;
			action = a1 = com[1];
		}
		/* first argument all digits or - means clear */
		while(isdigit(*a1))
			a1++;
		clear = (a1!=action && *a1==0);
		if(!clear)
		{
			++com;
			if(*action=='-' && action[1]==0)
				clear++;
		}
		while(a1 = *++com)
		{
			sig = sig_number(a1);
			if(sig>=MAXTRAP || sig<MINTRAP)
				sh_fail(a1,gettxt(_SGI_DMMX_e_trap,e_trap),ERROR);
			else if(clear)
				sig_clear(sig);
			else
			{
				if(a1=st.trapcom[sig])
					free(a1);
				st.trapcom[sig] = sh_heap(action);
				if(*action)
					sig_ontrap(sig);
				else
					sig_ignore(sig);
			}
		}
	}
	else /* print out current traps */
#ifdef POSIX
		sig_list(-1);
#else
	{
		p_setout(st.standout);
		for(sig=0; sig<MAXTRAP; sig++)
			if(st.trapcom[sig])
			{
				p_num(sig,':');
				p_str(st.trapcom[sig],NL);
			}
	}
#endif /* POSIX */
	return(0);
}
		
int	b_chdir(argn,com)
char **com;
{
	register char *a1 = com[1];
	register const char *dp;
	register char *cdpath = NULLSTR;
	int flag = 0;
	int rval = -1;
	int errval = 1;
	char *oldpwd;
	if(st.subflag)
		return(0);
	if(is_option(RSHFLG))
		sh_cfail(gettxt(_SGI_DMMX_e_restricted,e_restricted));
#ifdef LSTAT
#ifdef apollo
	/* 
	 * support for the apollo "set -o physical" feature.
	 */
	flag = is_option(PHYSICAL);
#endif /* apollo */
	while(a1 && *a1=='-' && a1[1]) 
	{
		flag = flagset(a1,~(PHYS_MODE|LOG_MODE));
		if(flag&LOG_MODE)
			flag = 0;
		com++;
		argn--;
		a1 =  com[1];
	}
#endif /* LSTAT */
	if(argn >3)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
	oldpwd = sh.pwd;
	if(argn==3)
		a1 = sh_substitute(oldpwd,a1,com[2]);
	else if(a1==0 || *a1==0)
		a1 = nam_strval(HOME);
	else if(*a1 == '-' && *(a1+1) == 0)
		a1 = nam_strval(OLDPWDNOD);
	if(a1==0 || *a1==0)
		sh_cfail(argn==3?gettxt(_SGI_DMMX_e_subst,e_subst):gettxt(_SGI_DMMX_e_direct,e_direct));
	if(*a1 != '/')
	{
		cdpath = nam_strval(CDPNOD);
		if(oldpwd==0)
			oldpwd = path_pwd(1);
	}
	if(cdpath==0)
		cdpath = NULLSTR;
	if(*a1=='.')
	{
		/* test for pathname . ./ .. or ../ */
		if(*(dp=a1+1) == '.')
			dp++;
		if(*dp==0 || *dp=='/')
			cdpath = NULLSTR;
	}
	do
	{
		dp = cdpath;
		cdpath=path_join((char*)dp,a1);
		if(*stakptr(OPATH)!='/')
		{
			char *last=(char*)stakfreeze(1);
			stakseek(OPATH);
			stakputs(oldpwd);
			stakputc('/');
			stakputs(last+OPATH);
			stakputc(0);
		}
#ifdef LSTAT
		if(!flag)
#endif /* LSTAT */
		{
			register char *cp;
#ifdef FS_3D
			if(!(cp = pathcanon(stakptr(OPATH))))
				continue;
			/* eliminate trailing '/' */
			while(*--cp == '/' && cp>stakptr(OPATH))
				*cp = 0;
#else
			if(*(cp=stakptr(OPATH))=='/')
				if(!pathcanon(cp))
					continue;
#endif /* FS_3D */
		}
		rval = chdir(path_relative(stakptr(OPATH)));
	}
	while(rval<0 && cdpath);
	/* use absolute chdir() if relative chdir() fails */
	if(rval<0 && *a1=='/' && *(path_relative(stakptr(OPATH)))!='/')
		rval = chdir(a1);
#ifdef apollo
	/*
	 * The label is used to display the error message if path_physical()
	 * routine fails.(See below)
	 */
unavoidable_goto:
#endif /* apollo */
	if(rval<0)
	{
		switch(errno)
		{
#ifdef ENAMETOOLONG
		case ENAMETOOLONG:
			dp = gettxt(_SGI_DMMX_e_longname,e_longname);
			break;
#endif /* ENAMETOOLONG */
#ifdef EMULTIHOP
		case EMULTIHOP:
			dp = gettxt(_SGI_DMMX_e_multihop,e_multihop);
			break;
#endif /* EMULTIHOP */
		case ENOTDIR:
			dp = gettxt(_SGI_DMMX_e_notdir,e_notdir);
			break;

		case ENOENT:
			dp = gettxt(_SGI_DMMX_e_found,e_found);
			errval = FNDBAD;
			break;

		case EACCES:
			dp = gettxt(_SGI_DMMX_e_access,e_access);
			break;
#ifdef ENOLINK
		case ENOLINK:
			dp = gettxt(_SGI_DMMX_e_link,e_link);
			break;
#endif /* ENOLINK */
		default: 	
			dp = gettxt(_SGI_DMMX_e_direct,e_direct);
			break;
		}
		sh_fail(a1,dp,errval);
	}
	if(a1 == nam_strval(OLDPWDNOD) || argn==3)
		dp = a1;	/* print out directory for cd - */
#ifdef LSTAT
	if(flag)
	{
		a1 = stakfreeze(1)+OPATH;
#ifdef apollo
		/*
		 * check the return status of path_physical().
		 * if the return status is 0 then the getwd() has
		 * failed, so print an error message.
		 */
		if ( !path_physical(a1) )
		{
			rval = -1;
			goto unavoidable_goto;
		}
#else
		path_physical(a1);
#endif /* apollo */
	}
	else
#endif /* LSTAT */
		a1 = (char*)stakfreeze(1)+OPATH;
#ifdef POSIX
	if(*dp && *dp!= ':' && strchr(a1,'/'))
#else
	if(*dp && *dp!= ':' && (st.states&PROMPT) && strchr(a1,'/'))
#endif /* POSIX */
	{
		p_setout(st.standout);
		p_str(a1,NL);
	}
	if(*a1 != '/')
		return(0);
	nam_fputval(OLDPWDNOD,oldpwd);
	if(oldpwd)
		free(oldpwd);
	nam_free(PWDNOD);
	nam_fputval(PWDNOD,a1);
	nam_ontype(PWDNOD,N_FREE|N_EXPORT);
	sh.pwd = PWDNOD->value.namval.cp;
	return(0);
}
		
int	b_shift(argn,com)
register char **com;
{
	register int flag = (com[1]?(int)sh_arith(com[1]):1);
	NOT_USED(argn);
	if(flag<0 || st.dolc<flag)
		sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	else
	{
		if(st.subflag)
			return(0);
		st.dolv += flag;
		st.dolc -= flag;
	}
	return(0);
}
		
int	b_wait(argn,com)
register char **com;
{
	NOT_USED(argn);
	st.states &= ~MONITOR;
	if(!st.subflag)
		job_bwait(com+1);
	return(sh.exitval);
}

/* 
 * b_read() adds C_FLAG to env_readline() to continue assigning variables 
 * after encountering readonly variables.
 */
		
int	b_read(argn,com)
char **com;
{
	register char *a1 = com[1];
	register int flag;
	register int fd;
	int r1,r2;
	NOT_USED(argn);
	st.states &= ~MONITOR;
	tty_set(-1);    /* reset tty info */
	argnum = 0;
	com += scanargs(com,~(R_FLAG|P_FLAG|U_FLAG|S_FLAG));
	a1 = com[1];
	flag = newflag;
	if(flag&P_FLAG)
	{
		if((fd = sh.cpipe[INPIPE])<=0)
			sh_cfail(gettxt(_SGI_DMMX_e_query,e_query));
	}
	else if(flag&U_FLAG)
		fd = argnum;
	else
		fd = 0;
	if(fd && !fisread(fd))
		sh_cfail(gettxt(_SGI_DMMX_e_file,e_file));
	/* look for prompt */
	if(a1 && (a1=strchr(a1,'?')) && tty_check(fd))
	{
		p_setout(ERRIO);
		p_str(a1+1,0);
	}
	r1 = env_readline(&com[1],fd,flag&(R_FLAG|S_FLAG)|C_FLAG);
	if(r2=(fiseof(io_ftable[fd])!=0))
	{
		if(flag&P_FLAG)
		{
			io_pclose(sh.cpipe);
			return(1);
		}
	}
	clearerr(io_ftable[fd]);
	return(r2?r2:(r1?r1:0));
}
	
int	b_set(argn,com)
register char **com;
{
	if(com[1])
	{
		arg_opts(argn,com,BUILTIN);
		st.states &= ~(READPR|MONITOR);
		st.states |= is_option(READPR|MONITOR);
	}
	else
		/*scan name chain and print*/
		env_scan(st.standout,0,sh.var_tree,0,0);
	return(sh.exitval);
}

int	b_eval(argn,com)
register char **com;
{
	NOT_USED(argn);
	st.states &= ~MONITOR;
	if(com[1])
	{
		sh.un.com = com+2;
		sh_eval(com[1]);
	}
	return(sh.exitval);
}

int	b_fc(argn,com)
char **com;
{
	register char *a1;
	register int flag;
	register struct history *fp;
	int fdo;
	char *argv[2];
	char fname[TMPSIZ];
	int index2;
	int indx = -1; /* used as subscript for range */
	char *edit = NULL;		/* name of editor */
	char *replace = NULL;		/* replace old=new */
	int incr;
	int range[2];	/* upper and lower range of commands */
	int lflag = 0;
	int nflag = 0;
	int rflag = 0;
	histloc location;
	NOT_USED(argn);
	st.states |= BUILTIN;
	if(!hist_open())
		sh_cfail(gettxt(_SGI_DMMX_e_history,e_history));
	fp = hist_ptr;
	while((a1=com[1]) && *a1 == '-')
	{
		if(*(a1+1) == '-')
		{
			++com;
			continue;
		}
		argnum = -1;
		flag = flagset(a1,~(E_FLAG|L_FLAG|N_FLAG|R_FLAG|S_FLAG));
		if(flag==0)
		{
			if(argnum < 0)
			{
				com++;
				break;
			}
			flag = fp->fixind - argnum-1;
			if(flag <= 0)
				flag = 1;
			range[++indx] = flag;
			argnum = 0;
			if(indx==1)
				break;
		}
		else
		{
			if(flag&E_FLAG)
			{
				/* name of editor specified */
				com++;
				if((edit=com[1]) == NULL)
					sh_cfail(gettxt(_SGI_DMMX_e_argexp,e_argexp));
			}
			if(flag&S_FLAG)
				edit = "-";
			if(flag&N_FLAG)
				nflag++;
			if(flag&L_FLAG)
				lflag++;
			if(flag&R_FLAG)
				rflag++;
		}
		com++;
	}
	flag = indx;
	while(flag<1 && (a1=com[1]))
	{
		/* look for old=new argument */
		if(replace==NULL && strchr(a1+1,'='))
		{
			replace = a1;
			com++;
			continue;
		}
		else if(isdigit(*a1) || *a1 == '-' || *a1 == '+')
		{
			/* see if completely numeric */
			do	a1++;
			while(isdigit(*a1));
			if(*a1==0)
			{
				a1 = com[1];
				range[++flag] = atoi(a1);
				if(*a1 == '-')
					range[flag] += (fp->fixind-1);
				com++;
				continue;
			}
		}
		/* search for last line starting with string */
		location = hist_find(com[1],fp->fixind-1,0,-1);
		if((range[++flag] = location.his_command) < 0)
			sh_fail(com[1],gettxt(_SGI_DMMX_e_found,e_found),FNDBAD);
		com++;
	}
	if(flag <0)
	{
		/* set default starting range */
		if(lflag)
		{
			flag = fp->fixind-16;
			if(flag<1)
				flag = 1;
		}
		else
			flag = fp->fixind-2;
		range[0] = flag;
		flag = 0;
	}
	if(flag==0)
		/* set default termination range */
		range[1] = (lflag?fp->fixind-1:range[0]);
	if((index2 = fp->fixind - fp->fixmax) <=0)
	index2 = 1;
	/* check for valid ranges */
	for(flag=0;flag<2;flag++)
		if(range[flag]<index2 ||
			range[flag]>=(fp->fixind-(lflag==0)))
			sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	if(edit && *edit=='-' && range[0]!=range[1])
		sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	/* now list commands from range[rflag] to range[1-rflag] */
	incr = 1;
	flag = rflag>0;
	if(range[1-flag] < range[flag])
		incr = -1;
	if(lflag)
	{
		fdo = st.standout;
		a1 = "\n\t";
	}
	else
	{
		fdo = io_mktmp(fname);
		a1 = "\n";
		nflag++;
	}
	p_setout(fdo);
	while(1)
	{
		if(nflag==0)
			p_num(range[flag],'\t');
		else if(lflag)
			p_char('\t');
		hist_list(hist_position(range[flag]),0,a1);
		if(lflag && (sh.trapnote&SIGSET))
			sh_exit(SIGFAIL);
		if(range[flag] == range[1-flag])
			break;
		range[flag] += incr;
	}
	if(lflag)
		return(0);
	io_fclose(fdo);
	hist_eof();
	p_setout(ERRIO);
	a1 = edit;
	if(a1==NULL && (a1=nam_strval(FCEDNOD)) == NULL)
		a1 = (char*)e_defedit;
#ifdef apollo
	/*
	 * Code to support the FC using the pad editor.
	 * Exampled of how to use: FCEDIT=pad
	 */
	if (strcmp (a1, "pad") == 0)
	{
		int pad_create();
		io_fclose(fdo);
		fdo = pad_create(fname);
		pad_wait(fdo);
		unlink(fname);
		strcat(fname, ".bak");
		unlink(fname);
		io_seek(fdo,(off_t)0,SEEK_SET);
	}
	else
	{
#endif /* apollo */
	if(*a1 != '-')
	{
		sh.un.com = argv;
		argv[0] =  fname;
		argv[1] = NULL;
		sh_eval(a1);
	}
	fdo = io_fopen(fname);
	unlink(fname);
#ifdef apollo
	}
#endif /* apollo */
	/* don't history fc itself unless forked */
	if(!(st.states&FORKED))
		hist_cancel();
	st.states |= (READPR|FIXFLG);	/* echo lines as read */
	st.exec_flag--;  /* needed for command numbering */
	if(replace!=NULL)
		hist_subst(sh.cmdname,fdo,replace);
	else if(sh.exitval == 0)
	{
		/* read in and run the command */
		st.states &= ~BUILTIN;
		sh.un.fd = fdo;
		sh_eval((char*)0);
	}
	else
	{
		io_fclose(fdo);
		if(!is_option(READPR))
			st.states &= ~(READPR|FIXFLG);
	}
	st.exec_flag++;
	return(sh.exitval);
}

int	b_getopts(argn,com)
char **com;
{
	register char *optstr = com[1];
	register char *namval;
	register int flag;
	register struct namnod *n;
	char namarg[3];
	int r = 0;
	extern opt_index,opt_opt;

	if(argn < 3)
		sh_cfail(gettxt(_SGI_DMMX_e_argexp,e_argexp));
	n = env_namset(com[2],sh.var_tree,P_FLAG);

	if(argn>3)
	{
		com +=2;
		argn -=2;
	}
	else
	{
		com = st.dolv;
		argn = st.dolc+1;
	}
	/* nam_offtype(OPTARGNOD,~N_RDONLY); */
	switch(flag=optget(argn,com,optstr))
	{
		case ':':
		case '?':
			if(*optstr != ':')
			{
				p_setout(ERRIO);
				p_prp(sh.cmdname);
				p_str(e_colon,opt_opt);
				p_char(' ');
				if(flag == '?')
				{
					p_str(gettxt(_SGI_DMMX_e_option,e_option),NL);
					r = 0;
				}
				else	
				{
					p_str(gettxt(_SGI_DMMX_e_argexp,e_argexp),NL);
					r = 1;
				}
				nam_fputval(OPTARGNOD,"");
				namval = "?";
			}
			else
			{
				char badarg[2];
				badarg[0] = opt_opt;
				badarg[1] = 0;
				if(flag == '?')
					namval = "?";
				else	namval = ":";
				r = 0;
				nam_fputval(OPTARGNOD,badarg);
			}
			break;
		case -1:
			namval = "?";
			r = 1;
			nam_fputval(OPTARGNOD,"");
			break;
		default:
			if(opt_plus)
			{
				namarg[0] = '+';
				namarg[1] = flag;
				namarg[2] = 0;
			}
			else
			{
				namarg[0] = flag;
				namarg[1] = 0;
			}
			namval = namarg;
			nam_fputval(OPTARGNOD,opt_arg);
			r = 0;
			break;

	}
	nam_putval(n, namval);
	/* nam_ontype(OPTARGNOD,N_RDONLY); */
	return(r);
}
	
int	b_whence(argn,com)
register char **com;
{
	NOT_USED(argn);
	com += scanargs(com,~(V_FLAG|P_FLAG));
	if(com[1] == 0)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
	p_setout(st.standout);
	return(sh_whence(com,newflag));
}


int	b_umask(argn,com)
char **com;
{
	register char *a1;
	register int flag = 0;
	register int Sflag = 0;
	register int c;
	struct sh_optget Optget;

	sv_optget(&Optget);
	if(argn == 1)
		a1 = NULL;
	else if(argn > 1 && *com[1] == '-')
	{
		while ((c = optget(argn, com, "S")) != EOF)
		{
			switch (c) 
			{
				case 'S':
					++Sflag;
					break;
				case '?':
					rs_optget(&Optget);
					sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
					/* NOTREACHED */
			}
		}
		if(Sflag && argn > opt_index || (argn-opt_index) > 1)
		{
			rs_optget(&Optget);
			sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
			/* NOTREACHED */
		}
		if(Sflag)
			a1 = NULL;
		else	a1 = com[opt_index];
	}
	else if(argn > 2)
	{
		rs_optget(&Optget);
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
		/* NOTREACHED */
	}
	else a1 = com[opt_index];
	rs_optget(&Optget);
	if(a1)
	{
		register int c;	
		if(st.subflag)
			return(0);
		if(isdigit(*a1))
		{
			while(c = *a1++)
			{
				if (c>='0' && c<='7')	
					flag = (flag<<3) + (c-'0');	
				else
					sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
			}
		}
		else
		{
			char **cp = com+1;
			flag = umask(0);
			c = strperm(a1,cp,~flag);
			if(**cp)
			{
				umask(flag);
				sh_cfail(gettxt(_SGI_DMMX_e_format,e_format));
			}
			flag = (~c&0777);
		}
		umask(flag);	
	}	
	else
	{
		p_setout(st.standout);
		a1 = utos((ulong)(flag=umask(0)),8);
		umask(flag);
		if(Sflag)
		{
			register int x;
			for(x=2;x>=0;--x)
			{
				(x==2?p_str("u=",0):(x==1?p_str(",g=",0):p_str(",o=",0)));
				if((~flag>>(x*3))&4)p_str("r",0);
				if((~flag>>(x*3))&2)p_str("w",0);
				if((~flag>>(x*3))&1)p_str("x",0);
			}
			p_char(NL);
		}
		else
		{
			*++a1 = '0';
			p_str(a1,NL);
		}
	}
	return(0);
}

#ifdef LIM_CPU
#		define HARD	1
#		define SOFT	2
		 /* BSD style ulimit */
int	b_ulimit(argn,com)
char **com;
{
	register char *a1;
	register int flag = 0;
#   ifdef RLIMIT_CPU
	struct rlimit64 rlp;
#   endif /* RLIMIT_CPU */
	const struct sysmsgnod *sp;
	rlim64_t i;
	int label;
	register int n;
	register int mode = 0;
	int unit;
	struct sh_optget Optget;
	sv_optget(&Optget);
	while((n = optget(argn,com,":HSacdfmnpstv")) != EOF)
	{
		switch(n)
		{
			case 'H':
				mode |= HARD;
				continue;
			case 'S':
				mode |= SOFT;
				continue;
			case 'a':
				flag = (0x2f
#   ifdef LIM_MAXRSS
				|(1<<4)
#   endif /* LIM_MAXRSS */
#   ifdef RLIMIT_NOFILE
				|(1<<6)
#   endif /* RLIMIT_NOFILE */
#   ifdef RLIMIT_VMEM
				|(1<<7)
#   endif /* RLIMIT_VMEM */
#   ifdef RLIMIT_PTHREAD
				|(1<<8)
#   endif /* RLIMIT_PTHREAD */
					);
				break;
			case 't':
				flag |= 1;
				break;
#   ifdef LIM_MAXRSS
			case 'm':
				flag |= (1<<4);
				break;
#   endif /* LIM_MAXRSS */
			case 'd':
				flag |= (1<<2);
				break;
			case 's':
				flag |= (1<<3);
				break;
			case 'f':
				flag |= (1<<1);
				break;
			case 'c':
				flag |= (1<<5);
				break;
#   ifdef RLIMIT_NOFILE
			case 'n':
				flag |= (1<<6);
				break;
#   endif /* RLIMIT_NOFILE */
#   ifdef RLIMIT_VMEM
			case 'v':
				flag |= (1<<7);
				break;
#   endif /* RLIMIT_VMEM */
#   ifdef RLIMIT_PTHREAD
			case 'p':
				flag |= (1<<8);
				break;
#   endif /* RLIMIT_PTHREAD */
			default:
				rs_optget(&Optget);
				sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
		}
	}
	a1 = com[opt_index];
	rs_optget(&Optget);
	/* default to -f */
	if(flag==0)
		flag |= (1<<1);
	/* only one option at a time for setting */
	label = (flag&(flag-1));
	if(a1 && label)
		sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
	sp = limit_names;
	if(mode==0)
		mode = (HARD|SOFT);
	for(; flag; sp++,flag>>=1)
	{
		if(!(flag&1))
			continue;
		n = sp->sysval>>11;
		unit = sp->sysval&0x7ff;
		if(a1)
		{
			if(st.subflag)
				return(0);
			if(strcmp(a1,gettxt(_SGI_DMMX_e_unlimited,e_unlimited))==0)
				i = INFINITY;
			else
			{
				long long n64;
				if((n64=sh_arith(a1)) < 0)
					sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
				i = (rlim64_t)n64;
				i *= unit;
			}
#   ifdef RLIMIT_CPU
			if(getrlimit64(n,&rlp) <0)
				sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
			if(mode&HARD)
				rlp.rlim_max = i;
			if(mode&SOFT)
				rlp.rlim_cur = i;
			if(setrlimit64(n,&rlp) <0)
				sh_cfail(gettxt(_SGI_DMMX_e_ulimit,e_ulimit));
#   endif /* RLIMIT_CPU */
		}
		else
		{
#   ifdef  RLIMIT_CPU
			if(getrlimit64(n,&rlp) <0)
				sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
			if(mode&HARD)
				i = rlp.rlim_max;
			if(mode&SOFT)
				i = rlp.rlim_cur;
#   else
			i = -1;
		}
		if((i=vlimit(n,i)) < 0)
			sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
		if(a1==0)
		{
#   endif /* RLIMIT_CPU */
			p_setout(st.standout);
			if(label)
				p_str(gettxt(sp->sysmsgid,sp->sysnam),SP);
			if(i!=INFINITY)
			{
				i = (i+unit-1)/unit;
				p_str(u64tos((unsigned long long)i,10),NL);
			}
			else
				p_str(gettxt(_SGI_DMMX_e_unlimited,e_unlimited),NL);
		}
	}
	return(0);
}
#else
int	b_ulimit(argn,com)
char **com;
{
	register char *a1 = com[1];
	register int flag;
#   ifndef VENIX
	long i;
	long ulimit();
	register int mode = 2;
	NOT_USED(argn);
	if(a1 && *a1 == '-')
	{
#	ifdef RT
		flag = flagset(a1,~(F_FLAG|P_FLAG));
#	else
		flag = flagset(a1,~F_FLAG);
#	endif /* RT */
		a1 = com[2];
	}
	if(flag&P_FLAG)
		mode = 5;
	if(a1)
	{
		if(st.subflag)
			return(0);
		if((i=sh_arith(a1)) < 0)
			sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	}
	else
	{
		mode--;
		i = -1;
	}
	if((i=ulimit(mode,i)) < 0)
		sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	if(a1==0)
	{
		p_setout(st.standout);
		p_str(utos((ulong)i,10),NL);
	}
#   endif /* VENIX */
	return(0);
}
#endif /* LIM_CPU */

#ifdef JOBS
#   ifdef SIGTSTP
int	b_bgfg(argn,com)
register char **com;
{
	register int flag = (**com=='b');
	NOT_USED(argn);
#ifdef POSIX
	if(com[1] && *com[1] == '-')
	{
		if(strcmp(com[1],"--")==0)
			++com;
		else
			sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
	}
#endif /* POSIX */
	if(!is_option(MONITOR) || !job.jobcontrol)
	{
		if(st.states&PROMPT)
			sh_cfail(gettxt(_SGI_DMMX_e_no_jctl,e_no_jctl));
		return(1);
	}
	if(job_walk(job_switch,flag,com+1))
		sh_cfail(gettxt(_SGI_DMMX_e_no_job,e_no_job));
	return(sh.exitval);
}
#    endif /* SIGTSTP */

int	b_jobs(argn,com)
char **com;
{
	NOT_USED(argn);
	com += scanargs(com,~(N_FLAG|L_FLAG|P_FLAG));
	if(*++com==0)
		com = 0;
	p_setout(st.standout);
	if(job_walk(job_list,newflag,com))
		sh_cfail(gettxt(_SGI_DMMX_e_no_job,e_no_job));
	return(sh.exitval);
}

int	b_kill(argn,com)
char **com;
{
	register int flag;
	register char *a1 = com[1];
	if(argn < 2)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
	/* just in case we send a kill -9 $$ */
	p_flush();
	flag = SIGTERM;
	if(*a1 == '-')
	{
		a1++;
		if(*a1 == 'l')
		{
#ifdef POSIX
			if(argn>2)
			{
				com++;
				while(a1= *++com)
				{
					if(isdigit(*a1))
						sig_list((atoi(a1)&0177)+1);
					else
					{
						if((flag=sig_number(a1)) < 0)
							sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
						p_num(flag,NL);
					}
				}
			}
			else
#endif /* POSIX */
			sig_list(0);
			return(0);
		}
		else if(*a1=='s')
		{
			com++;
			a1 = com[1];
		}
		flag = sig_number(a1);
		if(flag <0 || flag >= NSIG)
			sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
		com++;
	}
	if(*++com==0)
		sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
        if(strcmp(*com,"--")==0)
                ++com;
	if(job_walk(job_kill,flag,com))
		sh.exitval = 2;
	return(sh.exitval);
}
#endif

#ifdef LDYNAMIC
#   ifdef apollo
	/*
	 *  Apollo system support library loads into the virtual address space
	 */

	int	b_inlib(argn,com)
	char **com;
	{
		register char *a1 = com[1];
		int status;
		short len;
		std_$call void loader_$inlib();
		if(!st.subflag && a1)
		{
			len = strlen(a1);
			loader_$inlib(*a1, len, status);
			if(status!=0)
				sh_fail(a1, gettxt(_SGI_DMMX_e_badinlib,e_badinlib),ERROR);
		}
		return(0);
	}
#else
	/*
	 * dynamic library loader from Ted Kowalski
	 */

	int	b_inlib(argn,com)
	char **com;
	{
		register char *a1;
		if(!st.subflag)
		{
			ldinit();
			addfunc(ldname("nam_putval", 0), (int (*)())nam_putval);
			addfunc(ldname("nam_strval", 0), (int (*)())nam_strval);
			addfunc(ldname("p_setout", 0), (int (*)())p_setout);
			addfunc(ldname("p_str", 0), (int (*)())p_str);
			addfunc(ldname("p_flush", 0), (int (*)())p_flush);
			while(a1= *++com)
			{
				if(!ldfile(a1))
					sh_fail(a1, gettxt(_SGI_DMMX_e_badinlib,e_badinlib),ERROR);
			}
			loadend();
			if(undefined()!=0)
				sh_cfail(gettxt(_SGI_DMMX_e_undefsym,e_undefsym));
		}
	}
	/*
	 * bind a built-in name to the function that implements it
	 * uses Ted Kowalski's run-time loader
	 */
	int	b_builtin(argn,com)
	char **com;
	{
		register struct namnod *np;
		int (*fn)();
		int (*ret_func())();
		if(argn!=3)
			sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
		if(!(np = nam_search(com[1],sh.fun_tree,N_ADD)))
			sh_fail(com[1],gettxt(_SGI_DMMX_e_create,e_create),ERROR);
		if(!isnull(np))
			sh_fail(com[1],gettxt(_SGI_DMMX_is_builtin,is_builtin),ERROR);
		if(!(fn = ret_func(ldname(com[2],0))))
			sh_fail(com[2],gettxt(_SGI_DMMX_e_found,e_found),FNDBAD);
		funptr(np) = fn;
		nam_ontype(np,N_BLTIN);
	}
#   endif	/* !apollo */
#endif /* LDYNAMIC */


#ifdef FS_3D
#   define VLEN		14
    int	b_vpath_map(argn,com)
    char **com;
    {
	register int flag = (com[0][1]=='p'?2:4);
	register char *a1 = com[1];
	char version[VLEN+1];
	char *vend; 
	int n;
	switch(argn)
	{
	case 1:
	case 2:
		flag |= 8;
		p_setout(st.standout);
		if((n = mount(a1,version,flag)) >= 0)
		{
			vend = stakalloc(++n);
			n = mount(a1,vend,flag|(n<<4));
		}
		if(n < 0)
		{
			if(flag==2)
				sh_cfail(gettxt(_SGI_DMMX_e_getmap,e_getmap));
			else
				sh_cfail(gettxt(_SGI_DMMX_e_getvers,e_getvers));
		}
		if(argn==2)
		{
			p_str(vend,NL);
			break;
		}
		n = 0;
		while(flag = *vend++)
		{
			if(flag==' ')
			{
				flag  = e_sptbnl[n+1];
				n = !n;
			}
			p_char(flag);
		}
		if(n)
			newline();
		break;
	default:
		if(!(argn&1))
			sh_cfail(gettxt(_SGI_DMMX_e_nargs,e_nargs));
		/*FALLTHROUGH*/
	case 3:
		if(st.subflag)
			break;
 		for(n=1;n<argn;n+=2)
			if(mount(com[n+1],com[n],flag)<0)
			{
				if(flag==2)
					sh_cfail(gettxt(_SGI_DMMX_e_setmap,e_setmap));
				else
					sh_cfail(gettxt(_SGI_DMMX_e_setvpath,e_setvpath));
			}
	}
	return(0);
    }
#endif /* FS_3D */

#ifdef UNIVERSE
    /*
     * there are two styles of universe 
     * Pyramid universes have <sys/universe.h> file
     * Masscomp universes do not
     */

    int	b_universe(argn,com)
    char **com;
    {
	register char *a1 = com[1];
	if(a1)
	{
		if(setuniverse(univ_number(a1)) < 0)
			sh_cfail(gettxt(_SGI_DMMX_e_invalidnm,e_invalidnm));
		att_univ = (strcmp(a1,"att")==0);
		/* set directory in new universe */
		if(*(a1 = path_pwd(0))=='/')
			chdir(a1);
		/* clear out old tracked alias */
		stakseek(0);
		stakputs((PATHNOD)->namid);
		stakputc('=');
		stakputs(nam_strval(PATHNOD));
		a1 = stakfreeze(1);
		env_namset(a1,sh.var_tree,nam_istype(PATHNOD,~0));
	}
	else
	{
		int flag;
		char universe[TMPSIZ];
		if(getuniverse(universe) < 0)
			sh_cfail(gettxt(_SGI_DMMX_e_no_access,e_no_access));
		else
			p_str(universe,NL);
	}
	return(0);
    }
#endif /* UNIVERSE */


#ifdef SYSSLEEP
    /* fine granularity sleep builtin someday */
    int	b_sleep(argn,com)
    char **com;
    {
	extern double atof();
	register char *a1 = com[1];
	if(a1)
	{
		if(strmatch(a1,"*([0-9])?(.)*([0-9])"))
			sh_delay(atof(a1));
		else
			sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	}
	else
		sh_cfail(gettxt(_SGI_DMMX_e_argexp,e_argexp));
	return(0);
    }
#endif /* SYSSLEEP */

static const char flgchar[] = "efgilmnprstuvxEFHLPRZ";
static const int flgval[] = {E_FLAG,F_FLAG,G_FLAG,I_FLAG,L_FLAG,M_FLAG,
			N_FLAG,P_FLAG,R_FLAG,S_FLAG,T_FLAG,U_FLAG,V_FLAG,
			X_FLAG,N_DOUBLE|N_INTGER|N_EXPNOTE,N_DOUBLE|N_INTGER,
			N_HOST,N_LJUST,H_FLAG,N_RJUST,N_RJUST|N_ZFILL};
/*
 * process option flags for built-ins
 * flagmask are the invalid options
 */

static int flagset(flaglist,flagmask)
char *flaglist;
{
	register int flag = 0;
	register int c;
	register char *cp,*sp;
	int numset = 0;

	for(cp=flaglist+1;c = *cp;cp++)
	{
		if(isdigit(c))
		{
			if(argnum < 0)
			{
				argnum = 0;
				numset = -100;
			}
			else
				numset++;
			argnum = 10*argnum + (c - '0');
		}
		else if(sp=strchr(flgchar,c))
			flag |= flgval[sp-flgchar];
		else if(c!= *flaglist)
			goto badoption;
	}
	if(numset>0 && flag==0)
		goto badoption;
	if((flag&flagmask)==0)
		return(flag);
badoption:
	sh_cfail(gettxt(_SGI_DMMX_e_option,e_option));
	/* NOTREACHED */
}

/*
 * process command line options and store into newflag
 */

static int scanargs(com,flags)
char *com[];
{
	register char **argv = ++com;
	register int flag;
	register char *a1;
	newflag = 0;
	if(*argv)
		aflag = **argv;
	else
		aflag = 0;
	if(aflag!='+' && aflag!='-')
		return(0);
	while((a1 = *argv) && *a1==aflag)
	{
		if(a1[1] && a1[1]!=aflag)
			flag = flagset(a1,flags);
		else
			flag = 0;
		argv++;
		if(flag==0)
			break;
		newflag |= flag;
	}
	return(argv-com);
}

/*
 * evaluate the string <s> or the contents of file <un.fd> as shell script
 * If <s> is not null, un is interpreted as an argv[] list instead of a file
 */

void sh_eval(s)
register char *s;
{
	struct fileblk	fb;
	union anynode *t;
	char inbuf[IOBSIZE+1];
	struct ionod *saviotemp = st.iotemp;
	struct slnod *saveslp = st.staklist;
	io_push(&fb);
	if(s)
	{
		io_sopen(s);
		if(sh.un.com)
		{
			fb.feval=sh.un.com;
			if(*fb.feval)
				fb.ftype = F_ISEVAL;
		}
	}
	else if(sh.un.fd>=0)
	{
		io_init(input=sh.un.fd,&fb,inbuf);
	}
	sh.un.com = 0;
	st.exec_flag++;
	t = sh_parse(NL,NLFLG|MTFLG);
	st.exec_flag--;
	if(is_option(READPR)==0)
		st.states &= ~READPR;
	if(s==NULL && hist_ptr)
		hist_flush();
	p_setout(ERRIO);
	io_pop(0);
	sh_exec(t,(int)(st.states&(ERRFLG|MONITOR)));
	sh_freeup();
	st.iotemp = saviotemp;
	st.staklist = saveslp;
}


/*
 * Given the name or number of a signal return the signal number
 */

static int sig_number(string)
register char *string;
{
	register int n;

	if (string == NULL)
		return(-1);
	if(isdigit(*string))
		n = atoi(string);
	else
	{
		ltou(string,string);
		n = sh_lookup(string,sig_names);
		n &= (1<<SIGBITS)-1;
		n--;
	}
	return(n);
}

#ifdef JOBS
/*
 * list all the possible signals
 * If flag is 1, then the current trap settings are displayed
 */
static void sig_list(flag)
{
	register const struct sysnod	*syscan;
	register int n = MAXTRAP;
	const char *names[MAXTRAP+3];
	const char *etrap = gettxt(_SGI_DMMX_e_trap,e_trap);
	syscan=sig_names;
	p_setout(st.standout);
	/* not all signals may be defined */
#ifdef POSIX
	if(flag<0)
		n += 2;
#else
	NOT_USED(flag);
#endif /* POSIX */
	while(--n >= 0)
		names[n] = etrap;
	while(*syscan->sysnam)
	{
		n = syscan->sysval;
		n &= ((1<<SIGBITS)-1);
		names[n] = syscan->sysnam;
		syscan++;
	}
	n = MAXTRAP-1;
#ifdef POSIX
	if(flag<0)
		n += 2;
#endif /* POSIX */
	while(names[--n]==etrap);
	names[n+1] = NULL;
#ifdef POSIX
	if(flag<0)
	{
		while(--n >= 0)
		{
			if(st.trapcom[n])
			{
				p_str(gettxt(_SGI_DMMX_e_trap,e_trap),' ');
				p_qstr(st.trapcom[n],' ');
				p_str(names[n+1],NL);
			}
		}
	}
	else if(flag)
	{
		if(flag <= n && names[flag])
			p_str(names[flag],NL);
		else
			p_num(flag-1,NL);
	}
	else
#endif /* POSIX */
		p_list(n-1,(char**)(names+2));
}
#endif	/* JOBS */

#ifdef SYSSLEEP
/*
 * delay execution for time <t>
 */

#ifdef _poll_
#   include	<poll.h>
#endif /* _poll_ */
#ifndef TIC_SEC
#   ifdef HZ
#	define TIC_SEC	HZ	/* number of ticks per second */
#   else
#	define TIC_SEC	60	/* number of ticks per second */
#   endif /* HZ */
#endif /* TIC_SEC */


int	sh_delay(t)
double t;
{
	register int n = t;
#ifdef _poll_
	struct pollfd fd;
	if(t<=0)
		return;
	else if(n > 30)
	{
		sleep(n);
		t -= n;
	}
	if(n=1000*t)
		poll(&fd,0,n);
#else
#   ifdef _SELECT5_
	struct timeval timeloc;
	if(t<=0)
		return;
	timeloc.tv_sec = n;
	timeloc.tv_usec = 1000000*(t-(double)n);
	select(0,(fd_set*)0,(fd_set*)0,(fd_set*)0,&timeloc);
#   else
#	ifdef _SELECT4_
		/* for 9th edition machines */
		if(t<=0)
			return;
		if(n > 30)
		{
			sleep(n);
			t -= n;
		}
		if(n=1000*t)
			select(0,(fd_set*)0,(fd_set*)0,n);
#	else
		struct tms tt;
		if(t<=0)
			return;
		sleep(n);
		t -= n;
		if(t)
		{
			clock_t begin = times(&tt);
			if(begin==0)
				return;
			t *= TIC_SEC;
			n += (t+.5);
			while((times(&tt)-begin) < n);
		}
#	endif /* _SELECT4_ */
#   endif /* _SELECT5_ */
#endif /* _poll_ */
}
#endif /* SYSSLEEP */

#ifdef UNIVERSE
#   ifdef _sys_universe_
	int univ_number(str)
	char *str;
	{
		register int n = 0;
		while( n < NUMUNIV)
		{
			if(strcmp(str,univ_name[n++])==0)
				return(n);
		}
		return(-1);
	}
#   endif /* _sys_universe_ */
#endif /* UNIVERSE */

static void
limtail(cp, str0)
	char *cp, *str0;
{
	register char *str = str0;

	while (*cp && *cp == *str)
		cp++, str++;
	if (*cp)
		sh_cfail(gettxt(_SGI_DMMX_e_badscale,e_badscale));
}

static const struct sys2msgnod *
findlim(cp)
char *cp;
{
	register const struct sys2msgnod *sp, *res;

	res = 0;
	for (sp = blimit_names; sp->sysmsgid != NULLSTR; sp++)
		if (prefix(cp, sp->sysnam)) {
			if (res)
				sh_cfail(gettxt(_SGI_DMMX_e_ambiguous,e_ambiguous));
			res = sp;
		}
	if (res)
		return (res);
	sh_cfail(gettxt(_SGI_DMMX_e_nosuchres,e_nosuchres));

	/*NOTREACHED*/
}

static long long
getval(sp, v)
register const struct sys2msgnod *sp;
char **v;
{
	register double f;
	char *cp = *v++;
	extern double atof();
	int unit,limit;

	limit = sp->sysval>>11;
	unit = sp->sysval&0x7ff;
	f = atof((const char *)cp);
	while (isdigit(*cp) || *cp == '.' || 
			       *cp == 'e' || 
			       *cp == 'E' ||
			       *cp == '-' ||
			       *cp == '+' )
		cp++;
	if (*cp == 0) {
		if (*v == 0)
			return ((long long)(f+0.5) * unit);
		cp = *v;
	}
	switch (*cp) {

	case ':':
		if (limit != RLIMIT_CPU)
			goto badscal;
		return ((long long)(f * 60.0 + atof((const char *)cp+1)));

	case 'h':
		if (limit != RLIMIT_CPU)
			goto badscal;
		limtail(cp, gettxt(_SGI_DMMX_sc_hours,sc_hours));
		f *= 3600.;
		break;

	case 'm':
		if (limit == RLIMIT_CPU) {
			limtail(cp, gettxt(_SGI_DMMX_sc_minutes,sc_minutes));
			f *= 60.;
			break;
		}
	case 'M':
		if (limit == RLIMIT_CPU)
			goto badscal;
		*cp = 'm';
		limtail(cp, gettxt(_SGI_DMMX_sc_megabytes,sc_megabytes));
		f *= 1024.*1024.;
		break;

	case 's':
		if (limit != RLIMIT_CPU)
			goto badscal;
		limtail(cp, gettxt(_SGI_DMMX_sc_seconds,sc_seconds));
		break;

	case 'k':
		if (limit == RLIMIT_CPU)
			goto badscal;
		limtail(cp, gettxt(_SGI_DMMX_sc_kbytes,sc_kbytes));
		f *= 1024;
		break;

	case 'u':
		limtail(cp, gettxt(_SGI_DMMX_e_unlimited,e_unlimited));
		return (RLIM64_INFINITY);

	default:
badscal:
		sh_cfail(gettxt(_SGI_DMMX_e_badscale,e_badscale));
	}
	return ((long long)(f+0.5));
}

void static
psecs(__int64_t l)
{
	register __int64_t i;

	i = l / 3600;
	if (i) {
		p_str(u64tos((unsigned long long)i,10),':');
		i = l % 3600;
		p_str(u64tos((unsigned long long)(i/60),10),0);
		goto minsec;
	}
	i = l;
	p_str(u64tos((unsigned long long)(i/60),10),0);
minsec:
	i %= 60;
	p_char(':');
	p_str(u64tos((unsigned long long)i,10),0);
}

static void
plim(register const struct sys2msgnod *sp, char hard)
{
	struct rlimit64 rlim;
	rlim64_t limit;
	int unit,rlimit;

	rlimit = sp->sysval>>11;
	unit = sp->sysval&0x7ff;

	p_setout(st.standout);
	p_str(gettxt(sp->sysmsgid,sp->sysnam),0);
	if(getrlimit64(rlimit, &rlim) < 0)
		sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	limit = hard ? rlim.rlim_max : rlim.rlim_cur;
	if (limit == RLIM64_INFINITY)
		p_str(gettxt(_SGI_DMMX_e_unlimited,e_unlimited),0);
	else if (rlimit == RLIMIT_CPU)
		psecs(limit);
	else {
		limit = (limit+unit-1)/unit;
		p_str(u64tos((unsigned long long)limit,10),SP);
		if(strcmp(sp->sys2msgid,""))
			p_str(gettxt(sp->sys2msgid,sp->sys2nam),0);
	}
	p_char(NL);
}

static
setlim(register const struct sys2msgnod *sp, char hard, rlim64_t limit)
{
	struct rlimit64 rlim;
	int rlimit;

	rlimit = sp->sysval>>11;
	if(getrlimit64(rlimit, &rlim) < 0)
		sh_cfail(gettxt(_SGI_DMMX_e_number,e_number));
	if (hard)
		rlim.rlim_max = limit;
  	else if (limit == RLIM64_INFINITY && geteuid() != 0)
 		rlim.rlim_cur = rlim.rlim_max;
 	else
 		rlim.rlim_cur = limit;
	if (setrlimit64(rlimit, &rlim) < 0) {
		p_setout(ERRIO);
		p_str(gettxt(sp->sysmsgid,sp->sysnam),0);
		p_str(gettxt(_SGI_DMMX_e_cant,e_cant),0);
		if (limit == RLIM64_INFINITY)
			p_str(gettxt(_SGI_DMMX_e_remove,e_remove),0);
		else
			p_str(gettxt(_SGI_DMMX_e_set,e_set),0);
		if (hard)
			p_str(gettxt(_SGI_DMMX_e_hard,e_hard),0);
		p_str(gettxt(_SGI_DMMX_e_limit,e_limit),NL);
		return (-1);
	}
	return (0);
}

static
prefix(sub, str)
	register char *sub, *str;
{
	for (;;) {
		if (*sub == 0)
			return (1);
		if (*str == 0)
			return (0);
		if (*sub++ != *str++)
			return (0);
	}
}

/* Bourne shell limit() and unlimit() 
 * builtins added for backward compatability 
 */

b_limit(argn,v)
register char **v;
{
	const struct sys2msgnod *sp;
	register rlim64_t limit;
	char hard = 0;

	v++;
	if (*v && eq(*v, "-h")) {
		hard = 1;
		v++;
	}
	if (*v == 0) {
		for (sp = blimit_names; sp->sysmsgid != NULLSTR; sp++)
			plim(sp, hard);
		return(0);
	}
	sp = findlim(v[0]);
	if (v[1] == 0) {
		plim(sp,  hard);
		return(0);
	}
	limit = getval(sp, v+1);
	if (setlim(sp, hard, limit) < 0)
		sh_cfail(gettxt(_SGI_DMMX_e_ulimit,e_ulimit));
	return(0);
}

b_unlimit(argn,v)
register char **v;
{
	const struct sys2msgnod *sp;
	int err = 0;
	char hard = 0;

	v++;
	if (*v && eq(*v, "-h")) {
		hard = 1;
		v++;
	}
	if (*v == 0) {
		for (sp = blimit_names; sp->sysmsgid != NULLSTR ; sp++)
			if (setlim(sp, hard, (rlim64_t)RLIM64_INFINITY) < 0)
				err++;
		if (err)
			sh_cfail(gettxt(_SGI_DMMX_e_ulimit,e_ulimit));
		return(0);
	}
	while (*v) {
		sp = findlim(*v++);
		if (setlim(sp, hard, (rlim64_t)RLIM64_INFINITY) < 0)
			sh_cfail(gettxt(_SGI_DMMX_e_ulimit,e_ulimit));
	}
	return(0);
}
