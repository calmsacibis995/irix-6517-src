/*
 * stores and processes dhcp option value combinations
 */
#ifndef _dhcp_optvals_h
#define _dhcp_optvals_h

typedef void (*dhcp_optval_encode) (int tag, void *src, char **dst, int len);

struct dhcp_optval_field {
    void *vp_value;		/* value of the option */
    int i_len;			/* lenght to copy */
    dhcp_optval_encode doep_encoder; /* encoding function  */
};

struct dhcp_optval {
    int i_opt_num;
    struct dhcp_optval_field dof_fields[MAX_OPT_TYPES];
    struct dhcp_optval *dop_next;
};

struct vendor_options {
    int original;
    char *cp_vendor_tag;
    struct dhcp_optval *dop_optval;
    struct vendor_options *vop_next;
};

#define S_VENDOR_TAG "vendor_tag"
#define S_VENDOR_NETNUM "netnum"
#define S_VENDOR_NETMASK "netmask"
#define S_FSEP "fsep"
#define S_ASEP "asep"

#endif /* _dhcp_optvals_h */
