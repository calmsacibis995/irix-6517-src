#ifndef PACKETBUF_H
#define PACKETBUF_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * A circular snoop packet buffer.
 */
#include "snooper.h"

struct packetview;

struct pblinks {
	struct packet	*pbl_forw;	/* next packet in chain */
	struct packet	*pbl_back;	/* previous */
};

/*
 * Each element in the packetbuf ring begins with this structure.
 */
typedef struct packet {
	struct pblinks	p_pbl;		/* linkage */
	unsigned int	p_len;		/* packet bytes excluding snoop hdr */
	SnoopPacket	p_sp;		/* snoop packet */
} Packet;
#define	p_forw	p_pbl.pbl_forw
#define	p_back	p_pbl.pbl_back

typedef struct packetbuf {
	unsigned int	pb_packets;	/* logical size of ring */
	unsigned int	pb_packetsize;	/* byte size of ring element */
	struct pblinks	pb_busy;	/* list of packets which are "got" */
	struct pblinks	pb_free;	/* list of "put" packets */
	char		*pb_base;	/* base of ring */
} PacketBuf;

/*
 * Packetbuf operations.
 */
#define	PB_FULL(pb) \
	((pb)->pb_free.pbl_forw == (Packet *) &(pb)->pb_free)

void	pb_init(PacketBuf *, int, int);
void	pb_finish(PacketBuf *);
Packet	*pb_get(PacketBuf *);
Packet	*pb_put(PacketBuf *, Packet *);
void	pb_match(PacketBuf *, struct snooper *);
void	pb_decode(PacketBuf *, struct snooper *, int, struct packetview *);
void	pb_flush(PacketBuf *);
void	pb_scan(PacketBuf *, int (*)(PacketBuf *, Packet *, void *), void *);

/*
 * Arguments to the packetbuf match scanner.
 *
 * Ma_rawseq tracks the sequence number returned by the kernel raw domain.
 * Ma_seq tracks packets filtered by ma_snoop->sn_expr, but also follows
 * gaps in ma_rawseq to indicate potential matches which were dropped.
 */
enum seqflag { SEQ_RAW, SEQ_MATCH };

typedef struct matchargs {
	unsigned long	ma_rawseq;
	unsigned long	ma_seq;
	enum seqflag	ma_seqflag;
	struct snooper	*ma_snoop;
} MatchArgs;

#define	MA_INIT(ma, flag, sn) \
	((ma)->ma_rawseq = (ma)->ma_seq = 0, (ma)->ma_seqflag = (flag), \
	 (ma)->ma_snoop = (sn))

int	pb_matchpacket(PacketBuf *, Packet *, void *);

/*
 * Arguments to the packetbuf decode scanner.
 *
 * For each busy packet, pb_decodepacket calls pv_decodepacket to display
 * nested protocol frames, starting with da_snoop->sn_rawproto, on da_view.
 * Da_hex tells whether and what data to hexdump.
 */
typedef struct decodeargs {
	struct snooper		*da_snoop;
	int			da_hex;
	struct packetview	*da_view;
} DecodeArgs;

#define	DA_INIT(da, sn, hex, pv) \
	((da)->da_snoop = (sn), (da)->da_hex = (hex), (da)->da_view = (pv))

int	pb_decodepacket(PacketBuf *, Packet *, void *);

/*
 * Decode a packet given the snooper with which it was captured and a
 * packetview instance.  The packet should match the snooper's compiled
 * expression filter.
 */
int	p_decode(Packet *, struct snooper *, int, struct packetview *);

#endif
