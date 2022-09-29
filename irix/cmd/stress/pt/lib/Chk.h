#ifndef __CHK_H__
#define __CHK_H__

#include <errno.h>


/* Error checking
 *
 * Simple "assert" type macros which emit more information than
 * the standard version.
 */

/* ChkExp( 1 + 1 == 2, ( "oops" ) )
 */
#define ChkExp(cond, print_args) \
	if (!(cond)) { \
		ChkPrError print_args; \
		ChkPrError("ERROR:(%s:%d):\"%s\"\n", \
			   __FILE__, __LINE__, #cond); \
		ChkFatal; \
	}


/* _ChkX( malloc, (bytes), !=, 0, void * )
 * _CHKX( malloc, (bytes), !=, 0, void * )	errno version
 */
#define	_ChkX(stmt, args, rel, exp, type) \
	{	type __ret = (type)(stmt args); \
		ChkExp( __ret rel exp, \
			("["#stmt #args " "#rel " "#exp "] >%d<\n", __ret) ) \
	}
#define	_CHKX(stmt, args, rel, exp, type) \
	{	type __ret = (type)(stmt args); \
		ChkExp( __ret rel exp, \
			("["#stmt #args " "#rel " "#exp "] >%d<, errno %d\n", \
			__ret, errno) ) \
	}


#ifdef	OLD
#define ChkInt(stmt, args, rel, exp)	_ChkX( stmt, args, rel, exp, int )
#define CHKInt(stmt, args, rel, exp)	_CHKX( stmt, args, rel, exp, int )
#define ChkPtr(stmt, args, rel, exp)	_ChkX( stmt, args, rel, exp, void* )
#define CHKPtr(stmt, args, rel, exp)	_CHKX( stmt, args, rel, exp, void* )
#else	/* OLD */

/* ChkInt( ret = read(fd, buf, 1), >= 0 )
 * ChkPtr( ptr = malloc(bytes), != 0 )
 */
#define ChkInt(lhs, rhs)	_ChkX( lhs, , rhs, , int )
#define CHKInt(lhs, rhs)	_CHKX( lhs, , rhs, , int )
#define ChkPtr(lhs, rhs)	_ChkX( lhs, , rhs, , void* )
#define CHKPtr(lhs, rhs)	_CHKX( lhs, , rhs, , void* )
#endif	/* OLD */


/* Default values
 *
 * To be overridden by including file.
 */
#ifndef ChkFatal
#define ChkFatal	exit(1)
#endif
#ifndef ChkPrError
#define ChkPrError	printf
#endif

#endif	/* __CHK_H__ */

/* Shorthand

#include <errno.h>
#define ChkExp(cond, print_args) \
	if (!(cond)) { printf print_args; \
	printf("!!(%s:%d):<%s>\n", __FILE__, __LINE__, #cond); exit(1); }
#define ChkX(stmt, args, rel, exp, type) \
	{type __ret = (type)(stmt args); \
	ChkExp(__ret rel exp, \
	("[" #stmt #args " " #rel " " #exp "] >%d<\n", __ret))}
#define CHKX(stmt, args, rel, exp, type) \
	{type __ret = (type)(stmt args); \
	ChkExp(__ret rel exp, \
	("[" #stmt #args " " #rel " " #exp "] >%d<, errno %d\n", __ret, errno))}
#define ChkInt(stmt, args, rel, exp)	ChkX( stmt, args, rel, exp, int )
#define CHKInt(stmt, args, rel, exp)	CHKX( stmt, args, rel, exp, int )
#define ChkPtr(stmt, args, rel, exp)	ChkX( stmt, args, rel, exp, void* )
#define CHKPtr(stmt, args, rel, exp)	CHKX( stmt, args, rel, exp, void* )

 */
