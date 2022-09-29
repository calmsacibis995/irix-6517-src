/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: astp_com.c,v $
 * Revision 1.2  1994/02/21 19:01:30  jwag
 * add BB slot numbers; start support for interface slot numbers; lots of ksgen changes
 *
 * Revision 1.1  1993/08/31  06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.4.3  1993/01/03  21:37:43  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:32:07  bbelch]
 *
 * Revision 1.1.4.2  1992/12/23  18:42:42  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  00:59:06  zeliff]
 * 
 * Revision 1.1.2.3  1992/04/06  19:48:36  harrow
 * 	Change message from warning to error for override of pointer attributes.
 * 	[1992/04/01  19:16:31  harrow]
 * 
 * Revision 1.1.2.2  1992/03/24  14:07:45  harrow
 * 	Enums that are [v1_enums] are 4 ndr bytes not 2, so change
 * 	the ndr_size field when this attribute is applied.
 * 
 * 	Prevent override of pointer attributes as spec says.
 * 
 * 	Propagate V1 attributes to type node for fields, parameters and arms.
 * 	This enables use of [v1_*] attributes on anonymous types.
 * 	[1992/03/18  20:44:59  harrow]
 * 
 * Revision 1.1  1992/01/19  03:01:26  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989,1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME
**
**      ASTP_COM.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Common routines shared between the AST builder modules.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <nametbl.h>
#include <errors.h>
#include <astp.h>
#include <nidlmsg.h>
#include <y_tab.h>
#include <checker.h>
#include <message.h>
#include <backend.h>

/*
 * Prototypes
 */
char *KEYWORDS_lookup_text (
#ifdef PROTO
    long    token
#endif
);

static AST_type_n_t *AST_propagate_typedef (
#ifdef PROTO
    AST_type_n_t *type_node_ptr,
    ASTP_declarator_n_t *declarator_ptr,
    ASTP_attributes_t   *attributes
#endif
);

static AST_array_n_t *AST_propagate_array_type (
#ifdef PROTO
    AST_type_n_t *type_node_ptr,
    AST_type_n_t *element_type_node_ptr,
    ASTP_declarator_op_n_t *declarator_op_ptr
#endif
);

static void ASTP_verify_non_anonymous (
#ifdef PROTO
    AST_type_n_t *type,
    ASTP_declarator_n_t *declarator_ptr
#endif
);



/*
 *  A S T P _ a d d _ n a m e _ b i n d i n g
 *  ==========================================
 *
 * This routine add a binding between the name and
 * node specified.  If the name is already declared
 * the NIDL_NAMEALRDEC error message is displayed.
 */

void ASTP_add_name_binding
#ifdef PROTO
(
    NAMETABLE_id_t      name,
    char        *AST_node
)
#else
(name, AST_node)
    NAMETABLE_id_t      name;
    char        *AST_node;
#endif
{
    ASTP_node_t *binding;
    int issue_error;

    /*
     * Check to see if there is already a constant binding to the name.
     * If there is, then we can't make this declaration because constants
     * are implemented as #define which are global.
     */
    binding = (ASTP_node_t *)NAMETABLE_lookup_binding(name);
    if ((binding != NULL) && (binding->fe_info->node_kind == fe_constant_n_k))
        issue_error = true;
    else
        /* Add the name to nametable and bind it to the specified node */
        issue_error = !NAMETABLE_add_binding (name, AST_node);

    /*
     * If the name binding failed, output the error message.
     */
    if (issue_error)
    {
        char *identifier;       /* place to receive the identifier text pointer */
        char *filename;         /* place to receive the filename text pointer */
        NAMETABLE_id_to_string (name, &identifier);

        /* Select message and file information as present */
        if ((binding->fe_info != (fe_info_t *)NULL) &&
            (binding->fe_info->source_line != 0) &&
            (binding->fe_info->file != STRTAB_NULL_STR))
        {
            STRTAB_str_to_string(binding->fe_info->file, &filename);
            log_error (yylineno, NIDL_NAMEPREVDECLAT, identifier,
                    filename, binding->fe_info->source_line);
        }
        else
            log_error (yylineno, NIDL_NAMEALRDEC, identifier);
    }

    return;
}

/*-----------------------------------------------------------------------*/
/*
 *  A S T P _ v a l i d a t e _ f o r w a r d _ r e f
 *  =================================================
 *
 * This routine verifies that the specified type
 * is not a forward reference to an incomplete struct/union
 * by tag.  This is to prevent the generation of nonstandard C code
 * that may not compile.  Forward references to incomplete types
 * are only allowed in contexts where allocation size information is
 * not needed (e.g. a pointer to the type, or in a typedef).  If
 * the type passed to this routine is not yet completely defined, then
 * an error will be issued.
 */

void ASTP_validate_forward_ref
#ifdef PROTO
(
    AST_type_n_t *type
)
#else
(type)
    AST_type_n_t *type;
#endif
{
    /*
     * Check that the base type is not an incomplete struct/union
     */
    if (AST_DEF_AS_TAG_SET(type) &&
        ((type->kind == AST_disc_union_k) ||
         (type->kind == AST_structure_k)) &&
        (type->type_structure.structure == NULL))
    {
        char *identifier;
        NAMETABLE_id_to_string(type->fe_info->tag_name, &identifier);
        log_error(yylineno, NIDL_DEFNOTCOMP, identifier);
    }
}

/*-----------------------------------------------------------------------*/

/*
 * A S T P _ c h a s e _ p t r _ t o _ k i n d
 * ===========================================
 *
 * Chase a pointer type until a specified
 * "kind" type is found.
 *
 * Inputs:
 *  type_node   The starting type node which SHOULD
 *              be a pointer kind.
 *
 *  kind        The "kind" of type node we're
 *              searching for.
 *
 * Outputs:
 *
 *
 * Function Value:
 *  Pointer to the type node which contains
 *  the kind.  If search failed, NULL is returned.
 *  If successful, returned type node's fe_info
 *  pointer count if set to the number of pointer
 *  nodes traversed.
 */
AST_type_n_t *ASTP_chase_ptr_to_kind
#ifdef PROTO
(
    AST_type_n_t *type_node,
    AST_type_k_t kind
)
#else
(type_node, kind)
    AST_type_n_t *type_node;
    AST_type_k_t kind;
#endif
{
    AST_type_n_t *tp;
    short int    pointer_count;

    for (tp= type_node, pointer_count = 0;
        (tp->kind != kind ) && (tp->kind == AST_pointer_k);
        tp = tp->type_structure.pointer->pointee_type, pointer_count++)
    {
        /* Chase pointer until we find the "kind" type node */
    }

    if (tp->kind != kind)
    {
        tp = NULL;
    }
    else
    {
        tp->fe_info->pointer_count = pointer_count;
    }

    return tp;
}

/*-------------------------------------------------------------------*/

/*
 * A S T P _ c h a s e _ p t r _ t o _ t y p e
 * ===========================================
 *
 * Chase a pointer type until a non-pointer is
 * is found, i.e. until the base type is found.
 *
 * Inputs:
 *  type_node   The starting type node which SHOULD
 *              be a pointer kind.
 *
 * Outputs:
 *
 *
 * Function Value:
 *  Pointer to the type node which lives
 *  at the end of the chain of pointers.
 *  The returned type node's fe_info pointer_count
 *  field is set to the number of pointer nodes
 *  traversed.
 */
AST_type_n_t *ASTP_chase_ptr_to_type
#ifdef PROTO
(
    AST_type_n_t *type_node
)
#else
(type_node)
    AST_type_n_t *type_node;
#endif
{
    AST_type_n_t *tp;
    short int    pointer_count;

    for (tp= type_node, pointer_count = 0;
        (tp->kind == AST_pointer_k);
        tp = tp->type_structure.pointer->pointee_type, pointer_count++)
    {
        /* Chase pointer until we find the base type node */
    }

    tp->fe_info->pointer_count = pointer_count;

    return tp;
}

/*-------------------------------------------------------------------*/

/*
 *  A S T _ c l o n e _ c o n s t a n t
 *  ===================================
 *
 *  Clone a constant node.
 *
 */

AST_constant_n_t *AST_clone_constant
#ifdef PROTO
(
    AST_constant_n_t *constant_node_p
)
#else
(constant_node_p)
    AST_constant_n_t *constant_node_p;
#endif
{
    AST_constant_n_t *new_const_node_p;

    /*
     * Create constant node of the appropriate kind and
     * replicate value  and fe_info constant kind information
     */

    new_const_node_p = AST_constant_node (constant_node_p->kind);

    switch (new_const_node_p->kind)
    {
        case AST_boolean_const_k:
            new_const_node_p->value.boolean_val =
                    constant_node_p->value.boolean_val;
            break;

        case AST_char_const_k:
            new_const_node_p->value.char_val =
                    constant_node_p->value.char_val;
            break;

        case AST_int_const_k:
            new_const_node_p->value.int_val =
                    constant_node_p->value.int_val;
            break;

        case AST_string_const_k:
            new_const_node_p->value.string_val =
                    constant_node_p->value.string_val;
            break;

        default:
            break;
    }

    /* Get the constant kind from the front end info */
    new_const_node_p->fe_info->fe_type_id =
            constant_node_p->fe_info->fe_type_id;
    new_const_node_p->fe_info->type_specific.const_kind =
            constant_node_p->fe_info->type_specific.const_kind;

    return new_const_node_p;
}

/*----------------------------------------------------------------------*/

/*
 *  A S T _ c o n c a t _ e l e m e n t
 *
 *  A generic list handling routine that concatenates a element (which
 *  may be itself a singly-linked list) to a singly-linked list of the
 *  same type.
 *
 *  Assumptions:
 *      All element types have the next and last pointers as the first
 *      fields in the structure.
 *
 *  Function Value:
 *      The updated list header.
 */

ASTP_node_t *AST_concat_element
#ifdef PROTO
(
    ASTP_node_t *list_head,
    ASTP_node_t *element
)
#else
(list_head, element)
    ASTP_node_t *list_head;
    ASTP_node_t *element;
#endif
{
    /* Check for null element.  Grammer will sometimes call us this way */
    if (element == NULL)
    {
        return list_head;
    }

    /* List is empty.  Link in new element or list */
    if (list_head == NULL)
    {
        list_head = element;
        return list_head;
    }

    /* Only one item on list. Link in new element or list */
    if (list_head->last == NULL)
    {
        if (element->last == NULL)
        {
            list_head->last = element;
            list_head->next = element;
            return list_head;
        }

        list_head->last = element->last;
        list_head->next = element;
        return list_head;
    }

    /* Multiple items exist on list. */
    list_head->last->next = element;
    if (element->last != NULL)
    {
        list_head->last = element->last;
    }
    else
    {
        list_head->last = element;
    }

    return list_head;

}

/*---------------------------------------------------------------------*/

/*
 *  A S T P _ l o o k u p _ b i n d i n g
 *  ==========================================
 *
 * This routine looks up a binding of a particular
 * AST node.
 *
 * Inputs:
 *      name          Name identifier to lookup.
 *      node_kind     The node kind the binding should be.
 *      noforward_ref True, if forward references are not
 *                    expected.
 * Outputs:
 *      None.
 *
 * Function Value:
 *      If successful, the node bound to the name of the
 *      particular type, otherwise NULL is returned.
 */

ASTP_node_t *ASTP_lookup_binding
#ifdef PROTO
(
    NAMETABLE_id_t      name,
    fe_node_k_t         node_kind,
    boolean             noforward_ref

)
#else
(name, node_kind, noforward_ref)
    NAMETABLE_id_t      name;
    fe_node_k_t         node_kind;
    boolean             noforward_ref;
#endif
{
    ASTP_node_t     *bound_node;
    char            *identifier;

    /*
     * Lookup binding
     */
    bound_node = (ASTP_node_t *) NAMETABLE_lookup_binding(name);

    /*
     * If not found tell them if the caller wants
     * us to.  This is for parameters and field
     * array bound attributes which can be
     * forward referenced.
     */
    if ((bound_node == NULL) && noforward_ref)
    {
        NAMETABLE_id_to_string (name, &identifier);
        log_error(yylineno, NIDL_NAMENOTFND, identifier) ;
    }

    /*
     * If binding node of wrong type, return NULL, and tell them.
     */
    if ((bound_node != NULL) &&
        (bound_node->fe_info->node_kind != node_kind))
    {
        /* If forward references are not allowed, output error message now */
        if (noforward_ref)
        {
            NAMETABLE_id_to_string (name, &identifier);

            switch (node_kind)
            {
                case fe_constant_n_k:
                    log_error(yylineno, NIDL_NAMENOTCONST, identifier) ;
                    break;

                case fe_type_n_k:
                    log_error(yylineno, NIDL_NAMENOTTYPE, identifier) ;
                    break;

                case fe_field_n_k:
                    log_error(yylineno, NIDL_NAMENOTFIELD, identifier) ;
                    break;

                case fe_parameter_n_k:
                    log_error(yylineno, NIDL_NAMENOTPARAM, identifier) ;
                    break;

                default:
                    break;
            }


            /* State where the name was previously declared, if known */
            if ((bound_node->fe_info->source_line != 0) &&
                (bound_node->fe_info->file != STRTAB_NULL_STR))
            {
                char *filename;         /* place to receive the filename text pointer */
                STRTAB_str_to_string(bound_node->fe_info->file, &filename);
                log_error (yylineno, NIDL_NAMEPREVDECLAT, identifier,
                        filename, bound_node->fe_info->source_line);
            }
        }

        /* Return Null, so caller doesn't use wrong node type that we found */
        bound_node = NULL;
    }

    return bound_node;
}

/*-----------------------------------------------------------------------*/


/*
 *  A S T _ p o i n t e r _ n o d e
 *  ===============================
 *
 *  Create and initialize a pointer node
 */

AST_pointer_n_t * AST_pointer_node
#ifdef PROTO
(
    AST_type_n_t * pointee
)
#else
(pointee)
    AST_type_n_t * pointee;
#endif
{
    AST_pointer_n_t * pointer_node_ptr;

    pointer_node_ptr = (AST_pointer_n_t *) MALLOC (sizeof (AST_pointer_n_t));
    pointer_node_ptr->pointee_type = pointee;
    ASTP_set_fe_info((ASTP_node_t *)pointer_node_ptr,fe_pointer_n_k);
    return pointer_node_ptr;
}

/*---------------------------------------------------------------------*/

/*
 *  A S T _ d e c l a r a t o r s _ t o _ t y p e s
 *  ===============================================
 *
 * For each of the linked declarator nodes, allocate a
 * type node and collapse the declarator node to the
 * type node, forming a list of type pointer nodes.
 *
 * INPUTS:
 *  type_ptr        Pointer to type node.
 *  declarators_ptr Pointer to list of declarator nodes.
 *
 * OUTPUTS:
 *  None.
 *
 * FUNCTION VALUE:
 *  Pointer to a list of type node pointers.
 *
 * NOTES:
 *  The original type node is used for the first declarator node.
 *
 */

AST_type_p_n_t *AST_declarators_to_types
#ifdef PROTO
(
    AST_type_n_t *type_ptr,
    ASTP_declarator_n_t *declarators_ptr,
    ASTP_attributes_t   *attributes
)
#else
(type_ptr, declarators_ptr, attributes)
    AST_type_n_t *type_ptr;
    ASTP_declarator_n_t *declarators_ptr;
    ASTP_attributes_t   *attributes;
#endif
{
    AST_type_p_n_t
        *type_p_list = NULL,        /* List of type pointer nodes */
        *type_p_ptr;                /* Created for each type node */

    ASTP_declarator_n_t
        gen_declarator,             /* Generated declarator for use as base */
        *base_dp = NULL,            /* Base declarator used for the base    */
                                    /* type name and generates other types. */
        *dp;                        /* For traversing through declarators   */

    AST_type_n_t
        *base_type = type_ptr;      /* Base type used to generate other all */
                                    /* other types derived in this typedef. */

    /*
     * Look for a simple declarator to use when generating the base type node.
     * All other types are generated from the base type.
     */
    for (dp = declarators_ptr;
            dp != (ASTP_declarator_n_t *) NULL;
            dp = dp->next)
    {
        if (dp->next_op == NULL)
        {
            /*
             * Save the base declarator so we can skip over it during our second
             * pass over the declarators.
             */
            base_dp = dp;

            /*
             * If the original type is a struct/union with tag and this is a simple
             * declarator (not a pointer, array or function), then we must do some
             * special processing to set the name in the type node for tag
             * references to the type name.  The tag name is only stored in the
             * structure_node pointed to by the type node.
             */
            if ((type_ptr->fe_info->tag_ptr != NULL) &&
                (type_ptr->fe_info->tag_ptr->name == NAMETABLE_NIL_ID))
                type_ptr->fe_info->tag_ptr->name = base_dp->name;

            /*
             * All that's needed in this loop so exit loop
             */
            break;
        }
    }

    /*
     *  If we did not find a base declarator to use and the base type is not
     *  named, then generate a named base declarator from which to build all
     *  of the more complicated types.
     */
    if ((base_dp == NULL) &&                      /* No simple declarator */
        (base_type->name == NAMETABLE_NIL_ID) &&  /* Base type is anonymous */
        ((base_type->kind == AST_structure_k) ||  /* a struct or union */
         (base_type->kind == AST_disc_union_k)))
    {
        base_dp = &gen_declarator;
        base_dp->name = AST_generate_name(the_interface,"");
        base_dp->next = NULL;
        base_dp->last = NULL;
        base_dp->next_op = NULL;
        base_dp->last_op = NULL;
        base_dp->fe_info = NULL;
    }

    /*
     * create a type for the simple type declared in this typedef and
     * use it to generate the types for other declarators in this typedef.
     */
    if (base_dp != NULL)
    {
        ASTP_attributes_t local_attributes;  /* copy of attributes that     */
        local_attributes = *attributes;      /* we can manipulate           */

        /* Create a type pointer node */
        type_p_ptr = AST_type_ptr_node();

        /*
         * Propagate declarator to the type node, merge in any attributes, and
         * Link type node to type pointer node, and form type pointer list.
         */
        base_type = AST_propagate_typedef(type_ptr, base_dp, &local_attributes);
        type_p_ptr->type = base_type;
        type_p_list = (AST_type_p_n_t *)AST_concat_element (
                        (ASTP_node_t *)type_p_list, (ASTP_node_t *)type_p_ptr);


        /*
         *  If we are processing a generated declarator, ignore all pointer
         *  attributes
         */
        if (base_dp == &gen_declarator)
              ASTP_CLR_ATTR(&local_attributes,ASTP_IGNORE|ASTP_PTR|ASTP_REF|
                          ASTP_UNIQUE|ASTP_STRING);

        /* Set the type attributes to the new type node */
        AST_set_type_attrs(type_p_ptr->type, &local_attributes);

        /*
         * If there is an associated DEF_AS_TAG type node, then make sure to
         * set on that type node as well.
         */
        if (type_p_ptr->type->fe_info->tag_ptr != NULL)
            AST_set_type_attrs(type_p_ptr->type->fe_info->tag_ptr, &local_attributes);
     }


    /*
     * For each of the declarators create a new type as described by the
     * declarators.  These types will all be dervied from the base type.
     */
    for (dp = declarators_ptr;
            dp != (ASTP_declarator_n_t *) NULL;
            dp = dp->next)
    {
        /* Skip over the base declarator because it's been processed */
        if (dp == base_dp) continue;

        /* Create a type pointer node */
        type_p_ptr = AST_type_ptr_node();

        /*
         * Propagate declarator to the type node, merge in any attributes, and
         * Link type node to type pointer node, and form type pointer list.
         */
        type_p_ptr->type = AST_propagate_typedef(base_type, dp, attributes);
        type_p_list =  (AST_type_p_n_t *)AST_concat_element (
                        (ASTP_node_t *)type_p_list, (ASTP_node_t *)type_p_ptr);

        /* Set the type attributes to the new type node */
        AST_set_type_attrs(type_p_ptr->type, attributes);

        /*
         * If there is an associated DEF_AS_TAG type node, then make sure to
         * set on that type node as well.
         */
        if (type_p_ptr->type->fe_info->tag_ptr != NULL)
            AST_set_type_attrs(type_p_ptr->type->fe_info->tag_ptr, attributes);
    }


    /* Free temporary declarators list */
    ASTP_free_declarators(declarators_ptr);


    return type_p_list;
}

/*---------------------------------------------------------------------*/




/*
 *
 * A S T _ p r o p a g a t e _ a r r a y _ t y p e
 * ===============================================
 *
 * Create the AST array node and propagate
 * the array index information pointed to by
 * the declarator node to the array node
 *
 * Note, that in the declarator node, the
 * array index is a linked list of AST Private
 * array index nodes.  With the AST array node,
 * it is implemented as a vector.
 */

static AST_array_n_t *AST_propagate_array_type
#ifdef PROTO
(
    AST_type_n_t            *type_node_ptr,
    AST_type_n_t            *element_type_node_ptr,
    ASTP_declarator_op_n_t  *declarator_op_ptr
)
#else
(type_node_ptr, element_type_node_ptr, declarator_op_ptr)
    AST_type_n_t            *type_node_ptr;
    AST_type_n_t            *element_type_node_ptr;
    ASTP_declarator_op_n_t *declarator_op_ptr;
#endif
{
    AST_array_n_t        *array_node_ptr;
    ASTP_array_index_n_t *index_ptr;
    AST_array_index_n_t  *array_index_node;
    unsigned short       index_count;
    boolean              is_conformant = FALSE;


    /* Create the array node, creating linking in a element type node */
    array_node_ptr = AST_array_node(element_type_node_ptr);

    /* Propagate the array indice information */

    /* Count the number of dimensions */
    for (index_ptr = declarator_op_ptr->op_info.indices, index_count=0;
         index_ptr != NULL;
         index_ptr = index_ptr->next, index_count++)
         {};

    array_node_ptr->index_count = index_count;

    /* Allocate (and initialize) the array index vector */
    array_node_ptr->index_vec = AST_array_index_node(index_count);

    /*
     * Fill in the index information pointed
     * to by the array node and set the FIXED flags.
     */
    for (index_ptr = declarator_op_ptr->op_info.indices,
         array_index_node = array_node_ptr->index_vec;
         index_ptr != NULL;
         index_ptr=index_ptr->next, array_index_node++)
    {
        if (index_ptr->lower_bound != NULL)
        {
            array_index_node->lower_bound = index_ptr->lower_bound;
            AST_SET_FIXED_LOWER(array_index_node);
        }
        else
        {
            is_conformant = TRUE;
        }

        if (index_ptr->upper_bound != NULL)
        {
            array_index_node->upper_bound = index_ptr->upper_bound;
            AST_SET_FIXED_UPPER(array_index_node);
        }
        else
        {
            is_conformant = TRUE;
        }

    }

    /*
     * If any dimension of the array is not fixed,
     * set the conformant flag in the type node
     * specifying an unfixed allocation size
     */
    if (is_conformant)
    {
        AST_SET_CONFORMANT(type_node_ptr);
    }


    return array_node_ptr;
}

/*---------------------------------------------------------------------*/

/*
 *
 *  A S T _ p r o p a g a t e _ t y p e _ a t t r s
 *  ===============================================
 *
 *  Check for the presence of the REF, PTR or UNIQUE attribute
 *  and set the specified flag in the parameter or
 *  field's type node if present.
 *  We need to clone the type if the new type
 *  is (build by AST_propagate_type()) is a simple declarator.
 *
 *  This processing is done here (called from
 *  propagate_types() to minimize on the cloning,
 *  since we know we just created a new anonymous
 *  type (no clone) or used the current type (clone).
 *
 *  We also propagate type attributes which are also
 *  field/parameter/arm attributes to the field/parameter/arm node.
 *  These currently are string, small, and ignore.
 */

static AST_type_n_t *AST_propagate_type_attrs
#ifdef PROTO
(
    AST_type_n_t *type_node_ptr,
    ASTP_attributes_t *attributes,
    boolean needs_clone,
    ASTP_node_t *parent_node
)
#else
(type_node_ptr, attributes, needs_clone, parent_node)
    AST_type_n_t *type_node_ptr;
    ASTP_attributes_t *attributes;
    boolean needs_clone;
    ASTP_node_t *parent_node;
#endif
{
    AST_type_n_t *return_type;          /* type node to return */
    ASTP_attr_flag_t ptr_attrs = 0;


    return_type = type_node_ptr;

    /*
     * Only one of [unique], [ptr], or [ref] can be specified.
     * If that is not the case, output an error stating that the
     * two specified are conflicting.
     */
    if (attributes != NULL)
    {
        /*
         *  If any pointer attrs are set, make sure only one of them is
         *  specified.
         */
        ptr_attrs = (attributes->attr_flags) & (ASTP_UNIQUE|ASTP_PTR|ASTP_REF);
        if ((ptr_attrs != 0) &&
            !((ptr_attrs == ASTP_REF) || (ptr_attrs == ASTP_PTR) ||
              (ptr_attrs == ASTP_UNIQUE)))
        {
            ASTP_attr_flag_t attr1 = ASTP_REF;
            ASTP_attr_flag_t attr2 = ASTP_PTR;
            if (!ASTP_TEST_ATTR(attributes,ASTP_REF)) attr1 = ASTP_UNIQUE;
            if (!ASTP_TEST_ATTR(attributes,ASTP_PTR)) attr2 = ASTP_UNIQUE;
            log_error(yylineno, NIDL_CONFLICTATTR,
                  KEYWORDS_lookup_text(AST_attribute_to_token(&attr1)),
                  KEYWORDS_lookup_text(AST_attribute_to_token(&attr2)));
        }

        /*
         * Handle V1 attributes specified on an instance, by propagating to
         * the type node where they are expected.  Since this is propagation
         * from instance to type, only do it for anonymous types.  If the
         * type is not anonymous the V1 attributes should have been set
         * in the typedef in which it is declared.  Since we don't clear
         * the attribute, an error will occur in the routine AST_set_flags.
         */
        if (return_type->name == NAMETABLE_NIL_ID)
        {
            if (ASTP_TEST_ATTR(attributes,ASTP_UNALIGN)) 
                AST_SET_UNALIGN(return_type);
            if (ASTP_TEST_ATTR(attributes,ASTP_SMALL)) 
                AST_SET_SMALL(return_type);
            if (ASTP_TEST_ATTR(attributes,ASTP_STRING0)) 
                AST_SET_STRING0(return_type);
            if (ASTP_TEST_ATTR(attributes,ASTP_V1_ENUM)) 
                AST_SET_V1_ENUM(return_type);

            /* Remove the type-attributes, as they are now on the type */
            ASTP_CLR_ATTR(attributes,ASTP_UNALIGN|ASTP_SMALL|ASTP_STRING0|ASTP_V1_ENUM);
        }
    }


    /*
     * Do type attributes for parameter nodes
     */
    if (parent_node->fe_info->node_kind == fe_parameter_n_k)
    {
        /*
         * Process explicit [ptr], [ref], or [unique]
         */
         if (ptr_attrs != 0)
         {
            /* valid for anonymous pointer or array types only */
            if (((return_type->name == NAMETABLE_NIL_ID) &&
                (return_type->kind == AST_pointer_k)) ||
                (return_type->kind == AST_array_k))
            {
            }
            else
            {
                /*
                 * Output error, PTR or UNIQUE not valid on parameter
                 * being passed by value.
                 */
                log_error(yylineno, NIDL_PRMBYREF,
                  KEYWORDS_lookup_text(AST_attribute_to_token(&ptr_attrs)));
            }

        }
        else
        {
            /*
             *  Process default.  [REF] is implied for a parameter
             *  that is an anonymous pointer or
             *  an array type without any attributes.
             */
            if (((return_type->name == NAMETABLE_NIL_ID) &&
                 (return_type->kind == AST_pointer_k)) ||
                 (return_type->kind == AST_array_k))
            {
                /*
                 *  Don't set [ref] on void* types because it is either a
                 *  context handle, or checker will issue an error on it.
                 */
                if (!((return_type->kind == AST_pointer_k) &&
                    (return_type->type_structure.pointer->pointee_type->kind == AST_void_k) ))
                        AST_SET_REF(((AST_parameter_n_t*)parent_node));
            }
            else
            {
                /*
                 *  Not anonymous array or pointer, so propagate ptr, ref, &
                 *  unique attributes from type to parameter
                 *
                 */
                if (AST_PTR_SET(return_type))
                    AST_SET_PTR((AST_parameter_n_t *)parent_node);
                if (AST_REF_SET(return_type))
                    AST_SET_REF((AST_parameter_n_t *)parent_node);
                if (AST_UNIQUE_SET(return_type))
                    AST_SET_UNIQUE((AST_parameter_n_t *)parent_node);
            }
        } /* End pointer attributes processing */


        /*
         * Propagate small, string, and string0 attributes to the parameter node
         */
         if (AST_SMALL_SET(return_type))
            AST_SET_SMALL((AST_parameter_n_t *)parent_node);

         if (AST_STRING_SET(return_type))
            AST_SET_STRING((AST_parameter_n_t *)parent_node);

         if (AST_STRING0_SET(return_type))
            AST_SET_STRING0((AST_parameter_n_t *)parent_node);

    }  /* End if parameter node */


    /*
     * Do the type attributes for field and arm nodes
     */
    if ((parent_node->fe_info->node_kind == fe_field_n_k) ||
        (parent_node->fe_info->node_kind == fe_arm_n_k))
    {
        if (ptr_attrs == 0)
        {
            /*
             * Propagate ptr, ref, or unique to the arm and field nodes.
             */
             if (AST_PTR_SET(return_type))
             {
                if (parent_node->fe_info->node_kind == fe_field_n_k)
                    AST_SET_PTR((AST_field_n_t *)parent_node);
                else
                    AST_SET_PTR((AST_arm_n_t *)parent_node);
             }
             if (AST_REF_SET(return_type))
             {
                if (parent_node->fe_info->node_kind == fe_field_n_k)
                    AST_SET_REF((AST_field_n_t *)parent_node);
                else
                    AST_SET_REF((AST_arm_n_t *)parent_node);
             }
             if (AST_UNIQUE_SET(return_type))
             {
                if (parent_node->fe_info->node_kind == fe_field_n_k)
                    AST_SET_UNIQUE((AST_field_n_t *)parent_node);
                else
                    AST_SET_UNIQUE((AST_arm_n_t *)parent_node);
             }
        }
        else /* Attributes specified, make sure pointer is anonymous */
        {
            /* Only valid for anonymous pointers */
            if (return_type->name != NAMETABLE_NIL_ID) 
            {
                if (ASTP_TEST_ATTR(attributes,ASTP_REF)) 
                    log_error(yylineno,NIDL_REFATTRPTR);
                if (ASTP_TEST_ATTR(attributes,ASTP_PTR)) 
                    log_error(yylineno,NIDL_PTRATTRPTR);
                if (ASTP_TEST_ATTR(attributes,ASTP_UNIQUE)) 
                    log_error(yylineno,NIDL_UNIQATTRPTR);
            }
        }  /* End pointer attributes processing */


        /*
         * Propagate small, string and string0 to the arm and field nodes.
         */
         if (AST_SMALL_SET(return_type))
         {
            if (parent_node->fe_info->node_kind == fe_field_n_k)
                AST_SET_SMALL((AST_field_n_t *)parent_node);
            else
                AST_SET_SMALL((AST_arm_n_t *)parent_node);
         }

         if (AST_STRING_SET(return_type))
         {
            if (parent_node->fe_info->node_kind == fe_field_n_k)
                AST_SET_STRING((AST_field_n_t *)parent_node);
            else
                AST_SET_STRING((AST_arm_n_t *)parent_node);
         }

         if (AST_STRING0_SET(return_type))
         {
            if (parent_node->fe_info->node_kind == fe_field_n_k)
                AST_SET_STRING0((AST_field_n_t *)parent_node);
            else
                AST_SET_STRING0((AST_arm_n_t *)parent_node);
         }

        /*
         * Propagate ignore to the field node.
         */
         if (AST_IGNORE_SET(return_type) &&
            (parent_node->fe_info->node_kind == fe_field_n_k))
         {
            AST_SET_IGNORE((AST_field_n_t *)parent_node);
         }

    }   /* End if field/arm node */

    return return_type;
}

/*---------------------------------------------------------------------*/

/*
 *
 *  A S T _ p r o p a g a t e  _ t y p e d e f
 *  ==========================================
 *
 *  Propagate a declarator type to the new type node defined
 *  with the TYPEDEF keyword.  The new type is allocated and
 *  initialized based upon the declarator and the specified
 *  type node.
 */

static AST_type_n_t *AST_propagate_typedef
#ifdef PROTO
(
    AST_type_n_t *type_node_ptr,
    ASTP_declarator_n_t *declarator_ptr,
    ASTP_attributes_t   *attributes
)
#else
(type_node_ptr, declarator_ptr, attributes)
    AST_type_n_t *type_node_ptr;
    ASTP_declarator_n_t *declarator_ptr;
    ASTP_attributes_t   *attributes;
#endif
{
    AST_type_n_t *return_type;          /* type node to return */
    AST_type_n_t *current_type;         /* type node to return */
    ASTP_declarator_op_n_t  *dop;       /* declarator operation ptr */

    /* If there is no declarator operation, we need only copy the type */
    if (declarator_ptr->next_op == NULL)
    {
        /*
         * If the type is anonymous, not a base type, and
         * isn't identified by a struct/union tag, then we
         * can just use it to create the declaration.
         */
        if ((type_node_ptr->name == NAMETABLE_NIL_ID) &&
            !type_is_base(type_node_ptr) &&
            !AST_DEF_AS_TAG_SET(type_node_ptr))
        {
            return_type = type_node_ptr;
        }
        else
        {
            return_type = AST_type_node(type_node_ptr->kind);
            return_type->type_structure = type_node_ptr->type_structure;
            return_type->flags = type_node_ptr->flags;
            return_type->xmit_as_type = type_node_ptr->xmit_as_type;
        }

        /*
         * For simple declarators, if the base type is named, set
         * the defined_as pointer of the result type to the named type.
         */
        if ((type_node_ptr->name != NAMETABLE_NIL_ID) &&
            !AST_DEF_AS_TAG_SET(type_node_ptr))
                return_type->defined_as = type_node_ptr;
        /*
         * Do processing for possibly forward referenced tags.
         * 1) Propagate the incomplete, tag_ptr, and tag_name fields of the fe_info.
         * 2) Make sure that the DEF_AS_TAG bit is clear.
         * 3) If the original type was incomplete, make sure that this type
         *    is added to the tag reference list for later processing.
         */
        return_type->fe_info->tag_ptr = type_node_ptr->fe_info->tag_ptr;
        return_type->fe_info->tag_name = type_node_ptr->fe_info->tag_name;
        if (FE_TEST(type_node_ptr->fe_info->flags,FE_INCOMPLETE))
        {
            FE_SET(return_type->fe_info->flags,FE_INCOMPLETE);
            ASTP_save_tag_ref(type_node_ptr->fe_info->tag_name,
                        type_node_ptr->kind,return_type);
        }
    }
    else
    {
        return_type = type_node_ptr;
        /* Loop through declarator operations to generate the result type */
        for (dop = declarator_ptr->next_op; dop; dop = dop->next_op)
        {
            /* Specify the result type depending upon the declarator */
            current_type = return_type;
            switch (dop->op_kind)
            {
                case AST_array_k:               /* Array declarator */
                    return_type = AST_type_node(dop->op_kind);
                    return_type->type_structure.array =
                            AST_propagate_array_type(return_type,
                                                current_type, dop);
                    break;

                case AST_pointer_k:             /* Pointer declarator */
                    {
                    int i;

                    /* Create the pointer node for each STAR specified */
                    for (i = dop->op_info.pointer_count; i > 0; i--)
                    {
                        boolean is_void = (current_type->kind == AST_void_k);
                        return_type = AST_type_node(dop->op_kind);
                        return_type->type_structure.pointer =
                                AST_pointer_node(current_type);
                        current_type = return_type;

                        /*
                         * Do not use [pointer_default] attribute for the following:
                         *    types with [transmit_as] attribute
                         *    types with [context_handle] attribute
                         *    void * types (they must be context handles)
                         *    toplevel pointer with [ptr], [ref], or [unique]
                         *    [local] interfaces
                         */
                        if (ASTP_TEST_ATTR(attributes,ASTP_TRANSMIT_AS)) continue;
                        if (ASTP_TEST_ATTR(attributes,ASTP_CONTEXT)) continue;
                        if (is_void) continue;
                        if ((dop->next_op == NULL) && (i == 1) &&
                           (ASTP_TEST_ATTR(attributes,ASTP_PTR|ASTP_REF|
                            ASTP_UNIQUE) != 0))
                                  continue;

                        /*
                         *  If none of the above exceptions apply, obtain the
                         *  pointer class from the default for the module, if
                         *  any.
                         */
                        switch (interface_pointer_class)
                        {
                          case 0:            /* No interface default */
                              if (!AST_LOCAL_SET(the_interface))
                              {
                                  char *identifier;
                                  NAMETABLE_id_to_string (
                                      declarator_ptr->name, &identifier);
                                  log_warning(yylineno, NIDL_MISSPTRCLASS, identifier);
                              }
                              break;
                          case ASTP_PTR:      AST_SET_PTR(return_type); break;
                          case ASTP_REF:      AST_SET_REF(return_type); break;
                          case ASTP_UNIQUE:AST_SET_UNIQUE(return_type); break;
                          default:  /* shouldn't get here */
                                error(NIDL_INTERNAL_ERROR, __FILE__, __LINE__);
                        }
                    }
                    break;
                    }

                case AST_function_k:    /* Function declarator */
                    return_type = AST_type_node(dop->op_kind);
                    return_type->type_structure.function =
                            AST_function_node(current_type,
                                               declarator_ptr->name,
                                               dop->op_info.routine_params);
                    break;

                default:        /* Shouldn't get here */
                    error(NIDL_INTERNAL_ERROR, __FILE__, __LINE__);
                    break;
            }


            /*
             * Do processing for possibly forward referenced tags.
             * 1) Propagate the incomplete and tag_ptr fields of the fe_info.
             * 2) Make sure that the REF_AS_TAG bit is clear.
             * 3) If the original type was incomplete, make sure that this type
             *    is added to the tag reference list for later processing.
             */
            return_type->fe_info->tag_ptr = type_node_ptr->fe_info->tag_ptr;
            return_type->fe_info->tag_name = type_node_ptr->fe_info->tag_name;
            if (FE_TEST(type_node_ptr->fe_info->flags,FE_INCOMPLETE))
            {
                FE_SET(return_type->fe_info->flags,FE_INCOMPLETE);
                ASTP_save_tag_ref(type_node_ptr->fe_info->tag_name,
                            type_node_ptr->kind,return_type);
            }
        }
    }


    /* Set the type node name to the declarator's name */
    return_type->name = declarator_ptr->name;

    /* Add name to nametable and bind to new type */
    ASTP_add_name_binding(return_type->name, (char *)return_type);

    return return_type;
}

/*---------------------------------------------------------------------*/

/*
 *
 *  A S T _ p r o p a g a t e _ t y p e
 *  ===================================
 *
 *  Given a type and a declarator this function returns a
 *  type representing the combination of the specified type and
 *  declarator.  If the result is "simple" type, the original
 *  type_ptr is returned.  If the result is a "complex" type, a
 *  new type node is allocated and initialized.
 */

AST_type_n_t *AST_propagate_type
#ifdef PROTO
(
    AST_type_n_t *type_node_ptr,
    ASTP_declarator_n_t *declarator_ptr,
    ASTP_attributes_t *attributes,
    ASTP_node_t *parent_node
)
#else
(type_node_ptr, declarator_ptr, attributes, parent_node)
    AST_type_n_t *type_node_ptr;
    ASTP_declarator_n_t *declarator_ptr;
    ASTP_attributes_t *attributes;
    ASTP_node_t *parent_node;
#endif
{
    AST_type_n_t *return_type;          /* type node to return */
    AST_type_n_t *current_type;         /* type node to return */
    ASTP_declarator_op_n_t  *dop;       /* declarator operation ptr */
    enum
    {
        simple_type,
        complex_type
    } last_op_kind;             /* New type is either simple or complex type */


    return_type = type_node_ptr;
    last_op_kind = simple_type; /* If no declarator_ops, then simple_type */

    /* Loop through declarator operations to generate the result type */
    for (dop = declarator_ptr->next_op; dop; dop = dop->next_op)
    {
        last_op_kind = complex_type;    /* Assume complex */

        /* Specify the result type depending upon the declarator */
        current_type = return_type;
        switch (dop->op_kind)
        {
            case AST_array_k:           /* Array declarator */
                ASTP_validate_forward_ref(type_node_ptr);
                return_type = AST_type_node(dop->op_kind);
                return_type->type_structure.array =
                        AST_propagate_array_type(return_type,
                                            current_type, dop);
                break;

            case AST_pointer_k:         /* Pointer declarator */
                {
                int i;

                /* Create the pointer node for each STAR specified */
                for (i = dop->op_info.pointer_count; i > 0; i--)
                {
                    boolean is_void = (current_type->kind == AST_void_k);

                    ASTP_verify_non_anonymous(current_type, declarator_ptr);

                    return_type = AST_type_node(dop->op_kind);
                    return_type->type_structure.pointer =
                            AST_pointer_node(current_type);
                    current_type = return_type;

                    /*
                     * Do not use [pointer_default] attribute for the following:
                     *    operation results (are always [ptr]),
                     *    constants
                     *    types with [transmit_as] attribute
                     *    types with [context_handle] attribute
                     *    void * types (they must be context handles)
                     *    toplevel pointer with with [ptr], [ref] or [unique]
                     *    toplevel anonymous param pointers
                     *    [local] interfaces
                     */
                    if (parent_node->fe_info->node_kind == fe_operation_n_k)
                              continue;
                    if (parent_node->fe_info->node_kind == fe_constant_n_k)
                              continue;
                    if (ASTP_TEST_ATTR(attributes,ASTP_TRANSMIT_AS)) continue;
                    if (ASTP_TEST_ATTR(attributes,ASTP_CONTEXT)) continue;
                    if (is_void) continue;
                    if ((dop->next_op == NULL) && (i == 1) && (ASTP_TEST_ATTR(
                        attributes,ASTP_PTR|ASTP_REF|ASTP_UNIQUE) != 0))
                              continue;
                    if ((dop->next_op == NULL) && (i == 1) &&
                        (parent_node->fe_info->node_kind == fe_parameter_n_k))
                              continue;

                    /*
                     *  If we passed all of the exceptions, obtain the pointer
                     *  class from the default, and add to pointer.
                     */
                    switch (interface_pointer_class)
                    {
                    case 0:             /* No interface default */
                          if (!AST_LOCAL_SET(the_interface))
                          {
                              char *identifier;
                              NAMETABLE_id_to_string (
                                  declarator_ptr->name, &identifier);
                              log_warning(yylineno, NIDL_MISSPTRCLASS, identifier);
                          }
                          break;
                    case ASTP_PTR:      AST_SET_PTR(return_type); break;
                    case ASTP_REF:      AST_SET_REF(return_type); break;
                    case ASTP_UNIQUE:AST_SET_UNIQUE(return_type); break;
                    default:            /* shouldn't get here */
                            error(NIDL_INTERNAL_ERROR, __FILE__, __LINE__);
                    }
                }
                break;
                }

            case AST_function_k:    /* Function declarator */
                ASTP_validate_forward_ref(type_node_ptr);
                return_type = AST_type_node(dop->op_kind);
                return_type->type_structure.function =
                        AST_function_node(type_node_ptr,
                                           declarator_ptr->name,
                                           dop->op_info.routine_params);
                break;

            default:                /* Shouldn't get here */
                error(NIDL_INTERNAL_ERROR, __FILE__, __LINE__);
                break;
        }
    }

    /*
     * Set the special (REF, PTR, UNIQUE) field/parameter/arm attributes
     * to the type node, and propagate the appropriate type attributes
     * back to the field/parameter/arm node.
     */
    return_type =
        AST_propagate_type_attrs(return_type, attributes,
                                 (boolean)(last_op_kind == simple_type),
                                 (ASTP_node_t *)parent_node);

    return return_type;
}

/*---------------------------------------------------------------------*/


/*
 * A S T P _ s a v e _ f i e l d _ r e f _ c o n t e x t
 * =====================================================
 *
 * Saves the necessary information in a field reference
 * context block such that it can be processed at a
 * later time.
 *
 * Inputs:
 *  name                Nametable identifier specifying the reference name.
 *  field_ref_addr      The field reference address where the pointer to the
 *                      referenced parameter node goes when it is later defined.
 * Outputs:
 *  None.
 *
 * Function value:
 *  None.
 *
 */
static void ASTP_save_field_ref_context
#ifdef PROTO
(
    NAMETABLE_id_t name,
    AST_field_ref_n_t *field_ref_addr,
    fe_info_t *fe_info
)
#else
(name, field_ref_addr, fe_info)
    NAMETABLE_id_t name;
    AST_field_ref_n_t *field_ref_addr;
    fe_info_t *fe_info;
#endif
{
    ASTP_field_ref_ctx_t *field_ref_ctx_ptr;

    field_ref_ctx_ptr = (ASTP_field_ref_ctx_t *)
                            MALLOC (sizeof (ASTP_field_ref_ctx_t));
    field_ref_ctx_ptr->fe_info = fe_info;
    field_ref_ctx_ptr->be_info = NULL;
    field_ref_ctx_ptr->next = NULL;
    field_ref_ctx_ptr->last = NULL;

    /* Save context and link to context list */
    field_ref_ctx_ptr->name = name;
    field_ref_ctx_ptr->field_ref_ptr = field_ref_addr;

    ASTP_field_ref_ctx_list = (ASTP_field_ref_ctx_t *)
                    AST_concat_element((ASTP_node_t *)ASTP_field_ref_ctx_list,
                                        (ASTP_node_t *)field_ref_ctx_ptr);

    return;
}


/*--------------------------------------------------------------------*/

/*
 *  A S T _ s e t _ a r r a y _ r e p _ t y p e
 *  ============================================
 *
 *  This routine should be called with pointer that has the [string] attribute.
 *  It sets the array_rep_type of the type passed if it is pointer and it
 *  doesn't yet have an array rep.
 */

void ASTP_set_array_rep_type
#ifdef PROTO
(
    AST_type_n_t        *type_node_ptr,
    AST_type_n_t        *array_base_type,
    boolean             is_varying
)
#else
(type_node_ptr, array_base_type, is_varying)
    AST_type_n_t *type_node_ptr;
    AST_type_n_t        *array_base_type;
    boolean             is_varying;
#endif
{
    char *identifier;
    AST_type_p_n_t *tp_node;    /* type pointer node for interface's
                                   linked list of transmit_as types */

    /* If the type is pointer, and no array_rep_type then create one */
    if (type_node_ptr->kind == AST_pointer_k)
    {
        if (type_node_ptr->type_structure.pointer->pointee_type->array_rep_type == NULL)
        {
            /*
             * No array node, so special processing, for pointers treated as
             * arrays.  We now fill-in the array_rep_type field of the pointer node
             * to contain a conformant array node representing the pointer as if it
             * had been declared using array syntax.
             */
            AST_type_n_t    *array_rep_of_pointer;
            AST_array_n_t   *array_node;

            array_node = AST_array_node(array_base_type);
            array_node->index_count = 1;
            array_node->index_vec = AST_array_index_node(1);
            array_node->index_vec->lower_bound = zero_constant_p;
            AST_SET_FIXED_LOWER(array_node->index_vec);

            array_rep_of_pointer = AST_type_node(AST_array_k);
            array_rep_of_pointer->type_structure.array = array_node;
            AST_SET_CONFORMANT(array_rep_of_pointer);
            type_node_ptr->type_structure.pointer->pointee_type->array_rep_type = array_rep_of_pointer;

            /*
             * Do the same processing on the original of a DEF_AS_TAG node.
             */
            if (AST_DEF_AS_TAG_SET(type_node_ptr->type_structure.pointer->pointee_type) &&
                (type_node_ptr->type_structure.pointer->pointee_type->fe_info->original != NULL) &&
                (type_node_ptr->type_structure.pointer->pointee_type->fe_info->original->array_rep_type == NULL))
            {
                ASTP_set_array_rep_type(type_node_ptr,
                    type_node_ptr->type_structure.pointer->pointee_type->fe_info->original,
                    is_varying);
            }
        }

        /*
         * Set the varying flag on the type, if referenced as varying inorder
         * to indicate that the pointed at routine must be generated to
         * support varying instances.
         */
        if (is_varying)
        { 
            AST_SET_VARYING(type_node_ptr->type_structure.pointer->pointee_type->array_rep_type);

            /*
             * Do the same processing on the original of a DEF_AS_TAG node.
             */
            if (AST_DEF_AS_TAG_SET(type_node_ptr->type_structure.pointer->pointee_type) &&
               (type_node_ptr->type_structure.pointer->pointee_type->fe_info->original != NULL) &&
               (type_node_ptr->type_structure.pointer->pointee_type->fe_info->original->array_rep_type != NULL))
            {
                AST_SET_VARYING(type_node_ptr->type_structure.pointer->pointee_type->fe_info->original->array_rep_type);
            }
        }
    }
}

/*--------------------------------------------------------------------*/

/*
 *  A S T _ s e t _ f i e l d _ a t t r s
 *  =====================================
 *
 *  Set the field attributes of a parameter or structure.  In addition, if type
 *  is a pointer used as an array, the array_rep_type field of the pointer node
 *  is filled in with a type node used to represent the array implicitly created
 *  by using a pointer in this manner.
 *
 * Inputs:
 *  attributes   attr_flags = boolean attributes
 *               array_bounds = a pointer to a linked list describing the field
 *                   attributes that were specified.
 *
 *  parent_node  Pointer to the parent node containing the
 *               field attribute pointer.  This is either
 *               a pointer to a parameter node or field node.
 *               For parameters, this is used to track down
 *               the array node to get the array dimension.
 *               Used to also access the fe_info to see determine
 *               node it is, so we know whether we dealing with
 *               a field or parameter attributes.
 *
 *  type_node   Pointer to the type node representing the
 *              field or parameter type.
 *
 * Outputs:
 *  None.
 *
 * Function Value:
 *  Pointer to the field attribute node created.
 *
 */

AST_field_attr_n_t *AST_set_field_attrs
#ifdef PROTO
(
    ASTP_attributes_t  *attributes,
    ASTP_node_t     *parent_node,
    AST_type_n_t            *type_node
)
#else
(attributes, parent_node, type_node)
    ASTP_attributes_t  *attributes;
    ASTP_node_t     *parent_node;
    AST_type_n_t            *type_node;
#endif
{
    AST_field_attr_n_t
            *field_attr_node;   /* Pointer to field attribute node */
    AST_field_ref_n_t
            *field_ref_vector;  /* Pointer to the field_ref vector */
    ASTP_type_attr_n_t
            *attr_ptr,          /* Pointer to an element of array_bounds */
            *next_attr_ptr;     /* For loop - next attribute to process */
    ASTP_node_t
            *referent_node;     /* Pointer to node describing the reference
                                     specified as the array bound */
    AST_type_n_t
            *clone_type,        /* For setting conformant flag for pointer types */
            *tp;                /* For loop to get to index_count */
    unsigned short
            dimension,          /* number of dimensions for the array */
            index_count;        /* Used to catch mismatch in number of
                                    references specified and dimension */
    char    *identifier;        /* For logging error in name reference */
    boolean pointer_as_array = FALSE,
            noforward_ref = TRUE;   /* Forward refs. allowed only for parameters */
    ASTP_attr_k_t
            current_attr_kind;  /* Current attribute being worked on */

    /*
     *  If no bounds information, and not arrayified via [string], done
     */
    if ((attributes->bounds == NULL) && !ASTP_TEST_ATTR(attributes,ASTP_STRING))
       return (AST_field_attr_n_t *)NULL;

    /*
     * Determine dimension of the array for
     * allocating field_reference vector.
     */
    tp = ASTP_chase_ptr_to_kind(type_node, AST_array_k);

    /*
     *  If there is no array node, then verify that this type may be arrayified
     *  via attributes.  If not, issue an error.
     */
    if (tp == NULL)
    {
        if (((parent_node->fe_info->node_kind == fe_field_n_k) &&
             (type_node->kind != AST_pointer_k)) ||
            ((parent_node->fe_info->node_kind == fe_parameter_n_k) &&
             ((type_node->kind != AST_pointer_k)||
              (type_node->name != NAMETABLE_NIL_ID))))
        {
            /*
             *  Output an error (except when only [string] attribute is present
             *  because this is caught by checker) and return if we don't find
             *  an array node or anonymous pointer hanging off of this type.
             */
            if (attributes->bounds != NULL)
                  log_error(yylineno, NIDL_SIZEARRTYPE);
            return (AST_field_attr_n_t *)NULL;
        }
    }


    /*
     * We now know we either have an array or a top level pointer used an
     * array.  If we found an array node, we can directly access it.
     */
    if (tp != NULL)
    {
        dimension = tp->type_structure.array->index_count;
    }

    /* If create the array_rep_type for this pointee */
    else if (type_node->kind == AST_pointer_k)
    {
        dimension = 1;
        pointer_as_array = TRUE;
        ASTP_set_array_rep_type(type_node,
            type_node->type_structure.pointer->pointee_type,
            FALSE);
    }

    /*
     *  If only arrayified via [string] then all we needed to do was fill in
     *  the array_rep_type of the pointer which we have done, so return
     */
    if (attributes->bounds == NULL)
    {
        ASTP_set_array_rep_type(type_node,
            type_node->type_structure.pointer->pointee_type,
            TRUE);
        return (AST_field_attr_n_t *)NULL;
    }

    /* Allocate and initialize field attribute node */
    field_attr_node = AST_field_attr_node();

    /*
     * A field attribute node points to a vector of
     * field reference nodes, one for each dimension,
     * and tells whether the node contains a valid
     * reference.  If valid, it points to the field or
     * parameter node containing the attribute reference.
     *
     * Note, parameter references can be forwarded.  (This
     * is not allowed for field references).
     * If the parameter is referenced before the formal parameter
     * definition, we save the context containing a the address
     * to place the reference pointer, and patch it up later on
     * when finishing up the operation.
     */

    /*
     * Loop through the array bound linked list creating the
     * field reference vector for each field attribute present.
     * An inner loop processes the field references for each
     * dimension of the same array bound type.
     */

    for (attr_ptr=attributes->bounds;
         attr_ptr != NULL;
         attr_ptr= attr_ptr->next)
    {
        /*
         * Get (create if not yet specified) the field reference vector
         * for the attribute specified.
         */
        switch (attr_ptr->kind)
        {
            case first_is_k:
                if (field_attr_node->first_is_vec == NULL)
                {
                    field_attr_node->first_is_vec=AST_field_ref_node(dimension);
                }
                field_ref_vector = field_attr_node->first_is_vec;
                break;

            case last_is_k:
                if (field_attr_node->last_is_vec == NULL)
                {
                    field_attr_node->last_is_vec=AST_field_ref_node(dimension);

                }
                field_ref_vector = field_attr_node->last_is_vec;
                break;

            case length_is_k:
                if (field_attr_node->length_is_vec == NULL)
                {
                    field_attr_node->length_is_vec=AST_field_ref_node(dimension);
                }
                field_ref_vector = field_attr_node->length_is_vec;
                break;

            case min_is_k:
                if (field_attr_node->min_is_vec == NULL)
                {
                    field_attr_node->min_is_vec=AST_field_ref_node(dimension);
                }
                field_ref_vector = field_attr_node->min_is_vec;
                break;

            case max_is_k:
                if (field_attr_node->max_is_vec == NULL)
                {
                    field_attr_node->max_is_vec=AST_field_ref_node(dimension);
                }
                field_ref_vector = field_attr_node->max_is_vec;
                break;

            case size_is_k:
                if (field_attr_node->size_is_vec == NULL)
                {
                    field_attr_node->size_is_vec=AST_field_ref_node(dimension);
                }
                field_ref_vector = field_attr_node->size_is_vec;
                break;

            default:
                break;
        }

        /* Set varying flag if last_is/length_is/first_is */
        if ((attr_ptr->kind == last_is_k)   ||
            (attr_ptr->kind == length_is_k) ||
            (attr_ptr->kind == first_is_k))
            {
                if (parent_node->fe_info->node_kind == fe_parameter_n_k)
                {
                    AST_SET_VARYING((AST_parameter_n_t*)parent_node);
                }
                else
                {
                    AST_SET_VARYING((AST_field_n_t*)parent_node);
                }

                if (pointer_as_array)
                {
                    ASTP_set_array_rep_type(type_node,
                        type_node->type_structure.pointer->pointee_type,
                        TRUE);
                }
            }

        /* Process all references specified for this attribute */
        current_attr_kind = attr_ptr->kind;
        next_attr_ptr = attr_ptr;
        index_count = 0;
        do
        {
            if (index_count >= dimension)
            {
                log_error(yylineno, NIDL_SIZEMISMATCH);
                break;
            }

            /* Get next attribute reference */
            attr_ptr = next_attr_ptr;

            /* Proceed if reference name was specified */
            if (attr_ptr->name != NAMETABLE_NIL_ID)
            {

                /*
                 * Set the pointer count.
                 * (This will need to be a bit more
                 * sophisicated IF/WHEN IDL supports full
                 * declarators for array bound references.)
                 */
                if (attr_ptr->pointer)
                {
                    field_ref_vector->fe_info->pointer_count = 1;
                }

                /*
                 * Lookup binding, output error if invalid type,
                 * but allow forward references for parameters and fields.
                 */
                if ((parent_node->fe_info->node_kind == fe_parameter_n_k) ||
                    (parent_node->fe_info->node_kind == fe_field_n_k))
                {
                    noforward_ref = FALSE;
                }

                referent_node = (ASTP_node_t *)
                       ASTP_lookup_binding(attr_ptr->name,
                                           parent_node->fe_info->node_kind,
                                           noforward_ref);

                /*
                 * Set valid bit and fill in reference pointer
                 * if lookup succeeded.
                 */
                if (referent_node != NULL)
                {
                    field_ref_vector->valid = TRUE;

                    if (parent_node->fe_info->node_kind == fe_parameter_n_k)
                    {
                        field_ref_vector->ref.p_ref =
                                        (AST_parameter_n_t *)referent_node;
                    }
                    else
                    {
                        field_ref_vector->ref.f_ref =
                                        (AST_field_n_t *)referent_node;
                    }
                }
                else
                {
                    /*
                     * If name binding is unknown, save context if
                     * we're dealing with parameters or fields, otherwise
                     * output an error.
                     */
                    if ((parent_node->fe_info->node_kind == fe_parameter_n_k) ||
                        (parent_node->fe_info->node_kind == fe_field_n_k))
                    {
                        /* Name not bound yet, save context for later */
                        ASTP_save_field_ref_context(
                                        attr_ptr->name,
                                        field_ref_vector,
                                        parent_node->fe_info);
                    }
                    else
                    {
                        NAMETABLE_id_to_string (attr_ptr->name,
                                                &identifier);
                        log_error(yylineno, NIDL_NAMENOTFND, identifier) ;
                    }
                }
            }

            /*
             * Point to next array bound element
             * and reference vector element.
             */
            next_attr_ptr = next_attr_ptr->next;
            field_ref_vector++;
            index_count++;
        }
        while ( (attr_ptr->next != NULL) &&
                (attr_ptr->next->kind == current_attr_kind)) ;

    } /* end for processing all attributes in attributes->bounds */



    return field_attr_node;

}

/*--------------------------------------------------------------------*/

/*
 *  A S T _ s e t _ t y p e _ a t t r s
 *  ===================================
 *
 *  Merge the boolean and non-boolean attributes
 *  to the type node.
 */

AST_type_n_t *AST_set_type_attrs
#ifdef PROTO
(
    AST_type_n_t        *type_node_ptr,
    ASTP_attributes_t   *attributes
)
#else
(type_node_ptr, attributes)
    AST_type_n_t *type_node_ptr;
    ASTP_attributes_t   *attributes;
#endif
{
    char *identifier;
    AST_type_p_n_t *tp_node;    /* type pointer node for interface's
                                   linked list of transmit_as types */

    /* Propagate the boolean attributes to the type node */
    AST_set_flags(&type_node_ptr->flags,(ASTP_node_t *)type_node_ptr, attributes);

    /* if this is a [v1_enum] enum make it be a long at NDR level */
    if (AST_V1_ENUM_SET(type_node_ptr))
    {
        type_node_ptr->ndr_size = 4;
        type_node_ptr->alignment_size = type_node_ptr->ndr_size;
    }

    /* If [string] on a pointer type and no array_rep_type then create one */
    if (ASTP_TEST_ATTR(attributes,ASTP_STRING) &&
       (type_node_ptr->kind == AST_pointer_k))
    {
        ASTP_set_array_rep_type(type_node_ptr,
            type_node_ptr->type_structure.pointer->pointee_type,
            TRUE);
    }


    /* Check the transmit_as attribute */
    if (ASTP_TEST_ATTR(attributes,ASTP_TRANSMIT_AS))
    {
        type_node_ptr->xmit_as_type = ASTP_transmit_as_type;
        if (type_node_ptr->xmit_as_type == NULL)
            INTERNAL_ERROR("ASTP_transmit_as_type is NULL when attribute set");

        /*
         * Create a new type pointer node and link the transmit_as type on to
         * xa_types list to be later put on the interface node.
         */
        tp_node = AST_type_ptr_node();
        tp_node->type = type_node_ptr;

        the_interface->xa_types = (AST_type_p_n_t *)
                        AST_concat_element((ASTP_node_t *)the_interface->xa_types,
                                           (ASTP_node_t *)tp_node);

    }

    /* Check for pipe type and link onto pipe_types list */
    if (type_node_ptr->kind == AST_pipe_k)
    {
        /*
         * Create a new type pointer node and link the pipe type on to
         * pipe_types list to be later put on the interface node
         */
        tp_node = AST_type_ptr_node();
        tp_node->type = type_node_ptr;

        the_interface->pipe_types = (AST_type_p_n_t *)
                        AST_concat_element((ASTP_node_t *)the_interface->pipe_types,
                                           (ASTP_node_t *)tp_node);
    }

    return type_node_ptr;
}

/*---------------------------------------------------------------------*/


/*
 *  A S T P _ f r e e _ d e c l a r a t o r s
 *  ==========================================
 *
 *  Loop through the specified list of declarators and free
 *  them and any indices that are hanging off of the
 *  declarators.
 *
 */

void ASTP_free_declarators
#ifdef PROTO
(
    ASTP_declarator_n_t *declarators_ptr
)
#else
(declarators_ptr)
    ASTP_declarator_n_t *declarators_ptr;
#endif

{
    ASTP_declarator_n_t
        *dp,                        /*  For traversing through declarators */
        *next_dp;

    ASTP_declarator_op_n_t
        *dop,                       /*  For traversing through declarators */
        *next_dop;

    ASTP_array_index_n_t
        *index_node,                /* For freeing array index nodes */
        *next_index;                /* "" */


    /* Free temporary declarators list */
    for (dp = declarators_ptr, next_dp = declarators_ptr;
            next_dp != (ASTP_declarator_n_t *) NULL;
            dp= next_dp)
    {
        next_dp = dp->next;     /* Save next link before freeing */

        /* Free declarator operations linked list */
        for (dop = dp->next_op, next_dop = dp->next_op;
             next_dop != NULL;
             dop = next_dop)
        {
            /* Free array indices linked list in this declarator op, if any */
            if (dop->op_kind == AST_array_k)
            {
                for (index_node = dop->op_info.indices, next_index = dop->op_info.indices;
                     next_index != NULL;
                     index_node = next_index)
                {
                    next_index = index_node->next;
                    FREE(index_node);
                }
            }


            next_dop = dop->next_op;
            FREE(dop);
        }

        FREE(dp);
    }


    return;
}

/*-----------------------------------------------------------------------*/

/*
 *  A S T P _ f r e e _ s i m p l e _ l i s t
 *  =========================================
 *
 *  Loop through a list of simple elements, i.e.
 *  no allocated lists hanging off the list element.
 *
 */

void ASTP_free_simple_list
#ifdef PROTO
(
    ASTP_node_t *list_ptr
)
#else
(list_ptr)
    ASTP_node_t *list_ptr;
#endif
{
    ASTP_node_t *lp,
                 *next_lp;

    for (lp = list_ptr, next_lp = list_ptr;
            next_lp != (ASTP_node_t *) NULL;
            lp = next_lp)
    {
        next_lp = lp->next;
        FREE(lp);
    }


    return;
}

/*-----------------------------------------------------------------------*/

/*
 * A S T P _ p a t c h _ f i e l d _ r e f e r e n c e
 * ===================================================
 *
 * Patches the field reference node parameter node address
 * which at the time the field attribute was parsed,
 * a binding of a parameter type was not yet defined.
 *
 * Inputs:
 *  None.
 *
 * Outputs:
 *  None.
 *
 * Function value:
 *  None.
 *
 */
void ASTP_patch_field_reference
#ifdef PROTO
(void)
#else
()
#endif
{
    ASTP_field_ref_ctx_t *field_ref_ctx;
    AST_parameter_n_t    *parameter_node_ptr;
    AST_field_n_t    *field_node_ptr;

    for (field_ref_ctx = ASTP_field_ref_ctx_list;
         field_ref_ctx != NULL;
         field_ref_ctx = field_ref_ctx->next)
    {
        /* Processing for parameter references */
        if (field_ref_ctx->fe_info->node_kind == fe_parameter_n_k)
        {
            /* Lookup binding and output error if not found or invalid type */
            parameter_node_ptr = (AST_parameter_n_t *)
                                    ASTP_lookup_binding(field_ref_ctx->name,
                                                        fe_parameter_n_k, TRUE);
            if (parameter_node_ptr != NULL)
            {
                field_ref_ctx->field_ref_ptr->ref.p_ref = parameter_node_ptr;
                field_ref_ctx->field_ref_ptr->valid = TRUE;
            }
        }

        /* Processing for field references */
        if (field_ref_ctx->fe_info->node_kind == fe_field_n_k)
        {
            /* Lookup binding and output error if not found or invalid type */
            field_node_ptr = (AST_field_n_t *)
                                    ASTP_lookup_binding(field_ref_ctx->name,
                                                        fe_field_n_k, TRUE);
            if (field_node_ptr != NULL)
            {
                field_ref_ctx->field_ref_ptr->ref.f_ref = field_node_ptr;
                field_ref_ctx->field_ref_ptr->valid = TRUE;
            }
        }
    }

    ASTP_field_ref_ctx_list = NULL;     /* NULL listhead */

    return;
}

/*---------------------------------------------------------------------*/


/*
 *  A S T P _ s e t _ f e _ i n f o
 *  ===============================
 *
 *  Allocate and initialize a fe_info node and link it
 *  into the fe_info field of the specified node.  The
 *  be_info field is set to NULL.
 *
 *  Inputs:
 *      node_ptr -- node to have the fe_info and be_info set
 *      fe_node_kind -- type of fe_node to create
 */
void ASTP_set_fe_info
#ifdef PROTO
(
    ASTP_node_t *node_ptr,
    fe_node_k_t fe_node_kind
)
#else
(node_ptr,fe_node_kind)
    ASTP_node_t *node_ptr;
    fe_node_k_t fe_node_kind;
#endif
{
    node_ptr->fe_info = (fe_info_t *) BE_ctx_malloc(sizeof (fe_info_t));
    node_ptr->fe_info->source_line = yylineno;
    node_ptr->fe_info->file = error_file_name_id;
    node_ptr->fe_info->acf_source_line = 0;
    node_ptr->fe_info->acf_file = 0;
    node_ptr->fe_info->node_kind = fe_node_kind;
    node_ptr->fe_info->fe_type_id = fe_source_only;
    node_ptr->fe_info->tag_ptr = NULL;
    node_ptr->fe_info->tag_name = NAMETABLE_NIL_ID;
    node_ptr->fe_info->gen_index = 0;
    node_ptr->fe_info->pointer_count = 0;
    node_ptr->fe_info->flags = 0;
    node_ptr->fe_info->original = NULL;

    node_ptr->be_info = NULL;
}

/*---------------------------------------------------------------------*/

/*
 *  A S T P _ s a v e _ t a g _ r e f
 *  =================================
 *
 *  Allocate and initialize a tag reference node and link it
 *  into the ASTP_tag_ref_list to be processed at a later
 *  time.
 *
 *  Inputs:
 *      node_ptr -- node to have the fe_info and be_info set
 *      fe_node_kind -- type of fe_node to create
 */

void ASTP_save_tag_ref
#ifdef PROTO
(
    NAMETABLE_id_t      identifier,
    AST_type_k_t        kind,
    AST_type_n_t        *type_node_ptr
)
#else
(identifier, kind, type_node_ptr)
    NAMETABLE_id_t      identifier;
    AST_type_k_t        kind;
    AST_type_n_t        *type_node_ptr;
#endif
{
    ASTP_tag_ref_n_t    *tag_ref_node_ptr;      /* Tag reference node */

    /*
     * Make sure the type node specified could be a forward reference
     */
    if ((type_node_ptr->kind != AST_structure_k) &&
        (type_node_ptr->kind != AST_disc_union_k))  return;

    /*
     * Then build a tag ref node and add to the list to be processed later to
     * fill in the structure information in the type node when it becomes
     * known.
     */
    tag_ref_node_ptr = (ASTP_tag_ref_n_t *) MALLOC (sizeof (ASTP_tag_ref_n_t));
    tag_ref_node_ptr->next = NULL;
    tag_ref_node_ptr->last = NULL;
    tag_ref_node_ptr->fe_info = NULL;
    tag_ref_node_ptr->be_info = NULL;
    tag_ref_node_ptr->ref_kind = kind;
    tag_ref_node_ptr->name = identifier;
    tag_ref_node_ptr->type_node_ptr = type_node_ptr;
    ASTP_tag_ref_list = (ASTP_tag_ref_n_t *)
            AST_concat_element((ASTP_node_t *)ASTP_tag_ref_list,
                               (ASTP_node_t *)tag_ref_node_ptr);

    /*
     * Mark the specified type node as incomplete
     */
    FE_SET(type_node_ptr->fe_info->flags,FE_INCOMPLETE);
}

/*---------------------------------------------------------------------*/

/*
 *  A S T P _ p r o c e s s _ c o n t e x t _ t y p e
 *  =================================================
 *
 *  This routine provides the processing needed for types that have
 *  context_handle attribute.  If not already marked with the
 *  [context_handle] it is added to the ASTP_context_handle_types_list
 *
 *  Inputs:
 *      type_node_ptr -- The newly found context_handle type.
 */

static void ASTP_process_context_type
#ifdef PROTO
(
    AST_type_n_t *type_node_ptr
)
#else
(type_node_ptr)
    AST_type_n_t *type_node_ptr;
#endif
{
    AST_type_p_n_t *tp_node; /* type pointer node to link on chain */

    /*
     * If [context_handle] already set, then return otherwise set it
     * and put on context handle types list
     */
    if (AST_CONTEXT_RD_SET(type_node_ptr)) return;
    AST_SET_CONTEXT_RD(type_node_ptr);

    /*
     * Create a new type pointer node and link it on the context_handle_types
     * list of the interface node.
     */
    tp_node = AST_type_ptr_node();
    tp_node->type = type_node_ptr;

    /* link it on the sp_types list of the interface node */
    the_interface->ch_types = (AST_type_p_n_t *)AST_concat_element(
                        (ASTP_node_t *)the_interface->ch_types,
                        (ASTP_node_t *)tp_node);

}

/*---------------------------------------------------------------------*/

/*
 *  A S T _ s e t _ f l a g s
 *  =======================================
 *
 *  This routine accepts a node and the vector of flag bits from
 *  the grammar.  It verifies the the attributes are valid in this
 *  context, issues errors for those that are not, and then sets
 *  the bits in the flags word specified.
 *
 *  Inputs:
 *      field_node_ptr -- node on which to set attributes
 *      flags -- address of flags structure
 */

void AST_set_flags
#ifdef PROTO
(
    AST_flags_t         *flags,
    ASTP_node_t         *node_ptr,
    ASTP_attributes_t   *attributes
)
#else
(flags, node_ptr, attributes)
    AST_flags_t         *flags;
    ASTP_node_t         *node_ptr;
    ASTP_attributes_t   *attributes;
#endif
{
#define COPY_IF_LARGER(a,b) {if ((b) > (a)) (a) = (b);}
    ASTP_type_attr_n_t  *bp;                /* pointer to loop through bounds */
    ASTP_attr_flag_t    current_attribute;
    ASTP_attr_flag_t    valid_attr_flags;
    long                message_number;
    boolean             bounds_illegal = TRUE;

    /*
     * Determine which flags are valid for this node type
     */
    switch (node_ptr->fe_info->node_kind)
    {
        case fe_field_n_k:
            valid_attr_flags = ASTP_FIELD_FLAGS;
            message_number = NIDL_ILLFIELDATTR;
            bounds_illegal = FALSE;
            break;
        case fe_arm_n_k:
            valid_attr_flags = ASTP_ARM_FLAGS;
            message_number = NIDL_ILLMEMATTR;
            break;
        case fe_type_n_k:
            valid_attr_flags = ASTP_TYPE_FLAGS;
            message_number = NIDL_ILLTYPEATTR;
            break;
        case fe_parameter_n_k:
            valid_attr_flags = ASTP_PARAMETER_FLAGS;
            message_number = NIDL_ILLPARAMATTR;
            bounds_illegal = FALSE;
            break;
        case fe_operation_n_k:
            valid_attr_flags = ASTP_OPERATION_FLAGS;
            message_number = NIDL_ILLOPATTR;
            break;
        case fe_interface_n_k:
            valid_attr_flags = 0;
            message_number = NIDL_ILLINTATTR;
            break;
        default:
            valid_attr_flags = 0;
            break;
    }


    /*
     * Loop through all possible attributes and either set the
     * attribute on the target node, or issue a message that it
     * is not valid on this node type.
     */
    for (current_attribute = 1;         /* start with first bit */
          current_attribute <= (unsigned) ASTP_MAX_ATTRIBUTE; /* until and including last bit */
          current_attribute <<= 1)      /* Next higher bit */
    {
        /* Check if the attribute flag is set */
        if (ASTP_TEST_ATTR(attributes,current_attribute))
        {
            /* Yes, so make sure it is valid on this node type before setting */
            if ((current_attribute & ~valid_attr_flags) == 0)
            {
                switch (current_attribute)
                {
                 /* With BROADCAST, IDEMPOTENT is implicit */
                 case ASTP_BROADCAST:
                      *flags |= (AST_BROADCAST | AST_IDEMPOTENT); break;

                 /* MAYBE implies IDEMPOTENT */
                 case ASTP_MAYBE:
                      *flags |= (AST_MAYBE | AST_IDEMPOTENT); break;

                 case ASTP_IDEMPOTENT:
                      *flags |= AST_IDEMPOTENT; break;

                 case ASTP_IN:
                      *flags |= AST_IN; break;

                 case ASTP_IN_SHAPE:
                      *flags |= AST_IN_SHAPE; break;

                 case ASTP_OUT:
                      *flags |= AST_OUT; break;

                 case ASTP_OUT_SHAPE:
                      *flags |= AST_OUT_SHAPE; break;

                 case ASTP_STRING:
                      *flags |= AST_STRING; break;

                 case ASTP_STRING0:
                      *flags |= AST_STRING0; break;

                 case ASTP_UNIQUE:
                      *flags |= AST_UNIQUE; break;

                 case ASTP_REF:
                      *flags |= AST_REF; break;

                 case ASTP_PTR:
                      *flags |= AST_PTR; break;

                 case ASTP_V1_ENUM:
                      *flags |= AST_V1_ENUM; break;

                 case ASTP_IGNORE:
                      *flags |= AST_IGNORE; break;

                 case ASTP_SMALL:
                      *flags |= AST_SMALL; break;

                 case ASTP_HANDLE:
                      *flags |= AST_HANDLE; break;

                 case ASTP_UNALIGN:
                      *flags |= AST_UNALIGN; break;

                 case ASTP_ALIGN_SMALL:
                 case ASTP_ALIGN_SHORT:
                 case ASTP_ALIGN_LONG:
                 case ASTP_ALIGN_HYPER:
                 {
                    log_error(yylineno, NIDL_NYSALIGN);
#if 0
                    AST_type_n_t *type_node_ptr = (AST_type_n_t *)node_ptr;

                    /* Set alignment requirement on type */
                    switch (current_attribute)
                    {
                    case ASTP_ALIGN_SMALL:
                        COPY_IF_LARGER(type_node_ptr->alignment_size, NDR_C_SMALL_INT_SIZE)
                        break;
                    case ASTP_ALIGN_SHORT:
                        COPY_IF_LARGER(type_node_ptr->alignment_size, NDR_C_SHORT_INT_SIZE)
                        break;
                    case ASTP_ALIGN_LONG:
                        COPY_IF_LARGER(type_node_ptr->alignment_size, NDR_C_LONG_INT_SIZE)
                        break;
                    case ASTP_ALIGN_HYPER:
                        COPY_IF_LARGER(type_node_ptr->alignment_size, NDR_C_HYPER_INT_SIZE)
                        break;
                    }

                    /* Disallow align on anything by arrays of bytes */
                    if (!((node_ptr->fe_info->node_kind == fe_type_n_k) &&
                        (type_node_ptr->kind == AST_array_k) &&
                        (type_node_ptr->type_structure.array != NULL) &&
                        (type_node_ptr->type_structure.array->index_count == 1) &&
                        (type_node_ptr->type_structure.array->element_type->kind == AST_byte_k)))
                    {
                        log_error(yylineno, NIDL_ALIGNBYTEARRAY);
                    }
#endif
                    break;
                 }



                  /* process type node with [context_handle] attribute */
                  case ASTP_CONTEXT:
                  {
                    if (node_ptr->fe_info->node_kind == fe_type_n_k)
                            ASTP_process_context_type((AST_type_n_t *)node_ptr);
                    else
                            *flags |= AST_CONTEXT;
                    break;
                  }

                  /* Transmit_as handled in AST_set_type_attrs */
                  case ASTP_TRANSMIT_AS:
                      break;
#ifdef sgi
		  case ASTP_SLOT:
                      *flags |= AST_SLOT; break;
		  case ASTP_BB:
                      *flags |= AST_BB; break;
#endif

                  /* Attribute has not been handled, bug */
                  default:
                      error(NIDL_INTERNAL_ERROR, __FILE__, __LINE__);
                 }
            }
            else
            {
                /*
                 * It is not valid on this node type, so issue an error message
                 */
                log_error(yylineno, message_number,
                    KEYWORDS_lookup_text(AST_attribute_to_token(&current_attribute)));
            }
        }
    }


    /*
     * Issue errors on the bounds attributes if the are illegal here
     */
    if (bounds_illegal)
        for (bp = attributes->bounds; bp; bp = bp->next)
        {
            long    token;
            switch (bp->kind)
            {
                case last_is_k:         token = LAST_IS_KW; break;
                case first_is_k:        token = FIRST_IS_KW; break;
                case max_is_k:          token = MAX_IS_KW; break;
                case length_is_k:       token = LENGTH_IS_KW; break;
                case size_is_k:         token = SIZE_IS_KW; break;
            }
            log_error(yylineno, message_number, KEYWORDS_lookup_text(token)) ;
        }

}

/*---------------------------------------------------------------------*/


/*
 *  A S T _ a t t r i b u t e _ t o _ t o k e n
 *  ===========================================
 *
 *  This routine accepts an attribute value and
 *  returns it's token number.
 *
 *  Inputs:
 *      attribute -- Attribute to return the text of
 *
 *  Returns:
 *      long -- token number for the specified attribute
 */

long AST_attribute_to_token
#ifdef PROTO
(
    ASTP_attr_flag_t    *attribute
)
#else
(attribute)
    ASTP_attr_flag_t    *attribute;
#endif
{
    switch (*attribute)
    {
        case ASTP_ALIGN_SMALL:  return ALIGN_KW;
        case ASTP_ALIGN_SHORT:  return ALIGN_KW;
        case ASTP_ALIGN_LONG:   return ALIGN_KW;
        case ASTP_ALIGN_HYPER:  return ALIGN_KW;
        case ASTP_BROADCAST:    return BROADCAST_KW;
        case ASTP_MAYBE:        return MAYBE_KW;
        case ASTP_IDEMPOTENT:   return IDEMPOTENT_KW;
        case ASTP_IN:           return IN_KW;
        case ASTP_OUT:          return OUT_KW;
        case ASTP_STRING:       return STRING_KW;
        case ASTP_STRING0:      return V1_STRING_KW;
        case ASTP_IN_SHAPE:     return IN_KW;
        case ASTP_OUT_SHAPE:    return OUT_KW;
        case ASTP_UNIQUE:       return UNIQUE_KW;
        case ASTP_PTR:          return PTR_KW;
        case ASTP_REF:          return REF_KW;
        case ASTP_IGNORE:       return IGNORE_KW;
        case ASTP_SMALL:        return V1_ARRAY_KW;
        case ASTP_HANDLE:       return HANDLE_KW;
        case ASTP_CONTEXT:      return CONTEXT_HANDLE_KW;
        case ASTP_TRANSMIT_AS:  return TRANSMIT_AS_KW;
        case ASTP_UNALIGN:      return V1_STRUCT_KW;
        case ASTP_V1_ENUM:      return V1_ENUM_KW;
        default:                return 0;
    }
}

/*---------------------------------------------------------------------*/



/*
 *  A S T _ g e n e r a t e _ n a m e
 *  ===========================================
 *
 *  This routine generates a name from the
 *  the unique id stored in the interface
 *  node and specified suffix.  The name contains
 *  the interface name and version plus a unique
 *  integer.
 *
 *  Inputs:
 *      int_p -- interface node
 *      suffix -- char * to be appended to generated name
 *
 *  Returns:
 *      NAMETABLE_id_t -- name table entry for generated name
 */

NAMETABLE_id_t AST_generate_name
#ifdef PROTO
(
      AST_interface_n_t   *int_p,
      char                *suffix
)
#else
(int_p,suffix)
      AST_interface_n_t   *int_p;
      char                *suffix;
#endif
{
    char        gen_name[MAX_ID+1];  /* Buffer for generated tag name */
    char        *int_name;      /* Interface name */

    /* retrieve the interface name */
    NAMETABLE_id_to_string(int_p->name, &int_name);
    sprintf(gen_name, "%s_v%d_%d_%d%s", int_name, int_p->version%65536,
          int_p->version/65536, int_p->fe_info->gen_index++,suffix);

    if (strlen(gen_name) > MAX_ID)
        message_print(NIDL_NAMETOOLONG, yylineno);

    return NAMETABLE_add_id(gen_name);
}

/*---------------------------------------------------------------------*/

/*
 *  A S T P _ v a l i d a t e _ i n t e g e r
 *  ===========================================
 *
 *  This routine accepts an ASTP expression struct
 *  and coerces it into an integer for use in an
 *  integer constant expression.  If this is not possible,
 *  and error results.
 *
 */

void ASTP_validate_integer
#ifdef PROTO
(
    ASTP_exp_n_t *exp_node
)
#else
(exp_node)
    ASTP_exp_n_t *exp_node;
#endif
{
    /*
     *  If not already a long, then must be a constant node, so attempt to
     *  coerce it to an int.
     */
    if (exp_node->type != AST_int_const_k)
        switch (exp_node->val.other->kind)
        {
            case AST_int_const_k:
                  exp_node->type = AST_int_const_k;
                  exp_node->val.integer = exp_node->val.other->value.int_val;
                  break;
            case AST_char_const_k:
                  exp_node->type = AST_int_const_k;
                  exp_node->val.integer = exp_node->val.other->value.char_val;
                  break;
            default:
                  exp_node->type = AST_int_const_k;
                  exp_node->val.integer = 0;
                  log_error(yylineno,NIDL_NONINTEXP);
                  break;
        }
}
/*---------------------------------------------------------------------*/

/*
 *  A S T P _ v e r i f y _ n o n _ a n o n y m o u s
 *  =================================================
 *
 *  This routine accepts a type and verifies that
 *  it may be pointed-at without the problems of
 *  anonymous structures/unions not being compatible with
 *  similarly defined structures/unions.
 *
 */

static void ASTP_verify_non_anonymous
#ifdef PROTO
(
    AST_type_n_t *type,
    ASTP_declarator_n_t *declarator_ptr
)
#else
(type, declarator_ptr)
    AST_type_n_t *type;
    ASTP_declarator_n_t *declarator_ptr;
#endif
{
    /*
     * Check that the base type is not an anonymous struct/union
     */
    if (!AST_DEF_AS_TAG_SET(type) &&
        ((type->kind == AST_disc_union_k) ||
         (type->kind == AST_structure_k)) &&
        (type->name == NAMETABLE_NIL_ID))
    {
        char *identifier;
        NAMETABLE_id_to_string(declarator_ptr->name, &identifier);
        log_warning(yylineno, NIDL_ANONTYPE, identifier);
    }

}

/*---------------------------------------------------------------------*/
