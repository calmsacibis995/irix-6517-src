#include <sys/types.h>
#include <libsc.h>
#include "ide.h"

/* the module's startup script */
extern char ide_module_startup[];

/* output that gets turned on from prom via Debug env variable */
extern int Debug;
#define dprintf(x)	(Debug?printf x : 0) 

/* external function protos */
extern void end_module(void);
extern char *getversion(void);

/* internal function protos */
static void save_ide_state(void);
static void restore_ide_state(void);
static void save_parser_state(void);
static void restore_parser_state(void);

/* used to store original core prompt */
static char*      prompt_orig;

/* used to save original ide state */
static char*      ide_pstartup_orig;
static builtin_t* diagcmds_ptr_orig;
static int*       ndiagcmds_ptr_orig;
static char*      ide_version_orig;

/* important externs needed to save ide state */
extern char*      ide_pstartup;
extern builtin_t* diagcmds_ptr;
extern int*       ndiagcmds_ptr;
extern char*      ide_version;

/* used to save original parser state */
static int    yyinbp_orig;
static int    yysave_orig;
static int    yysflag_orig;
static size_t yyinlen_orig;
static int    ide_currline_orig;
static int    yychar_orig; 
static int    goteof_orig; 
static int    badflag_orig;
static int    inrawcmd_orig;

/* important externs needed to save parser state */
extern int    yyinbp;
extern int    yysave;
extern int    yysflag;
extern size_t yyinlen;
extern int    ide_currline;
extern int    yychar; 
extern int    goteof; 
extern int    badflag;
extern int    inrawcmd;

/*
 * module_main(): C main for IDE module - called by start_module asm routine
 *                sets up ide & parser state for the loaded module and
 *                returns to core parser which now starts parsing the
 *                module's startup script
 */
void 
module_main(int argc, char* argv[]) {

  int i;

  /* output args */
  dprintf(("argc = %d\n",argc));
  for (i=0 ; i<argc ; i++) {
    dprintf(("argv[%d] = %s\n",i,argv[i]));
  }

  /* save original PS1 prompt */
  prompt_orig = makestr(findsym("PS1")->sym_s);

  /* save ide state */
  save_ide_state();

  /* set up module's ide state */
  ide_pstartup = ide_module_startup;
  diagcmds_ptr = diagcmds;
  ndiagcmds_ptr = &ndiagcmds;
  ide_version = getversion();

  /* add module commands */
  ide_init_builtins(*ndiagcmds_ptr, diagcmds_ptr, B_DIAGNOSTICS);  

  /* save parse state */
  save_parser_state();

  /* call ide_refill() to start parsing module's script */
  ide_refill();

  /* call core main to process */
  main(argc,argv);

}

/*
 *  unload: unload module & return to core
 */
void
do_unload(int argc, char *argv[])
{

  sym_t* prompt;
  int	yyinbp_orig;

  /* restore ide state */
  restore_ide_state();

  /* restore orig PS1 prompt */
  prompt = findsym("PS1");
  prompt->sym_s = prompt_orig;

  /* restore parse state */
  restore_parser_state();

  /* XXX need to remove module's builtins here !!! */

  /* restore core's script to where we left off before the module got called */
  /* call ide_refill but save yyinbp cause ide_refill sets it to 0 */
  ide_pstartup -= yyinlen;
  yyinbp_orig = yyinbp;
  ide_refill();
  yyinbp = yyinbp_orig;

  /* return to core */
  end_module();

}

/*
 * save_ide_state: save important parser variables
 */
static void
save_ide_state(void) {

  /* save ide state */
  ide_pstartup_orig = ide_pstartup;
  diagcmds_ptr_orig = diagcmds_ptr;
  ndiagcmds_ptr_orig = ndiagcmds_ptr;
  ide_version_orig = ide_version;

}

/*
 * restore_ide_state: save important parser variables
 */
static void
restore_ide_state(void) {

  /* restore ide state */
  ide_pstartup = ide_pstartup_orig; 
  diagcmds_ptr = diagcmds_ptr_orig;
  ndiagcmds_ptr = ndiagcmds_ptr_orig;
  ide_version = ide_version_orig;

}

/*
 * save_parser_state: save important parser variables
 */   
static void 
save_parser_state(void) {

  /* save parser state */
  yyinbp_orig = yyinbp;
  yysave_orig = yysave;
  yysflag_orig = yysflag;
  yyinlen_orig = yyinlen;
  ide_currline_orig = ide_currline;
  yychar_orig = yychar;
  goteof_orig = goteof;
  badflag_orig = badflag;
  inrawcmd_orig = inrawcmd;

}

/*
 * restore_parser_state: restore important parser variables
 */   
static void
restore_parser_state(void) {

  /* restore parser state */
  yyinbp = yyinbp_orig;
  yysave = yysave_orig;
  yysflag = yysflag_orig;
  yyinlen = yyinlen_orig;
  ide_currline = ide_currline_orig;
  yychar = yychar_orig;
  goteof = goteof_orig;
  badflag = badflag_orig;
  inrawcmd = inrawcmd_orig;

}

