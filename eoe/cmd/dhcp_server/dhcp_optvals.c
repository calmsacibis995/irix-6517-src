/*
 * dhcp_optvals.c - reads options from files and set up
 * encoding functions etc
 */
#define _SGI_REENTRANT_FUNCTIONS

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "dhcp_types.h"
#include "dhcp_optvals.h"
#include "configdefs.h"

unsigned long inet_addr(const char *cp);


typedef void* (*type_read_function) (char *data, char *array_sep, int *len);
void *fget_unknown(char *data, char *asep, int *len);
void *fget_int8(char *data, char *asep, int *len);
void *fget_int8arr(char *data, char *asep, int *len);
void *fget_int16(char *data, char *asep, int *len);
void *fget_int16arr(char *data, char *asep, int *len);
void *fget_int32(char *data, char *asep, int *len);
void *fget_int32arr(char *data, char *asep, int *len);
void *fget_ipaddr(char *data, char *asep, int *len);
void *fget_ipaddrarr(char *data, char *asep, int *len);
void *fget_ipaddrpair(char *data, char *asep, int *len);
void *fget_ipaddrpairarr(char *data, char *asep, int *len);
void *fget_string(char *data, char *asep, int *len);

/* the order should match the definitions of types in dhcp_types.h - 
 * option_types
 */
type_read_function type_read_funcs[] = {
	fget_unknown,
	fget_int8,
	fget_int8arr,
	fget_int16,
	fget_int16arr,
	fget_int32,
	fget_int32arr,
	fget_ipaddr,
	fget_ipaddrarr,
	fget_ipaddrpair,
	fget_ipaddrpairarr,
	fget_string
	};

#define FILE_DHCP_VENDOR_OPTVALS "/var/dhcp/config/vendor_options"

extern FILE *log_fopen(char *, char *);
extern int insert_vendor_option_into_config(struct vendor_options *vop_opt, 
					    u_long netnum, u_long netmask);

/*
 * keep a list of all vendor options allocated in an array
 */
static struct vendor_options **vop_opts = NULL;
static int vop_opts_size = 8;
static int vop_opts_num = 0;

void
insert_vop_opt(struct vendor_options *vop_opt)
{
    struct vendor_options **m_vop_opts;

    if ((vop_opts == NULL) || (vop_opts_num == vop_opts_size)) {
	vop_opts_size *= 2;
	m_vop_opts = malloc(vop_opts_size * sizeof(struct vendor_options*));
	if (m_vop_opts == NULL) {
	    syslog(LOG_ERR, "Out of memory in insert_vop_opt");
	    exit(1);
	}
	if (vop_opts) {
	    memcpy(m_vop_opts, vop_opts, vop_opts_size/2);
	    vop_opts = m_vop_opts;
	}
	else {
	    vop_opts = m_vop_opts;
	}
    }
    vop_opts[vop_opts_num] = vop_opt;
    vop_opts_num++;
}
/* save the vendor option information into the config list
 */
int insert_vendor_option_into_config(struct vendor_options *vop_opt, 
				     u_long netnum, u_long netmask)
{
    int use_count;
    dhcpConfig *cfgPtr;
    struct vendor_options *orig_vop_opt = vop_opt;
    struct vendor_options *new_vop_opt;

    use_count = 0;
    cfgPtr = configListPtr;
    while (cfgPtr) {
	if ( ((netnum & netmask) == (cfgPtr->p_netnum & cfgPtr->p_netmask))
	     || (netnum == 0) ) {
	    if (use_count) {
		if ( (new_vop_opt = malloc(sizeof(struct vendor_options))) == NULL) {
		    syslog(LOG_ERR, "Out of memory in insert_vendor_option_into_config");
		    exit(1);
		}
		memcpy(new_vop_opt, orig_vop_opt, 
		       sizeof(struct vendor_options));
		new_vop_opt->original = 0;
		new_vop_opt->vop_next = 0;
		vop_opt = new_vop_opt;
		insert_vop_opt(new_vop_opt);
	    }
	    if (cfgPtr->vop_options) {
		vop_opt->vop_next = cfgPtr->vop_options;
		cfgPtr->vop_options = vop_opt;
		use_count++;
	    }
	    else {
		cfgPtr->vop_options = vop_opt;
		use_count++;
	    }
	}
	cfgPtr = cfgPtr->p_next;
    }
    return 0;
}

static void
free_dhcp_optval(struct dhcp_optval *dop_optval)
{
    struct dhcp_optval *dop, *dop_free;
    int index;

    dop = dop_optval;
    while (dop) {
	dop_free = dop;
	dop = dop_free->dop_next;
	index = 0;
	while (index < MAX_OPT_TYPES) {
	    if (dop_free->dof_fields[index].vp_value)
		free(dop_free->dof_fields[index].vp_value);
	    index++;
	}
	free(dop_free);
    }
}

void
free_vendor_options(void)
{
    struct vendor_options *vop;
    int i;

    if (vop_opts) {
	for (i = 0; i < vop_opts_num; i++) {
	    vop = vop_opts[i];
	    if (vop->original) {
		if (vop->cp_vendor_tag) {
		    free(vop->cp_vendor_tag); 
		    vop->cp_vendor_tag = NULL; 
		}
		if (vop->dop_optval) {
		    free_dhcp_optval(vop->dop_optval); 
		    vop->dop_optval = NULL; 
		}
	    }
	    vop->vop_next = NULL;
	    free(vop);
	    vop_opts[i] = NULL;
	}
    }
    vop_opts_num = 0;
}
		
		


void *
fget_unknown(char *data, char *sep, int *i_len)
{
    syslog(LOG_ERR, "Unknown data type in %s file", FILE_DHCP_VENDOR_OPTVALS);
    return 0;
}

void *
fget_int8(char *data, char *sep, int *i_len)
{
    char *p;
    if ( (p = malloc(sizeof(u_char))) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int8");
	return p;
    }
    *p = (int8_t)strtol(data, (char**)NULL, 0);
    *i_len = sizeof(int8_t);
    return p;
}

void *
fget_int8arr(char *data, char *sep, int *i_len)
{
    char *p, *cp;
    char *cp_m;
    int i_l, i_num;
    char *cp_last;

    cp_m = strpbrk(data, sep);
    i_l = 1;
    while (cp_m) {
	i_l++;
	cp_m = strpbrk(++cp_m, sep);
    }
    if ( (cp = p = malloc(sizeof(int8_t) * i_l)) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int8arr");
	return p;
    }
    i_num = 0;
    cp_last = NULL;
    cp_m = strtok_r(data, sep, &cp_last);
    while ( cp_m && (i_num < i_l) ) {
	i_num++;
	*p++ = (int8_t) strtol(cp_m, (char**)NULL, 0);
	cp_m = strtok_r(0, sep, &cp_last);
    }
    *i_len = i_l;
    return cp;
}

void *
fget_int16(char *data, char *sep, int *i_len)
{
    char *p;
    int16_t d;
    if ( (p = malloc(sizeof(int16_t))) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int16");
	return p;
    }
    d = (int16_t)strtol(data, (char**)NULL, 0);
    d = htons(d);
    memcpy(p, &d, sizeof(int16_t));
    *i_len = sizeof(int16_t);
    return p;
    
}
void *
fget_int16arr(char *data, char *sep, int *i_len)
{
    char *p, *cp;
    char *cp_m;
    int i_l, i_num;
    int16_t i_data;
    char *cp_last;

    cp_m = strpbrk(data, sep);
    i_l = 1;
    while (cp_m) {
	i_l++;
	cp_m = strpbrk(++cp_m, sep);
    }
    if ( (cp = p = malloc(sizeof(int16_t) * i_l)) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int16arr");
	return p;
    }
    i_num = 0;
    cp_last = NULL;
    cp_m = strtok_r(data, sep, &cp_last);
    while ( cp_m && (i_num < i_l) ) {
	i_num++;
	i_data = (int16_t) strtol(cp_m, (char**)NULL, 0);
	i_data = htons(i_data);
	memcpy(p, &i_data, sizeof(int16_t));
	p += sizeof(int16_t);
	cp_m = strtok_r(0, sep, &cp_last);
    }
    *i_len = i_num * sizeof(int16_t);
    return cp;
    
}
void *
fget_int32(char *data, char *sep, int *i_len)
{
    char *p;
    int32_t d;
    if ( (p = malloc(sizeof(int32_t))) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int32");
	return p;
    }
    d = (int32_t)strtol(data, (char**)NULL, 0);
    d = htonl(d);
    memcpy(p, &d, sizeof(int32_t));
    *i_len = sizeof(int32_t);
    return p;
}
void *
fget_int32arr(char *data, char *sep, int *i_len)
{
    char *p, *cp;
    char *cp_m;
    int i_l, i_num;
    int32_t i_data;
    char *cp_last;

    cp_m = strpbrk(data, sep);
    i_l = 1;
    while (cp_m) {
	i_l++;
	cp_m = strpbrk(++cp_m, sep);
    }
    if ( (cp = p = malloc(sizeof(int32_t) * i_l)) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int32arr");
	return p;
    }
    i_num = 0;
    cp_last = NULL;
    cp_m = strtok_r(data, sep, &cp_last);
    while ( cp_m && (i_num < i_l) ) {
	i_num++;
	i_data = (int32_t) strtol(cp_m, (char**)NULL, 0);
	i_data = htonl(i_data);
	memcpy(p, &i_data, sizeof(int32_t));
	p += sizeof(int32_t);
	cp_m = strtok_r(0, sep, &cp_last);
    }
    *i_len = i_num * sizeof(int32_t);
    return cp;
}

void *
fget_ipaddr(char *data, char *sep, int *i_len)
{
    uint32_t addr;
    void *p;

    if (*data == 0)
	addr = strtol(data, NULL, 0);
    else
	addr = inet_addr(data);
    addr = htonl(addr);
    if ( (p = malloc(sizeof(addr))) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_int8");
	return p;
    }
    memcpy(p, &addr, sizeof(addr));
    *i_len = sizeof(addr);
    return p;
}

void *
fget_ipaddrarr(char *data, char *sep, int *i_len)
{
    char *p, *cp;
    char *cp_m;
    int i_l, i_num;
    uint32_t addr;
    char *cp_last;

    cp_m = strpbrk(data, sep);
    i_l = 1;
    while (cp_m) {
	i_l++;
	cp_m = strpbrk(++cp_m, sep);
    }
    if ( (cp = p = malloc(sizeof(uint32_t) * i_l)) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_ipaddrarr");
	return p;
    }
    i_num = 0;
    cp_last = NULL;
    cp_m = strtok_r(data, sep, &cp_last);
    while ( cp_m && (i_num < i_l) ) {
	i_num++;
	if (*cp_m == 0)
	    addr = strtol(cp_m, NULL, 0);
	else
	    addr = inet_addr(cp_m);
	addr = htonl(addr);
	memcpy(p, &addr, sizeof(uint32_t));
	p += sizeof(uint32_t);
	cp_m = strtok_r(0, sep, &cp_last);
    }
    *i_len = i_num * sizeof(uint32_t);
    return cp;
}
void *
fget_ipaddrpair(char *data, char *sep, int *i_len)
{

    uint32_t addr;
    char *p, *cp;
    char *cp_last;

    cp_last = NULL;
    data = strtok_r(data, "- \t\n", &cp_last);
    if (*data == 0)
	addr = strtol(data, NULL, 0);
    else
	addr = inet_addr(data);
    addr = htonl(addr);
    if ( (cp = p = malloc(sizeof(addr) * 2)) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_ipaddrpair");
	return p;
    }
    memcpy(p, &addr, sizeof(addr));
    data = strtok_r(0, sep, &cp_last);
    if (*data == 0)
	addr = strtol(data, NULL, 0);
    else
	addr = inet_addr(data);
    addr = htonl(addr);
    memcpy(p + sizeof(addr), &addr, sizeof(addr));
    *i_len = sizeof(addr) * 2;
    return cp;
    
}
void *
fget_ipaddrpairarr(char *data, char *sep, int *i_len)
{
    char *p, *cp;
    char *cp_m, *cp_m1;
    int i_l, i_num;
    uint32_t addr;
    char *cp_last, *cp_last1;

    cp_m = strpbrk(data, sep);
    i_l = 1;
    while (cp_m) {
	i_l++;
	cp_m = strpbrk(++cp_m, sep);
    }
    if ( (cp = p = malloc(sizeof(uint32_t) * 2 * i_l)) == NULL) {
	syslog(LOG_ERR, "Out of memory in fget_ipaddrpairarr");
	return p;
    }
    i_num = 0;
    cp_last = NULL;
    cp_m = strtok_r(data, sep, &cp_last);
    while ( cp_m && (i_num < i_l) ) {
	cp_last1 = NULL;
	cp_m1 = strtok_r(cp_m, "- \t\n", &cp_last1);
	i_num++;
	if (cp_m1 && *cp_m1 == 0)
	    addr = strtol(cp_m1, NULL, 0);
	else
	    addr = inet_addr(cp_m1);
	addr = htonl(addr);
	memcpy(p, &addr, sizeof(uint32_t));
	p += sizeof(uint32_t);
	cp_m1 = strtok_r(0, "- \t\n", &cp_last1);
	if (cp_m1 && *cp_m1 == 0)
	    addr = strtol(cp_m1, NULL, 0);
	else
	    addr = inet_addr(cp_m1);
	addr = htonl(addr);
	memcpy(p, &addr, sizeof(uint32_t));
	p += sizeof(uint32_t);
	cp_m = strtok_r(0, sep, &cp_last);
    }
    *i_len = i_num * 2 * sizeof(uint32_t);
    return cp;

}

void *
fget_string(char *data, char *sep, int *i_len)
{
    char *p;
    p = strdup(data);
    if (p == NULL) 
	syslog(LOG_ERR, "Out of memory in fget_string");
    *i_len  = strlen(data);
    return p;
}

/*
 * general encoder function
 */
void
bcopy_value(int tag, void *src, char **dst, int len)
{
    char *p = *dst;

    memcpy(p, src, len);
    p += len;
    *dst = p;
}

/*
 * read configuration file for dhcp vendor options
 */
void
read_dhcp_vendor_optvals(void)
{
    FILE	*fp;
    char	line[256];
    int		linenum;
    char	*linep;		/* pointer to 'line' */
    int		i, index;
    struct	vendor_options *vop_opt;
    struct	dhcp_optval *dop_vendor;
    char 	*cp_vendor_tag;
    u_long	netnum, netmask;
    struct	dhcp_option *dop_dhcpopt;
    char	asep[2], fsep[2];
    u_long	*dst;
    char	*cp_lastf;
    char	*flinep;
    int		i_len;
    int		read_some_options;

    fp = log_fopen(FILE_DHCP_VENDOR_OPTVALS, "r");
    if(fp == NULL)
	return ;
    cp_vendor_tag = (char*)0;
    netnum = 0;
    netmask = 0xffffffff;
    strcpy(fsep,";");
    strcpy(asep,",");
    vop_opt = (struct vendor_options*)0;
    read_some_options = 0;

    while (fgets(line, 1024, fp) != NULL) {
	if ( (i = strlen(line)) && (line[i-1] == '\n') )
	    line[i-1] = 0;	/* remove trailing newline */
	linep = line;
	linenum++;
	if (line[0] == '#' || line[0] == 0 || line[0] == ' ')
	    continue;	/* skip comment lines */
	cp_lastf = (char*)0;
	linep = strtok_r(line, ":", &cp_lastf);
	if (!linep) {
	    syslog(LOG_ERR, "Error in option name on line %d: %s", 
		   linenum, linep);
	    continue;
	}
	/* There should be a vendor tag here If there is no vendor
	 * tag set as yet we just skip lines */
	if ( (strcmp(linep, S_VENDOR_TAG) == 0) || !cp_vendor_tag) {
	    if (read_some_options) {
		/* we must first store the option we just assembled
		 * before reading the next one */
		insert_vendor_option_into_config(vop_opt, netnum, netmask);
		read_some_options = 0;
	    }
	    if (strcmp(linep, S_VENDOR_TAG) != 0)
		continue;
	    linep = strtok_r(0, ":\n \t", &cp_lastf);
	    cp_vendor_tag = strdup(linep);
	    if ( (vop_opt = malloc(sizeof(struct vendor_options))) == NULL) {
		syslog(LOG_ERR, "Out of memory in read_dhcp_vendor_optvals");
		exit(1);
	    }
	    memset(vop_opt, '\0', sizeof(struct vendor_options));
	    vop_opt->cp_vendor_tag = cp_vendor_tag;
	    vop_opt->original = 1;
	    insert_vop_opt(vop_opt);
	    continue;
	}
	if (strcmp(linep, S_VENDOR_NETNUM) == 0) {
	    if (read_some_options) {
		/* we must first store the option we just assembled
		 * before reading the next one */
		insert_vendor_option_into_config(vop_opt, netnum, netmask);
		if ( (vop_opt = malloc(sizeof(struct vendor_options))) == NULL) {
		    syslog(LOG_ERR, "Out of memory in read_dhcp_vendor_optvals");
		    exit(1);
		}
		memset(vop_opt, '\0', sizeof(struct vendor_options));
		vop_opt->cp_vendor_tag = strdup(cp_vendor_tag);
		vop_opt->original = 1;
		insert_vop_opt(vop_opt);
		read_some_options = 0;
	    }
	    linep = strtok_r(0, ":\n \t", &cp_lastf);
	    dst = fget_ipaddr(linep, ":", &i_len);
	    netnum = *dst;
	    if (dst)
		free(dst);
	    continue;
	}
	if (strcmp(linep, S_VENDOR_NETMASK) == 0) {
	    if (read_some_options) {
		/* we must first store the option we just assembled
		 * before reading the next one */
		insert_vendor_option_into_config(vop_opt, netnum, netmask);
		if ( (vop_opt = malloc(sizeof(struct vendor_options))) == NULL) {
		    syslog(LOG_ERR, "Out of memory in read_dhcp_vendor_optvals");
		    exit(1);
		}
		memset(vop_opt, '\0', sizeof(struct vendor_options));
		vop_opt->cp_vendor_tag = strdup(cp_vendor_tag);
		vop_opt->original = 1;
		insert_vop_opt(vop_opt);
		read_some_options = 0;
	    }
	    linep = strtok_r(0, ":\n \t", &cp_lastf);
	    dst = fget_ipaddr(linep, ":", &i_len);
	    netmask = *dst;
	    if (dst)
		free(dst);
	    continue;
	}
	if (strcmp(linep, S_FSEP) == 0) {
	    linep = strtok_r(0, ":\n", &cp_lastf);
	    if (linep != NULL)
		strcpy(fsep, linep);
	    continue;
	}
	if (strcmp(linep, S_ASEP) == 0) {
	    linep = strtok_r(0, ":\n", &cp_lastf);
	    if (linep != NULL)
		strcpy(asep, linep);
	    continue;
	}
	/* if we got here it means this must be a vendor option 
	 * <optname>:<fsep><asep>:<value><fsep><array_field>... 
	 * <field> = <value><asep><value>.... */
	/* find the option number from the name */
	if (lookup_dhcp_option(linep, &dop_dhcpopt) == 0) {/* found */
	    if ( (dop_vendor = malloc(sizeof(struct dhcp_optval))) == NULL) {
		syslog(LOG_ERR, "Out of memory in read_dhcp_vendor_optvals");
		exit(1);
	    }
	    memset(dop_vendor, '\0', sizeof(struct dhcp_optval));
	    dop_vendor->i_opt_num = dop_dhcpopt->i_opt_num;
	    linep = strtok_r(0, "\n", &cp_lastf); /* end of line */
	    cp_lastf = 0;
	    flinep = strtok_r(linep, fsep, &cp_lastf);
	    index = 0;
	    while (flinep && (index < MAX_OPT_TYPES) ) {
		if (dop_dhcpopt->ot_types[index] == 0) {
		    syslog(LOG_ERR, "vendor_options:Extra data on line %d", 
			   linenum);
		    break;
		}
		dop_vendor->dof_fields[index].vp_value = 
		    type_read_funcs[dop_dhcpopt->
				   ot_types[index]](flinep, asep, &i_len);
		dop_vendor->dof_fields[index].i_len = i_len;
		dop_vendor->dof_fields[index].doep_encoder = bcopy_value;
		flinep = strtok_r(0, fsep, &cp_lastf);
		index++;
	    }
	    if (vop_opt->dop_optval == NULL)
		vop_opt->dop_optval = dop_vendor;
	    else {
		dop_vendor->dop_next = vop_opt->dop_optval;
		vop_opt->dop_optval = dop_vendor;
	    }
	    read_some_options = 1;
	}
	else 
	    syslog(LOG_ERR, 
		   "Error in option %s on line %d in dhcp_options_types file", 
		   linep, linenum);
    }
    if (read_some_options) 
	insert_vendor_option_into_config(vop_opt, netnum, netmask);
    fclose(fp);
}
	
