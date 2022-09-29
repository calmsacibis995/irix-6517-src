
#ifndef _TKC_PRIVATE_H_
#define _TKC_PRIVATE_H_ 1

/*
 * Functions exported by tkc.c
 */
extern void tkc_Hold(
	struct tkc_vcache *vcp
);
extern void tkc_ReleaseTokens(
	struct tkc_sets *setsp,
	register long size
);

/*
 * Functions exported by tkc_hostops.c
 */
extern long tkc_OrAllTokens(
	struct squeue *alp
);
extern tkc_RevokeToken(
	struct afsRevokeDesc *revokep,
	afs_hyper_t *tokenType
);

/*
 * Functions exported by tkc_revoke.c
 */
extern int tkc_RevokeDataToken(
	struct tkc_vcache *avc,
	struct afsRevokeDesc *at,
	long atype
);
extern int tkc_RevokeLockToken(
	struct tkc_vcache *avc,
	struct afsRevokeDesc *at,
	long atype,
	struct tkc_tokenList *tlp
);
extern int tkc_RevokeOpenToken(
    struct tkc_vcache *avc,
    struct afsRevokeDesc *at,
    long atype
);
extern int tkc_RevokeStatusToken(
	struct tkc_vcache *avc,
	struct afsRevokeDesc *at,
	long atype
);

/*
 * Functions exported by tkc_tokens.c
 */
extern tkc_DelToken(
	struct tkc_tokenList *tlp
);
extern tkc_DoDelToken(
	struct tkc_vcache *avc
);
extern tkc_FinishToken(
  struct tkc_vcache *vcp,
  struct tkc_tokenList *lp,
  afs_token_t *tokenp
);
extern tkc_ReturnSpecificTokens(
	struct tkc_vcache *vcp,
	u_long tokenType
);

#endif /* _TKC_PRIVATE_H_ */
