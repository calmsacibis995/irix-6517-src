/*    regcomp.h
 */

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	sv that must begin a match; Nullch if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 *  [regmust changed to SV* for bminstr()--law]
 * regmlen	length of regmust string
 *  [regmlen not used currently]
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that pregcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in pregexec() needs it and pregcomp() is computing
 * it anyway.
 * [regmust is now supplied always.  The tests that use regmust have a
 * heuristic that disables the test if it usually matches.]
 *
 * [In fact, we now use regmust in many cases to locate where the search
 * starts in the string, so if regback is >= 0, the regmust search is never
 * wasted effort.  The regback variable says how many characters back from
 * where regmust matched is the earliest possible start of the match.
 * For instance, /[a-z].foo/ has a regmust of 'foo' and a regback of 2.]
 */

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* definition	number	opnd?	meaning */
#define	END	 0	/* no	End of program. */
#define	BOL	 1	/* no	Match "" at beginning of line. */
#define MBOL	 2	/* no	Same, assuming multiline. */
#define SBOL	 3	/* no	Same, assuming singleline. */
#define	EOL	 4	/* no	Match "" at end of line. */
#define MEOL	 5	/* no	Same, assuming multiline. */
#define SEOL	 6	/* no	Same, assuming singleline. */
#define	ANY	 7	/* no	Match any one character (except newline). */
#define	SANY	 8	/* no	Match any one character. */
#define	ANYOF	 9	/* sv	Match character in (or not in) this class. */
#define	CURLY	10	/* sv	Match this simple thing {n,m} times. */
#define	CURLYX	11	/* sv	Match this complex thing {n,m} times. */
#define	BRANCH	12	/* node	Match this alternative, or the next... */
#define	BACK	13	/* no	Match "", "next" ptr points backward. */
#define	EXACT	14	/* sv	Match this string (preceded by length). */
#define	EXACTF	15	/* sv	Match this string, folded (prec. by length). */
#define	EXACTFL	16	/* sv	Match this string, folded in locale (w/len). */
#define	NOTHING	17	/* no	Match empty string. */
#define	STAR	18	/* node	Match this (simple) thing 0 or more times. */
#define	PLUS	19	/* node	Match this (simple) thing 1 or more times. */
#define BOUND	20	/* no	Match "" at any word boundary */
#define BOUNDL	21	/* no	Match "" at any word boundary */
#define NBOUND	22	/* no	Match "" at any word non-boundary */
#define NBOUNDL	23	/* no	Match "" at any word non-boundary */
#define REF	24	/* num	Match already matched string */
#define REFF	25	/* num	Match already matched string, folded */
#define REFFL	26	/* num	Match already matched string, folded in loc. */
#define	OPEN	27	/* num	Mark this point in input as start of #n. */
#define	CLOSE	28	/* num	Analogous to OPEN. */
#define MINMOD	29	/* no	Next operator is not greedy. */
#define GPOS	30	/* no	Matches where last m//g left off. */
#define IFMATCH	31	/* no	Succeeds if the following matches. */
#define UNLESSM	32	/* no	Fails if the following matches. */
#define SUCCEED	33	/* no	Return from a subroutine, basically. */
#define WHILEM	34	/* no	Do curly processing and see if rest matches. */
#define ALNUM	35	/* no	Match any alphanumeric character */
#define ALNUML	36 	/* no	Match any alphanumeric char in locale */
#define NALNUM	37	/* no	Match any non-alphanumeric character */
#define NALNUML	38	/* no	Match any non-alphanumeric char in locale */
#define SPACE	39	/* no	Match any whitespace character */
#define SPACEL	40	/* no	Match any whitespace char in locale */
#define NSPACE	41	/* no	Match any non-whitespace character */
#define NSPACEL	42	/* no	Match any non-whitespace char in locale */
#define DIGIT	43	/* no	Match any numeric character */
#define NDIGIT	44	/* no	Match any non-numeric character */

/*
 * Opcode notes:
 *
 * BRANCH	The set of branches constituting a single choice are hooked
 *		together with their "next" pointers, since precedence prevents
 *		anything being concatenated to any individual branch.  The
 *		"next" pointer of the last BRANCH in a choice points to the
 *		thing following the whole choice.  This is also where the
 *		final "next" pointer of each individual branch points; each
 *		branch starts with the operand node of a BRANCH node.
 *
 * BACK		Normal "next" pointers all implicitly point forward; BACK
 *		exists to make loop structures possible.
 *
 * STAR,PLUS	'?', and complex '*' and '+', are implemented as circular
 *		BRANCH structures using BACK.  Simple cases (one character
 *		per match) are implemented with STAR and PLUS for speed
 *		and to minimize recursive plunges.
 *
 * OPEN,CLOSE	...are numbered at compile time.
 */

#ifndef DOINIT
EXT char regarglen[];
#else
EXT char regarglen[] = {
    0,0,0,0,0,0,0,0,0,0,
    /*CURLY*/ 4, /*CURLYX*/ 4,
    0,0,0,0,0,0,0,0,0,0,0,0,
    /*REF*/ 2, 2, 2, /*OPEN*/ 2, /*CLOSE*/ 2,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
#endif

#ifndef DOINIT
EXT char regkind[];
#else
EXT char regkind[] = {
	END,
	BOL,
	BOL,
	BOL,
	EOL,
	EOL,
	EOL,
	ANY,
	ANY,
	ANYOF,
	CURLY,
	CURLY,
	BRANCH,
	BACK,
	EXACT,
	EXACT,
	EXACT,
	NOTHING,
	STAR,
	PLUS,
	BOUND,
	BOUND,
	NBOUND,
	NBOUND,
	REF,
	REF,
	REF,
	OPEN,
	CLOSE,
	MINMOD,
	GPOS,
	BRANCH,
	BRANCH,
	END,
	WHILEM,
	ALNUM,
	ALNUM,
	NALNUM,
	NALNUM,
	SPACE,
	SPACE,
	NSPACE,
	NSPACE,
	DIGIT,
	NDIGIT,
};
#endif

/* The following have no fixed length. */
#ifndef DOINIT
EXT char varies[];
#else
EXT char varies[] = {
    BRANCH, BACK, STAR, PLUS, CURLY, CURLYX, REF, REFF, REFFL, WHILEM, 0
};
#endif

/* The following always have a length of 1. */
#ifndef DOINIT
EXT char simple[];
#else
EXT char simple[] = {
    ANY, SANY, ANYOF,
    ALNUM, ALNUML, NALNUM, NALNUML,
    SPACE, SPACEL, NSPACE, NSPACEL,
    DIGIT, NDIGIT, 0
};
#endif

EXT char regdummy;

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 *
 * [If REGALIGN is defined, the "next" pointer is always aligned on an even
 * boundary, and reads the offset directly as a short.  Also, there is no
 * special test to reverse the sign of BACK pointers since the offset is
 * stored negative.]
 */

#ifndef gould
#ifndef cray
#ifndef eta10
#define REGALIGN
#endif
#endif
#endif

#define	OP(p)	(*(p))

#ifndef lint
#ifdef REGALIGN
#define NEXT(p) (*(short*)(p+1))
#define ARG1(p) (*(unsigned short*)(p+3))
#define ARG2(p) (*(unsigned short*)(p+5))
#else
#define	NEXT(p)	(((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define	ARG1(p)	(((*((p)+3)&0377)<<8) + (*((p)+4)&0377))
#define	ARG2(p)	(((*((p)+5)&0377)<<8) + (*((p)+6)&0377))
#endif
#else /* lint */
#define NEXT(p) 0
#endif /* lint */

#define	OPERAND(p)	((p) + 3)

#ifdef REGALIGN
#define	NEXTOPER(p)	((p) + 4)
#define	PREVOPER(p)	((p) - 4)
#else
#define	NEXTOPER(p)	((p) + 3)
#define	PREVOPER(p)	((p) - 3)
#endif

#define MAGIC 0234

/* Flags for first parameter byte of ANYOF */
#define ANYOF_INVERT	0x40
#define ANYOF_FOLD	0x20
#define ANYOF_LOCALE	0x10
#define ANYOF_ISA	0x0F
#define ANYOF_ALNUML	 0x08
#define ANYOF_NALNUML	 0x04
#define ANYOF_SPACEL	 0x02
#define ANYOF_NSPACEL	 0x01

/*
 * Utility definitions.
 */
#ifndef lint
#ifndef CHARMASK
#define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARMASK)
#endif
#else /* lint */
#define UCHARAT(p)	regdummy
#endif /* lint */

#define	FAIL(m)	croak("/%.127s/: %s",regprecomp,m)
