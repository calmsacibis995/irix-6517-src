#ident	"lib/libsc/lib/check_dbg.c:  $Revision: 1.33 $"

/*
 * check_dbg.c -- check for debugger variables in the environment and
 * load the debugger if any are set
 */

#include <arcs/spb.h>
#include <arcs/tvectors.h>
#include <stringlist.h>
#include <arcs/hinv.h>
#include <arcs/debug_block.h>
#include <libsc.h>

extern char *findcpu(void);

int dbg_loadstop;

#define DEBUG_NAME	"symmon"		/* arcs symmon */

void
_check_dbg(int argc, char **argv, char **envp, __psunsigned_t start_pc)
{
    register char *bootfile, *tmpbootfile, **wp;
    int (*loader)(char *, long, char *[], char *[]);
    char string[64];
    static db_t db;
    struct string_list dbg_argv;

    if (argc == 0 || argv == 0 || *argv == 0)
	return;		/* Something bad has happened */

    /* If the debugger is already there, don't bother loading it.
     */
    if (SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_magic == DB_MAGIC) {
	if (getenv ("dbgstop"))
	    dbg_loadstop = 1;
	return;
    }

    /*
     * Filenames ending in ".DBG" force debugger loading,
     * as well as having DEBUG_NAME or "dbgmon" set in the environment.
     */
    bootfile = 0;
    if (((bootfile = rindex (argv[0],'.')) && !strcasecmp(bootfile, ".dbg")) ||
	    getenv(DEBUG_NAME) ||
	    getenv("dbgmon"))
	bootfile = getenv("dbgname");	/* load debugger */
    else
	return;

    /* Create an argv for symmon which is a copy of the client's, but
     * with the client's debug starting address added.
     */
    init_str(&dbg_argv);
    for (wp = argv; wp && *wp; wp++)
	if (new_str1(*wp, &dbg_argv))
	    return;

    sprintf (string, "startpc=0x%x", start_pc);
    if (new_str1(string, &dbg_argv))
	return;

    /* Boot debugger from the same device this command was booted from
     * unless the user requested a specific debugger name ("dbgname").
     */
    if (!bootfile) {
	bzero (string, sizeof string);
	strncpy(string, argv[0], sizeof string - 1);
	if ((bootfile = rindex (string,':')) ||
	    (bootfile = rindex (string,')'))) {
	    if (tmpbootfile = rindex (string,'/'))
		bootfile = tmpbootfile;
	    ++bootfile;
	}
	else		
	    /* No device was specified on booting the client.
	     * Get debugger from whatever path is set.
	     */
	    bootfile = string; 	
	strcpy(bootfile, DEBUG_NAME);
	bootfile += sizeof DEBUG_NAME - 1;
	*bootfile++ = '.';
	strcpy (bootfile, findcpu());
	bootfile = string;
    }

    /*
     * Since init_saio has not yet been called, we really want
     * the prom to load the debugger, so we have to go through
     * the real spb instead of using the libsk internal shortcut.
     */
    loader = (int (*)())__TV->Execute;
    if (!loader) {
	printf ("No valid loader function in spb.\n");
	return;
    }

    if (getenv ("dbgstop"))
	dbg_loadstop = 1;

    /* set up local copy of db_t */
    db.db_magic = DB_MAGIC;
    db.db_idbgbase = db.db_brkpttbl = NULL;
    CPUMASK_CLRALL(db.db_flush);
    db.db_nametab = db.db_symtab = NULL;
    db.db_bpaddr = NULL;
    db.db_printf = NULL;
    db.db_symmax = 0;
    SPB->DebugBlock = (LONG*)&db;

    printf("Loading %s\n", bootfile);
    if ((*loader)(bootfile, (LONG)dbg_argv.strcnt, dbg_argv.strptrs, envp)) {
	printf("Debugger could not be loaded.\n");
	return;
    }
    /* NOTREACHED */
}

/* findcpu replaces inv_findcpu during setup time.  
 * go through real spb instead of using the libsk internal shortcut
 * since init_saio has not yet been called
 */
char *
findcpu(void)
{
    COMPONENT *c = __TV->GetChild(NULL);
    char *p;

    if( !c)
	return(NULL);

    /* Return cfgroot->id stripping off the SGI prefix if present */
    p = c->Identifier;

    if (!strncmp(p,"SGI ", 3))
	p += 4;
    
    return(p);
}
