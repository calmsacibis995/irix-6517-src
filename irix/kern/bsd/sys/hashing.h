#ifndef __SYS_HASHING_H__
#define __SYS_HASHING_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright (c) 1995 Silicon Graphics, Inc.
 */
/*
 * Generic hash table parameters
 */
#define	HASHTABLE_SIZE		256  /* Number entries in base hash table */
#define	HASH_MASK		0xFF /* Mask corresponding to above value */

#define	HASH_KEYSIZE		4   /* key len used in computing IP addr crc */
#ifdef INET6
#define	HASH_MAXKEYSIZE		36  /* large enough for <IP6, port,IP6,port> */
#else
#define	HASH_MAXKEYSIZE		12  /* large enough for <IP, port,IP,port> */
#endif

/*
 * hash bucket entry, pointed to by the indexed hash table
 * NOTE: All hash bucket entries should be reference counted but the
 * count should be maintained in the data object.
 */
struct hashbucket {
	struct hashbucket *next;	/* next entry in linked chain */
	struct hashbucket *prev;	/* previous entry in linked chain */

	unsigned short flags;		/* flags indicating data object type */
	unsigned int crc;		/* crc for this entry saved here */

	unsigned char key[HASH_MAXKEYSIZE]; /* max key when comparing entries*/
	/*
	 * The typical key is a 'struct in_addr' for the ip hash table case
	 *
	 * The data object being looked-up immediately follows this structure.
	 * The type of the data object is indicated by the 'flags' field above.
	 */
};

/*
 * Definition of the base hash table entry
 */
struct hashtable {
	struct hashbucket *next;	/* Head of hash table chain */
	struct hashbucket *prev;	/* Previous link for symmetry only */
	mrlock_t lock;			/* Multi-read/write lock for chain */
};

/*
 * Hash table flags, define which object this hash table entry corresponds to
 */
#define	HTF_NOTINTABLE	0x01	/* Entry not in hash table yet */
#define	HTF_INET	0x02	/* Normal Internet address entry */
#define	HTF_MULTICAST	0x04	/* Multicast Internet address entry */
#define	HTF_BROADCAST	0x08	/* Broadcast Internet address entry */
#define	HTF_INPCB	0x10	/* Protocol Control Block address entry */

/*
 * General information on a particular hash table
 */
struct hashinfo {
	struct hashtable *hashtable;	/* hash table, locks and chain heads */
	unsigned int hashtablesize;	/* number entries in hash table */
	unsigned int entrysize;		/* entry size */
	unsigned int key_len;		/* key len used computing hash index */
	unsigned int crc_mask;		/* mask to keep significant bits */
};

#ifdef _KERNEL
/*
 * Kernel interfaces specific to hashing
 */
extern void hash_init(
	struct hashinfo *hashinfo,
	struct hashtable hashtable[],
	int hashtablesize,
	int key_len,
	unsigned int crc_mask);

extern struct hashbucket *hash_enum(
	struct hashinfo *hashinfo,
	int (*function)(struct hashbucket *h, caddr_t mkey, caddr_t arg1,
		caddr_t arg2),
	unsigned short hashflags,
	caddr_t key,
	caddr_t arg1,
	caddr_t arg2);

extern struct hashbucket *hash_enum_bestmatch(
	struct hashinfo *hashinfo,
	int (*function)(struct hashbucket *h, caddr_t mkey, caddr_t arg1,
		caddr_t arg2),
	int (*refines)(struct hashbucket *h, struct hashbucket *best),
	unsigned short hashflags,
	caddr_t key,
	caddr_t arg1,
	caddr_t arg2);

extern struct hashbucket *hash_enum_getnext(
	struct hashinfo *hashinfo,
	int (*function)(struct hashbucket *h, caddr_t mkey, caddr_t arg1,
		caddr_t arg2),
	unsigned short hashflags,
	int start,
	int nth,
	caddr_t arg);

extern struct hashbucket *hash_insert(
	struct hashinfo *hashinfo,
	struct hashbucket *hashbucket,
	caddr_t insert_key); /* (struct in_addr *) */

extern struct hashbucket *hash_lookup(
	struct hashinfo *hashinfo,
	int (*function)(struct hashbucket *h, caddr_t lookup_key,
		caddr_t arg1, caddr_t arg2),
	caddr_t key,   /* (struct in_addr *) */
	caddr_t arg1,  /* (struct sockaddr *) */
	caddr_t arg2); /* (struct ifnet *) */

extern struct hashbucket *hash_lookup_refcnt(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t mkey,
		caddr_t marg1, caddr_t marg2),
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2),
	caddr_t key, /* (struct in_addr *) */
	caddr_t arg1, /* struct ifnet *ifp */
	caddr_t arg2);

extern struct hashbucket *hash_refcnt(
	struct hashinfo *hashinfo,
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2),
	struct hashbucket *entry);

extern struct hashbucket *hash_remove(
	struct hashinfo *hashinfo,
	struct hashbucket *entry);

extern struct hashbucket *hash_remove_refcnt(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t lookup_key,
		caddr_t arg1, caddr_t arg2),
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2),
	caddr_t key,   /* (struct in_addr *) */
	caddr_t arg1,  /* (struct sockaddr *) */
	caddr_t arg2); /* (struct ifnet *) */

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_HASHING_H__ */
