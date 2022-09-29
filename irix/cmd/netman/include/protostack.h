#ifndef PROTOSTACK_H
#define PROTOSTACK_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Variant information may be passed between two related protocols' match,
 * fetch, and decode operations using a protostack.  Each frame contains
 * stack linkage, a pointer to its protocol, and protocol-dependent data.
 */

struct expr;
struct __xdr_s;

typedef struct protostack {
	struct protostackframe	*ps_top;	/* top frame pointer */
	void			*ps_slink;	/* nested struct static link */
	struct snooper		*ps_snoop;	/* the capturing snooper */
	struct snoopheader	*ps_snhdr;	/* current snoop header */
	struct protodecodestate	*ps_state;	/* saved decode state */
} ProtoStack;

#define	PS_INIT(ps, sn, sh) \
	((ps)->ps_top = 0, (ps)->ps_slink = 0, (ps)->ps_snoop = (sn), \
	 (ps)->ps_snhdr = (sh), (ps)->ps_state = 0)
#define	PS_TOP(ps, type) \
	((type *) (ps)->ps_top)
#define	PS_STATE(ps, type) \
	((type *) (ps)->ps_state)

typedef struct protostackframe {
	struct protostackframe	*psf_down;
	struct protocol		*psf_proto;
} ProtoStackFrame;

#define	PS_PUSH(ps, psf, pr) \
	((psf)->psf_down = (ps)->ps_top, (ps)->ps_top = (psf), \
	 (psf)->psf_proto = (pr))
#define	PS_POP(ps) \
	((ps)->ps_top = (ps)->ps_top->psf_down)

/*
 * The base structure from which higher layer protocols derive a state
 * record telling where decoding should resume in a stream of fragments.
 * When a higher layer decodes the initial fragment of a larger segment,
 * it allocates a ProtoDecodeState derivative and points ps->ps_state at
 * the derived structure.
 *
 * Higher-layer protocol decode functions are coroutines of one another,
 * suspending themselves via return after decoding the first of several
 * fragments, and attempting to resume decoding when called with non-null
 * ps->ps_state.  Protocol decode functions should deal gracefully with
 * fragment loss and partial capture.
 */
typedef struct protodecodestate {
	unsigned int	pds_mysize;	/* derived struct size */
	struct protocol	*pds_proto;	/* protocol to resume */
} ProtoDecodeState;

/*
 * Allocating xdr filters used by fragment matching and decoding caches.
 */
struct expr	*savematchresult(struct expr *);
int		xdr_matchresult(struct __xdr_s *, struct expr **);
int		xdr_decodestate(struct __xdr_s *, ProtoDecodeState **);

#endif
