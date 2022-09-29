
/************************* Pm_parse.h ********************************
 *                                                                   *
 *          Include file for top-down Parse machine                  *
 *                                                                   *
 *********************************************************************/

/**********************************************************************

	Pm: Top-down Parse machine

  Copyright (c) 1995 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for non-commercial purposes
  and without fee is hereby granted, provided that the above copyright
  notice appear in all copies and that both the copyright notice and
  this permission notice appear in supporting documentation. and that
  any documentation, advertising materials, and other materials related
  to such distribution and use acknowledge that the software was
  developed by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

/*
 *	Format of one Parse machine (Pm) instruction word.
 */
typedef struct {
	u_char		 Pmi_class;	/*  Operation Class		*/
	u_char		 Pmi_op;	/*  Operation # within class	*/
	short		 Pmi_next;	/*  Next instr, rel to seq base	*/
	char		*Pmi_parm;	/*  Parameter string		*/
} Pm_inst;

/*
 *	Define Operation Class
 */
typedef enum {
	PmC_END = 0,
	PmC_SHOW,	/* Write optional string and return error	*/
	PmC_OP,		/* Invocation of intrinsic recognition operator */
	PmC_NOT_OP,	/* Negated invocation of intrinsic op		*/
	PmC_SUB,	/* Recursively invocation of sub recognizer seq	*/
	PmC_NOT_SUB,	/* Negated recursive invocation			*/
	PmC_FILE,	/* Map keyword-> string via file, invoke on strg*/
	PmC_NOT_FILE,	/* Hegated FILE					*/
	PmC_SET,	/* Set result value				*/
	PmC_ACTION,	/* Perform action				*/
	PmC_LABEL,	/* Label beginning of recognizer sequence	*/
	PmC_TRV		/* Transfer-vector entry at bottom of memory	*/
}  Pm_Class_t;

/*
 *	Intrinsic recognizer operations  (class PmC_OP and PmC_NOT_OP)
 */
typedef enum {
	PmO_none = 0,
	PmO_Integer,
	PmO_cString,
	PmO_Char,
	PmO_Flag,
	PmO_Host,
	PmO_EOL,
	PmO_WhSp
} Pm_Op_t;

/*
 *	Return values for Pmi_next field of operation
 */
#define  Pm_Ret		32764
#define  OK		-1 - Pm_Ret
#define  NO		-2 - Pm_Ret
#define  ERR		-3 - Pm_Ret

/*
 *	Recognition program macros
 */
#define Is(label, showtxt, next)     {PmC_SUB, label, (next), showtxt}
#define IsNot(label, showtxt, next)  {PmC_NOT_SUB, label, (next), showtxt}

#define InFile(label, chstr, next)   {PmC_FILE, label, (next), chstr}
#define Not_InFile(label, chstr, next)  PmC_NOT_FILE, label, (next), chstr}

#define String(str, next)            {PmC_OP, PmO_cString, (next), str}
#define Not_String(str, next)        {PmC_NOT_OP, PmO_cString, (next), str}

#define Integer(showtxt, next)       {PmC_OP, PmO_Integer, (next), showtxt}
#define Not_Integer(showtxt, next)   {PmC_NOT_OP, PmO_Integer, (next), showtxt}

#define EOL(showtxt, next)	     {PmC_OP, PmO_EOL, (next), showtxt}
#define Not_EOL(showtxt, next)	     {PmC_NOT_OP, PmO_EOL, (next), showtxt}

#define WhSp(showtxt, next)	     {PmC_OP, PmO_WhSp, (next), showtxt}
#define Not_WhSp(showtxt, next)	     {PmC_NOT_OP, PmO_WhSp, (next), showtxt}

#define Host(showtxt, next)	     {PmC_OP, PmO_Host, (next), showtxt}
#define Not_Host(showtxt, next)	     {PmC_NOT_OP, PmO_Host, (next), showtxt}

#define Flags(flgstr, next)          {PmC_OP, PmO_Flag, (next), flgstr}

#define Char(chstr, next)            {PmC_OP, PmO_Char, (next), chstr}
#define Not_Char(chstr, next)        {PmC_NOT_OP, PmO_Char, (next), chstr}

#define Action(todo, parm, next)     {PmC_ACTION, todo, (next), (parm)}
#define SetInt(parm, next)           {PmC_SET, PmO_Integer, (next), parm}
#define SetLit(parm, next)           {PmC_SET, 0, (next), (char *)parm}
#define Print(showtxt, next)         {PmC_SHOW, 0, (next), showtxt}
#define Go(next)		     {PmC_SHOW, 0, (next), NULL}
#define Label(label)		     {PmC_LABEL, 0, label, NULL}

int	Parse_machine(Pm_inst *, int);
int	Pm_Init(Pm_inst *);
int	Pm_Action(int, int, int);

/* these are defined in Pm_parse.c - mwang */
extern int		Pm_debug;	/* Debug output flag */
extern char	       *Pm_cp;		/* Scan pointer */
extern int		Pm_vi;		/* Value vector index */
extern u_int32_t	Pm_val[];	/* Value vector */

#define val0	Pm_val[0]
#define val1	Pm_val[1]
#define val2	Pm_val[2]
#define val3	Pm_val[3]
#define Append_Val(x) {Pm_val[Pm_vi++] = (u_int32_t) (x);  Pm_val[Pm_vi] = 0;}
