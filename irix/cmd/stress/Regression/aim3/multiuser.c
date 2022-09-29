
/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) multiuser.c:3.3 7/12/92 13:50:04" };
#endif

char	*version = "3.11";
char	*release_date = "June 1st, 1992"; 

#define MULTIUSER

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>

#ifdef T_TIMES
#include <time.h>
#else   /* NOT SYSTEM V or just wanted to use the ftime() instead */
#include <sys/timeb.h>
#include <sys/time.h>
#endif

#include "ttytest.h"
#include "vmtest.h" 
#include "testerr.h"
#include "suite3.h"
#include "tp_test.h"

long	rmtsec();

double		JPMHold;			/* JPM = Jobs Per Minute */

long		t1, t2, rt1, rt2;		/* time variables */
struct tms	t[2];
int		gun;				/* start all processes running */
int		work;				/* work load for processes */
int		numdir;				/* number of disk directories */
char		dkarr[MAXDRIVES][STRLEN];	/* dirs for disk thrasher */
char		wdev[STRLEN];			/* write tty character device */
char		rdev[STRLEN];			/* read tty character device */
int		nwbd;				/* baud rate for tty exerciser */
char		lpcom[STRLEN];			/* line printer command array */
char		tpcom[STRLEN];			/* tape drive device */
long		vm_res;				/* virtual memory results */
int		vmgo, tpgo, lpgo;		/* if exercising ... */
int		kids;				/* number of users forked */
int		lp_child;
char		mname[25];			/* machine name */
char		*ldate;				/* date */
char		sdate[25];			/* date again */
long		vtime;

struct {					/* command / weight array */
    int hits;
    char cmd[50];
} tasks[WORKLD];

int		maxusers[MAXITR];		/* max users at once */
int		minusers[MAXITR];		/* min number of users */
int		incr[MAXITR];			/* skip incr number each run */
int		iters;

int		init_vars();

void main(argc,argv)
int argc;
char **argv;
{
    FILE	*fp, *fopen();		/* file pointer */
    int		i, n;			/* counters */
    int		p, adapt=1;
    FPtr	sigvalu;
    int		runnum;			/* run number */
    int		inc;			/* # users to inc per iteration */
    long	atol();
    int		adjust_inc();
    int		skip, once;


	if((sigvalu = (FPtr)signal(SIGINT,SIG_IGN)) == (FPtr)(-1))  {
		fprintf(stderr,"Can't set signal\n");
		perror("SIGINT");
		exit(1);
	}
#if defined(SYS5)
        (void)setpgrp();
#else
	(void)setpgrp(0, getpid());
#endif

	(void)signal(SIGINT,SIG_IGN);	/* catch signal and ignore */

	(void)signal(SIGTERM,killall);	/* child process problem */

	/*** Init global variables ***/
	init_vars();
	printf("\nAIM Technology Suite III v%s, %s\n",version, release_date);
	printf("Copyright (C) 1986,1991 AIM Technology Inc.\n");
	printf("All Rights Reserved\n\n");

#ifdef SYS5 
        ulimit(2,1L<<20);       /* 1 million blocks for max file size */
#endif

	while(--argc > 0)  {
		if((*++argv)[0] == '-')  {
		    switch((*argv)[1])  {
#ifdef DEBUGON
		    case 'd': debug = atol(&(*argv)[2]);
			if (!debug) debug=1; /* default to level 1 */
			break;
#endif
		    case 't': adapt = 0; /* don't use adaptive timer */
			break;
		    default:
			fprintf(stderr,"Unknown option --> %s\n", *argv);
			fprintf(stderr,"Usage: %s [-t] [-dn]\n", *argv);
			fprintf(stderr,"  -t     turn off adaptive timer\n");
			fprintf(stderr,"  -dn    turn on debug level n\n\n");
			exit(1);
			break;
		    }
		}
	}
	if (adapt) {
	printf("\nYou have chosen to use the adaptive timer.\n");
	printf("You need to provide the initial increment for the user load\n");
	printf("so that the adaptive timer logic has a starting point to base\n");
	printf("its calculations.\n");
	printf("Use \"multiuser -t\" to run Suite III without the adaptive timer.\n\n");
	}
	getmname();

    /* open work file and read in tasks */
    if ((fp = fopen(WORKFILE,"r")) == NULL) {
		fprintf(stderr,
			"Can't find file \"%s\" in current directory ... EXIT\n", WORKFILE);
		perror(argv[0]);
		exit(1);
    }
	/*
	** This section rewritten to be more "error aware"
	** Use local char buf[]
	*/
	while ( !feof(fp) && !ferror(fp) ) {

		char	buf[132];

		if (fgets(buf, 132, fp) != NULL) {
			n = sscanf(buf, "%d %s", &(tasks[work].hits),
					tasks[work].cmd);
			if ((n<2) && (strlen(buf)>0)) {
				fprintf(stderr,
					"Error in file '%s' (line #%d)=[%s]\n",
					WORKFILE, work, buf);
				exit(1);
			}
		} else if ( !feof(fp) ) {
			if ( work ) {
				perror(argv[0]);
				fprintf(stderr,
					"Error reading file '%s' ", WORKFILE);
			} else
				fprintf(stderr,
					"File '%s' is empty\n", WORKFILE);
			exit(1);
		}
		work++;
	}
	fclose(fp);

	work--;	/* set to correct number of lines of work to be done found in workfile */

	/* Sample without replacement */
	for (i=0, tasks[work].hits=0; i<work; i++)
	    tasks[work].hits += tasks[i].hits;
	
	/* print out weighted task list */
	DEBUG(fprintf(stderr, "worklist: \n");
		for (i=0; i<=work; i++)
		    fprintf(stderr, "%d %s\n", tasks[i].hits, tasks[i].cmd));

	printf("\n...testing started\n\n");
	fflush(stdout);
	/* start virtual memory exercisers */
	if (vmgo) {
	    printf("Virtual Memory test running\n");
	    fflush(stdout);
	    if (vmctl(&vm_res,CONTINUE) < 0) {
		printf("VMTEST NOT WORKING..EXIT\n");
		fflush(stdout);
		killall(0);
	    }
	}

	/*
	 * create the disk files that all disk_rd and disk_cp tests
	 * will use. 
	 */
	if(diskw_all() == -1) {
	    perror("diskw_all()");
	    fprintf(stderr, "disk work file creation failed .. EXIT\n");
	    fflush(stderr);
	    killall(0);
	}

	/* here is where it all happens */
	for (p=0; p<iters; p++) {
		vtime = time((long *)0);
		ldate = ctime(&vtime);
		sscanf(ldate,"%*s %21c",sdate);
		sdate[20]=0;
		suite3_header(mname,sdate);
		inc = incr[p];
		skip = 0;
		once = 0;
		printf("			jobs/min	job/sec/user	 real	 cpu\n");
		for (runnum=minusers[p]; runnum<=maxusers[p]; ) {
			printf("Simulating %d user%c .... ", runnum,
				(runnum<2 ? ' ' : 's'));
			fflush(stdout);
			runtap(runnum,LOADNUM);
			if (runnum == maxusers[p]) break;
			if (adapt)
				inc = adjust_inc(inc, p);
			runnum += inc;
			if ((runnum > maxusers[p]) && (skip==0)) {
				runnum = maxusers[p];
				skip = 1;
			}
		}
	}
	if (vmgo) {
		vmctl(&vm_res,KILLMEM);
	}
	/* 
	 * unlink disk files used by disk_rd and disk_cp tests
	 * write results[] to file
	 */
	diskunl_all();
	dump_results();

	printf("\nAIM Technology Suite III\n   Testing over\n");
	fflush(stdout);
	exit(0);
}


runtap(cnum,loadnm)
int cnum;					/* current user load number */
int loadnm;					/* # procs per user */
{
#define NULLSTR		""

    int		user,
		i,
		err_code,
		j,
		dj,	/* disks array index */
		k,
		status;
    char	fofof[STRLEN];    
    int		rand();			/* random number generator */
    void	srand();		/* seed planter */
    int		rand2();		/* random number generator */
    void	srand2();		/* seed planter */
    int		randnum;		/* random number */
    char	cmd[256];		/* executable command to "system" */
    char	*ptr;			/* string holder */
    char	dmpstr[STRLEN];		/* temp data string to output */
    long	stuff;			/* ttyctl parameters */
    int		raw = 1;		/* raw tty (boolean) */
    struct	ttdata	data;		/* tty data */
    Cargs	*realargs,
		*hasargs();
    int		(*func)();
    double	proc_sec,
		real_time,
		cpu_time;
    extern	int	 errno;
    double	x;
    int		wlist[WORKLD],
		wlrs;			/* wlist rounding status */
    int		reduce_list();


	DEBUG(fprintf(stderr,"\n"));
	for (j=0, wlrs=0; j<work; j++) {/* How many of each to sample? */
		x = ((double)tasks[j].hits/tasks[work].hits)*loadnm;
		wlist[j] = (int)x;
		DEBUG(fprintf(stderr,"task weight is %d, x is %f, wlist is %d,",tasks[j].hits,x,wlist[j]));
		if ((x - (double)wlist[j]) >= 0.5) {	/* round up or down */
			wlist[j] += 1;
			wlrs += 1;
		}
		else if ((x - (double)wlist[j]) != 0.0) 
				{ if ( wlist[j] == 0) { 
					wlist[j]=1;
					wlrs +=1;
				} else wlrs -= 1;
				}
		DEBUG(fprintf(stderr," rounded to %d\n",wlist[j]));
	}
	DEBUG(fprintf(stderr,"\n"));
	DEBUG(fprintf(stderr,"wlrs value is %d\n",wlrs);
		fprintf(stderr,"wlist array is %d elements long :\n", work);
		for (i=0; i<work; i++) printf("%d \n", wlist[i]));

/*********************************************************/
	
	/* print out weighted task list */
	DEBUG(fprintf(stderr, "worklist: \n");
		for (i=0; i<work; i++)
		    fprintf(stderr, "%d \n", wlist[i]));

	/* compute the number of jobs for the selected mix */
	loadnm=0;
	for (i=0; i<work; i++) loadnm += wlist[i];
	DEBUG(fprintf(stderr, "number of jobs for this mix = %d\n",loadnm));

	proc_sec = real_time = cpu_time = 0.0;

	/* reset epoch in rtmsec */
	RTMSEC(TRUE);

	/* for each user */
	for (user=0; user<cnum; user++) {

	/* fork off a child process */
	if (fork_() == 0)  {	/* if we are child */
		srand(user);
		srand2(user);
		signal(SIGINT,letgo);
		signal(SIGTERM,SIG_DFL);
		signal(SIGHUP,dead_kid);
		signal(SIGFPE,math_err);

		/* wait for gun, so all children start together */
		pause(); 
		/*
		** We used to spawn off loadnm number of procs,
		** one at a time.  However, v3.0 and greater of Suite 3
		** no longer do that; one process per user plus one more
		** for pipe_cpy.
		** 
		*/

		dj = (numdir > 0 ? (rand2()%numdir) : 0);
		for (j=0; j<loadnm; j++) {
			randnum = rand();

		/* Sampling without replacement */
			k = randnum % work;
			if (wlist[k] > 0)
				wlist[k]--;
			else {
				k = reduce_list(wlist);
				wlist[k]--;
			}

			realargs = hasargs(tasks[k].cmd);
			func = realargs->f;
			ptr = NULLSTR;
			sprintf(cmd, "%s ", tasks[k].cmd);
			if (strcmp(realargs->args, "DISKS") == 0) {
				if (numdir > 0) {
					ptr = dkarr[dj%numdir];
					dj++;
					strcat(cmd, ptr);
				}
			}
			else if(strcmp(realargs->args, cmdargs[0].args) != 0) {
				ptr = realargs->args;
				strcat(cmd, ptr);
			}

			/*
			** run selected task, wait for completion or try
			** again; keep trying to execute a command */
			errno = 0;
			sprintf(fofof,"\nChild #%d: ", user);
			if ( (*func)(ptr,&results[j])<0 ) {
				perror(fofof);
				fprintf(stderr,
					"\nFailed to execute\n\t%s\n", cmd);
				kill(getppid(), SIGTERM);
				exit(1);
			}
		} /* for (j=0; j<loadnm; j++) */

		exit(0);
	    }	/* if (fork_() == 0) */
	}	/* for (user=0; user<cnum; user++) */

	/* start line printer exerciser */
	if (lpgo) {
	if ((lp_child = fork_()) == 0) {
		signal(SIGINT,letgo);
		signal(SIGTERM,SIG_DFL);
		pause();
		execl("./lptest", "lptest", lpcom);
		/* if you get here, lptest is not working... */
		printf("LINE PRINTER TEST NOT WORKING!\n");
		printf("AIM Suite III terminated abnormally.\n");
		fflush(stdout);
		kill(getppid(),SIGTERM);
	    }
	}
	DEBUG(printf("LP test going\n");
		fflush(stdout));

	/* start tty exerciser */
	if (nwbd != 0) {
	        if ((err_code=ttyctl(nwbd, wdev, rdev, raw, &data)) < 0) {
		    printf("TTY EXERCISER NOT WORKING!\n"); 
			printf("AIM Suite III terminated abnormally.\n");
			printf("- error code is %d\n",err_code);
		    fflush(stdout);
		    killall(0);
		}
	}
	DEBUG(printf("TTY test going\n");
		fflush(stdout));

	/* start tape drive exerciser */
	if (tpgo) {
	    if((err_code=tp_ctl(CONTINUE, tpcom, &stuff)) < 0 ) {
		printf("TAPE EXERCISER NOT WORKING!\n"); 
		printf("AIM Suite III terminated abnormally.\n");
		printf("- error code is %d\n",err_code);
		fflush(stdout);
		killall(0);
	    }
	}
	DEBUG(printf("Tape exerciser going\n");
	      fflush(stdout));

	/* wait for all user processes to fork */
	sleep(5);

	/* start timers and send sig to children to start doing their tasks  */
	t1 = CHILDHZ(0); rt1 = RTMSEC(FALSE);
	kill(0,SIGINT);

	while (1) {
		if (wait(&status) > 0) {
			DEBUG(printf("Got into child ended loop.\n"));
			if ((status & 0377) == 0177) { /* child proc stopped */
				fprintf(stderr,
				"\nChild process stopped by signal #%d\n",
				((status >> 8) & 0377));
			}
			else if ((status & 0377) != 0) { /* child term by sig */
				if ((status & 0377) & 0200) {
					fprintf(stderr,"\ncore dumped\n");
					fprintf(stderr,
					   "\nChild terminated by signal #%d\n",
					   (status & 0177));
				}
			}
			else { /* if ((status & 0377)==0) ** child exit()'ed */
				if (((status >> 8) & 0377))
					fprintf(stderr,
					   "\nChild process called exit(), status = %d\n",
						((status >> 8) & 0377));
			}
		--kids;
		if (kids == 0) break;
		}
		else if (errno == ECHILD) break;
	}

	DEBUG(printf("Got out of child ended loop.\n"));

	t2 = CHILDHZ(0); rt2 = RTMSEC(FALSE);
	/* testing over */

	/* Halt any exercisers that are running */
	if (nwbd != 0) {
	    KILLTTY;
	}

	if (tpgo) {
	    tp_ctl(KILLTAPE, tpcom, &stuff);
	}

	/* calculate stats for writing to toutput file */
	real_time = (rt2 - rt1) / 1000.0;
	proc_sec = (double)loadnm / real_time;
	JPMHold = proc_sec * 60.0 * (double)cnum;
	if (a_tn1 > 0.0) {
		a_tn = a_tn1;
		a_tn1 = real_time;
	} else
		a_tn1 = real_time;
	/*
	** print realtime to screen so we can see what's going on 
	*/
	cpu_time = (t2 - t1) / (double)HZ;
	printf( "%6.1lf		   %6.4lf	%6.1lf	%6.1lf\n", JPMHold, proc_sec, real_time, cpu_time);
	fflush(stdout);
	sprintf(dmpstr,
		"%d  %6.1lf real  %6.4lf job/sec/user  %6.1lf cpu  %6.1lf jobs/min\n",
		cnum, real_time, proc_sec, cpu_time, JPMHold);
	dump(dmpstr);
	suite3_out(cnum, JPMHold, real_time, cpu_time, proc_sec);
}

Cargs *hasargs(s)
char *s;
{
	int i;

	for(i=1; i<MAXCMDARGS; i++) {
	    if(strcmp(s, cmdargs[i].name) == 0) {
		return( &cmdargs[i] );
	    }
	}
	return( &cmdargs[0] );
}

fork_()
{
/*
**  fork_ is called by runntap to control process forking, and process
**  numbering.  repeated attempts are made to fork, returning process
**  ids if successful, or -1 if no process is available.
*/
	int k, fk;

	for (k=0; k<10; k++)
		if( (fk = fork()) < 0)  {
			sleep(1);  /* 9/18/89 wait for system to catchup TVL */
		}
		else {
			++kids;
			return (fk);
		}

	/* if fork fails */
	killall(0);
	return(-1);			/* shut up lint */
}

void letgo()
{
/*  start child process */
	gun=1;  /* BANG!... and they're off */
}

dump(str)
char *str;
{
/*  dump simply opens "toutput" in the current directory, and append
**  formatted data.
*/
	FILE *fp;

	fp = fopen("toutput","a");
	if (fp == NULL) {
		perror("dump()");
		fprintf(stderr, "Can not open toutput data file.. EXIT\n");
		exit(3);
	}
	fputs(str, fp);
	fclose(fp);
}

/*-----------------------------------------------------------
** dump_results()
**
**	Dump the data in results[] to file "results".  This
**	is to fool optimizing compilers into thinking that
**	data generated is needed and must be computed.
**
**	Tin Le 11/9/89
*/
dump_results()
{
	FILE	*fp;
	int	i;

	if ((fp = fopen("results", "a")) == NULL) {
		perror("dump_results()");
		fprintf(stderr, "Can not open results data file.. EXIT\n");
		exit(3);
	}
	fputc('\n', fp);
	for (i=0; i<78; i++) fputc('*', fp);
	fputc('\n', fp);
	for (i=0; i<LIST; i++) {
		fprintf(fp, "%10.5le\n", results[i].d);
	}
	fclose(fp);
}

getmname()
{
/*
**  getmname reads machine name and the range of users to simulate from
**  standard input.  The config file is then read, and peripheral exercisers
**  set up.  Extensive error checking is performed.  The routine is exited,
**  killing everything if errors are encountered.
*/
	char header[132];		/* formatted info for each machine */
	char ttlab[9];			/* TTY ON or OFF */
	char tplab[9];			/* TAPE ON or OFF */
	char lplab[9];			/* LP ON or OFF */
	char vmlab[9];			/* VM ON or OFF */
	char strtp[STRLEN];		/* temp string */
	FILE *fp;			/* file pointer */
	FILE *fp2;			/* file pointer */
	char rstring[STRLEN];		/* read from config file */
	char vmcom[STRLEN];		/* virtual memory command */
	char mixstr[STRLEN];		/* read from mix file */
	int  j;

	/* initializations */
	for (j=0; j<STRLEN; j++)
		rstring[j] = vmcom[j] = mixstr[j] = '\0';

	/* check for config file */
	fp = fopen(CONFIGFILE,"r");
	if(fp == NULL) {
		fprintf(stderr, "No config file: %s\n", CONFIGFILE);
		exit(1);
	}

	/* Pick up Machine name, and other information */
	printf("Please enter this machine's name: ");
	fflush(stdout);
	gets(mname);
	vtime = time((long *)0);
	ldate = ctime(&vtime);
	sscanf(ldate,"%*s %21c",sdate);
	sdate[20]=0;

	printf("Please enter the number of iterations to run: ");
	fflush(stdout);
	gets(strtp);
	/*
	** default iters to 1
	*/
	if ( !(iters=atoi(strtp)) || iters<=0 || iters>MAXITR ) {
		iters=1;
		fprintf(stderr, "Iteration must be between 1 to 5 inclusive!\n");
		fprintf(stderr, "Iteration default to %d\n", iters);
	}
	for (j=0; j<iters; j++) {
restart:
    		printf("\n\nEnter information for iteration #%d\n", j+1);
			printf("Please enter the starting number of users to simulate: ");
			fflush(stdout);
			gets(strtp); 
			if ( !(minusers[j]=atoi(strtp)) || minusers[j]<=0) {
			fprintf(stderr,"Start no. of users = %d?\nPlease reenter\n",minusers[j]);
			goto restart;
    		}
restart2:
    		printf("Please enter the maximum number of users to simulate: ");
    		fflush(stdout);
    		gets(strtp);
			if ( !(maxusers[j]=atoi(strtp)) || maxusers[j]<=0) {
			fprintf(stderr, "Max users = %d?\nPlease reenter\n", maxusers[j]);
			goto restart2;
			}
			if (minusers[j] > maxusers[j]) {
			fprintf(stderr, 
			"Number of users to start at must be < Maximum number of users\n\n");
			goto restart;
		}
		printf("Increment by how many users each time?: ");
		fflush(stdout);
		gets(strtp); 
		if ( !(incr[j]=atoi(strtp)) || incr[j]<=0) {
			fprintf(stderr, "Default to increment of 1\n");
			incr[j] = 1;
		}
	}  /* end iteration */

	numdir = 0;
	/* read config file */
	while(fgets(rstring, STRLEN, fp) != NULL) {
		switch(rstring[0]) {
		case '0':		/* TTY Baud Rate */
			sscanf(rstring,"%*c \"%d", &nwbd);
			break;
		case '1':		/* TTY port for reading */
			sscanf(rstring,"%*c \"%s",rdev);
			rdev[strlen(rdev)-1] = '\0';/* delete end " (quote) */
			break;
	 	case '2':		/* TTY port for writing */
			sscanf(rstring,"%*c \"%s",wdev);
			wdev[strlen(wdev)-1] = '\0';
			break;
		case '3':		/* Directories for disk exerciser */
		        sscanf(rstring,"%*c \"%s",dkarr[numdir]);
			dkarr[numdir][strlen(dkarr[numdir])-1] = '\0';
			numdir++;
			break;
		case '4':		/* LP command */
		        sscanf(rstring,"%*c \"%s",lpcom);
			lpcom[strlen(lpcom)-1] = '\0';
			break;
		case '5':		/* Tape command (raw) */
			sscanf(rstring,"%*c \"%s",tpcom);
			tpcom[strlen(tpcom)-1] = '\0';
			break;
		case '6':		/* Virtual Mem command */
			sscanf(rstring,"%*c \"%s",vmcom);
			vmcom[strlen(vmcom)-1] = '\0';
			break;
		default:		/* Anything else is comment */
			if (debug) {
				fprintf(stderr, "config: %s\n", rstring);
			}
			break;
		}
	}
	if ((nwbd == 0) || (*wdev == '\0') || (*rdev == '\0')) {
		nwbd = 0;
		sprintf(ttlab,"TTY OFF");
	}
	else 
		sprintf(ttlab,"TTY %d baud",nwbd);

	lpgo = 0;
	if ( strcmp(lpcom,"OFF") ) {	/* is it == OFF? */
		sprintf(lplab,"LP ON");	/* Nope, we're ON! */
		lpgo = 1;
	}
	else
		sprintf(lplab,"LP OFF");

	tpgo = 0;
	if ( strcmp(tpcom,"OFF") ) {	/* is it == OFF? */
		sprintf(tplab,"TAPE ON");	/* Nope, we're ON! */
		tpgo = 1;
	}
	else
		sprintf(tplab,"TAPE OFF");

	vmgo = 0;
	if ( strcmp(vmcom,"OFF") ) {	/* is it == OFF? */
		sprintf(vmlab,"VM ON");	/* Nope, we're ON! */
		vmgo = 1;
	}
	else
		sprintf(vmlab,"VM OFF");
	if (numdir)
		if ((strcmp(dkarr[0],"OFF")) == 0) /* if 1st one is OFF */
			numdir = 0;		/* ALL is OFF */

	if ((fp2=fopen("mixb", "r")) == NULL)  {
		perror("mixb");
		fprintf(stderr,"File 'mixb' is unavailable.  Exiting\n");
		exit(1);
	}
	if(fgets(mixstr, sizeof(mixstr), fp2) == NULL)  {
		fprintf(stderr,"File 'mixb' is empty\n");
		exit(1);
	} 

	sprintf(header,": %s %s %s, %s, %s, %s, %s",mname,sdate,ttlab,tplab,lplab,vmlab,mixstr);
	dump(header);
#ifdef DEBUGON
	if (debug) fprintf(stderr, "header=%s\n", header);
#endif
	fclose(fp);
	fclose(fp2);
}

void killall(sig)
int sig;
{
/*
**  killall sends signals to all child process and waits
**  for their death
*/
	signal(SIGTERM, SIG_IGN);
	diskunl_all();
	if (sig == 0)
	  fprintf(stderr, "Fatal Error! SIGTERM (#%d) received!\n\n\n", sig);
	fprintf(stderr, "\nSUITE III Testing over....\n\n");

	kill(0,SIGTERM);
	while (wait((int *)0) != -1) ; /* wait for all users to die */
	exit(0);
}

void dead_kid(sig)
int sig;
{
	int status;

	signal(sig,SIG_IGN);
	diskunl_all();
	fprintf(stderr, "\ndead_kid() received signal SIGHUP (%d)\n", sig);
	if (wait(&status) > 0) {
		if ((status & 0377) == 0177) { /* child proc stopped */
			fprintf(stderr,
				"Child process stopped on signal = %d\n",
				((status >> 8) & 0377));
		}
		else if ((status & 0377) != 0) { /* child term by sig */
			fprintf(stderr,
				"Child terminated by signal = %d\n",
				(status & 0177));
			if ((status & 0377) & 0200)
				fprintf(stderr,"core dumped\n");
		}
		else { /* if ((status & 0377) == 0) ** child exit()'ed */
			fprintf(stderr,
				"Child process called exit(), status = %d\n",
				((status >> 8) & 0377));
		}
	}
	kill(getppid(),SIGTERM);
	exit(1);
}

void math_err(sig)
int sig;
{
	int	status;

	signal(sig,SIG_IGN);
	fprintf(stderr, "\nmath_err() received signal SIGFPE (%d)\n", sig);
	fprintf(stderr, "Floating Point Exception error\n");
	if (wait(&status) > 0) {
		if ((status & 0377) == 0177) { /* child proc stopped */
			fprintf(stderr,
				"Child process stopped on signal = %d\n",
				((status >> 8) & 0377));
		}
		else if ((status & 0377) != 0) { /* child term by sig */
			fprintf(stderr,
				"Child terminated by signal = %d\n",
				(status & 0177));
			if ((status & 0377) & 0200)
				fprintf(stderr,"core dumped\n");
		}
		else { /* if ((status & 0377) == 0) ** child exit()'ed */
			fprintf(stderr,
				"Child process called exit(), status = %d\n",
				((status >> 8) & 0377));
		}
	}
	kill(getppid(),SIGTERM);
	exit(1);
}

init_vars()
{
	int	i, j;

	debug = gun = work = numdir = nwbd = 0;
	kids = vmgo = tpgo = lpgo = iters = 0;
	t1 = t2 = rt1 = rt2 = vm_res = 0l;
	a_tn = a_tn1 = JPMHold = 0.0;
	for (i=0; i<LIST; i++)
		results[i].d = 0.0;
	for (i=0; i<STRLEN; i++)
		wdev[i] = rdev[i] = lpcom[i] = tpcom[i] = '\0';
	for (j=0; j<MAXDRIVES; j++)
		for (i=0; i<STRLEN; i++)
			dkarr[j][i] = '\0';
}

/*
 * write out the work files that disk_rd and disk_cp use.  instead of having
 * each write out its own, we'll write out one copy ... in each directory
 * specified in the config file ... and let them all share it as input.  This
 * has two benefits, the first being that the test results are no longer
 * obfuscated with the disk write times, the second is that less overall
 * disk space is required
 */

#ifndef SYS5
#include <sys/file.h>
#else
#include <fcntl.h>
#endif

#define BUFCOUNT	100
char BUF[20*512];
char fn1[STRLEN];	/* STRLEN = 80 */
char fn1arr[MAXDRIVES][STRLEN];

diskw_all()
{
	int fd1, j, k;

	for (j=0; j<(20*512); j++)
		BUF[j] = (char)(j%127);
	if(numdir > 0) {
	    for(j=0; j<numdir; j++) {
	        sprintf(fn1,"%s/%s",dkarr[j],TMPFILE1);
		if((fd1 = creat(fn1,0666)) < 0) {
		    fprintf(stderr, "diskw_all: cannot create %s\n",fn1);
		    return(-1);
		}
		k = BUFCOUNT;
	        while(k--) {
		    if(write(fd1,BUF,sizeof BUF) != sizeof BUF) {
			fprintf(stderr, "diskw_all: cannot create %s\n",fn1);
			close(fd1);
			return(-1);
		    }
		}
		strcpy(fn1arr[j],fn1);
		close(fd1);
	    }
	}
	else {
	    sprintf(fn1,"%s",TMPFILE1);
	    if((fd1 = creat(fn1,0666)) < 0) {
		fprintf(stderr, "diskw_all: cannot create %s\n",fn1);
		return(-1);
	    }
	    k = BUFCOUNT;
	    while(k--) {
		if(write(fd1,BUF,sizeof BUF) != sizeof BUF) {
		    fprintf(stderr, "diskw_all: cannot create %s\n",fn1);
		    close(fd1);
		    return(-1);
		}
	    }
	    strcpy(fn1arr[0],fn1);
	    close(fd1);
	}
	return(0);
}
		
/*
 * get rid of 'em 
 */
diskunl_all()
{
	int  j;

	if(numdir > 0) {
	    for(j=0; j<numdir; j++) {
		unlink(fn1arr[j]);
	    }
	}
	else {
	    unlink(fn1arr[0]);
	}
}

/*
 * If adaptive flag is on and we have enough data points, then
 * "adaptively" adjust the increment.
 * 12/12/89 TVL
 */
int adjust_inc(inc, j)
int	inc, j;
{
	static int	a_cnt=0;	/* count data points */
	double		tmp, tmp2;
	double		a_tn_avg;

	tmp2 = (double)inc;
	if ((++a_cnt >= ADAPT_START) || (minusers[j] >= 20) ) {
		a_tn_avg = (a_tn + a_tn1) / 2.0;
		if (a_tn_avg == 0.0)	a_tn_avg = 1.0;
		if (a_tn == 0.0)	a_tn = 1.0;
		tmp = a_tn1 / a_tn;
		if (tmp<THRESHOLD_MIN)
			tmp2 *= (2.0 / (a_tn / a_tn_avg));
		else if (tmp>THRESHOLD_MAX)
			tmp2 /= (2.0 / (a_tn / a_tn_avg));
		if (tmp2 < 1.0)
			tmp2 = (double)inc;
	}
	return((int)tmp2);
}

/*
 * reduce_list()
 *	Used in sampling without replacement in runtap().
 *	Contributed by Jim Summerall of DEC.
 * 12/13/89 TVL
 */
int reduce_list(wlist)
int	wlist[];
{
	register int	i, total;
	int		rlist[WORKLD];

	for (i=0, total=0; i<work; i++) {
		if (wlist[i] == 0)
			continue;
		else
			rlist[total++] = i;
	}
	if (total)
		i = rand() % total;
	else {
		fprintf(stderr, "FATAL ERROR - DIVIDE BY ZERO\n");
		fprintf(stderr, "reduce_list(): total = 0\n");
		exit(1);
	}
	return(rlist[i]);
}

suite3_out(suite3_users, jobs_min,real_time,cpu_time,job_per_sec_per_user)
int suite3_users;
double jobs_min,real_time,cpu_time,job_per_sec_per_user;
{
	FILE *fptr;

        if ( (fptr = fopen("suite3.ss","a") ) == NULL )	{
		perror("multiuser");
                fprintf(stderr, "Can not open %s\n","suite3.ss");
		exit(3);
	}

	fprintf(fptr, "%d\t%.1lf\t%.1f\t%.1f\t%.4f\n", suite3_users, jobs_min,real_time,cpu_time,job_per_sec_per_user);
	fclose(fptr);
}

suite3_header(name,suite3_time)
char *name;
char *suite3_time;
{
	FILE *fptr;

        if ( (fptr = fopen("suite3.ss","a") ) == NULL )	{
		perror("multiuser");
                fprintf(stderr, "Can not open %s\n","suite3.ss");
		exit(3);
	}

	fprintf(fptr, "Benchmark\tVersion\tMachine\tRun Date\n");
	fprintf(fptr, "Suite III\t\"%s\"\t%s\t%s\n\n", version, name, suite3_time);
	fprintf(fptr, "Users\tJobs/Min\tReal\tCPU\tJob/sec/user\n");

	fclose(fptr);
}
