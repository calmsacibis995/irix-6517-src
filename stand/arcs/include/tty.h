/*
 * tty.h - Structures for tty drivers
 *
 * $Revision: 1.1 $
 */
#ifndef _TTY_H
#define _TTY_H

/*
 * Character device buffer
 */
#define DBBUFSIZE	1024

struct device_buf {
	int db_flags;		/* character device flags */
	char *db_in;		/* pts at next free char */
	char *db_out;		/* pts at next filled char */
	char db_buf[DBBUFSIZE];	/* circular buffer for input */
};

/*
 * Character device flags
 */
#define	DB_RAW		0x1	/* don't interpret special chars */
#define	DB_STOPPED	0x2	/* stop output */
#define DB_RRAW		0x4	/* don't interpret any special chars at all */
				/* DB_RAW interprets ^C, ^S, and ^Q */
/*
 * Simple circular buffer functions
 */
#define	CIRC_EMPTY(x)	((x)->db_in == (x)->db_out)
#define	CIRC_FLUSH(x)	((x)->db_in = (x)->db_out = (x)->db_buf)
#define	CIRC_STOPPED(x)	((x)->db_flags & DB_STOPPED)

extern int	_circ_nread(struct device_buf *);
extern void	_circ_putc(int c, struct device_buf *);
extern int	_circ_getc(struct device_buf *);
extern void	_ttyinput(struct device_buf *, char);
#endif
