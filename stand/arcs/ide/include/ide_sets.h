#ifndef __IDE_SETS_H__
#define __IDE_SETS_H__

/*
 * ide_sets.h
 *
 * Data structures, definitions, and prototypes relating to
 * ide sets, an enhancement for multiprocessing support.
 *
 */

#ident "$Revision: 1.5 $"

/* 
 * define the stuff needed to implement cpu sets as a builtin symbol type.
 * Use bitfields to represent membership; new "long long" datatype allows
 * 64 member maximum setsize.  Sets are used to manipulate cpus in groups,
 * and were added as part of the MP enhancements for Everest.  Arbitrarily
 * designate most significant (i.e. leftmost) bit of long long datatype 
 * as member 0 in the bitfield layout.  The members then number from left
 * to right, which seems intuitive when visually comparing a hex
 * representation to its "set" bitfield interpretation.  It
 * also simplifies suppression of leading zeros.
 */

#define MAXSNAMELEN	MAXIDENT
#define BITSPERLL	64
#define MAXSETSIZE	(BITSPERLL)

#define BITSPERHEXDIGIT	4
#define LLHEXDIGITS     (BITSPERLL / BITSPERHEXDIGIT)

#define SET_ZEROHEXDIG ((set_t)0xf << (BITSPERLL-BITSPERHEXDIGIT))
#define SET_ZEROBIT	((set_t)0x1 << (BITSPERLL-1))



typedef enum setfns {
	CREATE_SET=0, COPY_SET, DELETE_SET,			/* 0 -- 2 */
	DISPLAY_SET, ADD_CPU, DEL_CPU,				/* 3 -- 5 */
	CPU_INFO, pad7, pad8,					/* 6 -- 8 */
	pad9, pad10, SET_UNION,					/* 9 -- 11 */
	SET_DIFFER, SET_INTER, SET_EQUALITY,			/* 12 -- 14 */
	SET_INEQUALITY, SET_INCLUSION, CPU_IN, 			/* 15 -- 17 */
	SET_EMPTY, SET_EXISTS, pad20				/* 18 -- 20 */
} setfns_t;


/*
 * define the list of options that each of the ide set routines 
 * accepts, in getopts() format.
 */
#define CREATE_SET_OPTS		"aefs:c:"	/* 1 or more sets	  */
#define COPY_SET_OPTS		"s:"		/* 1 srcset, >=1 destsets */
#define DISPLAY_SET_OPTS	"s:"		/* 1 or more sets	  */
#define SET_UNION_OPTS		"s:"		/* 3 sets: src1,src2,dest */
#define SET_DIFF_OPTS		"s:"		/*	 "	"	  */
#define SET_INTER_OPTS		"s:"		/* 	 "	"	  */
#define SET_EQUALITY_OPTS	"s:"		/* 2 src sets		  */
#define SET_INEQUALITY_OPTS	"s:"		/* 	 "   		  */
#define SET_INCLUSION_OPTS	"s:"		/* 	 "   		  */
#define CPU_IN_OPTS		"s:v:"		/* 1 src set + 1 vpid  */
#define SET_EMPTY_OPTS		"s:"		/* 1 src set 		  */
#define SET_EXISTS_OPTS		"s:"		/* 1 src set 		  */

#define ADD_CPU_OPTS		"s:v:"		/* 1 or more sets & vpids */
#define DEL_CPU_OPTS		"s:v:"		/* 	 "	"	  */



typedef struct setlist_s { 
  int setcnt;				/* # of sets in 'setptrs' list */
  sym_t *setptrs[MAX_OPTARGS];		/* symptrs to sets */
} setlist_t;

typedef struct vplist_s { 
  int vpcnt;			/* # of virtual process ids in 'vps' list */
  int vps[MAX_OPTARGS];		/* list of vids */
} vplist_t;

/* many set operations or primitives allow the operation to be done to
 * a group of sets, and some also allow groups of vpids to be specified
 * (to be added or deleted from a set or sets, for example).  This concept
 * is reflected by a setcnt or vpcnt of ONE_OR_MORE_ARGS.
 */
#define ONE_OR_MORE_ARGS	(MAX_OPTARGS+1)

#define SET1	0
#define SET2	1
#define SET3	2

/* set_{union,difference,intersection} are 3-address operations:
 * two source sets + destination set
 */
#define SRC1	SET1
#define SRC2	SET2
#define DEST	SET3

/* set execution requires 3 sets to completely represent the results of
 * a sequential run for all participating cpus:
 * 
 * <RUNset, HUNGset, ERRORset>
 * - RUNset is the set of cpus on which the user wants the diag to execute.
 * - HUNGset is the set of cpus which didn't finish executing the function
 *   normally--i.e. when a timeout occurred the function was still
 *   ostensibly running, or an exception or interrupt occurred with
 *   nofault == NULL, meaning that the function was not anticipating
 *   the fault.  This return does NOT, however, imply anything about the
 *   return values (i.e. success or failure) of the diagnostic.
 * - ERRORset contains the (binary) return values of the diags' execution
 *   on each of the cpus: 0 == PASSED, 1 == FAILED; therefore a cpu's 
 *   presence in this set means that the diagnostic detected an error during
 *   execution, and therefore returned a nonzero result.  So, when dealing
 *   with set execution three distinct sets are necessary, ordered as above.
 */ 
#define RUN_SET			SET1
#define HUNG_SET		SET2
#define ERROR_SET		SET3


typedef struct return_kludge_s {
	int silent;
	int errno;
} r_kludge_t;


/* (sym_t *) versions (i.e. internally-called only) of *_set routines */
extern int	_display_set(sym_t *, int);
extern sym_t *	_set_exists(char *, r_kludge_t *);

extern int      _check_sets(option_t *, setlist_t *, int);
extern int      _check_vpids(option_t *, vplist_t *);

/* sets given as parameters to set operations must exist and be of basetype
 * SYMB_SET.  cpu numbers are range-checked.
 */
#define BAD_SET1	-1
#define BAD_SET2	-2
#define BAD_SET3	-3
#define BAD_CPUNUM	-4

/* prototypes of exported (script-level) set operations */
extern int	create_set(int, char **);
extern int	copy_set(int, char **);
extern int	delete_set(int, char **);
extern int	display_set(int, char **);
extern int	_add_cpu(int, char **);
extern int	_del_cpu(int, char **);

/* 7 classic set-operation primitives are provided.
 * They require 1, 2, or 3 setnames depending upon the operation;
 *   these are specified using the '-s' flag, followed by the proper
 *   number of comma-separated names, e.g. '-s set1,set2'.
 * All return ints as follows:
 *   == 0: iff the routine encountered no errors and succeeded (a la shell
 *	exit status) or (for TRUE/FALSE operations) evaluated to TRUE.
 *   < 0:  if errors occurred (see definitions of set errors below),
 *   > 0:  [TRUE/FALSE operations only] -- no errors occurred, but 
 *	the operation evaluated FALSE.
 * 
 * set_union, set_difference, and set_intersection each require three 
 *    setnames, since they perform the specified operation on two sets
 *    and place the result in a third.  Note that one of the source sets
 *    may be used for the operation result if desired (by specifying it
 *    as the 3rd setname).
 * set_equality, set_inequality, and set_inclusion require two setnames;
 *   'set_inclusion -s setA,setB' determines whether setB is a proper 
 *	subset of setA.
 * cpu_in requires a setname and a vp number:
 * e.g. 'cpu_in -s setname -v vid'
 *
 * For fun, add two additional simple set functions (which aren't necessary
 * because the above routines can synthesize them):
 * 'set_empty -s setname' and
 * 'set_exists -s setname'
 */

extern int set_union(int, char **);
extern int set_difference(int, char **);
extern int set_intersection(int, char **);
extern int set_equality(int, char **);
extern int set_inequality(int, char **);
extern int set_inclusion(int, char **);
extern int cpu_in(int, char **);
extern int set_empty(int, char **);
extern int set_exists(int, char **);
extern int	_do_setfnprep(optlist_t *, setlist_t *, vplist_t *, int);



#endif /* __IDE_SETS_H__ */
