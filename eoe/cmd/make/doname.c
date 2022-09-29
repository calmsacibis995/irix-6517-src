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

#ident	"@(#)make:doname.c	1.36.1.2"

#include "defs"
#include <time.h>
#include <errno.h>
#include <sys/stat.h>	/* struct stat */
#include <ctype.h>
#include <pfmt.h>
#include <locale.h>

#define	MAX(a, b)	( (a) > (b) ? (a) : (b) )

extern char	archmem[];	/* archive member name / files.c */
extern LINEBLOCK sufflist;	/* suffix list / main.c */
extern NAMEBLOCK curpname;	/* the name at level 1 of recursion */

extern CHARSTAR	mkqlist(), addstars(), trysccs();	/* misc.c */
extern DEPBLOCK	srchdir();	/* files.c */
extern time_t	exists(),	/* misc.c */
		time();
extern int	is_on(), is_off();

static int 	ndocoms = 0;
char	*touch_t;
size_t touch_t_size;
time_t maxtime;

/*
**	Declare local functions and make LINT happy.
*/

static void	touch();
static void	set_pred();
static void	addimpdep();
static void	ballbat();
static void	dbreplace();
static void	expand();
static int	docom1();
static int	noworkdone();
int	docom();

#ifdef MKDEBUG
static void	blprt();
#endif

/*  doname() is a recursive function that traverses the dependency graph
**
*/
int
doname(p, reclevel, tval)
register NAMEBLOCK p;		/* current target name to be resolved */
int	reclevel;		/* recursion depth level */
time_t	*tval;
{
	register DEPBLOCK q,	/* dependency chain ptr, for each line	*
				 *  that name is a target 		*/
			suffp, suffp1;	/* loop variables */
	register LINEBLOCK lp,	/* line chain ptr, for every line	*
				 *  p is a target of (usually only 1) 	*/
			lp1, lp2;	/* loop variables */
	int	access(),
		suffix(),
		is_sccs(),
		errstat,	/* cumulative error counter */
		okdel1,
		didwork;

	void	setvar(),
		dyndep();

	int	rebuild;	/* flag indicating target to be built */

	/* the following keep track of latest (max) of	*
	*	modification times of dependents	*/

	time_t	td,		/* max time for all dependents on a line */
		td1,		/* max time for an individual dependent */
		tdep,		/* max time for all dependents, all lines*/
		ptime,		/* time for target */
		ptime1,		/* temp space */
		ptime2,		/* temp space */
		exists();
	NAMEBLOCK p1, p2, p3,lookup_name();
	SHBLOCK implcom, 	/* implicit command */
		explcom;	/* explicit command */

	char	sourcename[MAXPATHLEN],	/* area to expand file name & path */
		prefix[MAXPATHLEN], 
		temp[MAXPATHLEN],
		concsuff[SUFF_SIZE];	/* suffix rule string goes here */
	CHARSTAR pnamep, p1namep;
	CHAIN 	qchain;
	void	appendq();
	int	found, onetime;

#ifdef OLDVPATH
        register CHARSTAR vpathp;
        register VARBLOCK vpathcp;
        char    vpathtempval[PATH_MAX];         /* hold dirnames */
        char    vpathname[PATH_MAX];            /* hold full name */
#endif

	if ( !p ) {	/* if no name, time=0 and done */
		*tval = 0;
		return(0);
	}

#ifdef MKDEBUG
	if (IS_ON(DBUG)) {
		blprt(reclevel);
		printf("doname(%s,%d)\n", p->namep, reclevel);
	}
#endif
	/* if file is: a) in process of determining status b) exists or
	 *	c) build failed (probably a looping definition)
	 */
	if(IS_ON(PAR)){
		if(p->done == D_MUTEX || p->done == D_NOEVAL){
			if(p->backname && p->backname->done == D_START)
				p->backname->done = D_NOEVAL;

			if(p->done == D_NOEVAL)
				add_ready(p);

			return(0);
		}
		if(p->done == D_INIT && mutex(p)){
			p->done = D_MUTEX ; /* this target is in mutex;not ready to be eval yet*/
			if(p->backname && p->backname->done == D_START)
				p->backname->done = D_NOEVAL;
#ifdef MKDEBUG
			if (IS_ON(DBUG))
				fprintf(stdout, "Not ready to be eval yet <%s>\n", p->namep);
#endif
			add_ready(p);
			return(0);
		}
		if(IS_PROC(p))
			if(p->backname && p->backname->done == D_START)
				p->backname->done = D_PROC ;
	}
		
	if ( p->done && p->done != D_DONAME ){

		if(IS_ON(PAR) && IS_PROC(p) )
			*tval = PRESTIME() + 1; /* If the target in process we always
																asume is will be updated. */
		else
			*tval = p->modtime;	/* last modified for this name */
		return(p->done == D_ERROR);	/* if make failed rtn 1 else 0 */
	}



	errstat = 0;		/* no errors (yet) */
	tdep = 0;		/* no dependency times yet */
	implcom = NULL;		/* no implicit command */
	explcom = NULL;		/* no explicit command */
	rebuild = 0;		/* nothing found out of date (yet) */



	ptime2 = ptime = exists(p);

	if(ptime < -1 ) { /* Very old file ( older then 1/1/1970 */
		ptime = ptime2 = 0; 
	}


#ifdef MKDEBUG
	if (IS_ON(DBUG)) {
		blprt(reclevel);
		printf("TIME(%s)=%ld\n", p->namep, ptime);
	}
#endif

	ptime1 = -1L;
	didwork = NO;
	p->done = D_START;	/* avoid infinite loops, set to START */
	qchain = NULL;

	/*	Perform runtime dependency translations.	 */

	if ( !(p->rundep) ) {	/* if runtime translation not done */
		okdel1 = okdel;
		okdel = NO;
		setvar("@", p->namep);	/* set name of file to build */
		dyndep(p);		/* substitute dynamic dependencies */
		setvar("@", Nullstr);
		okdel = okdel1;
	}

	/*
	 *	Expand any names that have embedded metacharacters.
	 *	Must be done after dynamic dependencies because the
	 *	dyndep symbols ($(*D)) may contain shell meta
	 *	characters. 
	 */
	expand(p);

	if (p->alias) {
		/* archmem is set only for '((' type deps */
		if (archmem[0] != CNULL) {
			p->namep = copys(archmem);	/*replace with member name */
			archmem[0] = CNULL;
		}
#ifdef MKDEBUG
		if (IS_ON(DBUG)) {
			blprt(reclevel);
			printf("archmem = %s\n", p->namep);
			blprt(reclevel);
			printf("archive alias = %s\n", p->alias);
		}
#endif
	}

	/**		FIRST SECTION		***
	***	   GO THROUGH DEPENDENCIES	**/
	p1 = p;

#ifdef OLDVPATH
        vpathcp = varptr("VPATH");
        vpathp = NULL;
        if(*vpathcp->varval.charstar != 0 && p->namep[0] != SLASH) {
                vpathp = vpathcp->varval.charstar;

                subst(vpathp, vpathtempval, 0);
                vpathp = vpathtempval;
        }

vpathdep:
#endif
#ifdef MKDEBUG
	if (IS_ON(DBUG)) {
		blprt(reclevel);
		printf("look for explicit deps of (%s). %d\n", p1->namep,  reclevel);
	}
#endif

	/* for each line on which this name is a target	*
	*	(usually only 1 line)			*/
	for (lp = p1->linep; lp; lp = lp->nextline) {
		td = 0;

		/* for each dependent on the line */
		for (q = lp->depp; q; q = q->nextdep) {

			/* set predecessor to dependent = target */

			set_pred(q->depname, p);

			/* do the dependency, to get it's time & level*/

			errstat += doname(q->depname, reclevel + 1, &td1);
			curpname = p;	/* set current name as the target */
#ifdef MKDEBUG
			if (IS_ON(DBUG)) {	/* print out dependency */
				blprt(reclevel);
				printf("TIME(%s)=%ld\n", q->depname->namep, td1);
			}
#endif

			/* set new max time for all dependents on line */
			td = MAX(td1, td);

			/* if out of date, add to out-of-date list */
			if(IS_ON(UCBLD) || ptime < td1 )
				appendq((CHAIN) &qchain, q->depname->namep);
		}

		/* if line is a double-colon one */
		if (p->septype == SOMEDEPS) {

			/* if a shell command exists for line */

			if ( lp->shp )
/*
** note: tests formerly here were deleted to force a rebuild on all 
** double-colon lines.  bl87-34119
*/
				if ( (ptime == -1) ||
				    (ptime < td) ||
				    (!lp->depp) ||
				    (IS_ON(UCBLD) && 
				     (explcom || implcom )))
					rebuild = YES;

			if ( rebuild ) {
				okdel1 = okdel;			/* save */
				okdel = NO;
				setvar("@", p->namep);
				if (p->alias) {
					setvar("@", p->alias);
					setvar("%", p->namep);
				}
				setvar("?", mkqlist(qchain) );

				/* ? is the list of names out of date
				 * for the target 
			 	 */
				qchain = NULL;
				if ( IS_OFF(QUEST) ) {
					/* link file in */

					/* set the predecessor chain */
					ballbat(p);

					/* do the shell cmd */
					errstat += docom(p, lp->shp, lp->depp);
				}
				setvar("@", Nullstr);
				setvar("%", Nullstr);
				okdel = okdel1;

				/* is target there yet ? */
				if(IS_ON(PAR))
					ptime1 = PRESTIME() + 1;
				else
					if ((ptime1 = exists(p )) == -1)
						/* no, set to present */
							ptime1 = PRESTIME();

				didwork = YES;
				rebuild = NO;		/* reset flag*/
			}
		} else {
			/* single colon or implicit */

                        /* its ok if two targets have the same
                         * shell line - this can occur with VPATH dependencies
                         * where the user explicitly states the dependency
                         * and we also implicitly find the same dependency
                         * in the code below
                         */
                        if (lp->shp && lp->shp != explcom) { /* shell line specified? */
				if (explcom){	/*  specified before? */
					if(IS_OFF(WARN))
						pfmt(stderr, MM_WARNING, ":58:Too many command lines for `%s' (bu10)\n",
					    p->namep);
				}else
					explcom = lp->shp;
			}

			/* max time, all lines */
			tdep = MAX(tdep, td);

		}
	}

#ifdef OLDVPATH
	{
extern CHARSTAR execat(CHARSTAR, CHARSTAR, CHARSTAR);
        /* now look for explicit dependencies for any VPATH targets of p */
        /* forget about VPATH="" */
        while (vpathp && *vpathp) {
                vpathp = execat(vpathp, p->namep, vpathname);
                if ((p1 = SRCHNAME(vpathname)) != NULL)
                        goto vpathdep;
        }
	}
#endif
	/**		SECOND SECTION		***
	***	LOOK FOR IMPLICIT DEPENDENTS	**/

#ifdef MKDEBUG
	if (IS_ON(DBUG)) {
		blprt(reclevel);
		printf("look for implicit rules. %d\n", reclevel);
	}
#endif
	found = 0;	onetime = 0;

	/* for each double suffix rule (default rules in rules.c) */
	for (lp = sufflist; lp; lp = lp->nextline)

		/* for each dependent suffix */
		for (suffp = lp->depp ; suffp ; suffp = suffp->nextdep) {
			/* get suffix string */
			pnamep = suffp->depname->namep;

			/* if it matches suffix of target name */
			if (suffix(p->namep , pnamep , prefix)) {
#ifdef MKDEBUG
				if (IS_ON(DBUG)) {
					blprt(reclevel);
					printf("right match = %s\n", p->namep);
				}
#endif
				found = 1;

				/* if archive member is target 	*
				 * 	set target suffix	*/
				/* Permit other than .a for archive suffix */
				if (p->alias) {
					int alen = strlen(p->alias);
					if (alen >= 2 && p->alias[alen-2] == '.')
						pnamep = &(p->alias[alen-2]);
					else
						pnamep = ".a";
				}

searchdir:			
				(void) compath(prefix);
				(void)copstr(temp, prefix);
				(void)addstars(temp);	/* find all files w/ same root*/
				/* (eg.  "*file.*" )  */
				(void)srchdir( temp , NO, (DEPBLOCK) NULL);
				for (lp1 = sufflist; lp1; lp1 = lp1->nextline)
					/* do again for all suffixes & */
					/*  all dependencies of suffixes */
					for (suffp1 = lp1->depp; suffp1;
					     suffp1 = suffp1->nextdep) {

	/* get suffix name */			p1namep = suffp1->depname->namep;
	/* concatenate suffixes */		(void)concat(p1namep, pnamep, concsuff);
						if (!(p1 = SRCHNAME(concsuff)))
							/*check if double suffix rule */
							continue;	/* if not double rule */
						if ( !(p1->linep) )
							continue;	/*no rule line for pair*/
						(void)concat(prefix, p1namep, sourcename);
						/*try target with other suffix*/

					        /* sccs file */
						if (ANY(p1namep, WIGGLE)) {
							sourcename[strlen(sourcename) - 1] = CNULL;
							if (!is_sccs(sourcename))

	/* put "s." in front of name */				(void)trysccs(sourcename);
						}
						if (!(p2 = SRCHNAME(sourcename)))
							/* find the name with the different
							 * suffix (and/or "s." prefix)
							 */
							continue;	/* if not found */
						if (STREQ(sourcename, p->namep))
							continue;	/* back to same name */

					/*		FOUND		**
					**	left and right match	*/

						found = 2;
#ifdef MKDEBUG
						if (IS_ON(DBUG)) {
							blprt(reclevel);
							/* prints out :
							 *  right name,
							 *  suffix rule,
							 *  and target name
	 					         */
							printf("%s-%s-%s\n",
								sourcename,
								concsuff,
								p->namep);
						}
#endif
						/* this is a dependent of parent */
							set_pred(p2, p);

						addimpdep(p, p2);
						errstat += doname(p2, reclevel + 1, &td );


						/* do the implied dependent */
						/* if dependent earlier than others */
						/* 	add to the out of date list*/

						if(IS_ON(UCBLD) || ptime < td  )
							appendq((CHAIN) &qchain, p2->namep);

						tdep = MAX(tdep, td);	/* max of all lines */

#ifdef MKDEBUG
						if (IS_ON(DBUG)) {
							blprt(reclevel);
							/* time of implicit dependent */
							printf("TIME(%s)=%ld\n", p2->namep, td);
						}
#endif

						p3=lookup_name(concsuff);
						if(p3 ){
							register DEPBLOCK dp;
							register LINEBLOCK lp;
							for (lp = p3->linep; lp; lp = lp->nextline)
								if ( dp = lp->depp )
									for (; dp; dp = dp->nextdep)
										if ( dp->depname) {
												set_pred(dp->depname, p);
											addimpdep(p,dp->depname);
											errstat += doname(dp->depname, reclevel + 1, &td);
											/* do the implied dependent */
											/* if dependent earlier than others */
											/* 	add to the out of date list*/
											if(IS_ON(UCBLD) || ptime < td )
												appendq((CHAIN) &qchain, dp->depname->namep);
											tdep = MAX(tdep, td);	/* max of all lines */
#ifdef MKDEBUG
						if (IS_ON(DBUG)) {
							blprt(reclevel);
							/* time of implicit dependent */
							printf("TIME(%s)=%ld\n", dp->depname->namep, td);
						}
#endif
										}
						}



						curpname = p;	/* set current name as target */
						/* min of all lines */
						setvar("*", prefix);	/* set name root */
						setvar("<", sourcename);/* full name */
						for (lp2 = p1->linep;
						     lp2;
						     lp2 = lp2->nextline)
							/* find the first shell cmd
							 * of all lines for implicit
							 * suffix rule
							 */
							if (implcom = lp2->shp)
								break;
						goto endloop;		/*found the rule, so done here*/
					} /* repeat for next suffix rule
 					   * if doing the single suffix
					   * type rule (see below), you
					   * only loop through this stuff
					   * once, using the root of the
					   * target name as the prefix, and
					   * the 1st suffix null.  
 					   */
				if ( onetime )
					goto endloop;
			}	/* get a new first suffix */
		}

	/*
	 * look for a single suffix type rule.
	 * only possible if no explicit dependents and no shell rules
	 * XXX we now permit limited dependency...
	 * are found, and nothing has been done so far. (previously, `make'
	 * would exit with 'Don't know how to make ...' message.
	 */
endloop:
	if ( !found &&      /* if not found AND */
      !onetime ){     /*    not the 2nd time thru AND */
    if(!p->linep)     /* no dep action */
      onetime=1;	/* try single suffix rule */
    else
      if(!p->linep->shp){ /* no shell action */
        LINEBLOCK lp;
        DEPBLOCK depp;
	if (IS_ON(NULLSFX))
		onetime = 1;
	else {
        for(lp = p->linep ; lp ; lp = lp->nextline)
        	for(depp = lp->depp ; depp ; depp = depp->nextdep)
          	if(depp->depname->linep) /* Has dep of dep */
            	break;
        if(!depp) /* Not found ; Apply single suffix rule */
          onetime=1;
	}
      }
		if(onetime){

#ifdef MKDEBUG
			if (IS_ON(DBUG)) {
				blprt(reclevel);
				printf("Looking for Single suffix rule.\n");
			}
#endif
			(void)concat(p->namep, "", prefix);		/*target name to target prefix*/
			pnamep = "";
			goto searchdir;
		}
	}


	/**		THIRD SECTION				***
	***	LOOK FOR DEFAULT CONDITION OR DO COMMAND	***
	***     after trying double				***
	***		(and maybe single) suffix rules		**/

	if ( ((ptime == -1) &&		/* target does not exist and */
	     (tdep == 0)) ||		/*	no dependencies there */
	    (ptime < tdep) ||		/* out of date target */
	    (IS_ON(UCBLD) &&		/* unconditional build  and */
	     (explcom || implcom)))	/*	a command to do */
		rebuild = YES;		

	/**	is the rebuilding necessary ?	**/

	if ( rebuild && !errstat ) {

		/* for each line for the target (usually only 1 line)*/

		if (p->alias) {
			setvar("@", p->alias);	/* archive file target */
			setvar("%", p->namep);	/* archive member name */
		} else
			setvar("@", p->namep);	/* regular old file target */
		setvar("?", mkqlist(qchain) );	/* string of all out of date things */
		ballbat(p);		/* predecessor chain */
		if (IS_ON(DBUG2)) {
			pfmt(stdout, MM_NOSTD, _SGI_MMX_make_why1
					":Re-making %s since ", p->namep);
			if (ptime == -1)
				pfmt(stdout, MM_NOSTD, _SGI_MMX_make_why2
					":it doesn't exist\n");
			else if (ptime < tdep) {
				pfmt(stdout, MM_NOSTD, _SGI_MMX_make_why3
					":it is out-of-date w.r.t: ");
				printf(varptr("?")->varval.charstar);
				printf("\n");
			} else if (IS_ON(UCBLD))
				pfmt(stdout, MM_NOSTD, _SGI_MMX_make_why4
					":unconditional option is set\n");
			if (implcom)
				pfmt(stdout, MM_NOSTD, _SGI_MMX_make_why5
					": using internal rule:%s\n", concsuff);
		}
		if (explcom)
			errstat += docom(p, explcom, p->linep?p->linep->depp:NULL);
		else if (implcom)
			errstat += docom(p, implcom, p->linep?p->linep->depp:NULL);
		else if ((p->septype != SOMEDEPS && IS_OFF(MH_DEP)) ||
		         ( !(p->septype)         && IS_ON(MH_DEP))) {

	 		/*      OLD WAY OF DOING TEST is
	 		 *              else if(p->septype == 0)
			 *
	 		 *      the flag is "-b".
	 		 */
			/* if there is a .DEFAULT rule, do it */
			if (p1 = SRCHNAME(".DEFAULT")) {
#ifdef MKDEBUG
				if (IS_ON(DBUG)) {
					blprt(reclevel);
					printf("look for DEFAULT rule. %d\n", reclevel);
				}
#endif
				setvar("<", p->namep);
				for (lp2 = p1->linep; lp2; lp2 = lp2->nextline)
					if (implcom = lp2->shp)
						errstat += docom(p, implcom, p->linep?p->linep->depp:NULL);
			} else if ( !(IS_ON(GET) && get(p->namep, 0, p)) )
				fatal1(":70:don't know how to make %s (bu42)", p->namep);

		}
		else if (IS_ON(POSIX))
			pfmt(stdout, MM_INFO,
				":59:`%s' no action taken.\n", p->namep);
		if(IS_ON(POSIX) && (explcom && noworkdone(explcom)) || 
				   (implcom && noworkdone(implcom)))
			pfmt(stdout, MM_INFO,
				":59:`%s' no action taken.\n", p->namep);
			
		setvar("@", Nullstr);
		setvar("%", Nullstr);

/*
*
* The following test handles those targets that have no dependents. Note
* that for some reason, the check for dependents is omitted if -n is
* turned on. Consequently, make -n behavior make != make without -n
*
*/
/*
		if (IS_ON(NOEX) || ((ptime = exists(p)) == -1))
*/
			ptime = PRESTIME();

		if(IS_ON(PAR))
			ptime++;

	} else if (errstat && !reclevel)
		pfmt(stderr, MM_ERROR,
			":60:`%s' not remade because of errors (bu14)\n",
				p->namep);

	else if ( !(IS_ON(QUEST) || reclevel || didwork) ) {
		if(IS_ON(POSIX))
			pfmt(stdout, MM_INFO,
				":61:`%s' is up to date.\n", p->namep);
		else	pfmt(stderr, MM_INFO,
				":61:`%s' is up to date.\n", p->namep);
	}

	if (IS_ON(QUEST) && !reclevel) {
		if(IS_ON(POSIX)) {
			if(ndocoms > 0)
				(errstat?mkexit(2):mkexit(1));
			mkexit(0);
		} 
		else  mkexit( -(ndocoms > 0) );
	}

	if(IS_OFF(PAR))
		p->done = (errstat ? D_ERROR:D_UPDATE);
	else
		if(p->done == D_START ){
			p->done = D_CHECK ;  /* Nothing to do but need a check in get_ready()
													 that all_dep_done() */
			add_ready(p);
		}

/*	In case of PARALLEL "done" is set in docom() */

	ptime = MAX(ptime1, ptime);

	if(IS_OFF(PAR) || p->done == D_CHECK){
		p->modtime = ptime;
	}else{
		p->modtime = ptime2;
	}
	*tval = ptime;
	setvar("<", Nullstr);
	setvar("*", Nullstr);
	return(errstat);
}

/*
 * keep track of maximum time ever seen and never reset the 'present'
 * time less than this. This fixes a problem where:
 *	foo:_force
 * And foo exists but is way in the future, and _force rule doesn't do anything
 * so after the _force rule, the code above says _force still doesn't exist
 * set its time to the present and continue. But foo's time is greater than
 * the present so its rules won't get run
 */
time_t
prestime()
{
	time_t t;
	time(&t);
	if (t > maxtime)
		maxtime = t;
	else
		t = maxtime+1;
	return(t);
}

int
docom(p, qblock, dep_line)
NAMEBLOCK p;			/* info about this file */
SHBLOCK qblock;		/* list of shell commands */
DEPBLOCK dep_line;
{
	register SHBLOCK q=qblock;	/* list of shell commands */
	register CHARSTAR s;
	int ret_com;
	int	sindex();
	char	*string;
	CHARSTAR subst();
	int	Makecall,	/* flags whether to exec $(MAKE) */
		ign, nopr, plus, noex;

	++ndocoms;
	if (IS_ON(QUEST) && IS_OFF(POSIX))
		return(0);

	if (IS_ON(TOUCH)) {

		s = varptr("@")->varval.charstar;
		if (is_off(SIL,p))
			pfmt(stdout, MM_ERROR, ":62:touch(%s)\n", s);
		if (IS_OFF(NOEX))
			touch(s);
		if(IS_OFF(POSIX))
			return(0);
	}
	if(IS_ON(PAR))
		if(!ready_to_run(p, q, dep_line))return(0);

	string = ck_malloc(outmax);
	for ( ; q ; q = q->nextsh ) {

	/* Allow recursive makes only if NOEX flag is set */

		if ( !(sindex(q->shbp, "$(BM)")    == -1 &&
		       sindex(q->shbp, "${MAKE}")  == -1 &&
		       sindex(q->shbp, "$(MAKE)")  == -1) &&
		    IS_ON(NOEX))
			Makecall = YES;
		else
			Makecall = NO;

		(void)subst(q->shbp, string, 0);

		ign = is_on(IGNERR,p);
		nopr = NO;
		plus = NO;
		for (s = string ; *s && *s == MINUS || *s == AT  || *s == PLUS ; ++s){
			if (!*s) break;
			if (*s == MINUS)  
				ign = YES;
			else if (*s == AT)  
				nopr = YES;
			else if(IS_ON(POSIX) && *s == PLUS)
				plus = YES;
		}

		if(IS_ON(PAR) && is_off(SIL,p) && 
			q == qblock && IS_OFF(NOEX) && p->done != D_NEXT){
			if(p->alias)
				pfmt(stdout, MM_NOSTD,
				    ":63:\nMaking target \"%s(%s)\"\n",
					p->alias, p->namep);
			else
				pfmt(stdout, MM_NOSTD,
				    ":64:\nMaking target \"%s\"\n",
					p->namep);

			(void)fflush(stdout);
		}
		if(plus) {
			if(noex = IS_ON(NOEX))
				TURNOFF(NOEX);		/* plus overrides */
		}
		else if(IS_ON(TOUCH) || IS_ON(QUEST))
			continue;

		ret_com = docom1(s, ign, nopr, Makecall, p) ;

		if(plus && noex) 
			TURNON(NOEX);		/* restore */

		if( IS_OFF(PAR)){
			if( ign ) continue;
			if(ret_com )
				if (IS_ON(KEEPGO))
				{	
					++k_error;
					return(1);
				}
				else  {
					free(string);
					fatal(0);
				}
		}else{  /* PAR is ON */
			if( ret_com <= 0 ){/* command did not exec. */
				if(ret_com == 0){
					q->shbp = NULL ;
					continue;
				}else{
					err_msg(ret_com, ign, p);
					if(ign){
						q->shbp = NULL ;
						continue;
					}else{
						if(IS_ON(KEEPGO)){
							++k_error;
							p->done = D_ERROR ;
							break;
						}else {
							free(string);
							fatal(0);
						}
					}
				}
			}
			nproc++;
			(void)add_run(p, ret_com);
			if(p->done == D_START)
				(void)set_in_proc(p);

			p->done = ign ? D_RUN_IGN : D_RUN ;
			break; /* if PARALLEL is set only one command will be exec. */
		}
	}
	free(string);
	if(ret_com == 0) p->done = D_UPDATE ; 
		/* target did not exec. empty comman's block */
	return(0);
}


static int
docom1(comstring, nohalt, noprint, Makecall, tarp)
register CHARSTAR comstring;
int	nohalt, noprint, Makecall;
NAMEBLOCK tarp;
{
	register int	status;
	int	dosys();

	if (comstring[0] == '\0') 
		return(0);

	if (is_off(SIL,tarp) && (!noprint || IS_ON(NOEX)) )
		(void)echo_cmd(comstring, tarp);

	if ( status = dosys(comstring, nohalt, Makecall, tarp) ) 
		if(IS_OFF(PAR))
			err_msg(status, nohalt, NULL);

	return(status);
}



/* expand()
 *      If there are any Shell meta characters in the name, search the
 *	directory, and if the search finds something replace the
 *	dependency in "p"'s dependency chain.  srchdir() produces a
 *	DEPBLOCK chain whose last member has a null nextdep pointer or
 *	the NULL pointer if it finds nothing.  The loops below do the
 *	following:
 *	for each dep in each line 
 *		if the dep->depname has a shell metacharacter in it and
 *		if srchdir succeeds,
 *			replace the dep with the new one created by
 *				srchdir. 
 *	The Nextdep variable is to skip over the new stuff inserted into
 *	the chain. 
 */
static void
expand(p)
register NAMEBLOCK p;
{
	register DEPBLOCK db, srchdb;
	register LINEBLOCK lp;
	register CHARSTAR s;

	for (lp = p->linep ; lp ; lp = lp->nextline)
		for (db = lp->depp ; db ; db = db->nextdep )
			if (((ANY((s = db->depname->namep), STAR)) ||
			     (ANY(s, QUESTN) || ANY(s, LSQUAR))) &&
			    (srchdb = srchdir(s , YES, (DEPBLOCK) NULL)))
				dbreplace(p, db, srchdb);
}



/*
 *      Replace the odb depblock in np's dependency list with the
 *      dependency chain defined by ndb.  dbreplace() assumes the last
 *	"nextdep" pointer in "ndb" is null.
 */
static void
dbreplace(np, odb, ndb)
NAMEBLOCK np;
register DEPBLOCK odb, ndb;
{
	register LINEBLOCK lp;
	register DEPBLOCK  db, enddb;

	for (enddb = ndb; enddb->nextdep; enddb = enddb->nextdep)
		;
	for (lp = np->linep; lp; lp = lp->nextline)
		if (lp->depp == odb) {
			enddb->nextdep  = lp->depp->nextdep;
			lp->depp        = ndb;
			return;
		} else
			for (db = lp->depp; db; db = db->nextdep)
				if (db->nextdep == odb) {
					enddb->nextdep  = odb->nextdep;
					db->nextdep     = ndb;
					return;
				}
}



#define NPREDS 50

static void
ballbat(np)
NAMEBLOCK np;
{
	static char *ballb;
	static int ballb_size;

	register CHARSTAR p;
	register NAMEBLOCK npp;
	register int	i;

	VARBLOCK vp;
	int	npreds = 0;
	NAMEBLOCK circles[NPREDS];
	int update_vp;
	int temp_size;
	char *temp_string;

	if ( ballb == NULL ) {
		ballb_size = 200;
		ballb = ck_malloc(ballb_size) ;
	}

	if ( !((vp = varptr("!"))->varval.charstar) ) {
		update_vp = 1;
		vp->varval.charstar = ballb;
	} else
		update_vp = 0;

	temp_string = varptr("<")->varval.charstar;
	temp_size = (temp_string == NULL)?0:strlen(temp_string) + 2;

	for (npp = np; npp; npp = npp->backname) {
		for (i = 0; i < npreds; i++)
			if (npp == circles[i]) {
p_error:	if(IS_OFF(WARN))
			pfmt(stderr, MM_WARNING,
				":65:$! nulled, predecessor cycle\n");
				ballb[0] = CNULL;
				return;
			}
		circles[npreds++] = npp;
		if (npreds >= NPREDS)
			goto p_error;

		temp_size += strlen(npp->namep) + 2;
	}

	if ( temp_size > ballb_size ) {
		ballb_size = temp_size;
		if ( (ballb = (char *) realloc(ballb,ballb_size)) == 0 )
			fatal(":71:realloc failed");
		if ( update_vp )
			vp->varval.charstar = ballb;
	}
	p = ballb;
	p = copstr(p, temp_string);
	p = copstr(p, " ");

	for (npp = np; npp; npp = npp->backname) {
		p = copstr(p, npp->namep);
		p = copstr(p, " ");
	}
}


#ifdef MKDEBUG
static void
blprt(n)	/* PRINT n BLANKS WHERE n = CURRENT RECURSION LEVEL. */
register int	n;
{
	while (n--)
		printf("  ");
}
#endif


static void
addimpdep(fname, dep)
register NAMEBLOCK dep;
NAMEBLOCK fname;
{
	register DEPBLOCK dpnom;
	register LINEBLOCK lpnom;

	for (lpnom = fname->linep; lpnom; lpnom = lpnom->nextline)
		for (dpnom = lpnom->depp; dpnom; dpnom = dpnom->nextdep)

			/* if file is already listed as explicit
			 * dependent don't add it to the list. */

			if (STREQ(dpnom->depname->namep, dep->namep)) 
				return;

	dpnom = ALLOC(depblock);
	if (!fname->linep)
		fname->linep = ALLOC(lineblock);
	else
		dpnom->nextdep = fname->linep->depp;
	fname->linep->depp = dpnom;
	dpnom->depname = dep;

}



static void
touch(s)
register CHARSTAR s;
{
	struct utimbuf {
		time_t T_actime;	/* access time */
		time_t T_modtime;	/* modification time */
	};
	unsigned sleep();
	CHARSTAR mktemp(), dname(), sname();
	int	unlink(), creat(), close(), fd;

	if (utime(s, NULL) ) {
		if ((fd = creat(s, (mode_t)0666)) < 0)
bad:			fatal1(":78:Cannot touch %s (bu25)", s);
		(void)close(fd);
	}
/*
		(void)sleep(1);
*/

}



static 
void
set_pred(dep, tar)
NAMEBLOCK dep, tar;
{
register NAMEBLOCK p;

	for(p = tar; p ; p = p->backname)
		if(p == dep)
			break;

	if(p){
		if(IS_OFF(WARN))
			pfmt(stderr, MM_WARNING,
				":68:predecessor cycle (%s)\n",
					dep->namep);
	}else
		dep->backname = tar;
}

/* Look if there was any real work done */

static int
noworkdone(qblock)
SHBLOCK qblock;
{
	register SHBLOCK q=qblock;	/* list of shell commands */

	for ( ; q ; q = q->nextsh ) {
		if(*q->shbp)
			return(0);
	}
	return(1);
}
