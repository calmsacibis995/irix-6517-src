/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.13 $
 */

/***********************************************************
	Copyright 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <smt_parse.h>
#include <sm_log.h>

/*
 * This is one element of an object identifier with either an integer
 * subidentifier, or a textual string label, or both.
 * The subid is -1 if not present, and label is NULL if not present.
 */
struct subid {
    	int subid;
    	char *label;
};

static void do_subtree(struct tree*, struct node**);
static int get_token(FILE*, char*);
static void free_node(struct node*);


static int Line = 1;

/* types of tokens */
#define	CONTINUE    -1
#define LABEL	    1
#define SUBTREE	    2
#define SYNTAX	    3
#define OBJID	    4
#define OCTETSTR    5
#define INTEGER	    6
#define NETADDR	    7
#define	IPADDR	    8
#define COUNTER	    9
#define GAUGE	    10
#define TIMETICKS   11
#define OPAQUE	    12
#define NUL	    13
#define SEQUENCE    14
#define OF	    15	/* SEQUENCE OF */
#define OBJTYPE	    16
#define ACCESS	    17
#define READONLY    18
#define READWRITE   19
#define	WRITEONLY   20
#define NOACCESS    21
#define STATUS	    22
#define MANDATORY   23
#define OPTIONAL    24
#define OBSOLETE    25
#define PUNCT	    26
#define EQUALS	    27

struct tok {
	char *name;			/* token name */
	int len;			/* length not counting nul */
	int token;			/* value */
	int hash;			/* hash of name */
	struct tok *next;		/* pointer to next in hash table */
};


static struct tok tokens[] = {
	{ "obsolete", 		sizeof("obsolete")-1, 		OBSOLETE},
	{ "Opaque", 		sizeof("Opaque")-1, 		OPAQUE},
	{ "optional", 		sizeof("optional")-1, 		OPTIONAL},
	{ "mandatory", 		sizeof("mandatory")-1, 		MANDATORY},
	{ "not-accessible", 	sizeof("not-accessible")-1, 	NOACCESS},
	{ "write-only", 	sizeof("write-only")-1, 	WRITEONLY},
	{ "read-write", 	sizeof("read-write")-1, 	READWRITE},
	{ "TimeTicks", 		sizeof("TimeTicks")-1, 		TIMETICKS},
	{ "OBJECTIDENTIFIER", 	sizeof("OBJECTIDENTIFIER")-1, 	OBJID},
	/*
	 * This CONTINUE appends the next word onto OBJECT,
	 * hopefully matching OBJECTIDENTIFIER above.
	 */
	{ "OBJECT", 		sizeof("OBJECT")-1, 		CONTINUE},
	{ "NetworkAddress", 	sizeof("NetworkAddress")-1, 	NETADDR},
	{ "Gauge", 		sizeof("Gauge")-1, 		GAUGE},
	{ "OCTETSTRING", 	sizeof("OCTETSTRING")-1, 	OCTETSTR},
	{ "OCTET", 		sizeof("OCTET")-1, 		-1},
	{ "OF", 		sizeof("OF")-1, 		OF},
	{ "SEQUENCE", 		sizeof("SEQUENCE")-1, 		SEQUENCE},
	{ "NULL", 		sizeof("NULL")-1, 		NUL},
	{ "IpAddress", 		sizeof("IpAddress")-1, 		IPADDR},
	{ "INTEGER", 		sizeof("INTEGER")-1, 		INTEGER},
	{ "Counter", 		sizeof("Counter")-1, 		COUNTER},
	{ "read-only", 		sizeof("read-only")-1, 		READONLY},
	{ "ACCESS", 		sizeof("ACCESS")-1, 		ACCESS},
	{ "STATUS", 		sizeof("STATUS")-1, 		STATUS},
	{ "SYNTAX", 		sizeof("SYNTAX")-1, 		SYNTAX},
	{ "OBJECT-TYPE", 	sizeof("OBJECT-TYPE")-1, 	OBJTYPE},
	{ "{", 			sizeof("{")-1, 			PUNCT},
	{ "}", 			sizeof("}")-1, 			PUNCT},
	{ "::=", 		sizeof("::=")-1, 		EQUALS},
	{ NULL }
};

#define	HASHSIZE	32
#define	BUCKET(x)	(x & 0x01F)

static struct tok	*buckets[HASHSIZE];

static void
hash_init(void)
{
	register struct tok	*tp;
	register char	*cp;
	register int	h;
	register int	b;

	for (tp = tokens; tp->name; tp++) {
		for (h = 0, cp = tp->name; *cp; cp++)
			h += *cp;
		tp->hash = h;
		b = BUCKET(h);
		if (buckets[b])
			tp->next = buckets[b];
		buckets[b] = tp;
	}
}


char *
Malloc(unsigned long num)
{
	char *cp;
    
	/* this is to fix (what seems to be) a problem with the
 	 * IBM RT C library malloc
 	 */
	if (num < 16)
		num = 16;
	else if (num % sizeof(long)) {
		num |= 0x3;
	}

	cp = malloc(num);
	if (cp == 0) {
		sm_log(LOG_EMERG, SM_ISSYSERR, "Malloc failed");
		exit(-1);
	}
	bzero(cp, num);
	return cp;
}

static void
print_error(char *string,
	    char *token)
{
	if (token)
		sm_log(LOG_ERR, 0, "%s(%s): On line %d\n", string,token,Line);
	else
		sm_log(LOG_ERR, 0, "%s: On line %d\n", string, Line);
}

/* non-aggregate types for tree end nodes */
char *test_tt[] = {
	"OTHER",
	"OBJID",
	"OCTETSTR",
	"INTEGER",
	"NETADDR",
	"IPADDR",
	"COUNTER",
	"GAUGE",
	"TIMETICKS",
	"OPAQUE",
	"SEQUENCE",
	"NULL",
};

/* access methods */
char *test_at[] = {
	"NOACCESS",
	"READONLY",
	"WRITEONLY",
	"READWRITE",
};

/* variable status */
char *test_st[] = {
	"OBSOLETE",
	"MANDATORY",
	"OPTIONAL",
};

void
print_subtree( struct tree *tree, int count)
{
	char tmp[256];
	register int i;
	struct tree *tp;

	tmp[0] = '\0';
	for (i = 0; i < count; i++)
	    	strcat(tmp, "  ");
	if (tree->child_list)
	    	strcat(tmp, "Children of ");
	if (tree->type < 11 && tree->access < 4 && tree->status < 3) {
    		sprintf(&tmp[strlen(tmp)], "%s(%d, %s, %s, %s)",
			tree->label, tree->subid,
			test_tt[tree->type],
			test_at[tree->access],
			test_st[tree->status]);
	} else {
    		sprintf(&tmp[strlen(tmp)], "%s(?%d, %d, %d, %d)",
			tree->label, tree->subid,
			tree->type, tree->access, tree->status);
	}
	if (tree->child_list)
	    	strcat(tmp, ":");
	sm_log(LOG_ERR, 0, "%s", tmp);

	count++;
	for (tp = tree->child_list; tp; tp = tp->next_peer){
		print_subtree(tp, count);
	}
}

static struct tree *
build_tree(struct node *nodes)
{
	struct node *np;
	struct tree *tp;
    
	/* build root node */
	tp = (struct tree *)Malloc(sizeof(struct tree));
	tp->parent = NULL;
	tp->next_peer = NULL;
	tp->child_list = NULL;
	tp->enums = NULL;
	strcpy(tp->label, "iso");
	tp->subid = 1;
	tp->type = 0;

	/*??? int access; int status; void (*printer)(); */

	/* grow tree from this root node */
	do_subtree(tp, &nodes);

	/* If any nodes are left, the tree is probably inconsistent */
	if (!nodes)
		return(tp);

	sm_log(LOG_ERR,0, "The mib description doesn't seem to be consistent.");
	sm_log(LOG_ERR,0, "Some nodes can't be linked under the \"iso\" tree.");
	sm_log(LOG_ERR,0, "these nodes are left:");
	for (np = nodes; np; np = np->next) {
		sm_log(LOG_ERR, 0, "%s ::= { %s %d } (%d)\n",
			np->label, np->parent, np->subid, np->type);
	}
	return(tp);
}


/*
 * Find all the children of root in the list of nodes.  Link them into the
 * tree and out of the nodes list.
 */
static void
do_subtree(struct tree *root,
	struct node **nodes)
{
    register struct tree *tp;
    struct tree *peer = NULL;
    register struct node *np;
    struct node *oldnp = NULL, *child_list = NULL, *childp = NULL;
    
    tp = root;
    /*
     * Search each of the nodes for one whose parent is root, and
     * move each into a seperate list.
     */
    for(np = *nodes; np; np = np->next){
	if ((tp->label[0] == np->parent[0]) && !strcmp(tp->label, np->parent)){
	    if (child_list == NULL){
		child_list = childp = np;   /* first entry in child list */
	    } else {
		childp->next = np;
		childp = np;
	    }
	    /* take this node out of the node list */
	    if (oldnp == NULL){
		*nodes = np->next;  /* fix root of node list */
	    } else {
		oldnp->next = np->next;	/* link around this node */
	    }
	} else {
	    oldnp = np;
	}
    }
    if (childp)
	childp->next = 0;	/* re-terminate list */
    /*
     * Take each element in the child list and place it into the tree.
     */
    for(np = child_list; np; np = np->next){
	tp = (struct tree *)Malloc(sizeof(struct tree));
	tp->parent = root;
	tp->next_peer = NULL;
	tp->child_list = NULL;
	strcpy(tp->label, np->label);
	tp->subid = np->subid;
	switch(np->type){
	    case OBJID:
		tp->type = TYPE_OBJID;
		break;
	    case OCTETSTR:
		tp->type = TYPE_OCTETSTR;
		break;
	    case INTEGER:
		tp->type = TYPE_INTEGER;
		break;
	    case NETADDR:
		tp->type = TYPE_IPADDR;
		break;
	    case IPADDR:
		tp->type = TYPE_IPADDR;
		break;
	    case COUNTER:
		tp->type = TYPE_COUNTER;
		break;
	    case GAUGE:
		tp->type = TYPE_GAUGE;
		break;
	    case TIMETICKS:
		tp->type = TYPE_TIMETICKS;
		break;
	    case OPAQUE:
		tp->type = TYPE_OPAQUE;
		break;
	    case SEQUENCE:
		tp->type = TYPE_SEQUENCE;
		break;
	    case NUL:
		tp->type = TYPE_NULL;
		break;
	    default:
		tp->type = TYPE_OTHER;
		break;
	}
	switch(np->access) {
	    case READONLY:
		tp->access = ACCESS_READONLY;
		break;
	    case READWRITE:
		tp->access = ACCESS_READWRITE;
		break;
	    case WRITEONLY:
		tp->access = ACCESS_WRITEONLY;
		break;
	    case NOACCESS:
		tp->access = ACCESS_NOACCESS;
		break;
	    default:
		SNMPDEBUG((LOG_DBGSNMP, 0, "NOACCESS set for %s\n", np->label));
		tp->access = ACCESS_NOACCESS;
	}
	switch(np->status) {
	    case MANDATORY:
		tp->status = STATUS_MANDATORY;
		break;
	    case OPTIONAL:
		tp->status = STATUS_OPTIONAL;
		break;
	    case OBSOLETE:
		tp->status = STATUS_OBSOLETE;
		break;
	    default:
		SNMPDEBUG((LOG_DBGSNMP,0,
			"OBSOLETE status for %s\n", np->label));
		tp->status = STATUS_OBSOLETE;
	}
	tp->enums = np->enums;
	np->enums = NULL;	/* so we don't free them later */
	if (root->child_list == NULL){
	    root->child_list = tp;
	} else {
	    peer->next_peer = tp;
	}
	peer = tp;
	do_subtree(tp, nodes);	/* recurse on this child */
    }
    /* free all nodes that were copied into tree */
    for(np = child_list; np;){
	oldnp = np;
	np = np->next;
	free_node(oldnp);
    }
}


/*
 * Takes a list of the form:
 * { iso org(3) dod(6) 1 }
 * and creates several nodes, one for each parent-child pair.
 * Returns NULL on error.
 */
static int
getoid(fp, oid,  length)
    register FILE *fp;
    register struct subid *oid;	/* an array of subids */
    int length;	    /* the length of the array */
{
    register int count;
    int type;
    char token[PARSE_TOKEN_LEN], label[PARSE_LABEL_LEN];
    register char *cp, *tp;

    if ((type = get_token(fp, token)) != PUNCT){
	if (type == -1)
	    print_error("Unexpected EOF", (char *)NULL);
	else
	    print_error("Unexpected", token);
	return NULL;
    }
    if (*token != '{'){
	print_error("Unexpected", token);
	return NULL;
    }
    for(count = 0; count < length; count++, oid++){
	oid->label = 0;
	oid->subid = -1;
	if ((type = get_token(fp, token)) != LABEL){
	    if (type == -1){
		print_error("Unexpected EOF", (char *)NULL);
		return NULL;
	    }
	    else if (type == PUNCT && *token == '}'){
		return count;
	    } else {
		print_error("Unexpected", token);
		return NULL;
	    }
	}
	tp = token;
	if (!isdigit(*tp)){
	    /* this entry has a label */
	    cp = label;
	    while(*tp && *tp != '(')
		*cp++ = *tp++;
	    *cp = 0;
	    cp = (char *)Malloc((unsigned)strlen(label));
	    strcpy(cp, label);
	    oid->label = cp;
	    if (*tp == '('){
		/* this entry has a label-integer pair in the form label(integer). */
		cp = ++tp;
		while(*cp && *cp != ')')
		    cp++;
		if (*cp == ')')
		    *cp = 0;
		else {
		    print_error("No terminating parenthesis", (char *)NULL);
		    return NULL;
		}
		oid->subid = atoi(tp);
	    }
	} else {
	    /* this entry  has just an integer sub-identifier */
	    oid->subid = atoi(tp);
	}
    }
    return count;


}

static void
free_node(struct node *np)
{
    struct enum_list *ep, *tep;

    ep = np->enums;
    while(ep){
	tep = ep;
	ep = ep->next;
	free((char *)tep);
    }
    free((char *)np);
}

/*
 * Parse an entry of the form:
 * label OBJECT IDENTIFIER ::= { parent 2 }
 * The "label OBJECT IDENTIFIER" portion has already been parsed.
 * Returns 0 on error.
 */
static struct node *
parse_objectid(fp, name)
    FILE *fp;
    char *name;
{
    int type;
    char token[PARSE_TOKEN_LEN];
    register int count;
    register struct subid *op, *nop;
    int length;
    struct subid oid[PARSE_OID_DEPTH];
    struct node *np, *root;

    type = get_token(fp, token);
    if (type != EQUALS){
	print_error("Bad format", token);
	return 0;
    }
    if (length = getoid(fp, oid, PARSE_OID_DEPTH)){
	np = root = (struct node *)Malloc(sizeof(struct node));
	np->next = NULL;
	np->enums = NULL;
	/*
	 * For each parent-child subid pair in the subid array,
	 * create a node and link it into the node list.
	 */
	for(count = 0, op = oid, nop=oid+1; count < (length - 2); count++,
	    op++, nop++){
	    /* every node must have parent's name and child's name or number */
	    if (op->label && (nop->label || (nop->subid != -1))){
		strcpy(np->parent, op->label);
		if (nop->label)
		    strcpy(np->label, nop->label);
		if (nop->subid != -1)
		    np->subid = nop->subid;
		np ->type = 0;
		np->enums = 0;
		/* set up next entry */
		np->next = (struct node *)Malloc(sizeof(*np->next));
		np = np->next;
		np->next = NULL;
		np->enums = NULL;
	    }
	}
	/*
	 * The above loop took care of all but the last pair.
	 * This pair is taken care of here.  The name for this node
	 * is taken from the label for this entry.
	 * np still points to an unused entry.
	 */
	if (count == (length - 2)){
	    if (op->label){
		strcpy(np->parent, op->label);
		strcpy(np->label, name);
		if (nop->subid != -1)
		    np->subid = nop->subid;
		else
		    print_error("Warning: This entry is pretty silly", token);
	    } else {
		free_node(np);
	    }
	} else {
	    print_error("Missing end of oid", (char *)NULL);
	    free_node(np);   /* the last node allocated wasn't used */
	    return NULL;
	}
	/* free the oid array */
	for(count = 0, op = oid; count < length; count++, op++){
	    if (op->label)
		free(op->label);
	    op->label = 0;
	}
	return root;
    } else {
	print_error("Bad object identifier", (char *)NULL);
	return 0;
    }
}

/*
 * Parses an asn type.  This structure is ignored by this parser.
 * Returns NULL on error.
 */
static int
parse_asntype(fp)
    FILE *fp;
{
    int type;
    char token[PARSE_TOKEN_LEN];

    type = get_token(fp, token);
    if (type != SEQUENCE){
	print_error("Not a sequence", (char *)NULL); /* should we handle this */
	return NULL;
    }
    while((type = get_token(fp, token)) != NULL){
	if (type == -1)
	    return NULL;
	if (type == PUNCT && (token[0] == '}' && token[1] == '\0'))
	    return -1;
    }
    print_error("Premature end of file", (char *)NULL);
    return NULL;
}

/*
 * Parses an OBJECT TYPE macro.
 * Returns 0 on error.
 */
static struct node *
parse_objecttype(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[PARSE_TOKEN_LEN];
    int count, length;
    struct subid oid[PARSE_OID_DEPTH];
    char syntax[PARSE_LABEL_LEN];
    int nexttype;
    char nexttoken[PARSE_TOKEN_LEN];
    register struct node *np;
    register struct enum_list *ep;
    register char *cp;
    register char *tp;

    type = get_token(fp, token);
    if (type != SYNTAX){
	print_error("Bad format for OBJECT TYPE", token);
	return 0;
    }
    np = (struct node *)Malloc(sizeof(struct node));
    np->next = NULL;
    np->enums = NULL;
    type = get_token(fp, token);
    nexttype = get_token(fp, nexttoken);
    np->type = type;
    switch(type){
	case SEQUENCE:
	    strcpy(syntax, token);
	    if (nexttype == OF){
		strcat(syntax, " ");
		strcat(syntax, nexttoken);
		nexttype = get_token(fp, nexttoken);
		strcat(syntax, " ");
		strcat(syntax, nexttoken);
		nexttype = get_token(fp, nexttoken);
	    }
	    break;
	case INTEGER:
	    strcpy(syntax, token);
	    if (nexttype == PUNCT &&
		(nexttoken[0] == '{' && nexttoken[1] == '\0')) {
		/* if there is an enumeration list, parse it */
		while((type = get_token(fp, token)) != NULL){
		    if (type == -1){
			free_node(np);
			return 0;
		    }
		    if (type == PUNCT &&
			(token[0] == '}' && token[1] == '\0'))
			break;
		    if (type == 1){
			/* this is an enumerated label */
			if (np->enums == 0){
			    ep = np->enums = (struct enum_list *)
					Malloc(sizeof(struct enum_list));
			} else {
			    ep->next = (struct enum_list *)
					Malloc(sizeof(struct enum_list));
			    ep = ep->next;
			}
			ep->next = NULL;
			/* a reasonable approximation for the length */
			ep->label = (char *)Malloc((unsigned)strlen(token));
			cp = ep->label;
			tp = token;
			while(*tp != '(')
			    *cp++ = *tp++;
			*cp = 0;
			cp = ++tp;    /* start of number */
			while(*tp != ')')
			    tp++;
			*tp = 0;    /* terminate number */
			ep->value = atoi(cp);
		    }
		}
		if (type == NULL){
		    print_error("Premature end of file", (char *)NULL);
		    free_node(np);
		    return 0;
		}
		nexttype = get_token(fp, nexttoken);
	    } else if (nexttype == LABEL && *nexttoken == '('){
		/* ignore the "constrained integer" for now */
		nexttype = get_token(fp, nexttoken);
	    }
	    break;
	case OBJID:
	case OCTETSTR:
	case NETADDR:
	case IPADDR:
	case COUNTER:
	case GAUGE:
	case TIMETICKS:
	case OPAQUE:
	case NUL:
	case LABEL:
	    strcpy(syntax, token);
	    break;
	default:
	    print_error("Bad syntax", token);
	    free_node(np);
	    return 0;
    }
    if (nexttype != ACCESS){
	print_error("Should be ACCESS", nexttoken);
	free_node(np);
	return 0;
    }
    np->access = type = get_token(fp, token);
    if (type != READONLY && type != READWRITE && type != WRITEONLY
	&& type != NOACCESS){
	print_error("Bad access type", nexttoken);
	free_node(np);
	return 0;
    }
    type = get_token(fp, token);
    if (type != STATUS){
	print_error("Should be STATUS", token);
	free_node(np);
	return 0;
    }
    np->status = type = get_token(fp, token);
    if (type != MANDATORY && type != OPTIONAL && type != OBSOLETE){
	print_error("Bad status", token);
	free_node(np);
	return 0;
    }
    type = get_token(fp, token);
    if (type != EQUALS){
	print_error("Bad format", token);
	free_node(np);
	return 0;
    }
    length = getoid(fp, oid, PARSE_OID_DEPTH);
    if (length > 1 && length <= PARSE_OID_DEPTH){
	/* just take the last pair in the oid list */
	if (oid[length - 2].label)
	    strncpy(np->parent, oid[length - 2].label, PARSE_LABEL_LEN);
	strcpy(np->label, name);
	if (oid[length - 1].subid != -1)
	    np->subid = oid[length - 1].subid;
	else
	    print_error("Warning: This entry is pretty silly", (char *)NULL);
    } else {
	print_error("No end to oid", (char *)NULL);
	free_node(np);
	np = 0;
    }
    /* free oid array */
    for(count = 0; count < length; count++){
	if (oid[count].label)
	    free(oid[count].label);
	oid[count].label = 0;
    }
    return np;
}


/*
 * Parses a mib file and returns a linked list of nodes found in the file.
 * Returns NULL on error.
 */
static struct node *
parse(fp)
    FILE *fp;
{
    char token[PARSE_TOKEN_LEN];
    char name[PARSE_LABEL_LEN];
    int	type = 1;
    struct node *np, *root = NULL;

    hash_init();

    while(type != NULL){
	type = get_token(fp, token);
	if (type != LABEL){
	    if (type == NULL){
		return root;
	    }
	    print_error(token, "is a reserved word");
	    return NULL;
	}
	strncpy(name, token, PARSE_LABEL_LEN);
	type = get_token(fp, token);
	if (type == OBJTYPE){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_objecttype(fp, name);
		if (np == NULL){
		    print_error("Bad parse of object type", (char *)NULL);
		    return NULL;
		}
	    } else {
		np->next = parse_objecttype(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of objecttype", (char *)NULL);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == OBJID){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_objectid(fp, name);
		if (np == NULL){
		    print_error("Bad parse of object id", (char *)NULL);
		    return NULL;
		}
	    } else {
		np->next = parse_objectid(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of object type", (char *)NULL);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == EQUALS){
	    type = parse_asntype(fp);
	} else if (type == NULL){
	    break;
	} else {
	    print_error("Bad operator", (char *)NULL);
	    return NULL;
	}
    }
    return root;
}

/*
 * Parses a token from the file.  The type of the token parsed is returned,
 * and the text is placed in the string pointed to by token.
 */
static int
get_token(FILE *fp,
    	char *token)
{
    	static char last = ' ';
    	register int ch;
    	register char *cp = token;
    	register int hash = 0;
    	register struct tok *tp;

    	*cp = 0;
    	ch = last;
    	/* skip all white space */
    	while(isspace(ch) && ch != -1){
		ch = getc(fp);
		if (ch == '\n')
	    		Line++;
    	}
    	if (ch == -1)
		return(NULL);

    	/*
     	 * Accumulate characters until white space is found.
     	 * Then attempt to match this * token as a reserved word.
     	 * If a match is found, return the type.  Else it is a label.
     	 */
    do {
	if (!isspace(ch)){
	    hash += ch;
	    *cp++ = ch;
	    if (ch == '\n')
		Line++;
	} else {
	    last = ch;
	    *cp = '\0';

	    for (tp = buckets[BUCKET(hash)]; tp; tp = tp->next) {
		if ((tp->hash == hash) && (strcmp(tp->name, token) == 0))
			break;
	    }
	    if (tp){
		if (tp->token == CONTINUE)
		    continue;
		return (tp->token);
	    }

	    if (token[0] == '-' && token[1] == '-'){
		/* strip comment */
		while ((ch = getc(fp)) != -1)
		    if (ch == '\n'){
			Line++;
			break;
		    }
		if (ch == -1)
		    return NULL;
		last = ch;
		return get_token(fp, token);		
	    }
	    return LABEL;
	}
    
    } while ((ch = getc(fp)) != -1);
    return NULL;
}

struct tree *
read_mib(char *filename)
{
	FILE *fp;
	struct node *nodes;
	struct tree *tree;

	if ((fp = fopen(filename, "r")) == NULL)
		return NULL;
	nodes = parse(fp);
	if (!nodes) {
		sm_log(LOG_EMERG, 0, "Mib table is bad.  Exiting");
		exit(1);
	}
	SNMPDEBUG((LOG_DEBUG, 0, "%s parsed", filename));

	tree = build_tree(nodes);
	SNMPDEBUG((LOG_DEBUG, 0, "tree built"));

	fclose(fp);
	return(tree);
}

/*
 * Test facilities.
 */
struct node *
test_parse(FILE *fp)
{
	return(parse(fp));
}

struct tree *
test_build_tree(struct node *nodes)
{
	return(build_tree(nodes));
}

