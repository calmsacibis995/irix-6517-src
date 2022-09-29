/*
 * rules.h - rule description data structures and parsing
 * 
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef RULES_H
#define RULES_H

#define DEFAULT_RULES		"/var/pcp/config/pmieconf"
#define DEFAULT_ROOT_PMIE	"/var/pcp/config/pmie/config.default"
#define DEFAULT_USER_PMIE	".pcp/pmie/config.pmie"	/* $HOME/... */

#define TYPE_STRING		0	/* arbitrary string (default) */
#define TYPE_DOUBLE		1	/* real number */
#define TYPE_INTEGER		2	/* integer number */
#define TYPE_UNSIGNED		3	/* cardinal number */
#define TYPE_PERCENT		4	/* percentage 0..100 */
#define TYPE_HOSTLIST		5	/* list of host names */
#define TYPE_INSTLIST		6	/* list of metric instances */
#define TYPE_PRINT		7	/* print action */
#define TYPE_SHELL		8	/* shell action */
#define TYPE_ALARM		9	/* alarm window action */
#define TYPE_SYSLOG		10	/* syslog action */
#define TYPE_RULE		11	/* rule definition */

#define ATTRIB_HELP		0	/* help= text */
#define ATTRIB_MODIFY		1	/* modify= text */
#define ATTRIB_ENABLED		2	/* enabled= y/n */
#define ATTRIB_DISPLAY		3	/* display= y/n */
#define ATTRIB_DEFAULT		4	/* default= value */
#define ATTRIB_VERSION		5	/* version= int */
#define ATTRIB_PREDICATE	6	/* predicate= text */
#define ATTRIB_ENUMERATE	7	/* enumerate= text */

#define IS_ACTION(t)	\
	(t==TYPE_PRINT || t==TYPE_SHELL || t==TYPE_ALARM || t==TYPE_SYSLOG)
#define IS_RULE(t)	(t == TYPE_RULE)

/* generic data block definition */
struct atom_type {
    char		*name;		/* atom identifier */
    char		*data;		/* value string */
    char		*ddata;		/* default value */
    char		*help;		/* help text */
    unsigned int	type    : 16;	/* data type */
    unsigned int	modify  :  1;	/* advice for editor */	
    unsigned int	display :  1;	/* advice for editor */
    unsigned int	enabled :  1;	/* switched on/off */
    unsigned int	denabled:  1;	/* default on/off */
    unsigned int	changed :  1;	/* has value been changed? */
    unsigned int	global  :  1;	/* global scope */
    unsigned int	padding : 10;	/* unused */
    struct atom_type	*next;
};
typedef struct atom_type atom_t;

typedef struct {
    unsigned int	version;
    atom_t		self;
    char		*predicate;
    char		*enumerate;	/* list of hostlist/instlist params */
} rule_t;


extern rule_t		*rulelist;
extern unsigned int	rulecount;
extern rule_t		*globals;

extern int		errno;
extern char		errmsg[];	/* error message buffer */


/*
 * routines below returning char*, on success return NULL else failure message
 */

char *initialise(char *, char *, char *);    /* setup global data structures */

char *get_pmiefile(void);
char *get_rules(void);
char *get_aname(rule_t *, atom_t *);

void sort_rules(void);
char *find_rule(char *, rule_t **);
char *lookup_rules(char *, rule_t ***, unsigned int *, int);

char *value_string(atom_t *, int);   /* printable string form of atoms value */
char *value_change(rule_t *, char *, char *); /* change rule parameter value */
char *validate(int, char *, char *);  /* check proposed value for named type */

char *write_pmiefile(char *);
char *lookup_processes(int *, char ***);

int is_attribute(char *);
char *get_attribute(char *, atom_t *);
char *rule_defaults(rule_t *, char *);

int is_overridden(rule_t *, atom_t *);
int read_token(FILE *, char *, int, int);
char *dollar_expand(rule_t *, char *, int);


/* deprecated rules stuff */
#define DEPRECATE_NORULE	0
#define DEPRECATE_VERSION	1

typedef struct {
    unsigned int	version;	/* version not matching/in rules */
    char		*name;		/* full name of the offending rule */
    char		*reason;	/* ptr to deprecation description */
    int			type;		/* reason for deprecating this rule */
} dep_t;
int fetch_deprecated(dep_t **list);


/* generic symbol table definition */
typedef struct {
    int		symbol_id;
    char	*symbol;
} symbol_t;

/* lookup keyword, returns symbol identifier or -1 if not there */
int map_symbol(symbol_t *, int, char *);

/* lookup symbol identifier, returns keyword or NULL if not there */
char *map_identifier(symbol_t *, int, int);

/* parse yes/no attribute value; returns 0 no, 1 yes, -1 error */
int map_boolean(char *);

#endif
