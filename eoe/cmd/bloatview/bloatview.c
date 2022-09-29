/*
 * bloatview.c
 *
 * Reveal the memory usage of bloated (and non-bloated) software
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysmp.h>
#include <sys/wait.h>
#include <sys/capability.h>

#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <bstring.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "process.h"
#include "draw.h"
#include "inode.h"
#include "print.h"

#define DEF_THRESH 500
#define DEF_DELTA 50

/*
 * Set to 1 once the inode table has been initialized
 */
static int inodesInitted = 0;

#ifdef CAP_DEBUG
/*
 *  void
 *  PrintCapabilities(const char* prefix)
 *
 *  Description:
 *      Function for debugging capabilities.
 */
void
PrintCapabilities(const char* prefix)
{
    char *text;
    cap_t cap = cap_get_proc();
    text = cap_to_text(cap, NULL);
    fprintf(stderr, "%s: %s\n", prefix, text);
    cap_free(text);
}
#endif

/*
 *  static void
 *  InitCapabilities(void)
 *
 *  Description:
 *      Set up our set of permitted capabilities, and then surrender
 *      our setuid root status.  The capabilities we set in our
 *      permitted set correspond to those used by the procfs code.
 *      They do not get moved into our effective set until we need
 *      them (see cap_open in process.c).
 *      
 *      Setting CAP_FLAG_PURE_RECALC in our inheritable set causes
 *      programs that we exec to inherit no capabilities from us.
 */
static void
InitCapabilities(void)
{
    cap_t cap = cap_from_text("all= "
		      "CAP_DAC_WRITE,CAP_DAC_READ_SEARCH,CAP_FOWNER+p");
    cap_set_proc(cap);
    cap_free(cap);
    cap_set_proc_flags(CAP_FLAG_PURE_RECALC);
    setuid(getuid());
#ifdef CAP_DEBUG
    PrintCapabilities("gmemusage");
#endif
}

/*
 *  static void
 *  PlaySound(char *sound)
 *
 *  Description:
 *      run playaiff asynchronously to play a sound
 *
 *  Parameters:
 *      sound
 */

static void
PlaySound(char *sound)
{
    static int firstTime = 1;
    static int ok = 0;
    static pid_t pid;
    int fd, stat;

    if (firstTime) {
	firstTime = 0;
	if (sound) {
	    if (access("/usr/sbin/playaiff", F_OK | X_OK) == -1) {
		perror("/usr/sbin/playiff");
		return;
	    }

	    if (access(sound, F_OK) == -1) {
		perror(sound);
		return;
	    }
	    ok = 1;
	}
    }

    if (!ok) {
	return;
    }

    /*
     * Don't interrupt it if the last one is still playing
     */
    if (waitpid(pid, &stat, WNOHANG) == 0) {
	return;
    }

    if (pid = fork()) {
	return;
    }

    for (fd = getdtablesize(); fd > 2; fd--) {
	close(fd);
    }

    setuid(getuid());		/* avoid security problems */
    execl("/usr/sbin/playaiff", "playaiff", sound, NULL);
    exit(-1);
}

/*
 *  static void
 *  InitInodes(void)
 *
 *  Description:
 *      Make sure inodes are initted.  This gets called from two
 *      different places, and the flag inodesInitted is global.
 */

static void
InitInodes(void)
{
    if (!inodesInitted) {
	WaitMessage("Initializing inode lookup table...", NULL);
	InodeInit();
	inodesInitted = 1;
    }	
}

/*
 *  static void
 *  RegionView(pid_t pid, void *vaddr)
 *
 *  Description:
 *      If region viewer is not running, start it.
 *      
 *      Write on the pipe which is region viewer's input the process
 *      id and virtual address of the region to monitor.
 *
 *      If we just started region viewer, wait for its window to come
 *      up.
 *      
 *  Parameters:
 *      pid    process id to monitor
 *      vaddr  virtual address of the region in pid to monitor
 */

static void
RegionView(pid_t pid, void *vaddr)
{
    char buf[20];
    int pipes[2], errorPipe[2], fd, err, n, conn, seenIt;
    static int wpipe = -1;
    pid_t p;
    Display *dpy;
    XEvent event;
    fd_set readfds;
    struct timeval tv;

    (void)snprintf(buf, sizeof buf, "%d %x\n", pid, vaddr);

    if (wpipe == -1
	|| write(wpipe, buf, strlen(buf)) == -1 && oserror() == EPIPE) {
	WaitMessage("Launching Region Viewer...", NULL);

	if (wpipe != -1) {
	    (void)close(wpipe);
	    wpipe = -1;
	}

	/*
	 * Create pipe for sending commands to regview
	 */
	if (pipe(pipes) == -1) {
	    perror("pipe");
	    return;
	}
	
	/*
	 * Create pipe that we use to tell if regview even got off the
	 * ground, using fcntl FD_CLOEXEC.

	 */
	if (pipe(errorPipe) == -1) {
	    perror("pipe");
	    return;
	}

	/*
	 * Get an X connection before forking to avoid race conditions
	 */
	dpy = XOpenDisplay(NULL);
	if (dpy != NULL) {
	    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureNotifyMask);
	    XFlush(dpy);
	}
	    
	p = fork();
	if (p == -1) {
	    perror("fork");
	    (void)close(pipes[0]);
	    (void)close(pipes[1]);
	    (void)close(errorPipe[0]);
	    (void)close(errorPipe[1]);
	    XCloseDisplay(dpy);
	    return;
	}

	if (p == 0) {
	    /*
	     * Child execs to become regview.  stdin of regview is the
	     * read end of the pipe we created for sending commands.
	     */
	    dup2(pipes[0], 0);
	    (void)close(errorPipe[0]);
	    if (errorPipe[1] != 3) {
		dup2(errorPipe[1], 3);
	    }

	    /*
	     * Setting FD_CLOEXEC will cause the read in the parent
	     * below to return 0 if the exec goes OK.  Otherwise,
	     * we'll write something on the pipe and our parent will
	     * know something went wrong.
	     */
	    (void)fcntl(3, F_SETFD, FD_CLOEXEC);
	    for (fd = getdtablehi(); fd > 3; fd--) {
		(void)close(fd);
	    }

	    execl("/usr/lib/regview", "regview", NULL);
	    perror("/usr/lib/regview");

	    /*
	     * Notify parent process that something has gone afoul, so
	     * that it doesn't wait for our window to be mapped.
	     */
	    err = 1;
	    (void)write(3, &err, sizeof err);
	}

	(void)close(pipes[0]);
	(void)close(errorPipe[1]);

	/*
	 * Tell new regview pid and vaddr to monitor
	 */
	wpipe = pipes[1];
	(void)write(wpipe, buf, strlen(buf));

	/*
	 * If things are cool, read should return 0 because the pipe
	 * was closed during exec.
	 */
	n = read(errorPipe[0], &err, sizeof err);
	(void)close(errorPipe[0]);

	if (dpy == NULL) {
	    return;
	}

	/*
	 * Wait for a map event on the X root window
	 */
	if (n == 0) {
	    conn = ConnectionNumber(dpy);
	    seenIt = 0;
	    while (1) {
		while (XPending(dpy)) {
		    XNextEvent(dpy, &event);
		    if (event.type == MapNotify) {
			seenIt = 1;
			break;
		    }
		}

		if (seenIt) {
		    break;
		}

		tv.tv_sec = 5;
		tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(conn, &readfds);
		if (select(conn + 1, &readfds, NULL, NULL, &tv) != 1) {
		    break;
		}
	    }
	}
	XCloseDisplay(dpy);
    }
}     

/*
 *  static char *
 *  HandleMouse(PROGRAM *bloat, long mousex, long mousey, int *procMode,
 *  	    int dragging)
 *
 *  Description:
 *      Handle mouse events.  We use the mouse to select bars to
 *      display detailed breakdowns, and to leave detailed mode.  The
 *      passed in procMode gets cleared if we should leave detailed
 *      mode.
 *
 *  Parameters:
 *      bloat      current all programs bloat being displayed
 *      mousex     mouse horiz position
 *      mousey     mouse vert position
 *      procMode   pointer to variable that says whether we're already
 *                 in procMode
 *      dragging   1 if we're dragging
 *
 *  Returns:
 *      pointer to name of program selected
 */

static char *
HandleMouse(PROGRAM *bloat, long mousex, long mousey, int *procMode,
	    int dragging)
{
    static char *oldProcName;
    char *procName;
    
    procName = Select(bloat, mousex, mousey, procMode, dragging);

    if (!procName && *procMode) {
	return oldProcName;
    }

    if (procName) {
	if (!inodesInitted && strcmp(procName, IRIX) != 0) {
	    InitInodes();
	}
	*procMode = 1;
	if (oldProcName) {
	    free(oldProcName);
	}
	oldProcName = procName;
    }

    return procName;
}

/*
 *  static int XIOError(Display *dpy)
 *
 *  Description:
 *      Error handler that gets called when an I/O error occurs.  The
 *      purpose of this function is to avoid printing:
 *      
 *  XIO:  fatal IO error 131 (Connection reset by peer) on X server ":0.0"
 *        after 7919 requests (6854 known processed) with 0 events remaining.
 *      
 *      When the user closes the window.
 */
/*ARGSUSED*/
static int XIOError(Display *dpy)
{
    exit(0);
    return 0;
}

/*
 * Show how bloated IRIX has become
 */
void
main(int argc, char *argv[])
{
    int xfd, opt, threshDelta = DEF_DELTA, doHelp = 0, doFreeze = 0;
    int gotInput, doProcMode = 0, needNewColors = 0;
    int barTotal, progBarTotal, numBars, numProgBars;
    int oldProgBarTotal, soundThresh = 12, pgsize;
    int printModeOn = 0;
    fd_set fds, tfds;
    PROGRAM *bloat = NULL, *lastBloat = NULL, *b;
    PROGRAM *progBloat = NULL, *lastProgBloat = NULL;
    long physMem, freeMem, threshHold = DEF_THRESH;
    struct timeval tv;
    char *procName = NULL, *pn;
    pid_t procPid = -1, pid;
    PROGNAME *progNames = NULL, *name, *prev = NULL;
    BloatType type, newType;
    SecondType stype, newStype;
    long usecTimeOut = 0, secTimeOut = 1, msecTimeOut;
    struct rminfo rminfo;
    char *bloatSound = getenv("GMEMUSAGESOUND");
    PIDLIST *pids;
    sigaction_t act;
    Display *dpy;
    XEvent xevent;
    char keyBuf[10];
    KeySym keySym;

    if (geteuid() != 0) {
	fprintf(stderr, "%s must be run as root or it must be setuid root.\n",
		argv[0]);
	exit(1);
    }

    InitCapabilities();

    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    (void)sigemptyset(&act.sa_mask);
    (void)sigaction(SIGPIPE, &act, NULL);

    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_NOCLDWAIT;
    (void)sigemptyset(&act.sa_mask);
    (void)sigaction(SIGCHLD, &act, NULL);
#if 0
    mallopt(M_FREEHD, 1);
#endif
    type = newType = Physical;
    stype = newStype = Nostype;
    /*
     * Process command line arguments
     */
    opterr = 0;
    while ((opt = getopt(argc, argv, "a:d:f:g:i:mnprst:uyv")) != EOF) {
	switch (opt) {
	case 'a':
	    bloatSound = optarg;
	    break;
	case 'f':
	    SetFontName(optarg);
	    break;
	case 'g':
	    soundThresh = atoi(optarg);
	    break;
	case 'i':
	    msecTimeOut = atoi(optarg);
	    secTimeOut = msecTimeOut / 1000;
	    usecTimeOut = 1000 * (msecTimeOut % 1000);
	    break;
	case 'd':
	    threshDelta = atoi(optarg);
	    if (threshDelta <= 0) {
		fprintf(stderr, "Resetting delta from %d\n", threshDelta);
		threshDelta = DEF_DELTA;
	    }
	    break;
	case 'm':
	    type = newType = MappedObjects;
	    break;
	case 'n':
	    SetNoDoubleBuffer();
	    break;
	case 'p':
	    type = newType = Physical;
	    break;
	case 'r':
	    type = newType = Resident;
	    break;
	case 's':
	    type = newType = Size;
	    break;
	case 't':
	    threshHold = atoi(optarg);
	    break;
	case 'u':
	    InvalidateInodeTable();
	    break;
	case 'v':
	    UseDefaultVisual();
	    break;
	case 'y':
	    printModeOn = 1;
	    break;
	default:
	    doHelp = 1;
	}
    }

    while (optind < argc) {
	name = malloc(sizeof *name);
	name->name = argv[optind];
	name->next = NULL;

	if (prev) {
	    prev->next = name;
	} else {
	    progNames = name;
	}
	prev = name;
	optind++;
    }

    if (sysmp(MP_SAGET, MPSA_RMINFO, &rminfo, sizeof rminfo) == -1) {
	perror("sysmp");
	exit(1);
    }

    pgsize = getpagesize()/1024;

    physMem = rminfo.physmem * pgsize;

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
	fprintf(stderr, "Unable to open X display\n");
	exit(1);
    }
    XSetIOErrorHandler(XIOError);
    ShowPrintMode(printModeOn);
    Init(argc, argv, dpy,
	 ExposureMask | KeyReleaseMask | ButtonPressMask
	 | StructureNotifyMask | Button1MotionMask
	 | VisibilityChangeMask, progNames, threshHold);

    /*
     * Wait for first expose event so we're not spending a lot of time
     * in process.c while our window is blank.
     */
    while (1) {
	XNextEvent(dpy, &xevent);
	if (xevent.type == Expose) {
	    break;
	}
    }

    xfd = ConnectionNumber(dpy);

    FD_ZERO(&fds);
    FD_SET(xfd, &fds);

    tv.tv_sec = secTimeOut;
    tv.tv_usec = usecTimeOut;

    gotInput = 0;

    if (type == MappedObjects) {
	InitInodes();
    }

    while (1) {
	/*
	 * Draw the window, either help or the bloat chart
	 */
	if (!doFreeze) {
	    if (doHelp) {
		Help();
	    } else {
		/*
		 * If the user pressed a key that we can deal with without
		 * getting all our data again, just redraw.
		 */
		if (!gotInput || type != newType || stype != newStype) {
		    stype = newStype;
		    type = newType;
		    if (lastBloat) {
			FreeBloat(lastBloat);
		    }
		    lastBloat = bloat;

		    if (lastProgBloat) {
			FreeBloat(lastProgBloat);
		    }
		    lastProgBloat = progBloat;

		    if (type == MappedObjects) {
			GetObjInfo(doProcMode ? procName : NULL,
				   &bloat, &progBloat);
		    } else {
			pn = doProcMode || procPid == -1 ? procName : NULL;
			pid = doProcMode ? procPid : -1;
			GetProcInfo(pn, pid, &bloat, &progBloat);
		    }
			
		    if (sysmp(MP_SAGET, MPSA_RMINFO, &rminfo,
			      sizeof rminfo) == -1) {
			perror("sysmp");
			exit(1);
		    }
		    freeMem = rminfo.freemem * pgsize;

		    if (bloat) {
			bloat = DrawSetup(bloat,
					  needNewColors ? NULL : lastBloat,
					  physMem, freeMem, type, stype, 1,
					  &barTotal, &numBars);
		    }
		    if (progBloat) {
			progBloat = DrawSetup(progBloat,
					      needNewColors ?
					      NULL : lastProgBloat,
					      physMem, freeMem,
					      type, stype, 0, &progBarTotal,
					      &numProgBars);
			if (bloatSound) {
			    if (oldProgBarTotal == 0 ||
				oldProgBarTotal > progBarTotal) {
				oldProgBarTotal = progBarTotal;
			    } else if (progBarTotal >
				       oldProgBarTotal + soundThresh) {
				PlaySound(bloatSound);
				oldProgBarTotal = progBarTotal;
			    }
			}
		    }
		    /*
		     * If the process we were looking at goes away,
		     * get out of process mode.
		     */
		    if (doProcMode && !progBloat) {
			doProcMode = 0;
			procName = NULL;
			procPid = -1;
			progBarTotal = 0;
		    }
		    needNewColors = 0;
		}
		
		if (doProcMode && progBloat) {
		    DrawShadow(bloat, barTotal, procName);
		    Draw(progBloat, progBarTotal, numProgBars,
			 procName, procPid, type, stype);
		} else if (bloat) {
		    Draw(bloat, barTotal, numBars, NULL, -1, type, stype);
		}
		gotInput = 0;
	    }
	    if (printModeOn) {
		if (doProcMode && progBloat) {
		    PrintProcBloat(stdout, progBloat,
				   procName, procPid);
		} else if (bloat) {
		    if (type == MappedObjects) {
			PrintMapBloat(stdout, bloat);
		    } else {
			PrintAllBloat(stdout, bloat);
		    }
		}
	    }	    
	}

	XFlush(dpy);
	/*
	 * Wait for X events, timing out after half a second (or user
	 * specified timeout value).
	 */
	tfds = fds;
	if (select(xfd + 1, &tfds, NULL, NULL,
		   (doHelp || doFreeze) ? NULL : &tv) < 0) {
	    perror("select");
	    exit(1);
	}

	/*
	 * Process X events
	 */
	if (FD_ISSET(xfd, &tfds)) {
	    while (XPending(dpy)) {
		XNextEvent(dpy, &xevent);
		switch (xevent.type) {
		case ConfigureNotify:
		    Resize(xevent.xconfigure.width,
			   xevent.xconfigure.height);
		    gotInput = 1;
		    break;
		case Expose:
		    gotInput = 1;
		    break;
		case MotionNotify:
		    needNewColors = 1;
		    procName = HandleMouse(bloat, xevent.xmotion.x,
					   xevent.xmotion.y,
					   &doProcMode, 1);
		    procPid = -1;
		    oldProgBarTotal = 0;
		    break;
		case ButtonPress:
		    if (xevent.xbutton.button == 1) {
			needNewColors = 1;
			if (doProcMode && procName && progBloat) {
			    PROGRAM *region;
			    region = SelectRegion(progBloat,
						  xevent.xbutton.x,
						  xevent.xbutton.y);
			    if (region && region->vaddr) {
				pid = -1;
				if (procPid != -1) {
				    pid = procPid;
				} else {
				    b = bloat;
				    while (b && strcmp(b->progName,
						       procName) != 0) {
					b = b->next;
				    }
				    if (b) {
					pid = b->pid;
				    }
				}
				if (pid != -1) {
				    RegionView(pid, region->vaddr);
				    break;
				}
			    }
			}

			if (bloat && !doHelp) {
			    procName = HandleMouse(bloat,
						   xevent.xbutton.x,
						   xevent.xbutton.y,
						   &doProcMode, 0);
			    procPid = -1;
			    oldProgBarTotal = 0;
			    if (procName && strcmp(procName, IRIX) == 0) {
				newStype = Nostype;
			    }
			}
		    }
		    break;
		case MapNotify:
		    doFreeze = 0;
		    break;
		case UnmapNotify:
		    doFreeze = 1;
		    break;
		case KeyRelease:
		    XLookupString(&xevent.xkey, keyBuf, sizeof keyBuf,
				  &keySym, NULL);
		    switch (keySym) {
#ifdef CAP_DEBUG
		    case XK_d:
			system("./capdebug");
			system("id");
			break;
#endif
		    case XK_h:
			/*
			 * Help
			 */
			doHelp = 1;
			needNewColors = 1;
			break;
		    case XK_space:
			/*
			 * End of help
			 */
			doProcMode = 0;
			doHelp = 0;
			break;
		    case XK_m:
			newStype = stype % Phys;
			newType = MappedObjects;
			doHelp = 0;
			InitInodes();
			needNewColors = 1;
			break;
		    case XK_p:
			/*
			 * Physical Memory Breakdown
			 */
			if (procName && strcmp(procName, IRIX) == 0) {
			    newStype = Nostype;
			} else {
			    newStype = stype % Phys;
			}
			newType = Physical;
			doHelp = 0;
			needNewColors = 1;
			break;
		    case XK_r:
			/*
			 * Resident Sizes of Processes
			 */
			if (!doProcMode
			    || strcmp(procName, IRIX) != 0) {
			    newStype = stype % Res;
			    newType = Resident;
			    doHelp = 0;
			    needNewColors = 1;
			}
			break;
		    case XK_s:
			/*
			 * Total Sizes of Processes
			 */
			if (!doProcMode
			    || strcmp(procName, IRIX) != 0) {
			    newStype = stype % (Res + 1);
			    newType = Size;
			    doHelp = 0;
			    needNewColors = 1;
			}
			break;
		    case XK_t:
			if (doProcMode && progBloat) {
			    PrintProcBloat(stdout, progBloat,
					   procName, procPid);
			} else if (bloat) {
			    if (type == MappedObjects) {
				PrintMapBloat(stdout, bloat);
			    } else {
				PrintAllBloat(stdout, bloat);
			    }
			}
			break;
		    case XK_v:
			doHelp = 0;
			newStype = stype + 1;
			switch (type) {
			case Size:
			    newStype %= Res + 1;
			    break;
			case Resident:
			    newStype %= Res;
			    break;
			case Physical:
			    if (procName && strcmp(procName, IRIX) == 0) {
				newStype = Nostype;
				break;
			    }
				/* intentional fall through */
			case MappedObjects:
			    newStype %= Phys;
			    break;
			default:
			    newStype = Nostype;
			    break;
			}
			break;
		    case XK_y:
			printModeOn = !printModeOn;
			ShowPrintMode(printModeOn);
			break;
		    case XK_Escape:
			/*
			 * Exit
			 */
			exit(0);
			break;
		    case XK_Up:
			/*
			 * Increase threshhold
			 */
			if (threshHold == 1)
			    threshHold = 0;
			threshHold += threshDelta;
			SetThreshHold(threshHold);
			doHelp = 0;
			needNewColors = 1;
			break;
		    case XK_Down:
			/*
			 * Decrease threshhold
			 */
			if (threshHold == threshDelta)
			    threshHold = 1;
			else
			    threshHold -= threshDelta;
			if (threshHold < 0) {
			    threshHold = 0;
			}
			SetThreshHold(threshHold);
			doHelp = 0;
			needNewColors = 1;
			break;
		    case XK_Page_Up:
			if (doProcMode && procPid != -1
			    && stype != MappedObjects) {
			    b = bloat;
			    while (b && strcmp(b->progName, procName) != 0) {
				b = b->next;
			    }
			    if (b) {
				pids = b->pids;
				while (pids && pids->pid != procPid) {
				    pids = pids->next;
				}
				if (pids && pids->prev) {
				    procPid = pids->prev->pid;
				} else {
				    procPid = -1;
				}
			    } else {
				procPid = -1;
			    }
			}
			break;
		    case XK_Page_Down:
			if (doProcMode && stype != MappedObjects) {
			    b = bloat;
			    while (b && strcmp(b->progName, procName) != 0) {
				b = b->next;
			    }
			    if (b && b->pids) {
				pids = b->pids;
				if (procPid == -1) {
				    procPid = pids->pid;
				} else {
				    while (pids && pids->pid != procPid) {
					pids = pids->next;
				    }
				    if (pids && pids->next) {
					procPid = pids->next->pid;
				    }
				}
			    } else {
				procPid = -1;
			    }
			}
			break;
		    }
		}
	    }
	}
    }
}
