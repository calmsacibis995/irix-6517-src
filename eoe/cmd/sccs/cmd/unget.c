/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sccs:cmd/unget.c	6.9" */
#ident	"$Revision: 1.6 $"
# include	"../hdr/defines.h"
# include	"../hdr/had.h"
# include	<unistd.h>
# include       <sys/utsname.h>

/*
 * This array of structures is a table with commands and options that they
 * accept (that need arguments). The options might be separated by blank
 * spaces from their arguments. This table is used in preprocessing such
 * input command lines, allowing spaces betwen options/args.
 */
static  struct cmd_str {
        char    *cmd;
        char    *optns;
} cmd_tbl[] = {
        { "unget",    "r" },
	{  0,	      0	  }
};

#define	OPRD_NEED_ARG(str)	(!strcmp(str, "-r"))

/*
		Program can be invoked as either "unget" or
		"sact".  Sact simply displays the p-file on the
		standard output.  Unget removes a specified entry
		from the p-file.
*/

static int	verbosity;
static int	num_files;
static int	cmd;
static long	Szqfile;
static char	Pfilename[FILESIZE];
static struct packet	gpkt;
static struct sid	sid;
static struct utsname 	un;
static char *uuname;

static void	unget(), catpfile();
static struct pfile	 *edpfile();

struct stat64	Statbuf;
char	Error[128];

void	chksid(), setsig(), do_file(), exit(), sinit();
void	ffreeall(), pf_ab(), clean_up();
void	increment_had();

char	*auxf();

int	fatal(), strcmp(), setjmp(), uname(), lockit();
int	xunlink(), unlockit(), unlink(), wait();
int	xopen(), xcreat(), fstat64(), mylock();

main(argc,argv)
int argc;
char *argv[];
{
	int	i, j, testmore, fg;
	int	f, nomoreoptn;
	char	c, *p;
	char	*sid_ab();
	char	*argvtmp;
	extern	int Fcnt;

	Fflags = FTLEXIT | FTLMSG | FTLCLN;
        /* Do some pre-processing first to take care of options that
           Have some space between the key letter and the argument
         */
	if (!strcmp(argv[0], "sact"))
		goto start;
        for (i = 1, j = -1, fg = 0; i < argc; i++){
                if (j == -1){
                        if (OPRD_NEED_ARG(argv[i])){
                                j = i;
                                fg = 1;
                        }
                }
                else {
                        if (fg){
				argvtmp = (char *) malloc(strlen(argv[i])+
							  strlen(argv[j])+1);
				strcpy(argvtmp, argv[j]);
				strcat(argvtmp, argv[i]);
				argv[j] = argvtmp;
                                fg = 0;
                                j++;
                        }
                        else {
                                argv[j] = (char *) strdup(argv[i]);
                                if (OPRD_NEED_ARG(argv[j])){
                                        fg = 1;
                                }
                                else {
                                        j++;
                                }
                        }
                }
        }
        if (j != -1)
                argc = (fg == 1) ? ++j: j;
        /* Pre-processing of command line arguments is done */
start:
	nomoreoptn = 0;
	for(i=1; i<argc; i++){
		j = 1;
		if(!strcmp(argv[i], "--")){
			/* No more options to follow */
			nomoreoptn = 1;
		}
		else if(argv[i][0] == '-' && nomoreoptn){
			/* What looks like an option is an operand */	
			argv[i]++;
			num_files++;
		}
		else if(argv[i][0] == '-' && argv[i][1] && !nomoreoptn) {
			p = &argv[i][2];
label:			testmore = 0; 
			c = argv[i][j];
			if (c){
				switch (c) {
				case 'r':
					if (!p[0]) {
						argv[i] = 0;
						continue;
					}
					chksid(sid_ab(p,&sid),&sid);
					increment_had(c);
					break;
				case 'n':
				case 's':
					testmore++;
					increment_had(c);
					j++;
					goto label;
				default:
					fatal("unknown key letter (cm1)");
				}
			}
			if (testmore) {
				testmore = 0;
				if (*p) {
					sprintf(Error,
						"value after %c arg (cm7)",c);
					fatal(Error);
				}
			}
			argv[i] = 0;
		}
		else num_files++;
	}
	if(num_files == 0)
		fatal("missing file arg (cm3)");

	/*	If envoked as "sact", set flag
		otherwise executed as "unget".
	*/
	if (equal(sname(argv[0]),"sact")) {
		cmd = 1;
		HADS = 0;
	}

	if (!HADS)
		verbosity = -1;
	setsig();
	Fflags &= ~FTLEXIT;
	Fflags |= FTLJMP;
	for (i=1; i<argc; i++)
		if (p=argv[i]){
			if (strcmp(argv[i], "--"))
				do_file(p, unget);
		}
	exit(Fcnt ? 1 : 0);
}

static void
unget(file)
char *file;
{
	extern	char had_dir, had_standinp;
	extern	char *Sflags[];
	int	i, status;
	char	gfilename[FILESIZE];
	char	str[BUFSIZ];
	char	*sid_ba();
	int	check_read, check_exist;
	struct	pfile *pp;

	if (setjmp(Fjmp))
		return;

	/*	Initialize packet, but do not open SCCS file.
	*/
	sinit(&gpkt,file,0);
	gpkt.p_stdout = stdout;
	gpkt.p_verbose = verbosity;

	copy(auxf(gpkt.p_file,'g'),gfilename);
	check_exist = access(gpkt.p_file, F_OK);
	/* Check to see if the file exists */
        if (check_exist)
		fatal(" `...' nonexistent (un4)");
	check_read = access(gpkt.p_file, R_OK);
        if (gpkt.p_verbose && (num_files > 1 || had_dir || had_standinp))
                /* We are to silently ignore files that are not */
                /* readable. Check to see if this file can be read */
                if (!check_read)
                        fprintf(gpkt.p_stdout,"\n%s:\n",gpkt.p_file);
		else	return;
	/*	If envoked as "sact", call catpfile() and return.
	*/
	if (cmd) {
		catpfile(&gpkt);
		return;
	}
	uname(&un);
	uuname = un.nodename;
	if (lockit(auxf(gpkt.p_file,'z'),2, getpid(),uuname))
		fatal("cannot create lock file (cm4)");
	pp = edpfile(&gpkt,&sid);
	if (gpkt.p_verbose) {
		sid_ba(&pp->pf_nsid,str);
		fprintf(gpkt.p_stdout,"%s\n",str);
	}
	fflush(gpkt.p_stdout);

	/*	If the size of the q-file is greater than zero,
		rename the q-file the p-file and remove the
		old p-file; else remove both the q-file and
		the p-file.
	*/
	if (Szqfile)
		rename(auxf(gpkt.p_file,'q'),Pfilename);
	else {
		xunlink(Pfilename);
		xunlink(auxf(gpkt.p_file,'q'));
	}
	ffreeall();
	uname(&un);
	uuname = un.nodename;
	unlockit(auxf(gpkt.p_file,'z'), getpid(),uuname);

	/*	A child is spawned to remove the g-file so that
		the current ID will not be lost.
	*/
	if (!HADN) {
		if ((i = fork()) < 0)
			fatal("cannot fork, try again");
		if (i == 0) {
			setuid((int) getuid());
			unlink(gfilename);
			exit(0);
		}
		else {
			wait(&status);
		}
	}
}


static struct pfile *
edpfile(pkt,sp)
struct packet *pkt;
struct sid *sp;
{
	static	struct pfile goodpf;
	char	*user, *logname();
	char	line[BUFSIZ];
	struct	pfile pf;
	int	cnt, name;
	FILE	*in, *out, *fdfopen();

	cnt = -1;
	name = 0;
	user = logname();
	zero((char *)&goodpf,sizeof(goodpf));
	in = xfopen(auxf(pkt->p_file,'p'),0);
	out = xfcreat(auxf(pkt->p_file,'q'),(mode_t)0644);
	while (fgets(line,sizeof(line),in) != NULL) {
		pf_ab(line,&pf,1);
		if (equal(pf.pf_user,user)) {
			name++;
			if (sp->s_rel == 0) {
				if (++cnt) {
					fclose(out);
					fclose(in);
					fatal("SID must be specified (un1)");
				}
				goodpf = pf;
				continue;
			}
			else if (sp->s_rel == pf.pf_nsid.s_rel &&
				sp->s_lev == pf.pf_nsid.s_lev &&
				sp->s_br == pf.pf_nsid.s_br &&
				sp->s_seq == pf.pf_nsid.s_seq) {
					goodpf = pf;
					continue;
			}
		}
		fputs(line,out);
	}
	fflush(out);
	fflush(stderr);
	fstat64((int) fileno(out),&Statbuf);
	Szqfile = Statbuf.st_size;
	copy(auxf(pkt->p_file,'p'),Pfilename);
	fclose(out);
	fclose(in);
	if (!goodpf.pf_user[0])
		if (!name)
			fatal("login name not in p-file (un2)");
		else fatal("specified SID not in p-file (un3)");
	return(&goodpf);
}


/* clean_up() only called from fatal().
*/

void
clean_up()
{
	/*	Lockfile and q-file only removed if lockfile
		was created by this process.
	*/
	uname(&un);
	uuname = un.nodename;
	if (mylock(auxf(gpkt.p_file,'z'), getpid(),uuname)) {
		unlink(auxf(gpkt.p_file,'q'));
		ffreeall();
		unlockit(auxf(gpkt.p_file,'z'), getpid(),uuname);
	}
}

static void
catpfile(pkt)
struct packet *pkt;
{
        int c;
	int check_read;
        FILE *in;

        /* Check to see if the "s.file" is readable */
        /* If yes, go ahead and look for "p.file"   */
        /* If not, silently ignore this file        */
	check_read = access(pkt->p_file, R_OK);
        if (!check_read){
                if(!(in = fopen(auxf(pkt->p_file,'p'),"r")))
                        fprintf(stderr,
                                "No outstanding deltas for: %s\n",pkt->p_file);
                else {
                        while ((c = getc(in)) != EOF)
                                putc(c,pkt->p_stdout);
                        fclose(in);
                }
        }
}

void    increment_had(c)
char    c;
{
        had[c-'a']++;
        if (had[c-'a'] > 1)
                fatal("key letter twice (cm2)");
        return;
}

