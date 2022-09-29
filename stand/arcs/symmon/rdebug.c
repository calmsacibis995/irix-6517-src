/*
 * rdebug.c -- remote debugging facilities
 */

#include <arcs/io.h>

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/sbd.h>
#include <sys/signal.h>

#include <setjmp.h>
#include <stringlist.h>
#include <fault.h>
#include "protocol.h"
#include "saioctl.h"
#include "dbgmon.h"
#include "mp.h"
#ifdef NETDBX
#ifdef EVEREST
#include <sys/EVEREST/IP19addrs.h>
#endif /* EVEREST */
#include <sys/EVEREST/evmp.h>
#include <net/in.h>
#include "coord.h"
#endif /* NETDBX */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>

#define WSTOPFLG		0177

/*
 * debug protocol fields for ptrace packets
 */
#define	PID_FIELD	0	/* process id field of ptrace pkt */
#define	REQ_FIELD	1	/* request field of ptrace pkt */
#define	ADDR_FIELD	2	/* address field of ptrace pkt */
#define	DATA_FIELD	3	/* data field of ptrace pkt */

/*
 * debug protocol fields for boot packets
 */
#define	PROG_FIELD	2	/* program name field of boot pkt */
#define	ARGS_FIELD	3	/* first argument field of boot pkt */

static char *btoh(register unsigned long long);
static void error_reply(ULONG, struct string_list *);
static void g_reply(ULONG, struct string_list *);
static int mk_status(unsigned, unsigned);
static void append(char *, char *);
static int get_mapreg(int);

static void send_reply(ULONG, struct string_list *, 
		unsigned long long, unsigned long long);

#ifdef NETDBX
static void m_reply(ULONG, struct string_list *, numinp_t, unsigned long);
static void status_reply(ULONG, struct string_list *);

static struct sockaddr_in saddr;

volatile int	nwkopen = 0;
volatile int	netrdebug = 0;
volatile int 	singlestepping = 0;
volatile int 	kprinting = 0;
volatile int 	inrdebug = 0;
volatile int 	shouldexit = 0;

#ifdef DEBUG
static 	 int 	debug_comm = 0;
#define dprintf(x,y)  if (debug_comm >= (x)) printf y
#else
#define dprintf(x,y)
#endif

#ifdef MULTIPROCESSOR
#define BOOTSTART \
    setcpustate(cpuid(),OutOfSymmon);				\
    invoke((inst_t *) private.regs[R_EPC], /* entry pc */ 	\
	    private.regs[R_A0], /* argc */			\
	    private.regs[R_A1], /* argv */			\
	    private.regs[R_A2], /* environ */			\
	    private.regs[R_A3], /* entry pc, again */		\
	    0,0,0,0);
#else
#define BOOTSTART \
    invoke((inst_t *) private.regs[R_EPC], /* entry pc */ 	\
	    private.regs[R_A0], /* argc */			\
	    private.regs[R_A1], /* argv */			\
	    private.regs[R_A2], /* environ */			\
	    private.regs[R_A3], /* entry pc, again */		\
	    0,0,0,0);
#endif

#ifdef SN0
#include <pgdrv.h>

#ifdef SN_PDI
static char *NetworkName = "network(0)" ;
#else
static char *NetworkName = 
"/xwidget/master_bridge/pci/master_ioc3/ef/" ;
#endif
#else
static char *NetworkName = "network(0)";
#endif

ULONG
nwksetup(struct sockaddr_in *source)
{
	ULONG 	err;
	ULONG 	fd, cc;
	char	buf[20];

	dprintf(3, ("Opening %s ...\n", NetworkName));
	if  ((err = Open(NetworkName, OpenReadWrite, &fd)) != ESUCCESS) {
		printf("Error in opening file %s (err = %d)\n", NetworkName, err);
		return(err);
	}
	/*
	 * 3152 is an arbitrary port number.
	 */
	dprintf(3, ("Open successful, attemting bind ...\n"));
	if  ((err = Bind(fd, (u_short) 3152)) != ESUCCESS) {
		printf("Error in binding (err = %d)\n",err);
		return(err);
	}
	dprintf(3, ("Bind successful, attempting ioctl ...\n"));
	if  ((err = ioctl(fd, NIOCREADANY, 1)) != ESUCCESS) {
		printf("Error in ioctl (NIOCREADANY) (err = %d)\n",err);
		return((int) err);
	}
	dprintf(3, ("Ioctl (NIOCREADANY) successful\n"));
	if (source == NULL) {
		/* first time around, have to figure out who
		 * to talk to.
		 */
		printf("Waiting for initiation from kdbx ...\n");
		if ((err = Read(fd, buf, 20, &cc)) != ESUCCESS) {
			printf("Error in reading file (err = %d)\n", err);
			return(err);
		}
		if (cc > (ULONG) 0)
			buf[cc] = NULL;
		dprintf(3, ("%s", buf));
		dprintf(3, ("Attempting to Get source address...\n"));
		GetSource(fd, &saddr);
		dprintf(3, ("Got source address...\n"));
		source = &saddr;
		printf("Established connection with kdbx ...\n");
	}
	BindSource(fd, *source);
	dprintf(3, ("Source is bound.\n"));
	nwkopen = 1;
	return(fd);
}

/*
 * We know that packets coming in are small, so, use a smaller buffer
 */
#define GETPACKETSIZE	MAXPACKET

static struct string_list 	fields;
static ULONG 			fd;
static int			booting = 1;
static char 			linebuf[GETPACKETSIZE];

extern unsigned 		_client_sp;
extern struct 			cmd_table ctbl[]; /* generic command table */
#endif /* NETDBX */

/*
 * rdebug -- remote debugging interface command
 */
/*ARGSUSED*/
int
_rdebug(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
#ifndef NETDBX
	struct string_list fields;
	char 		linebuf[MAXPACKET];
	ULONG		fd;
#endif /* !NETDBX */
	numinp_t 	wp;
	long long 	val, oval;
	int 		pkt_type;
	jmp_buf 	dbg_buf;
	int 		reg, fcnt, len;
	volatile int 	restart = 0;
	int 		Restart = 0;
	LONG 		error;


#ifdef NETDBX
	int 		cpu2continue = NO_CPUID;
#ifdef MULTIPROCESSOR
	extern 	int _get_numcpus(void);

	if (booting)
#ifndef SN0
		ncpus = _get_numcpus();
#else
		ncpus = 4;
#endif

	dprintf(3, ("SymmonStack = 0x%x (till 0x%x)\n", SYMMON_STACK, SYMMON_STACK - SYMMON_STACK_SIZE));
#else
	ncpus = 1;
#endif

	if (booting)
		printf("Cpu %d, no. of cpus = %d\n", cpuid(), ncpus);

	dprintf(3, ("Cpu %d, stack = 0x%x\n", cpuid(), &cpu2continue));

	argv++; argc--;
	if (argc >= 2 && strcmp(*argv, "-r") == 0) {
		argv++; argc--;
		if (strcmp(*argv, "netrdebug") == 0) {
			int i;

			printf("Remote Debugging over Ethernet\n");
			argv++; argc--;
			while (argc > 0) {
				char *var;
				char *val;

				var = *argv;
				if ((val = index(var, '=')) == NULL) {
					printf("ERROR: Malformatted environment variable <%s>; should be of the form <var=val>\n", *argv);
					return 1;
				}
				*val++ = NULL; /* terminate var */
				printf("setenv(%s,%s)\n", var, val);
				setenv(var, val);
				argv++; argc--;
			}
			printf("Initiating Entry Protocol.\n");
			netrdebug = 1;
			shouldexit = 0;
#ifdef MULTIPROCESSOR
			i = splockspl(symmon_owner_lock, splhi);
			symmon_owner = NO_CPUID;
			spunlockspl(symmon_owner_lock, i);
			private.bp_nofault = (jmpbufptr) 0xdeadbeefL; /* junk */
#else
			private.bp_nofault = (jmpbufptr) 0xdeadbeef; /* junk */
#endif
			enter_rdebug_loop(0); /* 0 == don't call rdebug loop */
		}
		booting = 0;
		restart++;	/* restarting after ^Z */
	}
	inrdebug = 1;
	if (argc == 2 && strcmp(*argv, "-R") == 0) {
		argv++; argc--;
		Restart++;
	}
	if (!netrdebug && argc != 1)
		return(1);

	if (!netrdebug) {
		/*
		 * open char device that is to be used as comm path
		 * with debugger
		 */
		if ((error = Open((CHAR *)*argv, OpenReadWrite, &fd)) != ESUCCESS) {
			printf("can't open %s (error %d)\n", *argv, error);
			return(0);
		}
		close_on_exception(fd);

		/*
		 * set-up comm path to speak protocol
		 */
		proto_enable(fd);
	} else {
#ifdef MULTIPROCESSOR
		if (Restart) {
			dprintf(3, ("rdebug: Restart\n"));
			if (nwkopen) {
				printf("Cpu %d, Hmm.. found nwk open on Restart\n", cpuid());
				Close(fd);
				nwkopen = 0;
			}
			fd = nwksetup(&saddr);
			if (singlestepping) {
			    _argvize("0x1 s 0x0", &fields);
			    singlestepping = 0;
			} else
			    _argvize("0x1 c junk", &fields);
			symmon_spl();
			error_reply(fd, &fields);
			dprintf(3, ("rdebug: error reply done\n"));
		} else
#endif
			fd = nwksetup(NULL);
	}

#ifdef MULTIPROCESSOR
#ifdef EVEREST
	if (booting)
		printf("MPCONF[0] @ 0x%x, MPCONF[1] @ 0x%x ...\n", &MPCONF[0], &MPCONF[1]);
#endif
#endif

	/*
	 * send a reply to indicate that boot was successful
	 * and we're in debug mode
	 */
	if ((!netrdebug && !restart) || (netrdebug && !Restart)) {
		printf("rdebug: doing boot stuff\n");
		if (netrdebug)
			netinit_proto(fd);
		else
			init_proto(fd);
		if (restart) /* counter the effect of normal symmon */
			private.regs[R_EPC] -= 4;
			
		_argvize("0x1 b junk", &fields);
		if (netrdebug)
		    send_reply(fd, &fields, ncpus, mk_status(SIGTRAP,WSTOPFLG));
		else
		    send_reply(fd, &fields, 0,mk_status(SIGTRAP, WSTOPFLG));
	}

#else /* NETDBX */

	argv++; argc--;
	if (argc == 2 && strcmp(*argv, "-r") == 0) {
		argv++; argc--;
		restart++;      /* restarting after ^Z */
	}
	if (argc != 1)
		return(1);

	/*
	 * open char device that is to be used as comm path
	 * with debugger
	 */
	if ((error = Open((CHAR *)*argv, OpenReadWrite, &fd)) != ESUCCESS) {
		printf("can't open %s (error %d)\n", *argv, error);
		return(0);
	}
	close_on_exception(fd);

	/*
	 * set-up comm path to speak protocol
	 */
	proto_enable(fd);

	/*
	 * send a reply to indicate that boot was successful
	 * and we're in debug mode
	 */
	if (!restart) {
		init_proto(fd);
		_argvize("0x1 b junk", &fields);
		send_reply(fd, &fields, 0, mk_status(SIGTRAP, WSTOPFLG));
	}

#endif /* NETDBX */

	for (;;) {
		pkt_type = DATA_PKTTYPE;
#ifdef NETDBX
		if (netrdebug)
			len = netgetpkt(fd, linebuf, GETPACKETSIZE, &pkt_type);
		else
			len = getpkt(fd, linebuf, GETPACKETSIZE, &pkt_type);
#else /* NETDBX */
		len = getpkt(fd, linebuf, MAXPACKET, &pkt_type);
#endif /* NETDBX */
		if (len < 0) {
			printf("ABNORMAL PROTOCOL TERMINATION");
			goto done;
		}

		/*
		 * DEBUG PROTOCOL DEFINITION
		 *
		 * Debug protocol incoming packets are of two formats:
		 *
		 * PTRACE PACKETS:
		 *
		 *	PID	REQUEST	ADDR	DATA
		 *	0x%x	%c	0x%x	0x%x
		 *
		 * where the fields correspond to the definitions in
		 * ptrace(2) with the exception that requests are specified
		 * as single characters rather than integers.  All fields
		 * are separated by one or more blanks.  Currently, the
		 * following requests are accepted:
		 *
		 *	char	ptrace#		operation
		 *
		 *	i	1		read i-space
		 *	d	2		read d-space
		 *	r	3		read registers
		 *	I	4		write i-space
		 *	D	5		write d-space
		 *	R	6		write registers
		 *	c	7		continue
		 *	x	8		terminate
		 *	s	9		single step
		 *	g	10		get multiple registers
		 *	f	11		get multiple float-pt registers
		 *	G	12		set multiple gp registers
		 *	F	13		set multiple float-pt registers
		 *
		 * Use of addr and data is described below for each
		 * command.
		 *
		 * BOOT PACKETS:
		 *
		 * The other class of request is the boot (b) request
		 * which is formatted:
		 *
		 *	PID	REQUEST	PROGNAME	ARGS
		 *	0x%x	b	%s		%s
		 *
		 * where:
		 *	PROGNAME is the program to boot (with the path
		 *	specified appropriately for the standalone
		 *	environment) and ARGS are a list of strings
		 *	which should be passed to the program being
		 *	debugged as its argv.
		 */
		linebuf[len] = 0;
		if ((fcnt = _argvize(linebuf, &fields)) < 3) {
			send_reply(fd, &fields, (unsigned long long)-1, EINVAL);
			break;
		}
#ifdef	DEBUG
#ifdef NETDBX
		if (debug_comm >= 2)
#endif /* NETDBX */
		{
			int i;

			printf("RECV:");
			for (i = 0; i < fcnt; i++)
				printf(" %s", fields.strptrs[i]);
			printf("\n");
		}
#endif	/* DEBUG */

		if (setjmp(dbg_buf)) {
#ifdef NETDBX
			dprintf(3, ("(cpu %d) Long jumped after resuming from brkpt\n", cpuid()));
			if (netrdebug) {
#ifdef MULTIPROCESSOR
			    inrdebug = 1;
#endif
			    if (nwkopen) {
				
				if (((*fields.strptrs[REQ_FIELD]) != 'd') &&
				      ((*fields.strptrs[REQ_FIELD]) != 'm'))
				    printf("*** Hmm. found nwk open on <%c>\n",
					(*fields.strptrs[REQ_FIELD]));
				Close(fd);
				nwkopen = 0;
			    }
			    fd = nwksetup(&saddr);
			    dprintf(3, ("(cpu %d) fields=0x%x\n",cpuid(),&fields));
			}
#endif /* NETDBX */
			symmon_spl();
			error_reply(fd, &fields);
			continue;
		}

		GetReadStatus(fd);
		private.pda_nofault = dbg_buf;	/* in case of trouble */
		private.bp_nofault = dbg_buf;
		switch (*fields.strptrs[REQ_FIELD]) {

#ifdef notdef
		/*
		 * Currently this is only useful for booting programs
		 * via the ethernet (most cases actually!) or from volume
		 * headers or from tpd tapes.   (The prom doesn't
		 * know about 4.2BSD or SysV file systems.)
		 *
		 * I suppose we could include file system code with
		 * the dbgmon, but that would make it huge!
		 * Another possibility would be to go through a two
		 * level boot process.
		 */
		case 'b':
			pa.pa_bootfile = fields.strptrs[PROG_FIELD];
			pa.pa_argc = fcnt - ARGS_FIELD;
			pa.pa_argv = &fields.strptrs[ARGS_FIELD];
			pa.pa_environ = environ;
			pa.pa_flags = EXEC_NOGO;
			if ((pc = promexec(&pa)) == -1)
				send_reply(fd, &fields,
					(unsigned long long)-1, ENOENT);
			else {
				send_reply(fd, &fields, 0,
				    mk_status(SIGTRAP, WSTOPFLG));
				_regs[R_SP] = _client_sp;
				_regs[R_EPC] = pc;
				_regs[R_A0] = 0;	/* no args, sorry */
				_regs[R_A1] = 0;
				_regs[R_A2] = (unsigned)environ;
			}
			break;
#endif
#ifdef NETDBX
		case 'k':
			printf("(cpu %d), request for kp <%s>\n",
				cpuid(), fields.strptrs[ADDR_FIELD]);
#ifdef MULTIPROCESSOR
			if (strcmp(fields.strptrs[ADDR_FIELD], "cpu") == 0) {
				status_reply(fd, &fields);
			} else if (strcmp(fields.strptrs[ADDR_FIELD], "nwkdown") == 0) {
				keepnwkdown = 1;
				send_reply(fd, &fields, 0, 0);
			} else 
#endif /* MULTIPROCESSOR */
#ifdef DEBUG
			if (strcmp(fields.strptrs[ADDR_FIELD], "debug_comm") == 0) {
				debug_comm = (fcnt == 4) ? atoi(fields.strptrs[DATA_FIELD]) : 2;
				send_reply(fd, &fields, 0, 0);
			} else
#endif /* DEBUG */
			{
				static char tmpbuf[64];

				kprinting = 1; /* to stop nwk from resuming */
				sprintf(tmpbuf, "%s %s", fields.strptrs[ADDR_FIELD], (fcnt == 4) ? fields.strptrs[DATA_FIELD] : "");
				printf("execute_command with <%s>\n", tmpbuf);
				execute_command(ctbl, tmpbuf);
				send_reply(fd, &fields, 0, 0);
				kprinting = 0;
			}
			break;
		case 'm': {
			/* read memory, N 4-byte words */
			if (*atob_s(fields.strptrs[ADDR_FIELD], &wp)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			if (fcnt<4 || *atob_L(fields.strptrs[DATA_FIELD],&val)){
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			m_reply(fd, &fields, wp, val); /* val is nwords */
			break;
		}
#endif /* NETDBX */
		case 'i':	/* read i-space word */
		case 'd':	/* read d-space word */
			if (*atob_s(fields.strptrs[ADDR_FIELD], &wp)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			val = GET_MEMORY(wp, SW_WORD);
			send_reply(fd, &fields, 0, val);
			break;

		case 'D':	/* write d-space word */
			if (*atob_s(fields.strptrs[ADDR_FIELD], &wp)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			if (fcnt<4 || *atob_L(fields.strptrs[DATA_FIELD], &val)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			oval = GET_MEMORY(wp, SW_WORD);
			SET_MEMORY(wp, SW_WORD, val);
			send_reply(fd, &fields, 0, oval);
			break;

		case 'I':	/* write i-space word */
			{
#define	BTTYPE_PERM	4		/* permanent breakpoint */
			extern int _kernel_bp[];

			if (*atob_s(fields.strptrs[ADDR_FIELD], &wp)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			if (fcnt<4 || *atob_L(fields.strptrs[DATA_FIELD], &val)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			oval = GET_MEMORY(wp, SW_WORD);
			if (oval == *_kernel_bp && val == *_kernel_bp) {
				printf("overwriting bp with bp - ignored\n");
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
			} else if (val == *_kernel_bp) {
				cpumask_t allcpus;

				/* setting a new breakpoint */
				/* so other dbgmon can handle it -
				 * set a real brkpt
				 */
				CPUMASK_CLRALL( allcpus );
				CPUMASK_CPYNOTM( allcpus, allcpus );
				NEW_BRKPT(wp, BTTYPE_PERM, allcpus);
			} else if (oval == *_kernel_bp) {
				/* deleting breakpoint (ignore what dbx sent) */
				OLD_BRKPT(wp);
			} else {
				/* just a plain old inst mod! */
				SET_MEMORY(wp, SW_WORD, val);
			}
			send_reply(fd, &fields, 0, oval);
			break;
			}

		case 'r':	/* read register */
			if (*atob(fields.strptrs[ADDR_FIELD], &reg) ||
			    (reg = get_mapreg(reg)) < 0) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			val = (long long)_get_register(reg);
			send_reply(fd, &fields, 0, val);
			break;

		case 'R':	/* write register */
			if (*atob(fields.strptrs[ADDR_FIELD], &reg) ||
			    (reg = get_mapreg(reg)) < 0) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			if (fcnt<4 || *atob_L(fields.strptrs[DATA_FIELD], &val)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			oval = (long long)_get_register(reg);
			_set_register(reg, val);
			send_reply(fd, &fields, 0, oval);
			break;
		
		case 'g':
			g_reply(fd, &fields);
			break;

		case 'c':	/* continue execution */
			if (fcnt<4 || *atob_L(fields.strptrs[ADDR_FIELD], &val)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}

			if (val != 1)
				private.regs[R_EPC] = val;
			private.dbg_modesav = MODE_CLIENT;/* force back to client mode */

#ifdef NETDBX
			dprintf(2, ("============================================================================\n"));
			if (netrdebug) {
				cpu2continue = atoi(fields.strptrs[PID_FIELD]);
				strcpy(fields.strptrs[PID_FIELD],"0x1"); /*FIX*/
				Close(fd);
				nwkopen = 0;
			        dprintf(3, ("rdebug: cpu2continue = %d\n", cpu2continue));
				if (restart) {
				    /*
				     * Go to beginning of loop, as though
				     * cpu just hit a break point, and send
				     * an error reply.
				     */

				    restart = 0;
				    _pdbx_clean(1); /* 1==clean PERM brkpts */
				    private.dbg_mode = MODE_DBGMON;
				    printf("restart: longjmp to start\n");
				    longjmp(private.bp_nofault, 1);
				    /* NOT REACHED */
				}
				if (cpu2continue == -3) {
					cpu2continue = -1;
					shouldexit = 1;
				} else if (cpu2continue == -4) {
					shouldexit = 2;
					leave_rdebug_loop(NO_CPUID);
					goto done;
				}
				dprintf(3, ("(cpu %d), jbuf sp=0x%x (@0x%x)\n", cpuid(), dbg_buf[JB_SP], &dbg_buf[0]));
#ifdef MULTIPROCESSOR
				inrdebug = 0;
				if (booting) {
					booting = 0;
					leave_rdebug_loop(-2);
					dprintf(3, ("(cpu %d), jbuf sp=0x%x (@0x%x)\n", cpuid(), dbg_buf[JB_SP], &dbg_buf[0]));
					BOOTSTART;
				} else {
					dprintf(3, ("rdebug: cpu2continue = 0x%x\n", cpu2continue));
					leave_rdebug_loop(cpu2continue);
				}
#else
				if (booting) {
					booting = 0;
					BOOTSTART;
				}
#endif
			} /* netrdebug */
			dprintf(3, ("rdebug: resuming at brkpt = 0x%x\n", private.regs[R_EPC]));
#endif /* NETDBX */
			_resume_brkpt();
			break;

		case 's':
			if (fcnt<4 || *atob_L(fields.strptrs[ADDR_FIELD], &val)) {
				send_reply(fd, &fields,
					(unsigned long long)-1, EINVAL);
				break;
			}
			if (val != 1)
				private.regs[R_EPC] = val;
#ifdef NETDBX
			dprintf(2, ("----------------------------------------------------------------------------\n"));
			if (netrdebug) {
			    Close(fd);
			    nwkopen = 0;
#ifdef MULTIPROCESSOR
			    dprintf(3, ("'s': fields @ 0x%x\n", &fields));
			    inrdebug = 0;
#endif 
			    dprintf(3, ("cpu %d, turn on singlestep\n", cpuid()));
			    singlestepping = 1;
			} /* netrdebug */
#endif /* NETDBX */
			single_step();
			break;

		case 'x':	/* exit debug mode */
			{
			char *xargv = "_quit";
			send_reply(fd, &fields, 0, 0);
			printf("Exiting debug monitor\n");
			private.pda_nofault = 0;
			private.bp_nofault = 0;
			proto_disable(fd);
			Close(fd);
#ifdef NETDBX
			if (netrdebug) nwkopen = 0;
#endif /* NETDBX */
			_quit(1, &xargv,0,0);
			}

		default:
			send_reply(fd, &fields,
				(unsigned long long)-1, EINVAL);
			break;
		}
		private.pda_nofault = 0;
		private.bp_nofault = 0;
	}
done:
	private.pda_nofault = 0;
	private.bp_nofault = 0;
	proto_disable(fd);
	Close(fd);
#ifdef NETDBX
	if (netrdebug) {
		int i, count;

		nwkopen = 0;
#ifdef MULTIPROCESSOR
		inrdebug = 0;
		printf("Cpu %d, waiting for all cpus to sync.\n", cpuid());

		count = 0;
		while (!waitedtoolong(&count,100000)) {
		    for (i = 0; i < ncpus; i++)
			if (getcpustate(i) == EnteredSymmon) break;
		    if (i == ncpus)
			break;
		}
		if (i != ncpus)
			printf("Warning! Synced with only %d cpus\n", i);
		else
			printf("Cpu %d, all cpus are in sync.\n", cpuid());
#endif
		netrdebug = shouldexit = 0;
#ifdef MULTIPROCESSOR
		i = splockspl(symmon_owner_lock, splhi);
		symmon_owner = NO_CPUID;
		spunlockspl(symmon_owner_lock, i);
		setcpustate(cpuid(),OutOfSymmon);
#endif
	}
#endif /* NETDBX */
	return(0);
}

/*
 * error_reply -- send appropriate remote debugging reply after an
 * exception
 */
static void
error_reply(ULONG fd, struct string_list *fieldp)
{

#ifdef NETDBX
	dprintf(3, ("error_reply: dbg_exc = 0x%x, cause = 0x%x\n", private.dbg_exc,
		((int)private.exc_stack[E_CAUSE] & CAUSE_EXCMASK)));
#endif /* NETDBX */
	switch (*fieldp->strptrs[1]) {
	case 'b':
		send_reply(fd, fieldp, (unsigned long long)-1, ENOENT);
		break;

	case 's':
	case 'c':
		switch (private.dbg_exc) {
		case EXCEPT_NORM:
			switch ((__psint_t)private.exc_stack[E_CAUSE] & CAUSE_EXCMASK) {
			case EXC_INT:
				send_reply(fd, fieldp, 0,
				    mk_status(SIGINT, WSTOPFLG));
				break;

			case EXC_MOD:
			case EXC_RMISS:
			case EXC_WMISS:
				send_reply(fd, fieldp, 0,
					mk_status(SIGSEGV, WSTOPFLG));
				break;

			case EXC_RADE:
			case EXC_WADE:
			case EXC_IBE:
			case EXC_DBE:
				send_reply(fd, fieldp, 0,
				    mk_status(SIGBUS, WSTOPFLG));
				break;

			case EXC_SYSCALL:
			case EXC_II:
			case EXC_CPU:
				send_reply(fd, fieldp, 0,
				    mk_status(SIGILL, WSTOPFLG));
				break;

			case EXC_OV:
				send_reply(fd, fieldp, 0,
				    mk_status(SIGFPE, WSTOPFLG));
				break;

			case EXC_BREAK:
				send_reply(fd, fieldp, 0,
				    mk_status(SIGTRAP, WSTOPFLG));
				break;

			default:
				send_reply(fd, fieldp, 0,
				    mk_status(SIGTERM, WSTOPFLG));
				break;
			}
			break;

		case EXCEPT_UTLB:
			send_reply(fd, fieldp, 0,
			    mk_status(SIGSEGV, WSTOPFLG));
			break;

		case EXCEPT_BRKPT:
#ifdef NETDBX
			if (IS_KSEGDM(private.regs[R_A0])) {
				jmp_buf 	a0_buf;
				jmpbufptr	oldnf;

				if (setjmp(a0_buf)) {
					printf("error_reply: Can't determine if 'ring' brkpt@ pc=0x%x, a0=0x%x\n", private.regs[R_EPC], private.regs[R_A0]);
					private.pda_nofault = oldnf;
					send_reply(fd, fieldp, 0,
						mk_status(SIGTRAP, WSTOPFLG));
				} else {
					oldnf = private.pda_nofault;
					private.pda_nofault = a0_buf;
					if (strncmp((char *)private.regs[R_A0],
						"ring",4) == 0) {
						private.pda_nofault=oldnf;
						/* Do an implicit step over the brk inst. */
#ifdef MULTIPROCESSOR
						if (!netrdebug || getcpustate(cpuid()) == EnteredSymmon)
#else
						    /* unconditional */
#endif
						    private.regs[R_EPC] += 4;
						send_reply(fd, fieldp, 0,
							mk_status(SIGINT,WSTOPFLG));
					}
					else  {
						private.pda_nofault=oldnf;
						send_reply(fd, fieldp, 0,
							mk_status(SIGTRAP, WSTOPFLG));
					}
				}
			}
#else /* NETDBX */
			if  (IS_KSEGDM(private.regs[R_A0]) &&
				!strncmp((char *)private.regs[R_A0],"ring",4)){
				/* Do an implicit step over the brk inst. */
				private.regs[R_EPC] += 4;
				send_reply(fd, fieldp, 0,
				mk_status(SIGINT,WSTOPFLG));
			}
#endif /* NETDBX */
			else
				send_reply(fd, fieldp, 0,
					mk_status(SIGTRAP, WSTOPFLG));
			break;
		}
		break;

	case 'i':
	case 'd':
	case 'r':
	case 'I':
	case 'D':
	case 'R':
#ifdef NETDBX
	case 'm':
#endif /* NETDBX */
		send_reply(fd,fieldp,(unsigned long long)-1,EFAULT);
		break;

	case 'x':
		_fatal_error("Error on remote debugging exit");
	}
}

/*
 * mk_status -- put together an exit status
 */
static int
mk_status(unsigned signal_code, unsigned exit_status)
{
#ifdef COMPILER_FIXED
	union wait wait_status;

	wait_status.w_stopsig = signal_code;
	wait_status.w_stopval = exit_status;
	return (*(unsigned short *)&wait_status);
#else

#ifdef NETDBX
	if (netrdebug)
		return(((signal_code&0xff)<<8)|(exit_status&0xff)|(cpuid() << 16));
	else
#endif /* NETDBX */
		return(((signal_code&0xff)<<8)|(exit_status&0xff));
#endif
}

#ifdef NETDBX
char g_reply_buf[MAXPACKET];
#endif /* NETDBX */

/*
 * g_reply -- reply to general register read command
 */
static void
g_reply(ULONG fd, struct string_list *fieldp)
{
#ifndef NETDBX
	char g_reply_buf[MAXPACKET];
#endif /* !NETDBX */
	unsigned regmask;
	register int i;

	if (*atob(fieldp->strptrs[ADDR_FIELD], (int *)&regmask)) {
		send_reply(fd, fieldp, (unsigned long long)-1, EINVAL);
		return;
	}
	g_reply_buf[0] = '\0';

	GetReadStatus(fd);
	if (regmask & 1)
		append(g_reply_buf, btoh(private.regs[R_EPC]));

	for (i = 1; i < 32; i++)
		if (regmask & (1 << i))
			append(g_reply_buf, btoh(private.regs[i]));

#ifdef NETDBX
	dprintf(2, ("SEND: %s\n", g_reply_buf));
	if (netrdebug)
		netputpkt(fd, g_reply_buf, (int)strlen(g_reply_buf), DATA_PKTTYPE);
	else
		putpkt(fd, g_reply_buf, (int)strlen(g_reply_buf), DATA_PKTTYPE);
	dprintf(3, ("SEND: %s DONE\n", g_reply_buf));
#else
	putpkt(fd, g_reply_buf, (int)strlen(g_reply_buf), DATA_PKTTYPE);
#endif /* NETDBX */

}

#ifdef NETDBX
char m_reply_buf[MAXPACKET];

/*
 * m_reply -- reply to read memory command
 */
static void
m_reply(ULONG fd, struct string_list *fieldp, numinp_t wp, unsigned long nwords)
{
	register int i;

	m_reply_buf[0] = '\0';

	GetReadStatus(fd);

	strcpy(m_reply_buf, fieldp->strptrs[PID_FIELD]);	/* pid */
	append(m_reply_buf, fieldp->strptrs[REQ_FIELD]);	/* req */
	/*
	 * NOTE: if get_memory failed, we will do an error reply, so for now
	 * assume all reads for all nwords will succeed,
	 * and concoct the reply accordingly.
	 */
	append(m_reply_buf, btoh(nwords));			/* nwords */

	for (i = 0; i < nwords; i++, wp+=4) {
		/* printf("m_reply: Getting word %llx\n", wp); */
		append(m_reply_buf, btoh(GET_MEMORY(wp, SW_WORD)));
		/* printf("m_reply: Got word %x\n", GET_MEMORY(wp, SW_WORD)); */
	}

	dprintf(2, ("SEND (m_reply) : %s\n", m_reply_buf));
	netputpkt(fd, m_reply_buf, (int)strlen(m_reply_buf), DATA_PKTTYPE);
	dprintf(3, ("SEND (m_reply) : DONE\n"));
}

#ifdef MULTIPROCESSOR
/*
 * status_reply -- reply to cpu status command
 */
/* ARGSUSED */
static void
status_reply(ULONG fd, struct string_list *fieldp)
{
	register 	i;

	GetReadStatus(fd);
	g_reply_buf[0] = '\0';
	append(g_reply_buf, btoh(ncpus));

	for (i = 0; i < ncpus; i++) {
		append(g_reply_buf, btoh(getcpustate(i)));
		append(g_reply_buf, btoh((ULONG) brkptaddr[i]));
	}

	printf("SEND: %s\n", g_reply_buf);
	netputpkt(fd, g_reply_buf, strlen(g_reply_buf), DATA_PKTTYPE);
	printf("SEND: DONE\n");
}

#endif

char send_reply_buf[MAXPACKET];
#endif /* NETDBX */

/*
 * send_reply -- send a normal debugging protocol reply
 */
static void
send_reply(ULONG fd, struct string_list *fieldp, unsigned long long num1,
	unsigned long long num2)
{
#ifndef NETDBX
	char send_reply_buf[MAXPACKET];
#endif /* !NETDBX */

	GetReadStatus(fd);
	strcpy(send_reply_buf, fieldp->strptrs[PID_FIELD]);	/* pid */
	append(send_reply_buf, fieldp->strptrs[REQ_FIELD]);	/* req */
	append(send_reply_buf, btoh(num1));			/* result */
	append(send_reply_buf, btoh(num2));			/* data */

#ifdef NETDBX
	dprintf(2, ("SEND: %s\n", send_reply_buf));
	if (netrdebug)
		netputpkt(fd, send_reply_buf, (int)strlen(send_reply_buf), DATA_PKTTYPE);
	else
		putpkt(fd, send_reply_buf, (int)strlen(send_reply_buf), DATA_PKTTYPE);
	dprintf(3, ("SEND: %s DONE\n", send_reply_buf));
#else
	putpkt(fd, send_reply_buf, (int)strlen(send_reply_buf), DATA_PKTTYPE);
#endif /* NETDBX */

}

static void
append(char *buf, char *str)
{
	strcat(buf, " ");
	strcat(buf, str);
}

/*
 * btoh -- convert to ascii base 16 representation (0x%x)
 */
static char *
btoh(register unsigned long long val)
{
	static char buf[20];
	register int i;
	register char *cp;
	int digit;

	for (i = 60; i > 0; i -= 4)
		if ((val >> i) & 0xf)
			break;

	buf[0] = '0';
	buf[1] = 'x';
	cp = &buf[2];
	for (; i >= 0; i -= 4) {
		digit = (val >> i) & 0xf;
		if (digit < 10)
			*cp++ = '0' + digit;
		else
			*cp++ = 'a' + (digit - 10);
	}
	*cp = 0;
	return (buf);
}

/*
 * map dbgmon internal register numbers to ptrace register numbers
 */
static struct regmap {
	int	rm_ptrbase;
	int	rm_nregs;
	int	rm_dbgbase;
} regmap[] = {
	{ GPR_BASE,	NGP_REGS,	R_R0 },
	{ FPR_BASE,	NFP_REGS,	R_F0 },
	{ PC,		1,		R_EPC },
	{ CAUSE,	1,		R_CAUSE },
	{ MMHI,		1,		R_MDHI },
	{ MMLO,		1,		R_MDLO },
	{ FPC_CSR,	1,		R_C1_SR },
	{ FPC_EIR,	1,		R_C1_EIR },
	{ SR,		1,		R_SR },
	{ TLBHI,	1,		R_TLBHI },
	{ TLBLO,	1,		R_TLBLO },
	{ BADVADDR,	1,		R_BADVADDR },
	{ INX,		1,		R_INX },
	{ RAND,		1,		R_RAND },
	{ CTXT,		1,		R_CTXT },
	{ EXCTYPE,	1,		R_EXCTYPE },
#if R4000 || R10000
	{ TLBLO1,	1,		R_TLBLO1 },
	{ PGMSK,	1,		R_PGMSK },
	{ WIRED,	1,		R_WIRED },
	{ COUNT,	1,		R_COUNT },
	{ COMPARE,	1,		R_COMPARE },
	{ LLADDR,	1,		R_LLADDR },
	{ WATCHLO,	1,		R_WATCHLO },
	{ WATCHHI,	1,		R_WATCHHI },
	{ EXTCTXT,	1,		R_EXTCTXT },
	{ ECC,		1,		R_ECC },
	{ CACHERR,	1,		R_CACHERR },
	{ TAGLO,	1,		R_TAGLO },
	{ TAGHI,	1,		R_TAGHI },
	{ ERREPC,	1,		R_ERREPC },
#endif /* R4000 || R10000 */
	{ 0,		0,		0 }
};

static int
get_mapreg(int ptrace_reg)
{
	register struct regmap *rm;
	int off;

	for (rm = regmap; rm->rm_nregs; rm++) {
		if (ptrace_reg >= rm->rm_ptrbase
		    && ptrace_reg < rm->rm_ptrbase + rm->rm_nregs) {
			off = ptrace_reg - rm->rm_ptrbase;
			return(rm->rm_dbgbase + off);
		}
	}
	return(-1);
}
