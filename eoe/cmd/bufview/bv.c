/*
 *  File system buffer cache display for Irix.
 *
 *  Converted from top(1) users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

/*
 *  This file contains "main" and other high-level routines.
 */

/*
 * The following preprocessor variables, when defined, are used to
 * distinguish between different Unix implementations:
 *
 *	SIGHOLD  - use SVR4 sighold function when defined
 *	SIGRELSE - use SVR4 sigrelse function when defined
 */

#include "os.h"
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/time.h>

/* includes specific to bufview */
#include "display.h"		/* interface to display package */
#include "screen.h"		/* interface to screen package */
#include "bv.h"
#include "boolean.h"
#include "machine.h"
#include "utils.h"

/* Size of the stdio buffer given to stdout */
#define Buffersize	2048

/* The buffer that stdio will use */
char stdoutbuf[Buffersize];

/* build Signal masks */
#define Smask(s)	(1 << ((s) - 1))

/* for system errors */
extern int errno;

/* for getopt: */
extern int  optind;
extern char *optarg;

/* imported from screen.c */
extern int overstrike;

/* signal handling routines */
sigret_t leave();
sigret_t onalrm();
sigret_t tstop();
#ifdef SIGWINCH
sigret_t mywinch();	/* Ariel: curses name space pollution: winch->mywinch */
#endif

/* internal routines */
void quit();
void show_help(void);

/* values which need to be accessed by signal handlers */
static int max_topn;		/* maximum displayable processes */

/* miscellaneous things */
char *myname = "bufview";
jmp_buf jmp_int;
struct system_info system_info;
struct buffer_select bst;

void
usage()
{
	fprintf(stderr,
		"Usage: %s [-SDb] [-d x] [-s x] [-o sort-specifier] [-n number] [-f buffer-flags]\n",
		myname);
	fprintf(stderr,
		"	-S -- don't display System buffers\n");
	fprintf(stderr,
		"	-D -- don't display Data buffers\n");
	fprintf(stderr,
		"	-b -- use batch (non-interactive) mode\n");
	fprintf(stderr,
		"	-s x -- update the display every x seconds\n");
	fprintf(stderr,
		"	-d x -- update the display only x times then exit\n");
	fprintf(stderr,
		"	-n x -- display only x buffers\n");
	fprintf(stderr,
		"	-o o -- use specified sort order to display buffers\n");
	fprintf(stderr,
		"		m -- display objects with most buffers\n");
	fprintf(stderr,
		"		l -- display objects with least buffers\n");
	fprintf(stderr,
		"		b -- display biggest buffers first\n");
	fprintf(stderr,
		"		s -- display smallest buffers first\n");
	fprintf(stderr,
		"		n -- display newest buffers first\n");
	fprintf(stderr,
		"		o -- display oldest buffers first\n");
	fprintf(stderr,
		"	-f flag -- display only buffers with specified flags\n");
	fprintf(stderr,
		"		dw -- delayed write buffer (B_DELWRI)\n");
	fprintf(stderr,
		"		bsy -- buffer in use (B_BUSY)\n");
	fprintf(stderr,
		"		swp -- buffer swapping user pages (B_SWAP)\n");
	fprintf(stderr,
		"		da -- backing store not allocated (B_DELALLOC)\n");
	fprintf(stderr,
		"		as -- asynchronous read/write (B_ASYNC)\n");
	fprintf(stderr,
		"		nc -- uncommitted NFS data (B_NFS_UNSTABLE)\n");
	fprintf(stderr,
		"		na -- asynchronous NFS read/write (B_NFS_ASYNC)\n");
	fprintf(stderr,
		"		inact -- buffer removed from buffer pools (B_INACTIVE)\n");
	fprintf(stderr,
		"		-- system buffer subtype flags\n");
	fprintf(stderr,
		"		ino -- inodes (B_FS_INO)\n");
	fprintf(stderr,
		"		inomap -- inode map (B_FS_INOMAP)\n");
	fprintf(stderr,
		"		dir_bt -- directory btree (B_FS_DIR_BTREE)\n");
	fprintf(stderr,
		"		map -- map (B_FS_MAP)\n");
	fprintf(stderr,
		"		attr_bt -- attribute btree (B_FS_ATTR_BTREE)\n");
	fprintf(stderr,
		"		agi -- agi (B_FS_AGI)\n");
	fprintf(stderr,
		"		agf -- agf (B_FS_AGF)\n");
	fprintf(stderr,
		"		agfl -- agfl (B_FS_AGFL)\n");
	fprintf(stderr,
		"		dquot -- quota buffer (B_FS_DQUOT)\n");
}

/*
 * routines that don't return int and are not declared in standard headers
 */

void get_buffer_info(struct system_info *);

/* display routines that need to be predeclared */
int i_sysbufs();
int u_sysbufs();
int i_databufs();
int u_databufs();
int i_emptybufs();
int u_emptybufs();
int i_getbufs();
int u_getbufs();
int i_order();
int u_order();
int i_message();
int u_message();
int i_header();
int u_header();
int i_buffers();
int u_buffers();

/* pointers to display routines */
int (*d_sysbufs)() = i_sysbufs;
int (*d_databufs)() = i_databufs;
int (*d_emptybufs)() = i_emptybufs;
int (*d_getbufs)() = i_getbufs;
int (*d_order)() = i_order;
int (*d_message)() = i_message;
int (*d_header)() = i_header;
int (*d_buffers)() = i_buffers;


main(int argc, char *argv[])
{
    int i;
    int active_users;
    int change;

    static char tempbuf[50];
    int old_sigmask;		/* only used for BSD-style signals */
    int topn = Default_TOPN;
    int delay = Default_DELAY;
    int displays = 0;		/* indicates unspecified */
    time_t curr_time;
    char *env_bufview;
    char **preset_argv;
    int  preset_argc = 0;
    char **av;
    int  ac;
    char interactive = Maybe;
    char warnings = 0;
#if Default_TOPN == Infinity
    char topn_specified = No;
#endif
    char ch;
    char *iptr;
    char no_command = 1;
    extern void display_message(void);

    static char command_chars[] = "\n\f qh?sdSDofFmM";
/* these defines enumerate the "strchr"s of the commands in command_chars */
#define CMD_noop	0
#define CMD_redraw	1
#define CMD_update	2
#define CMD_quit	3
#define CMD_help1	4
#define CMD_help2	5
#define CMD_OSLIMIT	5    /* terminals with OS can only handle commands */
#define CMD_delay	6
#define CMD_displays	7
#define CMD_systemtog	8
#define CMD_datatog	9
#define CMD_order	10
#define CMD_flag	11
#define CMD_dflag	12
#define CMD_device	13
#define CMD_alldevices	14

	/* set the buffer for stdout */
#ifdef DEBUG
	setbuffer(stdout, NULL, 0);
#else
	setbuffer(stdout, stdoutbuf, Buffersize);
#endif

	sortlist_init();
	devlist_init();

	/* get our name */
	if (argc > 0) {
		if ((myname = strrchr(argv[0], '/')) == 0) {
			myname = argv[0];
		} else {
			myname++;
		}
	}

	/* initialize some selection options */
	bst.system  = Yes;
	bst.fsdata  = Yes;

	/* get preset options from the environment */
	if ((env_bufview = getenv("BUFVIEW")) != NULL)
	{
		av = preset_argv = argparse(env_bufview, &preset_argc);
		ac = preset_argc;

		/*
		 * set the dummy argument to an explanatory message, in case
		 * getopt encounters a bad argument
		 */
		preset_argv[0] = "while processing environment";
	}

	/* process options */
	do {
		/*
		 * if we're done doing the presets,
		 * then process the real arguments
		 */
		if (preset_argc == 0)
		{
			ac = argc;
			av = argv;

			/* this should keep getopt happy... */
			optind = 1;
		}

		while ((i = getopt(ac, av, "SDbo:f:n:s:d:")) != EOF)
			{
			switch(i)
			{
			case 'S':
				/* do not show system buffers */
				bst.system = 0;
				break;

			case 'D':
				/* do not show data buffers */
				bst.fsdata = 0;
				break;

			case 'o':
				if (!sortlist_reorder(*optarg))
				{
					fprintf(stderr,
						"%s: warning: unknown display order specification %s -- ignored\n",
						myname, optarg);
					warnings++;
				}
				break;

			case 'f':
				if (!sortlist_bprune(optarg))
				{
					fprintf(stderr,
						"%s: warning: unknown buffer flag %s -- ignored\n",
						myname, optarg);
					warnings++;
				}
				break;

			case 'b':		/* interactive off */
				interactive = No;
				break;

			case 'd':		/* number of displays to show */
				if ((i = atoiwi(optarg)) == Invalid || i == 0)
				{
					fprintf(stderr,
						"%s: warning: display count should be positive -- option ignored\n",
						myname);
					warnings++;
				}
				else
				{
					displays = i;
				}
				break;

			case 's':
				if ((delay = atoi(optarg)) < 0)
				{
					fprintf(stderr,
						"%s: warning: seconds delay should be non-negative -- using default\n",
						myname);
					delay = Default_DELAY;
					warnings++;
				}
				break;

			case 'n':
				/* get count of top buffer holders to display (if any) */
				if ((topn = atoiwi(optarg)) == Invalid)
				{
					fprintf(stderr,
						"%s: warning: buffer use display count should be non-negative -- using default\n",
						myname);
					warnings++;
				}
#if Default_TOPN == Infinity
				else
				{
					topn_specified = Yes;
				}
#endif
				break;

			default:
				usage();
				exit(1);
			}
		}


		/* tricky:  remember old value of preset_argc & set preset_argc = 0 */
		i = preset_argc;
		preset_argc = 0;

	/* repeat only if we really did the preset arguments */
	} while (i != 0);

	/* initialize the kernel memory interface */
	if (machine_init() == -1)
	{
		exit(1);
	}

	/* initialize termcap */
	init_termcap(interactive);

	/* initialize display interface */
	if ((max_topn = display_init()) == -1)
	{
		fprintf(stderr, "%s: can't allocate sufficient memory\n",
			myname);
		exit(4);
	}
    
	/* print warning if user requested more processes than we can display */
	if (topn > max_topn)
	{
		fprintf(stderr,
			"%s: warning: this terminal can only display %d buffer users.\n",
			myname, max_topn);
		warnings++;
	}

	/* adjust for topn == Infinity */
	if (topn == Infinity)
	{
		/*
		 *  For smart terminals, infinity really means everything that
		 *  can be displayed, or Largest.  On dumb terminals, infinity
		 *  means every process in the system!  We only really want to
		 *  do that if it was explicitly specified.  This is always the
		 *  case when "Default_TOPN != Infinity".  But if topn wasn't
		 *  explicitly specified and we are on a dumb terminal and
		 *  the default is Infinity, then (and only then) we use
		 *  "Nominal_TOPN" instead.
		 */
#if Default_TOPN == Infinity
		topn = smart_terminal ? Largest :
			    (topn_specified ? Largest : Nominal_TOPN);
#else
		topn = Largest;
#endif
	}

	/* set header display accordingly */
	display_header(topn > 0);

	/* determine interactive state */
	if (interactive == Maybe)
	{
		interactive = smart_terminal;
	}

	/* if # of displays not specified, fill it in */
	if (displays == 0)
	{
		displays = smart_terminal ? Infinity : 1;
	}

	/*
	 * hold interrupt signals while setting up the screen and the handlers
	 */
#ifdef SIGHOLD
	sighold(SIGINT);
	sighold(SIGQUIT);
	sighold(SIGTSTP);
#else
	old_sigmask = sigblock(Smask(SIGINT) | Smask(SIGQUIT) | Smask(SIGTSTP));
#endif
	init_screen();
	(void) signal(SIGINT, leave);
	(void) signal(SIGQUIT, leave);
	(void) signal(SIGTSTP, tstop);
#ifdef SIGWINCH
	(void) signal(SIGWINCH, mywinch);
#endif
#ifdef SIGRELSE
	sigrelse(SIGINT);
	sigrelse(SIGQUIT);
	sigrelse(SIGTSTP);
#else
	(void) sigsetmask(old_sigmask);
#endif
	if (warnings)
	{
		fputs("....", stderr);
		fflush(stderr);			/* why must I do this? */
		sleep((unsigned)(3 * warnings));
		fputc('\n', stderr);
	}

	/* setup the jump buffer for stops */
	if (setjmp(jmp_int) != 0)
	{
		/* control ends up here after an interrupt */
		reset_display();
	}

    /*
     *  main loop -- repeat while display count is positive or while it
     *		indicates infinity.
     */

    while ((displays == Infinity) || (displays-- > 0))
    {
	/* get the current set of processes */
	get_buffer_info(&system_info);

	/* get the current stats */
	get_system_info(&system_info);

	/* display system buffers */
	(*d_sysbufs)();

	/* display the current time */
	time(&curr_time);
	i_timeofday(&curr_time);

	/* display data buffers */
	(*d_databufs)();

	/* display empty buffers */
	(*d_emptybufs)();

	/* display get buffers */
	(*d_getbufs)();

	/* display read/write stats */
	(*d_order)();

	/* handle message area */
	(*d_message)();

	/* update the header area */
	(*d_header)();
    
	if (topn > 0)
	{
		/* determine number of processes to actually display
		 * this number will be the smallest of:  active processes,
		 * number user requested, number current screen accomodates
		 */
		active_users = system_info.si_busers;
		if (active_users > topn)
		{
			active_users = topn;
		}
		if (active_users > max_topn)
		{
			active_users = max_topn;
		}

		/* now show the top "n" buffer users. */
		display_buffer_users(active_users, d_buffers);

		i = active_users;	/* for endscreen */
	}
	else
	{
		i = 0;
	}

	/* do end-screen processing */
	u_endscreen(i);

	/* now, flush the output buffer */
	fflush(stdout);

	/* only do the rest if we have more displays to show */
	if (displays)
	{
	    /* switch out for new display on smart terminals */
	    if (smart_terminal)
	    {
		if (overstrike)
		{
		    reset_display();
		}
		else
		{
		    d_sysbufs = u_sysbufs;
		    d_databufs = u_databufs;
		    d_emptybufs = u_emptybufs;
		    d_getbufs = u_getbufs;
		    d_order = u_order;
		    d_message = u_message;
		    d_header = u_header;
		    d_buffers = u_buffers;
		}
	    }
    
	    no_command = Yes;
	    if (!interactive)
	    {
		/* set up alarm */
		(void) signal(SIGALRM, onalrm);
		(void) alarm((unsigned)delay);
    
		/* wait for the rest of it .... */
		pause();
	    }
	    else while (no_command)
	    {
	        fd_set readfds;
		struct timeval timeout;

		/* assume valid command unless told otherwise */
		no_command = No;

		/* set up arguments for select with timeout */
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);		/* for standard input */
		FD_SET(1, &readfds);		/* for standard input */
		timeout.tv_sec  = delay;
		timeout.tv_usec = 0;

		/* wait for either input or the end of the delay period */
		if (select(32, &readfds, (fd_set *)NULL, (fd_set *)NULL,
			   &timeout) > 0)
		{
		    int newval;
    
		    /* something to read -- clear the message area first */
		    clear_message();

		    /* now read it and convert to command strchr */
		    /* (use "change" as a temporary to hold strchr) */
		    (void) read(0, &ch, 1);
		    if ((iptr = strchr(command_chars, ch)) == NULL)
		    {
			/* illegal command */
			new_message(MT_standout, " Command not understood");
			putchar('\r');
			no_command = Yes;
		    }
		    else
		    {
			change = iptr - command_chars;
			if (overstrike && change > CMD_OSLIMIT)
			{
			    /* error */
			    new_message(MT_standout,
			    " Command cannot be handled by this terminal");
			    putchar('\r');
			    no_command = Yes;
			}
			else switch(change)
			{
			    case CMD_redraw:	/* redraw screen */
				reset_display();
				break;
    
			    case CMD_update:	/* merely update display */
			    case CMD_noop:
				fflush(stdout);
				break;
	    
			    case CMD_quit:	/* quit */
				quit(0);
				/*NOTREACHED*/
				break;
	    
			    case CMD_help1:	/* help */
			    case CMD_help2:
				reset_display();
				clear();
				show_help();
				standout("Hit any key to continue: ");
				fflush(stdout);
				reset_display();
				(void) read(0, &ch, 1);
				break;
	
			    case CMD_delay:	/* new seconds delay */
				new_message(MT_standout, "Seconds to delay: ");
				if ((i = readline(tempbuf, 8, Yes)) > -1)
				{
				    delay = i;
				}
				clear_message();
				break;
	
			    case CMD_displays:	/* change display count */
				new_message(MT_standout,
					"Displays to show (currently %s): ",
					displays == Infinity ? "infinite" :
							 itoa(displays));

				if ((i = readline(tempbuf, 10, Yes)) > 0)
				{
				    displays = i;
				}
				else if (i == 0)
				{
				    quit(0);
				}
				clear_message();
				break;
    
			    case CMD_systemtog:
				bst.system = !bst.system;
				display_message();
				break;
    
			    case CMD_datatog:
				bst.fsdata = !bst.fsdata;
				display_message();
				putchar('\r');
				break;

			    case CMD_order:
				new_message(MT_standout,
					"Display order specifier: ");
				if ((i = readline(tempbuf, 10, No)) > 0)
				{
					char *msg =
						sortlist_reorder(tempbuf[0]);
					clear_message();
					new_message(MT_standout | MT_delayed,
						"First order: %s",
						msg ? msg :
						"unknown order specifier");
					reset_display();
				}
				break;

			    case CMD_flag:
				new_message(MT_standout,
					"Buffer flags to display: ");
				if ((i = readline(tempbuf, 12, No)) > 0)
				{
					char *msg =
						sortlist_bprune(tempbuf);
					clear_message();
					if (msg)
					new_message(MT_standout | MT_delayed,
				"Will show only buffers with flag: %s", msg);
					else
					new_message(MT_standout | MT_delayed,
						"unknown flag specifier");
					reset_display();
				}
				break;
    
			    case CMD_dflag:
				bst.bflags = 0;
				bst.bvtype = 0;
				reset_display();
				new_message(MT_standout | MT_delayed,
				    "Ignoring buffer flags in sort pruning");
				putchar('\r');
				break;

			    case CMD_device:
				new_message(MT_standout,
					"Display only mounted device: ");
				if ((i = readline(tempbuf, 12, No)) > 0)
				{
					display_only_dev(tempbuf);
					reset_display();
				}
				break;

			    case CMD_alldevices:
				display_all_devs();
				reset_display();
				new_message(MT_standout | MT_delayed,
				    "Displaying all mounted devices");
				putchar('\r');
				break;
	    
			    default:
				new_message(MT_standout, " BAD CASE IN SWITCH!");
				putchar('\r');
			}
		    }

		    /* flush out stuff that may have been written */
		    fflush(stdout);
		}
	    }
	}
    }

    quit(0);
    /*NOTREACHED*/
}

/*
 *  reset_display() - reset all the display routine pointers so that entire
 *	screen will get redrawn.
 */
void
reset_display(void)
{
	d_sysbufs	= i_sysbufs;
	d_databufs	= i_databufs;
	d_emptybufs	= i_emptybufs;
	d_getbufs	= i_getbufs;
	d_order		= i_order;
	d_message	= i_message;
	d_header	= i_header;
	d_buffers	= i_buffers;
}

/*
 *  signal handlers
 */

sigret_t
leave(void)	/* exit under normal conditions -- INT handler */
{
	end_screen();
	exit(0);
}

sigret_t
tstop(void)	/* SIGTSTP handler */
{
	/* move to the lower left */
	end_screen();
	fflush(stdout);

	/* default the signal handler action */
	(void) signal(SIGTSTP, SIG_DFL);

	/* unblock the signal and send ourselves one */
#ifdef SIGRELSE
	sigrelse(SIGTSTP);
#else
	(void) sigsetmask(sigblock(0) & ~(1 << (SIGTSTP - 1)));
#endif
	(void) kill(0, SIGTSTP);

	/* reset the signal handler */
	(void) signal(SIGTSTP, tstop);

	/* reinit screen */
	reinit_screen();

	/* jump to appropriate place */
	longjmp(jmp_int, 1);

	/*NOTREACHED*/
}

#ifdef SIGWINCH
sigret_t
mywinch(void)		/* SIGWINCH handler */
{
	/* reascertain the screen dimensions */
	get_screensize();

	/* tell display to resize */
	max_topn = display_resize();

	/* reset the signal handler */
	(void) signal(SIGWINCH, mywinch);

	/* jump to appropriate place */
	longjmp(jmp_int, 1);
}
#endif

void
quit(int status)		/* exit under duress */
{
	end_screen();
	exit(status);
	/*NOTREACHED*/
}

sigret_t
onalrm(void)	/* SIGALRM handler */
{
	/* this is only used in batch mode to break out of the pause() */
	/* return; */
}

void
display_message()
{
	char msg_buffer[64];

	if (bst.system && bst.fsdata)
	{
		sprintf(msg_buffer,
			"%s", "Displaying system and fsdata buffers");
	}
	else if (!bst.system && !bst.fsdata)
	{
		sprintf(msg_buffer,
			"%s", "Not displaying system or fsdata buffers");
	}
	else if (bst.system)
	{
		sprintf(msg_buffer,
			"%s", "Displaying only system buffers");
	}
	else
	{
		sprintf(msg_buffer,
			"%s", "Displaying only fsdata buffers");
	}

	new_message(MT_standout | MT_delayed, " %s", msg_buffer);
	putchar('\r');
}

/*
 *  show_help() - display the help screen; invoked in response to
 *		either 'h' or '?'.
 */
void
show_help(void)
{
	printf("bufview -- A file system buffer cache display for the Irix Operating System\n\
\n\
These single-character commands are available:\n\
\n\
^L - redraw screen\n\
q  - quit\n\
h or ? - help; show this text\n");

	/*
	 * not all commands are available with overstrike terminals
	 */
	if (overstrike)
	{
		printf("\n\
Other commands are also available, but this terminal is not\n\
sophisticated enough to handle those commands gracefully.\n\n");
	}
	else
	{
		printf("\
\n\
These commands require arguments:\n\
\n\
d  - change number of displays to show\n\
s  - change time interval (in seconds) between updates\n\
S  - toggle displaying system control buffers\n\
D  - toggle displaying data buffers\n\
o  - push sort specifier to top of sort list\n\
	[m]ost     - display files/devices with most buffers mapped\n\
	[l]east    - display files/devices with least buffers mapped\n\
	[b]iggest  - display biggest buffers first\n\
	[s]mallest - display smallest buffers first\n\
	[n]ewest   - display newest buffers first\n\
	[o]ldest   - display oldest buffers first\n\
f  - display only those buffers with specified flags\n\
	dw   (B_DELWRI) -- delayed write buffer\n\
	bsy  (B_BUSY) -- buffer in use\n\
	swp  (B_SWAP) -- buffer being used to swap user pages\n\
	da   (B_DELALLOC) -- backing store not yet allocated [XFS]\n\
	as   (B_ASYNC) -- asynchronous read/write\n\
	nc   (B_NFS_UNSTABLE) -- dirty NFS data not yet committed to backing store\n\
	na      (B_NFS_ASYNC) -- asynchronous NFS read/write\n\
	inact   (B_INACTIVE) -- temporarily removed from buffer pool\n\
	ino     (B_FS_INO)   -- inodes\n\
	inomap  (B_FS_INOMAP) -- inode map\n\
	dir_bt  (B_FS_DIR_BTREE) -- directory btree\n\
	map     (B_FS_MAP) -- map\n\
	attr_bt (B_FS_ATTR_BTREE) -- attribute btree\n\
	agi     (B_FS_AGI) -- agi\n\
	agf     (B_FS_AGF) -- agf\n\
	agfl    (B_FS_AGFL) -- agfl\n\
	dquot   (B_FS_DQUOT) -- quota buffer\n\
F  - disregard flags when selecting buffers to display\n\
m  - only show buffers mapping specified mounted file system\n\
M  - show buffers mapping all mounted file systems\n\
\n\n");
	}
}

