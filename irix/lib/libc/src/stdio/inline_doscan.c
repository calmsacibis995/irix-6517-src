/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.6 $"

#include "synonyms.h"
#include <mplib.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "_locale.h"
#include "priv_extern.h"

#define MAXDIGITS 17

/* This file contains a set of routines to perform the actions required
 * in scanf for the individual conversion characters.
 *
 * The routines are
 *
 *   _doscan_ws    : for matching with white space
 *   _doscan_match : for matching with aritrary strings
 *   _doscan_c     : for scanning a character ( %c )
 *   _doscan_s     : for scanning a string    ( %s )
 *   _doscan_i     : for scanning integers    ( %o, %i, %d, %p, %x, %X )
 *   _doscan_f     : for scanning floats      ( %f, %e, %E, %g, %G )
 *
 * The arguments to these routines are of the form
 *
 *   routine ( state, flags, src, value, arg1, arg2, arg3 )
 *
 *   where,
 *
 *     state   : keeps track of whether a mismatch occurred or EOF
 *               was encountered either in this or an earlier call
 *               while processing the format string.
 *
 *     flags   : passes information regarding the item to be formatted
 *               this includes the following
 * 
 *               routine being invoked (scanf,fscanf,sscanf)
 *               whether it is the first item (for locking)
 *               whether it is the last item  (for unlocking)
 *               whether the number of items scanned is needed
 *               whether the number of characters scanned is needed (%n)
 *               whether a width was specified
 *               whether a * was specified to suppress assignment
 *               the type of the field being assigned to
 *               the format operation for integer/float conversion
 * 
 *    src      : either the address of the address of the buffer for sscanf
 *               or the address of the file descriptor for scanf, fscanf
 * 
 *    value    : address of the item being assigned to.
 * 
 *    arg1,    : optional arguments to specify the address of item_count,
 *    arg2,      width of the field or the address of the char_count.
 *    arg3       These are extracted based on the values in the flags field.
 *
 */

/* values in state */

#define S_MISMATCH            1     /* mismatch occured during processing */
#define S_EOF                 2     /* enf of file encountered */

/* values in flags */

#define S_FORMAT_MASK         3     /* mask to get at format routine */
#define S_FIRST_ITEM          4     /* first item being formatted */
#define S_LAST_ITEM           8     /* last item being formatted */
#define S_ITEM_COUNT       0x10     /* need item count */
#define S_CHAR_COUNT       0x20     /* need char count */
#define S_WIDTH            0x40     /* has width */
#define S_SUPPRESS         0x80     /* suppress assignment */
#define S_DOTSEEN         0x100     /* dot appeared in format specification */
#define S_SHORT           0x200     /* h */
#define S_LENGTH          0x400     /* l */
#define S_LLONG           0x800     /* ll */
#define S_LDOUBLE        0x1000     /* L */
#define S_INTEGER_MASK  0x70000     /* 'o', 'd', 'u', 'x', 'X', 'p', 'i' */

/* name of the format routine */

typedef enum {
  SSCANF, FSCANF, SCANF
} FORMAT;

/* macro to get at the format routine */

#define GET_FORMAT(x) (FORMAT)(x & S_FORMAT_MASK)

/* converion operators for integers */

typedef enum {
  percent_no_i, percent_o, percent_d, percent_u,
  percent_p,    percent_x, percent_X, percent_i
} INTEGER_CODE;

/* macro to get converion operators from flags */

#define GET_INTEGER_CODE(x) (INTEGER_CODE)((x & S_INTEGER_MASK) >> 16)

/* bases for the integer operators */

static const int integer_base [] = { 0, 8, 10, 10, 16, 16, 16, 10 };

/* converion operators for integers */

typedef enum {
  percent_no_f, percent_f, percent_E, percent_e,
  percent_G,    percent_g
} FLOAT_CODE;

static int readchar (FILE *, int *);

/* macro to get next character from source */

#define GET() ( \
  chcount ++, \
  format == SSCANF \
    ? ((*bufptr == '\0') ? EOF : *bufptr++) \
    : getc(file) ) 
  
/* macro to unget next character from source */

#define UNGET(x) ( \
  chcount--, \
  (x == EOF) \
    ? EOF \
    : (format == SSCANF) \
      ? *(--bufptr) \
      : (++file->_cnt, *(--file->_ptr) ) )

/* macro to get next character from source.
 * Similar to GET except that it does not read 
 * the next buffer if end of buffer is met.
 */

#define READCHAR() ( \
  format == SSCANF \
    ? GET() \
    : readchar (file, &chcount) \
)


/* Common set of actions performed on entry.
 * These include initializing bufptr, file descriptor,
 * getting the address of item_count field if specified
 * locking of file if it is first item.
 */

#define DO_ENTRY_ACTIONS() { \
  format = GET_FORMAT(flags); \
  item_count = ( flags & S_ITEM_COUNT ) ? nargs++, ( int * ) arg1 : NULL; \
 \
  if ( format == SSCANF ) { \
    pbuf   = (char **) src; \
    bufptr = *pbuf; \
  } \
 \
  else { \
    file   = (FILE *) src; \
    if ( ! ( file->_flag & ( _IOREAD | _IORW ) ) ) { \
      setoserror (EBADF); \
      if ( item_count ) \
        *item_count = EOF; \
      if ( state ) \
        *state |= S_EOF; \
      return; \
    } \
    if ( flags & S_FIRST_ITEM ) { \
      LOCKINIT(lock_val, LOCKFILE(file)); \
    } \
  } \
} /* DO_ENTRY_ACTIONS */


/* Compute the width if specified */

#define GET_WIDTH(x) \
  ( flags & S_WIDTH ) ? nargs++, ( nargs == 2) ? arg2 : arg1 : x;


/* Get the address of char_count field if specified */

#define GET_CHAR_COUNT() \
  ( flags & S_CHAR_COUNT ) \
    ? nargs++, \
      ( nargs == 1 ) ? ( int * ) arg1 \
                     : ( nargs == 2 ) ? ( int * ) arg2 : ( int * ) arg3 \
    : NULL; 

/* Common set of actions performed on exit.
 * These include updating the buffer pointer for sscanf,
 * and unlocking for scanf, fscanf.
 */

#define DO_EXIT_ACTIONS() { \
  if ( format == SSCANF ) \
    *pbuf = bufptr; \
 \
  else { \
    if ( char_count ) \
      *char_count += chcount; \
    if ( flags & S_LAST_ITEM ) \
      UNLOCKFILE(file, lock_val); \
  } \
} /* DO_EXIT_ACTIONS */


static int
readchar ( FILE * file, int * chcount )
{
  int inchar;
  int count = *chcount;

  if ( file->_cnt != 0 )
    inchar = getc(file);

  else {

    if ( read ( fileno(file), &inchar, 1 ) != 1 )
      return ( EOF );

    count++;
  }

  *chcount = count;
  return (inchar);
} /* readchar */


/* match for a white space */

void
_doscan_ws ( int * state, int flags, void * src,
             long arg1, long arg2, long arg3 )
{
  FORMAT    format;
  char   ** pbuf;
  char    * bufptr;
  FILE    * file;
  int       ch;
  int       nargs = 0;
  int     * item_count;
  int     * char_count;
  int       chcount;
  LOCKDECL(lock_val);

  if ( state && *state )
    return;

  DO_ENTRY_ACTIONS();

  char_count = GET_CHAR_COUNT();
  chcount    = 0;
  while ( isspace ( ch = GET() ) ) {}

  ch = UNGET(ch);

  DO_EXIT_ACTIONS();
} /* _doscan_ws */


/* match arbitrary string */

void
_doscan_match ( int * state, int flags, void * src, char * cptr,
                long arg1, long arg2, long arg3 )
{
  FORMAT    format;
  char   ** pbuf;
  char    * bufptr;
  FILE    * file;
  int       ch;
  int       chcount;
  int       nargs = 0;
  int     * item_count;
  int     * char_count;
  LOCKDECL(lock_val);

  DO_ENTRY_ACTIONS();

  char_count = GET_CHAR_COUNT();
  chcount    = 0;
  while ( ( ch = GET() ) == *cptr++ ) {}

  ch = UNGET(ch);

  if ( state && *cptr )
    *state |= S_MISMATCH;

  DO_EXIT_ACTIONS();
} /* _doscan_match */


/* scan for a character */

void
_doscan_c ( int * state, int flags, void * src, char * cptr,
            long arg1, long arg2, long arg3 )
{
  int       chcount;
  char   ** pbuf;
  char    * bufptr;
  FILE    * file;
  FORMAT    format;
  long      width;
  int       suppress;
  int       ch;
  char    * start;
  int       nargs = 0;
  int     * item_count;
  int     * char_count;
  LOCKDECL(lock_val);

  DO_ENTRY_ACTIONS();

  width      = GET_WIDTH(1);
  char_count = GET_CHAR_COUNT();
  suppress   = flags & S_SUPPRESS;
  chcount    = 0;
  start      = cptr;

  if ( suppress ) {
    while ( ch = GET() != EOF ) {
      if ( --width <= 0 )
        break;
    }
  }

  else {

    while ( (ch = GET()) != EOF ) {
      *cptr++ = ch;
      if ( --width <= 0 )
        break;
    }
  }

  if ( width ) {
    UNGET(ch);
  }

  if ( cptr == start ) {
    if ( state )
      *state |= S_MISMATCH;
  }

  else
  if ( item_count && ! suppress )
    *item_count += 1;

  DO_EXIT_ACTIONS();
} /* _doscan_c */


/* scan a whitespace terminated string */

void
_doscan_s ( int * state, int flags, void * src, char * cptr,
            long arg1, long arg2, long arg3 )
{
  int       chcount;
  char   ** pbuf;
  char    * bufptr;
  FILE    * file;
  FORMAT    format;
  long      width;
  int       suppress;
  int       ch;
  char    * start;
  int       nargs = 0;
  int     * item_count;
  int     * char_count;
  LOCKDECL(lock_val);

  DO_ENTRY_ACTIONS();

  width      = GET_WIDTH(MAXINT);
  char_count = GET_CHAR_COUNT();
  suppress   = flags & S_SUPPRESS;
  chcount    = 0;
  start      = cptr;

  if ( suppress ) {
    while ( ch = GET() != EOF ) {
      if ( --width <= 0 )
        break;
    }
  }

  else {
    while ( (ch = GET()) != EOF && ! isspace(ch) ) {
      *cptr++ = ch;
      if ( --width <= 0 )
        break;
    }
  }

  if ( width ) {
    UNGET(ch);
  }

  if ( cptr == start ) {
    if ( state )
      *state |= S_MISMATCH;
  }

  else
  if ( item_count && ! suppress ) {
    *item_count += 1;
    *cptr = '\0';
  }

  DO_EXIT_ACTIONS();
} /* _doscan_s */


/* scan an integer */

void
_doscan_i ( int * state, int flags, void * src, void * value,
            long arg1, long arg2, long arg3 )
{
  char         ** pbuf;
  char          * bufptr;
  FILE          * file;
  int             nargs = 0;
  int           * item_count;
  long            width;
  int           * char_count;
  FORMAT          format;
  INTEGER_CODE    code;
  int             suppress;

  int             chcount;
  int             ch;
  int             inchar;
  int             lookahead;
  long            number;
  int             negate;
  int             digits;
  int             base;
  LOCKDECL(lock_val);

  DO_ENTRY_ACTIONS();

  width      = GET_WIDTH(MAXINT);
  char_count = GET_CHAR_COUNT();
  suppress   = flags & S_SUPPRESS;
  code       = GET_INTEGER_CODE(flags);
  base       = integer_base [code];
  chcount    = 0;
  number     = 0; 
  digits     = 0;
  negate     = 0;

  while ( ch = GET(), isspace(ch) ) {}

  switch ( ch ) {

    case '-':

      negate = 1;
      /* FALL THROUGH */

    case '+':

      if ( --width <= 0 )
        break;

      if ( (  ch = GET() ) != '0' )
        break;

      /* FALL THROUGH */

    case '0':

      if ( ( code < percent_x ) || ( width <= 1 ) )
        break;

      if ( ( ( inchar = GET() ) == 'x' ) || ( inchar == 'X' ) ) {

        lookahead = readchar ( file, &chcount );

        if ( isxdigit(lookahead) ) {

          base = 16;

          if ( width <= 2 ) {

            UNGET(lookahead);
            width -= 1;       /* Take into account the 'x' */
          }

          else {

            ch = lookahead;
            width -= 2;       /* Take into account '0x' */
          }
        }

        else {

          UNGET(lookahead);
          UNGET(inchar);
        }
      }

      else {

        UNGET(inchar);
        if ( code == percent_i )
          base = 8;
      }
      break;
  }

  if ( base == 10 ) {
    for ( ; --width >= 0 && ch >= '0' && ch <= '9'; ) {
      number = number * 10 + ch - '0';
      if ( ++digits > 62 ) {
        setoserror(ERANGE);
/*
        return (0);
*/
      }
      ch = GET();
    }
  }

  else
  if ( base == 16 ) {
    for ( ; --width >= 0 && isxdigit(ch); ) {
      int digit;
      digit = ch - ( isdigit(ch) ? '0' : isupper(ch) ? 'A' - 10 : 'a' - 10 );
      number = number * 16 + digit;
      if ( ++digits > 62 ) {
        setoserror(ERANGE);
/*
        return (0);
*/
      }
      ch = GET();
    }
  }

  else {
    for ( ; --width >= 0 && ch >= '0' && ch <= '7'; ) {
      number = number * 8 + ch - '0';
      if ( ++digits > 62 ) {
        setoserror(ERANGE);
/*
        return (0);
*/
      }
      ch = GET();
    }
  }

  if ( digits && ! suppress ) {

    if ( negate )
      number = 0 - number;

    if ( flags & S_LENGTH )
      *(long  *) value = number;

    else
    if ( flags & S_SHORT )

      *(short *) value = (short)number;

    else
      *(int   *) value = (int)number;

    if ( item_count )
      *item_count += 1;
  }

  if ( state && digits == 0 )
    *state |= S_MISMATCH;

  DO_EXIT_ACTIONS();
} /* doscan_i */


/* scan a floating point number */

void
_doscan_f ( int * state, int flags, void * src, void * value,
            long arg1, long arg2, long arg3 )
{
  char   ** pbuf;
  char    * bufptr;
  FILE    * file;
  int       nargs = 0;
  int     * item_count;
  long      width;
  int     * char_count;

  FORMAT    format;
  int       suppress;

  int       chcount;
  int       ch;
  int       inchar;
  int       lookahead;
  int       negate;
  int       decpoint;
  char    * d;
  int       exp;
  double    x;
  int       dpchar;
  char      digits [MAXDIGITS];
  int       do_store;
  LOCKDECL(lock_val);

  DO_ENTRY_ACTIONS();

  width      = GET_WIDTH(MAXINT);
  char_count = GET_CHAR_COUNT();
  suppress   = flags & S_SUPPRESS;
  chcount    = 0;
  d          = digits;
  dpchar     = _numeric[0];
  decpoint   = 0;
  exp        = 0;
  do_store   = 0;
  negate     = 0;

  while ( ch = GET(), isspace(ch) ) {}

  /* process sign */

  switch ( ch ) {

    case '-':

      negate = 1;
      /* FALL THROUGH */

    case '+':

      ch = GET();
      --width;
      break;
  }

  while (width > 0) {

    if ( ch >= '0' && ch <= '9' ) {

      if ( d == digits + MAXDIGITS ) {
        /* ignore more than 17 digits, but adjust exponent */
        exp += ( decpoint ^ 1 );
      }

      else {

        if ( ch == '0' && d == digits ) {
          /* ignore leading zeros */
          do_store++;
        }

        else {
          *d++ = ch - '0';
        }

        exp -= decpoint;
      }
    }

    else
    if ( ch == (unsigned int) dpchar && ! decpoint ) {
      /* INTERNATIONAL decimal point */
      decpoint = 1;
    }

    else {
      break;
    }

    ch = GET();
    width--;
  }

  if ( width <= 0 )
    goto convert;

  if ( ch == 'e' || ch == 'E' ) {

    int negate_exp = 0;
    int e = 0;

    if ( --width <= 0 )
      goto convert;

    inchar = READCHAR();

    if ( isdigit(inchar) ) {
      ch = inchar;
    }

    else
    if (    inchar == '+' || inchar == '-'
         || ( isspace(inchar) /* && ( _lib_version == c_issue_4 ) */ ) ) {

      if ( width - 2 < 0 ) {
        goto convert;
      }

      --width;
      lookahead = READCHAR();

      if ( isdigit(lookahead) ) {
        ch = lookahead;
        negate_exp = inchar == '-';
      }
      else {
        UNGET(lookahead);
        UNGET(inchar);
        goto convert;
      }
    }

    else {
      UNGET(inchar);
      goto convert;
    }

    do {
      if ( e <= 340 )
        e = e * 10 + ch - '0';
      ch = GET();
      if ( --width <= 0 )
        break;
    } while ( ch >= '0' && ch <= '9');

    if ( negate_exp )
      e = -e;

    if ( e < -340 || e > 340 )
      exp = e;

    else
      exp += e;
  }

convert:

  UNGET(ch);

  if ( d == digits )
    x = 0.0;

  else {

    if ( exp < -340 ) {
      x = 0;
      setoserror(ERANGE);
    }

    else
    if ( exp > 308 ) {
/*
      x = HUGE_VAL;
*/
      setoserror(ERANGE);
    }

    else {
      /* let _atod diagnose under- and over-flows.
       * If the input was == 0.0, we have already computed x,
       * so retval of +-Inf signals OVERFLOW, 0.0 UNDERFLOW.
       */
      x = _atod (digits, (int)(d - digits), exp );

      if (    ( x == 0.0 )
           || ( x == HUGE_VAL ) )
        setoserror(ERANGE);
    }

    if ( negate )
      x = -x;
  }

  if ( ! chcount ) {
    if ( state ) 
      *state |= S_MISMATCH;
  }
  
  else {
    if ( ! suppress ) {
      if ( flags & S_LENGTH )
        *(double *) value = x;
      else
        *(float *)  value = x;

      if ( item_count )
        *item_count += 1;
    }
  }

  DO_EXIT_ACTIONS();
} /* _doscan_f */
