#include "sys/types.h"
#include "sys/idbg.h"
#include "sys/syssgi.h"
#include "unistd.h"
#include "stdlib.h"
#include "malloc.h"
#include "string.h"
#include "stdio.h"
#include "ctype.h"
#include "errno.h"
#include "a.out.h"
#include "obj.h"
#include "string.h"
#include "unistd.h"
#include "sys/wait.h"
#include "getopt.h"
#include "sys/capability.h"

/*
 * idbg - invoke kernel debugger status dump
 */
#define RSZ		(1024*1024)
#define MAXARGS		4
dbgoff_t *db;		/* list of all posible functions */
char *Cmd;
int rflag = 0;			/* record output */
int useregexflag = 0;
char *rname;
FILE *rfd;
char *filename = "/unix";

#if (_MIPS_SZPTR == 64)
#define ADDR_FMT	"0x%-16.16lx"
#else
#define ADDR_FMT	"0x%-8.8x"
#endif

#define INTERPRET_ARGS		1
#define DONT_INTERPRET_ARGS	2
#define STRING_ARGS		3

#define NM_PATH		"/usr/bin/nm"

/*
 * Command used to get the kernel address.
 */
#define KADDR		"kaddr"

/*
 * For now, only support addition, subtraction.
 */
#define isoperator(c)	((c) == '+' || (c) == '-')

struct iarg {
	caddr_t addr;
	unsigned len;
} iargs[MAXARGS];
__psint_t intargs[MAXARGS];


#define CMDLINE

#ifdef	CMDLINE
int onecmd;
#endif

int Print_width = 1;
/*
 * Data structures associated with mapping of symbol names.
 */
typedef struct symelem {
	struct symelem	*sym_anext;
	struct symelem	*sym_nnext;
	char		*sym_name;
	__psunsigned_t	sym_value;
} symelem_t;

symelem_t *symelem_aq = 0;
symelem_t *symelem_nq = 0;

void insertsym(symelem_t *);
int loadsym(void);

int loadedallsyms = 0;

#define LOADSYM(x)	if (!loadedallsyms && !loadsym()) return (x) 

int _dec(struct idbgcomm *);
int _hex(struct idbgcomm *);
int _what(struct idbgcomm *);
int _quit(struct idbgcomm *);

struct builtincomm {
	char	*comm_name;
	int	(*comm_func)(struct idbgcomm *);
	int	comm_args;
} BuiltInComm[] = {
	"dec",		_dec,		DONT_INTERPRET_ARGS,
	"exit",		_quit,		DONT_INTERPRET_ARGS,
	"hex",		_hex,		DONT_INTERPRET_ARGS,
	"what",		_what,		DONT_INTERPRET_ARGS,
	"quit",		_quit,		DONT_INTERPRET_ARGS,
	0,		0,		0
};

struct builtincomm SpecialComm[] = {
	"kaddr",	0,		STRING_ARGS,
	"semctl",	0,		STRING_ARGS,
	"setpri",	0,		STRING_ARGS,
	0,		0,		0,
};

char *glob2regex(char* out, const char* in);
static unsigned int isRegEx(const char * str);
int builtin(struct idbgcomm *);
int chkparsetype(char *);
__psunsigned_t symtol(char *);
__psunsigned_t convexpr(char *);
__psunsigned_t lookupsymbyname(char *);
static void getargs(char *name, char *buf, struct idbgcomm *arg, int parsetype);
static int gettst(register char *buf, register struct idbgcomm *arg);
static void hddump(char *addr, char *buf, int len, int size);
static void prlist(void);
static void Usage(void);

static caddr_t uaddr;
static cap_value_t cap_sysinfo_mgt[] = {CAP_SYSINFO_MGT};

void
main(int argc, char **argv)
{
	char inbuf[1024];
	int c;
	struct idbgcomm ic;
	long len;
	int donext;
	ptrdiff_t tablesize;
	cap_t ocap;
	
	setbuf(stderr, NULL);

	Cmd = argv[0];

	while ((c = getopt(argc, argv, "r:f:R")) != EOF)
		switch (c) {
		case 'r':	/* record flag */
			rflag++;
			rname = optarg;
			break;
		case 'f':	/* unix name */
			filename = optarg;
			break;
		case 'R':
			useregexflag++;
			break;
		default:
			fprintf(stderr, "Invalid option:%c\n", c);
			Usage();
		}
#ifdef	CMDLINE
	if ( optind < argc ) {
		onecmd = 1;
		for ( inbuf[0] = '\0'; optind < argc; optind++ ) {
			strcat(inbuf, argv[optind]);
			strcat(inbuf, " ");
		}
	}
#endif

	ocap = cap_acquire(1, cap_sysinfo_mgt);
	if ((tablesize = syssgi(SGI_IDBG, 3)) < 0) {
		perror("cannot get debugger table size");
		cap_surrender(ocap);
		exit(-1);
	}
	cap_surrender(ocap);

	db = (dbgoff_t *)malloc(sizeof (dbgoff_t) * tablesize);
	ocap = cap_acquire(1, cap_sysinfo_mgt);
	if (syssgi(SGI_IDBG, 0, db, sizeof (dbgoff_t) * tablesize) < 0) {
		perror("cannot read debugger table");
		if (errno == ENODEV)
			fprintf(stderr, "kernel debugger not configured\n");
		cap_surrender(ocap);
		exit(-1);
	}
	cap_surrender(ocap);

	if (rflag) {
		if ((rfd = fopen(rname, "w+")) == NULL) {
			fprintf(stderr, "Cannot open recording file %s\n", rname);
			exit(-1);
		}
	}

	/* alloc some memory for results */
	if ((ic.i_uaddr = malloc(RSZ)) == NULL) {
		fprintf(stderr, "Cannot alloc results buffer!\n");
		exit(-1);
	}
	ic.i_ulen = RSZ;
	uaddr = ic.i_uaddr;

	for (;;) {
#ifdef	CMDLINE
		if ( onecmd )
			goto docmd;
#endif
		donext = 0;
		printf("idbg> ");
		fflush(stdout);
		if (gets(inbuf) == NULL) {
			printf("\n");
			break;
		}

		if (inbuf[0] == '\0') {
			prlist();
			continue;
		}
#ifdef	CMDLINE
docmd:
#endif
		switch (gettst(inbuf, &ic)) {
		case -1:
			fprintf(stderr, "Error <%s>\n", inbuf);
			donext++;
			break;
		case 1:
			switch (builtin(&ic)) {
			case 0:
				fprintf(stderr, "Unknown command <%s>\n",
					inbuf);
				break;
			case 1:
				break;
			case -1:
				fprintf(stderr, "Error in <%s>\n", inbuf);
				break;
			}
			donext++;
			break;
		default:
			break;
		}
		if (donext) {
			if(onecmd)
				exit(1);
			continue;
		}

		ocap = cap_acquire(1, cap_sysinfo_mgt);
		len = syssgi(SGI_IDBG, 1, &ic);
		cap_surrender(ocap);
		if (len < 0) {
			perror("idbg function failed");
		} else if (len > 0) {
			/* see if address error */
			if (ic.i_cause) {
				fprintf(stderr, "Address error @ 0x%x\n", 
					ic.i_badvaddr);
			}
			/* some commands get handled specially */
			if ((strcmp(db[ic.i_func].us_name, "hd")) == 0) {
				hddump((char *)ic.i_arg, ic.i_uaddr,
					(int)len, Print_width);
			} else {
				/* output results */
				write(1, ic.i_uaddr, len);

				if (rflag) {
					fwrite(ic.i_uaddr, 1, len, rfd);
					fflush(rfd);
				}
			}
		} else {
			fprintf(stderr, "No results returned?\n");
		}

#ifdef	CMDLINE
		if ( onecmd )
			exit(0);
#endif
	}
	exit(0);
}

static void
Usage(void)
{
	fprintf(stderr, "%s [-r fname]\n", Cmd);
	exit(-1);
}

static void
prlist(void)
{
	register dbgoff_t *dp;
	struct builtincomm *bp;
	register int i;

	/* print list of allowable commands */

	for (dp = db, i = 0; dp->s_type != DO_END; dp++) {
		if (dp->s_type == DO_ENV)
			continue;
		printf("%8.8s ", dp->us_name);
		if ((i++ % 8) == 7)
			printf("\n");
	}
	printf("\n");
	for (bp = &BuiltInComm[0], i = 0; bp->comm_name; bp++) {
		printf("%8.8s ", bp->comm_name);
		if ((i++ % 8) == 7)
			printf("\n");
	}
	printf("\n");
	fflush(stdout);
}

/*
 * parse input line into test and args
 */
static int
gettst(register char *buf, register struct idbgcomm *arg)
{
	register dbgoff_t *dp;
	char *tname;
	int parsetype;

	arg->i_func = 0;
	arg->i_arg = -1;
	arg->i_argcnt = 0;
	arg->i_argp = (caddr_t) iargs;
	arg->i_badvaddr = 0;
	arg->i_cause = 0;

	/* skip blanks */
	while (*buf && isspace(*buf))
		buf++;
	if (*buf == '\0')
		return(-1);
	
	/* point to name */
	tname = buf;

	/* skip until blank */
	while (*buf && !isspace(*buf))
		buf++;
	/* if not at end of string, insert a null and scan arg */
	if (*buf) {
		/* insert a null */
		*buf++ = '\0';
		
		/*
		 * Before getting args, check to see if builtin -- this
		 * will affect how we parse the arguments.
		 */
		parsetype = chkparsetype(tname);

		/* skip blanks */
		while (*buf && isspace(*buf))
			buf++;
		/* if something left parse it */
		if (*buf != '\0')
			getargs(tname, buf, arg, parsetype);
	} else
		/* end of string - no args use default -1 */
		;

	/* look up command */
	for (dp = db; dp->s_type != DO_END; dp++) {
		if ((strcmp(dp->us_name, tname)) == 0) {
			arg->i_func = (__psint_t)(dp - db);
			return(0);
		}
	}
	arg->i_func = (__psint_t)tname;
	return 1;
}		
	
/*
 * getargs - parse arguments
 */
static void
getargs(char *name, register char *buf, register struct idbgcomm *arg,
		int parsetype)
{
	register int anum = -1;		/* arg # */
	__psunsigned_t result;
	caddr_t addr;
	int len;
	char *nbp;

	if (parsetype == STRING_ARGS) {
		iargs[0].addr = buf;
		iargs[0].len = (int)(strlen(buf)+1);
		arg->i_argcnt = 1;
		return;
	}

	while (*buf) {
		if (anum >= MAXARGS) {
			fprintf(stderr, "Too many args\n");
			return;
		}
		if (isspace(*buf)) {
			buf++;
			continue;
		}
		
		if (*buf == '-') {
			buf++;
			switch(*buf) {
			default:  /* Revert back */ buf--;  break;
			case 'b': Print_width = 1; buf++; break;
			case 'h': Print_width = 2; buf++; break;
			case 'w': Print_width = 4; buf++; break;
			case 'd': Print_width = 8; buf++; break;
			} 

			/* Skip white space */
			while (*buf && isspace(*buf))
				buf++; 
		}

		nbp = buf;
		while (*nbp && !isspace(*nbp))
			nbp++;
		if (*nbp)
			*nbp++ = 0;
		switch (parsetype) {
		case INTERPRET_ARGS:

			if (isdigit(*buf))
				result = strtoul(buf, (char **)NULL, 0);
			else {
				result = symtol(buf);
				if (!result) {
					fprintf(stderr,
						"Unknown symbol '%s'\n", buf);
				}
			}
			addr = (caddr_t)&intargs[anum];
			len = sizeof(__psint_t);
			break;
		case DONT_INTERPRET_ARGS:
			result = (__psunsigned_t)buf;
			addr = (caddr_t)&intargs[anum];
			len = sizeof(__psint_t);
			break;
		}
		if (anum == -1)
			arg->i_arg = (long)result;
		else {
			iargs[anum].addr = (caddr_t)addr;
			iargs[anum].len = len;
			intargs[anum] = (__psint_t)result;
			arg->i_argcnt++;
		}
		buf = nbp;
		anum++;
	}
	
	/* 
	 * Special case.
	 */
	if (!strcmp(name, "hd") && !arg->i_argcnt) {
		iargs[0].addr = (caddr_t)&intargs[0];
		iargs[0].len = sizeof(__psint_t);
		intargs[0] = sizeof(__psint_t);
		arg->i_argcnt++;
	}
}

char *dump_format = {"%2.2x"};

/*
 * hddump - dump a set of numbers as hex
 */
static void
hddump(char *addr, char *buf, int len, int print_width)
{
	register int i, j;
	register char *p, *c;
	char line[1024];
	char work[64];

	for (i = 0; i < len; i += 16) {
		line[0] = '\0';
		sprintf(work, ADDR_FMT ":", addr);
		strcat(line, work);

		/* output a line of stuff */
		for (j = 0; j < 16; j++) {
			if ((j % print_width) == 0)
				strcat(line, " ");
			sprintf(work, dump_format, *(buf+j));
			strcat(line, work);
		}

		/* output ascii rep */
		c = work;
		for (j = 0, p = buf; j < 16; j++, p++) {
			if (isascii(*p) && isprint(*p))
				*c++ = *p;
			else
				*c++ = '.';
		}
		*c++ = '\n';
		*c = '\0';
		strcat(line, work);
		addr += 16;
		buf += 16;

		/* output results */
		write(1, line, strlen(line));

		if (rflag) {
			fwrite(line, 1, strlen(line), rfd);
			fflush(rfd);
		}
	}
}


/*
 * This function implements mapping of symbol names to values in the
 * image.
 */
__psunsigned_t
symtol(char *buf)
{
	__psunsigned_t result;
	char *ws;
	char w;

	/*
	 * Submit a null character at the first whitespace.
	 */
	for (ws = buf; !isspace(*ws) && !isoperator(*ws); ws++) {
	}
	if (*ws) {
		w = *ws;
		*ws = 0;
	} else
		ws = (char *)0;
	
	/*
	 * Now that the table is loaded, attempt to find the appropriate
	 * element.
	 */
	result = lookupsymbyname(buf);
	
	/*
	 * Do something based on whether this is an arithmetic operator
	 * or whitespace.
	 */
	if (ws) {
		*ws = w;
		if (isoperator(w))
			result += convexpr(ws);
	}
	
	return result;
}

/*
 * This routine converts arithmetic expressions (no symbols!) into
 * an integer value.
 */
__psunsigned_t
convexpr(char *ptr)
{
	__psunsigned_t result = 0;
	char *val;
	__psunsigned_t value;
	char w;
	char op;

	while (*ptr) {
		if (!isoperator(*ptr))
			return result;
		op = *ptr++;
		val = ptr;
		while (*ptr && !isspace(*ptr) && !isoperator(*ptr))
			ptr++;
		w = *ptr;
		*ptr = 0;
		value = strtoul(val, (char **)NULL, 0);
		switch (op) {
		case '+':
			result += value;
			break;
		case '-':
			result -= value;
			break;
		default:
			return result;
		}
		*ptr = w;
		if (!w)
			return result;
		if (isoperator(*ptr)) {
			ptr++;
		} else {
			while (isspace(*ptr))
				ptr++;
		}
	}
	
	return result;
}

/*
 * Load the symbol table
 */
int
loadsym(void)
{
	int fd[2];
	pid_t child;
	FILE *fp;
	struct stat sb;
	
	/*
	 * Preload the address of main -- this will allow us to
	 * determine if the kernel has been loaded.
	 */
	(void)lookupsymbyname("main");
	
	/*
	 * Need to check for presence of 'nm'.
	 */
	if (stat(NM_PATH, &sb) < 0) {
		fprintf(stderr, "%s is required for regex symbol generation\n",
			NM_PATH);
		perror(NM_PATH);
		loadedallsyms = 1;
		return 1;
	}

	/*
	 * Create the pipe between us and nm
	 */	
	if (pipe(fd) == -1) {
		perror("loading symbol table: pipe");
		return 0;
	}
	
	/*
	 * Start the child
	 */
	child = fork();

	if (!child) {
		/*
		 * This is the child.
		 */
		dup2(fd[1], 1);
		dup2(fd[1], 2);

		/*
		 * Exec the nm program
		 */
		if (execl(NM_PATH, NM_PATH, "-Bn", filename, 0) == -1) {
			perror("nm: exec");
			exit(1);
		}
		exit(0);
		/* no return */
	}

	/*
	 * In the parent, open the pipe for stdio
	 */	
	close(fd[1]);
	fp = fdopen(fd[0], "r");
	if (fp) {
		__psunsigned_t addr;
		char type;
		char *symname = malloc(1024);
		symelem_t *elem;
		
		/*
		 * Read elements out of the table.  Load the symbol
		 * table.
		 */
		while (fscanf(fp, "%lx %c %s",
					&addr, &type, symname) != EOF) {
			if (!addr)
				continue;
			elem = calloc(1, sizeof(symelem_t));
			elem->sym_name = strdup(symname);
			elem->sym_value = addr;
			insertsym(elem);		
		}
		free(symname);
	}

	while (wait(0) != child) {
	}
	
	close(fd[0]);	

	loadedallsyms = 1;
	return 1;
}

/*
 * Insert into the symbol element table.
 *
 * This algorithm is very slow -- can be improved.
 */
void
insertsym(symelem_t *sym)
{
	symelem_t *nsym;
	symelem_t *osym;
	int value;

	if (!symelem_aq) {
		symelem_aq = symelem_nq = sym;
		return;
	}
	
	for (nsym = symelem_nq, osym = 0; nsym; nsym = nsym->sym_nnext) {
		value = strcmp(nsym->sym_name, sym->sym_name);
			
		/*
		 * We already know about this symbol
		 */
		if (value == 0) {
			free(sym->sym_name);
			free(sym);
			return;
		}

		/*
		 * Else, break when we've found it in order.
		 */
		if (value > 0)
			break;
		osym = nsym;
	}
	
	if (osym) {
		sym->sym_nnext = osym->sym_nnext;
		osym->sym_nnext = sym;
	} else {
		sym->sym_nnext = symelem_nq;
		symelem_nq = sym;
	} 

	for (nsym = symelem_aq, osym = 0; nsym; nsym = nsym->sym_anext) {
		if ((unsigned)nsym->sym_value > (unsigned)sym->sym_value)
			break;
		osym = nsym;
	}
	
	if (osym) {
		sym->sym_anext = osym->sym_anext;
		osym->sym_anext = sym;
	} else {
		sym->sym_anext = symelem_aq->sym_anext;
		symelem_aq = sym;
	} 
}

/*
 * Lookup a symbol table element from the kernel
 */
__psunsigned_t
lookupsymbyname(char *buf)
{
	struct idbgcomm ic;
	dbgoff_t *dp;
	long len;
	__psunsigned_t addr;
	static __psint_t kaddrcmd = -1;
	char *symname;
	cap_t ocap;

	/*
	 * Go to kernel to get symbol name
	 */
	if (kaddrcmd == (__psint_t) -1) {
		int found = 0;
		for (dp = db; !found && dp->s_type != DO_END; dp++) {
			if ((strcmp(dp->us_name, KADDR)) == 0) {
				kaddrcmd = (int)(__psint_t)(dp - db);
				found = 1;
				
			}
		}
		if (!found) {
			fprintf(stderr,
				"Kernel idbg doesn't support symbol lookup\n");
			return 0;
		}
	}
	
	symname = strdup(buf);
	
	iargs[0].addr = buf;
	iargs[0].len = (unsigned int)(strlen(buf) + 1);

	ic.i_func = kaddrcmd;
	ic.i_arg = -1;
	ic.i_argcnt = 1;
	ic.i_argp = (caddr_t)iargs;
	ic.i_uaddr = uaddr;
	ic.i_ulen = RSZ;
	ic.i_cause = 0;
	ic.i_badvaddr = 0;

	ocap = cap_acquire(1, cap_sysinfo_mgt);
	if ((len = syssgi(SGI_IDBG, 1, &ic)) < 0) {
		perror("idbg function failed");
		cap_surrender(ocap);
		free(symname);
		return 0;
	}
	cap_surrender(ocap);

	ic.i_uaddr[len] = 0;
	addr = strtoul(ic.i_uaddr, (char **)NULL, 0);
	
	if (addr && !loadedallsyms) {
		symelem_t *elem = calloc(1, sizeof(symelem_t));
		elem->sym_name = symname;
		elem->sym_value = addr;
		insertsym(elem);
	}

	return addr;
}


/*
 * Support for builtin commands.
 */

int
chkparsetype(char *buf)
{
	struct builtincomm *bp;
	
	for (bp = &BuiltInComm[0]; bp->comm_name; bp++) {
		if (!strcmp(bp->comm_name, buf)) {
			return bp->comm_args;
		}
	}
	
	for(bp = &SpecialComm[0]; bp->comm_name; bp++) {
		if (!strcmp(bp->comm_name, buf)) {
			return bp->comm_args;
		}
	}
	return INTERPRET_ARGS;
}

int
builtin(struct idbgcomm *arg)
{
	struct builtincomm *bp;
	
	for (bp = &BuiltInComm[0]; bp->comm_name; bp++) {
		if (!strcmp(bp->comm_name, (char *)arg->i_func))
			return (*bp->comm_func)(arg);
	}
	return 0;
}

/*
 * Return decimal if hex, hex if decimal
 */
int
_dec(struct idbgcomm *arg)
{
	unsigned long value;
	
	if (arg->i_argcnt > 0 || arg->i_arg == (__psint_t)-1) {
		fprintf(stderr, "Arg count incorrect\n");
		return -1;
	}

	value = strtoul((char *)arg->i_arg, (char **)NULL, 16);
	fprintf(stderr, "%s (hex) ==> %d (decimal)\n", arg->i_arg, value);
	return 1;
}

int
_hex(struct idbgcomm *arg)
{
	unsigned long value;
	
	if (arg->i_argcnt > 0 || arg->i_arg == (__psint_t)-1) {
		fprintf(stderr, "Arg count incorrect\n");
		return -1;
	}

	value = strtoul((char *)arg->i_arg, (char **)NULL, 10);
	fprintf(stderr, "%s (decimal) ==> %x (hex)\n", arg->i_arg, value);
	return 1;
}


/*
 * Given a regular expression, give some possibilities for its name.
 */
int
_what(struct idbgcomm *arg)
{
	symelem_t *sym;
	char buf[2048];
	char *regexpr;
	
	if (arg->i_argcnt > 0 || arg->i_arg == (__psint_t)-1) {
		fprintf(stderr, "Arg count incorrect\n");
		return -1;
	}
	
	if (!isRegEx((char *)arg->i_arg)) {
		__psunsigned_t result = lookupsymbyname((char *)arg->i_arg);
		if (result) {
			fprintf(stderr,
				"%-24.24s " ADDR_FMT "\n", (char *)arg->i_arg,
				result);
		} else {
			fprintf(stderr,
				"Symbol <%s> not found\n", (char *)arg->i_arg);
		}
		return 1;
	}

	LOADSYM(-1);
	
	if (useregexflag)
		strcpy(buf, (char *)arg->i_arg);
	else
		glob2regex(buf, (char *)arg->i_arg);
	
	regexpr = re_comp(buf);
	
	if (regexpr) {
		fprintf(stderr, "%s\n", regexpr);
		return -1;
	}
	
	for (sym = symelem_nq; sym; sym = sym->sym_nnext) {
		if (re_exec(sym->sym_name) == 1) {
			/* XXX Should be sized by architecture */
			fprintf(stderr, "%-24.24s " ADDR_FMT "\n",
				sym->sym_name, sym->sym_value);
		}
	}
	
	free(regexpr);
	return 1;
}

/*
 * Given a regular expression, give some possibilities for its name.
 */
/* ARGSUSED */
int
_quit(struct idbgcomm *arg)
{
	exit(0);
	return 0;
}

/*
 * Regular expression functions.
 */
/*
**  NAME
**	glob2regex - convert a shell-style glob expr to a regular expr
**
**  SYNOPSIS
*/

char *
glob2regex(char* out, const char* in)

/*
**  DESCRIPTION
**	glob2regex converts the glob expression character string 'in' to a
**	the regex(3X) style regular expression.  This conversion permits the
**	use of a single expression matching algorithm yet allows globbing
**	style input.  The globbing style is similar to csh(1) globbing style
**	with the exception that curly brace '{' or'ing notation is not
**	supported.  Also ~ is ignored as it only applies to path names.
**
**	A pointer to 'out' is returned as a matter of convenience as is the
**	style for other string(3) routines.
**
**  SEE ALSO
**	csh(1), regex(3X), string(3).
**
**  AUTHOR
**	Dave Ciemiewicz (Ciemo)
**
**  COPYRIGHT (C) Silicon Graphics, 1991, All rights reserved.
**
*/

{
    const char* i;
    
    *out++ = '^';

    for(i=in; *i; i++) {
	switch (*i) {
	/*
	** Ignore certain regex meta characters which no correspondence in
	** globbing.
	*/
	case '^':
	case '$':
	case '+':
	case '.':
	case '(':
	case ')':
	case '{':	/* Curly brace or'ing notation is not supported */
	case '}':
	    *out = '\\'; out++;
	    *out = *i; out++;
	    break;

	/*
	** Map single character match
	*/
	case '?':
	    *out = '.'; out++;	
	    break;

	/*
	** Map any string of characters match
	*/
	case '*':
	    *out = '.'; out++;	
	    *out = '*'; out++;	
	    break;

	/*
	** All other characters are treated as is.  This fall through also
	** supports the square bracket '[' match character set notation which
	** is the same for both globbing and regex.
	*/
	default:
	    *out = *i; out++;
	}
    }
    *out++ = '$';
    *out = *i;
    return 0;
}

static unsigned int
isRegEx(const char * str)
{
    static char * regexpchars = ".+*^$(){}[]";
    return (strcspn(str,regexpchars) != strlen(str));
}
