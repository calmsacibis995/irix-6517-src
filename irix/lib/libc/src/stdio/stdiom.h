/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/stdiom.h	1.9"

#ifndef _STDIOM_H
#define _STDIOM_H

#include <sgidefs.h>
#if !defined(BSDSTDIO)
#include <stdarg.h>	/* for va_list from doscan.c */
#else
#include <varargs.h>
#endif

typedef unsigned char	Uchar;

#define MAXVAL (MAXINT - (MAXINT % BUFSIZ))

/*
* The number of actual pushback characters is the value
* of PUSHBACK plus the first byte of the buffer. The FILE buffer must,
* for performance reasons, start on a word aligned boundry so the value
* of PUSHBACK should be a multiple of word. 
* At least 4 bytes of PUSHBACK are needed.
* minimum buffer size must be at least 8 or shared library will break
*/
#if (_MIPS_SZPTR == 64)
#define PUSHBACK 8
#define _SMBFSZ  8
#else
#define PUSHBACK 4
#define _SMBFSZ  8
#endif

/*
 * max fd that can be stored in a FILE* is determined by the type of iob._file
 */
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define FD_MAX	INT_MAX
#elif (_MIPS_SIM == _MIPS_SIM_NABI32)
#define FD_MAX	USHRT_MAX
#else
#define FD_MAX	UCHAR_MAX
#endif

extern Uchar *_bufendtab[];
extern Uchar _bufendadj[];
extern Uchar _bufdirtytab[];

#if BUFSIZ & (BUFSIZ-1)		/* a power of two */
#	define MULTIBFSZ(SZ)	((SZ) & ~(BUFSIZ-1))
#else
#	define MULTIBFSZ(SZ)    ((SZ) - (SZ % BUFSIZ))
#endif

#undef _bufend

/* 
 * This macro is used to see whether an iop structure is one of the 
 * ones allocated initially, either by libc, or by a program (like 
 * csh, which can define their own _iob[]). Since iop might come from
 * the heap too, and heap addresses can be lower than data addresses,
 * we need to check iop against both the upper and lower _iob values.
 * This might be a performance hit and we should optimize the macros
 * in this file.
 */

#define INFIRSTLINK(iop)  ((iop >= _iob) && (iop < _lastbuf))

/*
 * Some of the foll. macros test whether they really need to set a 
 * value before they write the location - this is to reduce COW faults.
 */

#define _bufend(iop) (INFIRSTLINK(iop) ? _bufendtab[(iop - _iob)] : \
		_realbufend(iop))
#define setbufend(iop, end) \
        if (INFIRSTLINK(iop)) { \
                if (_bufendtab[(iop - _iob)] != end) \
                        _bufendtab[(iop - _iob)] = end; \
        } else _setbufend(iop,end)
#define resetbufend(iop) \
        if (INFIRSTLINK(iop)) { \
                if (_bufendadj[(iop - _iob)] != 0) { \
                        _bufendtab[(iop - _iob)] -= _bufendadj[(iop - _iob)]; \
			_bufendadj[(iop - _iob)] = 0; \
                } \
        } else _resetbufend(iop)
#define bufendadj(iop)  (INFIRSTLINK(iop) ? _bufendadj[(iop - _iob)] : \
		_realbufendadj(iop))
#define incbufend(iop, cnt) \
        if (INFIRSTLINK(iop)) { \
                _bufendtab[(iop - _iob)] += cnt; \
                _bufendadj[(iop - _iob)] += cnt; \
        } else _incbufend(iop, cnt)
#define bufdirty(iop) (INFIRSTLINK(iop) ? _bufdirtytab[(iop - _iob)] : \
		_realbufdirty(iop))
#define setbufdirty(iop) \
        if (INFIRSTLINK(iop)) _bufdirtytab[(iop - _iob)] = 1; \
        else _setbufdirty(iop)
#define setbufclean(iop) \
        if (INFIRSTLINK(iop)) { \
                if (_bufdirtytab[(iop - _iob)] != 0) \
                        _bufdirtytab[(iop - _iob)] = 0; \
        } else _setbufclean(iop)

extern void	_cleanup(void);
extern void	_flushlbf(void);
extern FILE	*_findiop(void);
extern void	_incbufend(FILE *, int);
extern Uchar 	*_realbufend(FILE *);
extern int 	_realbufendadj(FILE *);
extern int	_realbufdirty(FILE *);
extern void	_resetbufend(FILE *iop);
extern void	_setbufend(FILE *, Uchar *);
extern int	_wrtchk(FILE *iop);
extern void	_setbufclean(FILE *);
extern void	_setbufdirty(FILE *);

	/*
	* Internal routines from flush.c
	*/
extern void	_bufsync(FILE *, Uchar *);
extern int	_xflsbuf(FILE *);

	/*
	* Internal routines from _findbuf.c
	*/
extern Uchar 	*_findbuf(FILE *);

/* The following macros improve performance of the stdio by reducing the
	number of calls to _bufsync and _wrtchk.  _needsync checks whether 
	or not _bufsync needs to be called.  _WRTCHK has the same effect as
	_wrtchk, but often these functions have no effect, and in those cases
	the macros avoid the expense of calling the functions.  */

#define _needsync(p, bufend)	((bufend - (p)->_ptr) < \
					 ((p)->_cnt < 0 ? 0 : (p)->_cnt))

#define _WRTCHK(iop)	((((iop->_flag & (_IOWRT | _IOEOF)) != _IOWRT) \
				|| (iop->_base == 0)  \
				|| (iop->_ptr == iop->_base && iop->_cnt == 0 \
					&& !(iop->_flag & (_IONBF | _IOLBF)))) \
			? _wrtchk(iop) : 0 )

	/*
	 * Internal stuff from data.c
	 */
extern Uchar _smbuf[][_SMBFSZ];

	/*
	 * Internal routine from doscan.c
	 */
extern int _doscan(FILE *, unsigned char *, va_list);

#endif /* _STDIOM_H */
