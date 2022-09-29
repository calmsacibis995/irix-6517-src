/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: namtbpvt.h,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:42  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:02  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:49:45  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:19  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:02:58  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      namtbpvt.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  This header file contains the private definitions necessary for the
**  nametable modules.
**
**  VERSION: DCE 1.0
**
*/
/********************************************************************/
/*                                                                  */
/*   NAMTBPVT.H                                                     */
/*                                                                  */
/*              Data types private to the nametable routines.       */
/*                                                                  */
/********************************************************************/



/*
 *  We have a different (non-opaque) view of a NAMETABLE_id_t.
 */
typedef struct NAMETABLE_n_t * NAMETABLE_n_t_p;

typedef struct NAMETABLE_binding_n_t {
        int                              bindingLevel;
        char                            *theBinding;
        struct NAMETABLE_binding_n_t    *nextBindingThisLevel;
        struct NAMETABLE_binding_n_t    *oldBinding;
        NAMETABLE_n_t_p                  boundBy;
}
NAMETABLE_binding_n_t;


typedef struct NAMETABLE_n_t {
        struct NAMETABLE_n_t    *left;  /* Subtree with names less          */
        struct NAMETABLE_n_t    *right; /* Subtree with names greater       */
        struct NAMETABLE_n_t    *parent;/* Parent in the tree               */
                                        /* NULL if this is the root         */
        char                    *id;    /* The identifier string            */
        NAMETABLE_binding_n_t   *bindings;      /* The list of bindings known       */
                                                /* by this name at this time.       */
        NAMETABLE_binding_n_t   *tagBinding;    /* The structure known by this tag. */
}
NAMETABLE_n_t;

typedef struct NAMETABLE_temp_name_t {
        struct NAMETABLE_temp_name_t * next;  /* Next temp name chain block */
        NAMETABLE_n_t_p node;                 /* The temp name tree node    */
}
NAMETABLE_temp_name_t;

