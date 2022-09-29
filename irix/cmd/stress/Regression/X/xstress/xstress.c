
/*
 * xstress
 * 
 * $Revision: 1.1 $
 * 
 * Mark J. Kilgard
 * 
 * Front end for starting multiple instances of an X client at different
 * locations on the screen (using the -geometry option) to stress the X
 * server (and probably the operating system too!).  The clients will be
 * arranged in a tiled pattern so all the clients are visible.
 * 
 * -clients # specifies the number of X clients to start (defaults to 9).
 * 
 * -time # specifies seconds to wait after xstress has started before killing
 * all the clients and quitting (defaults to 12 seconds).
 * 
 * -display ??? specifies the X server to connect to.
 * 
 * EXAMPLE: xstress -clients 23 -time 5 xcalc -fg blue -bg red
 *
 * NOTE: the client being run by xstress should abide by the -geometry
 * option.  Some programs like ico don't set their hints right and
 * appear in a jumble.  This is the clients' fault for not properly
 * handling the -geometry option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>

Display        *dpy;
char           *display_name = NULL;
int             clients = 9;
int             wait_time = 12;
char          **command_argv = NULL;
int             command_argc = 0;
int             screen;
int             screen_width;
int             screen_height;
char            cmd[256] = "???";
char            display_env[256];
pid_t          *pids;

void sigchld()
{
    wait(0);
    if (!--clients)
	exit(0);
    sigrelse(SIGCHLD);
}

main(argc, argv)
    int             argc;
    char           *argv[];
{
    int             across;
    int             down;
    int             count;
    pid_t           pid;
    int             i;
    int             j;
    int             k;

    for (i = 1; i < argc; i++) {
	char           *arg;

	arg = argv[i];
	if (strcmp(arg, "-display") == 0 || strcmp(arg, "-d") == 0) {
	    arg = argv[++i];
	    if (arg == NULL) {
		fprintf(stderr, "xstress: missing argument to -clients\n");
		exit(1);
	    }
	    display_name = arg;
	    sprintf(display_env, "DISPLAY=%s", display_name);
	    putenv(display_env);
	} else if (strcmp(arg, "-clients") == 0) {
	    arg = argv[++i];
	    if (arg == NULL) {
		fprintf(stderr, "xstress: missing argument to -clients\n");
		exit(1);
	    }
	    clients = atoi(arg);
	} else if (strcmp(arg, "-time") == 0) {
	    arg = argv[++i];
	    if (arg == NULL) {
		fprintf(stderr, "xstress: missing argument to -time\n");
	    }
	    wait_time = atoi(arg);
	} else {
	    command_argc = argc - i + 2 /* two for geometry */ ;
	    command_argv = (char **)
		malloc((command_argc + 1) * sizeof(char *));
	    command_argv[0] = arg;
	    command_argv[1] = "-geometry";
	    command_argv[2] = cmd;
	    command_argv[command_argc] = NULL;
	    for (j = i + 1, k = 3; j < argc; j++, k++) {
		command_argv[k] = argv[j];
	    }
	    break;
	}
    }
    if (command_argv == NULL) {
	fprintf(stderr,
		"Usage: xstress [-display XXX] [-clients #] [-time #] command ...\n");
	exit(1);
    }
    dpy = XOpenDisplay(display_name);
    if (dpy == NULL) {
	fprintf(stderr, "xstress: could not open display %s\n",
		XDisplayName(display_name));
	exit(1);
    }
    sigset(SIGCHLD,sigchld);
    screen = DefaultScreen(dpy);
    screen_width = DisplayWidth(dpy, screen);
    screen_height = DisplayHeight(dpy, screen);
    pids = (pid_t *) malloc(sizeof(pid_t) * clients);
    down = (int) sqrt(clients);
    across = (clients + down - 1) / down;
    count = 0;
    for (i = 0; i < down; i++) {
	for (j = 0; j < across; j++) {
	    count++;
	    sprintf(cmd, "%dx%d+%d+%d",
		    screen_width / across - 20,
		    screen_height / down - 50,
		    (screen_width / across) * j + 10,
		    (screen_height / down) * i + 20);
	    pid = fork();
	    if (pid == 0) {
		close(ConnectionNumber(dpy));
		execvp(command_argv[0], command_argv);
		fprintf(stderr, "xstress: exec error\n");
		exit(1);
	    } else {
		pids[count - 1] = pid;
		if (pid == -1) {
		    printf("xstress: fork error\n");
		} else {
		    /* pause to let operating system catch its breath */
		    sleep(1);
		}
	    }
	    if (count >= clients) {
		goto done;
	    }
	}
    }
  done:
    sleep(wait_time);
    for (i = 0; i < clients; i++) {
	pid = pids[i];
	if (pid != -1) {
	    kill(pid, SIGKILL);
	}
    }
    exit(0);
}
