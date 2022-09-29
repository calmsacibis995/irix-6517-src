/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			   All Rights Reserved				*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	dbug.h    include file for environments with dbug package
 *
 *  SCCS
 *
 *	@(#)dbug.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "dbug.h"
 *
 *  DESCRIPTION
 *
 *	The bru source code contains extensive sets of macros of
 *	the form DBUG_XXX, which greatly aid debugging when the
 *	macro based C debugging package (dbug) is installed.
 *
 *	For production code, the dbug macro expansions are normally
 *	null, resulting in the fastest and most compact executable.
 *
 *	Although there are similar null macro definitions in <local/dbug.h>
 *	we include a suitable copy here for environments that don't
 *	have <local/dbug.h> installed.
 *
 *	To enable usage of the "dbug" package simply define the
 *	preprocessor symbol "DBUG".
 *
 *		cc -c -DDBUG ...
 *
 */

#if DBUG

#if SYSDBUGLIB

#include <local/dbug.h>

#else	/* Not SYSDBUGLIB */

/*
 *	Internally used dbug variables which must be global.
 */

extern int _db_on_;			/* TRUE if debug currently enabled */
extern FILE *_db_fp_;			/* Current debug output stream */
extern char *_db_process_;		/* Name of current process */
extern int _db_keyword_ ();		/* Accept/reject keyword */
extern VOID _db_push_ ();		/* Push state, set up new state */
extern VOID _db_enter_ ();		/* New user function entered */
extern VOID _db_return_ ();		/* User function return */
extern VOID _db_pargs_ ();		/* Remember args for line */
extern VOID _db_doprnt_ ();		/* Print debug output */

/*
 *	These macros provide a user interface into functions in the
 *	dbug runtime support library.  They isolate users from changes
 *	in the MACROS and/or runtime support.
 *
 *	The symbols "__LINE__" and "__FILE__" are expanded by the
 *	preprocessor to the current source file line number and file
 *	name respectively.
 *
 *	WARNING ---  Because the DBUG_ENTER macro allocates space on
 *	the user function's stack, it must precede any executable
 *	statements in the user function.
 *
 */

#define DBUG_ENTER(a) \
	auto char *_db_func_, *_db_file_; \
	int _db_level_; \
	_db_enter_ (a,__FILE__,__LINE__,&_db_func_,&_db_file_,&_db_level_)
#define DBUG_LEAVE \
	(_db_return_ (__LINE__, &_db_func_, &_db_file_, &_db_level_))
#define DBUG_RETURN(a1) return (DBUG_LEAVE, (a1))
/*   define DBUG_RETURN(a1) {DBUG_LEAVE; return(a1);}  Alternate form */
#define DBUG_VOID_RETURN {DBUG_LEAVE; return;}
#define DBUG_EXECUTE(keyword,a1) \
	{if (_db_on_) {if (_db_keyword_ (keyword)) { a1 }}}
#define DBUG_PRINT(keyword,arglist) \
	{if (_db_on_) {_db_pargs_(__LINE__,keyword); _db_doprnt_ arglist;}}
#define DBUG_PUSH(a1) _db_push_ (a1)
#define DBUG_PROCESS(a1) (_db_process_ = a1)
#define DBUG_FILE (_db_fp_)

#endif  /* SYSDBUGLIB */

#else	/* Not DBUG */

#define DBUG_ENTER(a1)
#define DBUG_RETURN(a1) return(a1)
#define DBUG_VOID_RETURN return
#define DBUG_EXECUTE(keyword,a1)
#define DBUG_PRINT(keyword,arglist)
#define DBUG_PUSH(a1)
#define DBUG_PROCESS(a1)
#define DBUG_FILE (stderr)

#endif	/* DBUG */

