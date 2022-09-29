#ifndef SMTD_SNMP_H
#define SMTD_SNMP_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

#define OID_ANY		0 /* SLT */

extern struct tree * find_oid(struct tree *, int, oid *, int);
extern int type_to_oid(int, oid *);
extern void quit(void);
extern int snmp_input(int, struct snmp_session *, int, struct snmp_pdu *, void *);
extern int snmp_trapinput(int, struct snmp_session *, int, struct snmp_pdu *, void *);
extern char *snmp_auth(char *, int *, char *, int);
extern struct tree *make_outvar(struct variable_list *, struct variable_list *);
extern struct tree *build_variable(struct tree *, struct variable_list *,
						struct variable_list *, int);
extern struct tree *find_variable(struct tree *, struct variable_list *);
extern struct tree *find_next(struct variable_list *);
extern void find_name(struct variable_list *, char *);
extern int find_value(struct tree *, struct variable_list *);

#endif
