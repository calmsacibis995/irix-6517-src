/*
 * ldap_cach.c - implemets a cache to improve performance
 * to be used for reservations and leases
 * a write once read many cache (no updates, only invalidates)
 */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <syslog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ndbm.h>
#include <lber.h>
#include <ldap.h>
#include "dhcp_common.h"
#include "ldap_api.h"
#include "ldap_dhcp.h"
extern void mk_cidstr(char *cid_ptr, int cid_length, char *str);
extern void mk_cid(char *cidstr, char *cid_ptr, int *cid_len);
extern struct ether_addr *ether_aton(char *);
extern char globalsbuf[];

extern DBM *db;
extern int dblock;

#define RESTABKEYSZ 64

struct restab_key {
    int type;
    char key[RESTABKEYSZ];		/* the first 16 bytes */
};

struct resvn {
    int cacheIndex;
    char **optionvalues;
    dhcpConfig* cfg;
};

typedef struct restab_ent {
    struct resvn m;
    struct restab_ent *link;
} *restab_list;

#define RESTAB_BUCKETS 128

static restab_list restab[RESTAB_BUCKETS];
#ifndef uint32
# define uint32 unsigned int
#endif
extern uint32 hash_buf(u_char *str, int len);
extern LCache *resvnLCache;

int is_a_reservation;

/* 
 * hash table code for reservations 
 */
void
cleanup_restabm(void)
{
    struct restab_ent *p, *tmp;
    int i;

    for (i = 0; i < RESTAB_BUCKETS; i++) {
	for (p = restab[i]; p ; p = tmp) {
	    tmp = p->link;
	    /*    
		  can't clean these as they are part of three entries
		  CID, HNM, IPA
		  if (p->m.optionvalues)
	       ldap_value_free(p->m.optionvalues);
	    p->m.optionvalues = (char**)0;
	    if (p->m.cfg)
	    cf0_free_config_list(p->m.cfg);*/
	    free(p);
	}
	restab[i] = 0;
    }
}

/* Insert an entry into the list
 */
int
insertrestab(restab_list *p_restab_list, struct resvn *m)
{
    struct restab_ent *p = (struct restab_ent*) 
	malloc(sizeof(struct restab_ent));
    if (p == (struct restab_ent*)0)
	return -1;
    bcopy(m, &p->m, sizeof(struct resvn));
    p->link = *p_restab_list;
    *p_restab_list = p;
    return 0;
}
/* Insert an entry into the hash table
 */
int 
insert_restab(int type, char *m, struct resvn *r)
{
    int ret = 0;
    struct restab_key a_restab_key;

    bzero(&a_restab_key, sizeof(a_restab_key));
    a_restab_key.type = type;
    bcopy(m, a_restab_key.key, strlen(m));

    if (insertrestab(&restab[hash_buf((u_char*)&a_restab_key, 
				      sizeof(struct restab_key)) 
			    % RESTAB_BUCKETS], r)
	!= 0) {
	ret = -1;
    }
    return ret;

}


/* Lookup into the hash table using the mac type and address as the key
 * the length should also match
 */
struct resvn*
lookuprestab(restab_list *p_restab_list, struct restab_key *k, LCacheEnt *c)
{
    struct restab_ent *p = *p_restab_list;
    char *ipc;
    struct in_addr ia;

    if (p == (struct restab_ent*)0)
	return (struct resvn*)0;
    for (;p;p = p->link) {
	switch(k->type) {
	case CID:
	    if (strncmp(k->key, c[p->m.cacheIndex].cid,
			RESTABKEYSZ) == 0)
		return &p->m;
	    break;
	case IPA:
	    ia.s_addr = c[p->m.cacheIndex].ipa;
	    if ((ipc = inet_ntoa(ia)) == NULL)
		break;
	    if (strncmp(ipc, k->key, RESTABKEYSZ) == 0)
		return &p->m;
	    break;
	case HNM:
	    if (strncmp(k->key, c[p->m.cacheIndex].cid, 
			RESTABKEYSZ) == 0)
		return &p->m;
	    break;
	}
    }
    return (struct resvn*)0;
}


struct resvn*
lookup_restab(int type, char *key)
{
    struct restab_key a_restab_key;
    
    bzero(&a_restab_key, sizeof(a_restab_key));
    a_restab_key.type = type;
    strncpy(a_restab_key.key, key, sizeof(a_restab_key.key));
    return (lookuprestab(&restab[hash_buf((u_char*)&a_restab_key, 
					  sizeof(struct restab_key)) 
				% RESTAB_BUCKETS], 
			 &a_restab_key, resvnLCache->lCacheEntp));
}    

/*
 * read only cache code
 */

LCache *makeLCache(int size)
{
    LCache *c;
    int i;

    ldhcp_logprintf(LDHCP_LOG_MAX, "makeLCache");
    if ((c = (LCache*)malloc(sizeof(LCache))) == NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "makeLCache: out of memory");
	return NULL;
    }
    if ((c->lCacheEntp = (LCacheEnt*)malloc(size * sizeof(LCacheEnt)))
	== NULL) {
	ldhcp_logprintf(LDHCP_LOG_CRIT, "makeLCache: out of memory");
	free(c);
	return NULL;
    }
    for (i = 0; i < size; i++) 
	c->lCacheEntp[i].valid = 0;
    c->sz = size;
    c->nextInsert = 0;
    return c;
}
    
int insertLCacheEntry(LCache* c, int class, _arg_ld_t_ptr result, char *r)
{
    LCacheEnt *pLCacheEnt;

    ldhcp_logprintf(LDHCP_LOG_MAX, "insertLCacheEntry class %d", class);
     if ((c == NULL) || (c->lCacheEntp == NULL))
	return LD_ERROR;
    pLCacheEnt = &(c->lCacheEntp[c->nextInsert]);
    pLCacheEnt->class = class;
    pLCacheEnt->type = 0;
    strcpy(pLCacheEnt->etherToIPLine, r);
    pLCacheEnt->valid = 1;
    result2keys(result, pLCacheEnt->cid, 
		&pLCacheEnt->ipa, pLCacheEnt->hostname);
    c->nextInsert = (c->nextInsert + 1) % c->sz;
    return LD_OK;
}

int insertLCacheEntryNULL(LCache* c, int class, int type, void *key)
{
    LCacheEnt *pLCacheEnt;

    ldhcp_logprintf(LDHCP_LOG_MAX, "insertLCacheEntry class %d", class);
     if ((c == NULL) || (c->lCacheEntp == NULL))
	return LD_ERROR;
    pLCacheEnt = &(c->lCacheEntp[c->nextInsert]);
    pLCacheEnt->class = class;
    pLCacheEnt->type = type;
    pLCacheEnt->etherToIPLine[0] = '\0';
    pLCacheEnt->valid = 1;
    switch (type) {
    case CID:
	strncpy(pLCacheEnt->cid, key, sizeof(pLCacheEnt->cid));
	break;
    case IPA:
	pLCacheEnt->ipa = *(u_long*)key;
	break;
    case HNM:
	strncpy(pLCacheEnt->hostname, key, sizeof(pLCacheEnt->cid));
	break;
    }
    c->nextInsert = (c->nextInsert + 1) % c->sz;
    return LD_OK;
}

dhcpConfig*
getReservationDhcpConfig(LCache* c, u_long addr)
{
    struct in_addr ia;
    char *k;
    struct resvn *r;

    if ((c == NULL) || (c->lCacheEntp == NULL))
	return NULL;

    ia.s_addr = addr;
    k = inet_ntoa(ia);

    r = lookup_restab(IPA, k);
    if (r) 
	return(r->cfg);
    else 
	return NULL;
}

char *getLCacheEntry(LCache* c, int class, int ktype, 
		     void *k, char *s, int *rc)
{
    LCacheEnt *pLCacheEnt;
    int i;
    char *sb;
    u_long ipa;
    struct resvn *r;
    struct in_addr ia;
    
    ldhcp_logprintf(LDHCP_LOG_MAX, "getLCacheEntry key %d->%s", class, k);

    *rc = -1;
    if ((c == NULL) || (c->lCacheEntp == NULL))
	return LD_ERROR;
    if (!s) 
	sb = globalsbuf;
    else
	sb = s;

    switch(class) {
    case DHCPRESERVATION:
	if (ktype == IPA) {
	    ia.s_addr = *(u_long*)k;
	    k = inet_ntoa(ia);
	}
	r = lookup_restab(ktype, k);
	if (r) {
	    strcpy(sb, c->lCacheEntp[r->cacheIndex].etherToIPLine);
	    *rc = LD_SUCCESS;
	    is_a_reservation = 1;
	    return sb;
	}
	else 
	    return NULL;
    case DHCPLEASE:
	for (pLCacheEnt = c->lCacheEntp, i = 0; i < c->sz; i++) {
	    if (pLCacheEnt[i].valid == 0)
		break;
	    if ((pLCacheEnt[i].valid) && (class == pLCacheEnt[i].class)) {
		switch(ktype) {
		case CID:
		    if ((strcmp(k, pLCacheEnt[i].cid) == 0) &&
			((pLCacheEnt[i].type == CID) || 
			 (pLCacheEnt[i].type == 0))) {
			if (pLCacheEnt[i].type == CID) {
			    *rc = LD_NOTFOUND;
			    return NULL;
			}
			strcpy(sb, pLCacheEnt[i].etherToIPLine);
			*rc = LD_SUCCESS;
			return sb;
		    }
		    break;
		case IPA:
		    ipa = *(u_long*)k;
		    if ((ipa == pLCacheEnt[i].ipa) &&
			((pLCacheEnt[i].type == IPA) || 
			 (pLCacheEnt[i].type == 0))) {
			if (pLCacheEnt[i].type == IPA) {
			    *rc = LD_NOTFOUND;
			    return NULL;
			}
			strcpy(sb, pLCacheEnt[i].etherToIPLine);
			*rc = LD_SUCCESS;
			return sb;
		    }
		    break;
		case HNM:
		    if ((strcmp(k, pLCacheEnt[i].hostname) == 0) &&
			((pLCacheEnt[i].type == HNM) || 
			 (pLCacheEnt[i].type == 0))) {
			if (pLCacheEnt[i].type == HNM) {
			    *rc = LD_NOTFOUND;
			    return NULL;
			}
			strcpy(sb, pLCacheEnt[i].etherToIPLine);
			*rc = LD_SUCCESS;
			return sb;
		    }
		    break;
		}
	    }
	}
	return NULL;
    default:
	ldhcp_logprintf(LDHCP_LOG_CRIT, "getLCacheEntry: unknown class %d",
			class);
    }
    return NULL;
}

		
void invalidateLCache(LCache *c)
{
    LCacheEnt *pLCacheEnt;
    int i;
    
    ldhcp_logprintf(LDHCP_LOG_MAX, "invalidateLCache sz %d", c->sz);
    if ((c == NULL) || (c->lCacheEntp == NULL))
	return;
    for (pLCacheEnt = c->lCacheEntp, i = 0; i < c->sz; i++) {
	if (pLCacheEnt[i].valid) {
	    pLCacheEnt[i].valid = 0;
	}
	else
	    break;
    }
    c->nextInsert = 0;
}

int insertLCacheEntries(LCache* c, int class, _arg_ld_t_ptr result)
{
    LCacheEnt *pLCacheEnt;
    int nument, first;
    _arg_ld_t_ptr a_result;
    struct in_addr ia;
    struct resvn r;
    char cid_ptr[MAXCIDLEN];
    int cid_len;
    char sbuf[128];		/* size ???? */
    char *eh, *ipc, *hna;
    char *lend;
    EtherAddr *eth;

    ldhcp_logprintf(LDHCP_LOG_MAX, "insertLCacheEntries class %d", class);

    bzero(restab, sizeof(restab));
    if (class == DHCPRESERVATION) {
	cleanup_restabm();
	if ((c == NULL) || (c->lCacheEntp == NULL))
	    return LD_ERROR;
    }
    nument = 0;
    first = 1;
    for (a_result = result; a_result; ) {
	if (nument != a_result->entnum) {
	    /* new record found */
	    if (first)
		first = 0;
	    else
		c->nextInsert = (c->nextInsert + 1) % c->sz;
	    nument = a_result->entnum;
	    pLCacheEnt = &(c->lCacheEntp[c->nextInsert]);
	    pLCacheEnt->class = class;
	    pLCacheEnt->valid = 1;
	    pLCacheEnt->type = 0;
	    switch (class) {
	    case DHCPLEASE:
		makeLeaseRecordWithCid(&a_result, pLCacheEnt->etherToIPLine, 
				       cid_ptr, &cid_len);
		result2keys(a_result, pLCacheEnt->cid, 
			    &pLCacheEnt->ipa, pLCacheEnt->hostname);
		break;
	    case DHCPRESERVATION:
		makeReservationRecord(a_result, pLCacheEnt->etherToIPLine,
				      "insertLCacheEntries");
		result2keys(a_result, pLCacheEnt->cid, 
			    &pLCacheEnt->ipa, pLCacheEnt->hostname);
		r.cacheIndex = c->nextInsert;
		r.optionvalues = result2optionvalues(a_result);
		r.cfg = getDHCPConfigForAddr(pLCacheEnt->ipa, r.optionvalues);
		insert_restab(CID, pLCacheEnt->cid, &r);
		ia.s_addr = pLCacheEnt->ipa;
		insert_restab(IPA, inet_ntoa(ia), &r);
		insert_restab(HNM, pLCacheEnt->hostname, &r);
		mk_cid(pLCacheEnt->cid, cid_ptr, &cid_len);
		break;
	    }
	    strcpy(sbuf, pLCacheEnt->etherToIPLine);
	    parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend);
	    eth = ether_aton(eh);
	    LOCKDHCPDB(dblock);
	    putRecByCid(cid_ptr, cid_len, db, pLCacheEnt->etherToIPLine);
	    putRecByEtherAddr(eth, db, cid_ptr, cid_len);
	    putRecByHostName(pLCacheEnt->hostname, db, cid_ptr, cid_len);
	    putRecByIpAddr(pLCacheEnt->ipa, db, cid_ptr, cid_len);
	    UNLOCKDHCPDB(dblock);
	}
	while (a_result && (a_result->entnum == nument))
	    a_result = a_result->next;
    }
    c->nextInsert = (c->nextInsert + 1) % c->sz;
    return LD_OK;
}


