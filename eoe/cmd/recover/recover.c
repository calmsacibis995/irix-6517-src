#include "recover.h"
#include <sys/param.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <mntent.h>
#include <unistd.h>
#include <sys/statfs.h>

int getwords(char *, char **, int);
int run(int, char **argv, int);

int trace;	/* don't hide debug info, and show exec cmds */

extern int _close(int);
extern _execv(const char *, char *const *);

close(int fd)
{
	if(!trace || fd != 2) return _close(fd);
	return 0;
}

execv(const char *p, char *const *args)
{
	if(trace) {
		char *const *targs = args;
		fprintf(stderr, "RUN cmd=%s ", p);
		while(*targs)
			fprintf(stderr, "{%s} ", *targs++);
		fprintf(stderr, "\n");
	}
	return _execv(p, args);
}


#define R	0
#define W	1

int	pipe_kids [NOFILE];

filematch (char *name, char *pat)
{
	int		c, m;
	char		*p;

	if (*name == '\0') {
		return (*pat == '\0');
	}
	if (*pat == 0) return (1);
	while ((c = *name) || (*pat)) {
		switch (*pat) {
		case '*':
			for (p = name + strlen (name); p >= name; --p) {
				if (filematch (p, pat + 1)) return (1);
			}
			return (0);
		case '[':
			++pat;
			m = 0;
			while (*pat && *pat != ']') {
				if (pat [1] == '-' && pat [2] != '\0') {
					if (c >= pat [0] && c <= pat [2])
						++m;
					pat += 3;
				} else {
					if (c == *pat) ++m;
					pat += 1;
				}
			}
			if (!m) return (0);
			if (*pat) ++pat;
			break;
		case '?':
			++pat;
			break;
		default:
			if (c != *pat++) return (0);
			break;
		}
		++name;
	}
	return (1);
}

derivedev (char *base, char *dev, int fs)
{
	char		*pfs;

	(void) strcpy (dev, base);
	pfs = dev + strlen (dev) - 1;
	if (filematch (base, "*d[0-9]s[0-9]")) {
		switch (fs) {
		case Root:
			*pfs = '0'; break;
		case Usr:
			*pfs = '6'; break;
		}
	} else if (filematch (base, "*[0-9][a-h]")) {
		switch (fs) {
		case Root:
			*pfs = 'a'; break;
		case Usr:
			if (filematch (base, "*si[0-9][a-h]")) {
				*pfs = 'f'; break;
			} else {
				*pfs = 'c'; break;
			}
		}
	} else {
		return (-1);
	}
	return (0);
}

pid_t pipe_pid;

waitpipe (int fd)
{
	int	 	w, status;
	void		(*istat) (), (*qstat) ();

	if (fd < 0 || fd >= NOFILE) return (-1);
	if (pipe_kids [fd] == 0) return (-1);
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	w = waitpid(pipe_pid, &status, 0);
	if(trace) fprintf(stderr, "wait=%d status=%#x\n", w, status);
	(void) signal (SIGINT, istat);
	(void) signal (SIGQUIT, qstat);
	pipe_kids [fd] = 0;
	return ((w == -1 || status != 0) ? -1 : 0);
}

runpipe (int rargc, char **rargv, int mode)
{
	char		cmd [1024], *p;
	int	 	pipefd [2], fd, pid;

	if (pipe (pipefd) < 0) {
		(void) perror (rargv [0]); return (-1);
	}
	if ((pid = fork ()) == 0) {
		if (mode == R) {
			(void) close (1);
			(void) close (2);
			(void) fcntl (pipefd [W], F_DUPFD, 1);
			(void) fcntl (pipefd [W], F_DUPFD, 2);
			(void) close (pipefd [R]);
			(void) close (0);
		} else {
			(void) close (0);
			(void) fcntl (pipefd [R], F_DUPFD, 0);
			(void) close (pipefd [W]);
			(void) close (1);
		}
		(void) strcpy (cmd, rargv [0]);
		for (p = rargv [0] + strlen (rargv [0]) - 1;
		    p >= rargv [0] && *p != '/'; --p) ;
		rargv [0] = p + 1;
		rargv [rargc] = NULL;
		(void) execv (cmd, rargv);
		(void) perror (cmd);
		_exit (127);
	}
	if (mode == R) {
		fd = pipefd [R];
		(void) close (pipefd [W]);
	} else {
		fd = pipefd [W]; (void) close (pipefd [R]);
	}
	if (pid == -1) {
		(void) close (pipefd [R]); (void) close (pipefd [W]);
		(void) perror (cmd);
		return (-1);
	} else {
		pipe_kids [fd] = pid;
		return (fd);
	}
}

vline (int vargc, char **vargv, char *line )
{
	int		f, n;

	if ((f = runpipe (vargc, vargv, 0)) < 0) return (-1);
	while ((n = read (f, line, 1)) == 1 && *line++ != '\n') ;
	*line = '\0';
	(void) close (f);
	return (waitpipe (f) < 0 || n < 0 ? -1 : 0);
}

char *
devnm (char *path)
{
	static char	buff [Strsize];
	char		*dargv [32], argbuff [Strsize];

	dargv [0] = "/etc/devnm";
	dargv [1] = path;
	dargv [2] = NULL;
	if (vline (2, dargv, argbuff) < 0) {
		return (NULL);
	}
	if (getwords (argbuff, dargv, N (dargv)) != 2) {
		return (NULL);
	}
	if (strcmp (dargv [0], "devnm:") == 0) {
		return (NULL);
	}
	if (strncmp (dargv [0], "/dev/", 5) != 0) {
		(void) strcpy (buff, "/dev/");
	} else {
		(void) strcpy (buff, "");
	}
	(void) strcat (buff, dargv [0]);
	return (buff);
}

/* typ needs to be **, so we can change the type from the question here */
void
mkfs(char *name, char *mountpt, char **typ)
{
	char buf[Strsize];
	char		*yargv [5];

	yargv [0] = "/etc/mkfs";
	yargv [1] = "-t";
	yargv [3] = name;
	yargv [4] = 0;

	/* Always ask them, since recover only asks about all of the
	 * filesystems, without listing them.  Also, mkfs can be run
	 * internally if fsstat doesn't show a filesystem present.
	 * Otherwise people can lose a lot of files without realizing it... */
	fprintf(stderr, "Create a filesystem on %s for %s [ yes or no ] (no)? ",
		name, mountpt);
	if(fgets(buf, sizeof(buf)-1, stdin) == NULL ||
		(*buf != 'y' && *buf != 'Y')) {
		fprintf(stderr, "Keeping existing filesystem %s on %s\n",
				mountpt, name);
		return;
	}

	*typ = "xfs";	/* always xfs as of irix 6.4 */

	fprintf(stderr, "Making %s filesystem %s on %s, type %s\n",
			*typ, mountpt, name);

	yargv [2] = *typ;
	if(run (4, yargv, Silent))
		fprintf(stderr, "The mkfs command apppears to have failed, may have problems\n");
}

void
mountfs(char *fs, char *dir, char *typ)
{
	char		buff [Strsize], *xargv [32];
	int		xargc, notyp=1;

	/* try to make the directory; If it fails with ENOENT,
	 * run the system mkdir -p, to do our best.  mount will
	 * catch it if it still doesn't work (a file is there,
	 * etc.). */
	if(mkdir(dir, 0755) && errno == ENOENT) {
		sprintf(buff, "mkdir -p %s >/dev/null 2>&1", dir);
		(void)system(buff);
	}
	if(typ) {
		sprintf (buff, "/etc/mount -t %s %s %s", typ, fs, dir);
		xargc = getwords (buff, xargv, N (xargv));
		notyp = run(xargc, xargv, Silent);
	}
	if(notyp) {	/* no type given, or mount with type failed */
		/* try again without specifying a type */
		sprintf (buff, "/etc/mount %s %s", fs, dir);
		xargc = getwords (buff, xargv, N (xargv));
		notyp = run(xargc, xargv, Silent);
	}
	if(notyp)
		fprintf(stderr, "The mount of %s on %s seems to have failed, may have problems\n",
			fs, dir);
}

void
umountfs(char *name)
{
	char buff [Strsize], *uargv [10];
	int  uargc;

	sprintf (buff, "/etc/umount %s", name);
	uargc = getwords (buff, uargv, N (uargv));
	(void) run(uargc, uargv, Silent);
}

int
getwords (char *bp, char **argv, int maxarg)
{
	int		argc, quote;

	argc = 0;
	while (*bp) {
		while (isspace ((int) *bp)) ++bp;
		if (*bp == '\0') break;
		argv [argc] = bp;
		for (quote = '\0'; *bp != '\0'; ++bp) {
			if (*bp == '\\') {
				++bp;
				continue;
			}
			if (quote) {
				if (*bp == quote) 
					quote = 0;
			} else {
				if (*bp == '\'' || *bp == '"')
					quote = *bp;
				else if (isspace (*bp))
					break;
			}
		}
		if (*bp != '\0') *bp++ = '\0';
		if (*argv [argc]) ++argc;
		if (argc >= maxarg) break;
	}
	if (argc < maxarg) argv [argc] = (char *) 0;
	return (argc);
}

int
run (int argc, char **argv, int volume)
{
	char		cmd [1024], *p;
	int	 	pid, w, status;
	void		(*istat) (), (*qstat) ();

	if ((pid = fork ()) == 0) {
		(void) strcpy (cmd, argv [0]);
		for (p = argv [0] - 1; p >= argv [0] && *p != '/'; --p) ;
		argv [0] = p + 1;
		argv [argc] = NULL;
		if (volume == Silent) {
			(void) close (1);
			(void) close (2);
			(void) open ("/dev/null", 1);
			(void) open ("/dev/null", 1);
		}
		(void) execv (cmd, argv);
		(void) perror (cmd);
		_exit (127);
	}
	if (pid == -1) {
		(void) perror (cmd);
		return (-1);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	w = waitpid(pid, &status, 0);
	if(trace) fprintf(stderr, "wait=%d status=%#x\n", w, status);
	(void) signal (SIGINT, istat);
	(void) signal (SIGQUIT, qstat);
	return ((w == -1) ? w : status);
}


void
checkdevice(char *device, char *mountpt, char **typ)
{
    char buf[Strsize];
    char *margv[32];
    int	margc;

    sprintf(buf, "/etc/fsstat %s", device);
    margc = getwords(buf, margv, N (margv));
    switch(run(margc, margv, Silent)) {
	case 0       :
	case (2 << 8):
		umountfs(device);
		/* and fall through */
	case (1 << 8):
		if(*typ && strcmp(*typ, "efs") == 0) {
			sprintf(buf, "/etc/fsck -g %s", device);
			margc = getwords(buf, margv, N (margv));
			if(run(margc, margv, Silent)) {
			fprintf(stderr,
			  "The %s (%s) filesystem is damaged enough that it can not\n"
			  "be safely repaired.  Do you wish to force repairs that\n"
			  "may result in loss of files [yes or no] (no)? ",
			  device, mountpt);
			if(fgets(buf, sizeof(buf)-1, stdin) == NULL ||
				(*buf != 'y' && *buf != 'Y')) {
				fprintf(stderr, "recover aborted\n");
				exit(1);
			}
			sprintf(buf, "/etc/fsck -y %s", device);
			margc = getwords(buf, margv, N (margv));
			if(run(margc, margv, Silent)) {
				fprintf(stderr,
				  "Failed to repair %s (%s) filesystem, aborting\n",
				  device, mountpt);
				exit(1);
			}
			}
		}
		else /* no xfs_repair yet; do not do fsck if type not known
			either... */
	    break;
	case (3 << 8):
	    mkfs(device, mountpt, typ);
	    break;
	default:
	    fprintf(stderr, "Internal error: fsstat %s (%s).\n", device,
		    mountpt);
	    exit(1);
    }
}

void
usage(void)
{
	/* don't mention -t in usage */
	fprintf(stderr, "Usage: recover [-m] [-f mtab_file]\n");
	exit(1);
}

main(int argc, char **argv)
{
	int	makefs_flag = 0;
	int	extrafs = 0;
	struct	mntent *mnt;
	char	rootdev[Pathlen];
	char	*fs;
	FILE	*f;
	FILE	*exf;
	char	exdir[Pathlen], exdev[Pathlen], extyp[Pathlen], rtexdir[Pathlen];
	char *unk_fs = NULL;
	int	o;

	while((o=getopt(argc, argv, "mtf:")) != -1) {
		switch(o)  {
		case 'm':
			makefs_flag = 1;
			break;
		case 't':
			trace++;
			break;
		case 'f':
			extrafs = 1;
			/* allow this to silently fail, because of way it's used */
			if((exf = fopen(optarg, "r")) == NULL)
				extrafs = 0;
			break;
		default:
			usage();
		}
	}
	if(optind != argc)
		usage();

	if(getenv("_RECOVERY_TRACE_")) trace++;

	if(access("/root", 0) != 0) {
		mkdir("/root", 0755);
	}
	fs = devnm("/");
	if(!fs) fs = "/dev/root"; /* be paranoid */
	derivedev( fs, rootdev, Root);

	if((f = setmntent("/root/etc/fstab", "r")) != NULL) {
		while ((mnt = getmntent (f)) != NULL) {
			if(strcmp(mnt->mnt_type, "efs") != 0)
				continue;
			if(hasmntopt(mnt, "hide") != NULL)
				continue;
			if(strcmp(mnt->mnt_dir, "/usr") == 0) {
				fs = devnm("/usr");
				break;
			}
		}
		endmntent(f);
	}

	/* no longer automatically assume /usr is a seperate filesystem.
	 * it won't always be.  restore-mr now passes us the -f option
	 * including /usr, if needed */

	if(makefs_flag)
		mkfs(rootdev, "/", &unk_fs);
	else
		checkdevice(rootdev, "/", &unk_fs);

	mountfs(rootdev, "/root", unk_fs);

	if(extrafs) {
		char *fstyp = extyp;
		while (fscanf(exf, "%s%s%s", exdev, exdir, extyp) != EOF) {
			if(makefs_flag)
				mkfs(exdev, exdir, &fstyp);
			else
				checkdevice(exdev, exdir, &fstyp);
			strcpy(rtexdir, "/root");
			strcat(rtexdir, exdir);
			mountfs(exdev, rtexdir, fstyp);
		}
		fclose(exf);
	}
	return 0;
}
