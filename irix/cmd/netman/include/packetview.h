#ifndef PACKETVIEW_H
#define PACKETVIEW_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Interface to packet display implementations.  The variations desired
 * include stdio, GUI, histogram-counting, and netlook protocol.
 *
 * Protocol decode operations may inspect pv_level to vary the level of
 * packet detail displayed.  If decoders use the pv_show*field functions,
 * protocol fields with level greater than pv_level will not be displayed.
 * Such decoders typically need not test pv_level.
 *
 * The pv_decodepacket function sets pv_stop to limit packet data decoding.
 * Any fields crossing or after this limit will be displayed with unknown
 * contents (???) by the pv_show*field functions.  If a protocol does its
 * own displaying with pv_printf, it should advance pv_off for each decoded
 * byte and test pv_off against pv_stop.
 */
#include "index.h"

struct snoopheader;
struct snooper;
struct protocol;
struct protofield;
struct protostack;
struct datastream;
struct tm;

#ifdef sun
#define __file_s _iobuf
#endif
struct __file_s;

/*
 * Well-known names for the first three verbosity levels.
 */
#define	PV_TERSE	0		/* show addresses or ports only */
#define	PV_DEFAULT	1		/* elide invariant/redundant fields */
#define	PV_VERBOSE	2		/* show every last decoded bit */

/*
 * Possible truth values for pv_nullflag, which disables protocol decoding,
 * optionally replacing it with hexdumping.
 */
#define	PV_NULLIFY	1		/* don't decode anything */
#define	PV_HEXIFY	2		/* hexdump protocol layers */
#define PV_NZIFY	3		

/*
 * Mode passed to pv_decodepacket, telling whether and what to dump in hex.
 */
#define	HEXDUMP_NONE	0		/* hexdump neither packet nor extra */
#define	HEXDUMP_PACKET	1		/* hexdump whole packet; don't decode */
#define	HEXDUMP_EXTRA	2		/* hexdump undecoded trailing data */

typedef struct packetview {
	void		*pv_private;	/* private data */
	struct pvops	*pv_ops;	/* operations on private data */
	char		*pv_name;	/* name for error reporting */
	int		pv_error;	/* error number if op failed */
	int		pv_level;	/* amount of decoding detail */
	int		pv_nullflag;	/* if true, nullify viewing */
	Index		pv_nullprotos;	/* protocols to nullify or hexify */
	int		pv_off;		/* current byte offset in packet */
	int		pv_bitoff;	/* bit offset in current byte */
	int		pv_stop;	/* limiting offset in packet */
	unsigned char	*pv_hexbase;	/* protocol data to hexify */
	int		pv_hexoff;	/* byte offset in packet for hexify */
	int		pv_hexcount;	/* residual byte count for hexify */
} PacketView;

/*
 * Packetview operations should set pv_error to an error number and return
 * zero if they fail.  They do not raise exceptions.
 */
struct pvops {
	int	(*pvo_head)(PacketView *, struct snoopheader *, struct tm *);
	int	(*pvo_tail)(PacketView *);
	int	(*pvo_push)(PacketView *, struct protocol *, char *, int,
			    char *);
	int	(*pvo_pop)(PacketView *);
	int	(*pvo_field)(PacketView *, void *, int, struct protofield *,
			     char *, int);
	int	(*pvo_break)(PacketView *);
	int	(*pvo_text)(PacketView *, char *, int);
	int	(*pvo_newline)(PacketView *);
	int	(*pvo_hexdump)(PacketView *, unsigned char *, int, int);
	void	(*pvo_destroy)(PacketView *);
};

/*
 * Subclass virtual operations.  These functions must be implemented
 * differently for each packetview variation.
 */
#define	pv_head(pv, sh, tm) \
	((*(pv)->pv_ops->pvo_head)(pv, sh, tm))
#define	pv_tail(pv) \
	((*(pv)->pv_ops->pvo_tail)(pv))
#define	pv_push(pv, pr, name, namlen, title) \
	((*(pv)->pv_ops->pvo_push)(pv, pr, name, namlen, title))
#define	pv_pop(pv) \
	((*(pv)->pv_ops->pvo_pop)(pv))
#define	pv_field(pv, base, size, pf, contents, contlen) \
	((*(pv)->pv_ops->pvo_field)(pv, base, size, pf, contents, contlen))
#define	pv_break(pv) \
	((*(pv)->pv_ops->pvo_break)(pv))
#define	pv_text(pv, buf, len) \
	((*(pv)->pv_ops->pvo_text)(pv, buf, len))
#define	pv_newline(pv) \
	((*(pv)->pv_ops->pvo_newline)(pv))
#define	pv_hexdump(pv, bp, off, len) \
	((*(pv)->pv_ops->pvo_hexdump)(pv, bp, off, len))
#define	pv_destroy(pv) \
	((*(pv)->pv_ops->pvo_destroy)(pv))

#define	pv_pushconst(pv, pr, name, title) \
	pv_push(pv, pr, name, constrlen(name), title)

#define	DefinePacketViewOperations(name,tag)				      \
static int makeident2(tag,_head)(PacketView*,struct snoopheader*,struct tm*); \
static int makeident2(tag,_tail)(PacketView*);				      \
static int makeident2(tag,_push)(PacketView*, struct protocol *, char *, int, \
				 char *);				      \
static int makeident2(tag,_pop)(PacketView*);				      \
static int makeident2(tag,_field)(PacketView *, void *, int,		      \
				  struct protofield *, char *, int);	      \
static int makeident2(tag,_break)(PacketView*);				      \
static int makeident2(tag,_text)(PacketView*, char*, int);		      \
static int makeident2(tag,_newline)(PacketView*);			      \
static int makeident2(tag,_hexdump)(PacketView*, unsigned char*, int, int);   \
static void makeident2(tag,_destroy)(PacketView*);			      \
static struct pvops name = {						      \
	makeident2(tag,_head),						      \
	makeident2(tag,_tail),						      \
	makeident2(tag,_push),						      \
	makeident2(tag,_pop),						      \
	makeident2(tag,_field),						      \
	makeident2(tag,_break),						      \
	makeident2(tag,_text),						      \
	makeident2(tag,_newline),					      \
	makeident2(tag,_hexdump),					      \
	makeident2(tag,_destroy),					      \
};

/*
 * Generic operations.  The int-valued functions return 1 on success and 0
 * on failure, with pv_error set on failure.  All of these functions raise
 * exceptions when pv_error gets set.
 */
void	pv_init(struct packetview *, void *, struct pvops *, char *, int);
void	pv_finish(struct packetview *);
void	pv_exception(struct packetview *, char *, ...);
int	pv_nullify(struct packetview *, char *);
int	pv_hexify(struct packetview *, char *);
int	pv_reify(struct packetview *, char *);
int	pv_decodepacket(struct packetview *, struct snooper *,
			struct snoopheader *, int, struct datastream *);
void	pv_decodeframe(struct packetview *, struct protocol *,
		       struct datastream *, struct protostack *);
int	pv_showfield(struct packetview *, struct protofield *, void *,
		     char *, ...);
int	pv_showvarfield(struct packetview *, struct protofield *, void *, int,
			char *, ...);
int	pv_replayfield(PacketView *, struct protofield *, void *, int, int,
		       char *, ...);
int	pv_printf(struct packetview *, char *, ...);
int	pv_showhex(struct packetview *, unsigned char *, int, int);
int	pv_decodehex(struct packetview *, struct datastream *, int, int);

/*
 * Packetview constructors.  A packetview implementation should ensure that
 * its constructor cannot fail.
 */
PacketView	*null_packetview();
PacketView	*stdio_packetview(struct __file_s *, char *, int);

/*
 * PIDL compiler runtime support.
 */
char	*_char_image(char);

#endif
