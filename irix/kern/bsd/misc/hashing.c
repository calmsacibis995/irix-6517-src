/*
 * Copyright (c) 1995 Silicon Graphics, Inc.
 */
#include "sys/param.h"
#include "sys/types.h"
#include "sys/systm.h"
#include "sys/tcp-param.h"
#include "sys/sema.h"
#include "sys/socketvar.h"
#include "netinet/in.h"
#include "sys/hashing.h"

/*
 * Forward Referenced Procedures
 */
void init_crc32_table(unsigned int hashtable_crc[]);

unsigned int compute_crc32(caddr_t key,
	unsigned int key_len, unsigned int crc_mask);

/*
 * Module defined global data structures
 *
 * The crc polynomial use in computing the crc32 table entries
 */
#define CRC32_POLY 0x04c11db7     /* AUTODIN II, Ethernet, & FDDI */

#define CRC32_HASHTABLE_SIZE 256
#define CRC32_HASHTABLE_MASK 0xFF

unsigned int hashtable_crc[CRC32_HASHTABLE_SIZE];

#define	MP_HASH_INIT(mrlockp) \
		mrlock_init((mrlockp), MRLOCK_DEFAULT, "hashlock", 0);
#define	MP_HASH_RDLOCK(mrlockp) \
		mraccess((mrlockp));
#define	MP_HASH_WRLOCK(mrlockp, s) \
		(s) = splimp(); \
		mrupdate((mrlockp));
#define	MP_HASH_RDUNLOCK(mrlockp) \
		mraccunlock((mrlockp));
#define	MP_HASH_WRUNLOCK(mrlockp, s) \
		mrunlock((mrlockp)); \
		splx((s));

/*
 * Compute 32-bit CRC for 'key_len' bytes starting at 'key' and return
 * the 32-bit CRC value after masking off any unwanted bit with 'crc_mask'.
 */
unsigned int
compute_crc32(caddr_t key, unsigned int key_len, unsigned int crc_mask)
{
	unsigned int crc, l;
	unsigned char *p;

	for (crc=0, p=(unsigned char *)key, l=key_len; l>0; ++p, --l) {
		crc = (crc<<8) ^ hashtable_crc[(crc>>24) ^ *p];
	}
	return (crc & crc_mask);
}

/*
 * Initialize CRC table
 * The 32-bit AUTODIN II, Ethernet, & FDDI crc can be computed using the
 * following table for a byte string 'key' of 'len' bytes in length.
 */
void
init_crc32_table(hashtable_crc)
	unsigned int hashtable_crc[];
{
        int i, j;
        unsigned int c;

        for (i = 0; i < CRC32_HASHTABLE_SIZE; ++i) {

		for (c = i << 24, j = 8; j > 0; --j) {
			c = (c & 0x80000000) ? (c << 1) ^ CRC32_POLY :(c << 1);
		}
		hashtable_crc[i] = c;
	}
	return;
}

/*
 * Initialize a general hash table and return the 'hashinfo' structure
 * updated to have the new information.
 */
void
hash_init(struct hashinfo *hashinfo, struct hashtable hashtable[],
	int hashtablesize,
	int key_len,
	unsigned int crc_mask)
{
	int i;

	hashinfo->hashtable = hashtable;
	hashinfo->hashtablesize = hashtablesize;
	hashinfo->entrysize = sizeof(struct hashtable);
	hashinfo->key_len = key_len;
	hashinfo->crc_mask = crc_mask;

	init_crc32_table(hashtable_crc);

	for (i = 0; i < hashtablesize; ++i) {

		MP_HASH_INIT(&hashtable[i].lock);
		hashtable[i].next = hashtable[i].prev =
			(struct hashbucket *)(&hashtable[i].next);
	}
	return;
}

/*
 * Enumerate entire hash table and call the procedure 'match_function' for
 * each entry. The enumerate match function 'match_function' returns 1
 * on a match and zero otherwise.
 */
struct hashbucket *
hash_enum(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t mkey, caddr_t arg1,
		caddr_t arg2),
	unsigned short hashflags,
	caddr_t key,
	caddr_t arg1,
	caddr_t arg2)
{
	struct hashtable *hashtable = hashinfo->hashtable;
	struct hashbucket *h, *head;
	int i, hashtablesize = hashinfo->hashtablesize;

	for (i=0; i < hashtablesize; i++) {

		head = (struct hashbucket *)(&hashtable[i].next);

		MP_HASH_RDLOCK(&hashtable[i].lock);

		for (h = hashtable[i].next; h != head; h = h->next) {

			if (h->flags & hashflags) { /* yes, right entry type */

				if ((*match_function)(h, key, arg1, arg2)) {

					MP_HASH_RDUNLOCK(&hashtable[i].lock);
					return h;
				}
			}
		}
		MP_HASH_RDUNLOCK(&hashtable[i].lock);
	}
	return ((struct hashbucket *)0);
}

/*
 * Special hash enumeration procedure to get the "best" element as specified
 *  by the match function and the refines function.
 *
 * Enumerate entire hash table and call the procedure 'match_function'
 * for each entry that matches the hashflags parameter. In addition this
 * procedure is used to find the "best' match amongst all of the entries
 * by doing a pair-wise comparision between the current "best" and the next
 * best candidate by calling the 'refines_function' procedure to refine the
 * criteria on what "best" means for these two elements.
 *
 * This procedure is used to find the best network number amongst all of
 * our candidate entries. The enumerate match function 'match_function'
 * returns 1 on a match and zero otherwise. The 'refines_function' returns
 * 1 if the first argument is better than the second. In that case the
 * new best is made the current best and the process continues.
 */
struct hashbucket *
hash_enum_bestmatch(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t key,
		caddr_t arg1, caddr_t arg2),
	int (*refines_function)(struct hashbucket *h, struct hashbucket *best),
	unsigned short hashflags,
	caddr_t key,
	caddr_t arg1,
	caddr_t arg2)
{
	struct hashtable *hashtable;
	struct hashbucket *h, *head, *bestsofar;
	int i, hashtablesize;

	hashtablesize = hashinfo->hashtablesize;
	hashtable = hashinfo->hashtable;
	bestsofar = (struct hashbucket *)0;

	for (i=0; i < hashtablesize; i++) {

		head = (struct hashbucket *)(&hashtable[i].next);
		MP_HASH_RDLOCK(&hashtable[i].lock);

		for (h = hashtable[i].next; h != head; h = h->next) {

			if (h->flags & hashflags) { /* yes, right entry type */

				if ((*match_function)(h, key, arg1, arg2)) {

					if (bestsofar == 0 ||
					    (*refines_function)(h,
						bestsofar)) {
						bestsofar = h;
					}
				}
			}
		}
		MP_HASH_RDUNLOCK(&hashtable[i].lock);
	}
	return bestsofar;
}

/*
 * Special hash enumeration procedure to get the specified next element.
 * Enumerate entire hash table and call the procedure 'match_function'
 * for the 'nth' entry that matches the hashflags parameter.
 * The enumerate match function 'match_function' returns 1 on a match
 * and zero otherwise.
 */
struct hashbucket *
hash_enum_getnext(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t key,
		caddr_t arg1, caddr_t arg2),
	unsigned short hashflags,
	int start,
	int nth,
	caddr_t arg)
{
	struct hashtable *hashtable;
	struct hashbucket *h, *head;
	int i, n, hashtablesize;

	if (nth < 0)
		return ((struct hashbucket *)0);

	n = start;
	hashtable = hashinfo->hashtable;
	hashtablesize = hashinfo->hashtablesize;

	for (i=0; i < hashtablesize; i++) {

		head = (struct hashbucket *)(&hashtable[i].next);
		MP_HASH_RDLOCK(&hashtable[i].lock);

		for (h = hashtable[i].next; h != head; h = h->next) {

			if (h->flags & hashflags) { /* yes, right entry type */

				if ((*match_function)(h,
					(caddr_t)(__psint_t)start,
					(caddr_t)(__psint_t)nth, arg)) {

					if (n == nth) { /* see if match */

					  MP_HASH_RDUNLOCK(&hashtable[i].lock);
					  return h;
					}
					n++;
				}
			}
		}
		MP_HASH_RDUNLOCK(&hashtable[i].lock);
	}
	return ((struct hashbucket *)0);
}

/*
 * Hash insert procedure
 * Do a hash table lookup and if not present in table then initialize
 * and link the new bucket entry onto the particular hash chain list.
 */
struct hashbucket *
hash_insert(hashinfo, entry, key)
	struct hashinfo *hashinfo;
	struct hashbucket *entry;
	caddr_t key; /* (struct in_addr *) */
{
	struct hashtable *hashtable;
	unsigned int crc;
	NETSPL_DECL(s)

	crc = compute_crc32(key, hashinfo->key_len, hashinfo->crc_mask);

	/* save the crc (for remove op) */
	entry->crc = crc;

	/* copy key into new hash bucket entry (HASH_KEYSIZE) */
	bcopy((void *)key, (void *)(&(entry->key)), hashinfo->key_len);

	hashtable = hashinfo->hashtable;
	MP_HASH_WRLOCK(&hashtable[crc].lock, s);

	/* link new element onto hash chain */
	insque(entry, hashtable[crc].prev);

	MP_HASH_WRUNLOCK(&hashtable[crc].lock, s);
	return entry;
}

/*
 * Hash lookup procedure
 * This procedure returns either pointer to a bucket entry or null.
 * if a non-null pointer is returned. Any refcount on the entry is the
 * responsibility of the procedures maintaining the object itself.
 * The deallocation of the storage for the hash bucket entry (and the
 * object which is appended) is the responsibility of the caller.
 */
struct hashbucket *
hash_lookup(hashinfo, match_function, key, arg1, arg2)
	struct hashinfo *hashinfo;
	int (*match_function)(struct hashbucket *h, caddr_t mkey,
		caddr_t arg1, caddr_t arg2);
	caddr_t key; /* (struct in_addr *) */
	caddr_t arg1; /* struct ifnet *ifp */
	caddr_t arg2;
{
	struct hashtable *hashtable;
	struct hashbucket *h, *hrtn, *head;
	unsigned int crc;

	/* compute the crc over the key */
	crc = compute_crc32(key, hashinfo->key_len, hashinfo->crc_mask);

	hashtable = hashinfo->hashtable;
	head = (struct hashbucket *)&hashtable[crc];
	hrtn = ((struct hashbucket *)0);

	MP_HASH_RDLOCK(&hashtable[crc].lock);
	for (h = hashtable[crc].next; h != head; h = h->next) {

		/* call compare func with key to bucket entries */
		if ((*match_function)(h, key, arg1, arg2)) { /* match */
			hrtn = h;
			break;
		}
	}
	MP_HASH_RDUNLOCK(&hashtable[crc].lock);
	return hrtn;
}

/*
 * Hash lookup and apply object's reference count procedure
 *
 * This procedure does the lookup of the entry via the 'key' and if one is
 * found the 'refcnt_function' is called on this entry.
 * We first obtain the write lock for the hash bucket chain and then call
 * the 'match_function' to see if we have found the desired entry. If we
 * find a match the 'refcnt_function' procedure is called to perform the
 * refcnt inc/dec operation which returns 1 to indicate the storage associated
 * for this entry can be deallocated and zero otherwise. The address of the
 * hash table entry is returned if the 'refcnt_function' procedure returned
 * non-zero indicating the storage can be reclaimed and NULL otherwise.
 *
 * NOTE: There is no 'refcount' in the generic hash bucket entry. It's the
 * responsibility of the client to maintain a refcnt in the hash object if one
 * is required. This is the case for all present networking uses, such as
 * in_ifaddr, multicast and in_bcast address entries.
 * The deallocation of the storage for the hash bucket entry (and the object
 * which is appended) is the responsibility of the client software.
 */
struct hashbucket *
hash_lookup_refcnt(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t mkey,
		caddr_t marg1, caddr_t marg2),
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2),
	caddr_t key, /* (struct in_addr *) */
	caddr_t arg1, /* struct ifnet *ifp */
	caddr_t arg2)
{
	struct hashtable *hashtable;
	struct hashbucket *h, *hrtn, *head;
	unsigned int crc;
	NETSPL_DECL(s)

	/* compute the crc over the key */
	crc = compute_crc32(key, hashinfo->key_len, hashinfo->crc_mask);

	hashtable = hashinfo->hashtable;
	head = (struct hashbucket *)&hashtable[crc];
	hrtn = ((struct hashbucket *)0);

	MP_HASH_WRLOCK(&hashtable[crc].lock, s);

	for (h = hashtable[crc].next; h != head; h = h->next) {

		/* call compare func with key on each bucket entry */
		if ((*match_function)(h, key, arg1, arg2)) { /* match */

			if ((*refcnt_function)(h, arg1, arg2)) {
				/* delink from hash chain if last reference */
				remque(h);

				/* return address of node to deallocate */
				hrtn = h;
				break;
			}
		}
	}
	MP_HASH_WRUNLOCK(&hashtable[crc].lock, s);
	return hrtn;
}

/*
 * Hash lookup and apply object's reference count procedure
 *
 * This procedure does the lookup of the entry via the 'key' and if one is
 * found the 'refcnt_function' is called on this entry.
 * We first obtain the write lock for the hash bucket chain and then call
 * the 'match_function' to see if we have found the desired entry. If we
 * find a match the 'refcnt_function' procedure is called to perform the
 * refcnt inc/dec operation which returns 1 to indicate the storage associated
 * for this entry can be deallocated and zero otherwise. The address of the
 * hash table entry is returned if the 'refcnt_function' procedure returned
 * non-zero indicating the storage can be reclaimed and NULL otherwise.
 *
 * NOTE: There is no 'refcount' in the generic hash bucket entry. It's the
 * responsibility of the client to maintain a refcnt in the hash object if one
 * is required. This is the case for all present networking uses, such as
 * in_ifaddr, multicast and in_bcast address entries.
 * The deallocation of the storage for the hash bucket entry (and the object
 * which is appended) is the responsibility of the client software.
 */
struct hashbucket *
hash_lookup_remove(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t mkey,
		caddr_t marg1, caddr_t marg2),
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2),
	caddr_t key, /* (struct in_addr *) */
	caddr_t arg1, /* struct ifnet *ifp */
	caddr_t arg2)
{
	struct hashtable *hashtable;
	struct hashbucket *h, *hrtn, *head;
	unsigned int crc;
	NETSPL_DECL(s)

	/* compute the crc over the key */
	crc = compute_crc32(key, hashinfo->key_len, hashinfo->crc_mask);

	hashtable = hashinfo->hashtable;
	head = (struct hashbucket *)&hashtable[crc];
	hrtn = ((struct hashbucket *)0);

	MP_HASH_WRLOCK(&hashtable[crc].lock, s);

	for (h = hashtable[crc].next; h != head; h = h->next) {

		/* call compare func with key on each bucket entry */
		if ((*match_function)(h, key, arg1, arg2)) { /* match */

			if ((*refcnt_function)(h, arg1, arg2)) {
				/* delink from hash chain if last reference */
				remque(h);

				/* return address of node to deallocate */
				hrtn = h;
				break;
			}
		}
	}
	MP_HASH_WRUNLOCK(&hashtable[crc].lock, s);
	return hrtn;
}

/*
 * Hash reference count procedure
 *
 * This procedure first obtain the write lock for the hash bucket chain
 * and then calls hash_refcnt() function to handle the reference
 * counting operation. The hash_refcnt() function returns 1 to
 * indicate this hash table entry should be delinked from the chain and
 * deallocated OR ZERO for the normal reference count decrement operation.
 * We then return the address of the hash table entry to the caller so
 * the storage can be reclaimed.
 *
 * NOTE: There is no 'refcount' in the generic hash bucket entry. It is the
 * responsibility of the client of these procedures to maintain a refct if one
 * is required. This is the case for the present networking uses.
 */
struct hashbucket *
hash_refcnt(hashinfo, refcnt_function, entry)
	struct hashinfo *hashinfo;
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2);
	struct hashbucket *entry;
{
	struct hashtable *hashtable;
	struct hashbucket *h, *head, *hrtn;
	unsigned int crc;
	NETSPL_DECL(s)

	crc = entry->crc;
	if (crc >= hashinfo->hashtablesize) { /* bogus index */
		return ((struct hashbucket *)0);
	}

	hashtable = hashinfo->hashtable;
	head = (struct hashbucket *)&hashtable[crc];
	hrtn = ((struct hashbucket *)0);

	MP_HASH_WRLOCK(&hashtable[crc].lock, s);

	/* compare key to bucket entries */
	for (h = hashtable[crc].next; h != head; h = h->next) {

		if (h == entry) { /* match so remove this one from list */

			/*
			 * if non-zero returned => ref ctn now <= 1 so delink
			 * and return address of the entry to the caller
			 */
			if ((*refcnt_function)(h, 0, 0)) {
				hrtn = h;
				remque(h);
			}
			break;
		}
	}
	MP_HASH_WRUNLOCK(&hashtable[crc].lock, s);

	return hrtn;
}

/*
 * Hash remove and apply object's reference count procedure
 *
 * This procedure does the lookup of the entry via the 'key' and if one is
 * found the 'refcnt_function' is called on this entry.
 * We first obtain the write lock for the hash bucket chain and then call
 * the 'match_function' to see if we have found the desired entry. If we
 * have a match then the 'refcnt_function' procedure is called to perform
 * the reference counting increment/decrement operation. The 'refcnt_function'
 * procedure returns 1 to indicate the storage associated with the hash table
 * entry can be deallocated and zero otherwise. In either case the entry
 * is delinked from the chain since this is used by a forceable remove
 * operation even tho ther storage can't be recovered right now.
 *
 * This procedure returns the address of the hash table entry if the
 *'refcnt_function' procedure returned an indication that the storage can be
 * reclaimed. In the other case, when you can't recover the storage NULL will
 * be returned.
 *
 * NOTE: There is no 'refcount' in the generic hash bucket entry. It's the
 * responsibility of the client to maintain a refcnt in the hash object if one
 * is required. This is the case for all present networking uses, such as
 * in_ifaddr, multicast and bcast address entries.
 * The deallocation of the storage for the hash bucket entry (and the object
 * which is appended) is the responsibility of the client software. We support
 * this later deferred recovery of storage by setting a special HTF flag.
 */
struct hashbucket *
hash_remove_refcnt(
	struct hashinfo *hashinfo,
	int (*match_function)(struct hashbucket *h, caddr_t mkey,
		caddr_t marg1, caddr_t marg2),
	int (*refcnt_function)(struct hashbucket *h,
		caddr_t narg1, caddr_t narg2),
	caddr_t key, /* (struct in_addr *) */
	caddr_t arg1, /* struct ifnet *ifp */
	caddr_t arg2)
{
	struct hashtable *hashtable;
	struct hashbucket *h, *hrtn, *head;
	unsigned int crc;
	NETSPL_DECL(s)

	/* compute the crc over the key */
	crc = compute_crc32(key, hashinfo->key_len, hashinfo->crc_mask);

	hashtable = hashinfo->hashtable;
	head = (struct hashbucket *)&hashtable[crc];
	hrtn = ((struct hashbucket *)0);

	MP_HASH_WRLOCK(&hashtable[crc].lock, s);

	for (h = hashtable[crc].next; h != head; h = h->next) {

		/* call compare func with key on each bucket entry */
		if ((*match_function)(h, key, arg1, arg2)) { /* match */

			if ((*refcnt_function)(h, arg1, arg2)) {
				/* return address of node to deallocate */
				hrtn = h;
			}
			/* always delink from hash chain and note that fact */
			remque(h);
			h->flags |= HTF_NOTINTABLE;
			break;
		}
	}
	MP_HASH_WRUNLOCK(&hashtable[crc].lock, s);
	return hrtn;
}

/*
 * Hash remove procedure
 *
 * This procedure delinks the hashbucket entry IF it was found on the
 * hash chain which is indexed via the crc value stored in the bucket entry.
 * We return a NULL pointer in the case the entry was NOT found on the chain
 * otherwise the address of the entry is returned.
 * NOTE: There is no 'refcount' in the hash bucket entry. It is the
 * responsibility of the client of these procedures to maintain a refct if one
 * is required. This is the case for the present networking uses.
 */
struct hashbucket *
hash_remove(struct hashinfo *hashinfo, struct hashbucket *entry)
{
	struct hashtable *hashtable;
	struct hashbucket *h, *head, *hrtn;
	unsigned int crc;
	NETSPL_DECL(s)

	crc = entry->crc;
	if (crc >= hashinfo->hashtablesize) { /* bogus index */
		return ((struct hashbucket *)0);
	}

	hashtable = hashinfo->hashtable;
	head = (struct hashbucket *)&hashtable[crc];
	hrtn = ((struct hashbucket *)0);

	MP_HASH_WRLOCK(&hashtable[crc].lock, s);

	/* compare key to bucket entries */
	for (h = hashtable[crc].next; h != head; h = h->next) {

		if (h == entry) { /* match so remove this one from list */

			remque(h);
			hrtn = h;
			break;
		}
	}
	MP_HASH_WRUNLOCK(&hashtable[crc].lock, s);

	return hrtn;
}
