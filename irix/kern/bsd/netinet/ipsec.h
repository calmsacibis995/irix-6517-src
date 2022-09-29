/*
 * IP security defines.
 */

#ifndef _NETINET_IPSEC_H_
#define _NETINET_IPSEC_H_

/* IP security pseudo-socket address (same layout than IPv6) */

#define IPSEC_IN	1
#define IPSEC_OUT	2

struct sockaddr_ipsec {
	u_int8_t	sis_len;	/* length */
	u_int8_t	sis_family;	/* family (AF_INET*) */
	u_int8_t	sis_way;	/* [in|out]bound */
	u_int8_t	sis_kind;	/* AH or ESP */
	u_int32_t	sis_spi;	/* security parameter index */
	struct in6_addr	sis_addr;	/* (destination) address */
};

/* IP security {get,set}sockopt argument header */

struct ipsec_req {
	struct sockaddr_ipsec	ipr_addr;		/* dest & SPI */
#define IANAMSIZ	16
	char			ipr_algo[IANAMSIZ];	/* algorithm */
	u_short			ipr_klen;		/* key length */
	u_short			ipr_ivlen;		/* init vect length */
	char			ipr_key[1];		/* key */
};

#define IPSEC_EXCLUSIVE		1	/* can't be shared */
#define IPSEC_WILDCARD		2	/* wildcard */

#define IPSEC_WANTAUTH		1	/* int */
#define IPSEC_WANTCRYPT		2	/* int */
#define IPSEC_CREATE		3	/* ipsec_req */
#define IPSEC_CHANGE		4	/* ipsec_req */
#define IPSEC_DELETE		5	/* sockaddr_ipsec */
#define IPSEC_ATTACH		6	/* sockaddr_ipsec */
#define IPSEC_ATTACHX		7	/* sockaddr_ipsec */
#define IPSEC_ATTACHW		8	/* any */
#define IPSEC_DETACH		9	/* sockaddr_ipsec */
#define IPSEC_DETACHX		10	/* sockaddr_ipsec */
#define IPSEC_DETACHW		11	/* any */

/* IP security mbuf flags */

#define M_AUTH		0x0400
#define M_CRYPT		0x0800

/* IP security entry header */

struct ipsec_entry_header {
	struct radix_node	ieh_nodes[2];	/* for radix code */
	struct ipsec_algo	*ieh_algo;	/* algorithm */
	int			ieh_refcnt;	/* reference counter */
	u_long			ieh_use;	/* use counter */
	u_short			ieh_klen;	/* key length */
	u_short			ieh_ivlen;	/* init vector length */
	char			ieh_key[1];	/* key */
};
#define iehtosis(ieh)	((struct sockaddr_ipsec *)(ieh)->ieh_nodes->rn_key)

/* IP security algorithm */

#define AH_ALGO		51		/* IP6_NHDR_AUTH */
#define ESP_ALGO	50		/* IP6_NHDR_ESP */

struct ipsec_algo {
	char  *isa_name;			/* name */
	u_char isa_type;			/* type */
	u_char isa_index;			/* index */
	u_char isa_mklen;			/* max key length */
	u_char isa_mivlen;			/* max init vect length */
	struct {				/* AH algorithms */
		caddr_t (*ah_init) __P((struct ipsec_entry_header *));
		caddr_t (*ah_update) __P((caddr_t, struct mbuf *));
		void	(*ah_finish) __P((caddr_t, caddr_t,
					  struct ipsec_entry_header *));
		int	ah_reslen;
	} isa_ah_algo;
	struct {				/* ESP algorithms */
		caddr_t (*esp_init) __P((struct ipsec_entry_header *,
				         caddr_t));
		caddr_t (*esp_encrypt) __P((caddr_t, struct mbuf *));
		caddr_t (*esp_decrypt) __P((caddr_t, struct mbuf *));
		void	(*esp_finish) __P((caddr_t));
	} isa_esp_algo;
};
#define isa_ah_init	isa_ah_algo.ah_init
#define isa_ah_update	isa_ah_algo.ah_update
#define isa_ah_finish	isa_ah_algo.ah_finish
#define isa_ah_reslen	isa_ah_algo.ah_reslen
#define isa_esp_init	isa_esp_algo.esp_init
#define isa_esp_encrypt	isa_esp_algo.esp_encrypt
#define isa_esp_decrypt	isa_esp_algo.esp_decrypt
#define isa_esp_finish	isa_esp_algo.esp_finish

/* IP security PCB options */

struct ip_seclist {
	struct {
		struct ip_seclist *le_next;  /* next element */
		struct ip_seclist **le_prev; /* previous next element */
	} isl_list; /* chain */
	struct ipsec_entry_header *isl_iep;	/* entry */
};

#ifdef _KERNEL
extern	struct radix_node_head *ipsec_tree;

void	ipsec_init __P((void));
struct	ipsec_entry_header *ipsec_lookup
		__P((struct sockaddr_ipsec *, struct ip_seclist *));
int	ipsec_create __P((struct ipsec_req *));
int	ipsec_change __P((struct ipsec_req *));
int	ipsec_delete __P((struct sockaddr_ipsec *));
struct	mbuf *ah6_build __P((struct mbuf *, struct mbuf *, int));
struct	mbuf *esp6_build __P((struct mbuf *, int, int, int));
struct	ipsec_algo *ipsec_getalgo __P((char *, u_char, u_char, u_char));

#endif

#endif
