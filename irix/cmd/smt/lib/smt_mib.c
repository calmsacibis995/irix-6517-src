/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.11 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

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
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>

#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>
#include <smt_parse.h>
#include <smt_mib.h>

static char * uptimeString(long, char *);
static void sprint_hexstring(char *, char *, int);
static void sprint_asciistring(char *, char *, int);
static void sprint_octet_string(char *, struct variable_list *);
static void sprint_opaque(char *, struct variable_list *);
static void sprint_object_identifier(char *, struct variable_list *);
static void sprint_timeticks(char *, struct variable_list *);
static void sprint_integer(char *, struct variable_list *, struct enum_list *);
static void sprint_gauge(char *, struct variable_list *);
static void sprint_counter (char *, struct variable_list *);
static void sprint_networkaddress (char *, struct variable_list *);
static void sprint_ipaddress (char *, struct variable_list *);
static void sprint_unsigned_short (char *, struct variable_list *);
static void sprint_null(char *, struct variable_list *);
static void sprint_badtype(char *);
static void sprint_by_type(char *, struct variable_list *, struct enum_list *);
static void set_functions(struct tree *);
static struct tree * find_subtree(struct tree *, oid *, int);
static int parse_subtree(struct tree *, char *, oid *, int *);
static int lc_cmp(char *, char *);


static char *
uptimeString(timeticks, buf)
	register long timeticks; /* in 10msec */
	char *buf;
{
	int seconds, minutes, hours, days;

	timeticks /= 100;
	days = timeticks / (60 * 60 * 24);
	timeticks %= (60 * 60 * 24);

	hours = timeticks / (60 * 60);
	timeticks %= (60 * 60);

	minutes = timeticks / 60;
	seconds = timeticks % 60;

	if (days == 0){
		sprintf(buf, "%d:%02d:%02d", hours, minutes, seconds);
	} else if (days == 1) {
		sprintf(buf,
			"%d day, %d:%02d:%02d", days, hours, minutes, seconds);
	} else {
		sprintf(buf,
			"%d days, %d:%02d:%02d", days, hours, minutes, seconds);
	}
	return(buf);
}

static void
sprint_hexstring(char *buf,
		 char *cp,
		 int len)
{
	while (len >= 8) {
		sprintf(buf, "%02X %02X %02X %02X %02X %02X %02X %02X ",
			cp[0],cp[1],cp[2],cp[3],cp[4],cp[5],cp[6],cp[7]);
		buf += 24;
		cp += 8;
		len -= 8;
	}

	while (len > 0) {
		sprintf(buf, "%02X ", *cp);
		buf += 3;
		cp++;
		len--;
	}
	*buf = '\0';
}

static void
sprint_asciistring(char *buf,
		   char *cp,
		   int len)
{
	int	x;

	for(x = 0; x < len; x++, cp++, buf++) {
		if (isprint(*cp))
	    		*buf = *cp;
		else
	    		*buf = '.';

		if ((x % 48) == 47) {
	    		*buf = '\n';
			buf++;
		}
	}
	*buf = '\0';
}

#ifdef UNUSED
int
read_rawobjid(input, output, out_len)
    char *input;
    oid *output;
    int	*out_len;
{
    char    buf[12], *cp;
    oid	    *op = output;
    int	    subid;

    while(*input != '\0'){
	if (!isdigit(*input))
	    break;
	cp = buf;
	while(isdigit(*input))
	    *cp++ = *input++;
	*cp = '\0';
	subid = atoi(buf);
	if(subid > MAX_SUBID){
	    SNMPDEBUG((LOG_DBGSNMP, 0, "sub-identifier too large: %s", buf));
	    return 0;
	}
	if((*out_len)-- <= 0){
	    SNMPDEBUG((LOG_DBGSNMP, 0, "object identifier too long"));
	    return 0;
	}
	*op++ = subid;
	if(*input++ != '.')
	    break;
    }
    *out_len = op - output;
    if (*out_len == 0)
	return 0;
    return 1;
}

#endif /* UNUSED */

static void
sprint_octet_string(buf, var)
	char *buf;
	struct variable_list *var;
{
	int hex, x;
	char *cp;

	if (var->type != ASN_OCTET_STR) {
		sprintf(buf, "Wrong Type (should be OCTET STRING): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}

	hex = 0;
	for (cp = var->val.string, x = 0; x < var->val_len; x++, cp++) {
		if (!(isprint(*cp) || isspace(*cp))) {
	    		hex = 1;
			break;
		}
	}
	if (var->val_len <= 4)
		hex = 1;    /* not likely to be ascii */

	if (hex) {
		sprintf(buf, "OCTET STRING-   (hex): ");
		buf += strlen(buf);
		sprint_hexstring(buf, var->val.string, var->val_len);
	} else {
		sprintf(buf, "OCTET STRING- (ascii): \"");
		buf += strlen(buf);
		sprint_asciistring(buf, var->val.string, var->val_len);
		buf += strlen(buf);
		*buf++ = '"';
		*buf = '\0';
	}
}

static void
sprint_opaque(buf, var)
	char *buf;
	struct variable_list *var;
{
	if (var->type != OPAQUE) {
		sprintf(buf, "Wrong Type (should be Opaque): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "OPAQUE -   (hex):\t");
	buf += strlen(buf);
	sprint_hexstring(buf, var->val.string, var->val_len);
}

static void
sprint_object_identifier(buf, var)
	char *buf;
	struct variable_list *var;
{

	if (var->type != ASN_OBJECT_ID) {
		sprintf(buf, "Wrong Type (should be OBJECT IDENTIFIER): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "OBJECT IDENTIFIER:\t");
	buf += strlen(buf);
	sprint_objid(buf, (oid *)(var->val.objid), var->val_len / sizeof(oid));
}

static void
sprint_timeticks(buf, var)
	char *buf;
	struct variable_list *var;
{
	char timebuf[32];

	if (var->type != TIMETICKS) {
		sprintf(buf, "Wrong Type (should be Timeticks): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "Timeticks: (%d) %s", *(var->val.integer),
		uptimeString(*(var->val.integer), timebuf));
}

static void
sprint_integer(buf, var, enums)
	char *buf;
	struct variable_list *var;
	struct enum_list	    *enums;
{
	char    *enum_string = NULL;

	if (var->type != ASN_INTEGER) {
		sprintf(buf, "Wrong Type (should be INTEGER): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}

	for (; enums; enums = enums->next) {
		if (enums->value == *var->val.integer) {
	    		enum_string = enums->label;
	    		break;
		}
	}

	if (enum_string == NULL)
		sprintf(buf, "INTEGER: %d", *var->val.integer);
	else
		sprintf(buf, "INTEGER: %s(%d)", enum_string, *var->val.integer);
}

static void
sprint_gauge(buf, var)
	char *buf;
	struct variable_list *var;
{

	if (var->type != GAUGE) {
		sprintf(buf, "Wrong Type (should be Gauge): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "Gauge: %lu", *var->val.integer);
}

static void
sprint_counter(buf, var)
	char *buf;
	struct variable_list *var;
{

	if (var->type != COUNTER){
		sprintf(buf, "Wrong Type (should be Counter): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "Counter: %lu", *var->val.integer);
}

static void
sprint_networkaddress(buf, var)
	char *buf;
	struct variable_list *var;
{
	int x, len;
	char *cp;

	sprintf(buf, "Network Address:\t");
	buf += strlen(buf);
	cp = var->val.string;    
	len = var->val_len;
	for(x = 0; x < len; x++){
		sprintf(buf, "%02X", *cp++);
		buf += strlen(buf);
		if (x < (len - 1)) {
			*buf = ':';
			buf++;
		}
	}
}

static void
sprint_ipaddress(buf, var)
	char *buf;
	struct variable_list *var;
{
	char *ip;

	if (var->type != IPADDRESS){
		sprintf(buf, "Wrong Type (should be Ipaddress): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	ip = var->val.string;
	sprintf(buf, "IpAddress:\t%d.%d.%d.%d",ip[0], ip[1], ip[2], ip[3]);
}

static void
sprint_unsigned_short(buf, var)
	char *buf;
	struct variable_list *var;
{
	if (var->type != ASN_INTEGER){
		sprintf(buf, "Wrong Type (should be INTEGER): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "INTEGER (0..65535): %lu", *var->val.integer);
}

static void
sprint_null(buf, var)
	char *buf;
	struct variable_list *var;
{
	if (var->type != ASN_NULL){
		sprintf(buf, "Wrong Type (should be NULL): ");
		buf += strlen(buf);
		sprint_by_type(buf, var, (struct enum_list *)NULL);
		return;
	}
	sprintf(buf, "NULL");
}

static void
sprint_badtype(buf)
	char *buf;
{
	sprintf(buf, "Variable has bad type");
}

static void
sprint_by_type(buf, var, enums)
char *buf;
	struct variable_list *var;
	struct enum_list	    *enums;
{
	switch (var->type){
		case ASN_INTEGER:
			sprint_integer(buf, var, enums);
			break;
		case ASN_OCTET_STR:
			sprint_octet_string(buf, var);
			break;
		case OPAQUE:
			sprint_opaque(buf, var);
			break;
		case ASN_OBJECT_ID:
			sprint_object_identifier(buf, var);
			break;
		case TIMETICKS:
			sprint_timeticks(buf, var);
			break;
		case GAUGE:
			sprint_gauge(buf, var);
			break;
		case COUNTER:
			sprint_counter(buf, var);
			break;
		case IPADDRESS:
			sprint_ipaddress(buf, var);
			break;
		case ASN_NULL:
			sprint_null(buf, var);
			break;
		default:
			sprint_badtype(buf);
			break;
	}
}

/*************************************************************/
static struct tree *Mib = 0;
static struct tree *Subroot = 0;
static int Subroot_oidlen = 0;
static oid *Subroot_oid = 0;
static char *Subroot_text = 0;

static struct tree *
find_subtree(struct tree *root, register oid *op, int oidlen)
{
	register struct tree *tp;
	register int len;

	for (len = oidlen; len; len--, op++) {
		for (tp = root; tp; tp = tp->next_peer) {
	    		if (tp->subid == *op) {
				root = tp->child_list;
				break;
	    		}
		}
		if (tp == NULL)
	    		return(NULL);
	}
	return(root);
}

struct tree *
init_mib(char *mib_text,
	 oid *mib_oid,
	 int mib_oidlen,
	 char *mfile)
{
	if (!mfile) {
		sm_log(LOG_EMERG, 0, "init_mib: mibfile = NULL");
		exit(2);
	}

	Mib = read_mib(mfile);
	if (Mib == NULL) {
		sm_log(LOG_EMERG, 0, "READ_MIB FAILED");
		exit(2);
	}
	set_functions(Mib);

	Subroot = find_subtree(Mib, mib_oid, mib_oidlen);
	Subroot_oid = mib_oid;
	Subroot_oidlen = mib_oidlen;
	Subroot_text = mib_text;
	return(Mib);
}

static void
set_functions(subtree)
	struct tree *subtree;
{
	for(; subtree; subtree = subtree->next_peer){
		switch(subtree->type){
	    	case TYPE_OBJID:
			subtree->printer = sprint_object_identifier;
			break;
	    	case TYPE_OCTETSTR:
			subtree->printer = sprint_octet_string;
			break;
	    	case TYPE_INTEGER:
			subtree->printer = sprint_integer;
			break;
	    	case TYPE_NETADDR:
			subtree->printer = sprint_networkaddress;
			break;
	    	case TYPE_IPADDR:
			subtree->printer = sprint_ipaddress;
			break;
	    	case TYPE_COUNTER:
			subtree->printer = sprint_counter;
			break;
	    	case TYPE_GAUGE:
			subtree->printer = sprint_gauge;
			break;
	    	case TYPE_TIMETICKS:
			subtree->printer = sprint_timeticks;
			break;
	    	case TYPE_OPAQUE:
			subtree->printer = sprint_opaque;
			break;
	    	case TYPE_NULL:
			subtree->printer = sprint_null;
			break;
	    	case TYPE_OTHER:
	    	default:
			subtree->printer = sprint_badtype;
			break;
		}
		set_functions(subtree->child_list);
	}
}

int
read_objid(input, output, out_len)
	char *input;
	oid *output;
	int *out_len;   	/* number of subid's in "output" */
{
	int i;
	oid *op = output;
	struct tree *root = Mib;

	if (*input == '.')
		input++;
	else {
		root = Subroot;
		for (i = 0; i < Subroot_oidlen/sizeof(oid); i++) {
	   	 	if ((*out_len)-- <= 0) {
				sm_log(LOG_ERR, 0,
					"object identifier too long");
				return(0);
			}
			*output++ = Subroot_oid[i];
		}
	}

	if (root == NULL){
		sm_log(LOG_EMERG, 0, "Mib not initialized.  Exiting.");
		exit(1);
	}

	if ((*out_len = parse_subtree(root, input, output, out_len)) == 0)
		return(0);
	*out_len += output - op;

	return(1);
}

static int
parse_subtree(subtree, input, output, out_len)
	struct tree *subtree;
	char *input;
	oid	*output;
	int	*out_len;   /* number of subid's */
{
	char buf[128], *to = buf;
	int subid = 0;
	struct tree *tp;

	/*
 	 * No empty strings.  Can happen if there is a trailing '.' or two '.'s
 	 * in a row, i.e. "..".
 	 */
	if ((*input == '\0') || (*input == '.'))
		return (0);
	
	if (isdigit(*input)) {
		/*
	 	 * Read the number, then try to find it in the subtree.
	 	 */
		while (isdigit(*input)) {
	    		subid *= 10;
	    		subid += *input++ - '0';
		}
		for (tp = subtree; tp; tp = tp->next_peer) {
	    		if (tp->subid == subid)
				goto found;
		}
		tp = NULL;
	} else {
		/*
	 	 * Read the name into a buffer.
	 	 */
		while ((*input != '\0') && (*input != '.')) {
	    		*to++ = *input++;
		}
		*to = '\0';

		/*
	 	 * Find the name in the subtree;
	 	 */
		for (tp = subtree; tp; tp = tp->next_peer) {
	    		if (lc_cmp(tp->label, buf) == 0) {
				subid = tp->subid;
				goto found;
	    		}
		}
	
		/*
	 	 * If we didn't find the entry, punt...
	 	 */
		if (tp == NULL) {
	    		sm_log(LOG_ERR, 0,"sub-identifier not found: %s", buf);
	    		return (0);
		}
	}

found:
	if (subid > MAX_SUBID) {
		sm_log(LOG_ERR, 0,"sub-identifier too large: %s", buf);
		return (0);
	}

	if ((*out_len)-- <= 0) {
		sm_log(LOG_ERR, 0,"object identifier too long");
		return (0);
	}
	*output++ = subid;

	if (*input != '.')
		return (1);

	input++;
	if (tp == NULL)
		*out_len = parse_subtree(NULL, input, output, out_len);
	else
		*out_len = parse_subtree(tp->child_list,input,output,out_len);

	if (*out_len == 0)
		return(0);
	return(++*out_len);
}

void
print_objid(objid, objidlen)
	oid *objid;
	int objidlen;		/* number of subidentifiers */
{
	char buf[256];

	*buf = '.';		/* this is a fully qualified name */
	get_symbol(objid, objidlen, Mib, &buf[1]);
	SNMPDEBUG((LOG_DBGSNMP, 0, "%s", buf));
}

void
sprint_objid(buf, objid, objidlen)
	char *buf;
	oid  *objid;
	int  objidlen;		/* number of subidentifiers */
{
	*buf = '.';		/* this is a fully qualified name */
	get_symbol(objid, objidlen, Mib, &buf[1]);
}


void
print_variable(objid, objidlen, variable)
	oid *objid;
	int  objidlen;
	struct variable_list *variable;
{
	char buf[512], *cp;
	struct tree  *subtree;

	*buf = '.';		/* this is a fully qualified name */
	subtree = get_symbol(objid, objidlen, Mib, &buf[1]);
	cp = buf;
	if ((strlen(buf) >= strlen(Subroot_text)) &&
		!bcmp(buf, Subroot_text, strlen(Subroot_text))) {
	    	cp += strlen(Subroot_text);
	}

	SNMPDEBUG((LOG_DBGSNMP, 0, "Name: %s", cp));
	*buf = '\0';
	if (subtree->printer)
		(*subtree->printer)(buf, variable, subtree->enums);
	else
		sprint_by_type(buf, variable, subtree->enums);
	SNMPDEBUG((LOG_DBGSNMP, 0, "%s", buf));
}

void
sprint_variable(buf, objid, objidlen, variable)
	char *buf;
	oid *objid;
	int objidlen;
	struct variable_list *variable;
{
	char tempbuf[512], *cp;
	struct tree *subtree;

	*tempbuf = '.';		/* this is a fully qualified name */
	subtree = get_symbol(objid, objidlen, Mib, &tempbuf[1]);
	cp = tempbuf;

	if ((strlen(buf) >= strlen(Subroot_text)) &&
		!bcmp(buf, Subroot_text, strlen(Subroot_text))) {
 		cp += strlen(Subroot_text);
	}

	sprintf(buf, "Name: %s\n", cp);
	buf += strlen(buf);

	if (subtree->printer)
		(*subtree->printer)(buf, variable, subtree->enums);
	else
		sprint_by_type(buf, variable, subtree->enums);
	strcat(buf, "\n");
}

void
sprint_value(buf, objid, objidlen, variable)
	char *buf;
	oid *objid;
	int objidlen;
	struct variable_list *variable;
{
	char tempbuf[512];
	struct tree *subtree;

	subtree = get_symbol(objid, objidlen, Mib, tempbuf);
	if (subtree->printer)
		(*subtree->printer)(buf, variable, subtree->enums);
	else
		sprint_by_type(buf, variable, subtree->enums);
}

void
print_value(objid, objidlen, variable)
	oid *objid;
	int objidlen;
	struct variable_list *variable;
{
	char tempbuf[512];
	struct tree *subtree;

	subtree = get_symbol(objid, objidlen, Mib, tempbuf);
	if (subtree->printer)
		(*subtree->printer)(tempbuf, variable, subtree->enums);
	else
		sprint_by_type(tempbuf, variable, subtree->enums);
	SNMPDEBUG((LOG_DBGSNMP, 0, "%s", tempbuf));
}

struct tree *
get_symbol(objid, objidlen, subtree, buf)
	oid *objid;
	int objidlen;
	struct tree *subtree;
	char *buf;
{
	struct tree *return_tree = NULL;

	for (; subtree; subtree = subtree->next_peer) {
		if (*objid == subtree->subid) {
	    		strcpy(buf, subtree->label);
	    		goto found;
		}
	}

	/* subtree not found */
	while (objidlen--) {		/* output rest of name, uninterpreted */
		sprintf(buf, "%d.", *objid++);
		while(*buf)
	    		buf++;
	}
	*(buf - 1) = '\0'; 		/* remove trailing dot */
	return NULL;

found:
	if (objidlen > 1){
		while(*buf)
	    		buf++;
		*buf++ = '.';
		*buf = '\0';
		return_tree =
			get_symbol(objid+1,objidlen-1,subtree->child_list,buf);
	} 

	if (return_tree != NULL)
		return return_tree;
	else
		return subtree;
}


static int
lc_cmp(s1, s2)
	char *s1, *s2;
{
	char c1, c2;

	while(*s1 && *s2){
		if (isupper(*s1))
	    		c1 = tolower(*s1);
		else
	    		c1 = *s1;
		if (isupper(*s2))
	    		c2 = tolower(*s2);
		else
	    		c2 = *s2;
		if (c1 != c2)
	    		return ((c1 - c2) > 0 ? 1 : -1);
		s1++;
		s2++;
	}

	if (*s1)
		return -1;
	if (*s2)
		return 1;
	return 0;
}
