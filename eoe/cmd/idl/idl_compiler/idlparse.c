/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: idlparse.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.5.5  1993/01/03  21:39:51  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:37  bbelch]
 *
 * Revision 1.1.5.4  1992/12/23  18:47:40  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:52  zeliff]
 * 
 * Revision 1.1.5.3  1992/12/04  17:09:58  marty
 * 	OSC1.1.1 upgrade.
 * 	[1992/12/04  17:01:34  marty]
 * 
 * Revision 1.1.7.2  1992/11/17  19:59:38  marty
 * 	OSC1.1.1 upgrade
 * 	[1992/11/17  19:57:58  marty]
 * 
 * Revision 1.1.5.2  1992/09/29  20:41:04  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:35:01  weisman]
 * 
 * Revision 1.1.2.2  1992/04/14  12:43:29  mishkin
 * 	Add stuff for HPUX lex/yacc
 * 	[1992/04/10  19:35:41  mishkin]
 * 
 * Revision 1.1  1992/01/19  03:01:18  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      IDLPARSE.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      This file deals with the problem of using yacc recursively.  On
**      platforms which define the yacc variables as static, there is no way to
**      save, reset, and later restore the current yacc state without having
**      the C code in the same file as the (staticly) declared variables.  The
**      code cannot be placed in the idl.y file because some yacc's (e.g.
**      SunOS 4.1) place any C code *before* the yacc variable definitions.  To
**      ensure that the yacc variables are defined prior to using them, this
**      file includes the yacc-generated code at the beginning of the file.
**
**     Layout for this file is as follows:
**
**          1. include yacc-generated file (from idl.y).
**          2. declare lex variables which must be saved/restored (see comment)
**          3. define the save/restore structure.
**          4. define save and restore routines.
**
**  VERSION: DCE 1.0
*/
 
/*
 * We assume sysdep.h is included by this file.
 */
#include <y_tab.c>   /* generated by yacc from idl.y */
 
/*
 * Variables declared in the generated yacc code (included above) :
 *
 *      -- common definitions --
 *      YYSTYPE yylval;
 *      YYSTYPE yyval;
 *      int yychar;
 *      int yynerrs;
 *      YACC_INT yyerrflag;
 *
 *      -- SunOS 4.0 definitions --
 *      -- Ultrix 3.1 definitions --
 *      YYSTYPE yyv[YYMAXDEPTH];
 *
 *      -- SunOS 4.1 --
 *      static YYSTYPE *yyv;
 *      static int *yys;
 *      static YYSTYPE *yypv;
 *      static int yystate;
 *      static int yytmp;
 *
 *      -- SVR4 definitions --
 *      int yy_yys[YYMAXDEPTH];
 *      int *yys;
 *      YYSTYPE yy_yyv[YYMAXDEPTH];
 *      YYSTYPE *yyv;
 */
 
/*
 * Declare definitions for yyv and yys variables.  The problem is that
 * some yacc's declare these as global variables, and others declare
 * a pointer and call malloc to define the array.
 *
 * Currently, platforms where yys is local to yyparse() declare this
 * as an array.  This could be optimized to use a pointer, or even not
 * use the variable at all.
 */
#if     defined (SVR4_LEX_YACC)
/*
 * SVR4 has both, and names the arrays yy_yy{v,s} and the pointers yy{v,s}
 */
# define NIDL_YYV_DEFINITION    YYSTYPE yy_yyv[YYMAXDEPTH], *yyv
# define NIDL_YYS_DEFINITION    YACC_INT yy_yys[YYMAXDEPTH], *yys
# define NIDL_YACC_YYV_ARRAY
# define NIDL_YACC_YYS_ARRAY

#elif   !(defined(SUN_LEX_YACC) && defined(SUN_41_LEX_YACC))
# define NIDL_YYV_DEFINITION    YYSTYPE yyv[YYMAXDEPTH]
# define NIDL_YYS_DEFINITION    YACC_INT yys[YYMAXDEPTH]
 
# define NIDL_YACC_YYV_ARRAY
# define NIDL_YACC_YYS_ARRAY
 
#else   /* !defined(Sun 4.1 lex and yacc) */
 
# define NIDL_YYV_DEFINITION    YYSTYPE *yyv
# define NIDL_YYS_DEFINITION    YACC_INT *yys
#endif  /* !defined(Sun 4.1 lex and yacc) */
 
 
/*
 * Non-existent yacc variables : variables which are present in some
 * versions of yacc, but not in others.  This varies from machine to
 * machine.  Again, these could be optimized to only use the variable
 * required by the underlying platform.
 */
#if     defined (UMIPS_LEX_YACC) || defined(OSF_LEX_YACC) || defined(ULTRIX_LEX_YACC) || (defined(SUN_LEX_YACC) && !defined(SUN_41_LEX_YACC))
#ifdef notyet
int     yystate;
int     yytmp;
int     *yyps;
YYSTYPE *yypv;
YYSTYPE *yypvt;
NIDL_YYS_DEFINITION;           /* define the yys variable globally */
#endif
#endif
 
#if     defined(SUN_LEX_YACC) && defined(SUN_41_LEX_YACC)
YYSTYPE *yypvt;
#endif
 
#if     defined(APOLLO_LEX_YACC) || defined(HPUX_LEX_YACC) || defined(SVR4_LEX_YACC)
YYSTYPE *yypvt;               /* Does not exist on Apollo */
#endif
 
/*
 * External variables defined by lex that must be saved and restored.
 *
 * Note that we should really define a save and restore routine in a
 * file similar to this one for the lex side of this code.  The current
 * method requires the lex variables to be exported, which may not be
 * the case, but seems safe to be on all current platforms.
 */
 
/* type definitions */
struct yywork {
        YYTYPE verify, advance;
};
struct yysvf {
        struct yywork *yystoff;
        struct yysvf *yyother;
        int *yystops;
};
 
/*
 * Variables for Ultrix 3.1; SunOS 4.0 and 4.1
 */
extern  int           yyleng;
extern  int           yymorfg;
extern  char          *yysptr;
extern  int           yytchar;
extern  FILE           *yyin;
extern  struct yysvf   *yyestate;
extern  struct yysvf   *yybgin;
extern  int           yylineno;
extern  char          yytext[];
extern  struct yysvf   *yylstate;
extern  struct yysvf   **yylsp;
extern  struct yysvf   **yyolsp;
extern  char          yysbuf[];
extern  char          *yysptr;
extern  int           *yyfnd;
extern  int           yyprevious;
 
/*
** State buffer used to save and restore the lex/yacc state.  This
** file handles the allocation and de-allocation of these structures,
** and only exports an opaque pointer to identify a saved structure.
*/
typedef struct {
    /* yacc variables */
    YYSTYPE             yylval;
    YYSTYPE             yyval;
    NIDL_YYV_DEFINITION;
    NIDL_YYS_DEFINITION;
    YYSTYPE             *yypv;
    int                 *yyps;
    YACC_INT            yystate;
    int                 yytmp;
    int                 yynerrs;
    YACC_INT            yyerrflag;
    int                 yychar;
    YYSTYPE             *yypvt;
 
    /* lex variables */
    int                 yyleng;
    int                 yymorfg;
    int                 yytchar;
    FILE                *yyin;
    struct yysvf        *yyestate;
    struct yysvf        *yybgin;
    int                 yylineno;
    char                yytext[ YYLMAX ];
    struct yysvf        *yylstate;
    struct yysvf        **yylsp;
    struct yysvf        **yyolsp;
    char                yysbuf[ YYLMAX ];
    char                *yysptr;
    int                 *yyfnd;
    int                 yyprevious;
 
}  lex_yacc_state_buffer_t;
 
 
/*
** NIDL_save_lex_yacc_state() -- save the current state so that a
** new parse can be started.  The malloc() of the save buffer could
** be optimized to take from a pool of buffers allocated in this file.
*/
lex_yacc_state_t NIDL_save_lex_yacc_state()
{
    /* Helper variables for saving and restoring the buffers and stacks. */
    YYSTYPE     * yyv_ptr;
    YYSTYPE     * yyv_end;
    YACC_INT    * yys_ptr;
    YACC_INT    * yys_end;
    YYSTYPE     * saved_yyv_ptr;
    YACC_INT    * saved_yys_ptr;
 
    /* Buffer to save current state in */
    lex_yacc_state_buffer_t *save_state;
 
    save_state = (lex_yacc_state_buffer_t *)
        malloc(sizeof(lex_yacc_state_buffer_t));
 
    /* Start State Save Code -- yacc variables first*/
    save_state->yylval    = yylval;
    save_state->yyval     = yyval;
    /* yyv and yys are saved below */
    save_state->yypv      = yypv;
    save_state->yyps      = yyps;
    save_state->yystate   = yystate;
    save_state->yytmp     = yytmp;
    save_state->yynerrs   = yynerrs;
    save_state->yyerrflag = yyerrflag;
    save_state->yychar    = yychar;
    save_state->yypvt      = yypvt;
 
    /* State Save Code -- lex variables */
    save_state->yyleng    = yyleng;
    save_state->yymorfg   = yymorfg;
    /* yysbuf is saved below */
    save_state->yytchar   = yytchar;
    save_state->yyin      = yyin;
    save_state->yyestate  = yyestate;
    save_state->yybgin    = yybgin;
    save_state->yylineno  = yylineno;
    /* yytext is saved below */
    save_state->yylstate  = yylstate;
    save_state->yylsp     = yylsp;
    save_state->yyolsp    = yyolsp;
    save_state->yysptr    = yysptr;
    save_state->yyfnd     = yyfnd;
    save_state->yyprevious= yyprevious;
 
    /* Now save various stacks and text buffers. */
#if     defined(NIDL_YACC_YYV_ARRAY)
#if     defined(SVR4_LEX_YACC)
    save_state->yyv = yyv;
    save_state->yys = yys;

    yyv_end = &yy_yyv[YYMAXDEPTH-1];       /* Point at last array element */
    for (yyv_ptr = yy_yyv, saved_yyv_ptr = save_state->yy_yyv;
#else
    yyv_end = &yyv[YYMAXDEPTH-1];       /* Point at last array element */
    for (yyv_ptr = yyv, saved_yyv_ptr = save_state->yyv;
#endif /* SVR4 */
         yyv_ptr <= yyv_end;
         yyv_ptr++, saved_yyv_ptr++)
        *saved_yyv_ptr = *yyv_ptr;
#else   /* NIDL_YACC_YYV_ARRAY */
    save_state->yyv = yyv;
#endif  /* NIDL_YACC_YYV_ARRAY */
 
#if     defined(NIDL_YACC_YYS_ARRAY)
#if     defined(SVR4_LEX_YACC)
    yys_end = &yy_yys[YYMAXDEPTH-1];       /* Point at last array element */
    for (yys_ptr = yy_yys, saved_yys_ptr = save_state->yy_yys;
#else
    yys_end = &yys[YYMAXDEPTH-1];       /* Point at last array element */
    for (yys_ptr = yys, saved_yys_ptr = save_state->yys;
#endif /* SVR4 */
         yys_ptr <= yys_end;
         yys_ptr++, saved_yys_ptr++)
        *saved_yys_ptr = *yys_ptr;
#else   /* NIDL_YACC_YYS_ARRAY */
    save_state->yys = yys;
#endif  /* NIDL_YACC_YYS_ARRAY */
 
    strcpy(save_state->yysbuf, yysbuf);
    strcpy(save_state->yytext, yytext);
 
    return ((lex_yacc_state_t)save_state);
}
 
/*
** NIDL_restore_lex_yacc_state() -- restore the given state so that
** a parse can continue.
**
** Note that state_ptr is an opaque pointer that points to a
** lex_yacc_state_buffer_t structure.  This routine free()'s the
** structure referenced by the given pointer.
*/
void NIDL_restore_lex_yacc_state(state_ptr)
    lex_yacc_state_t state_ptr;
{
    /* Helper variables for saving and restoring the buffers and stacks. */
    YYSTYPE     * yyv_ptr;
    YYSTYPE     * yyv_end;
    YACC_INT    * yys_ptr;
    YACC_INT    * yys_end;
    YYSTYPE     * saved_yyv_ptr;
    YACC_INT    * saved_yys_ptr;
 
    lex_yacc_state_buffer_t *saved_state;
 
    saved_state = (lex_yacc_state_buffer_t *) state_ptr;
 
    /* Restore yacc variables */
    yylval      = saved_state->yylval;
    yyval       = saved_state->yyval;
    /* yyv and yys are restored below */
    yypv        = saved_state->yypv;
    yyps        = saved_state->yyps;
    yystate     = saved_state->yystate;
    yytmp       = saved_state->yytmp;
    yynerrs     = saved_state->yynerrs + yynerrs;
    yyerrflag   = saved_state->yyerrflag;
    yychar      = saved_state->yychar;
    yypvt        = saved_state->yypvt;
 
    /* Restore lex variables */
    yyleng      = saved_state->yyleng;
    yymorfg     = saved_state->yymorfg;
    /* yysbuf is restored below */
    yytchar     = saved_state->yytchar;
    yyin        = saved_state->yyin;
    yyestate    = saved_state->yyestate;
    yybgin      = saved_state->yybgin;
    yylineno    = saved_state->yylineno;
    /* yytext is restored below */
    yylstate    = saved_state->yylstate;
    yylsp       = saved_state->yylsp;
    yyolsp      = saved_state->yyolsp;
    yysptr      = saved_state->yysptr;
    yyfnd       = saved_state->yyfnd;
    yyprevious  = saved_state->yyprevious;
 
    /* restore the various stacks and text buffers. */
#if     defined(NIDL_YACC_YYV_ARRAY)
#if     defined(SVR4_LEX_YACC)
    yys = saved_state->yys;
    yyv = saved_state->yyv;

    yyv_end = &yy_yyv[YYMAXDEPTH-1];       /* Point at last array element */
    for (yyv_ptr = yy_yyv, saved_yyv_ptr = saved_state->yy_yyv;
#else
    yyv_end = &yyv[YYMAXDEPTH-1];       /* Point at last array element */
    for (yyv_ptr = yyv, saved_yyv_ptr = saved_state->yyv;
#endif  /* SVR4 */
         yyv_ptr <= yyv_end;
         yyv_ptr++, saved_yyv_ptr++)
        *yyv_ptr = *saved_yyv_ptr;
#else   /* NIDL_YACC_YYV_ARRAY */
    yyv = saved_state->yyv;
#endif  /* NIDL_YACC_YYV_ARRAY */
 
#if     defined(NIDL_YACC_YYS_ARRAY)
#if     defined(SVR4_LEX_YACC)
    yys_end = &yy_yys[YYMAXDEPTH-1];       /* Point at last array element */
    for (yys_ptr = yy_yys, saved_yys_ptr = saved_state->yy_yys;
#else
    yys_end = &yys[YYMAXDEPTH-1];       /* Point at last array element */
    for (yys_ptr = yys, saved_yys_ptr = saved_state->yys;
#endif  /* SVR4 */
         yys_ptr <= yys_end;
         yys_ptr++, saved_yys_ptr++)
        *yys_ptr = *saved_yys_ptr;
#else   /* NIDL_YACC_YYS_ARRAY */
    yys = saved_state->yys;
#endif  /* NIDL_YACC_YYS_ARRAY */
 
    strcpy(yysbuf, saved_state->yysbuf);
    strcpy(yytext, saved_state->yytext);
 
    free((char *)saved_state);
 
    return;
}
