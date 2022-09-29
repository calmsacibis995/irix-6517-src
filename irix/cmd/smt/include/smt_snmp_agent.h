#ifndef SMT_SNMP_AGENT_H
#define SMT_SNMP_AGENT_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.6 $
 */

extern int	version_id_len;

extern int snmp_parse(u_char*, int, u_char*, int, u_long);
extern int parse_var_op_list(u_char*, int, u_char*, int, u_char, long*, int);
extern void create_identical(u_char*, u_char*, int, long, long);
extern int snmp_access(u_short, int, int);
extern int get_community(u_char);
extern int goodValue(u_char, int, u_char, int);
extern void setVariable(u_char*, u_char, int, u_char*, int);
extern void shift_array(u_char*, int, int);
#endif
