/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: bounds.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:38:14  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:58  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:43:57  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:58  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:43  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      bounds.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Bounds calculation routines
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <dutils.h>
#include <bounds.h>
#include <backend.h>


/*
 * align
 *
 * A utility routine to figure the required alignment padding, given the
 * current alignment.
 */
static long align
#ifdef PROTO
(
    long required,
    long * current
)
#else
(required, current)
    long required;
    long * current;
#endif
{
    long additional = 0;

    if (*current < required)
    {
        additional = required - *current;
    }
    *current = required;
    return additional;
}


/*
 * BE_pkt_size
 *
 * Returns a generous upper bound on the number of packet bytes required
 * to marshall this flattened parameter
 */
int BE_pkt_size
#ifdef PROTO
(
    AST_parameter_n_t *param,
    long * current_alignment
)
#else
(param, current_alignment)
    AST_parameter_n_t *param;
    long * current_alignment;
#endif
{
    int sum, this_arm, max_pkt;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    long temp_alignment;
    long worst_alignment;

    /*
     * Magic open record header parameter
     */
    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
    {
        return AST_SMALL_SET(BE_Open_Array(param)) ?
            align (2, current_alignment) + 2 :
            align (4, current_alignment) +
            4*BE_Open_Array(param)->type->type_structure.array->index_count;
    }

    /*
     * Magic array header parameter
     */
    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
    {
        sum = 0;

        if (!AST_SMALL_SET(param))
        {
            /*
             * 2.0 varying/conformant arrays: alignment + 1 long per dim. for
             * conformance; alignment + 2 longs per dim. for variance
             */
            if (AST_CONFORMANT_SET(param->type))
                sum += align (4, current_alignment) +
                       4*param->type->type_structure.array->index_count;
            if (AST_VARYING_SET(param))
            {
                if (!AST_CONFORMANT_SET(param->type))
                    sum += align (4, current_alignment);
                sum += 8*param->type->type_structure.array->index_count;
            }
        }
        else
        {
            /*
             * 1.5.1 varying/conformant arrays: alignment + 1 or 2 shorts
             */
            if (AST_VARYING_SET(param) && AST_CONFORMANT_SET(param->type))
                sum += align (2, current_alignment) + 2*2;
            else if (AST_VARYING_SET(param) || AST_CONFORMANT_SET(param->type))
                sum += align (2, current_alignment) + 2;
        }
        return sum;
    }

    /*
     * If param is just an alignment representation artifact
     * return max size gap it can induce in byte stream.
     */
    else if (param->type->kind == AST_null_k)
        return align (param->type->alignment_size, current_alignment);

    else if (param->type->kind == AST_array_k)
    {
        /*
         * Alignment to the elements and synchronization afterwards
         */
        long temp;
        long temp2;

        temp = BE_required_alignment(param);
        /*
        ** Synchronization is not the same as alignment.  Synchronization
        ** places the packet pointer at the same alignment as another
        ** marshalling pointer used by an out of line or sp routine.
        ** However,  after synchronization, the pmp may be badly aligned.
        ** Assume that we are completely unaligned.
        */

        *current_alignment = 1;
        temp2 = 1;
        return align (temp, &temp2) + 7;
    }

    else if (param->type->kind == AST_disc_union_k)
    {
        /*
         * Discriminated unions: find the maximum of the packet sizes of
         * each of the arms
         */
        max_pkt = 0;
        worst_alignment = 8;
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
        {
            temp_alignment = *current_alignment;
            this_arm = 0;
            for (pp = arm->flat; pp; pp = pp->next)
                this_arm += BE_pkt_size(pp, &temp_alignment);
            if (this_arm > max_pkt) max_pkt = this_arm;
            if (temp_alignment < worst_alignment)
                worst_alignment = temp_alignment;
            debug(("arm packet bytes required: %d\n", this_arm));
        }
        debug(("%s max pkt bytes required: %d\n", BE_get_name(param->name),
               max_pkt));
        *current_alignment = worst_alignment;
        return max_pkt;
    }
    /*
     * Pointer referent: only synchronization goes into the stack packet
     * Have to assume worst case alignment at this point.
     */
    else if (param->type->kind == AST_pointer_k &&
             !(BE_PI_Flags(param) & BE_DEFER))
    {
        /*
        ** The assumed way to get the required seven bytes of alignment
        ** here would be:
        **
        **        *current_alignment = 1;
        **        return align (8, current_alignment);
        **
        ** However, this doesn't work, undercalculating the size of the
        ** worst case packet.  The problem is that we are coming back from
        ** using something other than the stack packet, and synchronizing
        ** the stack packet's marshalling pointer ("mp") with the "op".
        ** Assume that the stack's mp is currently 2 mod 8, and the op is
        ** 1 mod 8.  The act of synchronization will require 7 bytes.
        ** This is the amount we are accounting for here. However, the
        ** resulting alignment is 1 mod 8.  This must be reflected in the
        ** resulting value of the current alignment cell. Therefore, we
        ** can't use the align() function, because that bumps the current
        ** alignment to the required alignment.
        */

        *current_alignment = 1;
        return 7;
    }
    /*
     * All other parameters: alignment + ndr_size
     */
    else return align (param->type->alignment_size, current_alignment) +
                param->type->ndr_size;
}

/*
 * BE_wire_bound
 *
 * This routine is used to determine whether or not a group of parameters
 * will fit in the first unmarshalling element.  It returns a generous
 * upper bound on the total wire size which could be occupied by the
 * parameter.  This routine makes the assumption that it is never called
 * with a discriminated union  (its caller(s) must decompose the union,
 * and call it once per arm).
 */
int BE_wire_bound
#ifdef PROTO
(
    AST_parameter_n_t *param,
    long * current_alignment
)
#else
(param, current_alignment)
    AST_parameter_n_t *param;
    long * current_alignment;
#endif
{
    int sum, this_arm, max_pkt;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    long temp_alignment;
    long worst_alignment;


    /*
     * Conformant record header--return infinity
     */
    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
    {
        *current_alignment = 1;
        return 1+MIN_BUFF_SIZE;
    }
    else if (param->type->kind == AST_array_k &&
             !(BE_PI_Flags(param) & BE_ARRAY_HEADER))
    {
        /*
         * Conformant arrays: no finite bound.  Other arrays: packet size
         * + num_elts * elt_size
         */
        if (AST_CONFORMANT_SET(param->type)) return 1+MIN_BUFF_SIZE;
        else
        {
            debug(("Wire bound for %s is %d\n", BE_get_name(param->name),
                   BE_pkt_size(param, current_alignment)+BE_Array_Info(param)->num_elts*
                   BE_Array_Info(param)->ndr_elt_size));

            temp_alignment = *current_alignment;
            *current_alignment = BE_Array_Info(param)->ndr_elt_size;

            return
                BE_pkt_size(param, &temp_alignment) +
                    BE_Array_Info(param)->num_elts *
                        BE_Array_Info(param)->ndr_elt_size;
        }
    }


#if 0
/*
**  DEAD CODE      DEAD CODE      DEAD CODE      DEAD CODE      DEAD CODE
**
** This code can never be reached, since BE_wire_bound's only caller,
** set_check_buffer, has already decomposed the union, calling (indirectly)
** BE_wire_bound once per arm (regardless of the depth of nesting of unions)
*/
    else if (param->type->kind == AST_disc_union_k)
    {
        /*
         * Find the maximum of the wire bounds of each of the arms
         * and the worst case resultant alignment.
         */
        max_pkt = 0;
        worst_alignment = 8;

        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
        {
            this_arm = 0;
            temp_alignment = *current_alignment;

            for (pp = arm->flat; pp; pp = pp->next)
                this_arm += BE_wire_bound(pp, &temp_alignment);

            if (this_arm > max_pkt) max_pkt = this_arm;
            debug(("arm wire bound: %d\n", this_arm));

            if (temp_alignment < worst_alignment)
                worst_alignment = temp_alignment;
            debug(("arm resultant alignment: %d\n", temp_alignment));
        }
        debug(("%s max wire bound: %d\n", BE_get_name(param->name), max_pkt));
        debug(("Union resultant alignment %d\n", worst_alignment));

        *current_alignment = worst_alignment;
        return max_pkt;
    }
#endif



    else if (param->type->kind == AST_pointer_k &&
             !(BE_PI_Flags(param) & BE_DEFER))
    {
        /*
         * Pointer referents--return infinity
         */
        *current_alignment = 1;
        return 1+MIN_BUFF_SIZE;
    }
    else return BE_pkt_size(param, current_alignment);
}

/*
 * ndr_size
 *
 * Returns an upper bound on the  number of bytes the decorated parameter
 * will occupy on the wire, assuming that it is already aligned.  If tile
 * is non-zero, returns the size necessary to tile the parameter in an array.
 */
static int ndr_size
#ifdef PROTO
(
    AST_parameter_n_t *param,
    boolean tile
)
#else
(param, tile)
    AST_parameter_n_t *param;
    boolean tile;
#endif
{
    BE_arm_t *arm;
    int size, this_arm, max_arm;

    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        return AST_SMALL_SET(BE_Open_Array(param)) ? 2 :
            4*BE_Open_Array(param)->type->type_structure.array->index_count;

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
    {
        size = 0;

        if (!AST_SMALL_SET(param))
        {
            /*
             * The conformant bits of an array which is a field of a conformant
             * structure will have been accounted for by the open record header
             * parameter, above
             */
            if (AST_CONFORMANT_SET(param->type) &&
                !(BE_PI_Flags(param) & BE_IN_ORECORD))
                size +=
                    4*param->type->type_structure.array->index_count;

            if (AST_VARYING_SET(param))
                size +=
                    8*param->type->type_structure.array->index_count;
        }
        else
        {
            if (AST_CONFORMANT_SET(param->type) &&
                !(BE_PI_Flags(param) & BE_IN_ORECORD)) size += 2;

            if (AST_VARYING_SET(param)) size += 2;
        }

        return size;
    }

    else if (param->type->kind == AST_array_k)
        return  BE_num_elts(param) *
                    BE_list_size(BE_Array_Info(param)->flat_elt, 1);
    else if (param->type->kind == AST_disc_union_k)
    {
        /*
         * Return the size of the largest arm
         */
        max_arm = 0;
        for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
        {
            if (arm->flat)
            {
                this_arm = BE_list_size(arm->flat, tile);
                if (this_arm > max_arm) max_arm = this_arm;
            }
        }
        return max_arm;
    }
    else return param->type->ndr_size;
}

/*
 * BE_list_size
 *
 * Takes a decorated parameter list (i.e. a flattened structure) and returns
 * an upper bound on the marshalling size of the parameter list, assuming the
 * first parameter of the list is aligned.  If tile is non-zero, returns the
 * size required to tile an array with the structure, computed as follows:
 *
 * If the array element is a structure with fields ABWDE, where the alignment
 * requirements for W are greater than or equal to the alignment requirements
 * for any of A, B, D, or E, then we can view the marshalling of the array as
 * the marshalling of an element AB,  followed by n-1 elements of a different
 * array with elements WDEAB,  followed by an element WDE.  Thus, for all but
 * the fragmentary first and last elements of the original array the ndr size
 * of each of the elements of the array will be exactly equal to the ndr size
 * of a properly aligned structure with fields WDEAB.
 */
int BE_list_size
#ifdef PROTO
(
    AST_parameter_n_t *list,
    boolean tile
)
#else
(list, tile)
    AST_parameter_n_t *list;
    boolean tile;
#endif
{
    int res, size = 0;
    AST_parameter_n_t *param;
    AST_parameter_n_t *first, *last, *prev;
    int alignment = RPC_MAX_ALIGNMENT, req_alignment;

    first = last = list;
    while (last && last->next) last = last->next;

    /*
     * If this list must tile an array, set first to the worst-case parameter
     * and set last to its predecessor
     */
    if (tile)
        for (param = list; param; prev = param, param = param->next)
            if (BE_required_alignment(param) > BE_required_alignment(first))
            {
                first = param;
                last = prev;
            }

    debug(("BE_list_size: first field is %s, last is %s\n",
            BE_get_name(first->name), BE_get_name(last->name)));

    param = first;
    while (param)
    {
        /*
         * Determine the alignment required by the parameter
         */
        req_alignment = BE_required_alignment(param);

        /*
         * If the current alignment is not an integer multiple of the
         * required alignment then add the difference between the current
         * alignment and the required aligment to the size
         */
        if ((alignment % RPC_MAX_ALIGNMENT) % req_alignment)
            size += req_alignment -
                        ((alignment % RPC_MAX_ALIGNMENT) % req_alignment);

        size += ndr_size(param, tile);

        if (res = BE_resulting_alignment(param)) alignment = res;

        if (param != last)
        {
            if (param->next) param = param->next;
            else param = list;
        }
        else param = NULL;
    }

    if (tile)
    {
        /*
         * Now add for alignment from the last field of the element back
         * to the first field
         */
        req_alignment = BE_required_alignment(first);

        if ((size % RPC_MAX_ALIGNMENT) % req_alignment)
            size += req_alignment - ((size % RPC_MAX_ALIGNMENT) % req_alignment);
    }

    debug(("list_size is %d\n", size));

    return size;
}

/*
 * ndr_size_exp_size
 *
 *  Returns the size in bytes of the character string size expression for
 *  number of bytes the decorated parameter will occupy on the wire, assuming
 *  that it is already aligned.
 */
static int ndr_size_exp_size
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int exp_size = 2; /* "(" plus one for NULL */
    int i;

    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        exp_size += 8;

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
        exp_size += 8;

    else if (param->type->kind == AST_array_k)
    {
        /*
         * Size expression is (number of elements)*(size of each element)
         */
        if (!AST_CONFORMANT_SET(param->type) && !AST_VARYING_SET(param))
            exp_size += 8;
        else if (AST_SMALL_SET(param))
            exp_size += strlen(BE_Array_Info(param)->count_exp);
        else
            for (i = 0; i < param->type->type_structure.array->index_count; i++)
            {
                if (i > 0) exp_size++;
                if (AST_VARYING_SET(param))
                    exp_size += strlen(BE_Array_Info(param)->B_exps[i]);
                else exp_size += strlen(BE_Array_Info(param)->Z_exps[i]);
            }
        /*
         * We must make fixed-size (i.e. worst-case) assumptions about
         * the sizes of the elements of the array, so we use BE_list_size()
         * here.
         */
        exp_size += 9;
    }
    else exp_size += 8;

    exp_size++;

    return exp_size;
}


/*
 * ndr_size_exp
 *
 * Returns a size expression for number of bytes the decorated parameter
 * will occupy on the wire, assuming that it is already aligned.  
 * 
 *  Note: The data returned is allocated via MALLOC and thus must be
 *      freed with FREE!  Thus the data returned by this function
 *      should not be used outside of this module otherwise it will
 *      cause a memory leak
 */
static char *ndr_size_exp
#ifdef PROTO
(
    AST_parameter_n_t *param
)
#else
(param)
    AST_parameter_n_t *param;
#endif
{
    int exp_size = ndr_size_exp_size(param);
    char *size_exp = MALLOC(exp_size);
    int i;

    sprintf(size_exp, "(");

    if (BE_PI_Flags(param) & BE_OPEN_RECORD)
        sprintf(size_exp+strlen(size_exp), "%d", ndr_size(param, false));

    else if (BE_PI_Flags(param) & BE_ARRAY_HEADER)
        sprintf(size_exp+strlen(size_exp), "%d", ndr_size(param, false));

    else if (param->type->kind == AST_array_k)
    {
        /*
         * Size expression is (number of elements)*(size of each element)
         */
        if (!AST_CONFORMANT_SET(param->type) && !AST_VARYING_SET(param))
            sprintf(size_exp+strlen(size_exp), "%d",
                        BE_Array_Info(param)->num_elts);
        else if (AST_SMALL_SET(param))
            strcat(size_exp, BE_Array_Info(param)->count_exp);
        else
            for (i = 0; i < param->type->type_structure.array->index_count; i++)
            {
                if (i > 0) strcat(size_exp, "*");
                if (AST_VARYING_SET(param))
                    strcat(size_exp, BE_Array_Info(param)->B_exps[i]);
                else strcat(size_exp, BE_Array_Info(param)->Z_exps[i]);
            }
        /*
         * We must make fixed-size (i.e. worst-case) assumptions about
         * the sizes of the elements of the array, so we use BE_list_size()
         * here.
         */
        sprintf(size_exp+strlen(size_exp), "*%d",
            BE_list_size(BE_Array_Info(param)->flat_base_elt, true));

    }
    else if (param->type->kind == AST_disc_union_k)
        sprintf(size_exp+strlen(size_exp), "%d", ndr_size(param, true));
    else sprintf(size_exp+strlen(size_exp), "%d", param->type->ndr_size);

    strcat(size_exp, ")");

    debug(("ndr size exp for %s is %s\n", BE_get_name(param->name),
           size_exp));

    ASSERTION((exp_size > strlen(size_exp)));
    return size_exp;
}

/*
 * BE_list_size_exp_size
 *
 *  Takes a decorated parameter list (i.e. a flattened structure ) and returns
 *  the size in bytes of string generated for the size expression
 */
static int BE_list_size_exp_size
#ifdef PROTO
(
    AST_parameter_n_t *list
)
#else
(list)
    AST_parameter_n_t *list;
#endif
{
    int exp_size = 2; /* 1 for trailing null, 1 for first "(" */
    AST_parameter_n_t *param;

    for (param = list; param; param = param->next)
    {
        if (param != list) exp_size++;
        exp_size += ndr_size_exp_size(param);
    }

    exp_size += 10;
    return exp_size;
}
/*
 * BE_list_size_exp
 *
 * Takes a decorated parameter list (i.e. a flattened structure ) and returns
 * size expression for the number of bytes the parameter list will occupy on
 * the wire, assuming that the first element of the list is aligned.
 */
char *BE_list_size_exp
#ifdef PROTO
(
    AST_parameter_n_t *list
)
#else
(list)
    AST_parameter_n_t *list;
#endif
{
    int exp_size = BE_list_size_exp_size(list);
    char *tmp, *size_exp = BE_ctx_malloc(exp_size);
    AST_parameter_n_t *param;
    int res, req_alignment, alignment = RPC_MAX_ALIGNMENT, pad = 0;

    sprintf(size_exp, "(");

    for (param = list; param; param = param->next)
    {
        if (param != list) strcat(size_exp, "+");

        req_alignment = BE_required_alignment(param);

        /*
         * If the current alignment is not an integer multiple of the
         * required alignment then advance the pad count by the difference
         * between its current alignment and the required aligment.
         */
        if ((alignment % RPC_MAX_ALIGNMENT) % req_alignment)
            pad += req_alignment -
                        ((alignment % RPC_MAX_ALIGNMENT) % req_alignment);

        strcat(size_exp, tmp = ndr_size_exp(param));

        if (res = BE_resulting_alignment(param)) alignment = res;

        FREE(tmp);
    }

    if (pad) sprintf(size_exp+strlen(size_exp), "+%d", pad);

    strcat(size_exp, ")");

    debug(("list size exp is %s\n", size_exp));

    ASSERTION((exp_size > strlen(size_exp)));
    return size_exp;
}
