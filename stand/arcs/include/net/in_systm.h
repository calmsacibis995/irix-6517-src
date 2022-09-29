#ident	"include/net/in_systm.h:  $Revision: 1.3 $"

/*
 * Miscellaneous internetwork
 * definitions for kernel.
 */

/*
 * Network types.
 *
 * Internally the system keeps counters in the headers with the bytes
 * swapped so that VAX instructions will work on them.  It reverses
 * the bytes before transmission at each protocol level.  The n_ types
 * represent the types with the bytes in ``high-ender'' order.
 */
typedef u_short n_short;		/* short as received from the net */
typedef u_int	n_long;			/* long as received from the net */

typedef	u_int	n_time;			/* ms since 00:00 GMT, byte rev */
