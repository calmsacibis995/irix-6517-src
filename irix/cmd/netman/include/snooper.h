#ifndef SNOOPER_H
#define SNOOPER_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Packet capturer.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/raw.h>
#ifdef sun
#include <sys/time.h>
#endif

#ifndef INADDR_NONE
#define INADDR_NONE	0xffffffff
#endif

struct __xdr_s;
struct exprsource;
struct exprerror;

typedef struct snoopheader SnoopHeader;
typedef struct snoopfilter SnoopFilter;

/*
 * Snoop packet template (just enough data for debugging peeks).
 */
typedef struct snooppacket {
	SnoopHeader	sp_hdr;		/* snoop header */
	unsigned char	sp_data[16];	/* padded raw packet */
} SnoopPacket;

#define	sp_seq		sp_hdr.snoop_seq
#define	sp_flags	sp_hdr.snoop_flags
#define	sp_packetlen	sp_hdr.snoop_packetlen
#define	sp_timestamp	sp_hdr.snoop_timestamp

#define	newSnoopPacket(datasize) \
	xnew(SnoopPacket, (datasize) - sizeof(((SnoopPacket *)0)->sp_data))

/*
 * The base class for all snooper variations.
 */
typedef struct snooper {
	void		*sn_private;	/* private data pointer */
	struct snops	*sn_ops;	/* operations vector */
	char		*sn_name;	/* interface identifier */
	struct sockaddr	sn_ifaddr;	/* interface address */
	int		sn_file;	/* file descriptor */
	int		sn_error;	/* last system errno */
	struct protocol	*sn_rawproto;	/* raw protocol interface */
	int		sn_snooplen;	/* bytes of data to capture */
	int		sn_packetsize;	/* gross read buffer size */
	unsigned short	sn_rawhdrpad;	/* header alignment padding */
	unsigned short	sn_errflags;	/* error flags to match */
	struct expr	*sn_expr;	/* expression parse tree */
} Snooper;

/*
 * Enumerated types used by snooper operations.
 */
enum snshutdownhow { SNSHUTDOWN_READ=0, SNSHUTDOWN_WRITE=1, SNSHUTDOWN_RDWR=2 };

/*
 * Snooper virtual operations should set sn_error to an error number and
 * return a failure value (negative for read and write; zero for the rest)
 * upon error.  They do not raise exceptions.
 */
struct snops {
	int	(*sno_add)(Snooper *, struct expr **, struct exprerror *);
	int	(*sno_delete)(Snooper *);
	int	(*sno_read)(Snooper *, SnoopPacket *, int);
	int	(*sno_write)(Snooper *, SnoopPacket *, int);
	int	(*sno_ioctl)(Snooper *, int, void *);
	int	(*sno_getaddr)(Snooper *, int, struct sockaddr *);
	int	(*sno_shutdown)(Snooper *, enum snshutdownhow);
	void	(*sno_destroy)(Snooper *);
};

#define	sn_add(sn,exp,err)	((*(sn)->sn_ops->sno_add)(sn,exp,err))
#define	sn_delete(sn)		((*(sn)->sn_ops->sno_delete)(sn))
#define	sn_read(sn,sp,len)	((*(sn)->sn_ops->sno_read)(sn,sp,len))
#define	sn_write(sn,sp,len)	((*(sn)->sn_ops->sno_write)(sn,sp,len))
#define	sn_ioctl(sn,cmd,data)	((*(sn)->sn_ops->sno_ioctl)(sn,cmd,data))
#define	sn_getaddr(sn,cmd,sa)	((*(sn)->sn_ops->sno_getaddr)(sn,cmd,sa))
#define	sn_shutdown(sn,how)	((*(sn)->sn_ops->sno_shutdown)(sn,how))

#define	DefineSnooperOperations(name,tag)				    \
int	makeident2(tag,_add)(Snooper *, struct expr **, struct exprerror *);\
int	makeident2(tag,_delete)(Snooper *);				    \
int	makeident2(tag,_read)(Snooper *, SnoopPacket *, int);		    \
int	makeident2(tag,_write)(Snooper *, SnoopPacket *, int);		    \
int	makeident2(tag,_ioctl)(Snooper *, int, void *);			    \
int	makeident2(tag,_getaddr)(Snooper *, int, struct sockaddr *);	    \
int	makeident2(tag,_shutdown)(Snooper *, enum snshutdownhow);	    \
void	makeident2(tag,_destroy)(Snooper *);				    \
struct snops name = {							    \
	makeident2(tag,_add),						    \
	makeident2(tag,_delete),					    \
	makeident2(tag,_read),						    \
	makeident2(tag,_write),						    \
	makeident2(tag,_ioctl),						    \
	makeident2(tag,_getaddr),					    \
	makeident2(tag,_shutdown),					    \
	makeident2(tag,_destroy),					    \
};

/*
 * Macros which tailor pr_test and pr_eval for use with SnoopPackets,
 * and corresponding functions.
 */
#define	SN_TEST(sn, ex, sp, len) \
	((sn)->sn_rawproto ? \
	 pr_test((sn)->sn_rawproto, (ex), (sp), (len), (sn)) : 0)

#define	SN_EVAL(sn, ex, sp, len, err, rex) \
	((sn)->sn_rawproto ? \
	 pr_eval((sn)->sn_rawproto, (ex), (sp), (len), (sn), (err), (rex)) : 0)

int	sn_test(Snooper *, struct expr *, SnoopPacket *, int);
int	sn_eval(Snooper *, struct expr *, SnoopPacket *, int,
		struct exprerror *, struct expr *);

/*
 * Snooper types and corresponding constructors.  The constructors raise
 * exceptions and return null on error.  Call getsnoopertype as follows:
 *
 *	type = getsnoopertype(interface, &ifname);
 *
 * to extract the interface part (ifname) of a remote snooper name such
 * as host:ifname.  For local and trace snoopers, getsnoopertype returns
 * with ifname pointing to interface.
 */
enum snoopertype {
	ST_NULL,
	ST_TRACE,
	ST_LOCAL,
	ST_REMOTE
};

enum snoopertype getsnoopertype(char *, char **);

Snooper	*nullsnooper(struct protocol *);
Snooper	*tracesnooper(char *, char *, struct protocol *);
Snooper	*localsnooper(char *, int, int);
Snooper	*remotesnooper(char *, char *, struct timeval *, int, int, int);
Snooper	*sunsnooper(char *, int);

/*
 * Given a local network interface name, return its raw protocol or null.
 */
struct protocol	*getrawprotobyif(char *);

/*
 * Generic snooper operations.  All raise exceptions and return false
 * for system errors and invalid arguments.  In the event of a system
 * call error, sn_error will contain the Unix error number.
 */
int	sn_init(Snooper *, void *, struct snops *, char *, int,
		struct protocol *);
void	sn_destroy(Snooper *);
int	sn_compile(Snooper *, struct exprsource *, struct exprerror *);
int	sn_match(Snooper *, SnoopPacket *, int);
int	sn_setsnooplen(Snooper *, int);
int	sn_startcapture(Snooper *);
int	sn_stopcapture(Snooper *);
int	sn_seterrflags(Snooper *, int);
int	sn_getstats(Snooper *, struct snoopstats *);

/*
 * Snoop filter set and operations.
 */
struct sfset {
	unsigned short	sfs_errflags;
	unsigned short	sfs_elements;
	SnoopFilter	sfs_vec[SNOOP_MAXFILTERS];
};
#define	sfs_init(sfs)	bzero((char *) (sfs), sizeof *(sfs))

int		sfs_compile(struct sfset *, struct expr *, struct protocol *,
			    int, struct exprerror *);
SnoopFilter	*sfs_allocfilter(struct sfset *);
int		sfs_freefilter(struct sfset *, SnoopFilter *);
void		sfs_unifyfilters(struct sfset *);

/*
 * XDR routines for remote and tracefile snooping.
 */
typedef struct snooppacketwrap {
	SnoopPacket	*spw_sp;
	unsigned int	*spw_len;
	unsigned int	spw_maxlen;
} SnoopPacketWrap;

int	xdr_snooppacketwrap(struct __xdr_s *, SnoopPacketWrap *);
int	xdr_snooppacket(struct __xdr_s *, SnoopPacket *, unsigned int *,
			unsigned int);

/*
 * Snoop protocol and header flag name/value pairs.
 */
#if !defined(_LANGUAGE_C_PLUS_PLUS) || defined(_LANGUAGE_C_PLUS_PLUS_2_0)
extern struct protocol	 snoop_proto;
#endif
extern struct enumerator snoopflags[];
extern int		 numsnoopflags;

#endif
