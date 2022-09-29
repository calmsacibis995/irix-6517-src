/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.24 $"

#include <assert.h>
#include <strings.h>
#include <mutex.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <arraysvcs.h>
#include <arraysvcs_pvt.h>
#include <bstring.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h" /* for now */

#define	NCPRARGS	16
#define	ARCPR		"array_cpr_helper"
#if _MIPS_SIM == _ABI64
#define	LIBARRAY	"/usr/lib64/libarray.so"
#else
#define	LIBARRAY	"/usr/lib32/libarray.so"
#endif

#define CKPT_CLIENT_SYNC_GO "ckpt_client_sync_go"
#define	CKPT_CHECKPOINT_TIMEOUT		300
#define	CKPT_RESTART_TIMEOUT		300

#define	ARRAYFDS_INIT(a,b)	{(a).nfds = 0;		\
				(a).fdarray = malloc(b->numresults * sizeof(int));}
#ifdef DEBUG_MEMALLOC
#define	ARRAYFDS_FREE(a)	(_free((a).fdarray))
#else
#define	ARRAYFDS_FREE(a)	(free((a).fdarray))
#endif

static int ckpt_issue_ascmd(ascmdreq_t *, ash_t, int);
static int extend_pathname(char *, const char *);
static int ckpt_exam_fds(int, int, char *, char *, int input);

extern int shutdown(int, int);

extern long *interactive_restart;
extern char *ckpt_array_name;
extern ch_t cpr; 		/* main CPR handle */

/* One per io type per array */
struct ckpt_arrayfds {
	int		nfds;		/* # of elements in fdarray */
	int		*fdarray;
};


static ulong no_machines = 0;
static ulong nclients_stopped = 0;
static ulong nclients_dumped = 0;
static ulong nclients_restarted = 0;
static int   client_go;

/* ARGSUSED */
int
ckpt_run_checkpoint_arserver(const char *path, ash_t ash, u_long type,
	struct ckpt_args *args[], size_t nargs)
{
	ascmdreq_t req;
	char argv[NCPRARGS][CPATHLEN];
	char *argvp[NCPRARGS];
	int argc;
	int i;

	/*
	 * rebuild the request arguments and add "-A"
	 */
	i = 0;
	sprintf(argv[i++], "%s", ARCPR); 
	sprintf(argv[i++], "-c");
	if (extend_pathname(argv[i++], path)) return (-1);
	sprintf(argv[i++], "-p");
	sprintf(argv[i++], "0x%016llx:%s", ash, ckpt_type_str(type));

        if (cpr_flags & CKPT_CHECKPOINT_UPGRADE)
                sprintf(argv[i++], "-u");

        if (cpr_flags & CKPT_CHECKPOINT_KILL)
                sprintf(argv[i++], "-k");
        else if (cpr_flags & CKPT_CHECKPOINT_CONT)
                sprintf(argv[i++], "-g");

	if (cpr_debug & 0x1)
		sprintf(argv[i++], "-v");
	if (cpr_debug & 0x2)
		sprintf(argv[i++], "-V");
	argc = i;
	for (i = 0; i < argc; i++) argvp[i] = argv[i];

	IFDEB1(cdebug("checkpointing arrayserv ASH 0x%016llx to file %s\n", ash, path));

	bzero(&req, sizeof(ascmdreq_t));

	req.flags   = ASCMDREQ_NOWAIT | ASCMDREQ_INTERACTIVE;
	req.ioflags = ASCMDIO_FULLIO;
	req.numargs = argc;
	req.args = argvp;

	return (ckpt_issue_ascmd(&req, ash, CKPT_CHECKPOINT_TIMEOUT));
}

int
ckpt_run_restart_arserver(const char *path, ckpt_id_t id)
{
	ascmdreq_t req;
	char argv[NCPRARGS][PSARGSZ];
	char *argvp[NCPRARGS];
	int argc;
	int i;

	IFDEB1(cdebug("restarting arrayserv statefile from file: %s\n", path));
	/*
	 * rebuild the request arguments and add "-A"
	 */
	i = 0;
	sprintf(argv[i++], "%s", ARCPR); 

	if (cpr_debug & 0x1)
		sprintf(argv[i++], "-v");
	if (cpr_debug & 0x2)
		sprintf(argv[i++], "-V");

        if (cpr_flags & CKPT_RESTART_INTERACTIVE)
                sprintf(argv[i++], "-j");

	sprintf(argv[i++], "-r");
	if (extend_pathname(argv[i++], path)) return (-1);
	argc = i;
	for (i = 0; i < argc; i++) argvp[i] = argv[i];
	IFDEB1(cdebug("# of args: %d\n", i));

	bzero(&req, sizeof(ascmdreq_t));

	/*
	 * The array will be filled by decoing ASH. If ASH has been changed, then
	 * default array has to be used.
	 */
	req.array   = 0;
	req.flags   = ASCMDREQ_NEWSESS | ASCMDREQ_INTERACTIVE;
	req.ioflags = ASCMDIO_FULLIO;
	req.numargs = argc;
	req.args = argvp;
	
	return (ckpt_issue_ascmd(&req, (ash_t)id, CKPT_RESTART_TIMEOUT));
}


/*
 * This is used at checkpoint time and the command line has -ASH..
 * if there is only one machine in the array, then we can restart the
 * process just by the local machine.
 */
int 
ckpt_is_ash_local(void)
{
	int local;
	asmachinelist_t *mlist;
	asserver_t server;
	void *ashandle;

	asserver_t (*casopenserver)(const char *, int);
	asmachinelist_t *(*caslistmachines)(asserver_t, const char *);
	void (*cascloseserver)(asserver_t);
	void (*casfreemachinelist)(asmachinelist_t *MachineList, uint32_t Flags);

	if ((ashandle = dlopen(LIBARRAY, RTLD_LAZY)) == NULL) {
		cerror("Failed to open library %s (%s)\n", LIBARRAY, dlerror());
		return (-1);
	}
	casopenserver = (asserver_t (*)())dlsym(ashandle, "asopenserver");
	caslistmachines = 
		(asmachinelist_t *(*)())dlsym(ashandle, "aslistmachines");
	casfreemachinelist = (void (*)())dlsym(ashandle, "asfreemachinelist");
	cascloseserver = (void (*)())dlsym(ashandle, "ascloseserver");

	if(casopenserver == NULL || caslistmachines == NULL ||
	   casfreemachinelist == NULL || cascloseserver == NULL) {
		cnotice("No libarray %s installed, assume no cluster "
			"casopenserver %d caslistmachines = %d\n",
			LIBARRAY, casopenserver, caslistmachines);
		return 1;
	}
	
	if((server = casopenserver(NULL, -1)) == NULL) {
		cnotice("Can't open server, assume no cluster\n");
		dlclose(ashandle);
		return 1;
	}
	if((mlist = caslistmachines(server, NULL)) == NULL) {
		cnotice("Can't find machines in cluster, assume no cluster\n");
		dlclose(ashandle);
		return 1;
	}
	local = (mlist->nummachines >1)? 0:1;
	casfreemachinelist(mlist, ASFLF_FREEDATA);
	IFDEB1(cdebug("ckpt_is_ash_local %d, nmachines %d\n",
						local, mlist->nummachines));
	cascloseserver(server);
	dlclose(ashandle);
	return local;
}

/*
 * ckpt_process_sync_points: return:
 * 0: need to sync, 1: sync is over and success. 2: sync is over and failed.
 */
static int 
ckpt_process_sync_points(ascmdrsltlist_t *rp, ascmdrslt_t *curp)
{
	int rc, i;
	extern int ckpt_restart_is_client;

	/*
	 * scan for the sync points
	 */
	rc = ckpt_exam_fds(curp->stdoutfd, 1, curp->machine, "STDOUT", 1);

	/*
	 * If one client fails to checkpoint or restart, abort all
	 */
	if (rc || (interactive_restart && *interactive_restart == -1)) {
		if (rc) {
			cerror("The CPR request failed on machine %s. Abort "
				"the entire CPR request.\n", curp->machine);
		}
		else {
			cerror("Failed to restore an interactive array job. "
				"Abort\n");
			kill(getppid(), SIGABRT);
		}
		for (i = 0; i < no_machines; i++) {
			ascmdrslt_t *cp;
			char sig;

			sig = SIGABRT;
			cp = rp->results[i];
			/* read error msg from that machine 
			 * whatever we can */
			ckpt_exam_fds(cp->stderrfd, 2, cp->machine, "STDERR",2);
			write(cp->signalfd, &sig, 1);

			shutdown(cp->stderrfd, 1);
			shutdown(cp->stdoutfd, 2);
			IFDEB1(cdebug("shutdown socket conn to machine %s\n",
							cp->machine));
		}
		/* failed, stop sync */
		return (2);
	}

	/*
	 * All client stopped or dumped, let them continue CPR
	 */
	IFDEB1(cdebug("mach#=%d stopped=%d dumped=%d restarted=%d\n",
	no_machines, nclients_stopped, nclients_dumped, nclients_restarted));
	if (nclients_stopped == no_machines || nclients_dumped == no_machines ||
	    nclients_restarted == no_machines) {

		for (i = 0; i < no_machines; i++) {
			ascmdrslt_t *cp;

			cp = rp->results[i];
			IFDEB1(cdebug("send SIGCONT to machine %s\n", 
								cp->machine));
			write(cp->stdinfd, CKPT_CLIENT_SYNC_GO , 
						strlen(CKPT_CLIENT_SYNC_GO));
		}
		if (nclients_stopped == no_machines) {
			IFDEB1(cdebug("All clients are stopped\n"));
			nclients_stopped = 0;
		} else if (nclients_dumped == no_machines) {
			IFDEB1(cdebug("All clients are finished saving "
				      "statefiles\n"));
			/* success on sync */
			return (1);
		} else if (nclients_restarted == no_machines) {
			if (cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE) {
				if (unblockproc(getppid()) < 0) {
					cerror("Failed to unblock cpr waitor"
						 "(%s)\n", getppid(), STRERR);
				}
			}
			IFDEB1(cdebug("All restarted clients are ready to "
					"run\n"));
			/* success on sync */
			return (1);
		}
	}
	/* need continue to sync */
	return (0);
}

static int
ckpt_issue_ascmd(ascmdreq_t *rp, ash_t ash, int timeout)
{
	ascmdrsltlist_t *rsltlist;
	void *ashandle;
	ascmdrsltlist_t *(*cascommand)(asserver_t, ascmdreq_t *);
	asashinfo_t *(*casgetashinfo)(asserver_t, ash_t, asashinforeq_t *);
	void (*casperror)();
	void (*cassetloginfo)();
	const char *(*casstrerror)(aserror_t);
	void (*casfreecmdrsltlist)(ascmdrsltlist_t *, int);
	void (*casfreeashinfo)(asashinfo_t *, int);
	asserver_t (*casopenserver)(const char *, int);
	void (*cascloseserver)(asserver_t);
	int (*cassetserveropt)(asserver_t, int, const void *, int);
	asserver_t server;
	asashinfo_t *ashinfo;
	struct ckpt_arrayfds mainsocket; /* stdin/out */
	struct ckpt_arrayfds ctrlsocket; /* signal/stderr */
	fd_set read_fdset;
	struct timeval select_timeout;
	int i, rc = 0, rc2;

	if ((ashandle = dlopen(LIBARRAY, RTLD_LAZY)) == NULL) {
		cerror("Failed to open library %s (%s)\n", LIBARRAY, dlerror());
		return (-1);
	}

	cascommand = (ascmdrsltlist_t *(*)())dlsym(ashandle, "ascommand");
	casgetashinfo = (asashinfo_t *(*)())dlsym(ashandle, "asgetashinfo");
	casperror = (void (*)())dlsym(ashandle, "asperror");
	cassetloginfo = (void (*)())dlsym(ashandle, "ASSetLogInfo");
	casstrerror = (const char *(*)())dlsym(ashandle, "asstrerror");
	casfreecmdrsltlist = (void (*)())dlsym(ashandle, "asfreecmdrsltlist");
	casfreeashinfo = (void (*)())dlsym(ashandle, "asfreeashinfo");
	casopenserver = (asserver_t (*)())dlsym(ashandle, "asopenserver");
	cascloseserver = (void (*)())dlsym(ashandle, "ascloseserver");
	cassetserveropt = (int (*)())dlsym(ashandle, "assetserveropt");

	if (!cascommand || !casgetashinfo || !casperror || !cassetloginfo ||
		!casstrerror || !casfreecmdrsltlist || !casfreeashinfo ||
		!casopenserver || !cascloseserver || !cassetserveropt) {
		cerror("Failed to load library %s (%s)\n", LIBARRAY, dlerror());
		return (-1);
	}

#ifdef DEBUG_ARRAY
	IFDEB1(cdebug(""));
	for (i = 0; i < rp->numargs; i++) 
		IFDEB1(fprintf(stderr, "%s ", rp->args[i]));
	IFDEB1(fprintf(stderr, "\n"));

	if (cpr_debug)
		cassetloginfo(ASDEBUG_ALL, ASLOG_DBGTOSTDERR|ASLOG_ERRTOSTDERR);
#endif

	/*
	 * set up timeout value for the default server (unless we can map ASH
	 * back to a particular server)
	 */
	server = casopenserver(ckpt_array_name, -1);
	if (cassetserveropt(server, AS_SO_TIMEOUT, &timeout, sizeof(timeout)) != 0) {
                casperror("Unable to set server TIMEOUT");
        }

	/*
	 * find the array given ASH
	 */
	if ((ashinfo = casgetashinfo(server, ash, NULL)) == NULL) {
		casperror("CPR array command casgetashinfo failed");
		cascloseserver(server);
		dlclose(ashandle);
		return (-1);
	}
	rp->array = ashinfo->array;
	IFDEB1(cdebug("about to issue CPR array request to ARRAY %s\n", rp->array));


	/*
	 * use default server (AS daemon) to start all clients
	 */
	rsltlist = cascommand(server, rp);
	cascloseserver(server);

	if (rsltlist == NULL) {
		casperror("CPR array command failed");
		casfreeashinfo(ashinfo, 0);
		dlclose(ashandle);
		return (-1);
	}

	/*
	 * Init all array read fds
	 */
	ARRAYFDS_INIT(mainsocket, rsltlist);
	ARRAYFDS_INIT(ctrlsocket, rsltlist);
	FD_ZERO(&read_fdset);

	no_machines = rsltlist->numresults;
	IFDEB1(cdebug("Number of nodes: %d\n", no_machines));
	for (i = 0; i < no_machines; i++) {
		ascmdrslt_t *curp;

		curp = rsltlist->results[i];

		/* decode the exit status */
		if (aserrnoc(curp->error) != ASE_OK) {
			cerror("machine %12s: %s\n", curp->machine,
				(*casstrerror)(curp->error));
			rc = -1;
			goto cleanup;
		}
		if (aserrwhyc(curp->error) != ASOK_CONNECTED) {
			cerror("machine %12s: Command terminated with signal %d\n",
				WTERMSIG(curp->status));
			rc = -1;
			goto cleanup;
		}
		/* record all i/o sockets */
		FD_SET(curp->stdoutfd, &read_fdset);
		FD_SET(curp->stderrfd, &read_fdset);
		mainsocket.fdarray[i] = curp->stdoutfd;
		ctrlsocket.fdarray[i] = curp->stderrfd;
		mainsocket.nfds++;
		ctrlsocket.nfds++;
		IFDEB1(cdebug("fdarray: mainfd=%d ctrlfd=%d\n", curp->stdoutfd,
			curp->stderrfd));
	}
	/*
	 * sync points with its clients
	 */
	select_timeout.tv_sec = CKPT_CHECKPOINT_TIMEOUT;
	select_timeout.tv_usec = 0;
	for (;;) { 
		fd_set ready_fdset;

		/* Wait unless somebody is ready */ 
		ready_fdset = read_fdset;
		rc = select(getdtablehi(), &ready_fdset, NULL, NULL, &select_timeout);
		if (rc <= 0) {
			if (oserror() == EINTR)
				continue;
			if (rc == 0) {
				cerror("SELECT timeout when waiting for the array "
					"command responses\n");
				rc = -1;
			}
			else 
				perror("ckpt_issue_ascmd:select");

			/* there is something wrong. We won't continue.
			 * abort all the cpr commands 
			 */
			for (i = 0; i < no_machines; i++) {
				ascmdrslt_t *cp;
				char sig;

				cp = rsltlist->results[i];
				sig = SIGABRT;
				/* read error msg from that machine 
				 * whatever we can */
				ckpt_exam_fds(cp->stderrfd, 2, 
						cp->machine, "STDERR", 2);
				write(cp->signalfd, &sig, 1);
				shutdown(cp->stderrfd, 1);
				shutdown(cp->stdoutfd, 2);
				IFDEB1(cdebug("shutdown socket connections to "
						"machine %s\n", cp->machine));
			}
			goto cleanup;
		}
		IFDEB1(cdebug("# of fds selected = %d\n", rc));

		/*
		 * loop on all nodes
		 */
		rc = 0;

		for (i = 0; i < no_machines; i++) {
			ascmdrslt_t *curp;

			curp = rsltlist->results[i];

			/*
			 * If any client reports error, abort the whole CPR request
			 */
			if (FD_ISSET(curp->stderrfd, &ready_fdset)) {
				ckpt_exam_fds(curp->stderrfd, 2, curp->machine,
					"STDERR", 2);
			}

			/*
			 * check sync data and other stdout data
			 */
			if (interactive_restart && *interactive_restart == 1) {
				nclients_restarted++;
				*interactive_restart = 0;
			}
			if (FD_ISSET(curp->stdoutfd, &ready_fdset)) {
				/*
				 * return non-null when all sync points 
				 * are checked or aborted.
				 */
				rc2 = ckpt_process_sync_points(rsltlist, curp);
				if(rc2 != 0) {
					ckpt_exam_fds(curp->stderrfd, 2,
						curp->machine, "STDERR", 2);
					/* rc2 =1 success, rc2 =2 fail */
					rc = rc2 -1;
					goto cleanup;
				}
			}
		}
	}

cleanup:
	ARRAYFDS_FREE(mainsocket);
	ARRAYFDS_FREE(ctrlsocket);
	casfreecmdrsltlist(rsltlist, ASFLF_FREEDATA | ASFLF_UNLINK);
	casfreeashinfo(ashinfo, 0);
	dlclose(ashandle);
	return (rc);
}

void
ckpt_catch_alarm()
{
	struct utsname mach;

	uname(&mach);

	cerror("Checkpoint timeout from PID %d: ASH 0x%x on machine %s.\n",
		getash(), getpid(), mach.nodename);
	cerror("Aborting...\n");
}

void
ckpt_reach_sync_point(char *sync_point, char stop)
{
	int rc;
	fd_set read_fdset;
	fd_set ready_fdset;
	struct timeval select_timeout;
	
	FD_ZERO(&read_fdset);
	FD_SET(0, &read_fdset);
	select_timeout.tv_sec = CKPT_CHECKPOINT_TIMEOUT;
	select_timeout.tv_usec = 0;

	/*
	 * send the sync code to the server; flush first so that we don't get
	 * duplicates due the fds inherited from ASH sockets
	 */
	fflush(stdout);
	fprintf(stdout, "%s", sync_point);
	fflush(stdout);
	IFDEB1(cdebug("reaching sync point %s for pid %d, stop = %d\n", 
						sync_point, getpid(), stop));
	if (stop) { /* wait until master send us a signal */
		client_go = 0;
		while(!client_go) {
			ready_fdset = read_fdset;
			rc = select(1,&ready_fdset,NULL,NULL, &select_timeout);
			if((rc <=0) && (oserror()!=EINTR) 
						&& (oserror() !=EAGAIN))  {
				/* something got wrong */
				return;	
			}
			/* ckpt_exam_fds will set client_go */
			if(ckpt_exam_fds(0, 2, "CLIENT", "CLIENT", 0))
				return;
		}
	}
	IFDEB1(cdebug("finished sync point %s for pid %d, stop = %d\n", 
						sync_point, getpid(), stop));
}

int
ckpt_get_localpids(ckpt_id_t id, pid_t **pidp, int *npids)
{
	aspidlist_t *p;
	void *ashandle;
	aspidlist_t *(*caspidsinash)(ash_t);
	void (*casperror)();

	ashandle = dlopen(LIBARRAY, RTLD_LAZY);
	caspidsinash = (aspidlist_t *(*)())dlsym(ashandle, "aspidsinash");
	casperror = (void (*)())dlsym(ashandle, "asperror");

	IFDEB1(cdebug("Find pids for ASH 0x%016llx\n", id));
	if ((p = caspidsinash((ash_t)id)) == NULL) {
		(*casperror)("aspidsinash");
		dlclose(ashandle);
		return (-1);
	}
	dlclose(ashandle);

	/*
	 * need to sync even w/o any client process running on this node.
	 */
	if (CKPT_IS_GASH(cpr.ch_type, cpr.ch_flags) && (p->numpids == 0)) {
		ckpt_reach_sync_point(CKPT_STOP_SYNC_POINT, 1);
		exit(0);
	}
	*npids = p->numpids;
	*pidp = p->pids;

	IFDEB1(cdebug("# of PIDS for ASH %lld: %d\n", id, p->numpids));
	return (0);
}

static int
extend_pathname(char *buf, const char *path)
{
	char *cwd;

	if (*path == '/') {
		/* already full path */
		sprintf(buf, "%s", path);
		return (0);
	}
	/*
	 * extend to full pathname since arrayd issues cmd with its own cdir
	 */
	if ((cwd = getcwd(NULL, 64)) == NULL) {
		perror("extend_pathname:getcwd");
		return (-1);
	}
	sprintf(buf, "%s/%s", cwd, path);
	free(cwd);
	return (0);
}

#define	IOBUFLEN 4096
#define FOUND_SYNC_POINT(w) (!strncmp(&buf[start], (w), end-start))

/* ckpt_exam_fds: return 0 on success, 1 on failure. */

/* ARGSUSED */
static int
ckpt_exam_fds(int infd, int outfd, char *mach, char *tag, int input)
{
	char buf[IOBUFLEN];
	ssize_t len;
	static int newline = 1;
	int infd_alive = 0;

	while ((len = read(infd, buf, IOBUFLEN)) > 0) {
		int end = 0, start;
		infd_alive = 1;
		for (;;) {
			if (end >= len)
				break;

			start = end;

			/* find the orig. newline */
			while (buf[end] != '\n' && buf[end] != 0 && end < (len-1))
				end++;
			end++;
 
			if (input == 1) { /* stdout */

				/*
				 * check if any client fails to checkpoint
				 */
				if (FOUND_SYNC_POINT(CKPT_ABORT_SYNC_POINT)) {
#ifdef DEBUG_ARRAY
					write(outfd, mach, strlen(mach));
					write(outfd, ": ", 2);
					write(outfd, &buf[start], end-start);
#endif
					return (1);
				}

				if (FOUND_SYNC_POINT(CKPT_STOP_SYNC_POINT)) {
					nclients_stopped++;
#ifndef DEBUG_ARRAY
					continue;
#endif
				}
				else if (FOUND_SYNC_POINT(CKPT_DUMP_SYNC_POINT)) {
					nclients_dumped++;
#ifndef DEBUG_ARRAY
					continue;
#endif
				}
				else if (FOUND_SYNC_POINT(CKPT_RESTART_SYNC_POINT)) {
					nclients_restarted++;
#ifndef DEBUG_ARRAY
					continue;
#endif
				}
			} else if( input == 0) { /* stdin */
				/* client uses this to read data from master */
				if(FOUND_SYNC_POINT(CKPT_CLIENT_SYNC_GO)) {
					client_go = 1;
					return 0;	
				}
			}
			if (newline) {
				write(outfd, mach, strlen(mach));
				write(outfd, ": ", 2);
#ifdef DEBUG_ARRAY
				write(outfd, "(", 1);
				write(outfd, tag, strlen(tag));
				write(outfd, ") ", 2);
#endif
			}
			write(outfd, &buf[start], end-start);
			if (buf[end-1] == '\n' || buf[end-1] == 0)
				newline = 1;
			else
				newline = 0;
		}
	};
	if (len < 0 && oserror() != EAGAIN) {
		cerror("Failed to read from infd %d (%s)\n", infd, STRERR);
		return 1;
	}
	if((len == 0) && (!infd_alive)) {
		/* broken pipe */
		return 1;
	}
	if (len > 0)
		write(outfd, "\n", 1);
	return (0);
}
