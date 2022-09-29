%{
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SMI Parser Generator
 *
 *	$Revision: 1.6 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/**** 
       This works as of 06/15.
       Putting the additional grammar for typedefs and enumerated types.
       
****/

#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>

#include "parser.h"

#define  MIBS "/usr/lib/netvis/mibs"
#define  MIB2 "/usr/lib/netvis/mibs/mib2"

static void yyerror(char*);
static void AddNode(struct mibNode *);
void        AddEnumList(struct enumList *);
void        AddTypeList(struct typeInfo *);

/* static variable to store the node info. temporarily.... */

static struct mibNode *node;
struct mibNode *mib;
char   ErrorMessage[MAXERRBUF];
char   mibNow[200];

struct enumList *list_head     = NULL;
struct enumList *listNode;

struct typeInfo *typelist_head = NULL;
struct typeInfo *typeNode;


static char parentName[100];

static char *index_name[] = {   "abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz",
				"abcdefghijklmnopqrstuvwxyz"
			    };


static int index_count = 0;
static int index_hit = 0;

int lineNumber;
%}

%union {
	int  integer;
	char *string;
      }
%token NAME ID NUMBER CCEQUALS LBRACE RBRACE LPAREN RPAREN COMMA DOTDOT
	OBJECTID OBJECT_TYPE
	SYNTAX SEQUENCE OF INTEGER OCTET_STRING DISPLAYSTRING
		IPADDRESS COUNTER GAUGE TIMETICKS
	ACCESS READ_ONLY READ_WRITE WRITE_ONLY NOT_ACCESSIBLE
	STATUS MANDATORY OPTIONAL OBSOLETE DEFINITIONS BEGIN1 SIZE DEPRECATED
	DESCRIPTION QSTRING INDEX DEFVAL

%type <integer> NUMBER SYNTAX_TYPE LBRACE ACCESS_TYPE READ_ONLY READ_WRITE
		WRITE_ONLY NOT_ACCESSIBLE 
%type <string>  NAME ID NAMELIST
%type <string>  QSTRING QUOTED_STRING

%start MIB

%%

MIB	: NODE
	{
		if ( mib != NULL )

			AddNode(node);

		else
			mib = node;
		
	}

	| MIB NODE
	{
		AddNode(node); /* adds the node as child of */
			       /* parentName                */
	}

	| MIB SEQDEF
	{
		/* printf("discarding a sequence\n"); */
	}

	| DEFS
	{
		/* printf("discarding definition and begin\n"); */
	}

	| TYPEDEFINITIONS
	{
		/* printf("reading typedefs....\n"); */
	}
	| MIB TYPEDEFINITIONS
	{
		/* printf("reading typedefs again..\n"); */
	}
	;

NODE	: NAME OBJECTID CCEQUALS LBRACE NUMBER RBRACE 
	{
		node = (struct mibNode *)malloc(sizeof(struct mibNode));
		strcpy(node->name, $1); 
		node->subid = $5; 

		node->parent  = NULL; /* This is THE ROOT! */
		node->child   = NULL;
		node->sibling = NULL;
		node->enum_list = NULL;

		node->group    = NULL;
		node->describe = NULL;
	}

	| NAME OBJECTID CCEQUALS LBRACE NAME NUMBER RBRACE
	{
                node = (struct mibNode *)malloc(sizeof(struct mibNode));
                strcpy(node->name, $1); 
                node->subid = $6;
		strcpy (node->parentname, $5); 

		node->enum_list = NULL;
		node->child   = NULL;
		node->sibling = NULL;

		node->group    = NULL;
		node->describe = NULL;


	}

	| NAME OBJECT CCEQUALS LBRACE NAME NUMBER RBRACE
	{

                strcpy(node->name, $1); 
                node->subid = $6;
		strcpy(node->parentname, $5); 

		if ( index_hit )

		     	node->index = isnodeindex($1);
		else

			node->index   = 0;

	 	index_count = 0; /* initialization for the next table */
		node->child   = NULL;
		node->sibling = NULL;

		node->group    = NULL;
		node->describe = NULL;

	}

	;

OBJECT  : OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM 
		ACCESS ACCESS_TYPE STATUS STATUS_TYPE 

	/* This is for Cabletron */

	{
                node = (struct mibNode *)malloc(sizeof(struct mibNode));
                if (!node)
                   yyerror("Could not allocate memory...\n");

                node->syntax = $3;
                if ( node->syntax == TABLE_TYPE )
                        node->table = TRUE;
                else
                        node->table = FALSE;

		node->enum_list = list_head;
		list_head = NULL;

                node->access = $6;

		node->group    = NULL;
		node->describe = NULL;

		
	}
	| OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM
		ACCESS ACCESS_TYPE STATUS STATUS_TYPE DESCRIPTION QUOTED_STRING
	{

		node = (struct mibNode *)malloc(sizeof(struct mibNode));
		if (!node)
                   yyerror("Could not allocate memory...\n");
		
		node->syntax = $3;
		if ( node->syntax == TABLE_TYPE )
		 	node->table = TRUE;
		else
			node->table = FALSE;

                node->enum_list = list_head;
                list_head = NULL;

		strcpy(node->descr, $10); 
		node->access = $6;

		node->group    = NULL;
		node->describe = NULL;
	}
        | OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM
                ACCESS ACCESS_TYPE STATUS STATUS_TYPE DESCRIPTION QUOTED_STRING
		DEFVAL LBRACE NUMBER RBRACE
        {

                node = (struct mibNode *)malloc(sizeof(struct mibNode));
                if (!node)
                   yyerror("Could not allocate memory...\n");

                node->syntax = $3;
                if ( node->syntax == TABLE_TYPE )
                        node->table = TRUE;
                else
                        node->table = FALSE;

                node->enum_list = list_head;
                list_head = NULL;

                strcpy(node->descr, $10);
                node->access = $6;

                node->group    = NULL;
                node->describe = NULL;
        }
        | OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM
                ACCESS ACCESS_TYPE STATUS STATUS_TYPE DESCRIPTION QUOTED_STRING
		DEFVAL LBRACE NAME RBRACE
        {

                node = (struct mibNode *)malloc(sizeof(struct mibNode));
                if (!node)
                   yyerror("Could not allocate memory...\n");

                node->syntax = $3;
                if ( node->syntax == TABLE_TYPE )
                        node->table = TRUE;
                else
                        node->table = FALSE;

                node->enum_list = list_head;
                list_head = NULL;

                strcpy(node->descr, $10);
                node->access = $6;

                node->group    = NULL;
                node->describe = NULL;
        }
        | OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM
                ACCESS ACCESS_TYPE STATUS STATUS_TYPE DESCRIPTION QUOTED_STRING
                DEFVAL LBRACE QUOTED_STRING RBRACE
        {

                node = (struct mibNode *)malloc(sizeof(struct mibNode));
                if (!node)
                   yyerror("Could not allocate memory...\n");

                node->syntax = $3;
                if ( node->syntax == TABLE_TYPE )
                        node->table = TRUE;
                else
                        node->table = FALSE;

                node->enum_list = list_head;
                list_head = NULL;

                strcpy(node->descr, $10);
                node->access = $6;

                node->group    = NULL;
                node->describe = NULL;
        }
	| OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM ACCESS ACCESS_TYPE STATUS
	  STATUS_TYPE DESCRIPTION QUOTED_STRING TABLE_INDEX
	{
	
                node = (struct mibNode *)malloc(sizeof(struct mibNode));

                if (!node)
                   yyerror("Could not allocate memory...\n");

                node->syntax = $3;
                strcpy(node->descr, $10);
                node->access = $6;

                node->enum_list = list_head;
                list_head = NULL;

		node->group    = NULL;
		node->describe = NULL;

		        
	}
	| OBJECT_TYPE SYNTAX SYNTAX_TYPE SYNTAX_ENUM ACCESS ACCESS_TYPE STATUS
	  STATUS_TYPE TABLE_INDEX
        {
	/*	printf("entering the INDEX with no description.\n"); */

                node = (struct mibNode *)malloc(sizeof(struct mibNode));

                if (!node)
                   yyerror("Could not allocate memory...\n");

                node->syntax = $3;
                strcpy(node->descr,""); /* This node has no description */
                node->access = $6;

                node->enum_list = list_head;
                list_head = NULL;

                node->group    = NULL;
                node->describe = NULL;


        }

	;

TABLE_INDEX : INDEX LBRACE NAMELIST RBRACE
	{
		/* printf("Index list..\n"); */
	}
	| INDEX LBRACE SYNTAXLIST RBRACE
	{
/*		printf("This is for CISCO where index is syntax type.\n"); */
	}
	;

NAMELIST :  NAME
	{

		index_hit = 1;

		index_name[index_count] =  strdup($1);
		index_count++;


	}
	| NAMELIST COMMA NAME
	{
                index_name[index_count] =  strdup($3);
                index_count++;

	}
	;

SYNTAXLIST : SYNTAX_TYPE
	{
	}
	| SYNTAX_TYPE COMMA SYNTAX_TYPE
	{
	}
	;
	
SEQDEF	: ID CCEQUALS SEQUENCE LBRACE SEQLIST RBRACE
	{
		/* printf("seqdef\n"); */
	}

	;

SEQLIST	: NAME SYNTAX_TYPE
	{
		/* printf("first seqlist entry\n"); */
	}

	;

	| SEQLIST COMMA NAME SYNTAX_TYPE
	{
		/* printf("another seqlist entry\n"); */
	}

	;

SYNTAX_TYPE : INTEGER
	{
	}

	| OCTET_STRING
	{
		/* printf("octet string\n"); */
	}

	| OBJECTID
	{
		/* printf("object identifier\n"); */
	}

	| IPADDRESS
	{
		/* printf("IP address\n"); */
	}

	| COUNTER
	{
		/* printf("counter\n"); */
	}

	| GAUGE
	{
		/* printf("gauge\n"); */
	}

	| TIMETICKS
	{
		/* printf("timeticks\n"); */
	}
        | DISPLAY_STRING
        {
                /* printf("string\n");  */
        }
	| SEQUENCE_OF_ID
	{
		 /* printf("sequence of ids.. This is TABLE\n");  */

		$$ = TABLE_TYPE;
	}

	| ID
	{
	/*	 printf("id\n");  */
		

		struct typeInfo *walk_list;
		list_head = NULL; /* ???? CVG ***/

/*		printf("The typedef is :%s\n", $1);  */

		for ( walk_list = typelist_head; walk_list != NULL; 
                      walk_list = walk_list->next )
		{
		    if ( strcmp(walk_list->typedefname, $1) == 0 )
		    {
	   	       $$ = walk_list->syntaxtype;
		       list_head = walk_list->list;
		       break;
		    }

		}


		      /* $$ = INTEGER;  remove this */
	}

	| SYNTAX_TYPE LPAREN NUMBER DOTDOT NUMBER RPAREN
	{
		/* printf("range\n"); */
	}
	| SYNTAX_TYPE LPAREN SIZE LPAREN NUMBER DOTDOT NUMBER RPAREN RPAREN
	{
		/* printf("integer with range\n"); /* For HP MIB */
	}
	| SYNTAX_TYPE LPAREN SIZE LPAREN NUMBER RPAREN RPAREN
	{
	/*	printf("This was added for synoptics.\n"); */
	}
	;

SEQUENCE_OF_ID : SEQUENCE OF ID
	{
		/* printf("sequence of id\n"); */
	}
	;

DISPLAY_STRING : DISPLAYSTRING
	{
		/* printf("Just plain displaystring\n"); */
	}
	|  DISPLAYSTRING LPAREN SIZE LPAREN NUMBER DOTDOT NUMBER RPAREN RPAREN
	{
		/* printf("Display string with size range\n"); */
        }
        |  DISPLAYSTRING LPAREN SIZE LPAREN NUMBER RPAREN RPAREN
        {
            	/* printf("Display string with size , but no range\n"); */
        }
	;

QUOTED_STRING : QSTRING
	{
		$$ = $1;
	}
	;             

SYNTAX_ENUM : LBRACE ENUM_LIST RBRACE
	{
		/* printf("syntax enum\n"); */
	}

	|
	{
		/* printf("empty syntax enum\n"); */
	}

	;

ENUM_LIST : NAME LPAREN NUMBER RPAREN
	{
/*		 printf("\n\nfirst enum entry\n"); */
/*	 printf("The string is %s and the value is %d\n", $1, $3); */


	 list_head = (struct enumList *)malloc(sizeof(struct enumList));

	 list_head->value = $3;
	 strcpy(list_head->name, $1);
	 list_head->next = NULL;

	}

	| ENUM_LIST COMMA NAME LPAREN NUMBER RPAREN
	{
/*		 printf("another enum entry\n"); 
         printf("The string is %s and the value is %d\n", $3, $5);
*/

	 listNode = (struct enumList *)malloc(sizeof(struct enumList));

         listNode->value = $5;
         strcpy(listNode->name, $3);
         listNode->next = NULL;

	 AddEnumList(listNode);

	}
	;
	
ENUMERATED_TYPE :
ACCESS_TYPE : READ_ONLY
	{
	}

	| READ_WRITE
	{
	}

	| WRITE_ONLY
	{
	}

	| NOT_ACCESSIBLE
	{
	}

	;

STATUS_TYPE : MANDATORY
	{
	}

	| OPTIONAL
	{
	}

	| OBSOLETE
	{
	}
	| DEPRECATED
	{
	}

	;

DEFS : DEFINITIONS CCEQUALS BEGIN1 
	{
/*		printf("defs\n"); */
	}
	;

TYPEDEFINITIONS : ID  CCEQUALS SYNTAX_TYPE
	{
	  /* printf("typedef without enumerated types.\n"); */


	  typeNode = (struct typeInfo *)malloc(sizeof(struct typeInfo));

	  strcpy(typeNode->typedefname, $1);
	  typeNode->syntaxtype = $3;
	  typeNode->list = NULL;
          typeNode->next = NULL;

	  AddTypeList(typeNode);


	}
	| SYNTAX_TYPE  CCEQUALS SYNTAX_TYPE
        {
/*          printf("typedef referring to other types.\n");  */

/*
          typeNode = (struct typeInfo *)malloc(sizeof(struct typeInfo));

          strcpy(typeNode->typedefname, $1);
          typeNode->syntaxtype = $3;
          typeNode->list = NULL;
          typeNode->next = NULL;

          AddTypeList(typeNode);
*/

        }

	| ID  CCEQUALS SYNTAX_TYPE SYNTAX_ENUM
	{
/*	  printf("typedefs WITH enumerated types.\n");  */


          typeNode = (struct typeInfo *)malloc(sizeof(struct typeInfo));

          strcpy(typeNode->typedefname, $1);
          typeNode->syntaxtype = $3;
          typeNode->list = list_head;
          typeNode->next = NULL;

          AddTypeList(typeNode);


	}
	| SYNTAX_TYPE  CCEQUALS SYNTAX_TYPE SYNTAX_ENUM
        {
/*          printf("typedefs WITH enumerated types referring to other types.\n"); */

/*
          typeNode = (struct typeInfo *)malloc(sizeof(struct typeInfo));

          strcpy(typeNode->typedefname, $1);
          typeNode->syntaxtype = $3;
          typeNode->list = list_head;
          typeNode->next = NULL;

          AddTypeList(typeNode);
*/

        }
	;


%%

static void
yyerror(char* message)
{
	extern char yytext[];
	char Msg[400];
	sprintf(Msg, "%s:Line number %d: %s near \"%s\"\n",  mibNow,lineNumber,message, yytext);
	strcat(ErrorMessage, Msg);
/*** CVG we should pop an error box after this with the filename to ***/
/*** the error ***/
}

/***
This is the old FindByName


struct mibNode * FindByName(x_ptr, pname)
	struct mibNode *x_ptr;
	char   *pname;
{
	struct mibNode *found_it;

	if ( x_ptr == NULL )
		return(NULL);

	if (  strcmp(x_ptr->name, pname) == 0 )
		return (x_ptr);

	found_it = FindByName(x_ptr->sibling, pname);
	if (found_it)
		return(found_it);

	found_it = FindByName(x_ptr->child, pname);
		return(found_it);

}

****/

struct mibNode * FindByName2(x_ptr, pname)
        struct mibNode *x_ptr;
        char   *pname;
{
        struct mibNode *found_it;

        if ( x_ptr == NULL )
                return(NULL);

        if (  strcmp(x_ptr->name, pname) == 0 )
                return (x_ptr);

        found_it = FindByName2(x_ptr->sibling, pname);
        if (found_it)
                return(found_it);

        found_it = FindByName2(x_ptr->child, pname);
                return(found_it);

}


struct mibNode * FindByName(x_ptr, pname)
        struct mibNode *x_ptr;
        char   *pname;
{
        struct mibNode *found_it;

        if ( x_ptr == NULL )
                return(NULL);

        if (  strcmp(x_ptr->name, pname) == 0 )
                return (x_ptr);

        found_it = FindByName2(x_ptr->child, pname);
                return(found_it);

}



/* this is like FindByName, but it works even with a full path name;
* FindByName requires only the tailname ("local name" of the node).
*/


struct mibNode *FindByFullName(x_ptr, pname)
	struct mibNode *x_ptr;
	char *pname;
{
	/**
	* go through the name, break into pieces separated by dots.
	* For each piece, find the corresponding node,
	* looking relative to where we are already.
	*/
	
	char *name;
	struct mibNode *a_node;
	
	name = strdup(pname);
	name = strtok(name, ".");
	a_node = x_ptr;
	
	while ( name )
	
	{
		a_node = FindByName(a_node, name);
		name = strtok(NULL, ".");
	}
	
	return (a_node);
    
}


static int insert_node_in_parent(parent_ptr, nodeadd)
	struct mibNode *parent_ptr;
	struct mibNode *nodeadd;
{
	if ( parent_ptr->child == NULL )  /* first child */

	{
		parent_ptr->child = nodeadd;
		nodeadd->parent = parent_ptr;
		
	}

	else /* there is already a child..*/

	{
		struct mibNode *x, *prev, *t = parent_ptr->child;
		int n = 0;
		        
		while ( t != NULL )  
		{  
		 if ( t->subid == nodeadd->subid ) /* Duplicate child */
		 {
/*		    printf("Duplicate child:%d for parent %s\n", nodeadd->subid,
			    nodeadd->parentname );
*/
		    free(nodeadd);
		    return;
		 }
		 prev = t; 
		 t  = t->sibling; 
		 n++;
	 	}
		
		prev->sibling = nodeadd;
		nodeadd->parent = parent_ptr;
	}
		
}


static void 
AddNode(struct mibNode *addnode)
{
	struct mibNode *parent_node, *found_parent;
	int result;
	struct mibNode *search_self;
	

	search_self = FindByName(mib, addnode);

	if ( search_self != NULL )
	{
	   if ( strcmp(search_self->parentname, addnode->parentname) == 0 )
	   {
	  /*   printf("Node:%s with Parent:%s exists.\n", addnode->name,
		      addnode->parentname);
	  */
	    
	      return;
	   }
	}

	found_parent = FindByName( mib, addnode->parentname);
	if ( found_parent != NULL )

		result = insert_node_in_parent(found_parent, addnode);
/*
	else
		printf("parent not found for : %s\n", addnode->name);
*/

	     
}

/* given an integer value, return the string for the matching enum */
char* getEnumString(struct enumList* enumlist, int value) {
    struct enumList *walk;
    for (walk = enumlist; walk != NULL; walk = walk->next) {
         if (walk->value == value)
            return walk->name;
    }
    return NULL;
}

/* given an enumerated type value string, return the matching int */
int getEnumValue(struct enumList* enumlist,  char* name) {
    struct enumList *walk;
    for (walk = enumlist; walk != NULL; walk = walk->next) {
         if (!strcmp(walk->name,  name))
            return walk->value;
    }
    return -1;
}


void print_enumlist(struct enumList *printList)
{
	struct enumList *walk;

	for (walk = printList; walk != NULL; walk = walk->next)
	    printf("The string is %s, value is %d\n", walk->name, walk->value);
}
void print_node( struct mibNode *fnode)

{
	if ( !fnode )

		printf("The node is a NULL node\n");

	else

	{
		if ( ! fnode->name )
		   printf("the node name is NULL..\n");

		else

			printf("Node name is %s \n", fnode->name);

/*
		printf("Node number is %d\n", fnode->subid);
		if ( fnode->index != 0 )
		{
			printf("This is an index and its value is %d\n",
				fnode->index);
		}
		printf("Node index is %d\n", fnode->index);
		printf("Node access is %d\n", fnode->access);

		if ( ! fnode->descr )

			printf("The description is NULL...\n");

		else
			printf("Node description is ---%s\n", fnode->descr);

*/

/*
                printf("Node syntax is %d\n", fnode->syntax);


		if ( fnode->enum_list )
		   print_enumlist(fnode->enum_list);

*/
		
		if ( !fnode->parentname )
			printf("The parentname is NULL...\n");

		else
			printf("Node parent is %s\n\n", fnode->parentname);
		      
		if ( fnode->table == TRUE )

			printf("This is a table....\n");
	}
}

static void print_tree( pnode)
	struct mibNode *pnode;
{



	if ( pnode != NULL )

	{
		print_tree(pnode->sibling);
		print_node(pnode);
		print_tree(pnode->child);
	}
}

isnodeindex(name)
char *name;
{
	int i, found_index;


	for ( i = 0; i < MAXROWS; i++ )

	{
		if ( strcmp(index_name[i], name) == 0 )
		  {
			strcpy(index_name[i], "foo");
			return (i+1);
		  }
	}

	return (0);
}


/* CVG   Make sure that correct id ie returned. Right now this function */
/*       return VOID, but in future it will return the ID string        */


FindFullNameId ( given_node, namestring, idstring )
struct mibNode *given_node;
char *namestring, *idstring;

{
	int id[20];
	char *subname[20];
	char *subidstring[20];
	struct mibNode *search_ptr;
	int n = 0;
	int j, k;

	
	if ( given_node == NULL )

	{
/*		printf("Node not found!...\n"); */
		return (FINDERROR); 
	}

	bzero(namestring, MAXSTRLEN);
	bzero(idstring, MAXIDLEN);
	
	for ( search_ptr = given_node; search_ptr != NULL; search_ptr =
		search_ptr->parent )

	{
		/* Concatenate the name string also */

		id[n] = search_ptr->subid;
		subname[n] = (char *)malloc(strlen(search_ptr->name) + 1);
		strcpy(subname[n], search_ptr->name); 
		n++;
	}
	

	j = 0;
	for ( k = (n-1); k >= 0 ; k = k-1 )
	{

		subidstring[j] = (char *)malloc(2);
		sprintf(subidstring[j], "%d", id[k]);
		strcat(idstring, subidstring[j]);
		strcat(idstring, ".");
		j++;
	}

	idstring[(strlen(idstring) - 1)] = '\0';

	/* construct the namestring */

	strcpy(namestring, subname[n-1]);
	strcat(namestring, ".");
	for ( k = ( n-2); k >= 0; k = k-1 )

	{
		strcat(namestring, subname[k]);
		strcat(namestring, ".");
	}

	namestring[(strlen(namestring) - 1)] = '\0';

return (FINDSUCCESS);
}




struct mibNode *find_child(parent, id)

	struct mibNode *parent;
	int id;

{
	struct mibNode *search_ptr;

	if ((parent == NULL) || (parent->child == NULL))

		return NULL;


	if ( parent->child->subid == id )

		return ( parent->child );

	for ( search_ptr = parent->child; search_ptr != NULL; search_ptr =
				search_ptr->sibling )

	{
		if ( search_ptr->subid == id )
			return ( search_ptr);
	}

	/* child not found.. */

	return (NULL);
}

struct mibNode *FindById(start_ptr, Id)
	struct mibNode *start_ptr;
	char *Id;
{
	static	char *id;
	struct mibNode *found_child;
	int  	i, id_int, total_id;
	int id_array[20];
	char IdCopy[200]; /* xxx vic - length??? */
	/* need to copy it; otherwise strtok trashes it.
	 * Also allocate it rather than do strdup,  so that it
	 * will clean itself up better.
	 */
	strcpy(IdCopy, Id);
	total_id = 0;


	for ( i = 0; i < 20; i++)
		id_array[i] = 0;

  	for ( (id = strtok(IdCopy, ".")); id != NULL; (id = strtok(NULL, ".")) )	
	{
		id_array[total_id] = atoi(id);
		total_id++;

	}
	

	/* Now search the tree */

	if (start_ptr == NULL )
	{
		found_child = mib;
        	for ( id_int = 1; id_int < total_id; id_int++ )

        	{
                	found_child = find_child(found_child, id_array[id_int]);

		}

		return(found_child);
	}

	else
	{
		found_child = start_ptr;

		for ( id_int = 0; id_int < total_id; id_int++ )
		{
			found_child = find_child(found_child, id_array[id_int]);
		}

		return(found_child);

	}
}


void AddEnumList(add_ptr)
struct enumList *add_ptr;

{
       	if ( list_head )  /* not the first entry */
	   add_ptr->next = list_head;

	else  /* first entry */
	   add_ptr->next = NULL;

	list_head = add_ptr;

	
}


void AddTypeList(type_ptr)
struct typeInfo *type_ptr;

{
	if (typelist_head)
	   type_ptr->next = typelist_head;

	else
	   type_ptr->next = NULL;

	typelist_head = type_ptr;
 
}

char *ReadAndParse()
{
        DIR *dirp;
        struct direct *dp;
	FILE *opentest;
	char currentMib[256];
	char parseMsg[400];
        int n = 0;
	


	/* First read the mib2 */

		/* printf("-- mib2\n");  */
	
                opentest = freopen(MIB2, "r", stdin);
		if ( opentest == NULL )
		{
		  strcpy(parseMsg, "Could not open the file :");
		  strcat(parseMsg, MIB2);
		  strcat(parseMsg, "\n");
		  if (( MAXERRBUF - strlen(ErrorMessage) - strlen(parseMsg)) < 20)
			strcat(ErrorMessage, "Too Many Errors.");
		  else
			strcat(ErrorMessage, parseMsg);

		 /* printf("Could not open the file %s\n", MIB2); */
		  return (ErrorMessage);

		  /*** CVG  Throw a error dialog box here ***/
		  /* exit(1); */
		  
		}
		lineNumber = 1;
		strcpy(mibNow, MIB2);
                yyparse();


	/* Now read other mibs in the directory */



        dirp = opendir(MIBS);

        while ((dp = readdir(dirp)) != NULL )

        {
                if (  ( strcmp(dp->d_name, ".") == 0 ) ||
                      ( strcmp(dp->d_name, "..") == 0 ) ||
                      ( strcmp(dp->d_name, "mib2") == 0 )  )

                        continue;

                /* printf(" -- %s \n", dp->d_name); */
                n++;

		strcpy(currentMib, MIBS);
                strcat(currentMib, "/");
		strcat(currentMib, dp->d_name);
		/* printf("the current mib is -- %s\n", currentMib); */
                opentest = freopen(currentMib, "r", stdin);
                if ( opentest == NULL )
                {
                  strcpy(parseMsg, "Could not open the file :");
                  strcat(parseMsg, MIBS);
		  strcat(parseMsg, currentMib);
                  /* printf("Could not open the file %s\n", currentMib); */
		  if (( MAXERRBUF - strlen(ErrorMessage) - strlen(parseMsg)) < 20 )
			strcat(ErrorMessage, "Too Many Errors.");
		  else
			strcat(ErrorMessage, parseMsg);
		
		  closedir(dirp);
		  return (ErrorMessage);

                  /*** CVG  Throw a error dialog box here ***/
                  /* exit(1); */

                }
		lineNumber = 1; /* CVG added 10/13 */
		strcpy(mibNow, currentMib);
                yyparse();
        }

        /* printf("Total files are %d\n", n ); */

        closedir(dirp);

	return (ErrorMessage);

/*
	printf("The complete mib tree being PRINTED now...\n");  

	print_tree(mib);  

	printf("Tree print completed.\n"); 
*/

}


#ifdef DO_PARSE_MAIN
main()
{

	struct mibNode *mynode_ptr, *result_ptr, *next_result;
	char fullname[256];
	char fullid[80];
	int status;


	mib = NULL;

	ReadAndParse();

        mynode_ptr = FindByName (mib, "bufferSmCreate" ); 
	status = FindFullNameId(mynode_ptr, fullname, fullid); 
	if ( status == FINDSUCCESS )
	{
		printf("\nFull name of bufferSmCreate is:%s\n", fullname);
		printf("Full id of bufferSmCreate is: %s\n\n", fullid);
	}

	result_ptr = FindById(NULL, "1.3.6.1.2");

	printf("printing result_ptr........\n\n");
	print_node(result_ptr);

	next_result = FindById(result_ptr, "1.2.2");
	printf("\n\nprinting next_result......\n");
	print_node(next_result);

        mynode_ptr = FindByName (mib, "etherStatsDataSource" );
	status = FindFullNameId(mynode_ptr, fullname, fullid);
        if ( status == FINDSUCCESS )
        {
                printf("\nFull name of etherStatsDataSource is:%s\n", fullname);
                printf("Full id of etherStatsDataSource is: %s\n\n", fullid);
        }


        mynode_ptr = FindByName (mib, "services" );
	status = FindFullNameId(mynode_ptr, fullname, fullid);
        if ( status == FINDSUCCESS )
        {
                printf("\nFull name of services is:%s\n", fullname);
                printf("Full id of services is: %s\n\n", fullid);
        }


        mynode_ptr = FindByName (mib, "tcp" );
        status = FindFullNameId(mynode_ptr, fullname, fullid);
        if ( status == FINDSUCCESS )
        {
                printf("\nFull name of tcp is:%s\n", fullname);
                printf("Full id of tcp is: %s\n\n", fullid);
        }


        mynode_ptr = FindByName (mib, "system" );
        status = FindFullNameId(mynode_ptr, fullname, fullid);
        if ( status == FINDSUCCESS )
        {
                printf("\nFull name of system is:%s\n", fullname);
                printf("Full id of system is: %s\n\n", fullid);
        }



}

#endif
