/*
 * dhcp_types.h
 */

#ifndef _dhcp_types_h
#define _dhcp_types_h

/* various types that can be read from the file */

typedef enum option_types { 
    D_UNKNOWN,
    D_INT8,
    D_INT8ARR,
    D_INT16,
    D_INT16ARR,
    D_INT32,
    D_INT32ARR,
    D_IPADDR,
    D_IPADDRARR,
    D_IPADDRPAIR,
    D_IPADDRPAIRARR,
    D_STRING
} option_types_t;


#define MAX_OPT_TYPES 8

struct dhcp_option {
    char *cp_opt_name;
    int	i_opt_num;
    option_types_t ot_types[MAX_OPT_TYPES];
};

int lookup_dhcp_option(char *cp_name, struct dhcp_option **dop_do);

#endif /* _dhcp_types_h */
