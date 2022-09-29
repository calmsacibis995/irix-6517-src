/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)make:parallel.c	1.3"

#include "defs"
#include <signal.h>
#include <pfmt.h>

FILE *open_block();
static void	print_block();
static SHENV add_shb();
static void arch_name();

extern int	is_off();

static READY_LIST ready_list = NULL ;
void
add_ready(tar)
NAMEBLOCK tar;
{
register READY_LIST p;
READY_LIST slot = NULL;

	if(!tar)return; /* No target to add; */

	if(IS_DONE(tar) || tar->done == D_INIT || IS_RUN(tar))return;

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "add_ready: tar=%s, done=%d\n", tar->namep, tar->done);
#endif


	if (!ready_list)
		p = ready_list = ALLOC(ready_list);
	else
		for( p = ready_list ; ; p = p->nextready){

			if( p->tarp == tar )  /* already in */
				return;

			if(p->tarp == NULL) /* Found empty slot */
				slot = p;

			if( !p->nextready){ /* End of the list */
				if(slot)
					p = slot ;
				else
					p = p->nextready = ALLOC(ready_list);
				break; 
			}
		}

	p->tarp = tar;  /* add the target */
}

ready_to_run(tar, sh_block, dep_line)
NAMEBLOCK tar;
SHBLOCK sh_block;
DEPBLOCK dep_line;
{
	void save_sh_env(), restore_macros();
	void set_in_proc();
	void pwait();

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "ready_to_run: Checking target=%s, done=%d\n",
				tar->namep, tar->done);
#endif


	if(!sh_block)
		fatal1(":141:No sh block tar=%s\n", tar->namep);

	if( tar->done == D_PROC ){ /* The target's dep in process now */

		save_sh_env(tar, sh_block, dep_line);  /* save the env for time to come */

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "ready_to_run: tar=%s IN PROC NOW! \n", tar->namep);
#endif

		tar->done = D_REBUILD;
		add_ready(tar); 		/* the target is ready to run when posible */

		return(NO);
	}

	if( IS_RUN(tar) || tar->done == D_REBUILD ){
		/* more commands to the same target. */

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "ready_to_run: tar=%s ADD MORE CMD \n", tar->namep);
#endif

		save_sh_env(tar, sh_block, dep_line); /* save the new env. */
		
		return(NO);
	}

	if( tar->done == D_NOEVAL || tar->done == D_MUTEX ){
#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "ready_to_run: tar=%s NOEVAL \n", tar->namep);
#endif
		add_ready(tar);
		return(NO);
		
	}

	if( tar->done == D_START ){

		save_sh_env(tar, sh_block, dep_line); /* for next commnds */

		if(nproc >= parallel){
#ifdef MKDEBUG
			if (IS_ON(DBUG))
				fprintf(stdout, "ready_to_run: PARALLEL LIMIT target=%s, done=%d\n",
			 		tar->namep, tar->done);
#endif
			add_ready(tar);
			tar->done = D_REBUILD;
			return(NO);
		}
#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "ready_to_run: OK target=%s, done=%d\n",
			 tar->namep, tar->done);
#endif

		add_ready(tar->backname);
		return(YES);
	}

/* from here only if pwait() did the call of docom() */

	if(tar->done != D_READY && tar->done != D_NEXT)
		fatal1(":142:Bad done tar=%s, done=%d\n", tar->namep, tar->done);

	return(YES);
}

static char *macros[] = { "!", "<", "*", "" };

void
save_sh_env(tar, sh_block, dep_line)
NAMEBLOCK tar;
SHBLOCK sh_block;
DEPBLOCK dep_line;
{
register CHARSTAR s, *p = macros;
MACROL m=NULL ;
SHENV shenvp, add_shb();

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "save_sh_env for: tar=%s, dep=%s\n", tar->namep,
				dep_line ?dep_line->depname->namep:" NO DEP" );
#endif
	
	shenvp = add_shb(tar, sh_block);

	if (shenvp == NULL ){ 

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "target=%s ENV WAS ALREADY SAVED!\n", tar->namep );
#endif

		return;
	}

	shenvp->depp = dep_line;

	while(**p != '\0' ){ /* Save the macros */
		if((s = varptr(*p)->varval.charstar) && *s){/*is this macro set in the env ? */

			if(!shenvp->macrolp) /* The first macro to add */
				m = shenvp->macrolp = ALLOC(macrol); /* add a macro list member */
			else
				m = m->nextmacro = ALLOC(macrol); /*add a macro to the end of chain */

			strcpy(m->mnamep , *p);
			m->mdefp = ck_malloc(strlen(s) + 1);
			strcpy(m->mdefp, s);
		}
		p++;
	}

#ifdef MKDEBUG
	if(m == NULL)
		if (IS_ON(DBUG))
			fprintf(stdout, "save_sh_env for: target=%s NO MACROS TO SAVE\n",
				 tar->namep );
#endif

}
			
void
restore_macros(tar, shenvp)
NAMEBLOCK tar;
SHENV shenvp;
{
MACROL m;
CHAIN	  qchain = NULL;
CHARSTAR mkqlist();
register CHARSTAR *p = macros;
void appendq();
DEPBLOCK depp;

	if(!shenvp)
		fatal1(":143:No shenv to restore tar=%s, done=%d\n",tar->namep, tar->done);
		

	m = shenvp->macrolp ;

#ifdef MKDEBUG
		if (IS_ON(DBUG)){
			if(!m)
				fprintf(stdout, "restore_macros for: target=%s NO MACROS TO RESTORE\n",
					tar->namep );
			else
				fprintf(stdout, "restore_macros for: target=%s\n", tar->namep );
		}
#endif

	while(**p != '\0' )
		setvar(*p++, Nullstr);

	while(m){
			setvar(m->mnamep, m->mdefp);
			m = m->nextmacro ;
	}

	for(depp = shenvp->depp; depp ; depp = depp->nextdep){
#ifdef MKDEBUG
		if (IS_ON(DBUG))
			printf("restore_macros: dep= %s, dep->done=%d tar-time=%d, dep-time=%d\n",
				depp->depname->namep, depp->depname->done, tar->modtime, depp->depname->modtime);
#endif
			
		if(IS_ON(UCBLD) || tar->modtime == -1 ||
			IS_UP(depp->depname) ||
			tar->modtime < depp->depname->modtime )
			appendq((CHAIN) &qchain, depp->depname->namep);
	}

 	setvar("?", mkqlist(qchain) );

	if(tar->alias){
		setvar("@", tar->alias); 
		setvar("%", tar->namep);
	}else
		setvar("@", tar->namep);

}

all_dep_done(tar)
NAMEBLOCK tar;
{
		register LINEBLOCK lp = tar->linep;
		register DEPBLOCK  dp;
		int err=0;

		for ( ; lp; lp = lp->nextline)
			for (dp = lp->depp; dp; dp = dp->nextdep){
				if (!IS_DONE(dp->depname))
					return(NO);

				if(dp->depname->done == D_ERROR)err++;

			}

	return(err?D_ERROR:D_UPDATE);
}

static NAMEBLOCK mutex_name = NULL; 

mutex(tar)
NAMEBLOCK tar;
{
int in_mutex, run_mutex;
void arch_name();
static int check_mutex=1;

	if(check_mutex){
		mutex_name = SRCHNAME(".MUTEX");
		check_mutex=0;
	}

	arch_name(tar); /* is tar a library member? ; if yes add/creat  .MUTEX */

	if (mutex_name){
		register LINEBLOCK lp = mutex_name->linep;
		register DEPBLOCK  dp;

		for ( ; lp; lp = lp->nextline){

			in_mutex=run_mutex=0; /* Mutex has to be on the same line */

			for (dp = lp->depp; dp; dp = dp->nextdep){

				if (dp->depname == tar){
					in_mutex++;
					if(run_mutex)break;
				}else
					if(IS_PROC(dp->depname)){
						run_mutex++;
						if(in_mutex)break;
					}

			}
			if(in_mutex && run_mutex ){

#ifdef MKDEBUG
				if (IS_ON(DBUG)){
					fprintf(stdout,
						"mutex:(YES) target=%s, in_mutex=%d, run_mutex=%d\n",
						tar->namep, in_mutex, run_mutex);
				}
#endif

				return(YES);
			}
		}
	}

#ifdef MKDEBUG
		if (IS_ON(DBUG)){
			fprintf(stdout,
				"mutex:(NO) target=%s, %s in_mutex=%d, run_mutex=%d\n",
				tar->namep, mutex_name?"":"(NO .MUTEX)", in_mutex, run_mutex);
		}
#endif

	return(NO);
}

static READY_LIST next_ready;

void
pwait(wait_only)
int wait_only;  /* if wait_only is 1  wait for all the process to finish */
{
long status;
int pid;
register NAMEBLOCK tar;
extern NAMEBLOCK mainname;
NAMEBLOCK find_tar(), get_ready_tar();
void err_msg();
void restore_macros();
int exit_is_on=0;
time_t tjunk;

	if(!nproc)return;

	while((pid = wait(&status)) != -1 ){
	
		if (!(tar = find_tar(pid)))continue;

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout,
			"pwait: finished target=%s, done=%d, status=%ld, nproc=%d, wait_only=%d\n",
				tar->namep, tar->done, status, nproc, wait_only);
#endif

		nproc--;
		
		if(status != 0 ){
		
			err_msg(status, tar->done == D_RUN_IGN, tar);

			if ( tar->done == D_RUN ){

				tar->done = D_ERROR;

				if ( IS_OFF(KEEPGO))
					exit_is_on++;
				else	++k_error;

			}
		}

		if( wait_only ){
			if(IS_ON(BLOCK))
				print_block(tar);

			continue;
		}


		if(tar->done != D_ERROR ){
			
			tar->done = D_UPDATE;

			while( tar->shenvp->shp && !tar->shenvp->shp->shbp) /* command did not exec. */
				tar->shenvp->shp = tar->shenvp->shp->nextsh;

			while( (tar->shenvp->shp = tar->shenvp->shp->nextsh) ||
					(tar->shenvp = tar->shenvp->nextshenv)){
							/* find the next cmd */

				tar->done = D_NEXT;
				restore_macros(tar, tar->shenvp);
				(void)docom(tar,  tar->shenvp->shp, tar->shenvp->depp);
			
				if(IS_RUN(tar)) /* The command is runing */
					break;
			}
		}

		if( exit_is_on ){
			if(IS_ON(BLOCK) && !IS_RUN(tar))
				print_block(tar);
			continue;
		}

		if(!IS_RUN(tar)){  /* Don't add another NEXT case */
			int dummy;
			if( exists(tar, &dummy) == -1 )
				tar->modtime = PRESTIME();
			add_ready(tar->backname);
			if(IS_ON(BLOCK))
				print_block(tar);
		}

		next_ready = ready_list ; /* next_ready is reset in get_ready_tar() */
															

		while(tar = get_ready_tar()){

			if( tar->done == D_NOEVAL || tar->done == D_MUTEX){
				tar->done = D_DONAME;
				(void)doname(tar, 1, &tjunk);

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "pwait: doname finished target=%s, done=%d, nproc=%d\n",
				tar->namep, tar->done,  nproc);
#endif

			}else{
					
				tar->done = D_READY;

				restore_macros(tar, tar->shenvp);
				(void)docom(tar, tar->shenvp->shp, tar->shenvp->depp);

			}
		}
	}

	if(exit_is_on )
		fatal(0);

	if(mainname->done == D_ERROR)
    pfmt(stdout, MM_ERROR, ":60:`%s' not remade because of errors (bu14)\n", mainname->namep);
}


NAMEBLOCK
get_ready_tar()
{
register READY_LIST p ;
register READY_LIST first=NULL,second=NULL ;
NAMEBLOCK found = NULL;
int ck_mutex = 1;
int look_again=0;
void add_ready();
int done_stat;
int not_done;

	
		if(nproc >= parallel) /* Wait for better time .. */
			return(NULL);
	
try_again:
		not_done=0;

		for( p = next_ready ; p ; p = p->nextready){

			if( p->tarp == NULL ) /* empty slot */
				continue;

			if(IS_RUN( p->tarp )) continue;


			if( IS_DONE(p->tarp) ){
				p->tarp = NULL;
					/*The target on the list was already in update stat */
				continue;
			}

			if( ck_mutex && mutex(p->tarp)){
				continue;
			}

			if( p->tarp->done == D_MUTEX )
				break;

			if(!first && p->tarp->done == D_NOEVAL )
				first=p;

			if(done_stat=all_dep_done(p->tarp)){
				if (done_stat == D_ERROR){
					p->tarp->done = D_ERROR;
					add_ready(p->tarp->backname);
					p->tarp = NULL;
					look_again++; /* if needed look again */
					continue;
				}
					
				if(p->tarp->done == D_PROC || p->tarp->done == D_CHECK){
					p->tarp->done = D_OK;
					add_ready(p->tarp->backname);
					p->tarp = NULL;
					look_again++; /* if needed look again */
					continue;
				}
				if(!second)
					second=p;
			}else
				not_done++;

		}
		if(!p)
			p=first?first:second;

		if(p){
			found = p->tarp ;
			p->tarp = NULL ;
			next_ready = p->nextready;
		}else{
			if( look_again){
				next_ready = ready_list;
				look_again = 0;
				goto try_again;
			}
			if(!nproc && ck_mutex){
				next_ready = ready_list;
				ck_mutex=  0;
				goto try_again; /* last time before quit = make finished */
			}
			if(!nproc && not_done)
				fatal(":144:Internal ERROR: Some dep. didn't built");
		}
		

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "get_ready: tar=%s done=%d\n",
				found ? found->namep:"NO READY TARGET",found?found->done:0);
#endif

		return(found);
}


void
set_in_proc(tar)
NAMEBLOCK tar;
{
register NAMEBLOCK p;

	for(p = tar->backname; p ; p= p->backname){
		if(p->done == D_START)
				p->done = D_PROC; /* set all the backnames to be in PROC stat */
	}
}

static SHENV
add_shb(tar, sh_block)
NAMEBLOCK tar;
SHBLOCK sh_block;
{
register SHENV shenvp = tar->shenvp;

	if( shenvp == NULL){ /* this target was not saved befor */

		shenvp = tar->shenvp = ALLOC(shenv);

	}else{


		for(; ; shenvp = shenvp->nextshenv){ /* Check the chain */

			if( shenvp->shp == sh_block)
				return(NULL); /* this sh_block is already in */

			if(!shenvp->nextshenv) /* Not found */
				break;

		}

		shenvp = shenvp->nextshenv = ALLOC(shenv); /* Not found add a new member */
	}

	shenvp->shp = sh_block;
	return(shenvp);
}


static void
arch_name(tar)
NAMEBLOCK tar;
{
CHARSTAR s;
		NAMEBLOCK bk_name; 
		register LINEBLOCK lp ;
		LINEBLOCK mutex_line=NULL;
		register DEPBLOCK  dp;


		if(!(bk_name = tar->backname))return; /* Can't be a library */

		for (s = tar->namep; !(*s == CNULL || *s == LPAREN); s++)
			;

		if(*s == CNULL) /* Left paren. was not found; No arch. member */
			return;

		for (s++ ; !(*s == CNULL || *s == RPAREN); s++)

		if(*s == CNULL)return; /* Bad syntax ? */


		if (!mutex_name) /* if .MUTEX not exists creat it */
			mutex_name = makename(".MUTEX");

		for (lp = mutex_name->linep; lp; lp = lp->nextline)
			for (dp = lp->depp; dp; dp = dp->nextdep){
				if(dp->depname == tar)
						return; /* it's already there. */

				if(!mutex_line && dp->depname->backname == bk_name) 
					mutex_line= lp;/* found the right depline */
			}
		
		if(!mutex_line){
			if(!mutex_name->linep)
				lp = mutex_name->linep = ALLOC(lineblock);  /* In case of new .MUTEX */
			else{

				for (lp = mutex_name->linep; lp->nextline; lp = lp->nextline) /*look for the end*/
					;

				lp = lp->nextline = ALLOC(lineblock);
			}
		}else
				lp=mutex_line;

		if(!lp->depp)
			dp = lp->depp = ALLOC(depblock); /* In case of NULL .MUTEX */

		else{												/* add library name into the chain */

			for(dp = lp->depp; dp->nextdep; dp = dp->nextdep) /* add in the end*/
				;
			dp = dp->nextdep = ALLOC(depblock);

		}

		dp->depname = tar;

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "add to .MUTEX  dep=%s of tar=%s\n",
				tar->namep, bk_name->namep);
#endif

		return;
}

static RUN_LIST run_list=NULL;

void
add_run(tar, pid)
NAMEBLOCK tar;
int pid;
{
register RUN_LIST p;

#ifdef MKDEBUG
		if (IS_ON(DBUG))
			fprintf(stdout, "add_run: tar=%s, pid=%d\n", tar->namep, pid);
#endif

	if (!run_list)
		p = run_list = ALLOC(run_list);
	else{
		for( p = run_list ; ; p = p->nextrun){
			if(p->pid == 0) /* found empty slot */
				break ;

			if( !p->nextrun){/* Not found */
				p = p->nextrun = ALLOC(run_list);
				break; 
			}
		}
	}
	p->tarp = tar;
	p->pid = pid;
}


void
kill_run()
{
		register RUN_LIST p = run_list ;

		for(; p; p= p->nextrun)
			if(p->pid){

#ifdef MKDEBUG
				if (IS_ON(DBUG))
					fprintf(stdout, "mkexit: kill sig=%d, tar=%s, pid=%d\n",
						SIGTERM, p->tarp->namep, p->pid);
#endif

				kill(p->pid, SIGTERM);
			}

		pwait(1);
}

NAMEBLOCK
find_tar(pid)
int pid;
{
	register RUN_LIST p = run_list ;

	for(; p; p= p->nextrun)
		if( p->pid == pid ){
			 p->pid=0;
			 return(p->tarp);
		}
	return(NULL);
}

void
err_msg(status, ign, tarp)
int status, ign;
NAMEBLOCK tarp;
{
	FILE *fp ;
	extern char* cur_wd;

	fp = open_block(tarp);

	if(IS_OFF(BLOCK) && IS_ON(POSIX))
		fp = stderr;
				
	if(IS_ON(PAR) && is_off(SIL,tarp))
		fprintf(fp, "%s: ", tarp->namep);

	if ( status >> 8 )
		pfmt(fp, MM_NOSTD, _SGI_MMX_make_t1 ":*** Error code %d (bu21)", status >> 8 );
	else
		pfmt(fp, MM_NOSTD, _SGI_MMX_make_t2 ":*** Termination code %d (bu21)", status );

	if (ign)
		pfmt(fp, MM_NOSTD, _SGI_MMX_make_t3 ": (ignored)");

	if(IS_ON(PAR) && is_off(SIL,tarp))
		fprintf(fp, "	[%s/%s]", cur_wd, cur_makefile);

	fprintf(fp, "\n");

	(void)fflush(fp);
}


void
echo_cmd(comstring, tarp)
CHARSTAR comstring;
NAMEBLOCK tarp;
{
	FILE *fp ;
	register CHARSTAR p1 = comstring;
	CHARSTAR ps = p1;

	fp = open_block(tarp);
			
	for (;;) {
		while (*p1 && *p1 != NEWLINE) 
			p1++;

		if (*p1) {
			*p1 = 0;
			fprintf(fp, "%s%s\n", PROMPT, ps);
			*p1 = NEWLINE;
			ps = p1 + 1;
			p1 = ps;
		} else {
			fprintf(fp, "%s%s\n", PROMPT, ps);
			break;
		}
	}
	(void)fflush(fp);
}
static 
void
print_block(tarp)
NAMEBLOCK tarp;
{
	int c;

	if(tarp->tbfp){
	
	if(is_off(SIL,tarp))
		putchar('\n');

		(void)rewind(tarp->tbfp);

		while((c = getc(tarp->tbfp)) != EOF)putchar(c);

		(void)fclose(tarp->tbfp);
	}
}
FILE *
open_block(tarp)
NAMEBLOCK tarp;
{
	FILE *fp = stdout;

	if(IS_ON(BLOCK)){
		if(!tarp->tbfp){

			if((fp = tarp->tbfp = fopen(tmp_block, "w+")) == NULL)
				fatal1(":145:Fail to open <%s>", tmp_block);

			(void)unlink(tmp_block);

		}else
			fp = tarp->tbfp;
	}
			
	return(fp);
}
