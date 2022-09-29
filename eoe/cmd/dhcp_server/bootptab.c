#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>

#include "bootptab.h"

typedef struct btab_ent {
    struct hosts h;
    struct btab_ent *link;
} *btab_list;

struct btab_key {
    u_char htype;
    u_char haddr[6];
};

#ifndef uint32
# define uint32 unsigned int
#endif


#define BTAB_BUCKETS 128
    
btab_list btabm[BTAB_BUCKETS];
btab_list btabi[BTAB_BUCKETS];

/*
 * Fowler/Noll/Vo hash
 *
 * The magic is in the interesting relationship between the special
 * prime 16777619 (2^24 + 403) and 2^32 and 2^8.  If you need a hash to
 * base 2^64 instead of 2^32, let me know and I'll search for a similar
 * magic prime.
 *
 * This hash was able to process 234936 words from the web2 dictionary
 * without any 32 bit collisions.  It also hashes 4 byte integers
 * really well (no collisions for the range 1..10000000)
 */
uint32
hash_buf(u_char *str, int len)
{
	uint32  val;                    /* current hash value */
	u_char   *str_end = str + len;

	/*
	 * The basis of the hash algorithm was taken from an idea
	 * sent by Email to the IEEE Posix P1003.2 mailing list from
	 * Phong Vo (kpv@research.att.com) and Glenn Fowler
	 * (gsf@research.att.com).
	 * Landon Curt Noll (chongo@toad.com) later improved on their
	 * algorithm to come up with Fowler/Noll/Vo hash.
	 *
	 */
	for (val = 0; str < str_end; ++str) {
		val *= 16777619;
		val ^= *str;
	}

	/* our hash value, (was: mod the hash size) */
	return val;
}

/* insert a hosts structure into a list
 * it is inserted at the front of the list
 */
int
insert(btab_list *p_btab_list, struct hosts *hp)
{
    struct btab_ent *p = (struct btab_ent*) malloc(sizeof(struct btab_ent));
    if (p == (struct btab_ent*)0)
	return -1;
    bcopy(hp, &p->h, sizeof(struct hosts));
    p->link = *p_btab_list;
    *p_btab_list = p;
    return 0;
}

/*
 * lookup a hosts entry by mac address and return if found
 */
struct hosts*
lookupm(btab_list *p_btab_list, struct btab_key* k)
{
    struct btab_ent *p = *p_btab_list;

    if (p == (struct btab_ent*)0)
	return (struct hosts*)0;
    for (;p;p = p->link) {
	if ((k->htype == p->h.htype) && 
	    (bcmp(k->haddr, p->h.haddr, sizeof(p->h.haddr)) == 0))
	    return &p->h;
    }
    return (struct hosts*)0;
}

/*
 * initialize entries
 */
void
cleanup_btabm(void)
{
    struct btab_ent *p, *tmp;
    int i;

    for (i = 0; i < BTAB_BUCKETS; i++) {
	for (p = btabm[i]; p ; p = tmp) {
	    tmp = p->link;
	    free(p);
	}
	btabm[i] = 0;
    }
}

/*
 * lookup a hosts entry by ip address and return if found
 */

struct hosts*
lookupi(btab_list *p_btab_list, struct in_addr* k)
{
    struct btab_ent *p = *p_btab_list;

    if (p == (struct btab_ent*)0)
	return (struct hosts*)0;
    for (;p;p = p->link) {
	if (k->s_addr == p->h.iaddr.s_addr)
	    return &p->h;
    }
    return (struct hosts*)0;
}

/*
 * initialize entries
 */

void
cleanup_btabi(void)
{
    struct btab_ent *p, *tmp;
    int i;

    for (i = 0; i < BTAB_BUCKETS; i++) {
	for (p = btabi[i]; p ; p = tmp) {
	    tmp = p->link;
	    free(p);
	}
	btabi[i] = 0;
    }
}

/*
 * insert an entry into the hash table by ip address
 */
int
insert_btabi(struct hosts *hp)
{
    struct in_addr a_btab_key;

    a_btab_key.s_addr = hp->iaddr.s_addr;
    if (insert(&btabi[hash_buf((u_char*)&a_btab_key, 
			       sizeof(struct in_addr)) % BTAB_BUCKETS], hp) == 0) {
	return 0;
    }
    return -1;
}

/*
 * insert an entry into both hash tables
 */

int
insert_btab(struct hosts *hp)
{
    struct btab_key a_btab_key;
    struct in_addr a_iaddr_key;
    int ret = 0;

    a_btab_key.htype = hp->htype;
    bcopy(hp->haddr, a_btab_key.haddr, sizeof(a_btab_key.haddr));
    a_iaddr_key.s_addr = hp->iaddr.s_addr;

    if ((insert(&btabm[hash_buf((u_char*)&a_btab_key, 
				sizeof(struct btab_key)) % BTAB_BUCKETS], hp)
	 != 0) ||
	(insert(&btabi[hash_buf((u_char*)&a_iaddr_key, 
				sizeof(struct in_addr)) % BTAB_BUCKETS], hp) 
	 != 0)) {
	ret = -1;
    }
    else {
	nhosts++;
    }
    return ret;
}

/*
 * lookup entry by mac address and return it if found
 */
    
struct hosts*
lookup_btabm(u_char htype, u_char *haddr)
{
    struct btab_key a_btab_key;

    a_btab_key.htype = htype;
    bcopy(haddr, a_btab_key.haddr, sizeof(a_btab_key.haddr));
    return (lookupm(&btabm[hash_buf((u_char*)&a_btab_key, 
				    sizeof(struct btab_key)) % BTAB_BUCKETS], 
		    &a_btab_key));
}    

/*
 * lookup entry by ip address and return it if found
 */

    
struct hosts*
lookup_btabi(struct in_addr ikey)
{
    return (lookupi(&btabi[hash_buf((u_char*)&ikey, 
				    sizeof(struct in_addr)) % BTAB_BUCKETS], 
		    &ikey));
}    

    
