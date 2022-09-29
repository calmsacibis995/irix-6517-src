#ifndef _KSYS_VMMACROS_H_
#define _KSYS_VMMACROS_H_

#include <sys/pfdat.h>
#include <os/as/region.h>
#include <ksys/umapop.h>

/****************************************************************
		Ordinal number support macros
*****************************************************************/
extern int max_pageord;
extern pfd_t *ordinal_ptr;

#ifdef DISCONTIG_PHYSMEM
extern pfn_t page_ordinal[MAX_COMPACT_NODES][MAX_MEM_SLOTS];
#define pfn_to_ordinal_num(pfn)		(page_ordinal[PFN_TO_CNODEID(pfn)]  \
						[pfn_to_slot_num(pfn)] +     \
						pfn_to_slot_offset(pfn))
#else
#define pfn_to_ordinal_num(pfn)		(pfntopfdat(pfn) - ordinal_ptr)
#endif

/******************************************************************
		Integrated job rss and rmap operation macros
*******************************************************************/

#ifdef MISER_ARRAY
/*
 * Possible operations include INS_PFD, INC_RELE_UNDO, FLIP_PFD_ADD. 
 * Ins and Flip are optimized to not call job rss routines.
 */
#define VPAG_UPDATE_RMAP_ADDMAP(vpag, op, pfd, pte, pm)			\
	if (((vpag) == NULL) ||						\
	   (VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd), 0, 0) == 0)) {	\
		RMAP_ADDMAP((pfd), (pte), (pm));			\
	}

/*
 * Only possible operation is INC_FOR_PFD
 */
#define VPAG_UPDATE_RMAP_ADDMAP_RET(vpag, op, pfd, pte, pm, ret)	\
	if (((vpag) == NULL) ||						\
	   ((ret = VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd), 0, 0))==0)) {\
		RMAP_ADDMAP((pfd), (pte), (pm));			\
		ret = 0;						\
	}

/*
 * Possible operations are JOBRSS_DECREASE, FLIP_PFD_DEL.
 */
#define VPAG_UPDATE_RMAP_DELMAP(vpag, op, pfd, pte) 			\
	if (((vpag) == NULL) ||						\
	   (VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd), 0, 0) == 0)) {	\
		DELETE_MAPPING((pfd), (pte));				\
	}

/*
 * The only possible operation is JOBRSS_INC_RELE.
 */
#define VPAG_UPDATE_RMAP_DELMAP_RET(vpag, op, pfd, pte, ret)		\
	if (((vpag) == NULL) ||						\
	   ((ret = VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd), 0, 0))==0)) {\
		DELETE_MAPPING((pfd), (pte));				\
		ret = 0;						\
	}
#else
/*
 * Possible operations include INS_PFD, INC_RELE_UNDO, FLIP_PFD_ADD. 
 * Ins and Flip are optimized to not call job rss routines.
 */
#define VPAG_UPDATE_RMAP_ADDMAP(vpag, op, pfd, pte, pm)			\
	if (((vpag) == NULL) || ((op) & JOBRSS_NO_OP)) {		\
		RMAP_ADDMAP((pfd), (pte), (pm));			\
	} else {							\
		VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd),		\
			(__psunsigned_t)(pte), (__psunsigned_t)(pm));	\
	}

/*
 * Only possible operation is INC_FOR_PFD
 */
#define VPAG_UPDATE_RMAP_ADDMAP_RET(vpag, op, pfd, pte, pm, ret)	\
	if ((vpag) == NULL) {						\
		RMAP_ADDMAP((pfd), (pte), (pm));			\
		ret = 0;						\
	} else {							\
		ret = VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd), 	\
			(__psunsigned_t)(pte), (__psunsigned_t)(pm));	\
	}

/*
 * Possible operations are JOBRSS_DECREASE, FLIP_PFD_DEL. Flip is
 * optimized to not call job rss routines.
 */
#define VPAG_UPDATE_RMAP_DELMAP(vpag, op, pfd, pte) 			\
	if (((vpag) == NULL) || ((op) & JOBRSS_NO_OP)) {		\
		DELETE_MAPPING((pfd), (pte));				\
	} else {							\
		VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd),		\
			(__psunsigned_t)(pte), 0);			\
	}

/*
 * The only possible operation is JOBRSS_INC_RELE.
 */
#define VPAG_UPDATE_RMAP_DELMAP_RET(vpag, op, pfd, pte, ret)		\
	if ((vpag) == NULL) {						\
		DELETE_MAPPING((pfd), (pte));				\
		ret = 0;						\
	} else {							\
		ret = VPAG_UPDATE_VM_RESOURCE((vpag), (op), (pfd),	\
			(__psunsigned_t)(pte), 0);			\
	}
#endif

#endif /* _KSYS_VMMACROS_H_ */
