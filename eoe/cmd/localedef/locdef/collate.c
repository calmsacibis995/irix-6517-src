//____________________________________________________________________________
//
// Copyright (C) 1995 Silicon Graphics, Inc
//
// All Rights Reserved Worldwide.
//
//____________________________________________________________________________
//
// FILE NAME :          collate.c
//
// DESCRIPTION :        functions for handling collation table information
//
//____________________________________________________________________________

#include "collate.h"

#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>

#include "localedef.h"
#include "tables.h"
 
/* ************** *
 * DECLARATIONS   *
 * ************** */

#define BUFSIZE 256

/* Collation table *
 * *************** */

typedef enum { e_collsym, e_collelt, e_char } coll_type_t;

typedef struct collid_t {

    encs_t	* collid;
    encs_t	* weights [ COLL_WEIGHTS_MAX ];
    coll_type_t	type;

    /* Managed by list functions */
    int		pos;
    struct collid_t * next;
} collid_t;

typedef struct {
    collid_t *	head;
    collid_t *	tail;
} coll_list_t;

static coll_list_t coll_head = { NULL, NULL };

void		coll_append_list ( coll_list_t * list, collid_t * element );
collid_t *	coll_find ( coll_list_t * list, encs_t * from );


collorder_t	collorder [ COLL_WEIGHTS_MAX ] = { e_forward };
int		collorder_len = 1;

/* Collation primary table
   Data structure used to create the input to colltbl
   */
struct cotable_t;
typedef struct
{
    collid_t *		primary;

    struct cotable_t *	next;
} coprim_t;

/* Compare based on the collation entry list */
int coprim_cmp ( const void *, const void * );

typedef struct cotable_t {
    coprim_t *	table;
    int		len;
    int		size;
} cotable_t;

static cotable_t cotable;

cotable_t *	cotable_new ( cotable_t * this );
cotable_t *	cotable_append ( cotable_t * this, collid_t * primary, collid_t * entry );
int		cotable_build ();

/* Substitution list *
 * ***************** */

typedef struct sub_t {
    encs_t *		from;
    encs_t *		to;

    struct sub_t *	next;
} sub_t;

static sub_t	* sub_list = NULL;

void sub_list_add ( encs_t * from, encs_t * to );


/* ************** *
 * IMPLEMENTATION *
 * ************** */

void coll_add_collsym ( const char * symbol )
{
    symbol_ptr n;
    collid_t * collid;

    /* Find symbol */
    if ( ! ( n = search_list ( collsym_list, (char*)symbol ) ) )
    {
	fprintf ( stderr, GETMSG ( MSGID_UNKNOWN_SYM ), symbol );
	return;
    }
    /* Add it to the list */
    if ( ! ( collid = malloc ( sizeof ( collid_t ) ) ) )
	exit_malloc_error ();

    collid->collid = n->encoding;
    collid->weights[0] = NULL;
    collid->type = e_collsym;

    coll_append_list ( & coll_head, collid );

    return;
}

void coll_add_collid ( const char *element, encs_t *weights[], int nb_weights )
{
    symbol_ptr n;
    collid_t * collid;
    int i;

    /* Find out encoding */
    if ( ( ( n = search_list ( charmap_list, (char *) element ) ) == NULL ) &&
	 ( ( n = search_list ( collelt_list, (char *) element ) ) == NULL ) )
    {
	fprintf ( stderr, GETMSG ( MSGID_UNKNOWN_SYM ), element );
	return;
    }

    /* Create a new element */
    if ( ! ( collid = malloc ( sizeof ( collid_t ) ) ) )
	exit_malloc_error ();

    collid->collid = n->encoding;

    /* Convert the weights */
    for ( i = 0; i < collorder_len ; i ++ )
	if ( i < nb_weights )
	    collid->weights[i] = weights[i];
	else
	    /* No weights have been defined */
	    collid->weights[i] = n->encoding;
    collid->type = e_char;
    
    /* Add it to the list */
    coll_append_list ( & coll_head, collid );

    return;
}

void coll_append_list ( coll_list_t * list, collid_t * element )
{
    element->next = NULL;
    if ( ! list->head )
    {
	list->head = element;
	element->pos = 0;
    }
    else
    {
	list->tail->next = element;
	element->pos = list->tail->pos + 1;
    }
    list->tail = element;
}

collid_t * coll_find ( coll_list_t * list, encs_t * from )
{
    collid_t * n;

    for ( n = list->head; n; n = n->next )
	if ( encs_compare ( n->collid, from ) )
	    return n;

    return NULL;
}


int
coprim_cmp ( const void *a, const void * b)
{
    int ai = ( ( coprim_t * ) a)->primary->pos;
    int bi = ( ( coprim_t * ) b)->primary->pos;

    return (
	( ai == bi ) ? 0 :
	( ai < bi ) ? -1 : 1
	);

}

#define COPRIM_INIT_SIZE	64
cotable_t * cotable_new ( cotable_t * this )
{
    if ( !this )
	this = malloc ( sizeof ( cotable_t ) );

    this->table = malloc ( COPRIM_INIT_SIZE * sizeof ( this->table[0] ) );
    this->len = 0;
    this->size = COPRIM_INIT_SIZE;

    return this;
}

void dump_colltable();

/* Returns true if there is something to output */
int
cotable_build ()
{
    collid_t * p, * q;

    cotable_new ( & cotable );

    /* Go through all collating entries */
    for ( p = coll_head.head ; p ; p = p->next )
    {
	/* Find the characters and collating elements */
	if ( p->type == e_char || p->type == e_collelt )
	{
	    if ( p->weights[0]->enc[0] == ENC_IGNORE )
		continue;

	    /* Is it a candidate for subsitutiton */
	    if ( p->weights[0]->is_string )
		sub_list_add ( p->collid, p->weights[0] );
	    else
	    {
		/* Find the collid of the weight */
		if ( q = coll_find ( & coll_head, p->weights[0] ) )
		    cotable_append ( & cotable, q, p );
		else
		    fprintf ( stderr, "Internal error: Unknown collation weight\n" );
	    }
	}
    }

    /* Sort tables based on collation entry*/
    qsort ( cotable.table, cotable.len, sizeof ( cotable.table[0] ), coprim_cmp );

    /* Return true if there is some content */
    return ( cotable.len > 0 );
}

void	cotable_output_header ( FILE * f );
void	cotable_output_order ( FILE * f );
void	cotable_output_sub ( FILE * f );

void
cotable_output ( FILE * f )
{
    cotable_output_header ( f );
    cotable_output_order ( f );
    cotable_output_sub ( f );
}

void
cotable_output_header ( FILE * f )
{
    fprintf ( f,
	      "# Collation table generated by localedef for locale %s\n"
	      "\n"
	      "codeset LC_COLLATE\n\n",
	      locale_name );
}

void
cotable_output_order ( FILE * f )
{
    int i = 0;
    int j = 0;
    cotable_t * sectable;
    char buf [ 64 ];

    fprintf ( f, "order is \\\n\t" );

    for ( i = 0; i < cotable.len ; i ++ )
    {
	sectable = (cotable_t *) cotable.table[i].next;
	if ( ! sectable )
	    continue;

	if ( sectable->len == 1 )
	    fprintf ( f, "%s", 
		      convert_encs_to_string ( sectable->table[0].primary->collid,
					       "\\x%2x", buf ) );
	else
	{
	    fputs ( "(", f );
	    for ( j = 0; j < sectable->len ; j ++ )
	    {
		fprintf ( f, "%s",
			  convert_encs_to_string ( sectable->table[j].primary->collid,
						   "\\x%2x", buf ) );

		if ( j < sectable->len - 1 )
		    fputs ( ";", f );
	    }
	    fputs ( ")", f );
	}
	if ( i < cotable.len - 1 )
	    fputs ( "; \\\n\t", f );
    }
    fputs ( "\n\n", f );
}

void
cotable_output_sub ( FILE * f )
{
    char buf [ BUFSIZE ];
    sub_t * p;

    fprintf ( f, "# Substituion list\n\n" );

    for ( p = sub_list; p ; p = p->next )
    {
	/* Warning: characters in substituion 
	   string have to be in octal form */
	convert_encs_to_string ( p->from, "\\%o", buf );
	fprintf ( f, "substitute \"%s\"", buf );
	
	convert_encs_to_string ( p->to, "\\%o", buf );
	fprintf ( f, " with \"%s\"\n", buf );
    }
    fputs ( "\n", f );
}

/* Debug output of collation table */
void
dump_colltable()
{
    int i = 0;
    int j = 0;
    cotable_t * sectable;
    char buf [ BUFSIZE ];

    printf ( "Collation Table dump\n" );
    for ( i = 0; i < cotable.len ; i ++ )
    {
	printf ( "Primary weight: %s\n",
		 convert_encs_to_string (
		     cotable.table[i].primary->collid, "0x%02x (%1$c)", buf ) );

	sectable = (cotable_t *) cotable.table[i].next;
	if ( ! sectable )
	    printf ( "\tNo secondary elements\n" );
	else
	    for ( j = 0; j < sectable->len ; j ++ )
		printf ( "\tSecondary element: %s\n",
			 convert_encs_to_string ( 
			     sectable->table[j].primary->collid, "0x%02x (%1$c)", buf ) );
    }
}

cotable_t *
cotable_append ( cotable_t * this, collid_t * primary, collid_t * entry )
{
    int i;
    if ( ! this )
	this = cotable_new ( this );

    /* Find existing entry with same primary weight */
    for ( i = 0; i < this->len; i ++ )
	if ( encs_compare ( this->table[i].primary->collid, primary->collid ) )
	    break;

    if ( i == this->len )
    {
	/* New entry in the table */
	if ( this->len >= this->size )
	{
	    this->size += COPRIM_INIT_SIZE;
	    this->table = realloc ( this->table, this->size * sizeof ( this->table[0] ) );

	    if ( ! this->table )
		exit_malloc_error ();
	}
	this->table [ this->len ].primary = primary;
	this->table [ this->len ].next = NULL;
	this->len ++; 
    }

    if ( entry )
	this->table [ i ].next = cotable_append ( (this->table [ i ].next), entry, 0 );

    return this;
}

void
sub_list_add ( encs_t * from, encs_t * to )
{
    sub_t * new = malloc ( sizeof ( sub_t ) );    

    if ( ! new )
	exit_malloc_error ();

    new->from = from;
    new->to = to;

    new->next = sub_list;
    sub_list = new;
}
