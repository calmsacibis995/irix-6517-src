#ifndef LIST_HEADER
#define LIST_HEADER

#ident "$Revision: 1.4 $"

#include "atomic.h"
#include "generic_list.h"
#include "ref.h"

/*
 * SATMPD CLIENT DATA STRUCTURES
 *
 * client			Generic_list attrwt
 *				Generic_list reqattr
 *				Generic_list hosts
 *				Generic_list servers
 *
 * attrwt			u_short attrid
 *				Generic_list weights
 * weights			char *domain
 *				u_char weight
 *
 * reqattr			u_short attrid
 *
 * hosts			u_int hostid
 *				u_int token_server
 *				Generic_list attrdot
 * attrdot			u_short attrid
 *				char *domain
 *
 * servers			u_int hostid
 *				atomic generation
 */

/*
 * Top-level client-side data structure
 */
struct client {
	struct ref ref_;
	Generic_list attrwt;
	Generic_list reqattr;
	Generic_list hosts;
	Generic_list servers;
};

/*
 * Local attribute weight database
 */
struct attrwt {
	struct ref ref_;
	u_short attrid;			/* identifier for this attribute
					   type */
	Generic_list weights;
};

struct weight {
	struct ref ref_;
	char *domain;			/* domain to translate */
	u_char weight;			/* how much we like this domain */
};

/*
 * Required attributes database
 */
struct reqattr {
	struct ref ref_;
	u_short attrid;			/* identifier for this attribute
					   type */
};

/*
 * Client-side token translation database
 */
struct host {
	struct ref ref_;
	u_int hostid;			/* id of client host */
	u_int token_server;		/* id of client's token server */
	Generic_list attrdots;
};

struct attrdot {
	struct ref ref_;
	u_short attrid;			/* identifier for this attribute
					   type */
	char *domain;			/* domain to translate */
};

struct tserver {
	struct ref ref_;
	u_int hostid;			/* id of token server */
	atomic generation;		/* generation of token server */
	Generic_list domains;		/* domains of token server */
};

struct tdomain {
	struct ref ref_;
	char *domain;
	Generic_list attributes;
};

struct attrtoken {
	struct ref ref_;
	u_short attrid;			/* identifier for this attribute */
	Generic_list tokens;		/* tokens + nrs */
};

struct token {
	struct ref ref_;
	u_int token;			/* token representing this attribute */
	u_short attrid;			/* identifier for this attribute */
	void *lrep;			/* local representation of this
					   attribute */
};

/*
 * SATMPD CLIENT INITIALIZE/DESTROY/PRINT FUNCTIONS
 */

struct client *initialize_client (void);
void print_client (struct client *);

struct host *initialize_host (u_int, u_int);

struct attrdot *initialize_attrdot (u_short, const char *);

struct tserver *initialize_tserver (u_int, u_int);

struct tdomain *initialize_tdomain (const char *);

struct attrtoken *initialize_attrtoken (u_short);

struct token *initialize_token (u_int, u_short, void *);

struct attrwt *initialize_attrwt (u_short);

struct weight *initialize_weight (const char *, u_char);

struct reqattr *initialize_reqattr (u_short);

/*
 * SATMPD SERVER DATA STRUCTURES
 *
 * server			u_int generation
 *				Generic_list attribute-list
 * attribute-list		char *name
 *				u_short attrid
 *				Generic_list domain-list
 * domain-list			char *name
 *				atomic seqno
 *				Generic_list attribute-values
 * attribute-values		void *lrep
 *				char *nrep
 *				size_t nrep_len (including trailing NUL)
 *				u_int token
 */

#define BAD_ATTRID	((u_short) ~0)

struct server {
	struct ref ref_;
	u_int generation;		/* generation number of database */
	Generic_list attribute_names;	/* attribute id<->name mappings */
};

struct attrname {
	struct ref ref_;
	char *name;
	u_short attrid;
	Generic_list domains;
};

struct domain {
	struct ref ref_;
	char *domain;			/* domain of translation */
	atomic seqno;			/* next token number */
	u_short attrid;
	Generic_list attrvals;		/* list of attributes in this
					   domain */
};

struct _lrep {
	struct ref ref_;
	void *_lrep;			/* local representation of this
					   attribute */
	size_t _lrep_len;		/* length in bytes of lrep */
};

struct _nrep {
	struct ref ref_;
	char *_nrep;			/* network representation of this
					   attribute */
	size_t _nrep_len;		/* length in bytes of nrep */
};

struct attrval {
	struct ref ref_;
	struct _lrep _lr;		/* local representation */
	struct _nrep _nr;		/* network representation */
	u_int token;			/* token representing this attribute */
	u_short attrid;
};

#define nrep		_nr._nrep
#define nrep_len	_nr._nrep_len

/*
 * SATMPD SERVER INITIALIZE/DESTROY/PRINT FUNCTIONS
 */

struct server *initialize_server (void);
void print_server (struct server *);

struct attrname *initialize_attrname (const char *, u_short);

struct domain *initialize_domain (const char *, u_short);

struct attrval *initialize_attrval (void *, size_t, char *, size_t,
				    atomic *, u_short);

/*
 * SATMPD ATTRIBUTE MAPPINGS DATA STRUCTURES
 *
 * map_head			u_short attrid
 *				Generic_list domains
 *
 * domains			char *domain
 *				Generic_list remote_map
 *				Generic_list local_map
 *
 * map				char *source
 *				char *dest
 */
struct map_head {
	struct ref ref_;
	u_short attrid;
	Generic_list domains;
};

struct map_domain {
	struct ref ref_;
	char *domain;
	Generic_list remote_map;	/* the to-remote map */
	Generic_list local_map;		/* the to-local map */
};

struct map {
	struct ref ref_;
	char *source;
	char *dest;
};

/*
 * SATMPD ATTRIBUTE MAPPINGS INITIALIZE/DESTROY/PRINT FUNCTIONS
 */
struct map_head *initialize_map_head (u_short);
void print_map_head (void *, void *);

struct map_domain *initialize_map_domain (const char *);

struct map *initialize_map (const char *, const char *);

#endif /* LIST_HEADER */
