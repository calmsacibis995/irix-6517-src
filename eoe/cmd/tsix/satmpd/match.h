#ifndef MATCH_HEADER
#define MATCH_HEADER

#ident "$Revision: 1.1 $"

/*
 * Return appropriate match function by attribute type
 */
typedef int (*match_fn) (const void *, const void *);
match_fn match_attribute (u_short);

/*
 * CLIENT MATCH FUNCTIONS
 */
int match_host_byid (const void *, const void *);
int match_attrdot_byid (const void *, const void *);
int match_tserver_byid (const void *, const void *);
int match_tdomain_byname (const void *, const void *);
int match_domain_byname (const void *, const void *);
int match_attrtoken_byid (const void *, const void *);
int match_token (const void *, const void *);
int match_attrwt_byid (const void *, const void *);
int match_weight_bydomain (const void *, const void *);
int match_weight_byvalue (const void *, const void *);

/*
 * SERVER MATCH FUNCTIONS
 */
int match_attrname_byid (const void *, const void *);
int match_attrname_byname (const void *, const void *);
int match_attrval_bytoken (const void *, const void *);

/*
 * ATTRIBUTE MAPPING MATCH FUNCTIONS
 */
int match_map_head_byid (const void *, const void *);
int match_map_domain_bydomain (const void *, const void *);
int match_map_bysource (const void *, const void *);
int match_map_asuser (const void *, const void *);
int match_map_asgroup (const void *, const void *);
int match_map_aslbltype (const void *, const void *);
int match_map_aslevel (const void *, const void *);
int match_map_ascategory (const void *, const void *);

#endif /* MATCH_HEADER */
