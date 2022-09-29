/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Evaluate a packet data expression in a protocol context.
 * NB: eval knows that ex_kid is equivalent to ex_right.
 */
#include "datastream.h"
#include "debug.h"
#include "expr.h"
#include "protocol.h"
#include "protostack.h"
#include "scope.h"
#include "snooper.h"

extern long	_now;

/*
 * Invariant arguments to recursive eval and test.
 */
typedef struct {
	Protocol	*proto;
	DataStream	*stream;
	ProtoStack	*stack;
} EvalData;

static ExprError	*evalerr;

static int eval(Expr *, EvalData *, Expr *);
static int test(Expr *, EvalData *);
static int mismatch(Expr *, Expr *);

int
pr_eval(Protocol *pr, Expr *ex, SnoopPacket *sp, int len, Snooper *sn,
	ExprError *err, Expr *rex)
{
	EvalData ed;
	DataStream ds;
	ProtoStack ps;

	_now = sp->sp_timestamp.tv_sec;
	ed.proto = pr;
	ds_init(&ds, sp->sp_data + sn->sn_rawhdrpad, len - sn->sn_rawhdrpad,
		DS_DECODE, pr->pr_byteorder);
	ed.stream = &ds;
	PS_INIT(&ps, sn, &sp->sp_hdr);
	ed.stack = &ps;
	evalerr = err;
	ex_null(rex);
	return eval(ex, &ed, rex);
}

/*
 * Evaluate a boolean expression given a protocol and packet data starting
 * with that protocol's frame.
 * XXX ExprError * result parameter?
 */
int
pr_test(Protocol *pr, Expr *ex, SnoopPacket *sp, int len, Snooper *sn)
{
	EvalData ed;
	DataStream ds;
	ProtoStack ps;
	ExprError err;

	_now = sp->sp_timestamp.tv_sec;
	ed.proto = pr;
	ds_init(&ds, sp->sp_data + sn->sn_rawhdrpad, len - sn->sn_rawhdrpad,
		DS_DECODE, pr->pr_byteorder);
	ed.stream = &ds;
	PS_INIT(&ps, sn, &sp->sp_hdr);
	ed.stack = &ps;
	evalerr = &err;
	return test(ex, &ed);
}

/*
 * Evaluate an expression into a number, an address, or a string.
 */
static int
eval(Expr *ex, EvalData *ed, Expr *rex)
{
	switch (ex->ex_op) {
	  case EXOP_OR:
		/*
		 * Short-circuiting logical or.
		 */
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = test(ex->ex_left, ed) || test(ex->ex_right, ed);
		break;

	  case EXOP_AND:
		/*
		 * Short-circuiting logical and.
		 */
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = test(ex->ex_left, ed) && test(ex->ex_right, ed);
		break;

	  case EXOP_ARRAY: {
		Expr tex;
		int index;
		Protocol *pr;
		DataStream ds;

		/*
		 * Evaluate the index expression.
		 */
		if (!eval(ex->ex_right, ed, &tex))
			return 0;
		assert(tex.ex_op == EXOP_NUMBER);
		index = tex.ex_val;

		/*
		 * Evaluate the array lvalue.  After recurring through some
		 * number of array and struct nodes, this call will evaluate
		 * an array field, returning its element type field (strings
		 * are a special case).
		 */
		if (!eval(ex->ex_left, ed, &tex))
			return 0;
		if (tex.ex_op == EXOP_STRING) {
			if (index >= (unsigned)tex.ex_str.s_len)
				return 0;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tex.ex_str.s_ptr[index];
			break;
		}
		assert(tex.ex_op == EXOP_FIELD);

		/*
		 * Set the element type field's cookie to the desired index
		 * and fetch the rvalue.
		 */
		tex.ex_field->pf_cookie = index;
		pr = ed->proto;
		ds_init(&ds, ed->stream->ds_next, ed->stream->ds_count,
			DS_DECODE, pr->pr_byteorder);
		if (!pr_fetch(pr, tex.ex_field, &ds, ed->stack, rex))
			return 0;
		break;
	  }

	  case EXOP_STRUCT: {
		Expr tex;
		DataStream *ds;
		int tell;

		/*
		 * Evaluate the structure-valued expression on the left.
		 * The underlying protocol returns the struct's field in tex,
		 * with its cookie set to the stream offset of the beginning
		 * of the struct relative to protocol origin.
		 */
		if (!eval(ex->ex_left, ed, &tex))
			return 0;
		assert(tex.ex_op == EXOP_FIELD);
		assert(tex.ex_field->pf_type == EXOP_STRUCT);

		/*
		 * Seek to the struct base and evaluate the right part, which
		 * may be a field, an array index expression, or another struct
		 * member expression.
		 */
		ds = ed->stream;
		tell = DS_TELL(ds);
		if (!ds_seek(ds, tex.ex_field->pf_cookie, DS_RELATIVE)
		    || !eval(ex->ex_right, ed, rex)) {
			(void) ds_seek(ds, tell, DS_ABSOLUTE);
			return 0;
		}
		(void) ds_seek(ds, tell, DS_ABSOLUTE);
		break;
	  }

	  case EXOP_NOT:
		/*
		 * Logical not must call test rather than ex_operate/ex_test
		 * in order to handle protocol, array, and struct matching.
		 */
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = !test(ex->ex_kid, ed);
		break;

	  case EXOP_PROTOCOL: {
		Protocol *pr;
		DataStream ds;

		/*
		 * Clone ed->stream with pr's byte order.  Check for the
		 * snoop pseudo-protocol, which we need never match, but
		 * skip instead via ex_match.  For other protocols, call
		 * pr_match for all but the last in a path, and for the
		 * last too if it has no discriminant.
		 */
		pr = ed->proto;
		ds_init(&ds, ed->stream->ds_next, ed->stream->ds_count,
			DS_DECODE, pr->pr_byteorder);
		if (ex->ex_prsym->sym_proto == &snoop_proto) {
			if (!ex_match(ex, &ds, ed->stack, rex))
				return mismatch(ex, rex);
		} else if (ex->ex_member || pr->pr_discriminant == 0) {
			if (!pr_match(pr, ex, &ds, ed->stack, rex))
				return mismatch(ex, rex);
		} else {
			if (!pr_fetch(pr, pr->pr_discriminant, &ds, ed->stack,
				      rex)) {
				return mismatch(ex, rex);
			}
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val =
			    (rex->ex_val == ex->ex_prsym->sym_prototype);
		}
		break;
	  }

	  case EXOP_FIELD: {
		Protocol *pr;
		DataStream ds;

		pr = ed->proto;
		ds_init(&ds, ed->stream->ds_next, ed->stream->ds_count,
			DS_DECODE, pr->pr_byteorder);
		if (!pr_fetch(pr, ex->ex_field, &ds, ed->stack, rex))
			return 0;
		break;
	  }

	  case EXOP_FETCH: {
		DataStream ds;

		/*
		 * Clone ed->stream, seek to the field's base, and fetch
		 * the specified number of bytes.
		 */
		ds_init(&ds, ed->stream->ds_next, ed->stream->ds_count,
			DS_DECODE, ed->proto->pr_byteorder);
		if (!ds_seek(&ds, ex->ex_off, DS_RELATIVE))
			return 0;
		switch (ex->ex_type) {
		  case EXOP_NUMBER:
			if (!ds_int(&ds, &rex->ex_val, ex->ex_size,
				    DS_ZERO_EXTEND)) {
				return 0;
			}
			rex->ex_op = EXOP_NUMBER;
			break;
		  case EXOP_ADDRESS:
			if (!ds_bytes(&ds, A_BASE(&rex->ex_addr, ex->ex_size),
				      ex->ex_size)) {
				return 0;
			}
			rex->ex_op = EXOP_ADDRESS;
			break;
		  case EXOP_STRING:
			rex->ex_str.s_ptr = (char *) ds.ds_next;
			if (!ds_seek(&ds, ex->ex_size, DS_RELATIVE))
				return 0;
			rex->ex_str.s_len = ex->ex_size;
			rex->ex_op = EXOP_STRING;
		}
		break;
	  }

	  case EXOP_CALL: {
		int argc;
		Expr argv[MAXCALLARGS];
		DataStream ds;

		/*
		 * Evaluate any arguments, terminating the list if there are
		 * fewer than MAXCALLARGS with an EXOP_NULL node in argv, then
		 * call the protocol function.
		 */
		for (argc = 0; argc < MAXCALLARGS; argc++) {
			if (ex->ex_args[argc] == 0) {
				argv[argc].ex_op = EXOP_NULL;
				break;
			}
			ex_null(&argv[argc]);
			if (!eval(ex->ex_args[argc], ed, &argv[argc]))
				return 0;
		}
		ds_init(&ds, ed->stream->ds_next, ed->stream->ds_count,
			DS_DECODE, ed->proto->pr_byteorder);
		if (!(*ex->ex_func)(argv, &ds, ed->stack, rex))
			return 0;
		break;
	  }

	  case EXOP_NUMBER:
	  case EXOP_ADDRESS:
	  case EXOP_STRING:
		*rex = *ex;
		break;

	  default: {
		Expr *lex, left;	/* left kid's result if binary */

		assert(ex->ex_arity != EX_NULLARY);
		switch (ex->ex_arity) {
		  case EX_BINARY:
			lex = &left;
			ex_null(lex);
			if (!eval(ex->ex_left, ed, lex))
				return 0;
			break;
		  case EX_UNARY:
			lex = 0;
		}
		if (!eval(ex->ex_kid, ed, rex))
			return 0;
		if (ex_operate(rex, ex->ex_op, lex, rex, evalerr)
		    != ET_SIMPLE) {
			return 0;
		}
	  }
	}
	return 1;
}

static int
test(Expr *ex, EvalData *ed)
{
	Expr rex;
	enum exop type;

	ex_null(&rex);
	if (!eval(ex, ed, &rex))
		return 0;
	switch (rex.ex_op) {
	  case EXOP_PROTOCOL:
		return 1;
	  case EXOP_FIELD:
		type = rex.ex_field->pf_type;
		return (type == EXOP_ARRAY || type == EXOP_STRUCT);
	  default:
		return ex_test(&rex);
	}
}

static int
mismatch(Expr *pex, Expr *rex)
{
	for (;;) {
		pex = pex->ex_member;
		if (pex == 0) {
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = 0;
			return 1;
		}
		if (pex->ex_op != EXOP_PROTOCOL)
			return 0;
	}
	/* NOTREACHED */
}

int
ex_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	Protocol *pr;
	Expr *ex;
	EvalData ed;

	pr = pex->ex_prsym->sym_proto;
	ex = pex->ex_member;
	if (ex == 0) {
		if (pr->pr_flags & PR_MATCHNOTIFY)
			(void) pr_match(pr, 0, ds, ps, rex);
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = 1;
		return 1;
	}

	ed.proto = pr;
	ed.stream = ds;
	ed.stack = ps;
	return eval(ex, &ed, rex);
}
