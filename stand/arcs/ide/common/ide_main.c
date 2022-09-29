
/*
 * Interactive Diagnostic Environment Command Line Interpreter
 *
 */
#ident "$Revision: 1.41 $"

#include <sys/sbd.h>

#include <genpda.h>
#include <parser.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include "ide.h"
#include "ide_mp.h"
#include <ide_msg.h>

/*
 * intr_jump is the the setjmp buffer used to store the state 
 * and address to which we return when an interrupt is issued
 * from the keyboard.  Therefore, only the process controlling
 * the duart (the ide_master) ever uses this jumpbuffer, so
 * it needn't be in ide's pda.
 */
jmp_buf	intr_jump;

/* 
 * genpda.h evaluates MULTIPROCESSOR; ensure it's been included
 */
#ifndef __GENPDA_H__
dontcompile("#include ERROR, ide_main.c: genpda.h hasn't been included!")
#endif

/* 
 * ide provides diagnostics with the ability to migrate the master
 * thread of execution (i.e. script-interpretation) to another
 * processor.  The previous master saves its state, signals the
 * selected slave, and longjmps back to wait in slave mode, where
 * one of three events may awaken it:  a) orders from the master to
 * launch a procedure, b) exchange-mode orders from master, or 
 * c) interrupts, if system errors have been directed to that
 * processor via software control.  Slaves and master both execute
 * the same fault-handling code, necessitating arrays of pdas,
 * each of which contains a complete set of the required global 
 * state values.
 */ 
#if defined(MULTIPROCESSOR)
jmp_buf		swap_mode;
#endif

int goteof = 0;

extern char	yyinbuff[];
extern size_t	yyinlen;
extern int	yyinbp;
extern char	ide_startup[];
extern int	inloop;   /* semantic feedback: the parser is inside a loop */
extern int	inblock;  /* ditto for block {} */
extern int	inswitch; /* ditto for switch */
extern int	inif;     /* ditto for if */
extern int 	ide_dosource;
extern int 	_dcache_size;
extern char	*ide_version;

extern struct reg_desc sr_desc[], config_desc[];
extern int	inif,inrawcmd,inblock,inswitch,inloop,badflag;

char	*ide_pstartup = ide_startup;
char	*ide_sname;

int	ide_cancelstartup;
int	_Verbose=0;
int	_Debug=0;
#ifdef IP32
extern void config_cache(void );
int ntlbentries;
#endif

static void intr_handler (void);

main(int argc, char *argv[])
{
	char cbuf[2*MAXLINELEN];
	int jmpval;

	ASSERT(!_ide_initialized);

	set_sr(IDE_MASTER_SR);

#ifdef IP32	/* used to be R10K and R4K */
	{
	    int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

	    if (!r5000) {
		    ntlbentries = R10K_NTLBENTRIES;
	    } else {
		    ntlbentries = R4K_NTLBENTRIES;
	    }
        }

	config_cache();

	cache_K0();		/* make sure K0 runs cached */
#endif /* IP32 */

#ifdef ARG_DEBUG
	if (argc && _SHOWIT(VRB)) {
	    int i;

	    printf("  IDE main(),  argc %d:\n    ", argc);
	    _dump_argvs(argc, argv);
	 	for (i = 0; i < argc; i++)
		    printf("%d: \"%s\"%s",i,(_STRPTR(argv[i])?argv[i]:"BADARG"),
			(i & 0x1 ? "\n      " : "  "));
	    printf("\n");
	}
#endif

	if ( getenv("VERBOSE") ) {
	    atob(getenv("VERBOSE"), &_Verbose);
	    msg_printf(INFO,
		"\n< Verbose environment (%d==0x%x) initialized from env>\n",
		_Verbose, _Verbose);
	}
	if ( getenv("DEBUG") ) {
            atob(getenv("DEBUG"), &_Debug);  /* initialize debugging flags */
	    msg_printf(INFO,
		"\n< Debug settings (%d==0x%x) initialized from env>\n",
		_Debug, _Debug);
	}

	ide_mpsetup();	/* set up ide's pdas */

	(void)version ();
	ide_init(argc, argv);

/*	buffoff(); */

#ifdef _STANDALONE
	/* trap ^C plus <esc> on field */
	if (strstr(ide_version,"field")) {
		Signal(SIGHUP,SIGDefault);
        	(void)ioctl(0,TIOCINTRCHAR,(long)"\003\033");
	}
#endif
	if (jmpval = setjmp(intr_jump)) {
	    msg_printf(SUM, "vid %d, restart", GPME(my_virtid));
	    if (Reportlevel >= DBG)
		msg_printf(SUM,":  badflag was %s\n",(badflag?"SET":"CLEAR"));

	    ide_whoami(cbuf);

	    switch(jmpval) {

	        case SCRIPT_ERR_RESTART:
		    msg_printf(INFO,
		    "%s, restart:  remainder of script was skipped\n", cbuf);
		    break;

	        case MASTER_INT_RESTART:
		    msg_printf(INFO, "%s: restart due to interrupt\n", cbuf);
		    break;

	        case SLAVE_INT_RESTART:
		    msg_printf(IDE_ERR,
			"%s: slave (v%d,pidx%d) is executing master code!!\n",
			cbuf, cpuid(), GPME(my_physid));
		    break;

	        default:
		    msg_printf(IDE_ERR, "%s: unknown longjmp returnval %d\n",
			cbuf, jmpval);
		    break;
	    }
	    busy (0);
	}

	Signal(SIGINT, intr_handler);

#ifdef _STANDALONE
	_hook_exceptions();
#endif
	while ( ! goteof )
		yyparse();
	
	msg_printf(JUST_DOIT,"!!! fell through in main\n");
	putchar('\n');

	EnterInteractiveMode();
	/*NOTREACHED*/
}

static void
intr_handler (void)
{
	int vid_me = cpuid();

	msg_printf(SUM, "\nInterrupt: ");
	ide_whoami(NULL);
	msg_printf(SUM, "\n");
	ide_cancelstartup = 1;
	yyinlen = 0;
	handle_interrupt();
	Signal (SIGALRM, SIGIgnore);

	/* 
	 * make damn sure nothing fishy's goin' on around here...
	 */
	if ((IDE_ME(ide_mode)!=IDE_MASTER && _ide_info.master_vid==vid_me) ||
   	    (IDE_ME(ide_mode)==IDE_MASTER && _ide_info.master_vid!=vid_me)) {
		msg_printf(IDE_ERR,
		    "!!intr_handler(v%d): ide_info disagreement!\n", vid_me);
		msg_printf(JUST_DOIT, "  my mode %d, NOT master (%d): ",
		    IDE_ME(ide_mode), IDE_MASTER);
		msg_printf(JUST_DOIT, " vid %d is master!\n",
			_ide_info.master_vid);
		ide_panic("intr_handler");
	}
	if (IDE_ME(ide_mode)!=IDE_MASTER || _ide_info.master_vid!=vid_me) {

	    ASSERT(IDE_ME(ide_mode) == IDE_SLAVE);
	    msg_printf(DBG,
		"  intr_hand(vid %d): my mode %d, master's vid is %d\n",
		vid_me, IDE_ME(ide_mode), _ide_info.master_vid);
	    msg_printf(DBG, "  longjmp back to slave_buf addr 0x%x: ", 
		(IDE_ME(slave_jbuf)));
	    longjmp(IDE_ME(slave_jbuf), SLAVE_INT_RESTART);
	}
	longjmp (intr_jump, MASTER_INT_RESTART);

	/*NOTREACHED*/

} /* intr_handler */

int
version (void)
{
	msg_printf(JUST_DOIT,"%s\n\n",ide_version);
	return 0;
}

/*
 * Get input from somewhere
 *
 * ide supports three different types of input:
 *  1. From a startup script linked in at compile time
 *  2. From the keyboard
 *  3. From a remote device via the "source" command
 *
 */
int
ide_refill(void)
{
	static struct edit_linebuf elinebuf;
	char *tmpp;

	if ( !ide_cancelstartup && *ide_pstartup ) {
		ide_sname = "startup script";
		if ( (yyinlen = strlen(ide_pstartup)) >= MAXLINELEN )
			strncpy(yyinbuff,ide_pstartup, yyinlen = MAXLINELEN-1 );
		else
			strcpy( yyinbuff, ide_pstartup );
		ide_pstartup += yyinlen;
	} else {
		if ( !(ide_dosource && (yyinlen = getsfromsource(yyinbuff))) ) {
			ide_sname = 0;
			if ( inloop || inswitch || inblock || inif ) {
				sym_t *psym = findsym("PS2");
				if ( psym )
					ide_puts(psym->sym_s);
				else
					ide_puts("> ");
			} else	{
				sym_t *psym = findsym("PS1");
				if ( psym )
					ide_puts(psym->sym_s);
				else
					ide_puts("! ");
			}

			/*  Interface from edit_gets() to yyinbuff is
			 * sub-optimal, and sizeof(yyinbuff) > LINESIZE.
			 */
			tmpp = edit_gets(&elinebuf);
			if (!tmpp) return EOF;
			strncpy(yyinbuff,tmpp,LINESIZE);

			yyinlen = strlen(yyinbuff);
			yyinbuff[yyinlen++] = '\n';
		}
	}

	yyinbp = 0;
	yyinbuff[yyinlen] = '\0';

	return yyinbuff[yyinbp++];
}

#if !EVEREST
/* stubs to keep some some textport data out of ide */
unsigned char clogo_data[1];
int clogo_w, clogo_h;
#endif
