/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: genpipes.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:39:39  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:35:14  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:47:10  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:02:29  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:13  devrcs
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
**      genpipes.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Declaration and code generation needed for [in] and [out] pipes
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <cstubgen.h>
#include <genpipes.h>
#include <backend.h>
#include <inpipes.h>
#include <outpipes.h>

/******************************************************************************/
/*                                                                            */
/*    Find index of next [in] or [out] pipe                                   */
/*                                                                            */
/******************************************************************************/
static void BE_get_next_pipe_index
#ifdef PROTO
(
    AST_parameter_n_t *p_parameter,
    unsigned long ast_in_or_out,    /* AST_IN or AST_OUT */
    long curr_pipe_index,
    long *p_next_pipe_index      /* 0 if no more pipes inrequested direction */
)
#else
( p_parameter, ast_in_or_out, curr_pipe_index, p_next_pipe_index )
    AST_parameter_n_t *p_parameter;
    unsigned long ast_in_or_out;    /* AST_IN or AST_OUT */
    long curr_pipe_index;
    long *p_next_pipe_index ;
#endif
{
    for ( p_parameter = p_parameter->next;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        if ( (p_parameter->type->kind == AST_pipe_k)
             || ((p_parameter->type->kind == AST_pointer_k)
                 && (p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_pipe_k)) )
        {
            curr_pipe_index++;
            if (ast_in_or_out & (p_parameter->flags))
            {
                *p_next_pipe_index = curr_pipe_index;
                return;
            }
        }
    }
    *p_next_pipe_index = 0;
}

/******************************************************************************/
/*                                                                            */
/*    Get pipe type name for parameter                                        */
/*                                                                            */
/******************************************************************************/
static void BE_get_pipe_type_name
#ifdef PROTO
(
    AST_parameter_n_t *p_parameter,
    char **p_p_name
)
#else
(p_parameter, p_p_name)
    AST_parameter_n_t *p_parameter;
    char **p_p_name;
#endif
{
    if (p_parameter->type->kind == AST_pipe_k)
    {
        NAMETABLE_id_to_string( p_parameter->type->name, p_p_name );
    }
    else /* parameter is reference pointer to pipe */
    {
        NAMETABLE_id_to_string( p_parameter->type->type_structure.pointer
                                 ->pointee_type->name, p_p_name );
    }
}

/******************************************************************************/
/*                                                                            */
/*    Spell name of pipe structure derived from parameter into memory         */
/*                                                                            */
/******************************************************************************/
void BE_spell_pipe_struct_name
#ifdef PROTO
(
    AST_parameter_n_t *p_parameter,
    char pipe_struct_name[]
)
#else
(p_parameter, pipe_struct_name)
    AST_parameter_n_t *p_parameter;
    char pipe_struct_name[];
#endif
{
    char *p_param_name;

    NAMETABLE_id_to_string( p_parameter->be_info.param->name,
                            &p_param_name );
    if (p_parameter->type->kind == AST_pipe_k)
    {
        strcpy( pipe_struct_name, p_param_name );
    }
    else /* parameter is reference pointer to pipe */
    {
        strcpy( pipe_struct_name, "(*" );
        strcat( pipe_struct_name, p_param_name );
        strcat( pipe_struct_name, ")" );
    }

}

/******************************************************************************/
/*                                                                            */
/*    Initialization of server pipes                                          */
/*                                                                            */
/******************************************************************************/
void CSPELL_init_server_pipes
#ifdef PROTO
(
    FILE *fid,
    AST_operation_n_t *p_operation,
    long *p_first_pipe      /* ptr to index and direction of first pipe */
)
#else
( fid, p_operation, p_first_pipe )
    FILE *fid;
    AST_operation_n_t *p_operation;
    long *p_first_pipe;      /* ptr to index and direction of first pipe */
#endif
{
    long first_in_pipe;     /* index of first [in] pipe */
    long first_out_pipe;    /* index of first [out] pipe */
    long curr_pipe_index;
    long next_in_pipe_index;
    long next_out_pipe_index;
    AST_parameter_n_t *p_parameter;
    char *p_pipe_type_name;
    char pipe_struct_name[36];

    /* Establish indices of first pipes */
    first_in_pipe = 0;
    first_out_pipe = 0;
    curr_pipe_index = 0;
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        if ( (p_parameter->type->kind == AST_pipe_k)
             || ((p_parameter->type->kind == AST_pointer_k)
                 && (p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_pipe_k)) )
        {
            curr_pipe_index++;
            if ( AST_IN_SET(p_parameter) )
            {
                if (first_in_pipe == 0) first_in_pipe = curr_pipe_index;
            }
            if ( AST_OUT_SET(p_parameter) )
            {
                if (first_out_pipe == 0) first_out_pipe = curr_pipe_index;
            }
        }
    }
    if ( first_in_pipe != 0 ) *p_first_pipe = first_in_pipe;
    else *p_first_pipe = -first_out_pipe;

    /* Emit initialization code */
    curr_pipe_index = 0;
    for ( p_parameter = p_operation->parameters;
          p_parameter != NULL;
          p_parameter = p_parameter->next )
    {
        if ( (p_parameter->type->kind == AST_pipe_k)
             || ((p_parameter->type->kind == AST_pointer_k)
                 && (p_parameter->type->type_structure.pointer->pointee_type
                     ->kind == AST_pipe_k)) )
        {
            curr_pipe_index++;
            BE_get_pipe_type_name( p_parameter, &p_pipe_type_name );

            /* If the parameter was passed by reference,
               make the reference pointer point at some storage */
            if (p_parameter->type->kind == AST_pointer_k)
            {
                spell_name( fid, p_parameter->be_info.param->name );
                fprintf( fid,
                         "=(%s *)rpc_ss_mem_alloc(&mem_handle,sizeof(%s));\n",
                         p_pipe_type_name, p_pipe_type_name );
            }

            /* Hook the push and pull routines */
            BE_spell_pipe_struct_name( p_parameter, pipe_struct_name );
            fprintf( fid, "%s.push=%s_h;\n",
                          pipe_struct_name, p_pipe_type_name);
            fprintf( fid, "%s.pull=%s_l;\n",
                          pipe_struct_name, p_pipe_type_name);

            /* Initialize the state block */
            next_in_pipe_index = 0;
            next_out_pipe_index = 0;
            if ( AST_IN_SET(p_parameter) )
            {
                BE_get_next_pipe_index( p_parameter, AST_IN, curr_pipe_index,
                                                         &next_in_pipe_index );
                if (next_in_pipe_index == 0)
                {
                    /* Next pipe is [out] */
                    if (first_out_pipe != 0)
                         next_in_pipe_index = -first_out_pipe;
                    else next_in_pipe_index = BE_FINISHED_WITH_PIPES;
                }
            }
            if ( AST_OUT_SET(p_parameter) )
            {
                BE_get_next_pipe_index( p_parameter, AST_OUT, curr_pipe_index,
                                                         &next_out_pipe_index );
                if (next_out_pipe_index == 0 )
                     next_out_pipe_index = BE_FINISHED_WITH_PIPES;
                else
                     next_out_pipe_index = -next_out_pipe_index;
            }
            fprintf( fid,
        "rpc_ss_initialize_callee_pipe(%d,%d,%d,&current_pipe,&mp,&op,\n",
             curr_pipe_index, next_in_pipe_index, next_out_pipe_index );
            fprintf( fid, "*drep,elt,&mem_handle,call_h,(rpc_ss_ee_pipe_state_t**)");
            fprintf( fid, "&%s.state,(error_status_t*)&st);\n",
                          pipe_struct_name );
        }
    }

}

/******************************************************************************/
/*                                                                            */
/*    Procedure headers for pipe support routines in _saux file               */
/*                                                                            */
/******************************************************************************/
void CSPELL_pipe_support_header
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_pipe_type,
    BE_pipe_routine_k_t push_or_pull,
    boolean in_header
)
#else
( fid, p_pipe_type, push_or_pull, in_header )
    FILE *fid;
    AST_type_n_t *p_pipe_type;
    BE_pipe_routine_k_t push_or_pull;
    boolean in_header;
#endif
{
    char *count_string;

    count_string =
            (push_or_pull == BE_pipe_push_k) ? "pipe_chunk_size" : "*ecount" ;
    fprintf( fid, "\nvoid " );
    spell_name( fid, p_pipe_type->name );
    fprintf( fid, "_%c", (push_or_pull == BE_pipe_push_k) ? 'h' : 'l' );
    fprintf( fid, "\n#ifdef IDL_PROTOTYPES\n" );
    fprintf( fid, "(\n" );
    fprintf( fid, "rpc_ss_pipe_state_t state,\n" );
    CSPELL_type_exp_simple( fid, p_pipe_type->type_structure.pipe->base_type );
    fprintf( fid, " *chunk_array,\n" );
    if ( push_or_pull == BE_pipe_pull_k )
    {
        fprintf( fid, "unsigned long esize,\n" );
    }
    fprintf( fid, "unsigned long %s\n", count_string );
    fprintf( fid, ")\n" );
    fprintf( fid, "#else\n" );
    if (in_header)
    {
        fprintf( fid, "()\n" );
    }
    else
    {
        fprintf( fid, "(state,chunk_array," );
        if ( push_or_pull == BE_pipe_pull_k )
        {
            fprintf( fid, "esize,ecount)\n" );
        }
        else
        {
            fprintf( fid, "pipe_chunk_size)\n" );
        }
        fprintf( fid, "rpc_ss_pipe_state_t state;\n" );
        CSPELL_type_exp_simple( fid,
                                p_pipe_type->type_structure.pipe->base_type );
        fprintf( fid, " *chunk_array;\n" );
        if ( push_or_pull == BE_pipe_pull_k )
        {
            fprintf( fid, "unsigned long esize;\n" );
        }
        fprintf( fid, "unsigned long %s;\n", count_string );
    }
    fprintf( fid, "#endif\n" );
    if (in_header)
    {
        fprintf( fid, ";\n" );
    }
}

/******************************************************************************/
/*                                                                            */
/*    Control of generation of pipe routines                                  */
/*                                                                            */
/******************************************************************************/
void BE_gen_pipe_routines
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface
)
#else
( fid, p_interface )
    FILE *fid;
    AST_interface_n_t *p_interface;
#endif
{
    AST_type_p_n_t *curr_pipe_type_link;  /* Position in list of node types */

    for( curr_pipe_type_link = p_interface->pipe_types;
         curr_pipe_type_link != NULL;
         curr_pipe_type_link = curr_pipe_type_link->next )
    {
        BE_push_malloc_ctx();
        NAMETABLE_set_temp_name_mode();
        CSPELL_pipe_pull_routine( fid, curr_pipe_type_link->type );
        CSPELL_pipe_push_routine( fid, curr_pipe_type_link->type );
        NAMETABLE_clear_temp_name_mode();
        BE_pop_malloc_ctx();
    }
}

/******************************************************************************/
/*                                                                            */
/*    Control of generation of pipe routine declarations                      */
/*                                                                            */
/******************************************************************************/
void BE_gen_pipe_routine_decls
#ifdef PROTO
(
    FILE *fid,
    AST_interface_n_t *p_interface
)
#else
( fid, p_interface )
    FILE *fid;
    AST_interface_n_t *p_interface;
#endif
{
    AST_type_p_n_t *curr_pipe_type_link;  /* Position in list of node types */

    for( curr_pipe_type_link = p_interface->pipe_types;
         curr_pipe_type_link != NULL;
         curr_pipe_type_link = curr_pipe_type_link->next )
    {
        CSPELL_pipe_support_header( fid, curr_pipe_type_link->type,
                                    BE_pipe_pull_k, true );
        CSPELL_pipe_support_header( fid, curr_pipe_type_link->type,
                                    BE_pipe_push_k, true );
    }
}

/******************************************************************************/
/*                                                                            */
/*    Mark all array types in a pipe's flattened clone as undecorated.        */
/*                                                                            */
/******************************************************************************/
void BE_undec_piped_arrays
#ifdef PROTO
(
    AST_parameter_n_t *flat
)
#else
(flat)
    AST_parameter_n_t *flat;
#endif
{
    BE_arm_t *armp;

    switch(flat->type->kind)
    {
        case AST_array_k:
            BE_Array_Info(flat)->decorated = false;
            BE_Array_Info(flat)->element_ptr_var_declared = false;
            BE_undec_piped_arrays(BE_Array_Info(flat)->flat_elt);
            break;
        case AST_disc_union_k:
            for (armp = BE_Du_Info(flat)->arms; armp; armp = armp->next)
                BE_undec_piped_arrays(armp->flat);
            break;
    }
    if (flat->next)
        BE_undec_piped_arrays(flat->next);
}

/******************************************************************************/
/*                                                                            */
/*    Spell cast for base type of pipe                                        */
/*                                                                            */
/******************************************************************************/
void CSPELL_pipe_base_cast_exp
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type
)
#else
(fid, p_type)
    FILE *fid;
    AST_type_n_t *p_type;
#endif
{
    if (p_type->kind == AST_pipe_k)
    {
        CSPELL_cast_exp( fid, p_type->type_structure.pipe
                                        ->base_type );
    }
    else /* parameter is reference pointer to pipe */
    {
        INTERNAL_ERROR("Expected pipe type");
    }
}

/******************************************************************************/
/*                                                                            */
/*    Spell type name base type of pipe                                       */
/*                                                                            */
/******************************************************************************/
void CSPELL_pipe_base_type_exp
#ifdef PROTO
(
    FILE *fid,
    AST_type_n_t *p_type
)
#else
(fid, p_type )
    FILE *fid;
    AST_type_n_t *p_type;
#endif
{
    if (p_type->kind == AST_pipe_k)
    {
        CSPELL_type_exp_simple( fid, p_type->type_structure.pipe
                                        ->base_type );
    }
    else /* parameter is reference pointer to pipe */
    {
        CSPELL_type_exp_simple( fid, p_type->type_structure.pointer
                               ->pointee_type->type_structure.pipe->base_type );
    }
}
