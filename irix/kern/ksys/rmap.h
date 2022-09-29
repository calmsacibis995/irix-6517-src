#ifndef _KSYS_RMAP_H_
#define _KSYS_RMAP_H_

#include <sys/rmap.h>

/*
 * The following 6.5/6.5.1 versions of the macros are slow, and have
 * been modified to be more performance oriented. Come 6.6, we should
 * merge all sys/rmap.h into ksys/rmap.h. For now, we just want to 
 * change the implementation for these macros. We must make sure to
 * preserve all macros/interfaces exported via sys/rmap.h in case
 * some 3rd party drivers use them. All kernel code where we care
 * about performance should use these new versions.
 */

#undef RMAP_ADDMAP
#undef RMAP_DELMAP

#define	RMAP_ADDMAP(pfdp, pdep, pm) \
	{ \
		int s; \
		s = RMAP_LOCK(pfdp); \
		RMAP_DOSTAT(rmapfastadd); \
		if (GET_RMAP_PDEP1(pfdp) == PDENULL) { \
			SET_RMAP_PDEP1(pfdp, pdep);\
			if (pfdp->pf_pdep2 == 0) { \
                                migr_start(pfdattopfn(pfdp), (pm)); \
                        } \
			RMAP_UNLOCK(pfdp, s); \
                } \
                else if (pfdp->pf_pdep2 == PDENULL) { \
                        ASSERT(GET_RMAP_PDEP1(pfdp) != NULL); \
                        ASSERT(pdep != GET_RMAP_PDEP1(pfdp)); \
                        pfdp->pf_pdep2 = pdep; \
			RMAP_UNLOCK(pfdp, s); \
                } \
		else { \
			while (rmap_addmap_nolock(pfdp, pdep, pm)) { \
				RMAP_UNLOCK(pfdp, s); \
				setsxbrk(); \
				s = RMAP_LOCK(pfdp); \
			} \
			RMAP_UNLOCK(pfdp, s); \
		} \
	}


#define	RMAP_DELMAP(pfdp, pdep) \
	{ \
		int s; \
		s = RMAP_LOCK(pfdp); \
		RMAP_DOSTAT(rmapfastdel); \
		if (pg_isshotdn(pdep)) { \
                	RMAP_UNLOCK(pfdp, s); \
        	} else if (GET_RMAP_PDEP1(pfdp) == pdep) { \
			SET_RMAP_PDEP1(pfdp, 0); \
			if (pfdp->pf_pdep2 == 0) { \
				ASSERT(pfdp->pf_rmapp == 0); \
				migr_stop(pfdattopfn(pfdp)); \
			} \
			RMAP_UNLOCK(pfdp, s); \
		} else if (pfdp->pf_pdep2 == pdep) { \
                        pfdp->pf_pdep2 = 0; \
                        if (GET_RMAP_PDEP1(pfdp) == 0) { \
                                migr_stop(pfdattopfn(pfdp)); \
                        } \
                	RMAP_UNLOCK(pfdp, s); \
                } \
                else { \
			rmap_delmap_nolock(pfdp, pdep); \
			RMAP_UNLOCK(pfdp, s); \
                } \
        }

/*
 * Operation definition for rmap_scanmap routine. Continued from
 * sys/rmap.h.
 */
#define	RMAP_JOBRSS_ANY	0x1000
#define RMAP_JOBRSS_TWO	0x2000
#define RMAP_MIGR_CHECK	0x4000

extern int 	rmap_addmap_nolock(pfd_t *, pde_t *, struct pm *);
extern void 	rmap_delmap_nolock(pfd_t *, pde_t *);

#endif /* _KSYS_RMAP_H_ */
