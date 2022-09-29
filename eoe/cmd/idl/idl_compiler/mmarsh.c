/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: mmarsh.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.4  1993/03/03  23:07:35  weisman
 * 	Install Al Sanders' and Carl Burnette's fixes for OT 3783 and 7412
 * 	[1993/03/03  22:53:56  weisman]
 *
 * Revision 1.1.5.2  1993/03/02  18:18:20  sanders
 * 	In BE_spell_varying_marshall, don't rely on BE_ARR_OF_STR
 * 	flag to determine if array of strings.  The flag isn't set
 * 	if the parameter is out-of-line.
 * 
 * Revision 1.1.4.3  1993/01/03  21:40:21  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:27  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  18:48:54  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:03:41  zeliff]
 * 
 * Revision 1.1.2.2  1992/03/24  14:11:19  harrow
 * 	Add support to enable status code mapping.  This
 * 	is selectable per-platform via stubbase.h.
 * 	[1992/03/19  14:41:16  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:33  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990, 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      mmarsh.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Emission routines for parameter marshalling
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <mutils.h>
#include <dutils.h>
#include <cspeldcl.h>
#include <cstubgen.h>
#include <nodmarsh.h>
#include <nodesupp.h>
#include <oolrtns.h>
#include <marshall.h>
#include <mmarsh.h>
#include <cspell.h>

static void sync_mp
#ifdef PROTO
(
    FILE *fid
)
#else
( fid )
    FILE *fid;
#endif
{
    fprintf(fid, "rpc_synchronize_mp(pmp, op, 8);\n");
}

void BE_m_align_mp
#ifdef PROTO
(
    FILE *fid,
    char *mp,
    AST_type_n_t *type,
    BE_mflags_t flags
)
#else
(fid, mp, type, flags)
    FILE *fid;
    char *mp;
    AST_type_n_t *type;
    BE_mflags_t flags;
#endif
{
    if ((type)->alignment_size > 1)
      if (flags & BE_M_MAINTAIN_OP)
        fprintf(fid, "rpc_align_mop(%s, op, %d);\n", mp, (type)->alignment_size);
        else fprintf(fid, "rpc_align_mp(%s, %d);\n", mp, (type)->alignment_size);
}

static void marshall_scalar
#ifdef PROTO
(
    FILE *fid,
    char *mp,
    AST_type_n_t *type,
    char *name
)
#else
(fid, mp, type, name)
    FILE *fid;
    char *mp;
    AST_type_n_t *type;
    char *name;
#endif
{
    fprintf(fid, "rpc_marshall_%s(%s, %s);\n", BE_scalar_type_suffix(type),
            mp, name);

    /* If marshalling an error_status_t, output mapping support call */
    while (type->defined_as != NULL) type = type->defined_as;
    if (type->name == NAMETABLE_add_id("error_status_t"))
    {
        fprintf(fid, "#ifdef IDL_ENABLE_STATUS_MAPPING\n");
        fprintf(fid, "rpc_ss_map_local_to_dce_status((error_status_t*)%s);\n#endif\n",mp);
    }
}

static void advance_mp
#ifdef PROTO
(
    FILE *fid,
    char *mp,
    AST_type_n_t *type,
    BE_mflags_t flags
)
#else
(fid, mp, type, flags)
    FILE *fid;
    char *mp;
    AST_type_n_t *type;
    BE_mflags_t flags;
#endif
{
    if (flags & BE_M_MAINTAIN_OP)
        fprintf(fid, "rpc_advance_mop(%s, op, %d);\n", mp, (type)->ndr_size);
    else fprintf(fid, "rpc_advance_mp(%s, %d);\n", mp, (type)->ndr_size);
}

static void new_slot
#ifdef PROTO
(
    FILE *fid,
    int slot_num,
    BE_mflags_t flags
)
#else
( fid, slot_num, flags )
    FILE *fid;
    int slot_num;
    BE_mflags_t flags;
#endif
{
    fprintf(fid, "iovec.elt[%d].data_addr = (byte_p_t)pmp;\n", slot_num);
    fprintf(fid, "iovec.elt[%d].flags = %s;\n", slot_num,
         flags & (BE_M_CALLEE | BE_M_PIPE) ? "rpc_c_iovector_elt_reused" : "0");
    fprintf(fid, "iovec.elt[%d].buff_dealloc = NULL_FREE_RTN;\n", slot_num);
}

void BE_m_start_buff
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags
)
#else
( fid, flags )
    FILE *fid;
    BE_mflags_t flags;
#endif
{
    if (!(flags & BE_M_BUFF_EXISTS))
    {
        fprintf(fid, "mp = (rpc_mp_t)op;\n");
        fprintf(fid, "space_in_buffer = 0;\n");
        fprintf(fid, "NIDL_msp->p_iovec->elt[0].buff_addr = NULL;\n");
        fprintf(fid, "NIDL_msp->space_in_buff = 0;\n");
        fprintf(fid, "NIDL_msp->mp = mp;\n");
        fprintf(fid, "NIDL_msp->op = op;\n");
    }
}

void BE_m_end_buff
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags
)
#else
( fid, flags )
    FILE *fid;
    BE_mflags_t flags;
#endif
{
    if (!(flags & BE_M_BUFF_EXISTS))
    {
        fprintf(fid, "mp = NIDL_msp->mp;\n");
        fprintf(fid, "op = NIDL_msp->op;\n");
        fprintf(fid, "space_in_buffer = NIDL_msp->space_in_buff;\n");
        fprintf(fid, "if (NIDL_msp->p_iovec->elt[0].buff_addr != NULL)\n");
        fprintf(fid,
  "NIDL_msp->p_iovec->elt[0].data_len = NIDL_msp->p_iovec->elt[0].buff_len - ");
        fprintf(fid,
 "(NIDL_msp->p_iovec->elt[0].data_addr - NIDL_msp->p_iovec->elt[0].buff_addr)");
        fprintf(fid, " - space_in_buffer;\n");
        fprintf(fid, "else\n{\nNIDL_msp->p_iovec->elt[0].data_len = 0;\n");
        fprintf(fid,
               "NIDL_msp->p_iovec->elt[0].buff_dealloc = NULL_FREE_RTN;\n}\n");
    }
}

static char *MP = "mp";
static char *PMP = "pmp";

/*
 * BE_spell_start_marshall
 */
void BE_spell_start_marshall
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags    /*  BE_M_MAINTAIN_OP */
)
#else
(fid, flags)
    FILE *fid;
    BE_mflags_t flags;
#endif
{
    if (flags & BE_M_MAINTAIN_OP)
        fprintf(fid, "rpc_init_op(op);\n");
}

/*
 * BE_spell_start_block
 *
 * Emit the beginning of a marshalling block
 */
void BE_spell_start_block
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags,  /* BE_M_SP_BLOCK */
    int num_elt
)
#else
(fid, flags, num_elt)
    FILE *fid;
    BE_mflags_t flags;
    int num_elt;
#endif
{
    if (!(flags & BE_M_SP_BLOCK))
    {
        fprintf(fid, "/* Start of marshalling block */\n");
        fprintf(fid, "iovec.num_elt = %d;\n", num_elt);
        if (flags & BE_M_FIRST_BLOCK)
        {
            fprintf(fid, "rpc_align_mp(pmp, 8);\n");
        }
        else
        {
            sync_mp(fid);
        }
    }
}

/*
 * BE_spell_end_block
 *
 * Close and transmit a marshalling block
 */
void BE_spell_end_block
#ifdef PROTO
(
    FILE *fid,
    BE_mflags_t flags    /* BE_M_SP_BLOCK | BE_M_TRANSCEIVE */
)
#else
(fid, flags)
    FILE *fid;
    BE_mflags_t flags;
#endif
{
    if (flags & BE_M_SP_BLOCK)
    {
        if (flags & BE_M_TRANSCEIVE)
        {
            fprintf(fid,"elt = &outs;\n");
            fprintf(fid,
 "rpc_call_transceive((rpc_call_handle_t)call_h, (rpc_iovector_p_t)NIDL_msp->p_iovec, elt, &drep, (unsigned32*)&st);\n");
            CSPELL_test_transceive_status( fid );
        }
        else
        {
            fprintf(fid,
              "rpc_call_transmit((rpc_call_handle_t)call_h, (rpc_iovector_p_t)NIDL_msp->p_iovec, (unsigned32*)&st);\n");
            CSPELL_test_status( fid );
        }
    }
    else
    {
        if (flags & BE_M_TRANSCEIVE)
        {
            fprintf(fid,"elt = &outs;\n");
            fprintf(fid,
   "rpc_call_transceive((rpc_call_handle_t)call_h, (rpc_iovector_p_t)&iovec, elt, &drep, (unsigned32*)&st);\n");
            CSPELL_test_transceive_status( fid );
        }
        else
        {
            fprintf(fid,
                 "rpc_call_transmit((rpc_call_handle_t)call_h, (rpc_iovector_p_t)&iovec, (unsigned32*)&st);\n");
            CSPELL_test_status( fid );
        }
        fprintf(fid, "/* End of marshalling block */\n");
    }
}

/*
 * BE_spell_slot_length
 *
 * Fill in the length field for a slot with the difference between
 * the value of the packet marshalling pointer and the value of the
 * data_addr field of the slot.  Note that the slot lengths for array
 * parameters are filled in by the array marshalling routine.
 */
void BE_spell_slot_length
#ifdef PROTO
(
    FILE *fid,
    int slot_num
)
#else
(fid, slot_num)
    FILE *fid;
    int slot_num;
#endif
{
    if (slot_num < 0)
        INTERNAL_ERROR("Attempt to close negative iovec slot")
    else
        fprintf(fid,
            "iovec.elt[%d].data_len = (byte_p_t)pmp-iovec.elt[%d].data_addr;\n",
            slot_num, slot_num);
}

/*
 * BE_m_flip_routine_mode
 *
 * Toggle between in-line and buffer based marshalling
 *
 */
void BE_m_flip_routine_mode
#ifdef PROTO
(
    FILE *fid,
    boolean old_routine_mode
)
#else
( fid, old_routine_mode )
    FILE *fid;
    boolean old_routine_mode;
#endif
{
            if (old_routine_mode)
            {
                fprintf( fid, "mp = NIDL_msp->mp;\n");
                fprintf( fid, "op = NIDL_msp->op;\n");
            }
            else
            {
                fprintf(
                    fid, "NIDL_msp->space_in_buff -= (op - NIDL_msp->op);\n"
                );
                fprintf(fid, "NIDL_msp->mp = mp;\n");
                fprintf(fid, "NIDL_msp->op = op;\n");
            }
}

/*
 * BE_spell_scalar_marshall
 */
void BE_spell_scalar_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_ALIGN_MP: align the marshalling pointer
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         * |BE_M_SLOTLESS
                         */
    int slot_num        /* iff BE_M_NEW_SLOT */
)
#else
(fid, param, flags, slot_num)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
#endif
{
    char *mp = (flags & BE_M_SLOTLESS) ? MP : PMP;

    if (flags & BE_M_NEW_SLOT)
    {
        if (flags & BE_M_SYNC_MP) sync_mp(fid);
        new_slot(fid, slot_num, flags);
    }
    if (flags & BE_M_ALIGN_MP)
        BE_m_align_mp(fid, mp, param->type, flags);
    marshall_scalar(fid, mp, param->type, BE_get_name(param->name));

    if (flags & BE_M_ADVANCE_MP)
        advance_mp(fid, mp, param->type, flags);
}

/*
 * BE_spell_null_marshall
 *
 * marshall a scalar that has no content just alignment requirements.
 */
void BE_spell_null_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_ALIGN_MP: align the marshalling pointer
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         * |BE_M_SLOTLESS
                         */
    int slot_num        /* iff BE_M_NEW_SLOT */
)
#else
(fid, param, flags, slot_num)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
#endif
{
    char *mp = (flags & BE_M_SLOTLESS) ? MP : PMP;

    if (flags & BE_M_NEW_SLOT)
    {
        if (flags & BE_M_SYNC_MP) sync_mp(fid);
        new_slot(fid, slot_num, flags);
    }
    if (flags & BE_M_ALIGN_MP)
        BE_m_align_mp(fid, mp, param->type, flags );
}

/*
 * spell_conformant_marshall
 *
 * Initialize size (and count) variables for an open array or open record and
 * marshall the conformant part of the header.
 */
static void spell_conformant_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags /*
                      *  BE_M_ALIGN_MP
                      * |BE_M_MAINTAIN_OP: maintain the offset pointer
                      * |BE_M_SLOTLESS
                      */
)
#else
(fid, param, flags)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
#endif
{
    char *mp = (flags & BE_M_SLOTLESS) ? MP : PMP;
    int i;

    if (flags & BE_M_ALIGN_MP)
        BE_m_align_mp(fid, mp,
                 AST_SMALL_SET(param) ? BE_ushort_int_p : BE_ulong_int_p,
                 flags);

    if (!AST_SMALL_SET(param))
    {
        for (i = 0; i < param->type->type_structure.array->index_count; i++)
        {
            if (!(BE_PI_Flags(param) & BE_PARRAY))
                fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(param)->Z[i]),
                    BE_Array_Info(param)->Z_exps[i]
                );
            /* Dimensions of the base type are not marshalled for a pipe */
            if ( (i == 0) || (!(flags & BE_M_PIPE)) )
            {
                marshall_scalar(fid, mp, BE_ulong_int_p,
                            BE_get_name(BE_Array_Info(param)->Z[i]));
                advance_mp(fid, mp, BE_ulong_int_p, flags);
            }
        }
        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->size_var),
                BE_Array_Info(param)->size_exp);
    }
    else
    {
        fprintf(fid, "if (!(%s <= 0xFFFF)) RAISE(rpc_x_invalid_bound);\n",
                BE_Array_Info(param)->size_exp);
        fprintf(fid, "%s = %s;\n",
            BE_get_name(BE_Array_Info(param)->size_var),
            BE_Array_Info(param)->size_exp);
        marshall_scalar(fid, mp, BE_ushort_int_p,
                        BE_get_name(BE_Array_Info(param)->size_var));
        advance_mp(fid, mp, BE_ushort_int_p, flags);
    }

    /*
     * The count for a conformant, non-varying array must be stuffed in
     * a loop controlling count variable here rather than later
     * in BE_spell_varying_marshall
     */
    if (!AST_VARYING_SET(param))
        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_Array_Info(param)->count_exp);

}

/*
 * BE_spell_varying_marshall
 *
 * Initialize the count variable for an open array or open record and
 * marshall the varying part of the header.
 */
void BE_spell_varying_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags /*
                      *  BE_M_ALIGN_HEAD: align before the header
                      * |BE_M_MAINTAIN_OP: maintain the offset pointer
                      * |BE_M_SLOTLESS
                      */
)
#else
(fid, param, flags)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
#endif
{
    char *mp = (flags & BE_M_SLOTLESS) ? MP : PMP;
    int i, index_count;
    AST_type_n_t *elt_type;

    /*
     * if this is the header for an object to be marshalled by a
     * call to an out_of_line routine defer marshalling the varying
     * info to the body of that routine
     */
    if (BE_PI_Flags(param) & BE_OOL_HEADER)
        return;

    /*
     * If we are generating a marshalling routine for a pointed-at or
     * out_of_line array, the marshalling of the A,B values needs to be
     * conditional since the routine can also be called to marshall a
     * non-varying instance of the array type.
     */
    if ((flags & BE_M_NODE) && !(BE_PI_Flags(param) & BE_FIELD))
        fprintf(fid, "if (NIDL_varying)\n{\n");

    /*
     * If this parameter was also conformant then we've already aligned
     * the header
     */
    if ((flags & BE_M_ALIGN_MP)
    && (!AST_CONFORMANT_SET(param->type) || (BE_PI_Flags(param)& BE_IN_ORECORD)))
        BE_m_align_mp(fid, mp,
                 AST_SMALL_SET(param) ? BE_ushort_int_p : BE_ulong_int_p,
                 flags);

    if (!AST_SMALL_SET(param))
    {
        index_count = param->type->type_structure.array->index_count;
	/*
	 * If parameter represents an array of strings, decrement the index
	 * count, so we don't wind up with an extra set of loop variables.
	 * We can't rely on the BE_ARR_OF_STR flag bit here, since it may
	 * not have been set if this is an out of line element
	 */
	for (elt_type = param->type->type_structure.array->element_type;
		elt_type->kind == AST_array_k;
		elt_type = elt_type->type_structure.array->element_type) {
		    if (AST_STRING_SET(elt_type)) {
			index_count--;
		    }
	}

        for (i = 0; i < index_count; i++)
        {
            if (!(BE_PI_Flags(param) & BE_PARRAY))
                fprintf(fid, "%s = %s;\n%s = %s;\n",
                        BE_get_name(BE_Array_Info(param)->A[i]),
                        BE_Array_Info(param)->A_exps[i],
                        BE_get_name(BE_Array_Info(param)->B[i]),
                        BE_Array_Info(param)->B_exps[i]);
            marshall_scalar(fid, mp, BE_ulong_int_p,
                            BE_get_name(BE_Array_Info(param)->A[i]));
            advance_mp(fid, mp, BE_ulong_int_p, flags);
            marshall_scalar(fid, mp, BE_ulong_int_p,
                            BE_get_name(BE_Array_Info(param)->B[i]));
            advance_mp(fid, mp, BE_ulong_int_p, flags);
        }
        fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_Array_Info(param)->count_exp);
    }
    else
    {
        if (!(BE_PI_Flags(param) & BE_PARRAY))
            fprintf(fid, "%s = %s;\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                BE_Array_Info(param)->count_exp);
        marshall_scalar(fid, mp, BE_ushort_int_p,
                        BE_get_name(BE_Array_Info(param)->count_var));
        advance_mp(fid, mp, BE_ushort_int_p, flags);
    }

    /*
     * Raise an exception if the number of elements to be marshalled is
     * greater than the amount of room in the array
     */
    if (!(BE_PI_Flags(param) & BE_STRING0))
        fprintf(fid, "if (%s > %s) RAISE(rpc_x_invalid_bound);\n",
                BE_get_name(BE_Array_Info(param)->count_var),
                AST_CONFORMANT_SET(param->type) ?
                    BE_get_name(BE_Array_Info(param)->size_var) :
                    BE_Array_Info(param)->size_exp);

    /*
     * If we're emitting an ool marshalling routine for a v1_stringifiable
     * array, emit conditional code to add one to the count variable.
     */
    if ((flags & BE_M_NODE) && (flags & BE_M_OOL_RTN)
        &&  !(BE_PI_Flags(param) & BE_FIELD))
    {
        boolean has_pointers;
        boolean v1_stringifiable;

        has_pointers = BE_any_pointers(param);
        v1_stringifiable = AST_SMALL_SET(param->type)
                            && !AST_STRING0_SET(param->type)
                            && !has_pointers;
        if (v1_stringifiable)
            fprintf(fid, "if (v1_string_flag) %s++;\n",
                          BE_get_name(BE_Array_Info(param)->count_var));
    }

    if ((flags & BE_M_NODE) && (flags & BE_M_OOL_RTN)
        && !(BE_PI_Flags(param) & BE_FIELD))
    {
        /* Output else clause for non-varying case. */
        if (!AST_SMALL_SET(param))
        {
            fprintf(fid, "}\nelse\n{\n");
            fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->count_exp);
        }
        else
        {
            if (!(BE_PI_Flags(param) & BE_PARRAY))
            {
                fprintf(fid, "}\nelse\n{\n");
                fprintf(fid, "%s = %s;\n",
                    BE_get_name(BE_Array_Info(param)->count_var),
                    BE_Array_Info(param)->size_exp);
            }
        }
    }

    if ((flags & BE_M_NODE) && !(BE_PI_Flags(param) & BE_FIELD))
        fprintf(fid, "}\n");
}

/*
 * spell_array_loop_head
 */
static void spell_array_loop_head
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param
)
#else
(fid, param)
    FILE *fid;
    AST_parameter_n_t *param;
#endif
{
    AST_type_n_t *elt_type = param->type->type_structure.array->element_type;

    fprintf(fid, "for (%s = ",
                BE_get_name(BE_Array_Info(param)->index_var));

    if (AST_VARYING_SET(param) || AST_CONFORMANT_SET(param->type))
        fprintf(fid, "%s",
                BE_get_name(BE_Array_Info(param)->count_var));
    else
        fprintf(fid, "%s",
                BE_Array_Info(param)->count_exp);

    /* if element is an ool array then the loop count need to be adjusted
       by dividing it by the the count exp for the element */
    if (elt_type->kind == AST_array_k
    && AST_OUT_OF_LINE_SET(elt_type)
    && !AST_STRING_SET(elt_type)
    && !AST_STRING0_SET(elt_type))
        fprintf(fid, "/(%s)",
                BE_Array_Info(BE_Array_Info(param)->flat_elt)->count_exp);

    fprintf(fid, "; %s; %s--)\n{\n",
                BE_get_name(BE_Array_Info(param)->index_var),
                BE_get_name(BE_Array_Info(param)->index_var));
}


/*
 * spell_array_pointees_marshall
 *
 * Emit code to marshall the pointees of an array
 */
static void spell_array_pointees_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_ALIGN_MP: align before the data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         * |BE_M_SLOTLESS|BE_M_CALLER|BE_M_CALLEE
                         */
    boolean *p_routine_mode,
    int total_slots
)
#else
(fid, param, flags, p_routine_mode, total_slots)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    boolean *p_routine_mode;
    int total_slots;
#endif
{
    AST_parameter_n_t *elt;
    BE_mflags_t elt_flags;
    int dummy_slots_used;

    BE_m_start_buff(fid, flags);

    fprintf(fid, "%s = %s;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var),
            BE_Array_Info(param)->first_element_exp);

    spell_array_loop_head (fid, param);

    elt_flags = BE_M_ADVANCE_MP | BE_M_SLOTLESS | BE_M_BUFF_EXISTS |
        (flags & (BE_M_MAINTAIN_OP | BE_M_DEFER | BE_M_NODE |
                  BE_M_CALLER | BE_M_CALLEE ));
    /*
     * marshall pointee array elements
     */
    for (elt = BE_Array_Info(param)->flat_elt; elt; elt = elt->next)
        if (!(BE_PI_Flags(elt) & BE_SKIP))
            BE_marshall_param(fid, elt, elt_flags, 0, &dummy_slots_used,
                              p_routine_mode, total_slots);

    fprintf(fid, "%s++;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var));
    fprintf(fid, "}\n");

    BE_m_end_buff(fid, flags);
}

/*
 * BE_spell_array_hdr_marshall
 *
 * Align to and marshall conformance or variance bytes, if any
 */
void BE_spell_array_hdr_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_ALIGN_MP: align before the data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         * |BE_M_SLOTLESS|BE_M_CALLER|BE_M_CALLEE
                         */
    int slot_num        /* iff BE_M_NEW_SLOT */
)
#else
(fid, param, flags, slot_num)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
#endif
{
    if (flags & BE_M_NEW_SLOT)
    {
        if (flags & BE_M_SYNC_MP) sync_mp(fid);
        new_slot(fid, slot_num, flags);
    }

    /*
     * Marshall the conformance bytes for arrays not contained in open records
     */
    if (AST_CONFORMANT_SET(param->type) && !(BE_PI_Flags(param)& BE_IN_ORECORD))
        spell_conformant_marshall(fid, param, flags);

    if (AST_VARYING_SET(param))
        BE_spell_varying_marshall(fid, param, flags);
}

/*
 * BE_spell_array_marshall
 */
void BE_spell_array_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_ALIGN_MP: align before the data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         * |BE_M_DEFER: node number pass
                         * |BE_M_SLOTLESS|BE_M_CALLER|BE_M_CALLEE
                         */
    int slot_num,       /* iff BE_M_NEW_SLOT */
    boolean *p_routine_mode,
    int total_slots
)
#else
(fid, param, flags, slot_num, p_routine_mode, total_slots)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
    boolean *p_routine_mode;
    int total_slots;
#endif
{
    char *mp;
    AST_parameter_n_t *elt;
    BE_mflags_t elt_flags;
    boolean do_bug2;
    int dummy_slots_used;
    boolean ool_in_elt;
    boolean elt_routine_mode;

    mp = (flags & BE_M_SLOTLESS) ? MP : PMP;

    if (BE_PI_Flags(param) & BE_REF_PASS)
    {
        /*
         * Second pass over array to marshall pointees
         */
        spell_array_pointees_marshall(fid, param, flags, p_routine_mode,
                                      total_slots);
        return;
    }

    if (!(flags & BE_M_SLOTLESS) && (flags & BE_M_ALIGN_MP)
         && (BE_PI_Flags(param) & BE_HDRLESS))
    {
        /* There is a slot used only for array alignment */
        new_slot( fid, slot_num-1, flags );
    }

    if ((flags & BE_M_ALIGN_MP)
        && BE_Array_Info(param)->flat_elt->type->alignment_size > 1)
    {
        /*
         * Don't align to the elements of empty arrays unless -bug 2 is
         * set and we're marshalling an aligned array of scalars.
         */
        do_bug2 = (BE_bug_array_align2 && AST_SMALL_SET(param) &&
                   (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY) &&
                   (param->type->type_structure.array->element_type->kind !=
                       AST_structure_k));

        if ((AST_CONFORMANT_SET(param->type) || AST_VARYING_SET(param)) &&
            !do_bug2)
            fprintf(fid, "if (%s > 0)\n{\n",
                BE_get_name(BE_Array_Info(param)->count_var));

        BE_m_align_mp(fid, mp, BE_Array_Info(param)->flat_elt->type,
                 flags);

        if ((AST_CONFORMANT_SET(param->type) || AST_VARYING_SET(param)) &&
            !do_bug2)
            fprintf(fid, "}\n");
    }

    /*
     * Close the header slot, if necessary
     */
    if ( ! (flags & BE_M_SLOTLESS)
         && ! ((BE_PI_Flags(param) & BE_HDRLESS) && !(flags & BE_M_ALIGN_MP)) )
        BE_spell_slot_length(fid, slot_num-1);

    /*
     * If this is a string0, kick up the count value
     */
    if (BE_PI_Flags(param) & BE_STRING0)
        fprintf(fid, "%s++; /* marshall trailing NULL */\n",
                BE_get_name(BE_Array_Info(param)->count_var));

    /*
     * Make the "point-at-the-array" optimization here
     */
    if (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY)
    {
        if (BE_Array_Info(param)->flat_elt->type->kind == AST_character_k)
            fprintf(fid, "\n#ifdef PACKED_CHAR_ARRAYS\n");
        else if (BE_Array_Info(param)->flat_elt->type->kind == AST_byte_k)
            fprintf(fid, "\n#ifdef PACKED_BYTE_ARRAYS\n");
        else fprintf(fid, "\n#ifdef PACKED_SCALAR_ARRAYS\n");
    }

    if (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY)
    {
        /*
         * Aligned scalar arrays and arrays of bytes: if we are at the top
         * level, then just point the iovector slot at the array.
         */
        if (!(flags & BE_M_SLOTLESS))
        {
            fprintf(fid, "iovec.elt[%d].data_addr = (byte_p_t)%s;\n", slot_num,
                    BE_Array_Info(param)->first_element_exp);
            fprintf(fid, "iovec.elt[%d].flags = %s;\n", slot_num,
                       flags & (BE_M_CALLEE | BE_M_PIPE)
                                          ? "rpc_c_iovector_elt_reused" : "0");

            /*
             * For string0's, use the count variable rather than the count
             * expression
             */
            if (!(BE_PI_Flags(param) & BE_STRING0))
                fprintf(fid, "iovec.elt[%d].data_len = %s*%d;\n", slot_num,
                        BE_Array_Info(param)->count_exp,
                        BE_Array_Info(param)->ndr_elt_size);
            else fprintf(fid, "iovec.elt[%d].data_len = %s;\n", slot_num,
                        BE_get_name(BE_Array_Info(param)->count_var));

            fprintf(fid, "iovec.elt[%d].buff_dealloc = NULL_FREE_RTN;\n",
                    slot_num);
            fprintf(fid, "rpc_advance_op(op, iovec.elt[%d].data_len);\n",
                    slot_num);
        }
        /*
         * If we are not at the top level then block copy the array into
         * the marshalling buffer
         */
        else
        {
            if (!(BE_PI_Flags(param) & BE_STRING0))
            {
                fprintf(fid,
                    "memcpy((char *)%s, (char *)%s, %s*%d);\n",
                        mp,
                        BE_Array_Info(param)->first_element_exp,
                        BE_Array_Info(param)->count_exp,
                        BE_Array_Info(param)->ndr_elt_size);
                fprintf(fid, "rpc_advance_mop(%s, %s, %s*%d);\n", mp, "op",
                        BE_Array_Info(param)->count_exp,
                        BE_Array_Info(param)->ndr_elt_size);
            }
            else
            {
                fprintf(fid,
                    "memcpy((char *)%s, (char *)%s, %s);\n",
                        mp,
                        BE_Array_Info(param)->first_element_exp,
                        BE_get_name(BE_Array_Info(param)->count_var));
                fprintf(fid, "rpc_advance_mop(%s, %s, %s);\n", mp, "op",
                        BE_get_name(BE_Array_Info(param)->count_var));
            }
        }

    }

    if (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY)
        fprintf(fid, "#else\n");

    /*
     * The iterate-over-the-array-elements path:
     * If we are at the top level then set up a new slot for the array
     * data
     */
    if (!(flags & BE_M_SLOTLESS))
    {
        fprintf(fid, "mp = (rpc_mp_t)malloc(7+%s*%d);\n",
                BE_Array_Info(param)->count_exp,
                BE_Array_Info(param)->ndr_elt_size);
	fprintf(fid, "if(!mp)RAISE(rpc_x_no_memory);\n");
        fprintf(fid, "iovec.elt[%d].buff_addr = (byte_p_t)mp;\n", slot_num);
        fprintf(fid, "rpc_synchronize_mp(mp, op, 8);\n");
        fprintf(fid, "iovec.elt[%d].data_addr = (byte_p_t)mp;\n",
                slot_num, slot_num);

        fprintf(fid, "iovec.elt[%d].flags = 0;\n", slot_num);
        fprintf(fid, "iovec.elt[%d].buff_dealloc = (rpc_ss_dealloc_t)rpc_ss_call_free;\n", slot_num);
    }

    fprintf(fid, "%s = ",
            BE_get_name(BE_Array_Info(param)->element_ptr_var));
    fprintf(fid, "%s;\n",
            BE_Array_Info(param)->first_element_exp);

    ool_in_elt = BE_ool_in_flattened_list( BE_Array_Info(param)->flat_elt );

    if (ool_in_elt & !(flags & BE_M_NODE))
        fprintf(fid, "NIDL_msp->space_in_buff = 2147483647L;\n");

    spell_array_loop_head(fid, param);

    /*
     * set up new flags for elements
     */
    elt_flags = BE_M_ADVANCE_MP | BE_M_SLOTLESS | BE_M_BUFF_EXISTS |
                BE_M_ARRAY_BUFF |
                 (flags & (BE_M_MAINTAIN_OP | BE_M_DEFER | BE_M_PIPE |
                   BE_M_NODE | BE_M_CALLER | BE_M_CALLEE));

    /*
     * marshall an array element
     */
    elt_routine_mode = *p_routine_mode;
    for (elt = BE_Array_Info(param)->flat_elt; elt; elt = elt->next)
    {
        if ( !(BE_PI_Flags(elt) & BE_SKIP) )
            BE_marshall_param(fid, elt, elt_flags, 0, &dummy_slots_used,
                              &elt_routine_mode, total_slots);
    }
    if ( elt_routine_mode != *p_routine_mode )
    {
        BE_m_flip_routine_mode( fid, elt_routine_mode );
    }


    fprintf(fid, "%s++;\n",
            BE_get_name(BE_Array_Info(param)->element_ptr_var));
    fprintf(fid, "}\n");

    /*
     * If we are at the top level then close the array data slot
     */
    if (!(flags & BE_M_SLOTLESS)) fprintf(fid,
        "iovec.elt[%d].data_len = (byte_p_t)mp - iovec.elt[%d].data_addr;\n",
        slot_num, slot_num);

    if (BE_Array_Info(param)->flags & BE_SIMPLE_ARRAY)
        fprintf(fid, "#endif\n");
}

/*
 * BE_spell_open_record_marshall
 *
 * Marshall the header for an open record
 */
void BE_spell_open_record_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         */
    int slot_num        /* iff BE_M_NEW_SLOT */
)
#else
(fid, param, flags, slot_num)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
#endif
{
    if (flags & BE_M_NEW_SLOT)
    {
        if (flags & BE_M_SYNC_MP) sync_mp(fid);
        new_slot(fid, slot_num, flags);
    }

    spell_conformant_marshall(fid, BE_Open_Array(param), flags);
}

/*
 * spell_union_pointees_marshall
 *
 * Marshall the pointees of a discriminated union
 */
static void spell_union_pointees_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         */
    boolean *p_routine_mode,
    int total_slots
)
#else
(fid, param, flags, p_routine_mode, total_slots)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    boolean *p_routine_mode;
    int total_slots;
#endif
{
    AST_case_label_n_t *label;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    BE_mflags_t new_flags;
    int dummy_slots_used;

    BE_m_start_buff(fid, flags);

    fprintf(fid, "switch (%s)\n{\n",
            BE_get_name(BE_Du_Info(param)->discrim->name));

    for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
    {
         for (label = arm->labels; label; label = label->next)
            if (label->default_label)
            {
                fprintf(fid, "default:\n");
            }
            else
            {
                fprintf(fid, "case ");
                CSPELL_constant_val(fid, label->value);
                fprintf(fid, ":\n");
            }

        if (flags & BE_M_OOL_RTN)
        {
            BE_ool_spell_varying_info( fid, arm->referred_to_by->flat );
        }

        new_flags = BE_M_ADVANCE_MP | BE_M_SLOTLESS | BE_M_BUFF_EXISTS |
            (flags &
                (BE_M_MAINTAIN_OP | BE_M_NODE | BE_M_DEFER | BE_M_ARRAY_BUFF |
                 BE_M_CALLER | BE_M_CALLEE));

        for (pp = arm->flat; pp; pp = pp->next)
            BE_marshall_param(fid, pp, new_flags, 0, &dummy_slots_used,
                              p_routine_mode, total_slots);

        fprintf(fid, "break;\n");
    }

    fprintf(fid, "}\n");

    BE_m_end_buff(fid, flags);
}

/*
 * BE_null_iovec_elt
 *
 * Make an entry in an io_vector null
 */
static void BE_null_iovec_elt
#ifdef PROTO
(
    FILE *fid,
    int slot
)
#else
( fid, slot )
    FILE *fid;
    int slot;
#endif
{
            fprintf(fid, "iovec.elt[%d].data_addr = NULL;\n", slot);
            fprintf(fid, "iovec.elt[%d].data_len = 0;\n", slot);
            fprintf(fid, "iovec.elt[%d].flags = 0;\n", slot);
            fprintf(fid, "iovec.elt[%d].buff_dealloc = NULL_FREE_RTN;\n", slot);
}
/*
 * BE_spell_union_marshall
 *
 * Marshall the switch part of a discriminated union
 */
void BE_spell_union_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,   /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         */
    int slot_num,        /* iff BE_M_NEW_SLOT */
    boolean *p_routine_mode,     /* Pointer to TRUE if currently using
                                    routine based marshalling */
    int total_slots
)
#else
(fid, param, flags, slot_num, p_routine_mode, total_slots)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
    boolean *p_routine_mode;
    int total_slots;
#endif
{
    AST_case_label_n_t *label;
    BE_arm_t *arm;
    AST_parameter_n_t *pp;
    boolean spell_length, defaulted = 0;
    BE_mflags_t new_flags;
    int slot, slots_used_this_param, slots_used_so_far, slot_to_close;
    boolean arm_routine_mode;   /* TRUE if arm element marshalled by routine */
    boolean previous_param_ool;

    if (BE_PI_Flags(param) & BE_REF_PASS)
    {
        /*
         * Second pass over union to marshall pointees
         */
        spell_union_pointees_marshall(fid, param, flags, p_routine_mode,
                                      total_slots);
        return;
    }

    fprintf(fid, "switch (%s)\n{\n",
            BE_get_name(BE_Du_Info(param)->discrim->name));

    for (arm = BE_Du_Info(param)->arms; arm; arm = arm->next)
    {
        arm_routine_mode = *p_routine_mode;
        for (label = arm->labels; label; label = label->next)
            if (label->default_label)
            {
                fprintf(fid, "default:\n");
                defaulted = 1;
            }
            else
            {
                fprintf(fid, "case ");
                CSPELL_constant_val(fid, label->value);
                fprintf(fid, ":\n");
            }

        slots_used_so_far = 0;
        spell_length = false;
        previous_param_ool = false;
        for (pp = arm->flat; pp; pp = pp->next)
        {
            if (BE_PI_Flags(pp) & BE_SKIP)
                continue;

            new_flags = flags;
            new_flags &= (
                BE_M_MAINTAIN_OP
                | BE_M_SLOTLESS
                | BE_M_BUFF_EXISTS
                | BE_M_ARRAY_BUFF
                | BE_M_DEFER
                | BE_M_NODE
                | BE_M_CALLER
                | BE_M_CALLEE
            );
            new_flags |= BE_M_ADVANCE_MP;

            if (!(flags & BE_M_SLOTLESS))
            {
                if ( !(BE_PI_Flags(pp) & BE_OOL) && previous_param_ool )
                {
                    BE_spell_end_block( fid, BE_M_SP_BLOCK );
                    sync_mp( fid );
                }
                if ( (BE_PI_Flags(pp) & BE_OOL) && !previous_param_ool )
                {
                    slot_to_close = slot_num + slots_used_so_far - 1;
                    if (spell_length)
                    {
                        BE_spell_slot_length(fid, slot_to_close);
                        spell_length = false;
                    }
                    /* Prematurely shut down iovector */
                    fprintf( fid, "iovec.num_elt=%d;\n", slot_to_close + 1);
                    BE_spell_end_block( fid, 0 );
                    /* Null out the entries we have despatched */
                    for (slot = 0; slot <= slot_to_close; slot++)
                        BE_null_iovec_elt( fid, slot );
                    /* And reset the iovector to its original length */
                    fprintf( fid, "iovec.num_elt=%d;\n", total_slots );
                }
                else if (BE_PI_Flags(pp) & BE_NEW_SLOT)
                {
                    /*
                     * Close the previous slot if necessary
                     */
                    if (spell_length && !previous_param_ool)
                        BE_spell_slot_length(fid,
                                             slot_num + slots_used_so_far - 1);
                    new_flags |= BE_M_NEW_SLOT;
                }
            }

            BE_marshall_param(fid, pp, new_flags, slot_num + slots_used_so_far,
                              &slots_used_this_param, &arm_routine_mode,
                              total_slots);

            if (BE_PI_Flags(pp) & BE_OOL)
                previous_param_ool = true;
            else
            {
                previous_param_ool = false;
                slots_used_so_far += slots_used_this_param;

                if (BE_PI_Flags(pp) & BE_OOL_HEADER)
                    spell_length = true;
                else if (pp->type->kind == AST_array_k)
                    spell_length =
                        (BE_PI_Flags(pp) & BE_ARRAY_HEADER)
                        && BE_Is_Arr_of_Refs(pp);
                else if (pp->type->kind == AST_disc_union_k)
                    spell_length = false;
                else if (pp->type->kind == AST_pointer_k)
                    spell_length = !!(BE_PI_Flags(pp) & BE_DEFER);
                else
                    spell_length = true;
            }
        }

        if ( !(flags & BE_M_SLOTLESS) && previous_param_ool )
        {
            BE_spell_end_block( fid, BE_M_SP_BLOCK );
            sync_mp( fid );
        }
        if (!(flags & BE_M_SLOTLESS) && spell_length)
            BE_spell_slot_length(fid, slot_num + slots_used_so_far - 1);

        /*
         * NULL out the unused slots for this arm
         */
        while (!(flags & BE_M_SLOTLESS) &&
                    slots_used_so_far < BE_Du_Info(param)->vec_size)
        {
            slot = slot_num + slots_used_so_far++;
            BE_null_iovec_elt( fid, slot );
        }
        if ( arm_routine_mode != *p_routine_mode )
        {
            BE_m_flip_routine_mode( fid, arm_routine_mode );
        }

        fprintf(fid, "break;\n");
    }

    if (!defaulted) fprintf(fid, "default: RAISE(rpc_x_invalid_tag);\n");

    fprintf(fid, "}\n");
}

/*
 * spell_pointee_marshall
 *
 * Emit the various arcana necessary to marshall a parameter by routine
 */
static void spell_pointee_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags        /* BE_M_CALLER|BE_M_CALLEE|BE_M_NODE */
)
#else
(fid, param, flags)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
#endif
{
    int i, num_array_dimensions;
    char *node_type_flag;
    boolean varying, pseudo_varying, string;
    AST_type_n_t *pointee_type =
        param->type->type_structure.pointer->pointee_type;

    /*
     * Decide which flag to send to the unmarshalling routine
     */
    if (AST_REF_SET(param))
        node_type_flag = "rpc_ss_old_ref_node_k";
    else
        node_type_flag = "rpc_ss_mutable_node_k";

    string = (BE_PI_Flags(param) & (BE_PTR_ARRAY | BE_ARRAYIFIED | BE_PTR2STR)) &&
             (AST_STRING_SET(param) || AST_STRING_SET(pointee_type));

    varying = (BE_PI_Flags(param) & (BE_PTR_ARRAY | BE_ARRAYIFIED | BE_PTR2STR)) &&
              (AST_VARYING_SET(param) || string);

    /*
     * Even if an array is not varying, it might need to be marshalled via a
     * generic routine which expects parameters semantically equivalent to
     * the A,B values for a varying array.
     */
    pseudo_varying =
        (!varying && pointee_type->kind == AST_array_k &&
         (  ((flags & BE_M_CALLER) && AST_IN_VARYING_SET(pointee_type))
         || ((flags & BE_M_CALLEE) && AST_OUT_VARYING_SET(pointee_type))));

    /*
     * If we are dealing with a pointed-at string, make an assignment to
     * the Z variables (which might be in terms of strlen()) to prepare the
     * B expressions, which might be in terms of the Z variables.
     */
    if (string && AST_CONFORMANT_SET(pointee_type))
        for (i = 0; i < pointee_type->type_structure.array->index_count; i++)
            fprintf(fid, "%s = %s;\n",
                BE_get_name(
                    BE_Array_Info(BE_Ptr_Info(param)->flat_array)->Z[i]),
                BE_Array_Info(BE_Ptr_Info(param)->flat_array)->Z_exps[i]);

    CSPELL_pa_routine_name(fid, pointee_type,
        flags & BE_M_CALLER ? BE_caller : BE_callee,
        BE_marshalling_k, varying);

    fprintf(fid, "(");

    /*
     * Add a cast to the correct type.  enums are a special case
     * an must be cast to int* which is expected by the pa routine.
     */
    if (pointee_type->kind == AST_enum_k)
        fprintf(fid, "(int*)");
    else
        CSPELL_cast_exp(fid, param->type);
    fprintf(fid, "%s, %s, ", BE_get_name(param->name), node_type_flag);
    fprintf(fid, "NIDL_msp");

    if (pointee_type->kind == AST_array_k)
    {
        AST_array_index_n_t *index_p =
                pointee_type->type_structure.array->index_vec;

        if (varying)
            fprintf(fid, ", idl_true");
        else if (pseudo_varying)
            fprintf(fid, ", idl_false");

        num_array_dimensions = pointee_type->type_structure.array->index_count;
        for (i=0; i<num_array_dimensions; i++)
        {
            if ((BE_PI_Flags(param)
                 & (BE_PTR_ARRAY | BE_ARRAYIFIED | BE_PTR2STR))
            && AST_CONFORMANT_SET(pointee_type))
            {
                fprintf(fid, ", %s", string ?
                    BE_get_name(
                        BE_Array_Info(BE_Ptr_Info(param)->flat_array)->Z[i]) :
                    BE_Array_Info(BE_Ptr_Info(param)->flat_array)->Z_exps[i]);
            }
            if (varying)
            {
                fprintf(fid, ", %s, %s",
                    BE_Array_Info(BE_Ptr_Info(param)->flat_array)->A_exps[i],
                    BE_Array_Info(BE_Ptr_Info(param)->flat_array)->B_exps[i]);
            }
            else if (pseudo_varying)
            {
                if (AST_FIXED_LOWER_SET(index_p)
                    &&  index_p->lower_bound->value.int_val == 0)
                {
                    fprintf(fid, ", %ld", index_p->lower_bound->value.int_val);
                    if (AST_FIXED_UPPER_SET(index_p))
                        fprintf(fid, ", %ld",
                                index_p->upper_bound->value.int_val-
                                index_p->lower_bound->value.int_val+1);
                    else
                        fprintf(fid, ", %s", string ?
                            BE_get_name(
                                BE_Array_Info(BE_Ptr_Info(param)->flat_array)->Z[i]) :
                            BE_Array_Info(BE_Ptr_Info(param)->flat_array)->Z_exps[i]);
                }
                else
                    INTERNAL_ERROR("Array with nonzero lower bound not supported");
            }
            index_p++;
        }
    }
    fprintf(fid, ");\n");

#ifdef NO_EXCEPTIONS
    CSPELL_return_on_status(fid);
#endif
}

/*
 * BE_spell_pointer_marshall
 *
 * Marshall a pointer
 */
void BE_spell_pointer_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         */
    int slot_num        /* iff BE_M_NEW_SLOT */
)
#else
(fid, param, flags, slot_num)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
#endif
{
    BE_mflags_t new_flags;
    char *mp = (flags & BE_M_SLOTLESS) ? MP : PMP;
    static char *node_number = "IDL_node_number";

    if (flags & BE_M_DEFER)
    {
        /*
         * Marshall just the node number
         */
        if (flags & BE_M_NEW_SLOT)
        {
            if (flags & BE_M_SYNC_MP) sync_mp(fid);
            new_slot(fid, slot_num, flags);
        }

        if (flags & BE_M_ALIGN_MP)
            BE_m_align_mp(fid, mp, BE_ulong_int_p, flags);

        if (AST_MUTABLE_SET(param) && !AST_IGNORE_SET(param))
        {
            /*
             * Marshall node number for mutable nodes
             */
            fprintf(fid, "{\nidl_ulong_int %s;\n",node_number);
            if (flags & BE_M_NODE)
                CSPELL_get_node_number(fid, BE_get_name(param->name),
                    "*p_node_number", false,
                    (flags & BE_M_CALLER) ? BE_caller: BE_callee);
            else
                CSPELL_get_node_number(fid, BE_get_name(param->name),
                    node_number, true,
                    (flags & BE_M_CALLER) ? BE_caller: BE_callee);

            marshall_scalar(fid, mp, BE_ulong_int_p,
                            "IDL_node_number");
            fprintf(fid, "}\n");

        }

        if (flags & BE_M_ADVANCE_MP)
            advance_mp(fid, mp, BE_ulong_int_p, flags);
    }
    else
    {
        BE_m_start_buff(fid, flags);

        spell_pointee_marshall(fid, param, flags);

        BE_m_end_buff(fid, flags);
    }
}

/*
 * BE_spell_ool_marshall
 *
 */
void BE_spell_ool_marshall
#ifdef PROTO
(
    FILE *fid,
    AST_parameter_n_t *param,
    BE_mflags_t flags,  /*
                         *  BE_M_NEW_SLOT: start a new iodesc slot
                         * |BE_M_SYNC_MP: recalibrate to offset pointer
                         *     to compensate for preceding out-of-packet data
                         * |BE_M_MAINTAIN_OP: maintain the offset pointer
                         */
    int slot_num        /* iff BE_M_NEW_SLOT */
)
#else
(fid, param, flags, slot_num)
    FILE *fid;
    AST_parameter_n_t *param;
    BE_mflags_t flags;
    int slot_num;
#endif
{
    int i, num_array_dimensions;
    boolean varying, string, v1_string_on_param, v1_string_type;
    AST_parameter_n_t *open_array_param, *flat_array_param;
    AST_type_n_t *ool_type;
    boolean ool_type_has_pointers;

    ool_type = BE_Ool_Info(param)->type;
    ool_type_has_pointers = BE_Ool_Info(param)->any_pointers;

    if (ool_type->kind == AST_array_k)
    {
        string = ( AST_STRING_SET(param) &&
                                     !(BE_Ool_Info(param)->has_calls_before) )
                     || AST_STRING0_SET(param);
        if (BE_PI_Flags(param) & BE_PTR2STR) string = true;
        varying = AST_VARYING_SET(param) || string;
        v1_string_type = AST_STRING0_SET(ool_type);
        v1_string_on_param = AST_STRING0_SET(param) && !v1_string_type;
    }
    else
    {
        string = false;
        varying = false;
        v1_string_type = false;
        v1_string_on_param = false;
    }

    if (BE_PI_Flags(param) & BE_PTR2STR)
        flat_array_param =  BE_Ptr_Info(param)->flat_array;
    else
        flat_array_param = param;

    /*
     * If we are dealing with a string, make an assignment to
     * the Z variables (which might be in terms of strlen()) to prepare the
     * B expressions, which might be in terms of the Z variables.
     */
    if (string && AST_CONFORMANT_SET(ool_type))
        for (i = 0; i < flat_array_param->type->type_structure.array->
                        index_count; i++)
            fprintf(fid, "%s = %s;\n",
                BE_get_name(
                    BE_Array_Info(flat_array_param)->Z[i]),
                BE_Array_Info(flat_array_param)->Z_exps[i]);


    BE_m_start_buff(fid, flags);

    CSPELL_ool_routine_name(fid, BE_Ool_Info(param)->call_type,
        flags & BE_M_CALLER ? BE_caller : BE_callee,
        BE_marshalling_k, varying,
        BE_Ool_Info(param)->use_P_rtn);

    fprintf(fid, "(");

    CSPELL_cast_exp(fid, BE_pointer_type_node(BE_Ool_Info(param)->call_type));

    fprintf(fid, "%s%s",
        ( ool_type->kind == AST_array_k
        || ((flags & BE_M_CALLEE)
             && AST_CONFORMANT_SET(ool_type)
             && (!(flags & BE_M_NODE))
             && (BE_Ool_Info(param)->top_level))
        || BE_Ool_Info(param)->has_calls_before )
            ? "" : "&", BE_get_name(BE_Ool_Info(param)->call_name) );

    fprintf(fid, ", NIDL_msp");
    if (ool_type->kind == AST_array_k)
        fprintf(fid, ", %s", (varying) ? "idl_true" : "idl_false");

    if (BE_Is_Open_Record(BE_Ool_Info(param)->call_type))
    {
        open_array_param = BE_Open_Array(param);
        if (AST_SMALL_SET(open_array_param))
        {
            fprintf(fid, ", %s", BE_Array_Info(open_array_param)->size_exp);
        }
        else
        {
            num_array_dimensions = open_array_param->
                type->type_structure.array->index_count;
            for (i=0; i<num_array_dimensions; i++)
            {
                fprintf(fid, ", %s",
                    BE_get_name(BE_Array_Info(open_array_param)->Z[i]));
            }
        }
    }

    if (BE_Ool_Info(param)->call_type->kind == AST_array_k)
    {
        AST_array_index_n_t *index_p =
                flat_array_param->type->type_structure.array->index_vec;

        if (AST_SMALL_SET(param))
        {
            /*
             * Need to tell an ool v1_array marshalling routine whether
             * the object being unmarshalled is actually a v1_string.
             */
            if (!ool_type_has_pointers && !v1_string_type)
                fprintf (fid, ", idl_%s", v1_string_on_param ? "true" : "false");
            if (AST_CONFORMANT_SET(BE_Ool_Info(param)->call_type))
                fprintf(fid, ", %s", BE_Array_Info(param)->size_exp);
            if (varying)
                fprintf(fid, ", (%s)", BE_Array_Info(param)->count_exp);
            else
            {
                if (AST_FIXED_LOWER_SET(index_p)
                    &&  index_p->lower_bound->value.int_val == 0)
                {
                    if (AST_FIXED_UPPER_SET(index_p))
                        fprintf(fid, ", %ld",
                                index_p->upper_bound->value.int_val-
                                index_p->lower_bound->value.int_val+1);
                    else
                        fprintf(fid, ", %s", BE_Array_Info(param)->size_exp);
                }
                else
                    INTERNAL_ERROR("Array with nonzero lower bound not supported");
            }
        }
        else
        {
            num_array_dimensions
                = flat_array_param->type->type_structure.array->index_count;
            for (i=0; i<num_array_dimensions; i++)
            {
                if (AST_CONFORMANT_SET(flat_array_param->type))
                    fprintf(fid, ", %s", string ?
                        BE_get_name(
                            BE_Array_Info(flat_array_param)->Z[i]) :
                        BE_Array_Info(flat_array_param)->Z_exps[i]);
                if (varying)
                {
                    fprintf(fid, ", %s, %s",
                        BE_Array_Info(flat_array_param)->A_exps[i],
                        BE_Array_Info(flat_array_param)->B_exps[i]);
                }
                else
                {
                    if (AST_FIXED_LOWER_SET(index_p)
                        &&  index_p->lower_bound->value.int_val == 0)
                    {
                        fprintf(fid, ", %ld", index_p->lower_bound->value.int_val);
                        if (AST_FIXED_UPPER_SET(index_p))
                            fprintf(fid, ", %ld",
                                    index_p->upper_bound->value.int_val-
                                    index_p->lower_bound->value.int_val+1);
                        else
                            fprintf(fid, ", %s", string ?
                                BE_get_name(
                                    BE_Array_Info(flat_array_param)->Z[i]) :
                                BE_Array_Info(flat_array_param)->Z_exps[i]);
                    }
                    else
                        INTERNAL_ERROR("Array with nonzero lower bound not supported");
                }
                index_p++;
            }
        }
    }

    fprintf(fid, ");\n");
#ifndef NO_EXCEPTIONS
    CSPELL_return_on_status( fid );
#endif

    BE_m_end_buff(fid, flags);
}
