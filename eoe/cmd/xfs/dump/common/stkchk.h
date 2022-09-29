#ifndef STKCHK_H
#define STKCHK_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/stkchk.h,v 1.1 1995/06/30 02:10:49 cbullis Exp $"

/* initializes the abstraction. must tell it how many pids to expect
 */
extern void stkchk_init( size_t maxpidcnt );

/* each process which wants to make use of stack checking must register
 * upon entry (main() or sproc entrypt).
 */
extern void stkchk_register( void );

/* called to validate the current stack pointer.
 * returns 0 if ok, 1 if close, 2 if over, 3 if under.
 */
extern int stkchk( void );

#endif /* STKCHK_H */
