#include "synonyms.h"
#include <shlib.h>
#include <mplib.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <nan.h>
#include <memory.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include "print.h"      /* parameters & macros for doprnt */
#include "../stdio/stdiom.h"
#include "_locale.h"
#include "priv_extern.h"

#define	NDIG 82	/* from ecvt.c */
/* Some definitions for printw so that we do not have to include
 * curses.h.
 */

struct window;
typedef struct window WINDOW;
extern WINDOW *stdscr;
#pragma weak stdscr
extern int waddstr (WINDOW *, char * );
#pragma weak waddstr

/* This file constins a set of routines to perform the actions required
 * by printf/fprintf/sprintf/printw for the individual conversion
 * characters.
 *
 * The routines are
 *
 *   _doprnt_c : for a single character        ( %c )
 *   _doprnt_s : for a string                  ( %s )
 *   _doprnt_d : for signed decimal integers   ( %d, %u )
 *   _doprnt_u : for unsigned decimal integers ( %u )
 *   _doprnt_b : for integers in octal, hex    ( %o, %p, %x, %X )
 *   _doprnt_g : for floating point numbers    ( %f, %e, %E, %g, %G )
 *
 * The arguments to these routines are of the form
 *
 *   routine ( flags, dst, value, arg1, arg2, arg3 )
 *
 *   where,
 *
 *     flags   : passes information rgarding the item to be formatted
 *               this includes the following
 *
 *               routine being invoked (sprintf,fprintf,printf,printw)
 *               whether it is the first item (for locking)
 *               whether it is the last item (for unlocking)
 *               whether the number of characters processed is needed
 *               whether a width was specified
 *               whether a precison was specified
 *               whether '+' was specified for item
 *               whether '-' was specified for item
 *               whether ' ' was specified for item
 *               whether '#' was specified for item
 *               whether '0' was specified for item
 *               the type of item being displayed
 *               the format operation for integer/float conversion
 *
 *     dst     : either the address of buffer for sprintf,
 *               address of file descriptor for fprintf,printf
 *               address of window for printw
 *
 *     value   : value to be displayed
 *
 *     arg1,   : optional arguments to specify the width, precision or
 *     arg2,     address of the char_count.
 *     arg3      These are extracted based on the values in the
 *               flags field.
 *
 * These routines return an integer specifying the number of characters
 * processed.
 *
 */

/* bit positions for flags */

#define P_FORMAT_MASK         3    /* mask to get at format routine */
#define P_FIRST_ITEM          4    /* first item being formatted */
#define P_LAST_ITEM           8    /* last item being formatted */
#define P_CHAR_COUNT       0x10    /* need char count */
#define P_WIDTH            0x20    /* has width */
#define P_PREC             0x40    /* has prec */
#define P_FPLUS            0x80    /* + */
#define P_FMINUS          0x100    /* - */
#define P_FBLANK          0x200    /* blank */
#define P_FSHARP          0x400    /* # */
#define P_PADZERO         0x800    /* padding zeroes requested via '0' */
#define P_DOTSEEN        0x1000    /* dot appeared in format specification */
#define P_SUFFIX         0x2000    /* a suffix is to appear in the output */
#define P_RZERO          0x4000    /* there will be trailing zeros in output */
#define P_LZERO          0x8000    /* there will be leading zeroes in output */
#define P_LDOUBLE       0x10000    /* L */
#define P_LLONG         0x20000    /* ll */
#define P_LENGTH        0x40000    /* l */
#define P_SHORT         0x80000    /* h */
#define P_INTEGER_MASK 0x700000    /* 'o', 'd', 'u', 'x', 'X', 'p', 'i' */
#define P_FLOAT_MASK   0x700000    /* 'f', 'E', 'e', 'G', 'g' */

/* name of the format routine */

typedef enum {
  SPRINTF, FPRINTF, PRINTF, PRINTW
} FORMAT;

/* macro to get at the format routine */

#define GET_FORMAT(x) (FORMAT)(x & P_FORMAT_MASK)

/* conversion operators for integers */

typedef enum {
  percent_no_i, percent_o, percent_d, percent_u,
  percent_p,    percent_x, percent_X, percent_i
} INTEGER_CODE;

/* macro to get at conversion operator for integers */

#define GET_INTEGER_CODE(x) (INTEGER_CODE)((x & P_INTEGER_MASK) >> 20)

/* conversion operators for floats */

typedef enum {
  percent_no_f, percent_f, percent_E, percent_e,
  percent_G,    percent_g
} FLOAT_CODE;

/* macro to get at conversion operators for floats */

#define GET_FLOAT_CODE(x)     (FLOAT_CODE)((x & P_INTEGER_MASK) >> 20)

static const char float_outchar [] = { '\0', ' ', 'E', 'e', 'E', 'e' };

static const char _blanks[] = "                    ";
static const char _zeroes[] = "00000000000000000000";
static const char uc_digs[] = "0123456789ABCDEF";
static const char lc_digs[] = "0123456789abcdef";
static const char  lc_nan[] = "nan0x";
static const char  uc_nan[] = "NAN0X";
static const char  lc_inf[] = "inf";
static const char  uc_inf[] = "INF";

static int _dowrite(const char *, int, FILE *, unsigned char **);

/* macro to place a sequence of characters in destination */

#define PUTOLD(p, n) { \
  register unsigned char * newbufptr; \
  if ((newbufptr = bufptr + n) > bufferend) { \
    if (!_dowrite(p, (int) n, file, &bufptr)) { \
      if ( char_count ) \
        *char_count = EOF; \
      return 0; \
    } \
  } else { \
    (void) memcpy((char *) bufptr, p, (size_t) n); \
    bufptr = newbufptr; \
  } \
}

#define PUT(p, n) { \
  if ( format == SPRINTF ) { \
    (void) memcpy((char *) bufptr, p, (size_t) n); \
    bufptr += n; \
  } \
  else \
  if ( format == PRINTW ) { \
    waddstr (window, (char *) p); \
  } \
  else { \
    register unsigned char * newbufptr; \
    if ((newbufptr = bufptr + n) > bufferend) { \
      if (!_dowrite(p, (int) n, file, &bufptr)) { \
        if ( char_count ) \
          *char_count = EOF; \
        return 0; \
      } \
    } else { \
      (void) memcpy((char *) bufptr, p, (size_t) n); \
      bufptr = newbufptr; \
    } \
  } \
}

/* macro to place a sequence of identical characters in destination */

#define PAD(s,n) {\
  register long nn; \
  for (nn = n; nn > 20; nn -= 20) \
    PUT(s, nn); \
  if (nn) \
    PUT(s + 20 - nn, nn); \
}


/* The function _dowrite carries out buffer pointer bookkeeping surrounding */
/* a call to fwrite.  It is called only when the end of the file output */
/* buffer is approached or in other unusual situations. */

static int
_dowrite ( register const char *p, register int n, register FILE *file,
           register unsigned char **ptrptr)
{
  if ( ! (file->_flag & _IOREAD) ) {

    file->_cnt -= (*ptrptr - file->_ptr);
    file->_ptr = *ptrptr;
    _bufsync(file, _bufend(file));

    if (fwrite(p, 1, (size_t) n, file) != (size_t) n)
                        return 0;

    *ptrptr = file->_ptr;
  }

  else
    *ptrptr = (unsigned char *) memcpy((char *) *ptrptr, p, (size_t) n) + n;

  return 1;
}

/* Common set of actions done on entry */

#define DO_ENTRY_ACTIONS() { \
 \
  format = GET_FORMAT(flags); \
 \
  switch ( format ) { \
 \
    case SPRINTF: \
 \
      bufptr    = (unsigned char *) dst; \
      bufferend = (unsigned char *)((long) bufptr | (-1L & MAXLONG)); \
      break; \
 \
    case PRINTF: \
    case FPRINTF: \
 \
      file      = (FILE *) dst; \
 \
      if ( flags & P_FIRST_ITEM ) \
        LOCKINIT(lock_val, LOCKFILE(file)); \
 \
      if ( ! ( file->_flag & _IOWRT ) ) { \
 \
        /* if no write flag */ \
        if (    ( file->_flag & _IORW ) \
             && ( file->_flag & ( _IOREAD | _IOEOF ) ) != _IOREAD ) { \
 \
          /* if ok, cause read-write */ \
          file->_flag = ( file->_flag & ~( _IOREAD | _IOEOF ) ) | _IOWRT; \
        } \
 \
        else { \
 \
          if ( flags & P_FIRST_ITEM ) \
            UNLOCKFILE(file, lock_val); \
 \
          setoserror(EBADF); \
          if ( char_count ) \
            *char_count = EOF; \
          return 0; \
        } \
      } \
 \
      if (file->_base == 0 && _findbuf(file) == 0) { \
        if ( char_count ) \
          *char_count = EOF; \
        return 0; \
      } \
      bufptr    = file->_ptr; \
      bufferend = (unsigned char *)_bufend(file); \
      break; \
 \
    case PRINTW: \
 \
      window = (WINDOW *) dst; \
      break; \
  } \
} /* DO_ENTRY_ACTIONS */

/* Macro to get width if specified */

#define GET_WIDTH() \
  flags & P_WIDTH  \
    ? nargs++, \
      ( arg1 < 0 )  \
      ? flags |= P_FMINUS, 0 - arg1 \
      : arg1 \
    : 0
 
/* Macro to get precision if specified */

#define GET_PREC() \
  flags & P_PREC \
    ? nargs++, \
    ( nargs == 1 ) \
      ? ( arg1 < 0 ) \
        ? flags &= ~P_DOTSEEN, 0 \
        : arg1 \
      : ( arg2 < 0 ) \
        ? flags &= ~P_DOTSEEN, 0 \
        : arg2 \
    : 0

/* Macro to get chr_count if specified */
 
#define GET_CHAR_COUNT() \
  flags & P_CHAR_COUNT \
    ? nargs++, \
    ( nargs == 1 ) \
      ? ( int * ) arg1 \
      : ( nargs == 2 ) ? ( int * ) arg2 : ( int * ) arg3 \
    : NULL

/* Common set of actions on exit */

#define DO_EXIT_ACTIONS() { \
  if ( flags & P_LAST_ITEM ) { \
 \
    if ( format == SPRINTF ) \
      *bufptr = '\0'; \
    else \
    if ( format == PRINTW ) { \
      if ( char_count ) \
        *char_count += (int)count; \
    } \
    else { \
      register long nn = bufptr - file->_ptr; \
 \
      file->_cnt -= nn; \
      file->_ptr = bufptr; \
 \
      if (    bufptr + file->_cnt > bufferend \
           && !(file->_flag & _IOREAD)) /* in case of interrupt */ \
        _bufsync(file, bufferend); /* during last several lines */ \
 \
      if (    file->_flag & (_IONBF | _IOLBF) \
           && (    file->_flag & _IONBF \
                || memchr((char *)(bufptr-nn), '\n', (size_t) nn) != NULL)) \
        (void) _xflsbuf(file); \
 \
      UNLOCKFILE(file, lock_val); \
      if ( ferror(file) ) { \
        if ( char_count ) \
          *char_count = EOF; \
        count = 0; \
      } \
      else { \
        if ( char_count ) \
          *char_count += (int)count; \
      } \
    } \
  } \
 \
  else { \
    if ( format != SPRINTF ) { \
      if ( char_count ) \
        *char_count += (int)count; \
      if ( format != PRINTW ) { \
        register long nn = bufptr - file->_ptr; \
        file->_cnt -= nn; \
        file->_ptr = bufptr; \
      } \
    } \
  } \
  return (int)count; \
}


/* actions for %c, %% */

int
_doprnt_c ( int flags, void * dst, char value,
            long arg1, long arg2, long arg3 )
{
  unsigned char * bufptr;
  unsigned char * bufferend;
  FILE          * file;
  WINDOW        * window;
  int             nargs = 0;
  long            width;
  int           * char_count;

  FORMAT          format;
  long            count;

  long            k = 1;
  long            lzero = 0;
  char            buf [2];
  LOCKDECL(lock_val);

  buf [0] = value;
  buf [1] = '\0';

  width      = GET_WIDTH();
  char_count = GET_CHAR_COUNT();

  DO_ENTRY_ACTIONS();

  if ( width <= 1 )
    count = k;

  else {
    count = width;

    /* Set up for padding zeroes if requested */
    /* Otherwise emit padding blanks unless output is */
    /* to be left-justified.  */

    if (flags & P_PADZERO) {
      if (!(flags & P_LZERO)) {
        flags |= P_LZERO;
        lzero = width - k;
      }
      else
        lzero += width - k;
      k = width; /* cancel padding blanks */
    } else
      /* Blanks on left if required */
      if (!(flags & P_FMINUS))
        PAD(_blanks, width - k);
  }

  /* Zeroes on the left */
  if (flags & P_LZERO)
    PAD(_zeroes, lzero);

  /* The value itself */

  PUT(buf, 1);

  if (flags & P_FMINUS) {
    /* Blanks on the right if required */
    if (flags & P_FMINUS && width > k)
      PAD(_blanks, width - k);
  }

  DO_EXIT_ACTIONS();
} /* _doprnt_c */


/* Actions for %s */

int
_doprnt_s ( int flags, void * dst, char * string,
            long arg1, long arg2, long arg3 )
{
  unsigned char * bufptr;
  unsigned char * bufferend;
  FILE          * file;
  WINDOW        * window;
  int             nargs = 0;
  long            width;
  long            prec;
  int           * char_count;

  FORMAT          format;
  long            count;

  char          * p;
  long            k;
  long            n;
  long            lzero = 0;
  LOCKDECL(lock_val);

  width      = GET_WIDTH();
  prec       = GET_PREC();
  char_count = GET_CHAR_COUNT();

  DO_ENTRY_ACTIONS();

  if ( flags & P_DOTSEEN ) {
    p = string;
    while ( *p++ != '\0' && --prec >= 0 ) {}
    n = k = (p - string) - 1;
  }
  else
    n = k = (long)strlen(string);

  if (width <= k)
    count = k;
  else {
    count = width;

    /* Set up for padding zeroes if requested */
    /* Otherwise emit padding blanks unless output is */
    /* to be left-justified.  */

    if (flags & P_PADZERO) {
      if (!(flags & P_LZERO)) {
        flags |= P_LZERO;
        lzero = width - k;
      }
      else
        lzero += width - k;
      k = width; /* cancel padding blanks */
    } else
      /* Blanks on left if required */
      if (!(flags & P_FMINUS))
        PAD(_blanks, width - k);
  }

  /* Zeroes on the left */
  if (flags & P_LZERO)
    PAD(_zeroes, lzero);

  /* The value itself */
  PUT(string, n);

  if (flags & P_FMINUS) {
    /* Blanks on the right if required */
    if (flags & P_FMINUS && width > k)
      PAD(_blanks, width - k);
  }

  DO_EXIT_ACTIONS();
} /* _doprnt_s */


/* Actions for %d, %i */

int
_doprnt_d ( int flags, void * dst, long value,
            long arg1, long arg2, long arg3 )
{
  unsigned char * bufptr;
  unsigned char * bufferend;
  FILE          * file;
  WINDOW        * window;
  int             nargs = 0;
  long            width;
  long            prec;
  int           * char_count;

  FORMAT          format;
  long            count;

  char            buf [24];
  char          * prefix;
  int             prefixlength = 0;
  long            lzero = 0;
  int             ndigs;
  long            k;
  LOCKDECL(lock_val);

  width      = GET_WIDTH();
  prec       = GET_PREC();
  char_count = GET_CHAR_COUNT();

  DO_ENTRY_ACTIONS();

  /* If signed conversion, make sign */
  if (value < 0) {
    prefix = "-";
    prefixlength = 1;
    value = -value;
  }
  else if (flags & P_FPLUS) {
    prefix = "+";
    prefixlength = 1;
  }
  else if (flags & P_FBLANK) {
    prefix = " ";
    prefixlength = 1;
  }

  if ( value != 0 || !(flags & P_DOTSEEN))
    ndigs = _ultoa (value, buf);
  else {
    ndigs = 0;
    buf [0] = '\0';
  }

  /* Calculate minimum padding zero requirement */

  if (flags & P_DOTSEEN) {
    lzero = prec - ndigs;
    if (lzero > 0)
      flags |= P_LZERO;
    else
      lzero = 0;
  }

  /* Calculate number of padding blanks */
  k = ndigs + prefixlength + lzero;

  if (width <= k)
    count = k;
  else {
    count = width;

    /* Set up for padding zeroes if requested */
    /* Otherwise emit padding blanks unless output is */
    /* to be left-justified.  */

    if (flags & P_PADZERO) {
      if (!(flags & P_LZERO)) {
        flags |= P_LZERO;
        lzero = width - k;
      }
      else
        lzero += width - k;
      k = width; /* cancel padding blanks */
    } else
      /* Blanks on left if required */
      if (!(flags & P_FMINUS))
        PAD(_blanks, width - k);
  }

  /* Prefix, if any */
  if (prefixlength != 0)
    PUT(prefix, prefixlength);

  /* Zeroes on the left */
  if (flags & P_LZERO)
    PAD(_zeroes, lzero);

  /* The value itself */
  if (ndigs > 0)
    PUT(buf, ndigs);

  if (flags & P_FMINUS) {
    /* Blanks on the right if required */
    if (flags & P_FMINUS && width > k)
      PAD(_blanks, width - k);
  }

  DO_EXIT_ACTIONS();
} /* _doprnt_d */


/* Actions for %u */

int
_doprnt_u ( int flags, void * dst, unsigned long value,
            long arg1, long arg2, long arg3 )
{
  unsigned char * bufptr;
  unsigned char * bufferend;
  FILE          * file;
  WINDOW        * window;
  int             nargs = 0;
  long            width;
  long            prec;
  int           * char_count;

  FORMAT          format;
  long            count;

  char            buf [24];
  long            lzero = 0;
  int             ndigs;
  long            k;
  LOCKDECL(lock_val);

  width      = GET_WIDTH();
  prec       = GET_PREC();
  char_count = GET_CHAR_COUNT();

  DO_ENTRY_ACTIONS();

  if ( value != 0 || !(flags & P_DOTSEEN))
    ndigs = _ultoa (value, buf);
  else {
    ndigs = 0;
    *buf  = '\0';
  }

  /* Calculate minimum padding zero requirement */

  if (flags & P_DOTSEEN) {
    lzero = prec - ndigs;
    if (lzero > 0)
      flags |= P_LZERO;
    else
      lzero = 0;
  }

  /* Calculate number of padding blanks */
  k = ndigs + lzero;

  if (width <= k)
    count = k;
  else {
    count = width;

    /* Set up for padding zeroes if requested */
    /* Otherwise emit padding blanks unless output is */
    /* to be left-justified.  */

    if (flags & P_PADZERO) {
      if (!(flags & P_LZERO)) {
        flags |= P_LZERO;
        lzero = width - k;
      }
      else
        lzero += width - k;
      k = width; /* cancel padding blanks */
    } else
      /* Blanks on left if required */
      if (!(flags & P_FMINUS))
        PAD(_blanks, width - k);
  }

  /* Zeroes on the left */
  if (flags & P_LZERO)
    PAD(_zeroes, lzero);

  /* The value itself */
  if (ndigs > 0)
    PUT(buf, ndigs);

  if (flags & P_FMINUS) {
    /* Blanks on the right if required */
    if (flags & P_FMINUS && width > k)
      PAD(_blanks, width - k);
  }

  DO_EXIT_ACTIONS();
} /* _doprnt_u */


/* Actions for %o, %p, %x, %X */

int
_doprnt_b ( int flags, void * dst, unsigned long value,
            long arg1, long arg2, long arg3 )
{
  unsigned char * bufptr;
  unsigned char * bufferend;
  FILE          * file;
  WINDOW        * window;
  int             nargs = 0;
  long            width;
  long            prec;
  int           * char_count;

  FORMAT          format;
  INTEGER_CODE    code;
  long            count;

  char          * p;
  char          * bp;
  int             mradix;
  int             lradix;
  long            lzero = 0;
  char          * prefix = NULL;
  int             prefixlength = 0;
  const char    * tab;
  char            buf[MAXDIGS+1];
  long            n;
  long            k;
  unsigned long   val = value;
  LOCKDECL(lock_val);

  width      = GET_WIDTH();
  prec       = GET_PREC();
  char_count = GET_CHAR_COUNT();

  DO_ENTRY_ACTIONS();

  code = GET_INTEGER_CODE(flags);

  if ( code == percent_o ) {
    mradix = 7;
    lradix = 3;
    tab = lc_digs;
  }
  else {
    mradix = 15;
    lradix = 4;
    tab = ( code == percent_X ) ? uc_digs : lc_digs;
  }

  /* Develop the digits of the value */
  p = bp = buf + MAXDIGS;
  *p = '\0';

  if (val == 0) {
    if (!(flags & P_DOTSEEN)) {
      lzero = 1;
      flags |= P_LZERO;
    }
  } else
    do {
      *--bp = tab[val & mradix];
      val = val >> lradix;
  } while (val != 0);

  /* Calculate minimum padding zero requirement */
  if (flags & P_DOTSEEN) {
    register long leadzeroes = prec - (p - bp);
    if (leadzeroes > 0) {
      lzero = leadzeroes;
      flags |= P_LZERO;
    }
  }

  /* Handle the # flag */
  if (flags & P_FSHARP && value != 0) {
    if ( code == percent_o ) {
      if (!(flags & P_LZERO)) {
        lzero = 1;
        flags |= P_LZERO;
      }
    }

    else
    if ( code != percent_p ) {
      if ( code == percent_x )
        prefix = "0x";
      else
        prefix = "0X";
      prefixlength = 2;
    }
  }

  /* Calculate number of padding blanks */
  k = (n = p - bp) + prefixlength + lzero;

  if (width <= k)
    count = k;
  else {
    count = width;

    /* Set up for padding zeroes if requested */
    /* Otherwise emit padding blanks unless output is */
    /* to be left-justified.  */

    if (flags & P_PADZERO) {
      if (!(flags & P_LZERO)) {
        flags |= P_LZERO;
        lzero = width - k;
      }
      else
        lzero += width - k;
      k = width; /* cancel padding blanks */
    } else
      /* Blanks on left if required */
      if (!(flags & P_FMINUS))
        PAD(_blanks, width - k);
  }

  /* Prefix, if any */
  if (prefixlength != 0)
    PUT(prefix, prefixlength);

  /* Zeroes on the left */
  if (flags & P_LZERO)
    PAD(_zeroes, lzero);

  /* The value itself */
  if (n > 0)
    PUT(bp, n);

  if (flags & P_FMINUS) {
    /* Blanks on the right if required */
    if (flags & P_FMINUS && width > k)
      PAD(_blanks, width - k);
  }

  DO_EXIT_ACTIONS();
} /* _doprnt_b_wp */


/* Actions for %f, %e, %E, %g, %G */

int
_doprnt_f ( int flags, void * dst, double dval,
            long arg1, long arg2, long arg3 )
{
  unsigned char * bufptr;
  unsigned char * bufferend;
  FILE          * file;
  WINDOW        * window;
  int             nargs = 0;
  long            width;
  long            prec;
  int           * char_count;

  FORMAT          format;
  FLOAT_CODE      code;

  int             decpt;
  int             sign;
  char          * bp;
  char          * p;
  int             maxfsig;
  char          * prefix;
  int             prefixlength = 0;
  long            otherlength = 0;
  char          * suffix = 0;
  int             suffixlength = 0;
  long            k;
  long            rzero;
  long            lzero;
  long            count;
  long            n;
#pragma set woff 1209
  char            buf[max(MAXDIGS, 1+max(MAXFCVT+MAXEXP, 2*MAXECVT))];
#pragma reset woff 1209
  char            expbuf[MAXESIZ + 1];
  long long       val;
  char	          cvtbuf[NDIG+2];		/* for [ef]cvt return values */
  LOCKDECL(lock_val);

  width      = GET_WIDTH();
  prec       = GET_PREC();
  char_count = GET_CHAR_COUNT();

  DO_ENTRY_ACTIONS();

  code = GET_FLOAT_CODE(flags);

  if ( ! ( flags & P_DOTSEEN ) )
    prec = 6;
  else
  if (    ( prec == 0 )
       && ( code == percent_g || code == percent_G ) )
    prec = 1;

  /* Check for NaNs and Infinities  */

  if (IsNANorINF(dval)) {

    if ( IsNegNAN(dval) ) {
      prefix = "-";
      prefixlength = 1;
    }
    else
    if ( flags & P_FPLUS ) {
      prefix = "+";
      prefixlength = 1;
    }
    else
    if ( flags & P_FBLANK ) {
      prefix = " ";
      prefixlength = 1;
    }

    if ( IsINF(dval) ) {
      bp = ( code == percent_E || code == percent_G ) ? ( char * ) uc_inf
                                                      : ( char * ) lc_inf;
      p = bp + 3;
    }

    else {
      bp = ( code == percent_E || code == percent_G ) ? ( char * ) uc_nan
                                                      : ( char * ) lc_nan;

      if ( code == percent_e || code == percent_E )
         p = bp + 3;

      else {
        p = buf;
        val = GETNaNPC(dval);
        if ( code == percent_G )
          p += sprintf ( buf, "%s%llX", bp, val );
        else
          p += sprintf ( buf, "%s%llx", bp, val );
        bp = buf;
      }
    }

    goto nan_join;
  }

  /* Do the conversion */

  if ( code == percent_f ) {

    bp = fcvt_r ( dval, min ( (int)prec, MAXFCVT ), &decpt, &sign, cvtbuf );
    goto f_merge;
  }

  if ( code == percent_e || code == percent_E ) {

    bp = ecvt_r( dval, min ( (int)prec + 1, MAXECVT ), &decpt, &sign, cvtbuf );
    goto e_merge;
  }

  if ( prec == 0 && ( flags & P_DOTSEEN ) )
    prec = 1;

  bp = ecvt_r( dval, min ( (int)prec, MAXECVT ), &decpt, &sign, cvtbuf );

  if ( dval == 0 )
    decpt = 1;

  {
    register long kk = prec;

    if ( ! ( flags & P_FSHARP ) ) {

      n = ( int ) strlen (bp);

      if ( n < kk )
        kk = n;

      while ( kk >= 1 && bp [kk - 1] == '0' )
        --kk;
    }

    if ( decpt < -3 || decpt > prec ) {

      prec = kk - 1;
      goto e_merge;
    }

    else {

      prec = kk - decpt;
      goto f_merge;
    }
    
  }

f_merge:

  /* Determine the prefix */

  if (sign && decpt > -prec && *bp != '0') {
    prefix = "-";
    prefixlength = 1;
  } else if (flags & P_FPLUS) {
    prefix = "+";
    prefixlength = 1;
  } else if (flags & P_FBLANK) {
    prefix = " ";
    prefixlength = 1;
  }

  /* Initialize buffer pointer */
  p = &buf[0];

  {
    register int nn = decpt;

    if(flags & P_LDOUBLE) 
      maxfsig = 2*MAXFSIG;
    else
      maxfsig = MAXFSIG;

    /* Emit the digits before the decimal point */
    k = 0;
    do {
      *p++ = (char) ((nn <= 0 || *bp == '\0' 
                             || k >= maxfsig) ?
                                 '0' : (k++, *bp++));
    } while (--nn > 0);

    /* Decide whether we need a decimal point */
    if ((flags & P_FSHARP) || prec > 0)
      *p++ = _numeric[0];

    /* Digits (if any) after the decimal point */
    nn = min((int)prec, MAXFCVT);
    if (prec > nn) {
      flags |= P_RZERO;
      otherlength = rzero = prec - nn;
    }
    while (--nn >= 0)
      *p++ = (char) ((++decpt <= 0 || *bp == '\0' ||
                            k >= 2*maxfsig) ? '0' : (k++, *bp++));
  }

  goto join;

e_merge:

  /* Determine the prefix */

  if (sign) {
    prefix = "-";
    prefixlength = 1;
  } else if (flags & P_FPLUS) {
    prefix = "+";
    prefixlength = 1;
  } else if (flags & P_FBLANK) {
    prefix = " ";
    prefixlength = 1;
  }

  /* Initialize the buffer pointer */

  p = &buf[0];

  /* Place the first digit in the buffer */

  *p++ = (char) ((*bp != '\0') ? *bp++ : '0');

  /* Put in a decimal point if needed */

  if (prec != 0 || (flags & P_FSHARP))
    *p++ = _numeric[0];

  /* Create the rest of the mantissa */
  {
    register long rz = prec;

    for ( ; rz > 0 && *bp != '\0'; --rz)
      *p++ = *bp++;

    if (rz > 0) {
      otherlength = rzero = rz;
      flags |= P_RZERO;
    }
  }

  bp = &buf[0];

  /* Create the exponent */

  *(suffix = &expbuf[MAXESIZ]) = '\0';

  if (dval != 0) {

    register int nn = decpt - 1;

    if (nn < 0)
      nn = -nn;

    for ( ; nn > 9; nn /= 10)
      *--suffix = (char) todigit(nn % 10);

    *--suffix = (char) todigit(nn);
  }

  /* Prepend leading zeroes to the exponent */

  while (suffix > &expbuf[MAXESIZ - 2])
    *--suffix = '0';

  /* Put in the exponent sign */

  *--suffix = (char) ((decpt > 0 || dval == 0) ? '+' : '-');

  /* Put in the e */

  *--suffix = (char) (float_outchar [code]);

  /* compute size of suffix */

  otherlength += (suffixlength = (int)(&expbuf[MAXESIZ] - suffix));
  flags |= P_SUFFIX;

join:

  *p = '\0';
  bp = &buf[0];

nan_join:

  /* Calculate number of padding blanks */

  k = (n = p - bp) + prefixlength + otherlength;

  if (width <= k)
    count = k;

  else {

    count = width;

    /* Set up for padding zeroes if requested */
    /* Otherwise emit padding blanks unless output is */
    /* to be left-justified.  */

    if (flags & P_PADZERO) {

      if (!(flags & P_LZERO)) {
        flags |= P_LZERO;
        lzero = width - k;
      }

      else
        lzero += width - k;

      k = width; /* cancel padding blanks */
    }

    else
      /* Blanks on left if required */
      if (!(flags & P_FMINUS))
        PAD(_blanks, width - k);
  }

  /* Prefix, if any */

  if (prefixlength != 0)
    PUT(prefix, prefixlength);

  /* Zeroes on the left */

  if (flags & P_LZERO)
    PAD(_zeroes, lzero);

  /* The value itself */

  if (n > 0)
    PUT(bp, n);

  if (flags & (P_RZERO | P_SUFFIX | P_FMINUS)) {

    /* Zeroes on the right */

    if (flags & P_RZERO)
      PAD(_zeroes, rzero);

    /* The suffix */

    if (flags & P_SUFFIX)
      PUT(suffix, suffixlength);

    /* Blanks on the right if required */

    if (flags & P_FMINUS && width > k)
      PAD(_blanks, width - k);
  }

  DO_EXIT_ACTIONS();
} /* _doprnt_f */
