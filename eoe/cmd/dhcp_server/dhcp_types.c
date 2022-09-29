/*
 * dhcp_types.c - read the /var/dhcp/config/dhcp_types file and store
 * various options and their types in a hash table that is indexed by the
 * option name
 * The format of the dhcp_types file is:
 * <option_name>:<option_number>:<type1>,<type2>,...
 */
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "dhcp_types.h"

#ifndef uint32
# define uint32 unsigned int
#endif
extern void read_dhcp_vendor_optvals(void);
extern uint32 hash_buf(u_char *str, int len);
extern FILE *log_fopen(char *, char *);

#define DHCP_TYPES_FILE "/var/dhcp/config/dhcp_option_types"

struct map_types {
    char *cp_type_desc;
    option_types_t i_opt_type;
};

struct map_types mt_map[] = {
	{ "Int8", D_INT8},
	{ "Int8Array", D_INT8ARR},
	{ "Int16", D_INT16},
	{ "Int16Array", D_INT16ARR},
	{ "Int32", D_INT32},
	{ "Int32Array", D_INT32ARR},
	{ "IPAddr", D_IPADDR},
	{ "IPAddrArray", D_IPADDRARR},
	{ "IPAddrPair", D_IPADDRPAIR},
	{ "IPAddrPairArray", D_IPADDRPAIRARR},
	{ "String", D_STRING}
};

#define NUM_MT_MAP sizeof(mt_map)/sizeof(struct map_types)


typedef struct dhcp_type_ent {
    struct dhcp_option do_dt;
    struct dhcp_type_ent *dtep_link;
} *dhcp_type_list;

#define DHCP_TYPE_BUCKETS	128

static dhcp_type_list dtl_hash_dhcp_types[DHCP_TYPE_BUCKETS];
static int inited_dhcp_type_list = 0;

void
init_dhcp_type_list(void)
{
    int i;
    dhcp_type_list dtl, dtl_next;

    for (i = 0; i < DHCP_TYPE_BUCKETS; i++) {
	if (inited_dhcp_type_list) {
	    dtl = dtl_hash_dhcp_types[i];
	    while (dtl) {
		dtl_next = dtl->dtep_link;
		if (dtl->do_dt.cp_opt_name)
		    free(dtl->do_dt.cp_opt_name);
		free(dtl);
		dtl = dtl_next;
	    }
	}
	dtl_hash_dhcp_types[i] = (dhcp_type_list)0;
    }
    inited_dhcp_type_list = 1;
}

int
insertdhcptypeshash(dhcp_type_list *p_dhcp_type_list, struct dhcp_option *dop)
{
    struct dhcp_type_ent *p = (struct dhcp_type_ent*)
	malloc(sizeof(struct dhcp_type_ent));
    memset(p, '\0', sizeof(struct dhcp_type_ent));
    if (p == (struct dhcp_type_ent*)0)
	return -1;
    memcpy(&p->do_dt, dop, sizeof(struct dhcp_option));
    p->dtep_link = *p_dhcp_type_list;
    *p_dhcp_type_list = p;
    return 0;
}

int
insert_dhcp_types_hash(struct dhcp_option *do_option)
{
    if (insertdhcptypeshash(&dtl_hash_dhcp_types
			    [hash_buf((u_char*)do_option->cp_opt_name,
				      strlen(do_option->cp_opt_name)) %
			    DHCP_TYPE_BUCKETS], do_option) != 0)
	return -1;
    return 0;
}

void
read_dhcp_types(void)
{
    
    FILE	*fp;
    char	line[256];
    int		linenum;
    char	*linep;		/* pointer to 'line' */
    int		i;
    struct dhcp_option	do_dhcp_option;
    struct dhcp_option *dop_dhcp_option;
    int		i_tmp;
    char	*cp_l_type;
    int		index;

    dop_dhcp_option = &do_dhcp_option;
    linenum = 0;
    fp = log_fopen(DHCP_TYPES_FILE, "r");
    if(fp == NULL)
	return ;
    while (fgets(line, 1024, fp) != NULL) {
	if ( (i = strlen(line)) && (line[i-1] == '\n') )
	    line[i-1] = 0;	/* remove trailing newline */
	linep = line;
	linenum++;
	if (line[0] == '#' || line[0] == 0 || line[0] == ' ')
	    continue;	/* skip comment lines */
	memset(dop_dhcp_option, '\0', sizeof(struct dhcp_option));
	linep = strtok(line, ":");
	if (!linep) {
	    syslog(LOG_ERR, "Error in option name on line %d: %s", 
		   linenum, linep);
	    continue;
	}
	dop_dhcp_option->cp_opt_name = strdup(linep);
	linep = strtok(0, ":");
	if ( (!linep) || (sscanf(linep, "%i", &i_tmp) != 1) || 
	     (i_tmp < 0) || (i_tmp > 256) ) {
	    syslog(LOG_ERR, "Error in option number on line %d: %s",
		   linenum, linep);
	    continue;
	}
	dop_dhcp_option->i_opt_num = i_tmp;
	linep = strtok(0, ":");
	if  (!linep) {
	    syslog(LOG_ERR, "Error in option types on line %d: %s",
		   linenum, linep);
	    continue;
	}
	cp_l_type = strtok(linep, ",");
	index = 0;
	while (cp_l_type) {
	    for (i = 0; i < NUM_MT_MAP; i++) {
		if (strncasecmp(cp_l_type, mt_map[i].cp_type_desc, 
				strlen(cp_l_type)) == 0) {
		    dop_dhcp_option->ot_types[index] = mt_map[i].i_opt_type;
		    break;
		}
	    }
	    if (i == NUM_MT_MAP) {
		syslog(LOG_ERR, "Error in types on line %d: %s",
		       linenum, cp_l_type);
		continue;
	    }
	    cp_l_type = strtok(0, ",");
	    index++;
	}
	
	if (insert_dhcp_types_hash(dop_dhcp_option) != 0) {
	    syslog(LOG_ERR, "Error inserting entry on line %d", linenum);
	    exit(1);
	}
    }
    fclose(fp);
}

/* lookup for a option name in the hash table
 */
struct dhcp_option *
lookupdhcpoption(dhcp_type_list *dtlp_dhcp_type_list, char *k, int len)
{
    struct dhcp_type_ent *p = *dtlp_dhcp_type_list;

    if (p == (struct dhcp_type_ent*)0)
	return (struct dhcp_option*)0;
    for (;p;p = p->dtep_link) {
	if (memcmp(k, p->do_dt.cp_opt_name, len) == 0)
	    return &p->do_dt;
    }
    return (struct dhcp_option*)0;
}

int 
lookup_dhcp_option(char *cp_name, struct dhcp_option **dop_do)
{
    int i_len;
    
    i_len = strlen(cp_name);
    *dop_do = lookupdhcpoption(&dtl_hash_dhcp_types
				 [hash_buf((u_char*)cp_name, i_len) %
				 DHCP_TYPE_BUCKETS],
				 cp_name, i_len);
    if (*dop_do == (struct dhcp_option*)0) {
	return 1;
    }
    return 0;
}

int
cf0_initialize_vendor_class(void)
{
    init_dhcp_type_list();
    read_dhcp_types();
    read_dhcp_vendor_optvals();
    return 0;
}
