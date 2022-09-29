/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.39 $"

/*
 * xlv_labd - User level daemon for processing kernel requests to update
 * disk labels.
 */
#define _BSD_SIGNALS
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bstring.h>
#include <time.h>
#include <paths.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <sys/lock.h>
#include <limits.h>
#include <sys/prctl.h>
#include <sched.h>
#include <sys/wait.h>
#include <xlv_cap.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_labd.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_utils.h>
#include <xlv_labd.h>

#define	XLV_DAEMON	"xlv_labd"

#define	dbg_printf	if (do_debug) printf

char msg_cant_read_vh[] = "Failed disk volume headers (re)read";
char msg_cant_create_fifo[] = "Failed to create xlv_labd fifo";
char msg_cant_open_fifo[] = "Failed to open xlv_labd fifo";
char msg_created_fifo[] = "Created xlv_labd fifo";
char msg_no_cursor[] = "Failed to get an XLV cursor";
char msg_no_labels[] = "There are no XLV labels on any disks.";
char msg_no_rqst[] = "Failed to get next XLV config request";
char msg_skip_vhread[] = "skip reread of disk labels";
char msg_read_labels[] = "Reading xlv labels from disks";

char fmt_cant_find_vol[] = "Failed to find vol %s at %s";
char fmt_cant_find_plex[] = "Failed to find plex %s at %s";
char fmt_remove_ve[] = "Remove from volume %s: %s at %s";
char fmt_change_ve_state[] = "Changing ve %s (%s) state from %s to %s";
char fmt_make_active[] = "Making active: %s at %s";
char fmt_unknown_op[] = "Unknown operation %d at %s";


typedef struct {
	uuid_t		*uuid_p;
	int		state;
} set_ve_state_arg_t;

static int set_ve_state (xlv_oref_t *I_oref, set_ve_state_arg_t *set_arg);

int	do_debug = 0;
int	do_verbose = 0;
char	*logname = NULL;
int	skipped_reread = 0;		/* re-read */
pid_t	slave_pid;			/* child pid who does all the work */

/*
 * Ensure that we at least change the facility string to syslog by using 
 * xlv_openlog().  xlv_closelog() only does a 'last' close of the SYSLOG,
 * which avoids any potential deadlock issues with labd waiting on an filesystem
 * open to complete.
 */
#define	slog(p,m)	\
	xlv_openlog(XLV_DAEMON, LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON), \
	syslog((LOG_DAEMON|(p)),(m)), \
	xlv_closelog()

static void	usage(void);
static void	background(void);
void xlv_labd_slave(void *);

xlv_vh_entry_t		*xlv_vh_list = NULL;	/* volume header list */
xlv_oref_t		*oref_list = NULL;	/* object reference list */
xlv_attr_cursor_t	last_cursor;		/* cursor to indicate 
						   changes in kernel
						   configuration. */
xlv_tab_vol_t		*tab_vol = NULL;
xlv_tab_t		*tab_sv = NULL;

uuid_t			root_vol_uuid;		/* root volume uuid */
xlv_ves_active_rqst_t	request;		/* last request read */
boolean_t		inprogress;		/* in the middle of processing
						 * a request */
int			inprogress_cnt;		/* number of times we've tried
						 * to process the last request*/


/*
 * Wait for all of our children, and gather their exit status 
 */
/*ARGSUSED*/
static void
sighandle(int sig)
{
	pid_t pid;
	int status;
	char buf[80];

	while ((pid = wait3(&status, WNOHANG, NULL)) > 0) {

		/* If the slave process dies (other than for EPIPE), then 
		 * restart it
		 */
		if (pid == slave_pid) {
			if (WEXITSTATUS(status) == EPIPE) {
				sprintf(buf, "slave process exited (%d). "
			             "Kernel plexing software not initialized",
				        WEXITSTATUS(status));
				slog(LOG_INFO, buf);
				exit(EPIPE);
			}

			sprintf(buf, "slave process exited (%d). Restarting.",
			        WEXITSTATUS(status));
			slog(LOG_INFO, buf);

			if ((slave_pid = 
				sproc(xlv_labd_slave, PR_SADDR|PR_SFDS)) < 0) {

				perror(XLV_DAEMON);
				exit(-1);
			}
		}
	}
}

#define MAX_RETRY	10
/*
 * Read xlv labels from all disk volume headers and
 * create all the  xlv objects.
 */
int
xlv_labd_init(void)
{
	int		status;
	xlv_oref_t	*oref, *next;
	int 		nretries = 0;
	extern boolean_t xlv_label_recovery;
	
	/* labd doesn't want to do default label recovery
	 * cuz the initial xlv_assemble call should have
	 * recovered the labels if they were inconsistent.
	 */
	xlv_label_recovery = B_FALSE;

#ifdef DEBUG
	slog(LOG_INFO, msg_read_labels);
#endif

re_reread:

	if (xlv_vh_list) {
		/*
		 * Already have a vh list, so free the current
		 * vh list before getting a new one.
		 */
		xlv_lab2_free_vh_list(&xlv_vh_list);

		/*
		 * Free up current oref list.
		 */
		oref = next = oref_list;
		while (next) {
			oref = next;
			next = oref->next;
			free(oref);
		}
		oref_list = NULL;

		free_tab_vol(&tab_vol);
		free_tab_subvol(&tab_sv);
	}

	/*
	 * Read the XLV label from the disk volume header.
	 */
	xlv_lab2_create_vh_info_from_hinv (&xlv_vh_list, NULL, &status);
	if (xlv_vh_list == NULL) {
		printf("%s\n", msg_no_labels);
		slog(LOG_INFO, msg_no_labels);
		return (-1);
	}

	/*
	 * Generate the oref list and filter out incomplete volumes.
	 */
	status = xlv_vh_list_to_oref (
		xlv_vh_list, &oref_list, &tab_vol, &tab_sv, 1, XLV_READ_ALL);

	/* The labels weren't in a consistent state, retry to see if the
	 * problem fixes itself.
	 */
	if (status == EAGAIN) {
		/* If labels still aren't consistent after MAX_RETRY attempts
		 * enable label recovery
		 */
		if (++nretries > MAX_RETRY)
			xlv_label_recovery = B_TRUE;

		dbg_printf("xlv_labd: inconsistent label found.  retry #%d\n",
			    nretries);
		/*
		 * Re-read label from the disk volume header.
	  	 */
		goto re_reread;
	}

	return (0);

} /* end of xlv_labd_init() */


/*
 * Enumeration routine to find a given volume element set its state.
 * Return 0 to continue search or 1 to terminate search. 
 */
static int
set_ve_state (xlv_oref_t *I_oref, set_ve_state_arg_t *set_arg)
{
	xlv_tab_vol_elmnt_t	*vep;
	uint_t			status;
	uuid_t			*uuid_p;
	int			state;
	int			priority;

	char			*uuid_str;
	char			buf[1024];
	char			name[512];

	uuid_p = set_arg->uuid_p;
	state = set_arg->state;

	if (state == XLV_VE_STATE_OFFLINE) {
		priority = LOG_ALERT;
	} else {
		priority = LOG_INFO;
	}

	vep = XLV_OREF_VOL_ELMNT (I_oref);
	if (vep == NULL) {
		ASSERT(vep != NULL);
		return(0);
	}

	if (uuid_is_nil(uuid_p, &status)) {
		return(0);
	}

	if (uuid_equal(uuid_p, &vep->uuid, &status)) {
		/*
		 * Update volume element state and terminate search.
		 */
		xlv_oref_to_string(I_oref, name);
		uuid_to_string(uuid_p, &uuid_str, &status);
		sprintf(buf, fmt_change_ve_state,
			name, uuid_str, xlv_ve_state_str(vep->state),
			xlv_ve_state_str(state));
		free(uuid_str);
		slog(priority, buf);
		dbg_printf("%s: %s\n", XLV_DAEMON, buf);

		vep->state = (unsigned char)state;
		return(1);	/* terminates search */
	}

	return(0);

} /* end of set_ve_state() */


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-dv] [-l filename]\n", XLV_DAEMON);
	fprintf(stderr, "\t-d  Debug mode. Stay in foreground.\n");
	fprintf(stderr, "\t-l  filename; log messages to file. NOT IMPLEMENTED\n");
	fprintf(stderr, "\t-v  Verbose mode.\n");
	exit(1);
}


static void
background(void)
{
	char	msg[1024];

	if (_daemonize(0, -1, -1, -1) < 0) {
		sprintf(msg, "%s: failed to daemonize", XLV_DAEMON); 
		perror(msg);
		exit(1);
	}
}


/*
 *
 * Kernel is taking these volume elements offline.
 * All these ve's belong to the same volume.
 * Update these ve's state and update timestamp of
 * the volume to which this ve belongs.
 */
void
xlv_labd_make_offline (
	xlv_config_rqst_t	*offline_rqst,
	char			*time_str)
{
	xlv_oref_t		oref;
	uuid_t			*uuid_p;
	char			*uuid_str;
	char			buf[1024];
	uint_t			status;
	unsigned		i;
	set_ve_state_arg_t	set_arg;
	xlv_tab_vol_entry_t	*vol_p;

	/*
	 * First find the volume oref.
	 */
	XLV_OREF_INIT(&oref);
	uuid_p = &offline_rqst->vol_uuid;

	XLV_OREF_SET_UUID(&oref, uuid_p);

	if (0 > xlv_oref_resolve_from_list(&oref, oref_list)) {
		uuid_to_string(uuid_p, &uuid_str, &status);
		sprintf(buf, fmt_cant_find_vol, uuid_str, time_str);
		free(uuid_str);
		slog(LOG_INFO, buf);
		dbg_printf("%s: %s\n", XLV_DAEMON, buf);

		/*
		 * XXX Failed to find the volume to mark offline.
		 * Do we have the latest disk vh? If not, let's
		 * re-read the disk vh and try it again.
		 *
		 * if (skipped_reread) {}
		 */
		return;
	}

	/*
	 * Disable incore updates and label writes for read-only volumes.
	 */
	 if ((vol_p = XLV_OREF_VOL(&oref)) &&
	     (vol_p->flags & XLV_VOL_READONLY))
	      return;

	/*
	 * Have the volume oref, now find the ve's.
	 */
	for (i=0; i < offline_rqst->num_vol_elmnts; i++) {
		uuid_p = &offline_rqst->vol_elmnt_uuid[i];
		uuid_to_string(uuid_p, &uuid_str, &status);
		sprintf(buf, fmt_remove_ve,
			XLV_OREF_VOL(&oref)->name, uuid_str, time_str);
		free(uuid_str);
		slog(LOG_INFO, buf);
		dbg_printf("%s: %s\n", XLV_DAEMON, buf);

		set_arg.uuid_p = uuid_p;
		set_arg.state = XLV_VE_STATE_OFFLINE;

		/*
		 * XXXjleong	Should this really be:
		 * xlv_for_each_ve_in_subvol()?
		 */
		xlv_for_each_ve_in_vol (
			&oref, (XLV_OREF_PF)set_ve_state, (void *)&set_arg);

		/*
		 * May not have found the ve to set the state to offline.
		 * But that's okay as long as the timestamp of the other
		 * ves in the volume are updated so the missing and
		 * "offline" ve has an older timestamp.
		 */
	}

	xlv_lab2_write_oref_component (
		&xlv_vh_list, &oref, XLV_OBJ_TYPE_VOL,
		NULL, NULL, (int *)&status, XLV_LAB_WRITE_PARTIAL);

} /* end of xlv_labd_make_offline() */


/*
 * Mark the specified volume elements in a plex active.
 */
void
xlv_labd_make_plex_active (
	xlv_ves_active_rqst_t	*active_rqst,
	char			*time_str)
{
	xlv_oref_t		oref;
	uuid_t			*uuid_p;
	char			*uuid_str;
	char			buf[1024];
	uint_t			status;
	unsigned		i;
	set_ve_state_arg_t	set_arg;

	/*
	 * First find the plex.
	 */
	XLV_OREF_INIT(&oref);
	uuid_p = &active_rqst->plex_uuid;
	XLV_OREF_SET_UUID(&oref, uuid_p);

	if (0 > xlv_oref_resolve_from_list(&oref, oref_list)) {
		uuid_to_string(uuid_p, &uuid_str, &status);
		sprintf(buf, fmt_cant_find_plex, uuid_str, time_str);
		free (uuid_str);
		slog(LOG_INFO, buf);
		dbg_printf("%s: %s\n", XLV_DAEMON, buf);
		return;
	}

	/*
	 * We have the plex, so find the ve and mark it active.
	 */
	for (i=0; i < active_rqst->num_vol_elmnts; i++) {
		uuid_p = &active_rqst->vol_elmnt_uuid[i];
		uuid_to_string(uuid_p, &uuid_str, &status);
		sprintf(buf, fmt_make_active, uuid_str, time_str);
		free (uuid_str);
		slog(LOG_INFO, buf);
		dbg_printf("%s: %s\n", XLV_DAEMON, buf);

		set_arg.uuid_p = uuid_p;
		set_arg.state = XLV_VE_STATE_ACTIVE;

		xlv_for_each_ve_in_plex (
			&oref, (XLV_OREF_PF)set_ve_state, (void *)&set_arg);
	}

	xlv_lab2_write_oref_component (
		&xlv_vh_list, &oref, XLV_OBJ_TYPE_SUBVOL,
		NULL, NULL, (int *)&status, XLV_LAB_WRITE_PARTIAL);

} /* end of xlv_labd_make_plex_active() */


/*
 * Create the label daemon fifo. Return the file descriptor.
 */
int
mk_labd_fifo(void)
{
	int	fd = -1;

	/*
	 * Don't bother stat'ing the fifo. Just delete the fifo and
	 * recreate it.
	 */
	(void)unlink(XLV_LABD_FIFO);

	if (0 > mknod(XLV_LABD_FIFO, S_IFIFO | 0666, 0)) {
		perror(msg_cant_create_fifo);
		slog(LOG_ERR, msg_cant_create_fifo);
		return(-1);
	}

	fd = open(XLV_LABD_FIFO, O_NDELAY|O_RDONLY);
	if (0 > fd) {
		perror(msg_cant_open_fifo);
		slog(LOG_ERR, msg_cant_open_fifo);
	} else {
		dbg_printf("%s\n", msg_created_fifo);
		slog(LOG_INFO, msg_created_fifo);
	}

	return(fd);

} /* end of mk_labd_fifo() */


/*
 * M A I N ()
 */
int
main (int argc, char **argv) 
{
	int			opt;
	struct sigvec		vec;
	struct sched_param      schedparam;
	cap_t			ocap;
	cap_value_t		cap_memory_mgt = CAP_MEMORY_MGT;
	cap_value_t		cap_sched_mgt = CAP_SCHED_MGT;

	extern int xlv_do_labsync;	/* libxlv flag for sync'ing vh */

	if (cap_envl(0, CAP_DEVICE_MGT, CAP_SCHED_MGT,
		     CAP_MEMORY_MGT, 0) == -1) {
		fprintf(stderr, "%s: must be started by super-user\n", argv[0]);
		exit(1);
	}

	/*
	 * Don't explicitly sync writes to the disk volume header.
	 * Writes to the volume header should already bypass the
	 * buffer cache.
	 *
	 * 6/13/96  This must be set to 0 to avoid a dead lock when
	 * the offline volume element is in the root volume.
	 */
	xlv_do_labsync = 0;

	do_debug = 0;
	logname = NULL;
	bzero(&last_cursor, sizeof(xlv_attr_cursor_t));

	opterr = 0;
	while ((opt = getopt(argc, argv, "dl:v")) != EOF)
		switch (opt) {
		case 'd':
			do_debug = 1;
			break;
		case 'v':
			do_verbose = 1;
			break;
		case 'l':
			logname = optarg;		/* log filename */
			break;
		default:
			usage();	/* and exit */
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	/* disable debugging if invoked from startup script */
	if (do_debug && !isatty(0))
		do_debug = 0;

	/*
	 * Look for other instances of the daemon.
	 */
	if (open(XLV_LABD_FIFO, O_NDELAY|O_WRONLY) >= 0) {
		/*
		 * STOP. There is a reader which means there already
		 * an existing xlv_labd. Don't want multiple instances.
		 */
		if (do_verbose) {
			fprintf(stderr, "%s already exists.\n", XLV_DAEMON);
		}
		exit(0);
	}

	if (do_debug == 0) {
		background();
	} else {
		setlinebuf(stdout);
	}

	/* set up signal handler to manage possible child completions 
         * from our slave or syslog() forking a process to write to the console
	 */
	vec.sv_handler = sighandle;
	vec.sv_mask = sigmask(SIGCHLD);
	vec.sv_flags = 0;
	sigvec(SIGCHLD, &vec, NULL);

	sigignore(SIGHUP);
	sigignore(SIGINT);
	sigignore(SIGQUIT);
	sigignore(SIGTERM);

	/* Open the syslog for logging. */
	xlv_openlog(XLV_DAEMON, LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);

	(void)mk_labd_fifo();		/* Okay, this is the label daemon. */

	/*
	 * Read the disk volume headers to get the
	 * current xlv configuration.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &last_cursor)) {
		slog(LOG_ERR, msg_no_cursor);
		perror(msg_no_cursor);
		(void)unlink(XLV_LABD_FIFO);
		exit(1);

	} else if (xlv_labd_init() != 0) {
		slog(LOG_ERR, msg_cant_read_vh);
		tab_vol = NULL;
		tab_sv = NULL;
	}

	/*
	 * Look for the root volume. Only need to do this here
	 * cuz root volume is determined at startup time.
	 */
	if (tab_sv && XLV_SUBVOL_EXISTS(&tab_sv->subvolume[0])) {
		/* root volume exists */
		bcopy(&tab_sv->subvolume[0].vol_p->uuid,
		      &root_vol_uuid, sizeof(root_vol_uuid));
	} else {
		bzero(&root_vol_uuid, sizeof(root_vol_uuid));
	}

	/*
	 * In tight memory situation where xlv_labd is swapped out,
	 * volumes waiting for label updates can hang the system.
	 * Set nondegrading scheduling priority and lock into memory
	 * so we don't get caught trying to swap ourselves in.
	 */
	schedparam.sched_priority = 176; /* above xfsd & grio */
	ocap = cap_acquire(1, &cap_sched_mgt);
	if (sched_setscheduler(0, SCHED_RR, &schedparam) == -1) {
		slog(LOG_ERR, "Unable to set scheduler priority");
	}
	cap_surrender(ocap);
	ocap = cap_acquire(1, &cap_memory_mgt);
	if (plock(PROCLOCK) == -1) {
		slog(LOG_ERR, "Unable to lock xlv_labd in memory");
	}
	cap_surrender(ocap);

	/* set up the slave process to do the rest of the work */
	if ((slave_pid = sproc(xlv_labd_slave, PR_SADDR|PR_SFDS)) < 0) {
		perror(XLV_DAEMON);
		exit(-1);
	}

	/* stick around until something happens */
	for(;;) {
		sigpause(0);
	}
	/*NOTREACHED*/
	(void)unlink(XLV_LABD_FIFO);
	xlv_closelog();			/* close the syslog */
} /* end of main() */


/*ARGSUSED*/
void
xlv_labd_slave(void *arg)
{
	time_t			t;
	char			*time_str;
	char			buf[1024];
	uint_t			status;
	xlv_attr_cursor_t       cursor;

	/*
	 * Main line function ...
	 */
	/*CONSTCOND*/
	for (;;) {
		/* don't get the last request if we died while processing
		 * a request.  Don't try and retry the request if we keep
		 * dieing in the middle of processing it.
		 */
		if (inprogress && inprogress_cnt <= 5) {
			inprogress_cnt++;
		} else {
			if (cap_ptr_syssgi(SGI_XLV_NEXT_RQST, &request)) {
				if (errno == EPIPE) {
				       /*
				        * There are no xlv volumes just exit.
				  	*/
					slog(LOG_ERR, msg_no_rqst);
					perror(msg_no_rqst);
					exit(EPIPE);
				}
				/* if we came back due to a signal, continue
				 * to loop since we can't just go away 
				 */
				continue;
			}
			/* got a sucessful request */
			inprogress_cnt = 0;
			inprogress = B_TRUE;
		}

		/*
		 * Find out if the configuration has changed.
		 * If so, may want to read in a new vh_list.
		 */
		if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
			slog(LOG_ERR, msg_no_cursor);
			perror(msg_no_cursor);
			exit(1);
		}

		if ((xlv_vh_list == NULL) || skipped_reread ||
		    bcmp(&cursor, &last_cursor, sizeof(xlv_attr_cursor_t))) {
			/*
			 * Need to (re)read xlv disk labels.
			 */
			if ((XLV_LABD_RQST_OFFLINE == request.operation)
			    && uuid_equal(&root_vol_uuid,
					  &request.plex_uuid, /* vol uuid */
					  &status)) {
				/*
				 * 6/17/96  jleong
				 *
				 * An existing root volume ve is offline.
				 * Don't do read cuz that causes a deadlock
				 * Not doing the vh reread should be okay
				 * cuz it is an existing ve (which xlv_labd
				 * should know) which is going offline.
				 *
				 * Note there is still a problem when
				 * the root volume has an offline entry
				 * in the kernel labd queue.
				 */
				skipped_reread++;	/* do it next time */
				slog(LOG_INFO, msg_skip_vhread);
			}
			else {
				/*
				 * Get the current volume configuration.
				 */
				if (xlv_labd_init() < 0) {
					slog(LOG_ERR, msg_cant_read_vh);
				} else {
					last_cursor = cursor;
					skipped_reread = 0;
				}
			}
		} /* (re)read of disk vh */

		/*
		 * At this point, notification came from the kernel driver.
		 */
		t = time(NULL);
		time_str = asctime(localtime(&t));

		switch(request.operation) {
		case XLV_LABD_RQST_OFFLINE:
			xlv_labd_make_offline (
				(xlv_config_rqst_t *)&request, time_str);
			break;

		case XLV_LABD_RQST_VES_ACTIVE:
			xlv_labd_make_plex_active (&request, time_str);
			break;

		case XLV_LABD_RQST_CONFIG_CHANGE:
			/*
			 * No explicit action is required since the
			 * re-reading of disk labels should have been
			 * triggered up above in the cursor comparsion.
			 */
			break;

		default:
			sprintf(buf, fmt_unknown_op,
				request.operation, time_str);
			slog(LOG_INFO, buf);
			dbg_printf("%s: %s\n", XLV_DAEMON, buf);
			break;
		} /* switch on request */

		/* 
		 * made it thru a request without a core dump.  clear
		 * the inprogress indicator
		 */
		inprogress = B_FALSE;
		inprogress_cnt = 0;

	} /* main loop */

	/*NOTREACHED*/
	exit(0);
} /* end of xlv_labd_slave */
