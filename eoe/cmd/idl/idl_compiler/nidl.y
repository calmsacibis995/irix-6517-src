/*
 *  @OSF_COPYRIGHT@
 *  COPYRIGHT NOTICE
 *  Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 *  ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 *  src directory for the full copyright text.
 */

/*
 * HISTORY
 * $Log: nidl.y,v $
 * Revision 1.3  1994/02/21 19:02:19  jwag
 * add BB slot numbers; start support for interface slot numbers; lots of ksgen changes
 *
 * Revision 1.2  1993/09/08  20:13:23  jwag
 * first ksgen hack
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.4  1993/01/03  21:40:50  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:37:14  bbelch]
 *
 * Revision 1.1.2.3  1992/12/23  18:49:56  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:31  zeliff]
 * 
 * Revision 1.1.2.2  1992/12/03  13:13:01  hinxman
 * 	Fix OT 6078 and OT6269 Duplicate occurrences of "uuid", "version", "endpoint"
 * 		or "pointer_default" are errors
 * 	[1992/12/03  13:08:21  hinxman]
 * 
 * $EndLog$
 */

%{
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      IDL.Y
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      This module defines the main IDL grammar accepted
**      by the IDL compiler.
**
**  VERSION: DCE 1.0
**
*/

#ifdef vms
#  include <types.h>
#else
#  include <sys/types.h>
#endif

#include <nidl.h>
#include <nametbl.h>
#include <errors.h>
#include <ast.h>
#include <astp.h>
#include <frontend.h>

extern boolean search_attributes_table ;
#ifdef sgi
extern int parsedbb, parsedslot;
#endif

int yyparse (
#ifdef PROTO
    void
#endif
);

int yylex (
#ifdef PROTO
    void
#endif
);

/*
**  Local cells used for inter-production communication
*/
static ASTP_attr_k_t       ASTP_bound_type;    /* Array bound attribute */


%}


        /*   Declaration of yylval, yyval                   */
%union
{
    NAMETABLE_id_t         y_id ;          /* Identifier           */
    STRTAB_str_t           y_string ;      /* String               */
    STRTAB_str_t           y_float ;       /* Float constant       */
    long                   y_ival ;        /* Integer constant     */
    AST_export_n_t*        y_export ;      /* an export node       */
    AST_import_n_t*        y_import ;      /* Import node          */
    AST_constant_n_t*      y_constant;     /* Constant node        */
    AST_parameter_n_t*     y_parameter ;   /* Parameter node       */
    AST_type_n_t*          y_type ;        /* Type node            */
    AST_type_p_n_t*        y_type_ptr ;    /* Type pointer node    */
    AST_field_n_t*         y_field ;       /* Field node           */
    AST_arm_n_t*           y_arm ;         /* Union variant arm    */
    AST_operation_n_t*     y_operation ;   /* Routine node         */
    AST_interface_n_t*     y_interface ;   /* Interface node       */
    AST_case_label_n_t*    y_label ;       /* Union tags           */
    ASTP_declarator_n_t*   y_declarator ;  /* Declarator info      */
    ASTP_array_index_n_t*  y_index ;       /* Array index info     */
    nidl_uuid_t            y_uuid ;        /* Universal UID        */
    char                   y_char;         /* character constant   */
    ASTP_attributes_t      y_attributes;   /* attributes flags     */
    struct {
        AST_type_k_t    int_size;
        int             int_signed;
        }                  y_int_info;     /* int size and signedness */
    ASTP_exp_n_t           y_exp;          /* constant expression info */
}

/********************************************************************/
/*                                                                  */
/*          Tokens used by the IDL parser.                          */
/*                                                                  */
/********************************************************************/


/* Keywords                 */
%token ALIGN_KW
%token BYTE_KW
%token CHAR_KW
%token CONST_KW
%token DEFAULT_KW
%token ENUM_KW
%token FLOAT_KW
%token HYPER_KW
%token INT_KW
%token INTERFACE_KW
%token IMPORT_KW
%token LONG_KW
%token PIPE_KW
%token REF_KW
%token SMALL_KW
%token STRUCT_KW
%token TYPEDEF_KW
%token UNION_KW
%token UNSIGNED_KW
%token SHORT_KW
%token VOID_KW
%token DOUBLE_KW
%token BOOLEAN_KW
%token CASE_KW
%token SWITCH_KW
%token HANDLE_T_KW
%token TRUE_KW
%token FALSE_KW
%token NULL_KW
%token BROADCAST_KW
%token COMM_STATUS_KW
%token CONTEXT_HANDLE_KW
%token FIRST_IS_KW
%token HANDLE_KW
%token IDEMPOTENT_KW
%token IGNORE_KW
%token IMPLICIT_HANDLE_KW
%token IN_KW
%token LAST_IS_KW
%token LENGTH_IS_KW
%token LOCAL_KW
%token MAX_IS_KW
%token MAYBE_KW
%token MIN_IS_KW
%token MUTABLE_KW
%token OUT_KW
%token POINTER_DEFAULT_KW
%token ENDPOINT_KW
%token PTR_KW
%token REMOTE_KW
%token SECURE_KW
%token SHAPE_KW
%token SIZE_IS_KW
%token STRING_KW
%token TRANSMIT_AS_KW
%token UNIQUE_KW
%token UUID_KW
%token VERSION_KW
%token V1_ARRAY_KW
%token V1_STRING_KW
%token V1_ENUM_KW
%token V1_STRUCT_KW
/* SGI specific */
%token BB_KW
%token SLOT_KW


/*  Non-keyword tokens      */

%token UUID_REP


/*  Punctuation             */

%token COLON
%token COMMA
%token DOTDOT
%token EQUAL
%token LBRACE
%token LBRACKET
%token LPAREN
%token RBRACE
%token RBRACKET
%token RPAREN
%token SEMI
%token STAR
%token QUESTION
%token BAR
%token BARBAR
%token LANGLE
%token LANGLEANGLE
%token RANGLE
%token RANGLEANGLE
%token AMP
%token AMPAMP
%token LESSEQUAL
%token GREATEREQUAL
%token EQUALEQUAL
%token CARET
%token PLUS
%token MINUS
%token NOT
%token NOTEQUAL
%token SLASH
%token PERCENT
%token TILDE
%token UNKNOWN  /* Something that doesn't fit in any other token class */

/*  Tokens setting yylval   */

%token <y_id>      IDENTIFIER
%token <y_string>  STRING
%token <y_ival>    INTEGER_NUMERIC
%token <y_char>    CHAR
%token <y_float>   FLOAT_NUMERIC
%start interface

%%

/********************************************************************/
/*                                                                  */
/*          Syntax description and actions for IDL                  */
/*                                                                  */
/********************************************************************/

interface:
        interface_init interface_start interface_tail
        {
            AST_finish_interface_node(the_interface);
        }
    ;

interface_start:
        interface_attributes INTERFACE_KW IDENTIFIER
        {
            the_interface->name = $<y_id>3;
            ASTP_add_name_binding(the_interface->name, (char *)the_interface);
        }
    ;

interface_init:
        /* Always create the interface node and auto-import the system idl */
        {
            STRTAB_str_t nidl_idl_str;
#ifdef __sgi
	    extern char *auto_import_file;
            nidl_idl_str = STRTAB_add_string (auto_import_file);
#else
            nidl_idl_str = STRTAB_add_string (AUTO_IMPORT_FILE);
#endif
            the_interface = AST_interface_node();
            the_interface->imports = AST_import_node(nidl_idl_str);
            the_interface->imports->interface = FE_parse_import (nidl_idl_str);
            if (the_interface->imports->interface != NULL)
            {
                AST_CLR_OUT_OF_LINE(the_interface->imports->interface);
                AST_SET_IN_LINE(the_interface->imports->interface);
            }
        }
    ;

interface_tail:
        LBRACE interface_body RBRACE
        { $<y_interface>$ = $<y_interface>2; }
    |   error
        {
            $<y_interface>$ = NULL;
            log_error(yylineno,NIDL_MISSONINTER);
        }
    |   error RBRACE
        {
            $<y_interface>$ = NULL;
        }
    ;


interface_body:
        optional_imports exports extraneous_semi
        {
            /* May already be an import of nbase, so concat */
            the_interface->imports = (AST_import_n_t *) AST_concat_element(
                                        (ASTP_node_t *) the_interface->imports,
                                        (ASTP_node_t *) $<y_import>1);
            the_interface->exports = $<y_export>2;
        }
  ;

optional_imports:
        imports
    |   /* Nothing */
        {
            $<y_import>$ = (AST_import_n_t *)NULL;
        }

imports:
        import
    |   imports import
        {
                $<y_import>$ = (AST_import_n_t *) AST_concat_element(
                                                (ASTP_node_t *) $<y_import>1,
                                                (ASTP_node_t *) $<y_import>2);
        }
    ;

import:
        IMPORT_KW error
        {
            $<y_import>$ = (AST_import_n_t *)NULL;
        }
    |   IMPORT_KW error SEMI
        {
            $<y_import>$ = (AST_import_n_t *)NULL;
        }
    |   IMPORT_KW import_files SEMI
        {
            $<y_import>$ = $<y_import>2;
        }
    ;

import_files:
        import_file
    |   import_files COMMA import_file
        {
                $<y_import>$ = (AST_import_n_t *) AST_concat_element(
                                                (ASTP_node_t *) $<y_import>1,
                                                (ASTP_node_t *) $<y_import>3);
        }
    ;



import_file:
        STRING
        {
            AST_interface_n_t  *int_p;
            int_p = FE_parse_import ($<y_string>1);
            if (int_p != (AST_interface_n_t *)NULL)
            {
                $<y_import>$ = AST_import_node($<y_string>1);
                $<y_import>$->interface = int_p;
            }
            else
                $<y_import>$ = (AST_import_n_t *)NULL;
        }
    ;

exports:
        export
    |   exports extraneous_semi export
        {
                $<y_export>$ = (AST_export_n_t *) AST_concat_element(
                                            (ASTP_node_t *) $<y_export>1,
                                            (ASTP_node_t *) $<y_export>3) ;
        }
    ;


export:
        type_dcl      SEMI
        {
                $<y_export>$ = AST_types_to_exports ($<y_type_ptr>1);
        }
    |   const_dcl     SEMI
        {
                $<y_export>$ = AST_export_node (
                        (ASTP_node_t *) $<y_constant>1, AST_constant_k);
        }
    |   operation_dcl SEMI
        {
            if (ASTP_parsing_main_idl)
                $<y_export>$ = AST_export_node (
                        (ASTP_node_t *) $<y_operation>1, AST_operation_k);
        }
    |   error SEMI
        {
            $<y_export>$ = (AST_export_n_t *)NULL;
        }
    ;

const_dcl:
        CONST_KW type_spec declarator EQUAL const_exp

        {
           $<y_constant>$ = AST_finish_constant_node ($<y_constant>5,
                                        $<y_declarator>3, $<y_type>2);
        }
    ;


const_exp:  expression
        {
            if ($<y_exp>1.type == AST_int_const_k)
                $<y_constant>$ = AST_integer_constant ($<y_exp>1.val.integer) ;
            else
                $<y_constant>$ = $<y_exp>1.val.other;
        }
    ;


type_dcl:
        TYPEDEF_KW type_declarator
        {
            $<y_type_ptr>$ = $<y_type_ptr>2;
        }
    ;

type_declarator:
        attributes type_spec declarators
        {
            $<y_type_ptr>$  = AST_declarators_to_types($<y_type>2,
                        $<y_declarator>3, &$<y_attributes>1) ;
            ASTP_free_simple_list((ASTP_node_t *)$<y_attributes>1.bounds);
        }
    ;


type_spec:
        simple_type_spec
    |   constructed_type_spec
    ;

simple_type_spec:
        floating_point_type_spec
    |   integer_type_spec
    |   char_type_spec
    |   boolean_type_spec
    |   byte_type_spec
    |   void_type_spec
    |   named_type_spec
    |   handle_type_spec
    ;


constructed_type_spec:
        struct_type_spec
    |   union_type_spec
    |   enum_type_spec
    |   pipe_type_spec
    ;


named_type_spec:
        IDENTIFIER
        {
            $<y_type>$ = AST_lookup_named_type($<y_id>1);
        }
    ;

floating_point_type_spec:
        FLOAT_KW
        {
            $<y_type>$ = AST_lookup_type_node(AST_short_float_k);
        }
    |   DOUBLE_KW
        {
            $<y_type>$ = AST_lookup_type_node(AST_long_float_k);
        }
    ;

extraneous_comma:
        /* Nothing */
    |   COMMA
        { log_warning(yylineno, NIDL_EXTRAPUNCT, ",");}
    ;

extraneous_semi:
        /* Nothing */
    |   SEMI
        { log_warning(yylineno, NIDL_EXTRAPUNCT, ";");}
    ;

optional_unsigned_kw:
        UNSIGNED_KW     { $<y_ival>$ = true; }
    |   /* Nothing */   { $<y_ival>$ = false; }
    ;

integer_size_spec:
        SMALL_KW
        {
            $<y_int_info>$.int_size = AST_small_integer_k;
            $<y_int_info>$.int_signed = true;
        }
    |   SHORT_KW
        {
            $<y_int_info>$.int_size = AST_short_integer_k;
            $<y_int_info>$.int_signed = true;
        }
    |   LONG_KW
        {
            $<y_int_info>$.int_size = AST_long_integer_k;
            $<y_int_info>$.int_signed = true;
        }
    |   HYPER_KW
        {
            $<y_int_info>$.int_size = AST_hyper_integer_k;
            $<y_int_info>$.int_signed = true;
        }
    ;

integer_modifiers:
        integer_size_spec
        { $<y_int_info>$ = $<y_int_info>1; }
    |   UNSIGNED_KW integer_size_spec
        {
            $<y_int_info>$.int_size = $<y_int_info>2.int_size;
            $<y_int_info>$.int_signed = false;
        }
    |   integer_size_spec UNSIGNED_KW
        {
            $<y_int_info>$.int_size = $<y_int_info>1.int_size;
            $<y_int_info>$.int_signed = false;
        }
    ;

integer_type_spec:
        integer_modifiers
        { $<y_type>$ = AST_lookup_integer_type_node($<y_int_info>1.int_size,$<y_int_info>1.int_signed); }
    |   integer_modifiers INT_KW
        { $<y_type>$ = AST_lookup_integer_type_node($<y_int_info>1.int_size,$<y_int_info>1.int_signed); }
    |   optional_unsigned_kw INT_KW
        {
            log_warning(yylineno,NIDL_INTSIZEREQ);
            $<y_type>$ = AST_lookup_integer_type_node(AST_long_integer_k,$<y_ival>1);
        }
    ;

char_type_spec:
        optional_unsigned_kw CHAR_KW
        { $<y_type>$ = AST_lookup_type_node(AST_character_k); }
    ;

boolean_type_spec:
        BOOLEAN_KW
        { $<y_type>$ = AST_lookup_type_node(AST_boolean_k); }
    ;

byte_type_spec:
        BYTE_KW
        { $<y_type>$ = AST_lookup_type_node(AST_byte_k); }
    ;

void_type_spec:
        VOID_KW
        { $<y_type>$ = AST_lookup_type_node(AST_void_k); }
    ;

handle_type_spec:
       HANDLE_T_KW
        { $<y_type>$ = AST_lookup_type_node(AST_handle_k); }
    ;

push_name_space:
        LBRACE
        {
            NAMETABLE_push_level ();
        }
    ;

pop_name_space:
        RBRACE
        {
            ASTP_patch_field_reference ();
            NAMETABLE_pop_level ();
        }
    ;

union_type_spec:
        UNION_KW SWITCH_KW LPAREN simple_type_spec IDENTIFIER RPAREN union_body
        {
        $<y_type>$ = AST_disc_union_node(
                         NAMETABLE_NIL_ID,      /* tag name          */
                         ASTP_tagged_union_id,  /* union name        */
                         $<y_id>5,              /* discriminant name */
                         $<y_type>4,            /* discriminant type */
                         $<y_arm>7 );           /* the arm list      */
        }
    |   UNION_KW SWITCH_KW LPAREN simple_type_spec IDENTIFIER RPAREN IDENTIFIER union_body
        {
        $<y_type>$ = AST_disc_union_node(
                         NAMETABLE_NIL_ID,      /* tag name          */
                         $<y_id>7,              /* union name        */
                         $<y_id>5,              /* discriminant name */
                         $<y_type>4,            /* discriminant type */
                         $<y_arm>8 );           /* the arm list      */
        }
    |   UNION_KW IDENTIFIER SWITCH_KW LPAREN simple_type_spec IDENTIFIER RPAREN union_body
        {
        $<y_type>$ = AST_disc_union_node(
                         $<y_id>2,              /* tag name          */
                         ASTP_tagged_union_id,  /* union name        */
                         $<y_id>6,              /* discriminant name */
                         $<y_type>5,            /* discriminant type */
                         $<y_arm>8 );           /* the arm list      */
        }
    |   UNION_KW IDENTIFIER SWITCH_KW LPAREN simple_type_spec IDENTIFIER RPAREN IDENTIFIER union_body
        {
        $<y_type>$ = AST_disc_union_node(
                         $<y_id>2,              /* tag name          */
                         $<y_id>8,              /* union name        */
                         $<y_id>6,              /* discriminant name */
                         $<y_type>5,            /* discriminant type */
                         $<y_arm>9 );           /* the arm list      */
        }
    |   UNION_KW IDENTIFIER
        {
            $<y_type>$ = AST_type_from_tag (AST_disc_union_k, $<y_id>2);
        }
    ;

union_body:
        push_name_space union_cases pop_name_space
        {
                $<y_arm>$ = $<y_arm>2;
        }
    ;

union_cases:
        union_case
    |   union_cases extraneous_semi union_case
        {
            $<y_arm>$ = (AST_arm_n_t *) AST_concat_element(
                                        (ASTP_node_t *) $<y_arm>1,
                                        (ASTP_node_t *) $<y_arm>3);
        }
    ;

union_case:
        union_case_list union_member
        {
            $<y_arm>$ = AST_label_arm($<y_arm>2, $<y_label>1) ;
        }
    ;

union_case_list:
        union_case_label
    |   union_case_list union_case_label
        {
            $<y_label>$ = (AST_case_label_n_t *) AST_concat_element(
                                        (ASTP_node_t *) $<y_label>1,
                                        (ASTP_node_t *) $<y_label>2);
        }
    ;

union_case_label:
        CASE_KW const_exp COLON
        {
            $<y_label>$ = AST_case_label_node($<y_constant>2);
        }
    |   DEFAULT_KW COLON
        {
            $<y_label>$ = AST_default_case_label_node();
        }
    ;

union_member:
        /* nothing */ SEMI
        {
            $<y_arm>$ = AST_arm_node(NAMETABLE_NIL_ID,NULL,NULL);
        }
    |   attributes type_spec declarator SEMI
        {
            $<y_arm>$ = AST_declarator_to_arm($<y_type>2,
                                $<y_declarator>3, &$<y_attributes>1);
            ASTP_free_simple_list((ASTP_node_t *)$<y_attributes>1.bounds);
        }
    ;

struct_type_spec:
        STRUCT_KW push_name_space member_list pop_name_space
        {
            $<y_type>$ = AST_structure_node($<y_field>3, NAMETABLE_NIL_ID) ;
        }
    |   STRUCT_KW IDENTIFIER push_name_space member_list pop_name_space
        {
            $<y_type>$ = AST_structure_node($<y_field>4, $<y_id>2) ;
        }
    |   STRUCT_KW IDENTIFIER
        {
            $<y_type>$ = AST_type_from_tag (AST_structure_k, $<y_id>2);
        }
    ;

member_list:
        member
    |   member_list extraneous_semi member
        {
            $<y_field>$ = (AST_field_n_t *)AST_concat_element(
                                    (ASTP_node_t *) $<y_field>1,
                                    (ASTP_node_t *) $<y_field>3) ;
        }
    ;

member:
        attributes type_spec old_attribute_syntax declarators SEMI
        {
            $<y_field>$ = AST_declarators_to_fields($<y_declarator>4,
                                                    $<y_type>2,
                                                    &$<y_attributes>1);
            ASTP_free_simple_list((ASTP_node_t *)$<y_attributes>1.bounds);
        }
    ;

enum_type_spec:
        ENUM_KW enum_body
        {
             $<y_type>$ = AST_enumerator_node($<y_constant>2, AST_short_integer_k);
        }
    ;

enum_body:
        LBRACE enum_ids RBRACE
        {
            $<y_constant>$ = $<y_constant>2 ;
        }
    ;

enum_ids:
    |   enum_id
    |   enum_ids COMMA extraneous_comma enum_id
        {
            $<y_constant>$ = (AST_constant_n_t *) AST_concat_element(
                                    (ASTP_node_t *) $<y_constant>1,
                                    (ASTP_node_t *) $<y_constant>4) ;
        }
    ;

enum_id:
        IDENTIFIER
        {
            $<y_constant>$  = AST_enum_constant($<y_id>1) ;
        }
    ;

pipe_type_spec:
        PIPE_KW type_spec
        {
            $<y_type>$ = AST_pipe_node ($<y_type>2);
        }
    ;

declarators:
        declarator
    |   declarators COMMA extraneous_comma declarator
        {
            $<y_declarator>$ = (ASTP_declarator_n_t *) AST_concat_element(
                                            (ASTP_node_t *) $<y_declarator>1,
                                            (ASTP_node_t *) $<y_declarator>4) ;
        }
    ;



declarator:
        direct_declarator
            { $<y_declarator>$ = $<y_declarator>1; }
       |    pointer direct_declarator
            {
                $<y_declarator>$ = $<y_declarator>2;
                AST_declarator_operation($<y_declarator>$, AST_pointer_k,
                        (ASTP_node_t *)NULL, $<y_ival>1 );
            };


pointer :
            STAR
            { $<y_ival>$ = 1;}
       |    STAR pointer
            { $<y_ival>$ = $<y_ival>2 + 1; };


direct_declarator:
            IDENTIFIER
            { $<y_declarator>$ = AST_declarator_node ( $<y_id>1 ); }
       |        direct_declarator array_bounds
            {
                $<y_declarator>$ = $<y_declarator>$;
                AST_declarator_operation($<y_declarator>$, AST_array_k,
                        (ASTP_node_t *) $<y_index>2, 0 );
            }
       |   LPAREN declarator RPAREN
            {
            $<y_declarator>$ = $<y_declarator>2;
            }
       |        direct_declarator parameter_dcls
            {
                $<y_declarator>$ = $<y_declarator>$;
                AST_declarator_operation($<y_declarator>$, AST_function_k,
                        (ASTP_node_t *) $<y_parameter>2, 0 );
            }
       ;


    /*
     * The following productions use an AST routine with the signature:
     *
     *   ASTP_array_index_node( AST_constant_n_t * lower_bound,
     *                          ASTP_bound_t lower_bound_type,
     *                          AST_constant_n_t * upper_bound,
     *                          ASTP_bound_t upper_bound_type);
     *
     * The type ASTP_bound_t is defined as:
     *
     *   typedef enum {ASTP_constant_bound,
     *                 ASTP_default_bound,
     *                 ASTP_open_bound} ASTP_bound_t;
     *
     * The bound value passed is only used if the associated bound type is
     * ASTP_constant_bound.
     */

array_bounds:
        LBRACKET RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node ( NULL, ASTP_default_bound,
                                                 NULL, ASTP_open_bound);
        }
    |   LBRACKET STAR RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node  ( NULL, ASTP_default_bound,
                                                 NULL, ASTP_open_bound);
        }
    |   LBRACKET const_exp RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node  ( NULL, ASTP_default_bound,
                                                 $<y_constant>2, ASTP_constant_bound);
        }
    |   LBRACKET STAR DOTDOT STAR RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node  ( NULL, ASTP_open_bound,
                                                 NULL, ASTP_open_bound);
        }
    |   LBRACKET STAR DOTDOT const_exp RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node  ( NULL, ASTP_open_bound,
                                                 $<y_constant>4, ASTP_constant_bound);
        }
    |   LBRACKET const_exp DOTDOT STAR RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node  ( $<y_constant>2, ASTP_constant_bound,
                                                 NULL, ASTP_open_bound);
        }
    |   LBRACKET const_exp DOTDOT const_exp RBRACKET
        {
            $<y_index>$ = ASTP_array_index_node  ( $<y_constant>2, ASTP_constant_bound,
                                                 $<y_constant>4, ASTP_constant_bound);
        }



operation_dcl:
        attributes type_spec declarators
        {
            if (ASTP_parsing_main_idl)
                $<y_operation>$ = AST_operation_node (
                                    $<y_type>2,         /*The type node*/
                                    $<y_declarator>3,   /* Declarator list */
                                   &$<y_attributes>1);  /* attributes */
            ASTP_free_simple_list((ASTP_node_t *)$<y_attributes>1.bounds);
        }
    | error declarators
        {
        log_error(yylineno,NIDL_MISSONOP);
        $<y_operation>$ = NULL;
        }
    ;

parameter_dcls:
        param_names param_list end_param_names
        {
            $<y_parameter>$ = $<y_parameter>2;
        }
    ;

param_names:
        LPAREN
        {
        NAMETABLE_push_level ();
        }
    ;

end_param_names:
        extraneous_comma RPAREN
        {
        ASTP_patch_field_reference ();
        NAMETABLE_pop_level ();
        }
    ;

param_list:
        param_dcl
    |   param_list COMMA extraneous_comma param_dcl
        {
            if (ASTP_parsing_main_idl)
                $<y_parameter>$ = (AST_parameter_n_t *) AST_concat_element(
                                    (ASTP_node_t *) $<y_parameter>1,
                                    (ASTP_node_t *) $<y_parameter>4);
        }
    |   /* nothing */
        {
            $<y_parameter>$ = (AST_parameter_n_t *)NULL;
        }
    ;

param_dcl:
        attributes type_spec old_attribute_syntax declarator_or_null
        {
            /*
             * We have to use special code here to allow (void) as a parameter
             * specification.  If there are no declarators, then we need to
             * make sure that the type is void and that there are no attributes .
             */
            if ($<y_declarator>4 == NULL)
            {
                /*
                 * If the type is not void or some attribute is specified,
                 * there is a syntax error.  Force a yacc error, so that
                 * we can safely recover from the lack of a declarator.
                 */
                if (($<y_type>2->kind != AST_void_k) ||
                   ($<y_attributes>1.bounds != NULL) ||
                   ($<y_attributes>1.attr_flags != 0))
                {
                    yywhere();  /* Issue a syntax error for this line */
                    YYERROR;    /* Allow natural error recovery */
                }

                $<y_parameter>$ = (AST_parameter_n_t *)NULL;
            }
            else
            {
                if (ASTP_parsing_main_idl)
                    $<y_parameter>$ = AST_declarator_to_param(
                                            &$<y_attributes>1,
                                            $<y_type>2,
                                            $<y_declarator>4);
            }
            ASTP_free_simple_list((ASTP_node_t *)$<y_attributes>1.bounds);
        }
    |    error old_attribute_syntax declarator_or_null
        {
            log_error(yylineno, NIDL_MISSONPARAM);
            $<y_parameter>$ = (AST_parameter_n_t *)NULL;
        }
    ;

declarator_or_null:
        declarator
        { $<y_declarator>$ = $<y_declarator>1; }
    |   /* nothing */
        { $<y_declarator>$ = NULL; }
    ;

/*
 * Attribute definitions
 *
 * Attributes may appear on types, fields, parameters, operations and
 * interfaces.  Thes productions must be used around attributes in order
 * for LEX to recognize attribute names as keywords instead of identifiers.
 * The bounds productions are used in attribute options (such
 * as size_is) because variable names the may look like attribute names
 * should be allowed.
 */

attribute_opener:
        LBRACKET
        {
            search_attributes_table = true;
        }
    ;

attribute_closer:
        RBRACKET
        {
            search_attributes_table = false;
        }
    ;

bounds_opener:
        LPAREN
        {
            search_attributes_table = false;
        }
    ;

bounds_closer:
        RPAREN
        {
            search_attributes_table = true;
        }
    ;


/*
 * Production to accept attributes in the old location, and issue a clear error that
 * the translator should be used.
 */
old_attribute_syntax:
        attributes
        {
            /* Give an error on notranslated sources */
            if (($<y_attributes>1.bounds != NULL) ||
               ($<y_attributes>1.attr_flags != 0))
            {
                log_error(yylineno,NIDL_ATTRTRANS);
                ASTP_free_simple_list((ASTP_node_t *)$<y_attributes>1.bounds);
            }
        }
    ;


/*
 * Interface Attributes
 *
 * Interface attributes are special--there is no cross between interface
 * attributes and other attributes (for instance on fields or types.
 */
interface_attributes:
        attribute_opener interface_attr_list attribute_closer
    |   attribute_opener error attribute_closer
        {
            log_error(yylineno,NIDL_ERRINATTR);
        }

    |   /* Nothing */
    ;

interface_attr_list:
        interface_attr
    |   interface_attr_list COMMA extraneous_comma interface_attr
    |   /* nothing */
    ;

interface_attr:
        UUID_KW error
        {
            log_error(yylineno,NIDL_SYNTAXUUID);
        }
    |   UUID_KW UUID_REP
        {
            {
                if (ASTP_IF_AF_SET(the_interface,ASTP_IF_UUID))
                        log_error(yylineno, NIDL_ATTRUSEMULT);
                ASTP_SET_IF_AF(the_interface,ASTP_IF_UUID);
                the_interface->uuid = $<y_uuid>2;
            }
        }
    |   ENDPOINT_KW LPAREN port_list RPAREN
        {
            if (ASTP_IF_AF_SET(the_interface,ASTP_IF_PORT))
                    log_error(yylineno, NIDL_ATTRUSEMULT);
            ASTP_SET_IF_AF(the_interface,ASTP_IF_PORT);
        }
    |   VERSION_KW LPAREN version_number RPAREN
        {
            {
                if (ASTP_IF_AF_SET(the_interface,ASTP_IF_VERSION))
                        log_error(yylineno, NIDL_ATTRUSEMULT);
                ASTP_SET_IF_AF(the_interface,ASTP_IF_VERSION);
            }

        }
    |   LOCAL_KW
        {
            {
                if (AST_LOCAL_SET(the_interface))
                        log_warning(yylineno, NIDL_MULATTRDEF);
                AST_SET_LOCAL(the_interface);
            }
        }
    |   POINTER_DEFAULT_KW LPAREN pointer_class RPAREN
        {
            if (interface_pointer_class != 0)
                    log_error(yylineno, NIDL_ATTRUSEMULT);
            interface_pointer_class = $<y_ival>3;
        }
    ;

pointer_class:
        REF_KW { $<y_ival>$ = ASTP_REF; }
    |   PTR_KW { $<y_ival>$ = ASTP_PTR; }
    |   UNIQUE_KW { $<y_ival>$ = ASTP_UNIQUE; }
    ;

version_number:
        INTEGER_NUMERIC
        {
            the_interface->version = $<y_ival>1;
            if ($<y_ival>1 > (unsigned int)ASTP_C_USHORT_MAX)
                log_error(yylineno, NIDL_MAJORTOOLARGE, ASTP_C_USHORT_MAX);
        }
   |    FLOAT_NUMERIC
        {
            char    *float_text;
            unsigned int            major_version,minor_version;
            STRTAB_str_to_string($<y_string>1, &float_text);
            sscanf(float_text,"%d.%d",&major_version,&minor_version);
            if (major_version > (unsigned int)ASTP_C_USHORT_MAX)
                log_error(yylineno, NIDL_MAJORTOOLARGE, ASTP_C_USHORT_MAX);
            if (minor_version > (unsigned int)ASTP_C_USHORT_MAX)
                log_error(yylineno, NIDL_MINORTOOLARGE, ASTP_C_USHORT_MAX);
            the_interface->version = (minor_version * 65536) + major_version;
        }
    ;

port_list:
        port_spec
    |   port_list COMMA extraneous_comma port_spec
    ;

port_spec:
        STRING
        {
            ASTP_parse_port(the_interface,$<y_string>1);
        }
    ;




/*
 * Attributes that can appear on fields or parameters. These are the array
 * bounds attributes, i.e., last_is and friends. They are handled differently
 * from any other attributes.
 */
fp_attribute:
        array_bound_type bounds_opener array_bound_id_list bounds_closer
        {
            $<y_attributes>$.bounds = $<y_attributes>3.bounds;
            $<y_attributes>$.attr_flags = 0;
        }
    ;

array_bound_type:
        FIRST_IS_KW
        {
            ASTP_bound_type = first_is_k;
        }
    |   LAST_IS_KW
        {
            ASTP_bound_type = last_is_k;
        }
    |   LENGTH_IS_KW
        {
            ASTP_bound_type = length_is_k;
        }
    |   MAX_IS_KW
        {
            ASTP_bound_type = max_is_k;
        }
    |   MIN_IS_KW
        {
            ASTP_bound_type = min_is_k;
        }
    |   SIZE_IS_KW
        {
            ASTP_bound_type = size_is_k;
        }
    ;


array_bound_id_list:
        array_bound_id
    |   array_bound_id_list COMMA array_bound_id
        {
        $<y_attributes>$.bounds = (ASTP_type_attr_n_t *) AST_concat_element (
                                (ASTP_node_t*) $<y_attributes>1.bounds,
                                (ASTP_node_t*) $<y_attributes>3.bounds);
        }
    ;

array_bound_id:
        IDENTIFIER
        {
        $<y_attributes>$.bounds = AST_array_bound_info ($<y_id>1, ASTP_bound_type, FALSE);
        }
    |   STAR IDENTIFIER
        {
        $<y_attributes>$.bounds = AST_array_bound_info ($<y_id>2, ASTP_bound_type, TRUE);
        }
    |   /* nothing */
        {
        $<y_attributes>$.bounds = AST_array_bound_info (NAMETABLE_NIL_ID, ASTP_bound_type, FALSE);
        }
    ;


/*
 * Generalized Attribute processing
 */
attributes:
        attribute_opener rest_of_attribute_list
        { $<y_attributes>$ = $<y_attributes>2; }
     |
        /* nothing */
        {
        $<y_attributes>$.bounds = NULL;
        $<y_attributes>$.attr_flags = 0;
        }
     ;


rest_of_attribute_list:
        attribute_list attribute_closer
     |  error attribute_closer
        {
        /*
         * Can't tell if we had any valid attributes in the list, so return
         * none.
         */
        $<y_attributes>$.bounds = NULL;
        $<y_attributes>$.attr_flags = 0;
        log_error(yylineno, NIDL_ERRINATTR);
        }
     |  error SEMI
        {
        /*
         * No closer to the attribute, so give a different message.
         */
        $<y_attributes>$.bounds = NULL;
        $<y_attributes>$.attr_flags = 0;
        log_error(yylineno, NIDL_MISSONATTR);
        search_attributes_table = false;
        }
     ;


attribute_list:
        attribute
        { $<y_attributes>$ = $<y_attributes>1; }
     |
        attribute_list COMMA extraneous_comma attribute
        {
          /*
           * If the same bit has been specified more than once, then issue
           * a message.
           */
          if (($<y_attributes>1.attr_flags & $<y_attributes>4.attr_flags) != 0)
                log_warning(yylineno, NIDL_MULATTRDEF);
          $<y_attributes>$.attr_flags = $<y_attributes>1.attr_flags |
                                        $<y_attributes>4.attr_flags;
          $<y_attributes>$.bounds = (ASTP_type_attr_n_t *) AST_concat_element (
                                (ASTP_node_t*) $<y_attributes>1.bounds,
                                (ASTP_node_t*) $<y_attributes>4.bounds);
        }
     ;


attribute:
        /* bound attributes */
        fp_attribute            { $<y_attributes>$ = $<y_attributes>1; }

        /* Operation Attributes */
    |   BROADCAST_KW            { $<y_attributes>$.attr_flags = ASTP_BROADCAST;
                                  $<y_attributes>$.bounds = NULL;       }
    |   MAYBE_KW                { $<y_attributes>$.attr_flags = ASTP_MAYBE;
                                  $<y_attributes>$.bounds = NULL;       }
    |   IDEMPOTENT_KW           { $<y_attributes>$.attr_flags = ASTP_IDEMPOTENT;
                                  $<y_attributes>$.bounds = NULL;       }
/* SGI specific */
    |   SLOT_KW LPAREN INTEGER_NUMERIC RPAREN
				{ $<y_attributes>$.attr_flags = ASTP_SLOT;
                                  $<y_attributes>$.bounds = NULL;
				  parsedslot = $3;			}
    |   BB_KW LPAREN INTEGER_NUMERIC RPAREN
				{ $<y_attributes>$.attr_flags = ASTP_BB;
                                  $<y_attributes>$.bounds = NULL;
				  parsedbb = $3;			}
/* end SGI specific */

        /* Parameter-only Attributes */
    |   PTR_KW                  { $<y_attributes>$.attr_flags = ASTP_PTR;
                                  $<y_attributes>$.bounds = NULL;       }
    |   IN_KW                   { $<y_attributes>$.attr_flags = ASTP_IN;
                                  $<y_attributes>$.bounds = NULL;       }
    |   IN_KW LPAREN SHAPE_KW RPAREN
                                { $<y_attributes>$.attr_flags =
                                        ASTP_IN | ASTP_IN_SHAPE;
                                  $<y_attributes>$.bounds = NULL;       }
    |   OUT_KW                  { $<y_attributes>$.attr_flags = ASTP_OUT;
                                  $<y_attributes>$.bounds = NULL;       }
    |   OUT_KW LPAREN SHAPE_KW RPAREN
                                { $<y_attributes>$.attr_flags =
                                        ASTP_OUT | ASTP_OUT_SHAPE;
                                  $<y_attributes>$.bounds = NULL;       }

        /* Type, Field, Parameter Attributes */
    |   V1_ARRAY_KW             { $<y_attributes>$.attr_flags = ASTP_SMALL;
                                  $<y_attributes>$.bounds = NULL;       }
    |   STRING_KW               { $<y_attributes>$.attr_flags = ASTP_STRING;
                                  $<y_attributes>$.bounds = NULL;       }
    |   V1_STRING_KW            { $<y_attributes>$.attr_flags = ASTP_STRING0;
                                  $<y_attributes>$.bounds = NULL;       }
    |   UNIQUE_KW               { $<y_attributes>$.attr_flags = ASTP_UNIQUE;
                                  $<y_attributes>$.bounds = NULL;       }
    |   REF_KW                  { $<y_attributes>$.attr_flags = ASTP_REF;
                                  $<y_attributes>$.bounds = NULL;       }
    |   IGNORE_KW               { $<y_attributes>$.attr_flags = ASTP_IGNORE;
                                  $<y_attributes>$.bounds = NULL;       }
    |   CONTEXT_HANDLE_KW       { $<y_attributes>$.attr_flags = ASTP_CONTEXT;
                                  $<y_attributes>$.bounds = NULL;       }

        /* Type-only Attribute(s) */
    |   V1_STRUCT_KW            { $<y_attributes>$.attr_flags = ASTP_UNALIGN;
                                  $<y_attributes>$.bounds = NULL;       }
    |   V1_ENUM_KW              { $<y_attributes>$.attr_flags = ASTP_V1_ENUM;
                                  $<y_attributes>$.bounds = NULL;       }
    |   ALIGN_KW LPAREN SMALL_KW RPAREN
                                { $<y_attributes>$.attr_flags = ASTP_ALIGN_SMALL;
                                  $<y_attributes>$.bounds = NULL;       }
    |   ALIGN_KW LPAREN SHORT_KW RPAREN
                                { $<y_attributes>$.attr_flags = ASTP_ALIGN_SHORT;
                                  $<y_attributes>$.bounds = NULL;       }
    |   ALIGN_KW LPAREN LONG_KW RPAREN
                                { $<y_attributes>$.attr_flags = ASTP_ALIGN_LONG;
                                  $<y_attributes>$.bounds = NULL;       }
    |   ALIGN_KW LPAREN HYPER_KW RPAREN
                                { $<y_attributes>$.attr_flags = ASTP_ALIGN_HYPER;
                                  $<y_attributes>$.bounds = NULL;       }
    |   HANDLE_KW               { $<y_attributes>$.attr_flags = ASTP_HANDLE;
                                  $<y_attributes>$.bounds = NULL;       }
    |   TRANSMIT_AS_KW LPAREN simple_type_spec RPAREN
                                { $<y_attributes>$.attr_flags = ASTP_TRANSMIT_AS;
                                  $<y_attributes>$.bounds = NULL;
                                  ASTP_transmit_as_type = $<y_type>3;
                                }
    |   IDENTIFIER      /* Not an attribute, so give an error */
        {
                char *identifier;       /* place to receive the identifier text */
                NAMETABLE_id_to_string ($<y_id>1, &identifier);
                log_error (yylineno, NIDL_UNKNOWNATTR, identifier);
                $<y_attributes>$.attr_flags = 0;
                $<y_attributes>$.bounds = NULL;
        }
    ;


/********************************************************************/
/*                                                                  */
/*          Compiletime Integer expression evaluation               */
/*                                                                  */
/********************************************************************/
expression: conditional_expression
        {$<y_exp>$ = $<y_exp>1;}
   ;

conditional_expression:
        logical_OR_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    logical_OR_expression QUESTION expression COLON conditional_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            ASTP_validate_integer(&$<y_exp>5);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer ? $<y_exp>3.val.integer : $<y_exp>5.val.integer;
        }
   ;

logical_OR_expression:
        logical_AND_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    logical_OR_expression BARBAR logical_AND_expression
        {
           ASTP_validate_integer(&$<y_exp>1);
           ASTP_validate_integer(&$<y_exp>3);
           $<y_exp>$.type = AST_int_const_k;
           $<y_exp>$.val.integer = $<y_exp>1.val.integer || $<y_exp>3.val.integer;
        }
   ;

logical_AND_expression:
        inclusive_OR_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    logical_AND_expression AMPAMP inclusive_OR_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>2.val.integer && $<y_exp>3.val.integer;
        }
   ;

inclusive_OR_expression:
        exclusive_OR_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    inclusive_OR_expression BAR exclusive_OR_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer | $<y_exp>3.val.integer;
        }
   ;

exclusive_OR_expression:
        AND_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    exclusive_OR_expression CARET AND_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer ^ $<y_exp>3.val.integer;
        }
   ;

AND_expression:
        equality_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    AND_expression AMP equality_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer & $<y_exp>3.val.integer;
        }
   ;

equality_expression:
        relational_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    equality_expression EQUALEQUAL relational_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer == $<y_exp>3.val.integer;
        }
   |    equality_expression NOTEQUAL relational_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer != $<y_exp>3.val.integer;
        }
   ;

relational_expression:
        shift_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    relational_expression LANGLE shift_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer < $<y_exp>3.val.integer;
        }
   |    relational_expression RANGLE shift_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer > $<y_exp>3.val.integer;
        }
   |    relational_expression LESSEQUAL shift_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer <= $<y_exp>3.val.integer;
        }
   |    relational_expression GREATEREQUAL shift_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer >= $<y_exp>3.val.integer;
        }
   ;

shift_expression:
        additive_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    shift_expression LANGLEANGLE additive_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer << $<y_exp>3.val.integer;
        }
   |    shift_expression RANGLEANGLE additive_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer >> $<y_exp>3.val.integer;
        }
   ;

additive_expression:
        multiplicative_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    additive_expression PLUS multiplicative_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer + $<y_exp>3.val.integer;
            if (($<y_exp>$.val.integer < $<y_exp>1.val.integer) &&
                ($<y_exp>$.val.integer < $<y_exp>3.val.integer))
                log_error (yylineno, NIDL_INTOVERFLOW, KEYWORDS_lookup_text(LONG_KW));
        }
   |    additive_expression MINUS multiplicative_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer - $<y_exp>3.val.integer;
        }
   ;

multiplicative_expression:
        cast_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    multiplicative_expression STAR cast_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>1.val.integer * $<y_exp>3.val.integer;
            if (($<y_exp>$.val.integer < $<y_exp>1.val.integer) &&
                ($<y_exp>$.val.integer < $<y_exp>3.val.integer))
                log_error (yylineno, NIDL_INTOVERFLOW, KEYWORDS_lookup_text(LONG_KW));
        }
   |    multiplicative_expression SLASH cast_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            if ($<y_exp>3.val.integer != 0)
                $<y_exp>$.val.integer = $<y_exp>1.val.integer / $<y_exp>3.val.integer;
            else
                log_error (yylineno, NIDL_INTDIVBY0);
        }
   |    multiplicative_expression PERCENT cast_expression
        {
            ASTP_validate_integer(&$<y_exp>1);
            ASTP_validate_integer(&$<y_exp>3);
            $<y_exp>$.type = AST_int_const_k;
            if ($<y_exp>3.val.integer != 0)
                $<y_exp>$.val.integer = $<y_exp>1.val.integer % $<y_exp>3.val.integer;
            else
                log_error (yylineno, NIDL_INTDIVBY0);
        }
   ;

cast_expression: unary_expression
        {$<y_exp>$ = $<y_exp>1;}
    ;

unary_expression:
        primary_expression
        {$<y_exp>$ = $<y_exp>1;}
   |    PLUS primary_expression
        {
            ASTP_validate_integer(&$<y_exp>2);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_exp>2.val.integer;
        }
   |    MINUS primary_expression
        {
            ASTP_validate_integer(&$<y_exp>2);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = -$<y_exp>2.val.integer;
        }
   |    TILDE primary_expression
        {
            ASTP_validate_integer(&$<y_exp>2);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = ~$<y_exp>2.val.integer;
        }
   |    NOT primary_expression
        {
            ASTP_validate_integer(&$<y_exp>2);
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = !$<y_exp>2.val.integer;
        }
   ;

primary_expression:
        LPAREN expression RPAREN
        { $<y_exp>$ = $<y_exp>2; }
    |   INTEGER_NUMERIC
        {
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = $<y_ival>1;
        }
    |   CHAR
        {
            $<y_exp>$.type = AST_nil_const_k;
            $<y_exp>$.val.other = AST_char_constant ($<y_char>1) ;
        }
    |   IDENTIFIER
        {
            $<y_exp>$.type = AST_nil_const_k;
            $<y_exp>$.val.other  = AST_named_constant($<y_id>1) ;
        }
    |   STRING
        {
            $<y_exp>$.type = AST_nil_const_k;
            $<y_exp>$.val.other = AST_string_constant ($<y_string>1) ;
        }
    |   NULL_KW
        {
            $<y_exp>$.type = AST_nil_const_k;
            $<y_exp>$.val.other = AST_null_constant() ;
        }

    |   TRUE_KW
        {
            $<y_exp>$.type = AST_nil_const_k;
            $<y_exp>$.val.other = AST_boolean_constant(true) ;
        }

    |   FALSE_KW
        {
            $<y_exp>$.type = AST_nil_const_k;
            $<y_exp>$.val.other = AST_boolean_constant(false) ;
        }
   |    FLOAT_NUMERIC
        {
            $<y_exp>$.type = AST_int_const_k;
            $<y_exp>$.val.integer = 0;
            log_error(yylineno, NIDL_FLOATCONSTNOSUP);
        }
   ;

%%
