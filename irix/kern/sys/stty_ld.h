/* streams tty interface
 *	definitions for the streams 'line discipline'
 *
 * $Revision: 3.14 $
 */

#ifndef __STTY_LD_H__
#define __STTY_LD_H__

#include "sys/termio.h"

/* view an IOCTL message as a termio structure */
#define STERMIO(bp) ((struct termio*)(bp)->b_cont->b_rptr)


/* one of these is created for each stream
 */
struct stty_ld {
	struct termio st_termio;	/* modes and such		*/
#define st_iflag st_termio.c_iflag	/* input modes			*/
#define st_oflag st_termio.c_oflag	/* output modes			*/
#define st_cflag st_termio.c_cflag	/* 'control' modes		*/
#define st_lflag st_termio.c_lflag	/* line discipline modes	*/
#define st_ospeed st_termio.c_ospeed	/* output speed			*/
#define st_ispeed st_termio.c_ispeed	/* input speed			*/
#define	st_line  st_termio.c_line	/* line discipline		*/
#define	st_cc	 st_termio.c_cc		/* control chars		*/

	u_char	st_rrunning;		/* input busy			*/

	uint	st_state;		/* current state		*/
	toid_t	st_tid;			/* input timer ID		*/
	uint	st_ocol;		/* current output 'column'	*/
	uint	st_ncol;		/* future current column	*/
	uint	st_xcol;		/* current 'interfering' column	*/
	uint	st_bcol;		/* 'base' of input line		*/

	uint	st_llen;		/* text waiting in lines	*/
	uint	st_ahead;		/* waiting text non-canonical	*/

	queue_t *st_rq;			/* our read queue		*/
	queue_t *st_wq;			/* our write queue		*/
	mblk_t	*st_imsg, *st_ibp;	/* input line			*/
	mblk_t	*st_lmsg, *st_lbp;	/* typed-ahead canonical lines	*/
	mblk_t	*st_emsg, *st_ebp;	/* current echos		*/
};

/* states */
#define ST_LIT		0x0001		/* have seen literal character	*/
#define ST_ESC		0x0002		/* have seen backslash		*/
#define ST_TIMING	0x0008		/* timer is running		*/
#define ST_TIMED	0x0010		/* timer has already run	*/
#define ST_TABWAIT	0x0020		/* waiting to echo HT delete	*/
#define ST_BCOL_BAD	0x0040		/* base column # is wrong	*/
#define ST_BCOL_DELAY	0x0080		/* mark column # bad after this */
#define ST_ISTTY	0x0100		/* told stream head about tty	*/
#define ST_INRAW	0x0200		/* input is very raw		*/
#define ST_INPASS	0x0400		/* pass input upstream fast	*/
#define ST_NOCANON	0x0800		/* don't do canonicalization    */
#define ST_MAXBEL	0x1000		/* doing imaxbel                */
#define ST_ECHOPRT	0x2000		/* doing echoprt                */
#define ST_CLOSE_DRAIN	0x4000		/* draining output on close     */

#endif
