/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_filter.c,v 1.2 1996/07/06 17:50:04 nn Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <rpc/rpc.h>
#ifndef sgi
#include <rpc/rpcent.h>
#endif

#include <sys/dlpi.h>
#include <snoop.h>

int eaddr;	/* need ethernet addr */

int opstack;	/* operand stack depth */

/*
 * These are the operators of the user-level filter.
 * STOP ends execution of the filter expression and
 * returns the truth value at the top of the stack.
 * OP_LOAD_OCTET, OP_LOAD_SHORT and OP_LOAD_LONG pop
 * an offset value from the stack and load a value of
 * an appropriate size from the packet (octet, short or
 * long).  The offset is computed from a base value that
 * may be set via the OP_OFFSET operators.
 * OP_EQ, OP_NE, OP_GT, OP_GE, OP_LT, OP_LE pop two values
 * from the stack and return the result of their comparison.
 * OP_AND, OP_OR, OP_XOR pop two values from the stack and
 * do perform a bitwise operation on them - returning a result
 * to the stack.  OP_NOT inverts the bits of the value on the
 * stack.
 * OP_BRFL and OP_BRTR branch to an offset in the code array
 * depending on the value at the top of the stack: true (not 0)
 * or false (0).
 * OP_ADD, OP_SUB, OP_MUL, OP_DIV and OP_REM pop two values
 * from the stack and perform arithmetic.
 * The OP_OFFSET operators change the base from which the
 * OP_LOAD operators compute their offsets.
 * OP_OFFSET_ZERO sets the offset to zero - beginning of packet.
 * OP_OFFSET_ETHER sets the base to the first octet after
 * the ethernet (DLC) header.  OP_OFFSET_IP, OP_OFFSET_TCP,
 * and OP_OFFSET_UDP do the same for those headers - they
 * set the offset base to the *end* of the header - not the
 * beginning.  The OP_OFFSET_RPC operator is a bit unusual.
 * It points the base at the cached RPC header.  For the
 * purposes of selection, RPC reply headers look like call
 * headers except for the direction value.
 * OP_OFFSET_POP restores the offset base to the value prior
 * to the most recent OP_OFFSET call.
 */
enum optype {
	OP_STOP = 0,
	OP_LOAD_OCTET,
	OP_LOAD_SHORT,
	OP_LOAD_LONG,
	OP_LOAD_CONST,
	OP_LOAD_LENGTH,
	OP_EQ,
	OP_NE,
	OP_GT,
	OP_GE,
	OP_LT,
	OP_LE,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_NOT,
	OP_BRFL,
	OP_BRTR,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_REM,
	OP_OFFSET_POP,
	OP_OFFSET_ZERO,
	OP_OFFSET_ETHER,
	OP_OFFSET_IP,
	OP_OFFSET_TCP,
	OP_OFFSET_UDP,
	OP_OFFSET_RPC,
	OP_LAST
};

char *opnames[] = {
	"STOP",
	"LOAD_OCTET",
	"LOAD_SHORT",
	"LOAD_LONG",
	"LOAD_CONST",
	"LOAD_LENGTH",
	"EQ",
	"NE",
	"GT",
	"GE",
	"LT",
	"LE",
	"AND",
	"OR",
	"XOR",
	"NOT",
	"BRFL",
	"BRTR",
	"ADD",
	"SUB",
	"MUL",
	"DIV",
	"REM",
	"OFFSET_POP",
	"OFFSET_ZERO",
	"OFFSET_ETHER",
	"OFFSET_IP",
	"OFFSET_TCP",
	"OFFSET_UDP",
	"OFFSET_RPC",
	""
};

#define	MAXOPS 1024
u_int oplist[MAXOPS];	/* array of operators */
u_int *curr_op;		/* last op generated */

void emitval(u_int x);
void expression(void);
void emitop(enum optype x);

void
codeprint()
{
	u_int *op;

	for (op = oplist; *op; op++) {
		if (*op <= OP_LAST)
			printf("\t%2d: %s\n", op - oplist, opnames[*op]);
		else
			printf("\t%2d: (%d)\n", op - oplist, *op);

		switch (*op) {
		case OP_LOAD_CONST:
		case OP_BRTR:
		case OP_BRFL:
			op++;
			if ((int) *op < 0)
				printf("\t%2d:   0x%08x\n", op - oplist, *op);
			else
				printf("\t%2d:   %d\n", op - oplist, *op);
		}
	}
	printf("\t%2d: STOP\n", op - oplist);
	printf("\n");
}


/*
 * Take a pass through the generated code and optimize
 * branches.  A branch true (BRTR) that has another BRTR
 * at its destination can use the address of the destination
 * BRTR.  A BRTR that points to a BRFL (branch false) should
 * point to the address following the BRFL.
 * A similar optimization applies to BRFL operators.
 */
void
optimize(oplist)
	u_int *oplist;
{
	u_int *op;

	for (op = oplist; *op; op++) {
		switch (*op) {
		case OP_LOAD_CONST:
			op++;
			break;
		case OP_BRTR:
			op++;
			optimize(&oplist[*op]);
			if (oplist[*op] == OP_BRFL)
				*op += 2;
			else if (oplist[*op] == OP_BRTR)
				*op = oplist[*op + 1];
			break;
		case OP_BRFL:
			op++;
			optimize(&oplist[*op]);
			if (oplist[*op] == OP_BRTR)
				*op += 2;
			else if (oplist[*op] == OP_BRFL)
				*op = oplist[*op + 1];
			break;
		}
	}
}

/*
 * RPC packets are tough to filter.
 * While the call packet has all the interesting
 * info: program number, version, procedure etc,
 * the reply packet has none of this information.
 * If we want to do useful filtering based on this
 * information then we have to stash the information
 * from the call packet, and use the XID in the reply
 * to find the stashed info.  The stashed info is
 * kept in a circular lifo, assuming that a call packet
 * will be followed quickly by its reply.
 */

struct xid_entry {
	unsigned	x_xid;		/* The XID (32 bits) */
	unsigned	x_dir;		/* CALL or REPLY */
	unsigned	x_rpcvers;	/* Protocol version (2) */
	unsigned	x_prog;		/* RPC program number */
	unsigned	x_vers;		/* RPC version number */
	unsigned	x_proc;		/* RPC procedure number */
};
struct xid_entry	xe_table[XID_CACHE_SIZE];
struct xid_entry	*xe_first = &xe_table[0];
struct xid_entry	*xe	  = &xe_table[0];
struct xid_entry	*xe_last  = &xe_table[XID_CACHE_SIZE - 1];

struct xid_entry *
find_rpc(rpc)
	struct rpc_msg *rpc;
{
	struct xid_entry *x;

	for (x = xe; x >= xe_first; x--)
		if (x->x_xid == rpc->rm_xid)
			return (x);
	for (x = xe_last; x > xe; x--)
		if (x->x_xid == rpc->rm_xid)
			return (x);
	return (NULL);
}

void
stash_rpc(rpc)
	struct rpc_msg *rpc;
{
	struct xid_entry *x;

	if (find_rpc(rpc))
		return;

	x = xe++;
	if (xe > xe_last)
		xe = xe_first;
	x->x_xid  = rpc->rm_xid;
	x->x_dir  = htonl(REPLY);
	x->x_prog = rpc->rm_call.cb_prog;
	x->x_vers = rpc->rm_call.cb_vers;
	x->x_proc = rpc->rm_call.cb_proc;
}

/*
 * This routine takes a packet and returns true or false
 * according to whether the filter expression selects it
 * or not.
 * We assume here that offsets for short and long values
 * are even - we may die with an alignment error if the
 * CPU doesn't support odd addresses.  Note that long
 * values are loaded as two shorts so that 32 bit word
 * alignment isn't important.
 */

#define	IP_HDR_LEN(p)	(((*(u_char *)p) & 0xf) * 4)
#define	TCP_HDR_LEN(p)	((((*((u_char *)p+12)) >> 4) & 0xf) * 4)

/*
 * Coding the constant below is tacky, but the compiler won't let us
 * be more clever.  E.g., &((struct ip *)0)->ip_xxx
 */
#define	IP_PROTO_OF(p)	(((u_char *)p)[9])

int
want_packet(pkt, len, origlen)
	u_char *pkt;
	int len, origlen;
{
	u_int stack[16];	/* operand stack */
	u_int *op;		/* current operator */
	u_int *sp;		/* top of operand stack */
	u_char *base;		/* base for offsets into packet */
	u_char *ip;		/* addr of IP header, unaligned */
	u_char *tcp;		/* addr of TCP header, unaligned */
	u_char *udp;		/* addr of UDP header, unaligned */
	struct rpc_msg rpcmsg;	/* addr of RPC header */
	struct rpc_msg *rpc;
	int newrpc = 0;
	int off, header_size;
	u_char *offstack[16];	/* offset stack */
	u_char **offp;		/* current offset */


	sp = stack;
	*sp = 1;
	base = pkt;
	offp = offstack;

	header_size = (*interface->header_len)((char *) pkt);

	for (op = oplist; *op; op++) {
		switch ((enum optype) *op) {
		case OP_LOAD_OCTET:
			*sp = *((u_char *) (base + *sp));
			break;
		case OP_LOAD_SHORT:
			off = *sp;
			/*
			 * Handle 2 possible alignments
			 */
			switch ((((unsigned)base)+off) % sizeof (u_short)) {
			case 0:
				*sp = ntohs(*((u_short *) (base + *sp)));
				break;
			case 1:
				*((u_char *) (sp)) =
					*((u_char *) (base + off));
				*(((u_char *) sp) + 1) =
					*((u_char *) (base + off) + 1);
				*sp = ntohs(*(u_short *)sp);
				break;
			}
			break;
		case OP_LOAD_LONG:
			off = *sp;
			/*
			 * Handle 3 possible alignments
			 */
			switch ((((unsigned)base) + off) % sizeof (u_long)) {
			case 0:
				*sp = *(u_long *) (base + off);
				break;

			case 2:
				*((u_short *) (sp)) =
					*((u_short *) (base + off));
				*(((u_short *) sp) + 1) =
					*((u_short *) (base + off) + 1);
				break;

			case 1:
			case 3:
				*((u_char *) (sp)) =
					*((u_char *) (base + off));
				*(((u_char *) sp) + 1) =
					*((u_char *) (base + off) + 1);
				*(((u_char *) sp) + 2) =
					*((u_char *) (base + off) + 2);
				*(((u_char *) sp) + 3) =
					*((u_char *) (base + off) + 3);
				break;
			}
			*sp = ntohl(*sp);
			break;
		case OP_LOAD_CONST:
			*(++sp) = *(++op);
			break;
		case OP_LOAD_LENGTH:
			*(++sp) = origlen;
			break;
		case OP_EQ:
			sp--;
			*sp = *sp == *(sp + 1);
			break;
		case OP_NE:
			sp--;
			*sp = *sp != *(sp + 1);
			break;
		case OP_GT:
			sp--;
			*sp = *sp > *(sp + 1);
			break;
		case OP_GE:
			sp--;
			*sp = *sp >= *(sp + 1);
			break;
		case OP_LT:
			sp--;
			*sp = *sp < *(sp + 1);
			break;
		case OP_LE:
			sp--;
			*sp = *sp <= *(sp + 1);
			break;
		case OP_AND:
			sp--;
			*sp &= *(sp + 1);
			break;
		case OP_OR:
			sp--;
			*sp |= *(sp + 1);
			break;
		case OP_XOR:
			sp--;
			*sp ^= *(sp + 1);
			break;
		case OP_NOT:
			*sp = !*sp;
			break;
		case OP_BRFL:
			op++;
			if (!*sp)
				op = &oplist[*op] - 1;
			break;
		case OP_BRTR:
			op++;
			if (*sp)
				op = &oplist[*op] - 1;
			break;
		case OP_ADD:
			sp--;
			*sp += *(sp + 1);
			break;
		case OP_SUB:
			sp--;
			*sp -= *(sp + 1);
			break;
		case OP_MUL:
			sp--;
			*sp *= *(sp + 1);
			break;
		case OP_DIV:
			sp--;
			*sp *= *(sp + 1);
			break;
		case OP_REM:
			sp--;
			*sp %= *(sp + 1);
			break;
		case OP_OFFSET_POP:
			base = *offp--;
			break;
		case OP_OFFSET_ZERO:
			*++offp = base;
			base = pkt;
			break;
		case OP_OFFSET_ETHER:
			*++offp = base;
			base = pkt + header_size;
			break;
		case OP_OFFSET_IP:
			*++offp = base;
			ip = pkt + header_size;
			base  = ((u_char *) ip) + IP_HDR_LEN(ip);
			break;
		case OP_OFFSET_TCP:
			*++offp = base;
			ip = pkt + header_size;
			tcp  = ip + IP_HDR_LEN(ip);
			base = tcp + TCP_HDR_LEN(tcp);
			break;
		case OP_OFFSET_UDP:
			*++offp = base;
			ip = pkt + header_size;
			udp  = (ip) + IP_HDR_LEN(ip);
			base = udp + sizeof (struct udphdr);
			break;
		case OP_OFFSET_RPC:
			*++offp = base;
			ip = pkt + header_size;
			rpc = NULL;
			switch (IP_PROTO_OF(ip)) {
			case IPPROTO_UDP:
				udp  = ip + IP_HDR_LEN(ip);
				rpc = (struct rpc_msg *) (udp
					+ sizeof (struct udphdr));
				break;
			case IPPROTO_TCP:
				tcp = ip + IP_HDR_LEN(ip);
				/*
				 * Need to skip an extra 4 for the xdr_rec
				 * field.
				 */
				rpc = (struct rpc_msg *) (tcp
						+ TCP_HDR_LEN(tcp) + 4);
				break;
			}
			if (rpc == NULL || (u_char *) rpc > pkt + len) {
				*(++sp) = 0;
				break;
			}
			/* align */
			rpc = (struct rpc_msg *) memcpy(&rpcmsg, rpc, 24);
			if (!valid_rpc(rpc, 24)) {
				*(++sp) = 0;
				break;
			}
			if (ntohl(rpc->rm_direction) == CALL) {
				base = (u_char *) rpc;
				newrpc = 1;
				*(++sp) = 1;
			} else {
				base = (u_char *) find_rpc(rpc);
				*(++sp) = base != NULL;
			}
			break;
		}
	}

	if (*sp && newrpc)
		stash_rpc(&rpcmsg);

	return (*sp);
}

void
load_const(c)
	u_int c;
{
	emitop(OP_LOAD_CONST);
	emitval(c);
}

void
load_value(offset, len)
	int offset, len;
{
	if (offset >= 0)
		load_const(offset);

	switch (len) {
		case 1:
			emitop(OP_LOAD_OCTET);
			break;
		case 2:
			emitop(OP_LOAD_SHORT);
			break;
		case 4:
			emitop(OP_LOAD_LONG);
			break;
	}
}

/*
 * Emit code to compare a field in
 * the packet against a constant value.
 */
void
compare_value(offset, len, val)
	u_int offset, len, val;
{
	load_const(val);
	load_value(offset, len);
	emitop(OP_EQ);
}

/*
 * Same as above except do the comparison
 * after and'ing a mask value.  Useful
 * for comparing IP network numbers
 */
void
compare_value_mask(offset, len, val, mask)
	u_int offset, len, val;
{
	load_value(offset, len);
	load_const(mask);
	emitop(OP_AND);
	load_const(val);
	emitop(OP_EQ);
}

/* Emit an operator into the code array */
void
emitop(x)
	enum optype x;
{
	if (curr_op >= &oplist[MAXOPS])
		pr_err("expression too long");
	*curr_op++ = x;
}

/*
 * Remove n operators recently emitted into
 * the code array.  Used by alternation().
 */
void
unemit(n)
	int n;
{
	curr_op -= n;
}


/*
 * Same as emitop except that we're emitting
 * a value that's not an operator.
 */
void
emitval(x)
	u_int x;
{
	if (curr_op >= &oplist[MAXOPS])
		pr_err("expression too long");
	*curr_op++ = x;
}

/*
 * Used to chain forward branches together
 * for later resolution by resolve_chain().
 */
chain(p)
{
	u_int pos = curr_op - oplist;

	emitval(p);
	return (pos);
}

/*
 * Proceed backward through the code array
 * following a chain of forward references.
 * At each reference install the destination
 * branch offset.
 */
void
resolve_chain(p)
	u_int p;
{
	u_int n;
	u_int pos = curr_op - oplist;

	while (p) {
		n = oplist[p];
		oplist[p] = pos;
		p = n;
	}
}

#define	EQ(val) (strcmp(token, val) == 0)

char *tkp;
char *token;
enum { EOL, ALPHA, NUMBER, FIELD, ADDR_IP, ADDR_ETHER, SPECIAL } tokentype;
u_int tokenval;

/*
 * This is the scanner.  Each call returns the next
 * token in the filter expression.  A token is either:
 * EOL:		The end of the line - no more tokens.
 * ALPHA:	A name that begins with a letter and contains
 *		letters or digits, hyphens or underscores.
 * NUMBER:	A number.  The value can be represented as
 * 		a decimal value (1234) or an octal value
 *		that begins with zero (066) or a hex value
 *		that begins with 0x or 0X (0xff).
 * FIELD:	A name followed by a left square bracket.
 * ADDR_IP:	An IP address.  Any sequence of digits
 *		separated by dots e.g. 129.144.40.13
 * ADDR_ETHER:	An ethernet address.  Any sequence of hex
 *		digits separated by colons e.g. 8:0:20:0:76:39
 * SPECIAL:	A special character e.g. ">" or "(".  The scanner
 *		correctly handles digraphs - two special characters
 *		that constitute a single token e.g. "==" or ">=".
 *
 * The current token is maintained in "token" and and its
 * type in "tokentype".  If tokentype is NUMBER then the
 * value is held in "tokenval".
 */

const char *namechars =
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";

void
next()
{
	static int savechar;
	char *p;
	int base, colons, dots;

	if (*tkp == '\0') {
		token = tkp;
		*tkp = savechar;
	}

	while (isspace(*tkp)) tkp++;
	token = tkp;
	if (*token == '\0') {
		tokentype = EOL;
		return;
	}

	if (isalpha(*tkp)) {
		tokentype = ALPHA;
		tkp += strspn(tkp, namechars);
		if (*tkp == '[') {
			tokentype = FIELD;
			*tkp++ = '\0';
		}
	} else

	if (isdigit(*tkp)) {
		tkp = token + strspn(token, "0123456789abcdefABCDEFXx:.");
		colons = dots = 0;
		for (p = token; p < tkp; p++) {
			if (*p == ':')
				colons++;
			else if (*p == '.')
				dots++;
		}
		if (colons == 5)
			tokentype = ADDR_ETHER;
		else if (dots)
			tokentype = ADDR_IP;
		else {
			tokentype = NUMBER;
			tkp = token;
			base = 10;
			if (*tkp == '0') {
				base = 8;
				tkp++;
				if (*tkp == 'x' || *tkp == 'X')
					base = 16;
			}
			tokenval = strtol(token, &tkp, base);
		}
	} else {
		tokentype = SPECIAL;
		tkp++;
		if ((*token == '=' && *tkp == '=') ||
		    (*token == '>' && *tkp == '=') ||
		    (*token == '<' && *tkp == '=') ||
		    (*token == '!' && *tkp == '='))
				tkp++;
	}

	savechar = *tkp;
	*tkp = '\0';
}

struct match_type {
	char *	m_name;
	int	m_offset;
	int	m_size;
	int	m_value;
	char *	m_depend;
} match_types[] = {
	/*
	 * This table presume ethernet data link headers, so watch it carefully!
	 * IP/ARP/RARP are fudged as a result.
	 */
	"ip",		12, 2, ETHERTYPE_IP,	"",
	"arp",		12, 2, ETHERTYPE_ARP,	"",
	"rarp",		12, 2, ETHERTYPE_REVARP, "",
	"tcp",		9, 1, IPPROTO_TCP,	"ip",
	"udp",		9, 1, IPPROTO_UDP,	"ip",
	"icmp",		9, 1, IPPROTO_ICMP,	"ip",
};

/*
 * Generate code based on the keyword argument.
 * This word is looked up in the match_types table
 * and checks a field within the packet for a given
 * value e.g. ether or ip type field.  The match
 * can also have a dependency on another entry e.g.
 * "tcp" requires that the packet also be "ip".
 */
int
comparison(s)
	char *s;
{
	struct match_type *mtp;
	u_int m;
	int offset;
	int elems = sizeof (match_types) / sizeof (*mtp);

	for (mtp = match_types; mtp < &match_types[elems]; mtp++) {
		if (strcmp(s, mtp->m_name))
			continue;
		/*
		 * Hack attack: Should really be more general, but for now
		 * the code below only understands (for sure) IP.  A dependency
		 * implies the checks are being done above the data link
		 * header
		 */
		offset = mtp->m_offset;
		if (*mtp->m_depend) {
			emitop(OP_OFFSET_ETHER);
		} else {
			/*
			 * The table is filled with ethernet offsets.  Here we
			 * fudge the value based on what know about the
			 * interface.  It is okay to do this because we are
			 * checking what we believe to be an IP/ARP/RARP
			 * packet, and we know those are carried in LLC-SNAP
			 * headers on FDDI.  We assume that it's unlikely
			 * another kind of packet, with a shorter FDDI header
			 * will happen to match the filter.
			 *
			 *		Ether	FDDI
			 *  edst addr	0	1
			 *  esrc addr	6	7
			 *  ethertype	12	19
			 */
			if (interface->mac_type == DL_FDDI) {
				if (offset < 12) {
					offset++;
				} else if (offset == 12) {
					offset = 19;
				}
			}
		}
		/* XXX token ring? */
		load_value(offset, mtp->m_size);
		load_const(mtp->m_value);
		if (*mtp->m_depend) {
			emitop(OP_OFFSET_POP);
		}
		emitop(OP_EQ);
		return (1);
	}
	return (0);
}

enum direction { ANY, TO, FROM };
enum direction dir;

/*
 * Generate code to match an IP address.  The address
 * may be supplied either as a hostname or in dotted format.
 * For source packets both the IP source address and ARP
 * src are checked.
 * Note: we don't check packet type here - whether IP or ARP.
 * It's possible that we'll do an improper match.
 */
void
ipaddr_match(which, hostname)
	enum direction which;
	char *hostname;
{
	u_int addr;
	u_int m;
	struct hostent *hp;

	if (isdigit(*hostname)) {
		addr = inet_addr(hostname);
	} else {
		hp = gethostbyname(hostname);
		if (hp == NULL)
			pr_err("host %s not known", hostname);
		addr = *(u_int *) hp->h_addr;
	}
	addr = ntohl(addr);

	emitop(OP_OFFSET_ETHER);
	switch (which) {
	case TO:
		compare_value(16, 4, addr);	/* dst IP addr */
		break;
	case FROM:
		compare_value(12, 4, addr);	/* src IP addr */
		emitop(OP_BRTR);
		m = chain(0);
		compare_value(14, 4, addr);	/* src ARP addr */
		resolve_chain(m);
		break;
	case ANY:
		compare_value(12, 4, addr);	/* src IP addr */
		emitop(OP_BRTR);
		m = chain(0);
		compare_value(16, 4, addr);	/* dst IP addr */
		emitop(OP_BRTR);
		m = chain(m);
		compare_value(14, 4, addr);	/* src ARP addr */
		resolve_chain(m);
		break;
	}
	emitop(OP_OFFSET_POP);
}

/*
 * Compare ethernet addresses. The address may
 * be provided either as a hostname or as a
 * 6 octet colon-separated address.
 * Note: this routine cheats and does a simple
 * longword comparison i.e. the first
 * two octets of the address are ignored.
 */
void
etheraddr_match(which, hostname)
	enum direction which;
	char *hostname;
{
	u_int addr;
	int to_offset, from_offset;
	struct ether_addr e, *ep;
	struct ether_addr *ether_aton();
	int m;

	if (isdigit(*hostname)) {
		ep = ether_aton(hostname);
		if (ep == NULL)
			pr_err("bad ether addr %s", hostname);
	} else {
		if (ether_hostton(hostname, &e))
			pr_err("cannot obtain ether addr for %s",
				hostname);
		ep = &e;
	}
	memcpy(&addr, (u_short *) ep + 1, 4);

	switch (interface->mac_type) {
	case DL_ETHER:
		from_offset = 8;
		to_offset = 2;
		break;

	case DL_FDDI:
		from_offset = 9;
		to_offset = 3;
		break;

	default:
		/*
		 * Where do we find "ether" address for FDDI & TR?
		 * XXX can improve?  ~sparker
		 */
		load_const(1);
		return;
	}
	switch (which) {
	case TO:
		compare_value(to_offset, 4, htonl(addr));
		break;
	case FROM:
		compare_value(from_offset, 4, htonl(addr));
		break;
	case ANY:
		compare_value(to_offset, 4, htonl(addr));
		emitop(OP_BRTR);
		m = chain(0);
		compare_value(from_offset, 4, htonl(addr));
		resolve_chain(m);
		break;
	}
}

void
ethertype_match(val)
	int	val;
{
	int	m;
	int	ether_offset;

	switch (interface->mac_type) {
	case DL_ETHER:
		ether_offset = 12;
		break;

	case DL_FDDI:
		/* XXX Okay to assume LLC SNAP? */
		ether_offset = 19;
		break;

	default:
		load_const(1);	/* Assume a match */
		return;
	}
	compare_value(ether_offset, 2, val); /* XXX.sparker */
}

/*
 * Match a network address.  The host part
 * is masked out.  The network address may
 * be supplied either as a netname or in
 * IP dotted format.  The mask to be used
 * for the comparison is assumed from the
 * address format (see comment below).
 */
void
netaddr_match(which, netname)
	enum direction which;
	char *netname;
{
	u_int addr;
	u_int mask = 0xff000000;
	u_int m;
	struct netent *np;

	if (isdigit(*netname)) {
		addr = inet_network(netname);
	} else {
		np = getnetbyname(netname);
		if (np == NULL)
			pr_err("net %s not known", netname);
		addr = np->n_net;
	}
	addr = ntohl(addr);

	/*
	 * Left justify the address and figure
	 * out a mask based on the supplied address.
	 * Set the mask according to the number of zero
	 * low-order bytes.
	 * Note: this works only for whole octet masks.
	 */
	if (addr) {
		while ((addr & ~mask) != 0) {
			mask |= (mask >> 8);
		}
	}

	emitop(OP_OFFSET_ETHER);
	switch (which) {
	case TO:
		compare_value_mask(16, 4, addr, mask);
		break;
	case FROM:
		compare_value_mask(12, 4, addr, mask);
		break;
	case ANY:
		compare_value_mask(12, 4, addr, mask);
		emitop(OP_BRTR);
		m = chain(0);
		compare_value_mask(16, 4, addr, mask);
		resolve_chain(m);
		break;
	}
	emitop(OP_OFFSET_POP);
}

/*
 * Match either a UDP or TCP port number.
 * The port number may be provided either as
 * port name as listed in /etc/services ("nntp") or as
 * the port number itself (2049).
 */
void
port_match(which, portname)
	enum direction which;
	char *portname;
{
	struct servent *sp;
	u_int m, port;

	if (isdigit(*portname)) {
		port = atoi(portname);
	} else {
		sp = getservbyname(portname, NULL);
		if (sp == NULL)
			pr_err("invalid port number or name: %s",
				portname);
		port = ntohs(sp->s_port);
	}

	emitop(OP_OFFSET_IP);

	switch (which) {
	case TO:
		compare_value(2, 2, port);
		break;
	case FROM:
		compare_value(0, 2, port);
		break;
	case ANY:
		compare_value(2, 2, port);
		emitop(OP_BRTR);
		m = chain(0);
		compare_value(0, 2, port);
		resolve_chain(m);
		break;
	}
	emitop(OP_OFFSET_POP);
}

/*
 * Generate code to match packets with a specific
 * RPC program number.  If the progname is a name
 * it is converted to a number via /etc/rpc.
 * The program version and/or procedure may be provided
 * as extra qualifiers.
 */
void
rpc_match_prog(which, progname, vers, proc)
	enum direction which;
	char *progname;
	int vers, proc;
{
	struct rpcent *rpc;
	u_int prog;
	u_int m, n;

	if (isdigit(*progname)) {
		prog = atoi(progname);
	} else {
		rpc = (struct rpcent *) getrpcbyname(progname);
		if (rpc == NULL)
			pr_err("invalid program name: %s", progname);
		prog = rpc->r_number;
	}

	emitop(OP_OFFSET_RPC);
	emitop(OP_BRFL);
	n = chain(0);

	compare_value(12, 4, prog);
	emitop(OP_BRFL);
	m = chain(0);
	if (vers >= 0) {
		compare_value(16, 4, vers);
		emitop(OP_BRFL);
		m = chain(m);
	}
	if (proc >= 0) {
		compare_value(20, 4, proc);
		emitop(OP_BRFL);
		m = chain(m);
	}

	switch (which) {
	case TO:
		compare_value(4, 4, CALL);
		emitop(OP_BRFL);
		m = chain(m);
		break;
	case FROM:
		compare_value(4, 4, REPLY);
		emitop(OP_BRFL);
		m = chain(m);
		break;
	}
	resolve_chain(m);
	emitop(OP_OFFSET_POP);
	resolve_chain(n);
}

/*
 * Generate code to parse a field specification
 * and load the value of the field from the packet
 * onto the operand stack.
 * The field offset may be specified relative to the
 * beginning of the ether header, IP header, UDP header,
 * or TCP header.  An optional size specification may
 * be provided following a colon.  If no size is given
 * one byte is assumed e.g.
 *
 *	ether[0]	The first byte of the ether header
 *	ip[2:2]		The second 16 bit field of the IP header
 */
void
load_field()
{
	int size = 1;
	int s;

	if (EQ("ether"))
		emitop(OP_OFFSET_ZERO);
	else if (EQ("ip"))
		emitop(OP_OFFSET_ETHER);
	else if (EQ("udp") || EQ("tcp") || EQ("icmp"))
		emitop(OP_OFFSET_IP);
	else
		pr_err("invalid field type");
	next();
	s = opstack;
	expression();
	if (opstack != s + 1)
		pr_err("invalid field offset");
	opstack--;
	if (*token == ':') {
		next();
		if (tokentype != NUMBER)
			pr_err("field size expected");
		size = tokenval;
		if (size != 1 && size != 2 && size != 4)
			pr_err("field size invalid");
		next();
	}
	if (*token != ']')
		pr_err("right bracket expected");

	load_value(-1, size);
	emitop(OP_OFFSET_POP);
}

/*
 * Check that the operand stack
 * contains n arguments
 */
void
checkstack(n)
	int n;
{
	if (opstack != n)
		pr_err("invalid expression at \"%s\".", token);
}

void
primary()
{
	int m, s;

	for (;;) {
		if (tokentype == FIELD) {
			load_field();
			opstack++;
			next();
			break;
		}

		if (comparison(token)) {
			opstack++;
			next();
			break;
		}

		if (EQ("not") || EQ("!")) {
			next();
			s = opstack;
			primary();
			checkstack(s + 1);
			emitop(OP_NOT);
			break;
		}

		if (EQ("(")) {
			next();
			s = opstack;
			expression();
			checkstack(s + 1);
			if (!EQ(")"))
				pr_err("right paren expected");
			next();
		}

		if (EQ("to") || EQ("dst")) {
			dir = TO;
			next();
			continue;
		}

		if (EQ("from") || EQ("src")) {
			dir = FROM;
			next();
			continue;
		}

		if (EQ("ether")) {
			eaddr = 1;
			next();
			continue;
		}

		if (EQ("proto")) { /* ignore */
			next();
			continue;
		}

		if (EQ("broadcast")) {
			/*
			 * Be tricky: FDDI ether dst address begins at
			 * byte one.  Since the address is really six
			 * bytes long, this works for FDDI & ethernet.
			 * XXX - Token ring?
			 */
			compare_value(1, 4, 0xffffffff);
			opstack++;
			next();
			break;
		}

		if (EQ("multicast")) {
			/* XXX Token ring? */
			if (interface->mac_type == DL_FDDI) {
				compare_value_mask(1, 1, 0x01, 0x01);
			} else {
				compare_value_mask(0, 1, 0x01, 0x01);
			}
			opstack++;
			next();
			break;
		}

		if (EQ("decnet")) {
			/* XXX Token ring? */
			if (interface->mac_type == DL_FDDI) {
				load_value(19, 2);	/* ether type */
				load_const(0x6000);
				emitop(OP_GE);
				emitop(OP_BRFL);
				m = chain(0);
				load_value(19, 2);	/* ether type */
				load_const(0x6009);
				emitop(OP_LE);
				resolve_chain(m);
			} else {
				load_value(12, 2);	/* ether type */
				load_const(0x6000);
				emitop(OP_GE);
				emitop(OP_BRFL);
				m = chain(0);
				load_value(12, 2);	/* ether type */
				load_const(0x6009);
				emitop(OP_LE);
				resolve_chain(m);
			}
			opstack++;
			next();
			break;
		}

		if (EQ("apple")) {
			ethertype_match(0x809b); /* ether type */
			emitop(OP_BRTR);
			m = chain(0);
			ethertype_match(0x803f); /* ether type */
			resolve_chain(m);
			opstack++;
			next();
			break;
		}

		if (EQ("ethertype")) {
			next();
			if (tokentype != NUMBER)
				pr_err("ether type expected");
			ethertype_match(tokenval);
			opstack++;
			next();
			break;
		}

		if (EQ("length")) {
			emitop(OP_LOAD_LENGTH);
			opstack++;
			next();
			break;
		}

		if (EQ("less")) {
			next();
			if (tokentype != NUMBER)
				pr_err("packet length expected");
			emitop(OP_LOAD_LENGTH);
			load_const(tokenval);
			emitop(OP_LT);
			opstack++;
			next();
			break;
		}

		if (EQ("greater")) {
			next();
			if (tokentype != NUMBER)
				pr_err("packet length expected");
			emitop(OP_LOAD_LENGTH);
			load_const(tokenval);
			emitop(OP_GT);
			opstack++;
			next();
			break;
		}

		if (EQ("nofrag")) {
			emitop(OP_OFFSET_ETHER);
			compare_value_mask(6, 2, 0, 0x1fff);
			emitop(OP_OFFSET_POP);
			emitop(OP_BRFL);
			m = chain(0);
			ethertype_match(ETHERTYPE_IP);
			resolve_chain(m);
			opstack++;
			next();
			break;
		}

		if (EQ("net") || EQ("dstnet") || EQ("srcnet")) {
			if (EQ("dstnet"))
				dir = TO;
			else if (EQ("srcnet"))
				dir = FROM;
			next();
			netaddr_match(dir, token);
			dir = ANY;
			opstack++;
			next();
			break;
		}

		if (EQ("port") || EQ("srcport") || EQ("dstport")) {
			if (EQ("dstport"))
				dir = TO;
			else if (EQ("srcport"))
				dir = FROM;
			next();
			port_match(dir, token);
			dir = ANY;
			opstack++;
			next();
			break;
		}

		if (EQ("rpc")) {
			u_int vers, proc;
			char savetoken[16];

			vers = proc = -1;
			next();
			(void) strcpy(savetoken, token);
			next();
			if (*token == ',') {
				next();
				if (tokentype != NUMBER)
					pr_err("version number expected");
				vers = tokenval;
				next();
			}
			if (*token == ',') {
				next();
				if (tokentype != NUMBER)
					pr_err("proc number expected");
				proc = tokenval;
				next();
			}
			rpc_match_prog(dir, savetoken, vers, proc);
			dir = ANY;
			opstack++;
			break;
		}

		if (EQ("and") || EQ("or")) {
			break;
		}

		if (EQ("gateway")) {
			next();
			if (eaddr || tokentype != ALPHA)
				pr_err("hostname required: %s", token);
			etheraddr_match(dir, token);
			dir = ANY;
			emitop(OP_BRFL);
			m = chain(0);
			ipaddr_match(ANY, token);
			emitop(OP_NOT);
			resolve_chain(m);
			opstack++;
			next();
		}

		if (EQ("host") || EQ("between") ||
		    tokentype == ALPHA ||	/* assume its a hostname */
		    tokentype == ADDR_IP ||
		    tokentype == ADDR_ETHER) {
			if (EQ("host") || EQ("between"))
				next();
			if (eaddr || tokentype == ADDR_ETHER)
				etheraddr_match(dir, token);
			else
				ipaddr_match(dir, token);
			dir = ANY;
			eaddr = 0;
			opstack++;
			next();
			break;
		}

		if (tokentype == NUMBER) {
			load_const(tokenval);
			opstack++;
			next();
			break;
		}

		break;	/* unknown token */
	}
}

struct optable {
	char *op_tok;
	enum optype op_type;
};

struct optable
mulops[] = {
	"*",	OP_MUL,
	"/",	OP_DIV,
	"%",	OP_REM,
	"&",	OP_AND,
	"",	OP_STOP,
};

struct optable
addops[] = {
	"+",	OP_ADD,
	"-",	OP_SUB,
	"|",	OP_OR,
	"^",	OP_XOR,
	"",	OP_STOP,
};

struct optable
compareops[] = {
	"==",	OP_EQ,
	"=",	OP_EQ,
	"!=",	OP_NE,
	">",	OP_GT,
	">=",	OP_GE,
	"<",	OP_LT,
	"<=",	OP_LE,
	"",	OP_STOP,
};

/*
 * Using the table, find the operator
 * that corresponds to the token.
 * Return 0 if not found.
 */
find_op(tok, table)
	char *tok;
	struct optable *table;
{
	struct optable *op;
	int i;

	for (op = table; *op->op_tok; op++) {
		if (strcmp(tok, op->op_tok) == 0)
			return (op->op_type);
	}

	return (0);
}

void
expr_mul()
{
	int op;
	int r, s = opstack;

	primary();
	while (op = find_op(token, mulops)) {
		next();
		primary();
		checkstack(s + 2);
		emitop((enum optype)op);
		opstack--;
	}
}

void
expr_add()
{
	int op, s = opstack;

	expr_mul();
	while (op = find_op(token, addops)) {
		next();
		expr_mul();
		checkstack(s + 2);
		emitop((enum optype)op);
		opstack--;
	}
}

void
expr_compare()
{
	int op, s = opstack;

	expr_add();
	while (op = find_op(token, compareops)) {
		next();
		expr_add();
		checkstack(s + 2);
		emitop((enum optype) op);
		opstack--;
	}
}

/*
 * Alternation ("and") is difficult because
 * an implied "and" is acknowledge between
 * two adjacent primaries.  Just keep calling
 * the lower-level expression routine until
 * no value is added to the opstack.
 */
void
alternation()
{
	int m = 0;
	int s = opstack;

	expr_compare();
	checkstack(s + 1);
	for (;;) {
		if (EQ("and"))
			next();
		emitop(OP_BRFL);
		m = chain(m);
		expr_compare();
		if (opstack != s + 2)
			break;
		opstack--;
	}
	unemit(2);
	resolve_chain(m);
}

void
expression()
{
	int m = 0;
	int s = opstack;

	alternation();
	while (EQ("or") || EQ(",")) {
		emitop(OP_BRTR);
		m = chain(m);
		next();
		alternation();
		checkstack(s + 2);
		opstack--;
	}
	resolve_chain(m);
}

/*
 * Take n args from the argv list
 * and concatenate them into a single string.
 */
char *
concat_args(argv, n)
	char **argv;
	int n;
{
	int i, len;
	char *str, *p;

	/* First add the lengths of all the strings */
	len = 0;
	for (i = 0; i < n; i++)
		len += strlen(argv[i]) + 1;

	/* allocate the big string */
	str = (char *) malloc(len);
	if (str == NULL)
		pr_err("no mem");

	p = str;

	/*
	 * Concat the strings into the big
	 * string using a space as separator
	 */
	for (i = 0; i < n; i++) {
		strcpy(p, argv[i]);
		p += strlen(p);
		*p++ = ' ';
	}
	*--p = '\0';

	return (str);
}

/*
 * Take the expression in the string "e"
 * and compile it into the code array.
 * Print the generated code if the print
 * arg is set.
 */
void
compile(e, print)
	char *e;
	int print;
{
	e = strdup(e);
	if (e == NULL)
		pr_err("no mem");
	curr_op = oplist;
	tkp = e;
	dir = ANY;

	next();
	if (tokentype != EOL)
		expression();
	emitop(OP_STOP);
	if (tokentype != EOL)
		pr_err("invalid expression");
	optimize(oplist);
	if (print)
		codeprint();
}
