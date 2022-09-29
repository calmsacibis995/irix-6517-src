/*
 * elf builtin commands for modular IDE
 */

#include <sys/types.h>
#include <sys/fpu.h>
#include <sys/sbd.h>
#include <genpda.h>

#include <arcs/io.h>
#include <arcs/time.h>
#include <parser.h>
#include <stringlist.h>
#include "uif.h"
#include "ide.h"
#include "ide_sets.h"
#include <ide_msg.h>
#include "ide_mp.h"
#include "sys/dir.h"
#include "sys/file.h"
#include <libsk.h>
#include <libsc.h>
#include <errno.h>

#include "ide_elf.h"

/* output that gets turned on from prom via Verbose env variable */
extern int Verbose;
#define VRB_ARG 3      
#define VRB_DUMP_SYMBOLS 10
#define Vrb_printf(x,n)	(Verbose>=n?printf x : 0)

/* output that gets turned on from prom via Debug env variable */
extern int Debug;
#define dbg_printf(x)	(Debug?printf x : 0)
#define Dbg_printf(x,n)	(Debug>=n?printf x : 0)

#if IP22
#define set_loadelfdynamicELFSIZE set_loadelfdynamic32
#define load_symtabELFSIZE        load_symtab32
#define dump_symbol_tableELFSIZE  dump_symbol_table32
#define ide_loadelfELFSIZEdynamic ide_loadelf32dynamic
#elif IP30
#define set_loadelfdynamicELFSIZE set_loadelfdynamic64
#define load_symtabELFSIZE        load_symtab64
#define dump_symbol_tableELFSIZE  dump_symbol_table64
#define ide_loadelfELFSIZEdynamic ide_loadelf64dynamic
#else
<<< !die! >>>
#endif

/*ARGSUSED*/
int
do_load(int argc, char *argv[])
{
  int i;
  long err;
  char **wp;
  struct string_list exec_argv_str;
  extern struct string_list argv_str;
  static int symbol_table_done = 0;

  /* check for proper usage */
  if (argc < 2) {
    msg_printf(JUST_DOIT,"usage: load FileName\n");
    return(EINVAL); /* bad argument */
  }

  msg_printf(JUST_DOIT, "Loading Module %s\n",argv[1]);

  /* turn on loading of dynamic elf binaries in stand/arcs/lib/libsk/lib/loadelf.c */
  set_loadelfdynamicELFSIZE(ide_loadelfELFSIZEdynamic);

  /* output args */
  Vrb_printf(("argc = %d\n",argc),VRB_ARG);
  for (i=0 ; i<argc ; i++)
    Vrb_printf(("argv[%d] = %s\n",i,argv[i]),VRB_ARG);

  /* output argv to core ide ... */
  Vrb_printf(("argv_str.strcnt = %d\n",argv_str.strcnt),VRB_ARG);
  for (i=0 ; i<argv_str.strcnt ; i++)
    Vrb_printf(("argv_str.strptrs[%d] = %s\n",i,argv_str.strptrs[i]),VRB_ARG);

  /* load symbol table of core */
  if (!symbol_table_done) {
    printf("Reading Symbol Table of File %s ...\n", argv_str.strptrs[0]);
    err = load_symtabELFSIZE(argv_str.strptrs[0],&_core_symbol_table);
    if (err) {
      msg_printf(JUST_DOIT,"Load symtab error = 0x%X\n",err);
      return(err);
    }
    symbol_table_done = 1;
  }

  /* print out all symbols in very verbose mode ... */
  if (Verbose >= VRB_DUMP_SYMBOLS)
    dump_symbol_tableELFSIZE(&_core_symbol_table);

  /* generate argv for module ... same as argv for core except
     that argv[0] holds module path instead of core's path */
  init_str(&exec_argv_str);
  if (argv_str.strcnt) {
    /* set exec_argv[0] to path name of module*/
    new_str1(argv[1], &exec_argv_str);
    /* we use exec_argv[1] to put the module's starting gp value -
       the actual gp value is filled in during loading ... */
    new_str1("1234567812345678", &exec_argv_str);
    /* save ptr to this space - needed by load to put the gp there ... */
    _module_gp_ptr = exec_argv_str.strptrs[1];
    /* copy rest */
    for (wp=&argv_str.strptrs[1] ; wp && *wp; wp++)
      if (new_str1(*wp, &exec_argv_str))
	return(ENOMEM);
  }

  /* dbg */
  dbg_printf((" _module_gp_ptr = 0x%X\n", _module_gp_ptr));
  Vrb_printf(("exec_argv_str.strcnt = %d\n",exec_argv_str.strcnt),VRB_ARG);
  for (i=0 ; i<exec_argv_str.strcnt ; i++)
    Vrb_printf(("exec_argv_str.strptrs[%d] = %s\n",i,exec_argv_str.strptrs[i]),VRB_ARG);

  /* done loading */
  printf("done\n");

  /* execute */
  err = Execute(argv[1],exec_argv_str.strcnt,exec_argv_str.strptrs,environ);
  if (err) {
    msg_printf(JUST_DOIT,"Load error = 0x%X\n",err);
    return(err);
  }

  dbg_printf(("Core: ide module loaded\n"));

  /* success */
  return(0);

} /* do_load */
