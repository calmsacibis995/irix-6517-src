/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:sh/xec.c	1.6.4.2"

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
#include	"test.h"
#include	"builtins.h"

#ifdef _sys_timeb_
#   ifndef TIC_SEC
#	define TIC_SEC		60
#   endif /* TIC_SEC */
#   include	<sys/timeb.h>
#endif	/* _sys_timeb_ */


/* These routines are referenced by this module */
#ifdef DEVFD
    extern void	close_pipes();
#endif	/* DEVFD */
#ifdef VFORK
    extern int	vfork_check();
    extern int	vfork_save();
    extern void	vfork_restore();
#endif	/* VFORK */

static int trim_eq();
static char *word_trim();
static char locbuf[TMPSIZ]; 	/* store last argument here if it fits */
static char pipeflag;

/* ========	command execution	========*/

sh_exec(argt, flags)
union anynode 	*argt;
int flags;
{
	register union anynode *t;
	register int 	type = flags;
	char 	*sav=stakptr(0);
	int cmdflg=0;
	int errorflg = (type&ERRFLG);
	int execflg = (type&1);
	int lastpipe = (type&LASTPIPE);
	int mainloop = (type&PROMPT);
	unsigned save_states = (st.states&(ERRFLG|MONITOR|LASTPIPE));
#ifdef VFORK
	int v_fork;
#endif	/* VFORK */
	if(sh.trapnote&SIGSET)
		sh_exit(SIGFAIL);
	flags &= ~(LASTPIPE|PROMPT);
	st.states &= ~(ERRFLG|MONITOR);
	st.states |= ((save_states&flags)|lastpipe);
	if((t=argt) && st.execbrk==0 && !is_option(NOEXEC))
	{
		char	**com;
		int	argn;
		char *com0 = NIL;
		pid_t forkpid;
		jmp_buf retbuf;
		jmp_buf *savreturn;
		int save;
		int topfd;
		struct fileblk *savio;
		int skipexitset = 0;
		int jmpval;
		if(lastpipe)
		{
			/* save state for last element of a pipe */
			topfd = sh.topfd;
			forkpid = job.parent;
			if(io_ftable[0])
				save = io_ftable[0]->flag;
			savreturn = sh.freturn;
			sh.freturn = (jmp_buf*)retbuf;
			save_states |= (st.states&PROMPT);
			st.states &= ~PROMPT;
			savio = sh.savio;
			sh.savio = st.standin;
			jmpval =SETJMP(retbuf);
			if(jmpval)
				goto endpipe;
			if(!execflg)
				io_save(0,topfd);
			io_renumber(sh.inpipe[0],0);
		}
		type = t->tre.tretyp;
		if(!sh.intrap)
			sh.oldexit=sh.exitval;
		sh.exitval=0;
		sh.lastpath = 0;
		switch(type&COMMSK)
		{
			case TCOM:
			{
				register struct argnod	*argp;
				register struct namnod* np;
				struct ionod	*io;
				st.cmdline = t->com.comline;
				com = arg_build(&argn,&(t->com));
				if(t->tre.tretyp&COMSCAN)
				{
					argp = t->com.comarg;
					if(argp && *com && !(argp->argflag&A_RAW))
					if(sh.trapnote&SIGSET)
						sh_exit(SIGFAIL);
				}
				if(!(np = t->com.comnamp)&& com[0])
					np = nam_search(com[0],sh.fun_tree,0);
				sh.cmdname = com0 = com[0];
				io = t->tre.treio;
				if(argp = t->com.comset)
				{
					if(argn==0 || (!st.subflag && np && 
						nam_istype(np,BLT_SPC) &&
						!(st.states&CMD_SPC)))
					{
						env_setlist(argp, 0);
						argp = NULL;
					}
				}
				if((io||argn))
				{
					if(argn==0)
					{
						/* fake true built-in */
						argn=1;
						np = SYSTRUE;
						com = &np->namid;
					}
					/* set +x doesn't echo */
					else if((np!=SYSSET) && is_option(EXECPR))
						sh_trace(com,1);
					if(io)
						p_flush();
					/* check for builtins */
					if(np && is_abuiltin(np))
					{
						/* failures on built-ins not fatal */
						jmp_buf retbuf;
						jmp_buf *savreturn = sh.freturn;
						struct fileblk *saveio=sh.savio;
						int indx = sh.topfd;
						int scope=0;
						int jmpval;
						int ismon = (st.states&MONITOR);
						if(st.subflag || !nam_istype(np,BLT_SPC) ||
						   st.states&CMD_SPC)
							sh.freturn = (jmp_buf*)retbuf;
						sh.savio = st.standin;
						jmpval = SETJMP(retbuf);
						if(jmpval == 0)
						{
							int flag = execflg;
							if(nam_istype(np,(BLT_ENV)))
							{
								if(np==SYSLOGIN)
									flag=1;
								else if(np==SYSEXEC)
									flag=1+(com[1]==0);
							}
							else if(!nam_istype(np,BLT_SPC) ||
								st.states&CMD_SPC)
								st.states |= BUILTIN;
							if(io)
								indx = io_redirect(io,flag);
							if(argp)
							{
								nam_scope(argp);
								scope++;
							}
							sh.exitval = (*funptr(np))(argn,com,execflg);
						}
						sh.savio = saveio;
						st.states &= ~BUILTIN;
						st.states |= ismon;
						sh.freturn = savreturn;
						if(scope)
							nam_unscope();
						if(io)
							io_restore(indx);
						if(jmpval>1)
							LONGJMP(*sh.freturn,jmpval);
						goto setexit;
					}
					/* check for functions */
				dofunction:
					if(np && !(st.states&NOFUNC))
					{
						int indx;
						struct slnod *slp;
						if(np->value.namval.ip==0)
						{
							if(!nam_istype(np,N_FUNCTION))
								goto dosearch;
							path_search(com0,0);
							if(np->value.namval.ip==0)
								sh_cfail(gettxt(_SGI_DMMX_e_found,e_found));
						}
						/* increase refcnt for unset */
						slp = (struct slnod*)np->value.namenv;
						sh_funstaks(slp->slchild,1);
						staklink(slp->slptr);
						indx = io_redirect(io,execflg);
						sh_funct((union anynode*)(funtree(np))
							,com
							,flags|(int)(nam_istype(np,T_FLAG)?EXECPR:0)
							,t->com.comset);
						io_restore(indx);
						sh_funstaks(slp->slchild,-1);
						stakdelete(slp->slptr);
						goto setexit;
					}
					if(strchr(com0,'/')==0)
					{
					dosearch:
						if(path_search(com0,1))
						{
							np=nam_search(com0,sh.fun_tree,0);
							goto dofunction;
						}
					}
				}
				else if(io==0)
				{
				setexit:
					sh.exitval &= EXITMASK;
					exitset();
					if(sh.trapnote || (sh.exitval && (st.states&ERRFLG)))
						sh_chktrap();
					break;
				}
				type = TCOM;
			}
			case TFORK:
			{
				register pid_t parent;
				int no_fork;
				int pipes[2];
				io_sync();
				no_fork = (execflg && !(type&(FAMP|FPOU)) &&
					!st.trapcom[0] && !st.trapcom[ERRTRAP]);
				if(no_fork)
					job.parent=parent=0;
				else
				/* FORKLIM is the max period between forks -
					power of 2 usually.  Currently shell tries after
					2,4,8,16, and 32 seconds and then quits */
				{
					register unsigned forkcnt=1;
					if(!job_slot())	
						sh_fail(gettxt(_SGI_DMMX_e_jobs,e_jobs),NIL,ERROR);
					if(type&FTMP)
					{
						io_linkdoc(st.iotemp);
						st.linked = 1;
					}
					if(type&FCOOP)
					/* set up pipe for cooperating process */
					{
						if(sh.cpid)
							sh_fail(gettxt(_SGI_DMMX_e_pexists,e_pexists),NIL,ERROR);
						if(sh.cpipe[0]<=0 || sh.cpipe[1] <= 0)
						{
							/* first co-process */
							io_popen(pipes);
							sh.cpipe[0] = io_renumber(pipes[0],CINPIPE);
							sh.cpipe[1] = io_renumber(pipes[1],CINPIPE2);
						}
						sh.outpipe = sh.cpipe;
						io_popen(sh.inpipe=pipes);
						sh.inpipe[1] = io_renumber(sh.inpipe[1],COTPIPE);
						if(!(st.states&PROFILE))
							st.states |= is_option(MONITOR);
					}
					/* async commands allow job control */
					if((type&FAMP) && !(type&(FCOMSUB|FPOU))
						&&!(st.states&PROFILE))
						st.states |= is_option(MONITOR);
					nam_strval(RANDNOD);
#ifdef VFORK
					if(v_fork=vfork_check(t))
						vfork_save();
					while((job.parent=parent=(v_fork?vfork():fork())) == -1)
#else
					while((job.parent=parent=fork()) == -1)
#endif	/* VFORK */
					{
						if((forkcnt *= 2) > FORKLIM)
						{
							register const char *msg;
							switch(errno)
							{
							case ENOMEM:
								msg = gettxt(_SGI_DMMX_e_swap,e_swap);
								break;
							default:
							case EAGAIN:
								msg = gettxt(_SGI_DMMX_e_fork,e_fork);
								break;
							}
							sh_fail(msg,NIL,ERROR);
						}
						if(sh.trapnote&SIGSET)
							sh_exit(SIGFAIL);
						mysleep(forkcnt);
						job_wait((pid_t)1);
					}
				}
#ifdef SIGTSTP
				if(!job.jobcontrol)
#endif /* SIGTSTP */
				{
					/* disable foreground job control */
					if(!(type&FAMP))
						job_nonstop();
#   ifdef DEVFD
					else if(!(type&FINT))
						job_nonstop();
#   endif /* DEVFD */
				}
				if(flags&MONITOR)
					job.pipeflag = pipeflag;
				else
					job.pipeflag = 0;
				if(parent)
				/* This is the parent branch of fork
				 * It may or may not wait for the child
				 */
				{
					register int pno;
#ifdef VFORK
					if(v_fork)
						vfork_restore();
#endif	/* VFORK */
					forkpid = parent;
#ifdef JOBS
					if(st.states&MONITOR)
					{
						pno = (pipeflag?job.curpgid:parent);
						/*
						 * errno==EPERM means that an
						 * earlier processes completed.
						 * Make parent the job group id
						 */
						if(setpgid(parent,pno)<0 && errno==EPERM)
							setpgid(parent,parent);
					}
					/* first process defines process gid */
					if(!job.pipeflag)
						job.curpgid = parent;
#endif /* JOBS */
					if(lastpipe && (st.states&MONITOR))
					{
						io_restore(topfd);
						forkpid = 0;
						execflg++;
					}
					if(type&FPCL)
						close(sh.inpipe[0]);
					if(type&FCOOP)
						sh.cpid = parent;
					else if(type&FCOMSUB)
						sh.subpid = parent;
					pno = job_post(parent);
					if(lastpipe)
						pipeflag = 0;
					if((type&(FAMP|FPOU))==0)
					{
#ifdef DEVFD
						close_pipes();
#endif	/* DEVFD */
						job_wait(parent);
						job.curpgid = 0;
						/* invalidate tty state */
						tty_set(-1);
					}
					if(type&FAMP)
					{
						sh.bckpid = parent;
						job.curpgid = 0;
						if(st.states&(PROFILE|PROMPT))
						{
							/* print job number */
							p_setout(ERRIO);
#ifdef JOBS
							p_sub(pno,'\t');
#endif /* JOBS */
							p_num(parent,NL);
						}
					}
					sh_chktrap();
					break;
				}
				else
				/*
				 * this is the FORKED branch (child) of execute
				 */
				{
					register pid_t pid = getpid();
					if(st.standout != 1)
						st.standout = io_renumber(st.standout,1);
					st.states |= (FORKED|NOLOG);
					st.states &= ~(BUILTIN|LASTPIPE);
					st.fn_depth = 0;
					if(no_fork==0)
					{
						sh.login_sh = 0;
						st.states &= ~(RM_TMP|IS_TMP);
						io_settemp(pid);
						if(st.linked == 1)
						{
							io_swapdoc(st.iotemp);
							st.linked = 2;
						}
						else
							st.iotemp=0;
						job.waitsafe = 0;
					}
					else if(st.states&RM_TMP)
						rm_files(io_tmpname);
#ifdef ACCT
					suspacct();
#endif	/* ACCT */
					/* child should not unlink the tmpfile */
					/* Turn off INTR and QUIT if `FINT'  */
					/* Reset remaining signals to parent */
					/* except for those `lost' by trap   */
#ifdef VFORK
					sig_reset(1);
#else
					sig_reset(0);
					job.pwlist = 0;	/* don't try to cleanup
						* parent's jobs...  ends up causing problems
						* with data structures in scripts that do
						* lots of forks, without this.  VFORK code
						* did it in vfork.c */
#endif /* VFORK */
#ifdef JOBS
					if(st.states&MONITOR)
					{
						int pgrp;
						if(job.pipeflag==0)
							pgrp = pid;
						else
							pgrp = job.curpgid;
						while(setpgid(0,pgrp)<0 && pgrp!=pid)
							job.curpgid=pgrp=pid;
						if(!job.jobcontrol&&(type&FINT))
							goto closein;
#   ifdef SIGTSTP
						if(!(type&(FAMP|FPOU)))
							tcsetpgrp(JOBTTY,pgrp);
						signal(SIGTTIN,SIG_DFL);
						signal(SIGTTOU,SIG_DFL);
						signal(SIGTSTP,SIG_DFL);
#   endif /* SIGTSTP */
					}
					else if(type&FINT)
#else
					if(type&FINT)
#endif /* JOBS */
					{
						/* default std input for & */
						signal(SIGINT,SIG_IGN);
						signal(SIGQUIT,SIG_IGN);
					closein:
						if(!st.ioset)
						{
							close(0);
							io_fopen(e_devnull);
						}
					}
					/* pipe in or out */
					if((type&FAMP) && is_option(BGNICE))
						nice(4);
					if(type&FPIN)
					{
						io_renumber(sh.inpipe[0],0);
						if((type&FPOU)==0||(type&FCOOP))
							close(sh.inpipe[1]);
					}
					if(type&FPOU)
					{
						io_renumber(sh.outpipe[1],1);
						io_pclose(sh.outpipe);
					}
					st.states &= ~MONITOR;
#ifdef JOBS
					job.jobcontrol = 0;
#endif /* JOBS */
					pipeflag = 0;
					if(type!=TCOM)
						st.cmdline = t->fork.forkline;
					io_redirect(t->tre.treio,1);
					if(type!=TCOM)
					{
						/* don't clear job table for out
						   pipes so that jobs can be
						   piped
						 */
						if(no_fork==0 && (type&FPOU)==0)
							job_clear();
						sh_exec(t->fork.forktre,flags|1);
					}
					else if(com0!=ENDARGS)
					{
						off_option(ERRFLG);
						sh_freeup();
						path_exec(com,t->com.comset);
					}
					sh_done(0);
				}
			}

			case TSETIO:
			{
			/*
			 * don't create a new process, just
			 * save and restore io-streams
			 */
				int indx;
				st.cmdline = t->fork.forkline;
				indx = io_redirect(t->fork.forkio,execflg);
				sh_exec(t->fork.forktre,flags);
				io_restore(indx);
				break;
			}

			case TPAR:
				sh_exec(t->par.partre,flags);
				sh_done(0);
	
			case TFIL:
			{
			/*
			 * This code sets up a pipe.
			 * All elements of the pipe are started by the parent.
			 * The last element executes in current environment
			 */
				register union anynode *tf;
				int pvo[2];	/* old pipe for multi-pipeline */
				int pvn[2];	/* set up pipe */
				type = 1;
				pipeflag = 0;
				sh.inpipe = pvo;
				sh.outpipe = pvn;
				do
				{
					/* create the pipe */
					io_popen(pvn);
					tf = t->lst.lstlef;
					/* type==0 on multi-stage pipe */
					if(type==0)
						tf->fork.forktyp |= FPCL|FPIN;
					/* execute out part of pipe no wait */
					type = sh_exec(tf, MONITOR|errorflg);
					tf = t->lst.lstrit;
					t = tf->fork.forktre;
					/* save the pipe stream-ids */
					pvo[0] = pvn[0];
					/* close out-part of pipe */
					close(pvn[1]);
					/* pipeline all in one process group */
					pipeflag++;
				}
				/* repeat until end of pipeline */
				while(!type && !tf->fork.forkio && t->tre.tretyp==TFIL);
				sh.inpipe = pvn;
				sh.outpipe = 0;
				if(type == 0)
					/*
					 * execute last element of pipeline
					 * in the current process
					 */
					sh_exec(tf->fork.forktre,flags|(LASTPIPE));
				else
					/* execution failure, close pipe */
					io_pclose(pvn);
				break;
			}
	
			case TLST:
			{
				/*  a list of commands are executed here */
				do
				{
					sh_exec(t->lst.lstlef,errorflg);
					t = t->lst.lstrit;
				}
				while(t->tre.tretyp == TLST);
				sh_exec(t,flags);
				break;
			}
	
			case TAND:
				if(type&TTEST)
					skipexitset++;
				if(sh_exec(t->lst.lstlef,0)==0)
					sh_exec(t->lst.lstrit,flags);
				break;
	
			case TORF:
				if(type&TTEST)
					skipexitset++;
				if(sh_exec(t->lst.lstlef,0)!=0)
					sh_exec(t->lst.lstrit,flags);
				break;
	
			case TFOR: /* for and select */
			{
				register char **args;
				register int nargs;
				struct namnod *n;
				char **arglist;
				struct dolnod	*argsav=NULL;
				struct comnod	*tp;
				char *nullptr = NULL;
				int refresh = 1;
				char *cp;
				if((tp=t->for_.forlst)==NULL)
				{
					args=st.dolv+1;
					nargs = st.dolc;
					argsav=arg_use();
				}
				else
				{
					args=arg_build(&argn,tp);
					nargs = argn;
				}
				n = env_namset(t->for_.fornam, sh.var_tree,P_FLAG|V_FLAG);
				st.loopcnt++;
				while(*args !=ENDARGS && st.execbrk == 0)
				{
					if(t->tre.tretyp==TSELECT)
					{
						char *val;
						/* reuse register */
#define c	type
						if(refresh)
						{
							p_setout(ERRIO);
							p_list(nargs,arglist=args);
							refresh = 0;
						}
						sh_prompt(1);
						env_readline(&nullptr,0,R_FLAG);
						if(fiseof(io_ftable[0]))
						{
							sh.exitval = 1;
							clearerr(io_ftable[0]);
							break;
						}
						if((val=nam_fstrval(REPLYNOD))==NULL)
							continue;
						else
						{
							if(*(cp=val) == 0)
							{
								refresh++;
								goto checkbrk;
							}
							while(c = *cp++)
							if(c < '0' && c > '9')
								break;
							if(c!=0)
								c = nargs;
							else
								c = atoi(val)-1;
							if(c<0 || c >= nargs)
							{
								cp = (char*)e_nullstr;
								args = &cp;
							}
							else
								args += c;
						}
					}
#undef c
					nam_putval(n, *args);
					sh_exec(t->for_.fortre,errorflg);
					if(t->tre.tretyp == TSELECT)
					{
						if((cp=nam_fstrval(REPLYNOD)) && *cp==0)
							refresh++;
						args = arglist;
					}
					else
						args++;
				checkbrk:
					if(st.breakcnt<0)
						st.execbrk = (++st.breakcnt !=0);
					}
				if(st.breakcnt>0)
					st.execbrk = (--st.breakcnt !=0);
				st.loopcnt--;
				arg_free(argsav,0);
				break;
			}
	
			case TWH: /* while and until */
			{
				register int 	i=0;
				st.loopcnt++;
				while(st.execbrk==0 && (sh_exec(t->wh.whtre,0)==0)==(type==TWH))
				{
					i = sh_exec(t->wh.dotre,errorflg);
					if(st.breakcnt<0)
						st.execbrk = (++st.breakcnt !=0);
				}
				if(st.breakcnt>0)
					st.execbrk = (--st.breakcnt !=0);
				st.loopcnt--;
				sh.exitval= i;
				break;
			}
	
			case TARITH: /* (( expression )) */
			{
				static char *arg[4]=  {"((", 0, "))"};
				if(!(t->ar.arexpr->argflag&A_RAW))
					arg[1] = word_trim(t->ar.arexpr,0);
				else
					arg[1] = t->ar.arexpr->argval;
				st.cmdline = t->ar.arline;
				if(is_option(EXECPR))
					sh_trace(arg,1);
				sh.exitval = !sh_arith(arg[1]);
				break;
			}

			case TIF:
				if(sh_exec(t->if_.iftre,0)==0)
					sh_exec(t->if_.thtre,flags);
				else if(t->if_.eltre)
					sh_exec(t->if_.eltre, flags);
				else
					sh.exitval=0; /* force zero exit for if-then-fi */
				break;
	
			case TSW:
			{
				char *r = word_trim(t->sw.swarg,0);
				t= (union anynode*)(t->sw.swlst);
				while(t)
				{
					register struct argnod	*rex=(struct argnod*)t->reg.regptr;
					while(rex)
					{
						register char *s;
						if(rex->argflag&A_MAC)
							s = mac_expand(rex->argval);
						else
							s = rex->argval;
						type = (rex->argflag&A_RAW);
						if((type && eq(r,s)) ||
							(!type && (strmatch(r,s)
							|| trim_eq(r,s))))
						{
							do	sh_exec(t->reg.regcom,flags);
							while(t->reg.regflag &&
								(t=(union anynode*)t->reg.regnxt));
							t=0;
							break;
						}
						else
							rex=rex->argnxt.ap;
					}
					if(t)
						t=(union anynode*)t->reg.regnxt;
				}
				break;
			}

			case TTIME:
#ifdef POSIX
				if(type!=TTIME)
				{
					sh_exec(t->par.partre,0);
					sh.exitval = !sh.exitval;
					break;
				}
#endif /* POSIX */
			{
				/* time the command */
				struct tms before,after;
				clock_t at,bt;
#ifdef _sys_timeb_
				struct timeb tb,ta;
				ftime(&tb);
#endif	/* _sys_timeb_ */
				bt = times(&before);
				sh_exec(t->par.partre,0);
				at = times(&after);
#ifdef _sys_timeb_
				ftime(&ta);
				at = TIC_SEC*(ta.time-tb.time);
				at +=  ((TIC_SEC*((clock_t)(ta.millitm-tb.millitm)))/1000);
#else
				at -= bt;
#endif	/* _sys_timeb_ */
				p_setout(ERRIO);
				p_str(gettxt(_SGI_DMMX_e_real,e_real),'\t');
				p_time(at,NL);
				p_str(gettxt(_SGI_DMMX_e_user,e_user),'\t');
				at = after.tms_utime - before.tms_utime;
				at += after.tms_cutime - before.tms_cutime;
				p_time(at,NL);
				p_str(gettxt(_SGI_DMMX_e_sys,e_sys),'\t');
				at = after.tms_stime - before.tms_stime;
				at += after.tms_cstime - before.tms_cstime;
				p_time(at,NL);
				break;
			}
			case TPROC:
			{
				register struct namnod *np;
				register char *fname = ((struct procnod*)t)->procnam;
				register struct slnod *slp;
				if(!isalpha(*fname))
					sh_fail(fname,gettxt(_SGI_DMMX_e_ident,e_ident),ERROR);
				np = env_namset(fname,sh.fun_tree,P_FLAG|V_FLAG);
				/* a function name cannot be a built-in yet */
				if(is_abuiltin(np))
				{
					p_setout(ERRIO);
					p_prp(gettxt(_SGI_DMMX_e_restricted,e_restricted));
					p_str(":",' ');
					p_str(fname,0);
					p_str(gettxt(_SGI_DMMX_is_builtin,is_builtin),NL);
					sh.exitval++;
					break;
				}
				if(np->value.namval.rp)
				{
					slp = (struct slnod*)np->value.namenv;
					sh_funstaks(slp->slchild,-1);
					stakdelete(slp->slptr);
				}
				else
					np->value.namval.rp = new_of(struct Ufunction,0);
				if(t->proc.procstak)
				{
					slp = t->proc.procstak;
					sh_funstaks(slp->slchild,1);
					staklink(slp->slptr);
					np->value.namenv = (char*)slp;
					funtree(np) = (int*)(t->proc.proctre);
					np->value.namval.rp->hoffset = t->proc.procloc;
				}
				else
					nam_free(np);
				nam_ontype(np,N_FUNCTION);
				break;
			}

#ifdef NEWTEST
			/* new test compound command */
			case TTST:
			{
				register int n;
				int nargs;
				char *left;
				/* we need the following for execution trace */
				static char arg0[]=  "[[ !";
				static char *arg[6]=  {arg0};
				static char unop[3]= "-?";
				if(type&TTEST)
					skipexitset++;
				if (type&TPAREN)
				{
					sh_exec(t->lst.lstlef,0);
					n = !sh.exitval;
				}
				else
				{
					n = type>>TSHIFT;
					left = word_trim(&(t->lst.lstlef->arg),0);
					if(type&TUNARY)
					{
						nargs = 3;
						unop[1] = n;
						arg[1] = unop;
						arg[2] = left;
						n = unop_test(n,left);
					}
					else if(type&TBINARY)
					{
						nargs = 4;
						arg[1] = left;
						arg[2] = (char*)(test_optable+(n&017)-1)->sysnam;
						arg[3] = word_trim(&(t->lst.lstrit->arg),0);
						n = test_binop(n,left,
							word_trim(&(t->lst.lstrit->arg),(n==TEST_PEQ||n==TEST_PNE)));
					}
					arg[nargs] = "]]";
					arg[nargs+1] = 0;
					if(type&TNEGATE)
						arg[0][2] = ' ';
					else
						arg[0][2] = 0;
					sh_trace(arg,1);
				}
				sh.exitval = (!n^((type&TNEGATE)!=0));
				break;
			}
#endif /* NEWTEST */
		}
		/* set $. */
		if(mainloop && com0)
		{
			if(sh.lastarg!= locbuf && sh.lastarg)
				free(sh.lastarg);
			if(strlen(com[argn-1]) < TMPSIZ)
			{
				nam_ontype(L_ARGNOD,N_FREE);
				sh.lastarg = strcpy(locbuf,com[argn-1]);
			}
			else
			{
				nam_offtype(L_ARGNOD,~N_FREE);
				sh.lastarg = sh_heap(com[argn-1]);
			}
		}
		if(lastpipe)
		{
		endpipe:
			if(!execflg)
			{
				io_restore(topfd);
				type = sh.exitval;
				job_wait(forkpid);
				sh.exitval = type;
			}
			if(io_ftable[0])
				io_ftable[0]->flag = save;
			sh.savio = savio;
			st.states &= ~LASTPIPE;
			sh.freturn = savreturn;
			st.ioset = 0;
			pipeflag = 0;
			sh_chktrap();
			if(jmpval>1)
				LONGJMP(*sh.freturn,jmpval);
		}
		if(!skipexitset)
			exitset();
	}
	if(sav != stakptr(0))
		stakset(sav,0);
	else if(staktell())
		stakseek(0);
	st.states |= save_states;
	st.linked = 0;
	if(sh.trapnote&SIGSET)
		sh_exit(SIGFAIL|sh.exitval);
	return(sh.exitval);
}

/*
 * test for equality with second argument trimmed
 * returns 1 if r == trim(s) otherwise 0
 */

static trim_eq(r,s)
register char *r,*s;
{
	register char c;
	while(c = *s++)
	{
		if(c==ESCAPE)
			c = *s++;
		if(c && c != *r++)
			return(0);
	}
	return(*r==0);
}

/*
 * print out the command line if set -x is on
 */

int sh_trace(com,nl)
char **com;
{
	if(is_option(EXECPR))
	{
		p_setout(ERRIO);
		p_str(mac_try(nam_fstrval(PS4NOD)),0);
		if(com)
			echo_list(1,com);
		if(nl)
			newline();
		return(1);
	}
	return(0);
}


static char *word_trim(arg,flag)
register struct argnod *arg;
{
	register char *r = arg->argval;
	if((arg->argflag&A_RAW))
		return(r);
	if(flag==0)
		return(mac_trim(r,0));
	r = mac_expand(r);
	return(r);
}

