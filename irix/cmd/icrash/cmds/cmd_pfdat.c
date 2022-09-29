#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_pfdat.c,v 1.23 1999/12/17 20:27:52 lucc Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <klib/klib.h>
#include <malloc.h>
#include "icrash.h"

/*
 * The below defines apply only to SN0. They will have to be modified at SN(>1)
 * time. Use icrash internal variables as much as possible.
 */
#define KL_NODE_MAX_MEM_SIZE K_MEM_PER_NODE
#define KL_NODE_PFDAT_OFFSET (24*1024*1024LL)      /* This is hardwired. */
#define KL_PFN_TO_NODE_BASE_PFN(PFN)\
		((PFN) & ~(KL_NODE_MAX_MEM_SIZE/K_NBPC-1))
#define KL_PFN_TO_NODE_OFFSET_PFN(PFN)\
	((PFN) & (KL_NODE_MAX_MEM_SIZE/K_NBPC-1))

#define KL_PFN_TO_NODE_PFDAT_BASE(PFN)     \
 ( KL_PHYS_TO_K0(Ctob(KL_PFN_TO_NODE_BASE_PFN(PFN)) + \
   KL_NODE_PFDAT_OFFSET) )

#define KL_PFD_TO_NODE_BASE_PFN(PFD)       \
( (k0_to_phys(PFD) & ~(KL_NODE_MAX_MEM_SIZE-1) ) >> K_PNUMSHIFT)

#define KL_PFD_TO_NODE_OFFSET_PFN(PFD)     \
((((unsigned long)(PFD) & (KL_NODE_MAX_MEM_SIZE-1)) - \
   KL_NODE_PFDAT_OFFSET) / PFDAT_SIZE )

#define KL_NASID_TO_COMPACT_NODEID(nnode) \
	((PLATFORM_SNXX) ? sn0_nasid_to_compact_node(nnode) : 0)
#define KL_PFN_NASIDOFFSET(PFN) ((PFN) & ~KL_PFN_NASIDMASK)
#define KL_MKPFN(NASID, OFF) (((NASID) << KL_PFN_NASIDSHIFT) | (OFF))
#define SN0_MAX_NASIDS          256
#define SN0_MAX_COMPACT_NODES   64
#define KL_LOCAL_PFDATTONASID(PFD) \
	(((__psunsigned_t)(PFD) & KL_PFD_NASIDMASK) >> KL_NASID_SHIFT)
#define KL_COMPACT_TO_NASID_NODEID(CNODE) \
	((PLATFORM_SNXX) ? sn0_compact_to_nasid_node(CNODE) : 0)
#define KL_SLOT_GETBASEPFN(NODE,SLOT) \
	(KL_MKPFN(KL_COMPACT_TO_NASID_NODEID(NODE), SLOT<<KL_SLOT_PFNSHIFT))
#define INVALID_CNODEID (signed short)-1

/* IP32 specific defines.. What are these ?.
 */
#define ECCBYPASS_BASE  0x80000000      /* ECC-bypass memory alias */
#define P_ECCSTALE      0x100000        /* stale ECC, ref via no-ECC alias */

kaddr_t kl_pfntopfdat(kaddr_t pfn);

/*================================== compact_to_nasid_node ========================*/
static k_ptr_t Pcompact_to_nasid_node;
/*
 *  compact_to_nasid_node()
 */
void 
sn0_compact_to_nasid_node_init()
{
	int size;

#ifdef ICRASH_DEBUG
	assert(PLATFORM_SNXX);
#endif

	size = sizeof(signed short);
	Pcompact_to_nasid_node = 
		kl_alloc_block(((int)size * SN0_MAX_COMPACT_NODES), K_PERM);
	
	kl_get_block(kl_sym_addr("compact_to_nasid_node"),
			 ((int)size * SN0_MAX_COMPACT_NODES),
			 Pcompact_to_nasid_node, "compact_to_nasid_node");
}
void
sn0_compact_to_nasid_node_end()
{
	kl_free_block(Pcompact_to_nasid_node);
}
/*
 *  compact_to_nasid_node()
 */
int 
sn0_compact_to_nasid_node(int cnode)
{
	signed short nasid;

	nasid = *(signed short *)((__psunsigned_t)Pcompact_to_nasid_node + 
					cnode * sizeof(signed short));
	return((int)nasid);
}

/*============================  sn0_nasid_to_compact_node =======================*/
static  k_ptr_t Pnasid_to_compact_node;
/*
 *  Store nasid/nodeid lut locally for needed speedup
 */
void
sn0_nasid_to_compact_node_init()
{
        int size;

#ifdef ICRASH_DEBUG
        assert(PLATFORM_SNXX);
#endif

        size = sizeof(signed short);
        Pnasid_to_compact_node =
                kl_alloc_block((int)size * SN0_MAX_NASIDS, K_PERM);

        kl_get_block(kl_sym_addr("nasid_to_compact_node"),
                         ((int)size*SN0_MAX_NASIDS),
                         Pnasid_to_compact_node,"nasid_to_compact_node");
}
/* undo what the _init function did */
void
sn0_nasid_to_compact_node_end()
{
	kl_free_block(Pnasid_to_compact_node);
}
/*
 *  nasid_to_compact_node()
 */
int 
sn0_nasid_to_compact_node(int nnode)
{
	signed short cnodeid;

#ifdef ICRASH_DEBUG
	assert(PLATFORM_SNXX);
#endif

	cnodeid = *(signed short *)((__psunsigned_t)Pnasid_to_compact_node + nnode * sizeof(signed short));
	return((int)cnodeid);
}


/*==========================  sn0_slot_getsize ==========================*/
static k_ptr_t P_slots;
void
sn0_slot_getsize_init()
{
	P_slots = kl_alloc_block((K_SLOTS_PER_NODE * SN0_MAX_COMPACT_NODES * 
				   sizeof(signed short)), K_PERM);
	kl_get_block(kl_sym_addr("slot_psize_cache"),
			 (K_SLOTS_PER_NODE * SN0_MAX_COMPACT_NODES *
			  sizeof(signed short)), P_slots, "slot_psize_cache");
}
void 
sn0_slot_getsize_end()
{
	kl_free_block(P_slots);
}

/*
 * sn0_slot_getsize()
 *
 * size in pfn..
 */
int 
sn0_slot_getsize(int node,int slot) 
{
	signed short size;

#ifdef ICRASH_DEBUG
	assert(PLATFORM_SNXX);
#endif
	
	size = *(signed short *)((__psunsigned_t)P_slots + 
				   sizeof(signed short) * ((node * K_SLOTS_PER_NODE) + slot));

	return((int)size);
}

void
sn0_init_init()
{
	sn0_compact_to_nasid_node_init();
	sn0_nasid_to_compact_node_init();
	sn0_slot_getsize_init();
}

/* Need a "end" function for the above at some point */


/*
 * Code is the same as function ckphyspnum in the kernel.
 *
 * It basically checks if the pfn is valid in the node with nasid 
 * KL_PFN_NASID(pfn)
 *
 */
int 
sn0_valid_pfn(int pfn)
{
	register signed short node;
	signed short nasid;
	register int slot, numslots;
	register int start;
	
#ifdef ICRASH_DEBUG
	assert(PLATFORM_SNXX);
#endif

	if ((nasid = KL_PFN_NASID(pfn)) < 0 || nasid >= SN0_MAX_NASIDS ||
			(node = KL_NASID_TO_COMPACT_NODEID(nasid)) == INVALID_CNODEID) {
		return 0;			       /* illegal pfn */
	}

	numslots = K_SLOTS_PER_NODE;
	
	for (slot = 0; slot < numslots; slot++) {
		start = KL_SLOT_GETBASEPFN(node, slot);
		
		if (pfn < start) {
			break;			       /* illegal pfn */
		}
		else if (pfn < start + sn0_slot_getsize(node, slot)) {
			return(1);		       /* legal pfn */
		}
	}
	
	return(0);				       /* illegal pfn */
}

/*
 * Return the next valid pfdat after pfdat_first in between pfdat_first and 
 * pfdat_last within this node.
 * Returns (kaddr_t)-1 if unable to find a valid pfdat within this node in
 * between pfdat_first and pfdat_last.
 *
 * This was done to speed up the printing of pfdat's when the -a option is
 * specified and we encounter a hole in the pfdat array. So instead of 
 * incrementing the pfdat one pfdat at a time ... we try to jump across the
 * hole to the next valid pfdat.
 */
kaddr_t 
sn0_next_valid_pfdat(kaddr_t pfdat_first, kaddr_t pfdat_last)
{
	int pfn_pfdat_ret;
	int pfn_pfdat_last = KL_PFDATTOPFN(pfdat_last);
	int slot, numslots, pfn_slot_first, pfns_in_slot, node;
	kaddr_t pfdat_ret;

#ifdef ICRASH_DEBUG
	assert(PLATFORM_SNXX);
#endif

	pfdat_ret = pfdat_first + PFDAT_SIZE;
	if (pfdat_ret >= pfdat_last) {
		return pfdat_ret;
	}

	pfn_pfdat_ret = KL_PFDATTOPFN(pfdat_ret);
	numslots = K_SLOTS_PER_NODE;
	node = KL_NASID_TO_COMPACT_NODEID(KL_PFN_NASID(pfn_pfdat_ret));
	
	for (slot = 0; slot < numslots; slot++) {
		pfn_slot_first = KL_SLOT_GETBASEPFN(node, slot);
		pfns_in_slot  = sn0_slot_getsize(node, slot);

		/* All we have to make sure here is that pfdat_ret is 
		 * in a slot with physical memory. If it is not in a 
		 * slot with physical memory... find the first slot 
		 * with memory and return the pfdat of the base of that
		 * slot.
		 */
		if (!pfns_in_slot) {
			/* No physical memory in this slot.
			 */
			continue;
		}
		
		/* This slot has memory.  
		*/
		if (pfn_pfdat_ret > pfn_slot_first && 
			(pfn_pfdat_ret < (pfn_slot_first + pfns_in_slot))) {
			/* We are in this slot.
			 */
			return(KL_PFNTOPFDAT(pfn_pfdat_ret));
		}
		else if (pfn_pfdat_ret < pfn_slot_first) {

			/* Set to the base of this slot... since we must
			 * have hit a hole in memory.
			 */
			pfn_pfdat_ret = pfn_slot_first;
			return KL_PFNTOPFDAT(pfn_pfdat_ret);
		}
	}
	/*
	 * We were unable to find the next valid pfdat in this node. The 
	 * caller now is expected to either give up... or move on to the 
	 * next node.
	 */
	return((kaddr_t) -1);
}

/*
 * sn0_next_valid_pfn()
 *
 * This returns the next valid pfn after pfn_first but less than pfn_last.
 * It goes through all the nodes... starting from compact node id 0.
 */
int 
sn0_next_valid_pfn(int pfn_first, int pfn_last)
{
	int i=0;
	kaddr_t KPpfdat_first, KPpfdat_last, KPpfdat_next;
	int pfn;

#ifdef ICRASH_DEBUG
	assert(PLATFORM_SNXX);
#endif

	for (i = 0; i < K_NUMNODES; i++) {
		if (KL_SLOT_GETBASEPFN(i,0) > pfn_first) {
			pfn_first = KL_SLOT_GETBASEPFN(i, 0);
			if (valid_pfn(pfn_first)) {
				/* Just return if the first pfn we find is
				 * valid.
				 */
				return(pfn_first);
			}
		}
		
		KPpfdat_first = KL_PFNTOPFDAT(pfn_first);
		KPpfdat_last  = KL_PFNTOPFDAT(pfn_last);
		KPpfdat_next  = NEXT_PFDAT(KPpfdat_first,KPpfdat_last);

		/* NEXT_PFDAT returns an invalid pfdat if it
		 * could not find a valid pfdat between KPpfdat_first &
		 * KPpfdat_last in this node. So we need to check for that case
		 */
		if ((KPpfdat_next != (kaddr_t)-1) && 
				(valid_pfn(pfn=KL_PFDATTOPFN(KPpfdat_next)))) {
			return pfn;
		}
	}
	return(-1);
}

#ifdef ICRASH_DEBUG
/* 
 * Print out all the contiguous areas of memory. Extremely slow function --
 * because it labors through each pfn one at a time.
 */
void 
print_pfns(FILE *ofp)
{
	register int i = 0, total_pfns = 0;
	register int pfn = 0;
	register int pfn_old = 0;
	__uint32_t maxclick;
	kaddr_t KPmaxclick;

	fprintf(ofp,"VALID PFNS :\n");
	KPmaxclick = kl_sym_addr("maxclick");
	if (!KL_ERROR) {
		kl_get_block(KPmaxclick, 4, &maxclick, "maxclick");
	}
	else {
		return;
	}
			
	for (pfn=0 ;(pfn <= maxclick) && (pfn >= 0);
			 pfn=next_valid_pfn(pfn,maxclick)) {
		assert(pfn >= 0);
		if (!valid_pfn(pfn)) {
			continue;
		}
		if (pfn_old != (pfn-1) || pfn == 0) {

			/* Hit a hole!. Ugly logic... It's ok.. since
			 * this gets enabled only with ICRASH_DEBUG.
			 */
			if (pfn != 0 && valid_pfn(pfn_old)) {

				/* If we hit a hole.. that means we 
				 * also are at the end of one set of
				 * contiguous pfn's.
				 */
				fprintf(ofp,"-- %-10d \t(%d)", pfn_old, i);
				i = 0;
			}
			fprintf(ofp,"\n\tNode : %4d %10d ", KL_PFN_NASID(pfn), pfn);
		}
		pfn_old = pfn;
		i++, total_pfns++;
	}
	fprintf(ofp,"-- %-10d \t(%d)\n", maxclick, i);
	fprintf(ofp,"\tTotal : %28s\t %d\n"," ", total_pfns);
}
#endif
int 
next_valid_pfn(int pfn_first, int pfn_last)
{
	int i = 0, pfn = 0;
	kaddr_t KPpfdat_first,KPpfdat_last,KPpfdat_next;


	if (pfn_first < 0) {
		pfn_first = 0;
	}

	/* This function is called only when we are searching memory for
	   a specific patern. So to speed things up we need to make sure
	   the page is in the vmcore file.

	   XXX if someone calls this function for other purpose, this
	   will need to be changed. Probably a new function next_pfn_valid_and_in_core()
	   would be needed for the search functionality itself.
	*/
			
	if (PLATFORM_SNXX) {

			return sn0_next_valid_pfn(pfn_first,pfn_last);
	}
	else {
		for (; pfn_first < pfn_last; pfn_first++) {

			/* In the non-SNXX case there is no easy way to jump 
			 * over holes.. So we go the painfully slow way of one
			 * pfn at a time.
			 */
			KPpfdat_first = KL_PFNTOPFDAT(pfn_first);
			KPpfdat_last  = KL_PFNTOPFDAT(pfn_last);
			KPpfdat_next  = NEXT_PFDAT(KPpfdat_first,KPpfdat_last);
			
			/*
			 * NEXT_PFDAT returns an invalid pfdat if it
			 * could not find a valid pfdat between KPpfdat_first &
			 * KPpfdat_last in this node. So we need to check for 
			 * that case
			 */
			if ((KPpfdat_next != (kaddr_t)-1) &&
					(valid_pfn(pfn=KL_PFDATTOPFN(KPpfdat_next)))) {
				return(pfn);
			}
		}
	}
	return -1;
}

/*
 * This returns 1 if the pfn is part of physically present memory.
 */
int 
valid_pfn(int pfn)
{
	__uint32_t maxclick,kpbase;
	kaddr_t KPmaxclick,KPkpbase;
	k_ptr_t Ppfdat;
	int valid = 0;
	kaddr_t KPpfdat;

	if (PLATFORM_SNXX) {
		valid = sn0_valid_pfn(pfn);
	}
	else {

		/* We assume the rest all have contiguous memory.
		 * Do octanes have holes in memory too ?.
		 */
		Ppfdat = kl_alloc_block(PFDAT_SIZE, K_TEMP);
		KPpfdat = KL_PFNTOPFDAT(pfn);
		kl_get_struct(KPpfdat, PFDAT_SIZE, Ppfdat, "pfdat");
		KPmaxclick = kl_sym_addr("maxclick");
		if (!KL_ERROR) {
			kl_get_block(KPmaxclick, 4, &maxclick, "maxclick");
		}
		else {
			return 0;
		}

		KPkpbase = kl_sym_addr("kpbase");
		if (!KL_ERROR) {
			kl_get_block(KPkpbase, 4, &kpbase, "kpbase");
		}
		else {
			kpbase = Btoc(KL_PFDATTOPFN(K_PFDAT));
		}

		/* CKPHYSPNUM macro..
		 */
		valid = !(((Ctob(pfn) >= kpbase) && 
			   ((ushort)KL_INT(Ppfdat,"pfdat","pf_use") == (ushort)-1))
			  || (pfn > maxclick) ||
			  (pfn < (int)Btoc(k0_to_phys(K_K0BASE|K_RAM_OFFSET))));
		kl_free_block(Ppfdat);
	}
	return(valid);
}


/*
 * Do the machine dependent thing and return the correct value.
 * We assume that the pfdat passed to us is a real pfdat value.
 */
int 
kl_pfdattopfn(kaddr_t pfdat)
{
	k_ptr_t Ppfdat = kl_alloc_block(PFDAT_SIZE, K_TEMP);
	int pfn;

	if (PLATFORM_SNXX) {
		pfn = (KL_PFD_TO_NODE_BASE_PFN(pfdat) | 
			   KL_PFD_TO_NODE_OFFSET_PFN(pfdat));
	}
	else if (K_IP == 32) {
		kl_get_struct(pfdat, PFDAT_SIZE, Ppfdat, "pfdat");
		pfn = (pfdat - K_PFDAT)/PFDAT_SIZE;
		pfn = (kl_uint(Ppfdat,"pfdat","pf_flags",0) & P_ECCSTALE) ?
				(Btoc(ECCBYPASS_BASE) | (pfn)) : pfn;
	}
	else {
		pfn = (pfdat - K_PFDAT)/PFDAT_SIZE;
	}
	kl_free_block(Ppfdat);
	return pfn;
}

/*
 * Macro pfntopfdat in the kernel.
 */
kaddr_t 
kl_pfntopfdat(kaddr_t pfn)
{
	if (PLATFORM_SNXX) {
		return((KL_PFN_TO_NODE_PFDAT_BASE(pfn) + 
			KL_PFN_TO_NODE_OFFSET_PFN(pfn) * PFDAT_SIZE));
	}
	else if (K_IP == 32) {
		return K_PFDAT + (pfn & ~Btoc(ECCBYPASS_BASE)) * PFDAT_SIZE;
	}
	else {
		return K_PFDAT + pfn * PFDAT_SIZE;
	}
}


/*
 * pfdat_cmd() -- Dump out page frame data address data.
 */
int
pfdat_cmd(command_t cmd)
{
	int i, pfn, pfdat_cnt = 0, firsttime = 1,j;
	int pfdat_offset;
	kaddr_t value = 0, pfdat_first, pfdat_last,KPtemp;
	k_ptr_t pfp;

	pfp = kl_alloc_block(PFDAT_SIZE, K_TEMP);

	if (cmd.flags & C_ALL) {
		/*
		 * User wants us to print all the pfdats in the sytem.
		 */
		pfdat_banner(cmd.ofp, BANNER|SMAJOR);
		for (i=0;i<K_NUMNODES;i++)
		{
			KPtemp = kl_kaddr_to_ptr(K_NODEPDAINDR + K_NBPW *
				KL_NASID_TO_COMPACT_NODEID(K_MASTER_NASID));
			KPtemp += 
				kl_member_offset("nodepda_s","node_pg_data");
			KPtemp = kl_kaddr_to_ptr((KPtemp + 
				kl_member_offset("pg_data_t", "pg_freelst")));
			KPtemp += kl_struct_len("pg_free") * i;

			/* KPtemp now points to the pg_free structure.
			 */
			pfdat_first = kl_kaddr_to_ptr((KPtemp + 
				kl_member_offset("pg_free", "pfd_low")));
			pfdat_last = kl_kaddr_to_ptr((KPtemp + 
				kl_member_offset("pg_free", "pfd_high")));
			for (;((pfdat_first <= pfdat_last) && 
				(pfdat_first != (kaddr_t)-1));
				 pfdat_first = NEXT_PFDAT(pfdat_first, pfdat_last)) {
				value = kl_pfdattopfn(pfdat_first);

				/* At present this only checks if the pfn is
				 * a valid physical pfn. This is ok for this
				 * test here... since we got the pfn value from
				 * the pfdat.. but it is not ok later... 
				 */
				if (!VALID_PFN((int)value)) {
					if (DEBUG(DC_GLOBAL, 1)) {
						fprintf(cmd.ofp, "Invalid pfdat(pfn):0x%16llx(0x%x)\n",
							pfdat_first, (unsigned)value);
					}
					continue;
				}

				if (pfdat_cnt >= K_PHYSMEM) {
					/* ERROR!. Aargh!... Outta' here.
					 */
					break;
				}

				kl_get_struct(pfdat_first, PFDAT_SIZE, pfp, "pfdat");
				if (!KL_ERROR) {
					if (DEBUG(DC_GLOBAL, 1) > 1 || (cmd.flags & C_FULL)) {
						if (!firsttime) {
							pfdat_banner( cmd.ofp, BANNER|SMAJOR);
						} 
						else {
							firsttime = 0;
						}
					}
					print_pfdat(pfdat_first, pfp, cmd.flags, cmd.ofp);
					if (DEBUG(DC_GLOBAL, 1)) {
						fprintf(cmd.ofp, "pfn=0x%x, pfdat=0x%llx\n",
							KL_PFDATTOPFN(pfdat_first), pfdat_first);
					}
					pfdat_cnt++;
				}
				else {		       
					/* We hit an error. Outta' here.
					 */
					break;
				}
			}
		}
	}
#ifdef ICRASH_DEBUG
	else if (cmd.flags & C_NEXT) {
		print_pfns(cmd.ofp);
		return 0;
	}
#endif
	else if (cmd.nargs == 0) {
		pfdat_usage(cmd);
		kl_free_block(pfp);
		return(1);
	}
	else {
		pfdat_banner(cmd.ofp, BANNER|SMAJOR);
		for (i = 0; i < cmd.nargs; i++) {
			if (!strncmp(cmd.args[i], "#", 1)) {
				pfn = atoi(&cmd.args[i][1]);
				if (!VALID_PFN(pfn)) {

					/* At present this only checks if the pfn is
					 * a valid physical pfn. This is not ok...since
					 * pfn's which are below the pfdat array for
					 * each node also are returned as valid.
					 * However, We rely on the check below
					 *   if ((value >= pfdat_first) &&
					 *       (value <= pfdat_last))
					 * to catch this case.
					 */
					fprintf(cmd.ofp, "pfdat: PFN %d is not valid\n", pfn);
					continue;
				}
				value = KL_PFNTOPFDAT(pfn);
			} 
			else {
				GET_VALUE(cmd.args[i], &value);
				if (KL_ERROR) {
					KL_ERROR |= KLE_BAD_PFDAT;
					kl_print_error();
					continue;
				}
				pfn = KL_PFDATTOPFN(value);
			}
			for (j = 0; j < K_NUMNODES; j++) {
				KPtemp = kl_kaddr_to_ptr((K_NODEPDAINDR + K_NBPW * 
							  K_MASTER_NASID));
				KPtemp += kl_member_offset("nodepda_s",
							   "node_pg_data");
				KPtemp = kl_kaddr_to_ptr((KPtemp + 
					kl_member_offset("pg_data_t", "pg_freelst")));
				KPtemp += kl_struct_len("pg_free") * j;

				/* KPtemp now points to the pg_free 
				 * structure.
				 */
				pfdat_first = kl_kaddr_to_ptr((KPtemp + 
					kl_member_offset("pg_free", "pfd_low")));
				pfdat_last = kl_kaddr_to_ptr((KPtemp + 
					kl_member_offset("pg_free", "pfd_high")));
				if ((value >= pfdat_first) && (value <= pfdat_last)) {
					break;
				}
			}
			if (j == K_NUMNODES) {
				/* Invalid pfdat.
				 */
				fprintf(cmd.ofp, "0x%llx: invalid pfdat pointer\n", value);
				continue;
			}
			kl_get_struct(value, PFDAT_SIZE, pfp, "pfdat");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PFDAT;
				kl_print_error();
				continue;
			}
			else {
				if (DEBUG(DC_GLOBAL, 1) > 1 || (cmd.flags & C_FULL)) {
					if (!firsttime) {
						pfdat_banner(cmd.ofp, BANNER|SMAJOR);
					} 
					else {
						firsttime = 0;
					}
				}
				print_pfdat(value, pfp, cmd.flags, cmd.ofp);
				pfdat_cnt++;
			} 
		}
	}
	pfdat_banner(cmd.ofp, SMAJOR);
	PLURAL("pfdat struct", pfdat_cnt, cmd.ofp);
	kl_free_block(pfp);
	return(0);
}

#ifdef ICRASH_DEBUG
#define _PFDAT_USAGE "[-n] [-a] [-w outfile] [-f pfdat_list]"
#else
#define _PFDAT_USAGE "[-a] [-w outfile] [-f pfdat_list]"
#endif

/*
 * pfdat_usage() -- Print the usage string for the 'pfdat' command.
 */
void
pfdat_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PFDAT_USAGE);
}

/*
 * pfdat_help() -- Print the help information for the 'pfdat' command.
 */
void
pfdat_help(command_t cmd)
{
	CMD_HELP(cmd, _PFDAT_USAGE,
		 "Display the pfdat structure located at each virtual address "
		 "included in pfdat_list. If the -a flag is issued, display "
		 "all of the pfdat structures on the system "
		 "(one per physical page)."
		 "The pfn can also be specified instead of the pfdat by"
		 "prepending it with a #."
#ifdef ICRASH_DEBUG
		 "Use -n option to print a list of contiguous pfn's.");
#else
	);
#endif
}

/*
 * pfdat_parse() -- Parse the command line arguments for 'pfdat'.
 */
int
pfdat_parse(command_t cmd)
{
#ifdef ICRASH_DEBUG
	return (C_MAYBE|C_WRITE|C_FULL|C_ALL|C_NEXT);
#else
	return (C_MAYBE|C_WRITE|C_FULL|C_ALL);
#endif
}
