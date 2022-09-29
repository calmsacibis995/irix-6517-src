/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snooper expression filter compiler.
 */
#include <bstring.h>
#include "debug.h"
#include "expr.h"
#include "macros.h"
#include "protocol.h"
#include "snooper.h"

/*
 * Invariant argument to recursive compile.
 */
static ExprError *comperr;

static ExprType	compile(Expr *, Protocol *, int, struct sfset *);

/*
 * If ex is null, be promiscuous.  Otherwise parse and compile it.
 */
int
sfs_compile(struct sfset *sfs, Expr *ex, Protocol *rawproto, int rawhdrpad,
	    ExprError *err)
{
	if (ex == 0) {
		if (sfs->sfs_elements != 0) {
			int i;
			SnoopFilter *sf;

			i = SNOOP_MAXFILTERS;
			for (sf = &sfs->sfs_vec[0]; --i >= 0; sf++)
				if (sf->sf_allocated)
					(void) sfs_freefilter(sfs, sf);
		}
		(void) sfs_allocfilter(sfs);
	} else {
		comperr = err;
		switch (compile(ex, rawproto, rawhdrpad, sfs)) {
		  case ET_SIMPLE:
			assert(sfs->sfs_elements != 0);
			break;
		  case ET_COMPLEX:
			if (sfs->sfs_elements == 0)
				(void) sfs_allocfilter(sfs);
			break;
		  case ET_ERROR:
			return 0;
		}
	}
	return 1;
}

/*
 * Forward prototypes of private routines called by compile.
 */
static SnoopFilter *allocfilter(struct sfset *, ExprType *);
static void	   joinfilters(SnoopFilter *, SnoopFilter *, SnoopFilter *);
static int	   ismaskedfield(Expr *, Expr **, Expr **, unsigned short *);
static int	   isfield(Expr *, unsigned short *);
static ExprType	   compile_fetch(Expr *, Expr *, ProtoCompiler *);

static ExprType
compile(Expr *ex, Protocol *pr, int offset, struct sfset *sfs)
{
	ExprType type, rtype;

	switch (ex->ex_op) {
	  case EXOP_OR: {
		int i, j;
		SnoopFilter *sfi, *sfj;

		/*
		 * Compile both kids into unique snoopfilters.
		 */
		type = compile(ex->ex_left, pr, offset, sfs);
		if (type == ET_ERROR)
			return type;
		rtype = compile(ex->ex_right, pr, offset, sfs);
		if (rtype == ET_ERROR)
			return rtype;
		i = SNOOP_MAXFILTERS;
		for (sfi = &sfs->sfs_vec[0]; --i > 0; sfi++) {
			if (!sfi->sf_allocated)
				break;
			j = i;
			for (sfj = sfi + 1; --j >= 0; sfj++) {
				if (!sfj->sf_allocated)
					break;
				if (bcmp(sfi, sfj, sizeof *sfi))
					continue;
				(void) sfs_freefilter(sfs, sfj);
			}
		}
		break;
	  }

	  case EXOP_AND: {
		struct sfset lset, rset;
		int i, j;
		SnoopFilter *sfi, *sfj, *sf;

		sfs_init(&lset);
		type = compile(ex->ex_left, pr, offset, &lset);
		if (type == ET_ERROR)
			return type;
		sfs_init(&rset);
		rtype = compile(ex->ex_right, pr, offset, &rset);
		if (rtype == ET_ERROR)
			return rtype;

		/*
		 * If one operand couldn't be compiled into snoopfilters,
		 * use the other.
		 */
		if (lset.sfs_elements == 0) {
			*sfs = rset;
			if (lset.sfs_errflags)
				sfs->sfs_errflags |= lset.sfs_errflags;
			return rtype;
		}
		if (rset.sfs_elements == 0) {
			*sfs = lset;
			if (rset.sfs_errflags)
				sfs->sfs_errflags |= rset.sfs_errflags;
			return type;
		}

		/*
		 * Compute filter set cross product.
		 */
		i = SNOOP_MAXFILTERS;
		for (sfi = &lset.sfs_vec[0]; --i >= 0; sfi++) {
			if (!sfi->sf_allocated)
				continue;
			j = SNOOP_MAXFILTERS;
			for (sfj = &rset.sfs_vec[0]; --j >= 0; sfj++) {
				if (!sfj->sf_allocated)
					continue;
				sf = allocfilter(sfs, &type);
				joinfilters(sfi, sfj, sf);
			}
		}
		sfs->sfs_errflags |= (lset.sfs_errflags | rset.sfs_errflags);
		break;
	  }

	  /*
	   * Compile 'field == target' or '(field & mask) == target'.
	   */
	  case EXOP_EQ: {
		Expr *fex, *mex, *tex;
		SnoopFilter *sf;
		ProtoCompiler pc;
		Protocol *parent;

		/*
		 * Find a field on either side (but not both) of ex.
		 */
		if (ismaskedfield(ex->ex_left, &fex, &mex, 0)) {
			tex = ex->ex_right;
			if (isfield(tex, 0))
				return ET_COMPLEX;
		} else {
			if (!ismaskedfield(ex->ex_right, &fex, &mex, 0))
				return ET_COMPLEX;
			tex = ex->ex_left;
		}

		/*
		 * Initialize a protocompiler using the next free filter.
		 */
		rtype = ET_SIMPLE;
		sf = allocfilter(sfs, &rtype);
		pc_init(&pc, sf, offset, pr->pr_byteorder, &sfs->sfs_errflags,
			comperr);

		/*
		 * Match nested protocols by their type fields.
		 */
		parent = pr;
		while (fex->ex_op != EXOP_FETCH) {
			if (fex->ex_op == EXOP_FIELD) {
				if (fex->ex_field->pf_size == PF_VARIABLE)
					return ET_COMPLEX;
				break;
			}
			assert(fex->ex_op == EXOP_PROTOCOL);
			if (fex->ex_prsym->sym_proto == &snoop_proto) {
				parent = &snoop_proto;
				fex = fex->ex_member;
				break;
			}
			if (parent->pr_discriminant == 0)
				return ET_COMPLEX;
			type = pr_compile(parent, parent->pr_discriminant, 0,
					  fex, &pc);
			if (type != ET_SIMPLE)
				return type;
			parent = fex->ex_prsym->sym_proto;
			pc_setbyteorder(&pc, parent->pr_byteorder);
			fex = fex->ex_member;
		}

		/*
		 * If the target node is a protocol path, skip to the last
		 * component (which must be a protocol).
		 */
		if (tex->ex_op == EXOP_PROTOCOL) {
			while (tex->ex_member) {
				pr = tex->ex_prsym->sym_proto;
				tex = tex->ex_member;
				if (tex->ex_op != EXOP_PROTOCOL)
					return ET_COMPLEX;
			}
			if (pr != parent) {
				ex_error(tex, "incompatible protocol", comperr);
				return ET_ERROR;
			}
		}

		if (fex->ex_op == EXOP_FIELD)
			type = pr_compile(parent, fex->ex_field, mex, tex, &pc);
		else
			type = compile_fetch(fex, tex, &pc);
		break;
	  }

	  /*
	   * We can compile 'field & mask' as '(field & mask) == mask' if
	   * the PR_COMPILETEST protocol flag is set or if mask has exactly
	   * one bit set.
	   */
	  case EXOP_BITAND: {
		Expr *fex, *mex, eex;
		unsigned short flags;

		flags = pr->pr_flags;
		if (!ismaskedfield(ex, &fex, &mex, &flags))
			return ET_COMPLEX;
		if (mex->ex_op == EXOP_ADDRESS)
			return ET_COMPLEX;
		assert(mex->ex_op == EXOP_NUMBER);
		if (!(flags & PR_COMPILETEST)
		    && (mex->ex_val & mex->ex_val-1) != 0) {
			return ET_COMPLEX;
		}
		eex.ex_op = EXOP_EQ;
		eex.ex_left = ex;
		eex.ex_right = mex;
		return compile(&eex, pr, offset, sfs);
	  }

	  /*
	   * Look for a protocol predicate, e.g. 'ip.tcp.ftp'.
	   */
	  case EXOP_PROTOCOL: {
		SnoopFilter *sf;
		ProtoCompiler pc;

		/*
		 * Initialize a protocompiler using the next free filter.
		 */
		rtype = ET_SIMPLE;
		sf = allocfilter(sfs, &rtype);
		pc_init(&pc, sf, offset, pr->pr_byteorder, &sfs->sfs_errflags,
			comperr);

		/*
		 * Match the type code of each component in a protocol path
		 * against its parent protocol's type field.
		 */
		for (;;) {
			/*
			 * Match the current protocol path component against
			 * its parent's discriminant field, unless the current
			 * protocol is "snoop".
			 */
			if (ex->ex_prsym->sym_proto != &snoop_proto) {
				if (pr->pr_discriminant == 0)
					return ET_COMPLEX;
				type = pr_compile(pr, pr->pr_discriminant, 0,
						  ex, &pc);
				if (type != ET_SIMPLE)
					return type;
				offset += pr->pr_maxhdrlen;
			}
			pr = ex->ex_prsym->sym_proto;
			ex = ex->ex_member;
			if (ex == 0)
				break;

			/*
			 * If there is an expression at the end of the path,
			 * compile it, then combine the resulting filter set
			 * with the path's filter in sfs.
			 */
			if (ex->ex_op != EXOP_PROTOCOL) {
				struct sfset tmp;
				int j;
				SnoopFilter *sfj, *sfk;

				sfs_init(&tmp);
				type = compile(ex, pr, offset, &tmp);
				if (type == ET_ERROR)
					return type;
				j = SNOOP_MAXFILTERS;
				for (sfj = &tmp.sfs_vec[j-1]; --j >= 0; --sfj) {
					if (!sfj->sf_allocated)
						continue;
					if (j > 0)
						sfk = allocfilter(sfs, &type);
					else
						sfk = sf;	/* reuse sf */
					joinfilters(sf, sfj, sfk);
				}
				sfs->sfs_errflags |= tmp.sfs_errflags;
				return type;
			}
			pc_setbyteorder(&pc, pr->pr_byteorder);
		}
		break;
	  }

	  case EXOP_FIELD: {
		ProtoCompiler pc;

		if (!(pr->pr_flags & PR_COMPILETEST))
			return ET_COMPLEX;
		rtype = ET_SIMPLE;
		pc_init(&pc, allocfilter(sfs, &rtype), offset, pr->pr_byteorder,
			&sfs->sfs_errflags, comperr);
		type = pr_compile(pr, ex->ex_field, 0, 0, &pc);
		break;
	  }

	  /*
	   * Number or address, possibly connected by && or ||.
	   */
	  case EXOP_NUMBER:
	  case EXOP_ADDRESS:
		type = ET_SIMPLE;
		(void) allocfilter(sfs, &type);
		return type;

	  default:
		return ET_COMPLEX;
	}

	if (rtype != ET_SIMPLE)
		return rtype;
	return type;
}

/*
 * Given two masks, lm and rm, set m to their conjunction.
 */
static void
joinmasks(unsigned long *lm, unsigned long *rm, unsigned long *m)
{
	int n;

	for (n = SNOOP_FILTERLEN; --n >= 0; m++, lm++, rm++)
		*m = *lm & *rm;
}

/*
 * Allocate a filter from sfs.  If there are no free filters, find the
 * two "most-overlapping" filters and merge them into the lower one, then
 * allocate and return the upper one.  Return ET_COMPLEX in type only if
 * two filters were merged to free a filter.
 */
static SnoopFilter *
allocfilter(struct sfset *sfs, ExprType *type)
{
	SnoopFilter *sf, *sf2;	/* filters to be joined */
	SnoopFilter *sfi, *sfj;	/* used to find best filter join */
	int max, count;		/* best and last joined mask bitcount */
	int i, j;		/* counters used with sfi and sfj */

	/*
	 * Try to allocate a new snoopfilter.
	 */
	if (sfs->sfs_elements < SNOOP_MAXFILTERS)
		return sfs_allocfilter(sfs);

	/*
	 * Out of filters.  Change type to complex so that our caller knows
	 * the filter expression cannot be compiled to snoopfilters, but must
	 * be saved and tested for each received packet.  Search for the best
	 * two filters to join, in order to free the second.
	 */
	*type = ET_COMPLEX;
	max = 0;
	i = SNOOP_MAXFILTERS;
	for (sfi = &sfs->sfs_vec[0]; --i > 0; sfi++) {
		assert(sfi->sf_allocated);
		j = i;
		for (sfj = sfi + 1; --j >= 0; sfj++) {
			unsigned long mask[SNOOP_FILTERLEN];
			unsigned long *m, v;
			int n;

			/*
			 * Make an imperfect filter mask given sfi and sfj.
			 * Count the 1 bits in it and remember the two filters
			 * which joined to yield the most mask bits.
			 */
			assert(sfj->sf_allocated);
			joinmasks(sfi->sf_match, sfj->sf_match, mask);
			count = 0;
			for (m = mask, n = SNOOP_FILTERLEN; --n >= 0; m++) {
				v = *m;
				while (v) {
					v &= v - 1;
					count++;
				}
			}
			if (count > max) {
				max = count;
				sf = sfi;
				sf2 = sfj;
			}
		}
	}

	/*
	 * If max is zero, all filter masks are disjoint and we must become
	 * promiscuous.  Otherwise join sf2 into sf, freeing sf2.
	 */
	if (max == 0) {
		sfs_unifyfilters(sfs);
		return sfs_allocfilter(sfs);
	}
	joinmasks(sf->sf_match, sf2->sf_match, sf->sf_mask);
	bzero(sf2, sizeof *sf2);
	sf2->sf_allocated = 1;
	return sf2;
}

/*
 * Logically conjoin lsf and rsf into sf.
 */
static void
joinfilters(SnoopFilter *lsf, SnoopFilter *rsf, SnoopFilter *sf)
{
	int i;

	for (i = 0; i < SNOOP_FILTERLEN; i++) {
		sf->sf_mask[i] = lsf->sf_mask[i] | rsf->sf_mask[i];
		sf->sf_match[i] = lsf->sf_match[i] | rsf->sf_match[i];
	}
}

static int
ismaskedfield(Expr *ex, Expr **fexp, Expr **mexp, unsigned short *flags)
{
	if (ex->ex_op == EXOP_BITAND) {
		if (isfield(ex->ex_left, flags)
		    && (ex->ex_right->ex_op == EXOP_NUMBER
			|| ex->ex_right->ex_op == EXOP_ADDRESS)) {
			*fexp = ex->ex_left;
			*mexp = ex->ex_right;
			return 1;
		}
		if (isfield(ex->ex_right, flags)
		    && (ex->ex_left->ex_op == EXOP_NUMBER
			|| ex->ex_left->ex_op == EXOP_ADDRESS)) {
			*fexp = ex->ex_right;
			*mexp = ex->ex_left;
			return 1;
		}
		return 0;
	}
	if (isfield(ex, flags)) {
		*fexp = ex;
		*mexp = 0;
		return 1;
	}
	return 0;
}

static int
isfield(Expr *ex, unsigned short *flags)
{
	do {
		if (ex->ex_op == EXOP_FIELD || ex->ex_op == EXOP_FETCH)
			return 1;
		if (ex->ex_op != EXOP_PROTOCOL)
			return 0;
		if (flags)
			*flags = ex->ex_prsym->sym_proto->pr_flags;
	} while ((ex = ex->ex_member) != 0);
	return 0;
}

static ExprType
compile_fetch(Expr *fex, Expr *tex, ProtoCompiler *pc)
{
	unsigned int size;

	if (tex->ex_op != fex->ex_type) {
		pc_badop(pc, tex);
		return ET_ERROR;
	}
	if (!pc_skipbytes(pc, fex->ex_off))
		return ET_COMPLEX;
	size = fex->ex_size;
	switch (fex->ex_type) {
	  case EXOP_NUMBER:
		if (!pc_int(pc, PC_ALLINTBITS, tex->ex_val, size))
			return ET_COMPLEX;
		break;
	  case EXOP_ADDRESS:
		if (!pc_bytes(pc, PC_ALLBYTES,
			      (char *) A_BASE(&tex->ex_addr, size), size)) {
			return ET_COMPLEX;
		}
		break;
	  case EXOP_STRING:
		if (!pc_bytes(pc, PC_ALLBYTES, tex->ex_str.s_ptr,
			      MIN(tex->ex_str.s_len, size))) {
			return ET_COMPLEX;
		}
	}
	return ET_SIMPLE;
}

void
sfs_unifyfilters(struct sfset *sfs)
{
	SnoopFilter *sf0, *sf;
	int i;

	if (sfs->sfs_elements == 0)
		return;
	sf0 = &sfs->sfs_vec[0];
	assert(sf0->sf_allocated);
	i = SNOOP_MAXFILTERS;
	for (sf = &sfs->sfs_vec[1]; --i > 0; sf++) {
		if (!sf->sf_allocated)
			continue;
		joinmasks(sf->sf_match, sf0->sf_match, sf0->sf_mask);
		bzero(sf, sizeof *sf);
	}
	sfs->sfs_elements = 1;
}
