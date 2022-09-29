/*
 * tty.c - tty driver support functions
 *
 * These routines maintain a circular buffer for incoming characters that
 * is used to connect to serial driver polling function to the upper level
 * read requests.  It also implements some primitive line discipline
 * functionality like ^C and Xon/Xoff.
 *
 * $Revision: 1.9 $
 *
 */
#include <arcs/types.h>
#include <arcs/signal.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>
#include <tty.h>

#define	CTRL(x)		((x)&0x1f)	/* use CTRL('a') for ^A */

static int (*intr_func)(char);

int psignal(int);

extern char *_intr_char;

static char *_def_intr_char = "\003";	/* default interrupt ^C */

int ignore_xoff;		/* set during autoboots */
int ignore_intr;		/* set by ioctl */

static int
generic_intr_func(c)
register char c;
{
	char *pp;

	if (_intr_char) {
	    for (pp = _intr_char; *pp ; ++pp)
		if ((c & 0xff) == *pp) {

			/*
			 * Handle cntrl-c signal. psignal calls appropriate
			 * signal handler. If someone catches sig and has set
			 * up a setjmp/longjmp, then won't return. Only 
			 * returns if signal was ignored or someone caught it 
			 * and didn't do a setjmp/longjmp.
			 *
			 * If we get an escape as an interrupt character, also
			 * send SIGHUP, which is ignored by default.
			 */

			if (c == '\033')
				psignal (SIGHUP);
			psignal (SIGINT);
			return (1);
		}
	}
	return (0);
}

/*
 * _ttyinput -- called by char driver scandev ioctl routines to deal with
 * circ_buf and special characters
 */
void
_ttyinput(struct device_buf *db, char c)
{
	extern int symmon;

	/*
	 * Check if we are running in REAL raw mode 
	 * (no input processing at all)
	 */
	if (!(db->db_flags & DB_RRAW)) {
		if ((c & 0xff) == CTRL('S')) {
			if (!ignore_xoff)
				db->db_flags |= DB_STOPPED;
			return;
		}
		db->db_flags &= ~DB_STOPPED;
		if ((c & 0xff) == CTRL('Q'))
			return;

		if ((*intr_func)(c))
			return;

		if (c && (db->db_flags & DB_RAW) == 0) {
			if ((c & 0xff) == CTRL('D'))
				EnterInteractiveMode ();
			if (((c & 0xff) == CTRL('A')) &&
			    (SPB->DebugBlock && !symmon)) {
				debug("ring");
				return;
			}
			/*
		 	* duplicate this stuff here so that in cooked
		 	* mode they aren't sensitive to the parity bit
		 	*/
			if ((c & 0xff) == CTRL('S')) {
				if (!ignore_xoff)
					db->db_flags |= DB_STOPPED;
				return;
			}
			db->db_flags &= ~DB_STOPPED;
			if ((c & 0xff) == CTRL('Q'))
				return;

			if ((*intr_func)(c))
				return;
		}
	}
	_circ_putc(c, db);
}

void
_intr_init(void)
{
 	_intr_char = _def_intr_char;	/* set default console intr char */

	if (intr_func == NULL)
		intr_func = generic_intr_func;
}

/*
 * _circ_getc -- remove character from circular buffer
 */
int
_circ_getc(struct device_buf *db)
{
	int c;

	if (CIRC_EMPTY(db))	/* should always check before calling */
		panic("console getc error");

	c = *db->db_out++ & 0xff;
	if (db->db_out >= &db->db_buf[sizeof(db->db_buf)])
		db->db_out = db->db_buf;
	return (c);
}

/*
 * _circ_putc -- insert character in circular buffer
 */
void
_circ_putc(int c, struct device_buf *db)
{
	char *cp;
	extern int Verbose;

	cp = db->db_in + 1 >= &db->db_buf[sizeof(db->db_buf)]
	    ? db->db_buf
	    : db->db_in + 1;

	/*
	 * if buffer is full, ignore the character
	 */
	if (cp == db->db_out) {
		if (Verbose)
			_errputs("\ndropped char\n");
		return;
	}

	*db->db_in = c;
	db->db_in = cp;
}

/*
 * _circ_nread - return number of characters in circular buffer
 */
int
_circ_nread(struct device_buf *db)
{
	int size = (int)((long)db->db_in - (long)db->db_out);

	if (size < 0)
		return size + sizeof(db->db_buf);
	else
		return size;
}
