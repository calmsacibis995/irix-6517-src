/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: nametbl.c,v $
 * Revision 1.1  1993/08/31 06:48:01  jwag
 * Initial revision
 *
 * Revision 1.1.2.3  1993/01/03  21:40:38  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  14:36:55  bbelch]
 *
 * Revision 1.1.2.2  1992/12/23  18:49:34  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:04:12  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:01:40  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      NAMETBL.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Name and scope table management
**
**  VERSION: DCE 1.0
**
*/


#include <nidl.h>
#include <ctype.h>
#include <nidlmsg.h>
#include <errors.h>
#include <nametbl.h>
#include <namtbpvt.h>



/********************************************************************/
/*                                                                  */
/*              Private data declarations.                          */
/*                                                                  */
/********************************************************************/



/*
 * Define the binding level stack.
 *
 * Bindings are associations between a name and an object.
 * The binding stack gets pushed each time a new naming scope is
 * entered, and popped when it is exited.
 * Each entry in the binding level stack points to the list of
 * bindings made at the current level.
 */
#define MAX_LEVELS 20

static  NAMETABLE_binding_n_t * levelStack[MAX_LEVELS]; /* The level stack itself   */

static int  currentLevel;                       /* Current scoping level    */



/*
 * The nametable root is allocated in static storage. Everything else is from
 * the heap. Also declare a cell to keep track of insertions since last
 * balancing.
 */
NAMETABLE_n_t_p NAMETABLE_root;
static long     NAMETABLE_unbalanced;
/*
 * Definitions for the string table. This contains filenames, etc.
 */
#ifndef     STRTAB_SIZE
#define     STRTAB_SIZE     10*1024
#endif


/*
 * The head of the temporary name chain list and the temporary name flag.
 */
NAMETABLE_temp_name_t * NAMETABLE_temp_chain;
static long NAMETABLE_names_are_temporary;



#ifdef MSDOS
    The support for the STRTAB must be fixed for MSDOS to not enter strings more
    than once, and just return the previously allocated index.

    /*
     * Compiling with Microsoft C: use huge modifier to put
     * name and string tables in separate segments
     */
    static char huge STRTAB[STRTAB_SIZE];/* The string table */
    static int  STRTAB_index = 1;       /* Current index... (index 0 is NULL string */
#else

    /* Compiling for UNIX or AEGIS */
#   define nameTable(id,member) ((NAMETABLE_n_t_p)(id))->member
#endif /*MSDOS*/




/******************************************************************************/
/*                                                                            */
/*                           P R I V A T E   I N T E R F A C E S              */
/*                                                                            */
/******************************************************************************/




/*
 * Function:  Find the first set bit in a longword.
 *
 * Inputs:    A long, whose least-significant bit is to be found.
 *
 * Outputs:   The position of the least significant bit.
 *
 * Functional Value: None
 *
 */

#define FIND_FIRST_SET find_first_set

static void find_first_set
#ifdef PROTO
(
    long n_arg,
    long * position
)
#else
(n_arg, position)
    long n_arg;
    long * position;
#endif
{
    long n, m, tp;

    tp = 31;
    n = n_arg;

    if ((m = n & 0X0000FFFF)) {tp = tp - 16; n = m;}
    if ((m = n & 0X00FF00FF)) {tp = tp -  8; n = m;}
    if ((m = n & 0X0F0F0F0F)) {tp = tp -  4; n = m;}
    if ((m = n & 0X33333333)) {tp = tp -  2; n = m;}
    if ((m = n & 0X55555555)) {tp = tp -  1;}

    *position = tp;
}


/*
 * Function:  Balance the nametable tree.
 *
 * Inputs:    None.  (Implicitly, the root of the tree, NAMETABLE_root)
 *
 * Outputs:   None.
 *
 * Functional Value: None
 *
 */

/*
 * The size of the array (balance_arr_size) must be the number of bits in
 * the variable n plus one.
 */

#define balance_arr_size 33
#define NAMETABLE_max_unbalance 10

static void NAMETABLE_balance_tree ()
{
    NAMETABLE_n_t_p  this,
                        temp,
                        back,
                        arr [balance_arr_size];

    long                i,
                        n;


    /*
     * Initialize the array to NULL pointers.
     */
    for (n=0; n<balance_arr_size; n++) {
        arr [n] = NULL;
    };

    n = 0;

    /*
     * Initialize our node pointers.  ("temp" is written before reading.)
     */
    this = NAMETABLE_root;
    back = NULL;


    for (;;) {          /* Termination of loop will be determined below */

        /*
         * This works our way down to the bottom of the left most chain
         * of this subtree, the least valued member.
         */
        for (; this != NULL;) {
            temp = this->left;      /* Save the left pointer. */
            this->left = back;      /* Store the back pointer. */
            if  (back)              /* Maintain the parent pointer */
                back->parent = this;
            back = this;            /* We now have a new back pointer. */
            this = temp;            /* And a new node to visit. */
        };      /* for (; this != NULL;) */


        /*
         * The loop above went one step too far. Back up one.
         * Also, if the result of backing up is NULL, we are done.
         */
        if ((this = back) == NULL)
            break;


        back = this->left;          /* Get a new back pointer */
        ++n;                        /* Processing a new node. */
        FIND_FIRST_SET (n, &i);     /* Find the first set bit */
        arr [i+1] = this;           /* Save the current node  */
        this->left = temp = arr [i];/* New left pointer for this node */
        if (temp)                   /* Maintain the parent pointer */
            temp->parent = this;
        if ((temp = arr [i+2]) != NULL) {
            temp->right = this;     /* Save this node as a right son. */
            if (this)               /* Maintain the parent pointer */
                this->parent = temp;
        }

        /*
         * Done processing this node, clear out the descendants.
         */
        for (; i>0; i--)
            arr [i] = NULL;


        /*
         * Set up for processing the next node.
         */
        temp = this->right;
        this->right = NULL;
        this = temp;

    };          /* for (;;) */


    /*
     * Done with phase 1. Handle the nametable root, then all the
     * remaining nodes are right sons.
     */

    for (i = balance_arr_size -1; (i>0 && (temp = arr [i]) == 0); i--);
    NAMETABLE_root = temp;
    temp->parent = NULL;

    this = temp;

    for (i--; i>0; i--) {
        if ((temp = arr [i]) != 0) {
            this->right = temp;
            temp->parent = this;
            this = temp;
        }
    }      /*for (i; i>0; i--)*/

    /*
     * Now the tree is balanced.
     */
    NAMETABLE_unbalanced = 0;
}


/*
 * Function: Delete a nametable node from the nametable.
 *
 * Inputs: node  - the nametable node to be deleted.
 *
 * Outputs: None.
 *
 * Functional Value: None.
 *
 */

void NAMETABLE_delete_node
#ifdef PROTO
(
    NAMETABLE_id_t node
)
#else
(node)
    NAMETABLE_id_t node;
#endif
{

NAMETABLE_n_t_p n,       /* The node being deleted */
                p,       /* The parent of the node being deleted */
                l,       /* The left child of the node being deleted */
                r,       /* The right child of the node being deleted */
                t;       /* Used to search for an insertion point in the tree */

/*
 * Get local copy of pointer to the node, and pointers to the
 * other nodes of interest.
 */
    n = (NAMETABLE_n_t_p) node;
    p = n->parent;
    l = n->left;
    r = n->right;

/*
 * For this deletion algorithm, we are going to place "r" where "n" is now,
 * and then hang "l" off the end of "r"s left child chain.  That's great, as
 * long as there is an "l" and and "r". If there is no "l", we simply
 * do the "r" operations and don't do the "l" operations. If there is neither
 * an "l" nor an "r", then performing the "r" operations has the correct
 * effect of nulling out "p"s appropriate chain.  The problem comes when there
 * is an "l", but no "r"; this is because the "l" processing uses "r".
 * To handle this problem, we check for it and make "r" be "l", and null out
 * "l".
 */

    if ((l != NULL) && (r == NULL)) {
        r = l;
        l = NULL;
    }

/*
 * Handle the root of the tree specially.  Make the right child the new root.
 */
    if (p == NULL) {
        NAMETABLE_root = r;
        r->parent = NULL;
    } else {

/*
 * Not deleting the root node. Determine if n is lc or rc of p.
 */
        if (p->left == n)
            p->left = r;
        else
            if (p->right == n)
                p->right = r;
            else
/*
 * Parent doesn't point to child.  Corrupted nametable.
 */
                INTERNAL_ERROR ("Corrupted name table");


/*
 * Make r's parent be p (if there is an r!).
 */
        if (r != NULL) r->parent = p;
    }

/*
 * Now r is hooked into the tree in the appropriate place.
 * We need to hang l next.  Find the first null left pointer off of r
 * or r's left children. Hang l there.  Of course, if l is NULL, we are done.
 */
    if (l != NULL) {
        t = r;
        while (t->left != NULL)
        {
            t = t->left;
        }

        t->left = l;
        l->parent = t;
    }

/*
 * At this point, node n has been removed from the tree, and the tree has
 * been reconnected.  Now we need to free any associated binding, and then
 * free n itself.
 *
 * At the time this code is implemented, temporary names are only used
 * by the back end, and the back end does not associate bindings with them.
 * Therefore, simply bugcheck if there is a binding associated with this
 * nametable entry.
 */
    if (n->bindings != NULL)
        INTERNAL_ERROR ("Binding associated with temporary name.");

/*
 * Free the nametable node. This frees the id string storage as well,
 * since they were allocated as one chunk.
 */
    FREE (n);

} /* End of NAMETABLE_delete_node */





/******************************************************************************/
/*                                                                            */
/*                           P U B L I C   I N T E R F A C E S                */
/*                                                                            */
/******************************************************************************/


/*
 * Function:  add an identifier to the id table.
 *
 * Inputs:    id     - The character string to be added.
 *
 * Outputs:
 *
 * Functional Value: a handle which can be used to refer to the
 *                   stored identifier.
 *
 */

NAMETABLE_id_t NAMETABLE_add_id
#ifdef PROTO
(
    char * id
)
#else
(id)
    char   *id;
#endif
{
    NAMETABLE_n_t_p     this,
                        parent,
                        np,
                      * insert_point;
    NAMETABLE_temp_name_t * new_temp_name_block;
    int                 i;
    char              * cp;


    if (NAMETABLE_root == NULL) {
        /* The first entry in the name table. */
        insert_point = &NAMETABLE_root;
        parent = NULL;

    } else {
        for (this = NAMETABLE_root; this; ) {
            if ((i = strcmp (id, this->id)) == 0) {
                /* We have a match. Return with this node id. */
                return (NAMETABLE_id_t) this;
            }; /* if strcmp */

            /* No match, which subtree to use? */
            if (i > 0) {
                /*
                 * The id is greater than that of this node.
                 * Use the right subtree.
                 */
                if (this->right == NULL) {
                    insert_point = &this->right;
                    parent = this;
                }
                this = this->right;
            } else {
                /*
                 * The id is less than that of this node.
                 * Use the left subtree.
                 */
                if (this->left == NULL) {
                    insert_point = &this->left;
                    parent = this;
                }
                this = this->left;
            }; /* if (i > 0) */

        }; /* for */
    };  /* NAMETABLE_root == NULL */


    /*
     * At this point we have either returned with a match, or
     * have set up the pointer variable insert_point.
     * We need to allocate a new node and initialize it, hooking
     * it into the tree.
     */

    this = (NAMETABLE_n_t_p) MALLOC (sizeof (NAMETABLE_n_t) +
                                                strlen (id) + 1);

    /* Get a pointer to the buffer after the nametable node. */
    np = this;
    cp = (char *) ++np;

    /* Copy the string into the buffer. */
    strcpy (cp, id);

    /* Initialize the nametable node. */
    this -> left = NULL;
    this -> right = NULL;
    this -> id = cp;
    this -> bindings = NULL;
    this -> tagBinding = NULL;

    /* Link it into the nametable. */
    *insert_point = this;
    this->parent = parent;

    /* If a temporary node, record it. */
    if (NAMETABLE_names_are_temporary) {
        new_temp_name_block = (NAMETABLE_temp_name_t*)MALLOC (sizeof (NAMETABLE_temp_name_t));
        new_temp_name_block->next = NAMETABLE_temp_chain;
        new_temp_name_block->node = this;
        NAMETABLE_temp_chain = new_temp_name_block;
    }

    /* Indicate that we are getting unbalanced; balance tree if necessary*/
    ++NAMETABLE_unbalanced;
    if (NAMETABLE_unbalanced > NAMETABLE_max_unbalance)
        NAMETABLE_balance_tree();


    /* Return the id (the address of) the new node. */
    return (NAMETABLE_id_t) this;

}


/****************************************************************************/

/*
 * Function:  Check if an identifier exists.
 *
 * Inputs:    id     - The character string to check for.
 *
 * Outputs:
 *
 * Functional Value: a handle which is used to refer to the
 *                   stored identifier.
 *
 */

NAMETABLE_id_t NAMETABLE_lookup_id
#ifdef PROTO
(
    char * id
)
#else
(id)
    char   *id;
#endif
{
    NAMETABLE_n_t_p     this;
    int                 i;

    if (NAMETABLE_root != NULL) {
        for (this = NAMETABLE_root; this; ) {
            if ((i = strcmp (id, this->id)) == 0) {
                /* We have a match. Return with this node id. */
                return (NAMETABLE_id_t) this;
            }; /* if strcmp */

            /* No match, which subtree to use? */
            if (i > 0) {
                /* id is greater than this node. Use right subtree. */
                this = this->right;
            } else {
                /* id is less than this node. Use the left subtree. */
                this = this->left;
            }; /* if (i > 0) */
        }; /* for */
    };  /* NAMETABLE_root != NULL */

    return NAMETABLE_NIL_ID;
}
/*--------------------------------------------------------------------*/

/*
 * Function:  Converts an namtable identifier back to the original string.
 *
 * Inputs:    id      - The handle returned by NAMTABLE_add_id
 *
 * Outputs:   str_ptr - The character string is returned through here.
 *
 * Functional Value: void
 *
 * Notes:
 *
 */

void NAMETABLE_id_to_string
#ifdef PROTO
(
    NAMETABLE_id_t NAMETABLE_id,
    char ** str_ptr
)
#else
(NAMETABLE_id, str_ptr)
    NAMETABLE_id_t NAMETABLE_id;
    char  **str_ptr;
#endif
{
    NAMETABLE_n_t_p id;


    if (NAMETABLE_id == NAMETABLE_NIL_ID)
        *str_ptr = "";
    else {
        id = (NAMETABLE_n_t_p) NAMETABLE_id;
        *str_ptr = nameTable(id,id);
    };
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Binds a symbol to a value.
 *
 * Inputs:    id      - The handle returned by NAMETABLE_add_id
 *
 *            binding - a pointer representing the binding
 *
 * Outputs:
 *
 * Functional Value: Boolean true if binding completed successfully.
 *                           false if name already bound.
 *
 * Notes:     For each symbol in the name table, there is a binding
 *            stack.  Conceptually the stack gets pushed every time
 *            a new scope is entered, and popped on scope exit.  The
 *            symbol table is "shallowly bound" rather than
 *            "deeply bound' in the lisp since.  This makes lookup
 *            fast, but pushing and popping slow.
 *
 */

boolean NAMETABLE_add_binding
#ifdef PROTO
(
    NAMETABLE_id_t id,
    char * binding
)
#else
(id, binding)
    NAMETABLE_id_t id;
    char   *binding;
#endif
{
    NAMETABLE_n_t_p     idP;

    NAMETABLE_binding_n_t * bindingP;
    NAMETABLE_binding_n_t * newBindingP;

    /* Get a local handle on the nametable entry. */
    idP = (NAMETABLE_n_t_p) id;

    if (!idP->bindings) {
        /*
         *  First binding for this identifier.
         *
         */

        /* Allocate and initialize the new binding. */
        bindingP = (NAMETABLE_binding_n_t *) MALLOC (sizeof (NAMETABLE_binding_n_t));
        bindingP->oldBinding = NULL;

    } else {

        /* Get a handle on the binding chain. */
        bindingP = idP->bindings;

        /* Fail if we already have a binding at this level. */
        if (bindingP->bindingLevel == currentLevel)
                return false;


        /* Allocate and init this binding, making it the chain head. */
        newBindingP = (NAMETABLE_binding_n_t *) MALLOC (sizeof (NAMETABLE_binding_n_t));
        newBindingP->oldBinding = bindingP;
        bindingP = newBindingP;
    };

    /* Tie in the actual supplied binding. */
    bindingP->theBinding = binding;

    /* Mark the level, tie it into the level chain. */
    bindingP->bindingLevel = currentLevel;
    bindingP->nextBindingThisLevel = levelStack[currentLevel];
    levelStack[currentLevel] = bindingP;

    /* Create the back link to the name table and hook it into the id. */
    bindingP->boundBy = idP;
    idP->bindings = bindingP;

    return true;
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Returns the binding associated with the id
 *
 * Inputs:    id      - The handle returned by NAMETABLE_add_id
 *
 *
 * Outputs:
 *
 * Functional Value:  - A pointer to the binding if there is one,
 *                      0 otherwise.
 *
 */

char   *NAMETABLE_lookup_binding
#ifdef PROTO
(
    NAMETABLE_id_t identifier
)
#else
(identifier)
    NAMETABLE_id_t identifier;
#endif
{
    if (identifier == NAMETABLE_NIL_ID)
        return NULL;

    if (nameTable(identifier,bindings) == NULL) {
        return NULL;
        };

    return (char *) (nameTable(identifier,bindings)->theBinding);
}

/*--------------------------------------------------------------------*/
/*
 * Function:  Binds a tag symbol to a value.
 *
 * Inputs:    id      - The handle returned by NAMETABLE_add_id
 *
 *            binding - a pointer representing the binding
 *
 * Outputs:
 *
 * Functional Value: Boolean true if binding completed successfully.
 *                           false if name already bound.
 *
 * Notes:     For each symbol in the name table, there is a binding
 *            stack.  Conceptually the stack gets pushed every time
 *            a new scope is entered, and popped on scope exit.  The
 *            symbol table is "shallowly bound" rather than
 *            "deeply bound' in the lisp since.  This makes lookup
 *            fast, but pushing and popping slow.
 *
 */

boolean NAMETABLE_add_tag_binding
#ifdef PROTO
(
    NAMETABLE_id_t id,
    char * binding
)
#else
(id, binding)
    NAMETABLE_id_t id;
    char   *binding;
#endif
{
    NAMETABLE_n_t_p     idP;

    NAMETABLE_binding_n_t * bindingP;
    NAMETABLE_binding_n_t * newBindingP;

    /* Get a local handle on the nametable entry. */
    idP = (NAMETABLE_n_t_p) id;

    if (!idP->tagBinding) {
        /*
         *  First binding for this identifier.
         *
         */

        /* Allocate and initialize the new binding. */
        bindingP = (NAMETABLE_binding_n_t *) MALLOC (sizeof (NAMETABLE_binding_n_t));
        bindingP->oldBinding = NULL;

    } else {

        /* Get a handle on the binding chain. */
        bindingP = idP->tagBinding;

        /* Fail if we already have a binding at this level. */
        if (bindingP->bindingLevel == currentLevel)
                return false;


        /* Allocate and init this binding, making it the chain head. */
        newBindingP = (NAMETABLE_binding_n_t *) MALLOC (sizeof (NAMETABLE_binding_n_t));
        newBindingP->oldBinding = bindingP;
        bindingP = newBindingP;
    };

    /* Tie in the actual supplied binding. */
    bindingP->theBinding = binding;

    /* Create the back link to the name table and hook it into the id. */
    bindingP->boundBy = idP;
    idP->tagBinding = bindingP;

    return true;
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Returns the binding associated with the tag id
 *
 * Inputs:    id      - The handle returned by NAMETABLE_add_id
 *
 *
 * Outputs:
 *
 * Functional Value:  - A pointer to the binding if there is one,
 *                      0 otherwise.
 *
 */

char   *NAMETABLE_lookup_tag_binding
#ifdef PROTO
(
    NAMETABLE_id_t identifier
)
#else
(identifier)
    NAMETABLE_id_t identifier;
#endif
{
    if (identifier == NAMETABLE_NIL_ID)
        return NULL;

    if (nameTable(identifier,tagBinding) == NULL) {
        return NULL;
        };

    return (char *) (nameTable(identifier,tagBinding)->theBinding);
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Returns the binding associated with the id.  The binding must
 *            be within the local scope.
 *
 * Inputs:    id      - The handle returned by NAMETABLE_add_id
 *
 *
 * Outputs:
 *
 * Functional Value:  - A pointer to the binding if there is one,
 *                      0 otherwise.
 *
 */

char   *NAMETABLE_lookup_local
#ifdef PROTO
(
    NAMETABLE_id_t identifier
)
#else
(identifier)
    NAMETABLE_id_t identifier;
#endif
{
    if (identifier == NAMETABLE_NIL_ID)
        return NULL;

    if (nameTable(identifier,bindings) == NULL){
        return NULL;
        };

    if (nameTable(identifier,bindings)->bindingLevel < currentLevel) {
        return NULL;
        };

    return (char *) (nameTable(identifier,bindings)->theBinding);
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Pushes a new scoping level.
 *
 * Inputs:
 *
 * Outputs:
 *
 * Functional Value: void
 *
 * Notes:    Aborts with an error if too many levels are pushed.
 *
 */

void NAMETABLE_push_level () {
    if (currentLevel < MAX_LEVELS)
        levelStack[++currentLevel] = NULL;
    else
        error(NIDL_SCOPELVLS);
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Pops a scoping level.
 *
 * Inputs:
 *
 * Outputs:
 *
 * Functional Value: void
 *
 * Notes:    Aborts with an error if there are no more levels.
 *
 */

void NAMETABLE_pop_level () {
    NAMETABLE_binding_n_t * bp;
    NAMETABLE_binding_n_t * obp;
    NAMETABLE_n_t_p hsp;

    if (currentLevel == 0)
            INTERNAL_ERROR ("Attempt to pop symbol table too many times");

    bp = levelStack[currentLevel--];

    while (bp) {
            hsp = bp->boundBy;
            hsp->bindings = (struct NAMETABLE_binding_n_t  *) bp->oldBinding;
            obp = bp;
            bp = obp->nextBindingThisLevel;
            FREE ((char *)obp);
    }
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Enters a string into the string table
 *
 * Inputs:
 *
 * Outputs:
 *
 * Functional Value: The STRTAB identifier for this string.
 *
 * Notes:
 *
 */

STRTAB_str_t STRTAB_add_string
#ifdef PROTO
(
    char * string
)
#else
(string)
    char   *string;
#endif
{
#ifdef MSDOS
    int     string_handle;

    strcpy (&STRTAB[STRTAB_index], string);
    string_handle = STRTAB_index;
    STRTAB_index += strlen (string) + 1;
    return string_handle;
#else
    return (STRTAB_str_t)NAMETABLE_add_id(string);
#endif
}

/*--------------------------------------------------------------------*/

/*
 * Function:  Given a string ID, returns the corresponding string
 *
 * Inputs:    id - a previously obtained string id
 *
 * Outputs:   strp - a pointer to corresponding string is return here.
 *
 * Functional Value: void
 *
 * Notes:
 *
 */

void STRTAB_str_to_string
#ifdef PROTO
(
    STRTAB_str_t str,
    char ** strp
)
#else
(str, strp)
    STRTAB_str_t str;
    char  **strp;
#endif
{
#ifdef MSDOS
    *strp = &STRTAB[str];
#else
    NAMETABLE_id_to_string((NAMETABLE_id_t)str, strp);
#endif
}

/*--------------------------------------------------------------------*/

/*
 * Function: Initialize the name table.
 *
 * Inputs:
 *
 *
 * Outputs:
 *
 * Functional Value:
 *
 * Notes:
 *
 */

#ifdef MSDOS
#ifdef TURBOC
static hash_slot huge *nameTablePtr(i)
int i;
{
    char huge *tmp;
    hash_slot huge *return_val;
    long offset;
    tmp = (char *) nameTableData;
    offset = (long) sizeof(hash_slot)  * (long)i;
    tmp += offset;
    return_val = (hash_slot *)tmp;
    return (return_val);
}
#endif
#endif

void NAMETABLE_init ()
{
#ifdef MSDOS
#ifdef TURBOC
    {

This still needs more work. I do not understand the TURBO-C stuff yet.

            void far *farcalloc();
            nameTableData = farcalloc((unsigned long)NAMETABLE_SIZE,
                                  (unsigned long) sizeof(hash_slot));
            if(!nameTableData)
                    error(NIDL_ALLOCNAMTBL);
    }
#endif
#endif

    NAMETABLE_root = NULL;
    NAMETABLE_temp_chain = NULL;
    NAMETABLE_unbalanced = 0;
    NAMETABLE_names_are_temporary = FALSE;

    /*
     * Initialize the binding level stack pointer
     */
    currentLevel = 0;

}


/*--------------------------------------------------------------------*/

/*
 *
 * Function:    Initializes the string table.
 *
 * Inputs:
 *
 * Outputs:
 *
 * Notes:       The string table is a fixed size
 *
 */

void STRTAB_init () {
#ifdef MSDOS
        STRTAB_index = 0;
#endif
}

/*--------------------------------------------------------------------*/

/*
 *
 * Function:    Given a nametable id and a string, creates a new
 *              nametable entry with the name dentoed by the id
 *              concatenated with the given string.
 *
 * Inputs:      id - a nametable id.
 *              suffix - a string to be added to the name.
 *
 * Outputs:
 *
 * Functional value: the new nametable id.
 *
 */

NAMETABLE_id_t NAMETABLE_add_derived_name
#ifdef PROTO
(
    NAMETABLE_id_t identifier,
    char * matrix
)
#else
(identifier, matrix)
    NAMETABLE_id_t identifier;
    char   *matrix;
#endif
{
    char    new_name[max_string_len];
    char   *old_name_p;

    NAMETABLE_id_to_string (identifier, &old_name_p);
    sprintf (new_name, matrix, old_name_p);

    return NAMETABLE_add_id (new_name);
}

NAMETABLE_id_t NAMETABLE_add_derived_name2
#ifdef PROTO
(
    NAMETABLE_id_t identifier1,
    NAMETABLE_id_t identifier2,
    char * matrix
)
#else
(identifier1, identifier2, matrix)
    NAMETABLE_id_t identifier1, identifier2;
    char   *matrix;
#endif
{
    char    new_name[max_string_len];
    char   *old_name1_p,
           *old_name2_p;
    NAMETABLE_n_t_p id1, id2;

    id1 = (NAMETABLE_n_t_p) identifier1;
    id2 = (NAMETABLE_n_t_p) identifier2;


    NAMETABLE_id_to_string ((NAMETABLE_id_t)id1, &old_name1_p);
    NAMETABLE_id_to_string ((NAMETABLE_id_t)id2, &old_name2_p);
    sprintf (new_name, matrix, old_name1_p, old_name2_p);

    return NAMETABLE_add_id (new_name);
}


void NAMETABLE_set_temp_name_mode
#ifdef PROTO
(
    void
)
#else
()
#endif
{
/*
 * Bugcheck if already in temporary mode.
 */
    if (NAMETABLE_names_are_temporary)
        INTERNAL_ERROR ("Recursive entry to temp name mode");

/*
 * Set temporary mode.
 */
    NAMETABLE_names_are_temporary = TRUE;
}


void NAMETABLE_set_perm_name_mode
#ifdef PROTO
(
    void
)
#else
()
#endif
{
/*
 * Clear temporary mode.
 */
    NAMETABLE_names_are_temporary = FALSE;
}


void NAMETABLE_clear_temp_name_mode
#ifdef PROTO
(
    void
)
#else
()
#endif
{
    NAMETABLE_temp_name_t * this,
                          * next;

/*
 * Bugcheck if not in temporary mode.
 */
    if (!NAMETABLE_names_are_temporary)
        INTERNAL_ERROR ("Not in temp name mode");

/*
 * Walk the list of temp name blocks, freeing the name and then the block.
 */
    for (this = NAMETABLE_temp_chain; this != NULL; ) {
        NAMETABLE_delete_node ((NAMETABLE_id_t)this->node);
        next = this->next;
        FREE (this);
        this = next;
    }

/*
 * Balance the nametable after all these deletions.
 */
    NAMETABLE_balance_tree();

/*
 * Clear the temporary flag and the chain head.
 */
    NAMETABLE_names_are_temporary = FALSE;
    NAMETABLE_temp_chain = NULL;
}
