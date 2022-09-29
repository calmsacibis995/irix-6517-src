/*
 *  @OSF_COPYRIGHT@
 *  COPYRIGHT NOTICE
 *  Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 *  ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 *  src directory for the full copyright text.
 */

/*
 * HISTORY
 * $Log: acf.y,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.3.5  1993/01/03  21:37:29  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:31:49  bbelch]
 *
 * Revision 1.1.3.4  1992/12/23  18:42:12  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:58:47  zeliff]
 * 
 * Revision 1.1.3.3  1992/12/03  13:12:22  hinxman
 * 	Fix OT CR 6269 - multiple instances of "implicit_handle" or "represent_as
 * 		are errors
 * 	[1992/12/03  13:00:02  hinxman]
 * 
 * Revision 1.1.3.2  1992/09/29  20:40:27  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:34:30  weisman]
 * 
 * $EndLog$
 */

/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
** (c) Copyright 1991, 1992 
** Siemens-Nixdorf Information Systems, Burlington, MA, USA
** All Rights Reserved
** 
**
**  NAME:
**
**      acf.y
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  "yacc" source file for Attribute Configuration File (ACF) parser.
**
**  VERSION: DCE 1.0
**
*/

/*******************************
 *  yacc declarations section  *
 *******************************/

%{
/*----------------------*
 *  yacc included code  *
 *----------------------*/

/* Declarations in this section are copied from yacc source to y_tab.c. */

#include <nidl.h>               /* IDL compiler-wide defs */
#include <acf.h>                /* ACF include file - keep first! */

#include <ast.h>                /* Abstract Syntax Tree defs */
#include <astp.h>               /* Import AST processing routine defs */
#include <command.h>            /* Command line defs */
#include <message.h>            /* Error message defs */
#include <nidlmsg.h>            /* Error message IDs */
#include <files.h>
#include <propagat.h>

extern AST_interface_n_t *the_interface;    /* Ptr to AST interface node */
extern boolean ASTP_parsing_main_idl;       /* True when parsing main IDL */

typedef union                   /* Attributes bitmask */
{
    struct
    {
        unsigned auto_handle    : 1;
        unsigned code           : 1;
        unsigned comm_status    : 1;
        unsigned enable_allocate: 1;
        unsigned explicit_handle: 1;
        unsigned fault_status   : 1;
        unsigned heap           : 1;
        unsigned implicit_handle: 1;
        unsigned in_line        : 1;
        unsigned nocode         : 1;
        unsigned out_of_line    : 1;
        unsigned represent_as   : 1;
    }   bit;
    long    mask;
}   acf_attrib_t;

typedef struct acf_param_t      /* ACF parameter info structure */
{
    struct acf_param_t *next;                   /* Forward link */
    acf_attrib_t    parameter_attr;             /* Parameter attributes */
    NAMETABLE_id_t  param_id;                   /* Parameter name */
}   acf_param_t;


static acf_attrib_t interface_attr,     /* Interface attributes */
                    type_attr,          /* Type attributes */
                    operation_attr,     /* Operation attributes */
                    parameter_attr;     /* Parameter attributes */

static char     *interface_name,        /* Interface name */
                *impl_name,             /* Implicit handle name */
                *type_name,             /* Current type name */
                *repr_type_name,        /* Current represent_as type */
                *operation_name;        /* Current operation name */

static boolean  named_type;             /* True if parsed type is named type */

static AST_include_n_t  *include_list,  /* List of AST include nodes */
                        *include_p;     /* Ptr to a created include node */

static acf_param_t  *parameter_list,        /* Param list for curr. operation */
                    *parameter_free_list;   /* List of available acf_param_t */
static boolean      parameter_attr_list;    /* True if param attrs specified */

static boolean      *cmd_opt;       /* Array of command option flags */
static void         **cmd_val;      /* Array of command option values */
%}

/*------------------------------------*
 *  yylval and yyval type definition  *
 *------------------------------------*/

/*
 * Union declaration defines the possible datatypes of the external variables
 * yylval and yyval.
 */

%union
{
    NAMETABLE_id_t  y_id;       /* Identifier */
    STRTAB_str_t    y_string;   /* Text string */
#ifdef SNI_SVR4
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
#endif /*SNI_SVR4*/
}

/*-----------------------------*
 *  Tokens used by the parser  *
 *-----------------------------*/

/* Keywords */

%token AUTO_HANDLE_KW
%token CODE_KW
%token COMM_STATUS_KW
%token ENABLE_ALLOCATE_KW
%token EXPLICIT_HANDLE_KW
%token FAULT_STATUS_KW
%token HANDLE_T_KW
%token HEAP_KW
%token IMPLICIT_HANDLE_KW
%token INCLUDE_KW
%token INTERFACE_KW
%token IN_LINE_KW
%token NOCODE_KW
%token OUT_OF_LINE_KW
%token REPRESENT_AS_KW
%token TYPEDEF_KW

/* Punctuation */

%token COMMA
%token LBRACE
%token LBRACKET
%token LPAREN
%token RBRACE
%token RBRACKET
%token RPAREN
%token SEMI
%token UNKNOWN  /* Unrecognized by LEX */

/* Tokens setting yylval */

%token <y_id>       IDENTIFIER
%token <y_string>   STRING

/*-----------------------------*
 *  Starting state for parser  *
 *-----------------------------*/

%start acf_interface

%%

/************************
 *  yacc rules section  *
 ************************/

acf_interface:
        acf_interface_header acf_interface_body
    ;

acf_interface_header:
        acf_interface_attr_list INTERFACE_KW acf_interface_name
    {
        char            *ast_int_name;  /* Interface name in AST node */
        NAMETABLE_id_t  type_id;        /* Nametable id of impl_handle type */
        NAMETABLE_id_t  impl_name_id;   /* Nametable id of impl_handle var */
        AST_type_n_t    *type_p;        /* Ptr to implicit_handle type node */

#ifdef DUMPERS
        if (cmd_opt[opt_dump_acf])
            dump_attributes("ACF interface", interface_name, &interface_attr);
#endif

        /* Store source information. */
        if (the_interface->fe_info != NULL)
        {
            the_interface->fe_info->acf_file = error_file_name_id;
            the_interface->fe_info->acf_source_line = acf_yylineno;
        }

        /*
         *  Interface attributes are saved for main and imported interfaces.
         *  the_interface = pointer to main or imported interface node
         *
         *  Make sure that the interface name in the ACF agrees with the
         *  interface name in the main IDL file.  Then set the parsed
         *  attributes in the interface node.
         *
         *  interface_attr = bitmask of interface attributes parsed.
         *  interface_name = ACF interface name parsed.
         */

        NAMETABLE_id_to_string(the_interface->name, &ast_int_name);

        if (strcmp(interface_name, ast_int_name) != 0)
        {
            char    *acf_int_name;      /* Ptr to permanent copy */
            NAMETABLE_id_t name_id;     /* Handle on permanent copy */
            char    *file_name;         /* Related file name */

            name_id = NAMETABLE_add_id(interface_name);
            NAMETABLE_id_to_string(name_id, &acf_int_name);

            STRTAB_str_to_string(the_interface->fe_info->file, &file_name);

            acf_error(NIDL_INTNAMDIF, acf_int_name, ast_int_name);
            acf_error(NIDL_NAMEDECLAT, ast_int_name, file_name,
                      the_interface->fe_info->source_line);
        }
        else
        {
            if (interface_attr.bit.code)
                AST_SET_CODE(the_interface);
            if (interface_attr.bit.nocode)
                AST_SET_NO_CODE(the_interface);
            if (interface_attr.bit.explicit_handle)
                AST_SET_EXPLICIT_HANDLE(the_interface);
            if (interface_attr.bit.in_line)
                AST_SET_IN_LINE(the_interface);
            if (interface_attr.bit.out_of_line)
                AST_SET_OUT_OF_LINE(the_interface);
            if (interface_attr.bit.auto_handle)
                AST_SET_AUTO_HANDLE(the_interface);

            if (interface_attr.bit.implicit_handle)
            {
                /* Store the [implicit_handle] variable name in nametbl. */
                impl_name_id = NAMETABLE_add_id(impl_name);

                /*
                 * Store the type and name information in the AST.  If a
                 * named type, determine whether it is an IDL-defined type
                 * and process accordingly.  Otherwise the type is handle_t.
                 */
                if (named_type)
                {
                    if (lookup_type(type_name, FALSE, &type_id, &type_p))
                    {
                        the_interface->implicit_handle_name = impl_name_id;
                        the_interface->implicit_handle_type = type_p;
                        the_interface->implicit_handle_type_name = type_p->name;
                        if (AST_HANDLE_SET(type_p))
                            AST_SET_IMPLICIT_HANDLE_G(the_interface);
                    }
                    else    /* A user-defined type, not defined in IDL */
                    {
                        /* Store the user type name in nametbl. */
                        the_interface->implicit_handle_type_name
                            = NAMETABLE_add_id(type_name);

                        the_interface->implicit_handle_name = impl_name_id;
                        the_interface->implicit_handle_type = NULL;
                        AST_SET_IMPLICIT_HANDLE_G(the_interface);
                    }
                }
                else
                {
                    the_interface->implicit_handle_name = impl_name_id;
                    the_interface->implicit_handle_type = ASTP_handle_ptr;
                }
            }
        }

        interface_name = NULL;
        type_name = NULL;
        impl_name = NULL;
        interface_attr.mask = 0;        /* Reset attribute mask */
    }
    ;

acf_interface_attr_list:
        LBRACKET acf_interface_attrs RBRACKET
    |   /* Nothing */
    ;

acf_interface_attrs:
        acf_interface_attr
    |   acf_interface_attrs COMMA acf_interface_attr
    ;

acf_interface_attr:
        acf_code_attr
    {
        if (interface_attr.bit.code)
            log_warning(yylineno, NIDL_MULATTRDEF);
        interface_attr.bit.code = TRUE;
    }
    |   acf_nocode_attr
    {
        if (interface_attr.bit.nocode)
            log_warning(yylineno, NIDL_MULATTRDEF);
        interface_attr.bit.nocode = TRUE;
    }
    |   acf_explicit_handle_attr
    {
        if (interface_attr.bit.explicit_handle)
            log_warning(yylineno, NIDL_MULATTRDEF);
        interface_attr.bit.explicit_handle = TRUE;
    }
    |   acf_inline_attr
    {
        if (interface_attr.bit.in_line)
            log_warning(yylineno, NIDL_MULATTRDEF);
        interface_attr.bit.in_line = TRUE;
    }
    |   acf_outofline_attr
    {
        if (interface_attr.bit.out_of_line)
            log_warning(yylineno, NIDL_MULATTRDEF);
        interface_attr.bit.out_of_line = TRUE;
    }
    |   acf_implicit_handle_attr
    {
        if (interface_attr.bit.implicit_handle)
            log_error(yylineno, NIDL_ATTRUSEMULT);
        interface_attr.bit.implicit_handle = TRUE;
    }
    |   acf_auto_handle_attr
    {
        if (interface_attr.bit.auto_handle)
            log_warning(yylineno, NIDL_MULATTRDEF);
        interface_attr.bit.auto_handle = TRUE;
    }
    ;

acf_implicit_handle_attr:
        IMPLICIT_HANDLE_KW LPAREN acf_implicit_handle RPAREN
    ;

acf_implicit_handle:
        acf_impl_type acf_impl_name
    ;

acf_impl_type:
        acf_handle_type
    {
        named_type = FALSE;
    }
    |   IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &type_name);
        named_type = TRUE;
    }
    ;

acf_handle_type:
        HANDLE_T_KW
    ;

acf_impl_name:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &impl_name);
    }
    ;

acf_interface_name:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &interface_name);
    }
    ;

acf_interface_body:
        LBRACE acf_body_elements RBRACE
    |   LBRACE RBRACE
    |   error
        { log_error(yylineno, NIDL_SYNTAXERR); }
    |   error RBRACE
        { log_error(yylineno, NIDL_SYNTAXERR); }
    ;

acf_body_elements:
        acf_body_element
    |   acf_body_elements acf_body_element
    ;

acf_body_element:
        acf_include SEMI
    |   acf_type_declaration SEMI
    |   acf_operation_declaration SEMI
    |   error SEMI
        { log_error(yylineno, NIDL_SYNTAXERR); }
    ;

acf_include:
        INCLUDE_KW acf_include_list
        {
        if (ASTP_parsing_main_idl)
            the_interface->includes = (AST_include_n_t *)
                AST_concat_element((ASTP_node_t *)the_interface->includes,
                                   (ASTP_node_t *)include_list);
        include_list = NULL;
        }
    |   INCLUDE_KW error
        { log_error(yylineno, NIDL_SYNTAXERR); }
    ;

acf_include_list:
        acf_include_name
    |   acf_include_list COMMA acf_include_name
    ;

acf_include_name:
        STRING
    {
        if (ASTP_parsing_main_idl)
        {
            char            *parsed_include_file;
            char            include_type[PATH_MAX];
            char            include_file[PATH_MAX];
            STRTAB_str_t    include_file_id;

            STRTAB_str_to_string($<y_string>1, &parsed_include_file);

            /*
             * Log warning if include name contains a file extension.
             * For this version, we always tack on a ".h" extension.
             * In subsequent versions, it will be tied to the -lang selection.
             */
            FILE_parse(parsed_include_file, (char *)NULL, (char *)NULL,
                       include_type);
            if (include_type[0] != '\0')
                acf_error(NIDL_INCLUDEXT);

            FILE_form_filespec(parsed_include_file, (char *)NULL, ".h",
                               (char *)NULL, include_file);

            /* Create an include node. */
            include_file_id = STRTAB_add_string(include_file);
            include_p = AST_include_node(include_file_id);

            /* Store source information. */
            if (include_p->fe_info != NULL)
            {
                include_p->fe_info->acf_file = error_file_name_id;
                include_p->fe_info->acf_source_line = acf_yylineno;
            }

            include_list = (AST_include_n_t *)
                AST_concat_element((ASTP_node_t *)include_list,
                                   (ASTP_node_t *)include_p);
        }
    }
    ;

acf_type_declaration:
        TYPEDEF_KW error
        { log_error(yylineno, NIDL_SYNTAXERR); }
    |   TYPEDEF_KW acf_type_attr_list acf_named_type_list
    {
        type_attr.mask = 0;             /* Reset attribute mask */
        repr_type_name = NULL;          /* Reset represent_as type name */
    }
    ;

acf_named_type_list:
        acf_named_type
    {
        type_name = NULL;               /* Reset type name */
    }
    |   acf_named_type_list COMMA acf_named_type
    {
        type_name = NULL;               /* Reset type name */
    }
    ;

acf_named_type:
        IDENTIFIER
    {
        NAMETABLE_id_t  type_id;        /* Nametable id of type name */
        NAMETABLE_id_t  repr_type_id;   /* Nametable id of represent_as type */
        AST_type_n_t    *type_p;        /* Ptr to AST type node */
        AST_type_n_t    *repr_type_p;   /* Ptr to AST represent_as type node */

        NAMETABLE_id_to_string($<y_id>1, &type_name);

#ifdef DUMPERS
        if (cmd_opt[opt_dump_acf])
            dump_attributes("ACF type", type_name, &type_attr);
#endif

        /*
         *  Lookup the type_name parsed and verify that it is a valid type
         *  node.  Then set the parsed attributes in the type node.
         *
         *  type_attr = bitmask of type attributes parsed.
         *  type_name = name of type_t node to look up.
         *  [repr_type_name] = name of represent_as type.
         */

        if (lookup_type(type_name, TRUE, &type_id, &type_p))
        {
            /* Store source information. */
            if (type_p->fe_info != NULL)
            {
                type_p->fe_info->acf_file = error_file_name_id;
                type_p->fe_info->acf_source_line = acf_yylineno;
            }

            if (type_attr.bit.heap)
                PROP_set_type_attr(type_p,AST_HEAP);
            if (type_attr.bit.in_line)
                PROP_set_type_attr(type_p,AST_IN_LINE);
            if ((type_attr.bit.out_of_line) &&
                (type_p->kind != AST_pointer_k) &&
                (type_p->xmit_as_type == NULL))
                PROP_set_type_attr(type_p,AST_OUT_OF_LINE);
            if (type_attr.bit.represent_as)
            {
                char    *file_name;             /* Related file name */
                char    *stored_repr_type_name; /* Perm copy of type name */
                AST_type_n_t *parent_type_p;    /* Type with a represent_as */
                char    *parent_name;           /* Name of parent type */

                repr_type_id = NAMETABLE_add_id(repr_type_name);

                /*
                 * Report error if the represent_as type also has a represent_
                 * as clause, i.e. represent_as types cannot nest.
                 */
                if (lookup_rep_as_name(the_interface->ra_types, repr_type_id,
                                       &parent_type_p, &stored_repr_type_name))
                {
                    NAMETABLE_id_to_string(parent_type_p->name, &parent_name);
                    STRTAB_str_to_string(parent_type_p->fe_info->acf_file,
                                         &file_name);

                    acf_error(NIDL_REPASNEST);
                    acf_error(NIDL_TYPEREPAS, parent_name,
                              stored_repr_type_name);
                    acf_error(NIDL_NAMEDECLAT, parent_name, file_name,
                              parent_type_p->fe_info->acf_source_line);
                }

                /*
                 * If the type node already has a represent_as name,
                 * this one must duplicate that same name.
                 */
                if (type_p->rep_as_type != NULL)
                {
                    NAMETABLE_id_to_string(type_p->rep_as_type->type_name,
                                           &stored_repr_type_name);

                    if (strcmp(stored_repr_type_name, repr_type_name) != 0)
                    {
                        char    *new_repr_type_name;/* Ptr to permanent copy */
                        NAMETABLE_id_t name_id;     /* Handle on perm copy */

                        name_id = NAMETABLE_add_id(repr_type_name);
                        NAMETABLE_id_to_string(name_id, &new_repr_type_name);

                        STRTAB_str_to_string(
                            type_p->rep_as_type->fe_info->acf_file, &file_name);

                        acf_error(NIDL_CONFREPRTYPE, new_repr_type_name,
                                  stored_repr_type_name);
                        acf_error(NIDL_NAMEDECLAT, stored_repr_type_name,
                                  file_name,
                                  type_p->rep_as_type->fe_info->acf_source_line);
                    }
                }
                else
                {
                    AST_type_p_n_t  *typep_p;   /* Used to link type nodes */
                    AST_rep_as_n_t  *repas_p;   /* Ptr to represent_as node */
                    AST_type_p_n_t *assoc_type_p; /* Pointer to clone list */

                    /* Add rep_as type name and build rep_as AST node. */

                    repas_p = type_p->rep_as_type = AST_represent_as_node
                                                        (repr_type_id);
                    /* Store source information. */
                    if (repas_p->fe_info != NULL)
                    {
                        repas_p->fe_info->acf_file = error_file_name_id;
                        repas_p->fe_info->acf_source_line = acf_yylineno;
                    }


                    /* Check for associate def-as-tag node */
                    if (type_p->fe_info->tag_ptr != NULL)
                        type_p->fe_info->tag_ptr->rep_as_type = type_p->rep_as_type;

#if 0
                     /*
                      * TBS -- Need to set rep_as_type on clones too?
                      */
                     if (type_p->fe_info->fe_type_id == fe_clone_info)
                     {
                          /* loop through all clones of the original and set the flags also */
                          for (assoc_type_p = type_p->fe_info->type_specific.clone;
                               assoc_type_p != NULL;
                               assoc_type_p = assoc_type_p->next)
                          {
                              assoc_type_p->type->rep_as_type = type_p->rep_as_type;
                          }
                     }
#endif

                    /* Link type node into list of represent_as types. */

                    typep_p = AST_type_ptr_node();
                    typep_p->type = type_p;

                    the_interface->ra_types =
                        (AST_type_p_n_t *)AST_concat_element(
                                (ASTP_node_t *)the_interface->ra_types,
                                (ASTP_node_t *)typep_p);
                }
            }
        }
    }
    ;

acf_type_attr_list:
        LBRACKET acf_rest_of_attr_list
    |   /* Nothing */
    ;

acf_rest_of_attr_list:
        acf_type_attrs RBRACKET
    |   error SEMI
        {
        log_error(yylineno, NIDL_MISSONATTR);
        }
    |   error RBRACKET
        {
        log_error(yylineno, NIDL_ERRINATTR);
        }
    ;

acf_type_attrs:
        acf_type_attr
    |   acf_type_attrs COMMA acf_type_attr
    ;

acf_type_attr:
        acf_represent_attr
    {
        if (type_attr.bit.represent_as)
            log_error(yylineno, NIDL_ATTRUSEMULT);
        type_attr.bit.represent_as = TRUE;
    }
    |   acf_heap_attr
    {
        if (type_attr.bit.heap)
            log_warning(yylineno, NIDL_MULATTRDEF);
        type_attr.bit.heap = TRUE;
    }
    |   acf_inline_attr
    {
        if (type_attr.bit.in_line)
            log_warning(yylineno, NIDL_MULATTRDEF);
        type_attr.bit.in_line = TRUE;
    }
    |   acf_outofline_attr
    {
        if (type_attr.bit.out_of_line)
            log_warning(yylineno, NIDL_MULATTRDEF);
        type_attr.bit.out_of_line = TRUE;
    }
    ;

acf_represent_attr:
        REPRESENT_AS_KW LPAREN acf_repr_type RPAREN
    ;

acf_repr_type:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &repr_type_name);
    }
    ;

acf_operation_declaration:
        acf_op_attr_list acf_operations
    {
        operation_attr.mask = 0;                /* Reset attribute mask */
    }
    ;

acf_operations:
        acf_operation
    |   acf_operations COMMA acf_operation
    ;

acf_operation:
        IDENTIFIER LPAREN acf_parameter_list RPAREN
    {
        acf_param_t         *p;         /* Ptr to local parameter structure */
        NAMETABLE_id_t      op_id;      /* Nametable id of operation name */
        NAMETABLE_id_t      param_id;   /* Nametable id of parameter name */
        AST_operation_n_t   *op_p;      /* Ptr to AST operation node */
        AST_parameter_n_t   *param_p;   /* Ptr to AST parameter node */
        boolean             log_error;  /* TRUE => error if name not found */
        char                *param_name;/* character string of param id */

        NAMETABLE_id_to_string($<y_id>1, &operation_name);
#ifdef DUMPERS
        if (cmd_opt[opt_dump_acf])
            dump_attributes("ACF operation", operation_name, &operation_attr);
#endif

        /*
         *  Operation and parameter attributes are ignored for imported
         *  interfaces.  Operations and parameters within imported interfaces
         *  are not put in the AST.
         */
        if (ASTP_parsing_main_idl)
        {
            /*
             *  Lookup the operation_name parsed and verify that it is a valid
             *  operation node.  Then set the parsed attributes in the operation
             *  node.  For each parameter_name that was parsed for this
             *  operation, chase the parameter list off the AST operation node
             *  to verify that it is a valid parameter for that operation.
             *  Then set the parsed attributes for that parameter into the
             *  relevant parameter node.
             *
             *  operation_attr = bitmask of operation attributes parsed.
             *  operation_name = name of routine_t node to look up.
             *  parameter_list = linked list of parameter information.
             */

            if (lookup_operation(operation_name, TRUE, &op_id, &op_p))
            {
                /* Store source information. */
                if (op_p->fe_info != NULL)
                {
                    op_p->fe_info->acf_file = error_file_name_id;
                    op_p->fe_info->acf_source_line = acf_yylineno;
                }

                if (operation_attr.bit.comm_status)
                {
                    /*
                     * Assume the AST Builder always builds a result param,
                     * even for void operations.
                     */
                    AST_SET_COMM_STATUS(op_p->result);
                }
                if (operation_attr.bit.fault_status)
                    AST_SET_FAULT_STATUS(op_p->result);

                if (operation_attr.bit.code)
                    AST_SET_CODE(op_p);
                if (operation_attr.bit.nocode)
                    AST_SET_NO_CODE(op_p);
                if (operation_attr.bit.enable_allocate)
                    AST_SET_ENABLE_ALLOCATE(op_p);
                if (operation_attr.bit.explicit_handle)
                    AST_SET_EXPLICIT_HANDLE(op_p);

                for (p = parameter_list ; p != NULL ; p = p->next)
                {
                    /*
                     * If the [in_line], [out_of_line], or [heap] attributes
                     * are present, the referenced parameter must be defined in
                     * the IDL.  If just [comm_status] and/or [fault_status] is
                     * present, it needn't be IDL-defined.
                     */
                    if (!p->parameter_attr.bit.heap
                        &&  !p->parameter_attr.bit.in_line
                        &&  !p->parameter_attr.bit.out_of_line
                        &&  (p->parameter_attr.bit.comm_status
                             || p->parameter_attr.bit.fault_status))
                        log_error = FALSE;
                    else
                        log_error = TRUE;

                    NAMETABLE_id_to_string(p->param_id, &param_name);
                    if (lookup_parameter(op_p, param_name, log_error,
                                         &param_id, &param_p))
                    {
                        /* Store source information. */
                        if (param_p->fe_info != NULL)
                        {
                            param_p->fe_info->acf_file = error_file_name_id;
                            param_p->fe_info->acf_source_line = acf_yylineno;
                        }

                        if (p->parameter_attr.bit.comm_status)
                            AST_SET_COMM_STATUS(param_p);
                        if (p->parameter_attr.bit.fault_status)
                            AST_SET_FAULT_STATUS(param_p);
                        if (p->parameter_attr.bit.heap)
                            AST_SET_HEAP(param_p);
                        if (p->parameter_attr.bit.in_line)
                            AST_SET_IN_LINE(param_p);
                        /*
                         * We parse the [out_of_line] parameter attribute,
                         * but disallow it.
                         */
                        if (p->parameter_attr.bit.out_of_line)
                            acf_error(NIDL_INVOOLPRM);
                    }
                    else if (log_error == FALSE)
                    {
                        /*
                         * Lookup failed, but OK since the parameter only has
                         * attribute(s) that specify an additional parameter.
                         * Append a parameter to the operation parameter list.
                         */
                        NAMETABLE_id_to_string(p->param_id, &param_name);
                        append_parameter(op_p, param_name, &p->parameter_attr);
                    }
                }
            }
        }

        free_param_list(&parameter_list);       /* Free parameter list */

        operation_name = NULL;
    }
    ;

acf_op_attr_list:
        LBRACKET acf_op_attrs RBRACKET
    |   /* Nothing */
    ;

acf_op_attrs:
        acf_op_attr
    |   acf_op_attrs COMMA acf_op_attr
    ;

acf_op_attr:
        acf_commstat_attr
    {
        if (operation_attr.bit.comm_status)
            log_warning(yylineno, NIDL_MULATTRDEF);
        operation_attr.bit.comm_status = TRUE;
    }
    |   acf_code_attr
    {
        if (operation_attr.bit.code)
            log_warning(yylineno, NIDL_MULATTRDEF);
        operation_attr.bit.code = TRUE;
    }
    |   acf_nocode_attr
    {
        if (operation_attr.bit.nocode)
            log_warning(yylineno, NIDL_MULATTRDEF);
        operation_attr.bit.nocode = TRUE;
    }
    |   acf_enable_allocate_attr
    {
        if (operation_attr.bit.enable_allocate)
            log_warning(yylineno, NIDL_MULATTRDEF);
        operation_attr.bit.enable_allocate = TRUE;
    }
    |   acf_explicit_handle_attr
    {
        if (operation_attr.bit.explicit_handle)
            log_warning(yylineno, NIDL_MULATTRDEF);
        operation_attr.bit.explicit_handle = TRUE;
    }
    |   acf_faultstat_attr
    {
        if (operation_attr.bit.fault_status)
            log_warning(yylineno, NIDL_MULATTRDEF);
        operation_attr.bit.fault_status = TRUE;
    }
    ;

acf_parameter_list:
        acf_parameters
    |   /* Nothing */
    ;

acf_parameters:
        acf_parameter
    |   acf_parameters COMMA acf_parameter
    ;

acf_parameter:
        acf_param_attr_list IDENTIFIER
    {
#ifdef DUMPERS
        if (cmd_opt[opt_dump_acf])
        {
            char *param_name;
            NAMETABLE_id_to_string($<y_id>2, &param_name);
            dump_attributes("ACF parameter", param_name, &parameter_attr);
        }
#endif

        if (parameter_attr_list)        /* If there were param attributes: */
        {
            acf_param_t *p;             /* Pointer to parameter record */

            /*
             * Allocate and initialize a parameter record.
             */
            p = alloc_param();
            p->parameter_attr = parameter_attr;
            p->param_id = $<y_id>2;

            /*
             * Add to end of parameter list.
             */
            add_param_to_list(p, &parameter_list);

            parameter_attr.mask = 0;
        }
    }
    ;

acf_param_attr_list:
        LBRACKET acf_param_attrs RBRACKET
    {
        parameter_attr_list = TRUE;     /* Flag that we have param attributes */
    }
    |   /* Nothing */
    {
        parameter_attr_list = FALSE;
    }
    ;

acf_param_attrs:
        acf_param_attr
    |   acf_param_attrs COMMA acf_param_attr
    ;

acf_param_attr:
        acf_commstat_attr
    {
        if (parameter_attr.bit.comm_status)
            log_warning(yylineno, NIDL_MULATTRDEF);
        parameter_attr.bit.comm_status = TRUE;
    }
    |   acf_faultstat_attr
    {
        if (parameter_attr.bit.fault_status)
            log_warning(yylineno, NIDL_MULATTRDEF);
        parameter_attr.bit.fault_status = TRUE;
    }
    |   acf_heap_attr
    {
        if (parameter_attr.bit.heap)
            log_warning(yylineno, NIDL_MULATTRDEF);
        parameter_attr.bit.heap = TRUE;
    }
    |   acf_inline_attr
    {
        if (parameter_attr.bit.in_line)
            log_warning(yylineno, NIDL_MULATTRDEF);
        parameter_attr.bit.in_line = TRUE;
    }
    |   acf_outofline_attr
    {
        if (parameter_attr.bit.out_of_line)
            log_warning(yylineno, NIDL_MULATTRDEF);
        parameter_attr.bit.out_of_line = TRUE;
    }
    ;

acf_auto_handle_attr:   AUTO_HANDLE_KW;
acf_code_attr:          CODE_KW;
acf_nocode_attr:        NOCODE_KW;
acf_enable_allocate_attr: ENABLE_ALLOCATE_KW;
acf_explicit_handle_attr: EXPLICIT_HANDLE_KW;
acf_heap_attr:          HEAP_KW;
acf_inline_attr:        IN_LINE_KW;
acf_outofline_attr:     OUT_OF_LINE_KW;
acf_commstat_attr:      COMM_STATUS_KW;
acf_faultstat_attr:     FAULT_STATUS_KW;

%%

/***************************
 *  yacc programs section  *
 ***************************/

/*
 *  a c f _ i n i t
 *
 *  Function:   Called before ACF parsing to initialize variables.
 *
 */

void acf_init
#ifdef PROTO
(
    boolean     *cmd_opt_arr,   /* [in] Array of command option flags */
    void        **cmd_val_arr,  /* [in] Array of command option values */
    char        *acf_file       /* [in] ACF file name */
)
#else
(cmd_opt_arr, cmd_val_arr, acf_file)
    boolean     *cmd_opt_arr;   /* [in] Array of command option flags */
    void        **cmd_val_arr;  /* [in] Array of command option values */
    char        *acf_file;      /* [in] ACF file name */
#endif

{
    /* Save passed command array and interface node addrs in static storage. */
    cmd_opt = cmd_opt_arr;
    cmd_val = cmd_val_arr;

    /* Set global (STRTAB_str_t error_file_name_id) for error processing. */
    set_name_for_errors(acf_file);

    interface_attr.mask = 0;
    type_attr.mask      = 0;
    operation_attr.mask = 0;
    parameter_attr.mask = 0;

    interface_name      = NULL;
    type_name           = NULL;
    repr_type_name      = NULL;
    operation_name      = NULL;

    include_list        = NULL;

    parameter_list      = NULL;
    parameter_free_list = NULL;
}

/*
 *  a c f _ c l e a n u p
 *
 *  Function:   Called after ACF parsing to free allocated memory.
 *
 */

#ifdef PROTO
void acf_cleanup(void)
#else
void acf_cleanup()
#endif

{
    acf_param_t *p, *q;     /* Ptrs to parameter record */

    p = parameter_free_list;

    while (p != NULL)
    {
        q = p;
        p = p->next;
        FREE(q);
    }
}

/*
**  a c f _ e r r o r
**
**  Issues an error message, and bumps the error count.
**
**  Note:       This function is not prototyped since the way we use it allows
**              it to be called with 1 to 6 arguments.
*/

static void acf_error(msgid, arg1, arg2, arg3, arg4, arg5)
    long    msgid;              /* [in] Message id */
    char    *arg1;              /* [in] 0 to 5 arguments to fill in message */
    char    *arg2;              /*      directives */
    char    *arg3;
    char    *arg4;
    char    *arg5;

{
    log_error(acf_yylineno, msgid, arg1, arg2, arg3, arg4, arg5);
}

/*
**  l o o k u p _ t y p e
**
**  Looks up a name in the nametable, and if it is bound to a valid type
**  node, returns the address of the type node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_type
#ifdef PROTO
(
    char            *type_name, /* [in] Name to look up */
    boolean         log_error,  /* [in] TRUE => log error if name not found */
    NAMETABLE_id_t  *type_id,   /*[out] Nametable id of type name */
    AST_type_n_t    **type_ptr  /*[out] Ptr to AST type node */
)
#else
(type_name, log_error, type_id, type_ptr)
    char            *type_name; /* [in] Name to look up */
    boolean         log_error;  /* [in] TRUE => log error if name not found */
    NAMETABLE_id_t  *type_id;   /*[out] Nametable id of type name */
    AST_type_n_t    **type_ptr; /*[out] Ptr to AST type node */
#endif

{
    AST_type_n_t    *type_p;    /* Ptr to node bound to looked up name */
    char            *perm_type_name;    /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id;            /* Handle on permanent copy */

    *type_id = NAMETABLE_lookup_id(type_name);

    if (*type_id != NAMETABLE_NIL_ID)
    {
        type_p = (AST_type_n_t *)NAMETABLE_lookup_binding(*type_id);

        if (type_p != NULL && type_p->fe_info->node_kind == fe_type_n_k)
        {
            *type_ptr = type_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        name_id = NAMETABLE_add_id(type_name);
        NAMETABLE_id_to_string(name_id, &perm_type_name);
        acf_error(NIDL_TYPNOTDEF, perm_type_name);
    }

    *type_ptr = NULL;
    return FALSE;
}

/*
**  l o o k u p _ o p e r a t i o n
**
**  Looks up a name in the nametable, and if it is bound to a valid operation
**  node, returns the address of the operation node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_operation
#ifdef PROTO
(
    char            *op_name,   /* [in] Name to look up */
    boolean         log_error,  /* [in] TRUE => log error if name not found */
    NAMETABLE_id_t  *op_id,     /*[out] Nametable id of operation name */
    AST_operation_n_t **op_ptr  /*[out] Ptr to AST operation node */
)
#else
(op_name, log_error, op_id, op_ptr)
    char            *op_name;   /* [in] Name to look up */
    boolean         log_error;  /* [in] TRUE => log error if name not found */
    NAMETABLE_id_t  *op_id;     /*[out] Nametable id of operation name */
    AST_operation_n_t **op_ptr; /*[out] Ptr to AST operation node */
#endif

{
    AST_operation_n_t   *op_p;  /* Ptr to node bound to looked up name */
    char            *perm_op_name;      /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id;            /* Handle on permanent copy */

    *op_id = NAMETABLE_lookup_id(op_name);

    if (*op_id != NAMETABLE_NIL_ID)
    {
        op_p = (AST_operation_n_t *)NAMETABLE_lookup_binding(*op_id);

        if (op_p != NULL && op_p->fe_info->node_kind == fe_operation_n_k)
        {
            *op_ptr = op_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        name_id = NAMETABLE_add_id(op_name);
        NAMETABLE_id_to_string(name_id, &perm_op_name);
        acf_error(NIDL_OPNOTDEF, perm_op_name);
    }

    *op_ptr = NULL;
    return FALSE;
}

/*
**  l o o k u p _ p a r a m e t e r
**
**  Searches an operation node's parameter list for the parameter name passed.
**  If found, returns the address of the parameter node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_parameter
#ifdef PROTO
(
    AST_operation_n_t   *op_p,          /* [in] Ptr to AST operation node */
    char                *param_name,    /* [in] Parameter name to look up */
    boolean             log_error,      /* [in] TRUE=> log error if not found */
    NAMETABLE_id_t      *param_id,      /*[out] Nametable id of param name */
    AST_parameter_n_t   **param_ptr     /*[out] Ptr to AST parameter node */
)
#else
(op_p, param_name, log_error, param_id, param_ptr)
    AST_operation_n_t   *op_p;          /* [in] Ptr to AST operation node */
    char                *param_name;    /* [in] Parameter name to look up */
    boolean             log_error;      /* [in] TRUE=> log error if not found */
    NAMETABLE_id_t      *param_id;      /*[out] Nametable id of param name */
    AST_parameter_n_t   **param_ptr;    /*[out] Ptr to AST parameter node */
#endif

{
    AST_parameter_n_t   *param_p;       /* Ptr to operation parameter node */
    char                *op_param_name; /* Name of an operation parameter */
    char                *op_name;       /* Operation name */
    char            *perm_param_name;   /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id;            /* Handle on permanent copy */

    for (param_p = op_p->parameters ; param_p != NULL ; param_p = param_p->next)
    {
        NAMETABLE_id_to_string(param_p->name, &op_param_name);

        if (strcmp(param_name, op_param_name) == 0)
        {
            *param_id   = param_p->name;
            *param_ptr  = param_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        char    *file_name;     /* Related file name */

        NAMETABLE_id_to_string(op_p->name, &op_name);
        name_id = NAMETABLE_add_id(param_name);
        NAMETABLE_id_to_string(name_id, &perm_param_name);

        STRTAB_str_to_string(op_p->fe_info->file, &file_name);

        acf_error(NIDL_PRMNOTDEF, perm_param_name, op_name);
        acf_error(NIDL_NAMEDECLAT, op_name, file_name,
                  op_p->fe_info->source_line);
    }

    return FALSE;
}

/*
**  l o o k u p _ r e p _ a s _ n a m e
**
**  Scans a list of type nodes that have represent_as types for a match with
**  the type name given by the parameter repr_name_id.  If so, returns the
**  address of the found type node and a pointer to the associated
**  represent_as type name.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_rep_as_name
#ifdef PROTO
(
    AST_type_p_n_t  *typep_p,           /* [in] Listhead of type ptr nodes */
    NAMETABLE_id_t  repr_name_id,       /* [in] represent_as name to look up */
    AST_type_n_t    **ret_type_p,       /*[out] Type node if found */
    char            **ret_type_name     /*[out] Type name if found */
)
#else
(typep_p, repr_name_id, ret_type_p, ret_type_name)
    AST_type_p_n_t  *typep_p;           /* [in] Listhead of type ptr nodes */
    NAMETABLE_id_t  repr_name_id;       /* [in] represent_as name to look up */
    AST_type_n_t    **ret_type_p;       /*[out] Type node if found */
    char            **ret_type_name;    /*[out] Type name if found */
#endif

{
    AST_type_n_t    *type_p;            /* Ptr to a type node */

    for ( ; typep_p != NULL ; typep_p = typep_p->next )
    {
        type_p = typep_p->type;
        if (type_p->name == repr_name_id)
        {
            *ret_type_p = type_p;
            NAMETABLE_id_to_string(type_p->rep_as_type->type_name,
                                   ret_type_name);
            return TRUE;
        }
    }

    return FALSE;
}

/*
 *  a c f _ a l l o c _ p a r a m
 *
 *  Function:   Allocates an acf_param_t, either from the free list or heap.
 *
 *  Returns:    Address of acf_param_t
 *
 *  Globals:    parameter_free_list - listhead for free list
 *
 *  Side Effects:   Exits program if unable to allocate memory.
 */

#ifdef PROTO
static acf_param_t *alloc_param(void)
#else
static acf_param_t *alloc_param()
#endif

{
    acf_param_t *p;     /* Ptr to parameter record */

    if (parameter_free_list != NULL)
    {
        p = parameter_free_list;
        parameter_free_list = parameter_free_list->next;
    }
    else
    {
        p = (acf_param_t *)MALLOC(sizeof(acf_param_t));
        p->next                 = NULL;
        p->parameter_attr.mask  = 0;
        p->param_id             = NAMETABLE_NIL_ID;
    }

    return p;
}

/*
 *  a c f _ f r e e _ p a r a m
 *
 *  Function:   Frees an acf_param_t by reinitilizing it and returning it to
 *              the head of the free list.
 *
 *  Input:      p - Pointer to acf_param_t record
 *
 *  Globals:    parameter_free_list - listhead for free list
 */

static void free_param
#ifdef PROTO
(
    acf_param_t *p              /* [in] Pointer to acf_param_t record */
)
#else
(p)
    acf_param_t *p;             /* [in] Pointer to acf_param_t record */
#endif

{
    p->parameter_attr.mask  = 0;
    p->param_id             = NAMETABLE_NIL_ID;

    p->next                 = parameter_free_list;
    parameter_free_list     = p;
}


/*
 *  a c f _ f r e e _ p a r a m _ l i s t
 *
 *  Function:   Frees a list of acf_param_t records.
 *
 *  Input:      list - Address of list pointer
 *
 *  Output:     list pointer = NULL
 */

static void free_param_list
#ifdef PROTO
(
    acf_param_t **list          /* [in] Address of list pointer */
)
#else
(list)
    acf_param_t **list;         /* [in] Address of list pointer */
#endif

{
    acf_param_t *p, *q;     /* Ptrs to parameter record */

    p = *list;

    while (p != NULL)
    {
        q = p;
        p = p->next;
        free_param(q);
    }

    *list = NULL;            /* List now empty */
}

/*
 *  a d d _ p a r a m _ t o _ l i s t
 *
 *  Function:   Add a acf_param_t record to the end of a list.
 *
 *  Inputs:     p - Pointer to parameter record
 *              list - Address of list pointer
 *
 *  Outputs:    List is modified.
 */

void add_param_to_list
#ifdef PROTO
(
    acf_param_t *p,             /* [in] Pointer to parameter record */
    acf_param_t **list          /* [in] Address of list pointer */
)
#else
(p, list)
    acf_param_t *p;             /* [in] Pointer to parameter record */
    acf_param_t **list;         /* [in] Address of list pointer */
#endif

{
    acf_param_t *q;         /* Ptr to parameter record */

    if (*list == NULL)      /* If list empty */
        *list = p;          /* then list now points at param */
    else
    {
        for (q = *list ; q->next != NULL ; q = q->next)
            ;
        q->next = p;        /* else last record in list now points at param */
    }

    p->next = NULL;         /* Param is now last in list */
}

/*
**  a p p e n d _ p a r a m e t e r
**
**  Appends a parameter to an operation's parameter list.
*/

static void append_parameter
#ifdef PROTO
(
    AST_operation_n_t   *op_p,          /* [in] Ptr to AST operation node */
    char                *param_name,    /* [in] Parameter name */
    acf_attrib_t        *param_attr     /* [in] Parameter attributes */
)
#else
(op_p, param_name, param_attr)
    AST_operation_n_t   *op_p;          /* [in] Ptr to AST operation node */
    char                *param_name;    /* [in] Parameter name */
    acf_attrib_t        *param_attr;    /* [in] Parameter attributes */
#endif

{
    NAMETABLE_id_t      new_param_id;   /* Nametable id of new parameter name */
    AST_parameter_n_t   *new_param_p;   /* Ptr to new parameter node */
    AST_type_n_t        *new_type_p;    /* Ptr to new parameter type node */
    AST_pointer_n_t     *new_ptr_p;     /* Ptr to new pointer node */
    NAMETABLE_id_t      status_id;      /* Nametable id of status_t */
    AST_type_n_t        *status_type_p; /* Type node bound to status_t name */
    AST_parameter_n_t   *param_p;       /* Ptr to operation parameter node */

    /* Look up error_status_t type. */
    status_id = NAMETABLE_add_id("error_status_t");
    status_type_p = (AST_type_n_t *)NAMETABLE_lookup_binding(status_id);
    if (status_type_p == NULL)
    {
        acf_error(NIDL_ERRSTATDEF);
        return;
    }

    /*
     * Have to create an '[out] error_status_t *param_name' parameter
     * that has the specified parameter attributes.
     */
    new_param_id = NAMETABLE_add_id(param_name);
    new_param_p = AST_parameter_node(new_param_id);
    new_type_p  = AST_type_node(AST_pointer_k);
    new_ptr_p   = AST_pointer_node(status_type_p);

    new_type_p->type_structure.pointer = new_ptr_p;
    AST_SET_REF(new_type_p);

    new_param_p->name = new_param_id;
    new_param_p->type = new_type_p;
    new_param_p->uplink = op_p;
    if (param_attr->bit.comm_status)
        AST_SET_ADD_COMM_STATUS(new_param_p);
    if (param_attr->bit.fault_status)
        AST_SET_ADD_FAULT_STATUS(new_param_p);
    AST_SET_OUT(new_param_p);
    AST_SET_REF(new_param_p);

    param_p = op_p->parameters;
    if (param_p == NULL)
    {
        /* Was null param list, now has one param. */
        op_p->parameters = new_param_p;
    }
    else if (param_p->last == NULL)
    {
        /* Was one param, now have two params. */
        param_p->next = new_param_p;
        param_p->last = new_param_p;
    }
    else
    {
        /* Was more than one param, now have one more. */
        param_p->last->next = new_param_p;
        param_p->last = new_param_p;
    }
}

#ifdef DUMPERS
/*
 *  d u m p _ a t t r i b u t e s
 *
 *  Function:   Prints list of attributes parsed for a particular node type
 *
 *  Inputs:     header_text - Initial text before node name and attributes
 *              node_name   - Name of interface, type, operation, or parameter
 *              node_attr_p - Address of node attributes structure
 *
 *  Globals:    repr_type_name  - represent_as type name, used if bit is set
 */

static void dump_attributes
#ifdef PROTO
(
    char            *header_text,       /* [in] Initial output text */
    char            *node_name,         /* [in] Name of tree node */
    acf_attrib_t    *node_attr_p        /* [in] Node attributes ptr */
)
#else
(header_text, node_name, node_attr_p)
    char            *header_text;       /* [in] Initial output text */
    char            *node_name;         /* [in] Name of tree node */
    acf_attrib_t    *node_attr_p;       /* [in] Node attributes ptr */
#endif

#define MAX_ATTR_TEXT   256

{
    char            attr_text[MAX_ATTR_TEXT];   /* Buf for formatting attrs */
    int             pos;                /* Position in buffer */
    acf_attrib_t    node_attr;          /* Node attributes */

    node_attr = *node_attr_p;

    printf("%s %s", header_text, node_name);

    if (node_attr.mask == 0)
        printf("\n");
    else
    {
        printf(" attributes: ");
        strcpy(attr_text, "[");

        if (node_attr.bit.auto_handle)
            strcat(attr_text, "auto_handle, ");
        if (node_attr.bit.code)
            strcat(attr_text, "code, ");
        if (node_attr.bit.nocode)
            strcat(attr_text, "nocode, ");
        if (node_attr.bit.comm_status)
            strcat(attr_text, "comm_status, ");
        if (node_attr.bit.enable_allocate)
            strcat(attr_text, "enable_allocate, ");
        if (node_attr.bit.explicit_handle)
            strcat(attr_text, "explicit_handle, ");
        if (node_attr.bit.fault_status)
            strcat(attr_text, "fault_status, ");
        if (node_attr.bit.heap)
            strcat(attr_text, "heap, ");
        if (node_attr.bit.implicit_handle)
            strcat(attr_text, "implicit_handle, ");
        if (node_attr.bit.in_line)
            strcat(attr_text, "in_line, ");
        if (node_attr.bit.out_of_line)
            strcat(attr_text, "out_of_line, ");
        if (node_attr.bit.represent_as)
        {
            strcat(attr_text, "represent_as(");
            strcat(attr_text, repr_type_name);
            strcat(attr_text, "), ");
        }

        /* Overwrite trailing ", " with "]" */

        pos = strlen(attr_text) - strlen(", ");
        attr_text[pos] = ']';
        attr_text[pos+1] = '\0';

        printf("%s\n", attr_text);
    }
}
#endif
