/*
 *  bufview - display file sytem buffer cache
 *
 *  General (global) definitions
 */

/* Maximum number of columns allowed for display */
#define MAX_COLS	128

/* Special atoi routine returns either a non-negative number or one of: */
#define Infinity	-1
#define Invalid		-2

/* maximum number we can have */
#define Largest		0x7fffffff

/*
 *  "Nominal_TOPN" is used as the default TOPN when Default_TOPN is Infinity
 *  and the output is a dumb terminal.  If we didn't do this, then
 *  installations who use a default TOPN of Infinity will get every
 *  process in the system when running top on a dumb terminal (or redirected
 *  to a file).  Note that Nominal_TOPN is a default:  it can still be
 *  overridden on the command line, even with the value "infinity".
 */
#ifndef Nominal_TOPN
#define Nominal_TOPN	40
#endif

#ifndef Default_TOPN
#define Default_TOPN	-1
#endif

#ifndef Default_DELAY
#define Default_DELAY	3
#endif
