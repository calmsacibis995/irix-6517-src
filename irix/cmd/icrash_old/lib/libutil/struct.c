#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/struct.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#define _KERNEL 0
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include "icrash.h"
#include "extern.h"
#include "listvec.h"
#include <sys/param.h>
#include <sys/sema.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <sys/domain.h>
#include <sys/fsid.h>
#include <sys/mbuf.h>
#include <sys/tcpipstats.h>
#include <search.h>
#include <assert.h>

/* XXX - 'TIL we get the current root/toolroot
 */
#ifndef NSCHEDCLASS
#define NSCHEDCLASS (-(PWEIGHTLESS)+1)
#endif

#define INODE_TO_BHP(ip) \
		((k_ptr_t)((unsigned)ip + FIELD("inode", "i_bhv_desc")))

#define XFS_INODE_TO_BHP(xip) \
		((k_ptr_t)((unsigned)xip + FIELD("xfs_inode", "i_bhv_desc")))

#define LSNODE_TO_BHP(lsp) \
		((k_ptr_t)((unsigned)lsp + FIELD("lsnode", "ls_bhv_desc")))

#define CSNODE_TO_BHP(lsp) \
		((k_ptr_t)((unsigned)lsp + FIELD("csnode", "cs_bhv_desc")))

#define RNODE_TO_BHP(rp) \
		((k_ptr_t)((unsigned)rp + FIELD("rnode", "r_bhv_desc")))

/*
 *  Just a reminder: All the below address space macros take arguments which are
 *  pointers in icrash process address space and not in kernel address space.
 *  -- which is the same as k_ptr_t.
 *  We use KL_* for any macro which is also defined by the kernel.
 */
#define UT_ASID(ut) \
 ((k_ptr_t)((__psunsigned_t)ut + (__psunsigned_t)FIELD("uthread_s","ut_asid")))

#define KL_ASID_TO_VAS(asid) \
 ((kaddr_t)kl_kaddr(K,asid,"asid_t","as_obj"))

#define KL_ASID_TO_PASID(asid) \
 ((kaddr_t)kl_kaddr(K,asid,"asid_t","as_pasid"))

#define KL_AS_ISNULL(asid) \
 ((kaddr_t)KL_ASID_TO_VAS(asid) == (kaddr_t)0xf)

/* our own macro. BHVH -- behavior head. */
#define VAS_TO_BHVH(vas) \
 ((k_ptr_t)((__psunsigned_t)vas + (__psunsigned_t)FIELD("vas_s","vas_bhvh")))

#define KL_BHV_HEAD_FIRST(bhvh) \
 ((kaddr_t)kl_kaddr(K,bhvh,"bhv_head","bh_first"))

#define KL_BHV_PDATA(bhv) \
 ((kaddr_t)kl_kaddr(K,bhv,"bhv_desc","bd_pdata"))

#define KL_BHV_OPS(bhv) \
 ((kaddr_t)kl_kaddr(K,bhv,"bhv_desc","bd_ops"))

#define KL_BHV_VOBJ(bhv) \
 ((kaddr_t)kl_kaddr(K,bhv,"bhv_desc","bd_vobj"))

#define KL_BHV_TO_PAS(bhv) \
 ((kaddr_t)KL_BHV_PDATA(bhv))

#define KL_BHV_TO_VAS(bhv) \
 ((kaddr_t)KL_BHV_VOBJ(bhv))

#define KL_PREG_FIRST(pregion_set) \
 ((kaddr_t)kl_kaddr(K,pregion_set,"preg_set_t","avl_firstino"))

/*
 * We can assume the pregion is the same as the avlnode... because the first 
 * field in the pregion is the avlnode.. This won't change any time soon.
 */
#define KL_PREG_NEXT(pregion) \
 ((kaddr_t)kl_kaddr(K,pregion,"avlnode_t","avl_nextino"))

#define PAS_TO_PREG_SET(pas) \
 ((k_ptr_t)((__psunsigned_t)(pas) + (__psunsigned_t)FIELD("pas_s","pas_pregions")))

#define PPAS_TO_PREG_SET(ppas) \
 ((k_ptr_t)((__psunsigned_t)(ppas) + (__psunsigned_t)FIELD("ppas_s","ppas_pregions")))


typedef struct stype_s {
	char  *name;                     /* struct name                          */
	int	   size;                     /* struct size                          */
	int    links;             /* struct contains linked list pointer(s)      */
	void  (*banner)(FILE *, int);                  /* struct banner function */
	int   (*print)(kaddr_t, k_ptr_t, int, FILE *); /* struct print routine   */
} stype_t;

stype_t Struct[] = {
    /*----------------------------------------------------------------------*/
	/*  NAME          SIZE LINKS  BANNER()               PRINT()            */
    /*----------------------------------------------------------------------*/
	{ "avlnode",        0,  1, avlnode_banner,  	  print_avlnode    	     },
	{ "bhv_desc",       0,  1, bhv_desc_banner,  	  print_bhv_desc   	     },
	{ "buf",            0,  1, buf_banner,  	      print_buf        	     },
	{ "eframe_s",       0,  0, eframe_s_banner,  	  print_eframe_s   	     },
	{ "file",           0,  1, vfile_banner,  	      print_vfile      	     },
	{ "graph_vertex_s", 0,  0, graph_vertex_s_banner, print_graph_vertex_s   },
	{ "inode",          0,  1, inode_banner,    	  print_inode      	     }, 
	{ "inpcb",          0,  1, inpcb_banner,    	  print_inpcb      	     }, 
	{ "inventory_s",    0,  1, inventory_s_banner,    print_inventory_s 	 }, 
#ifdef ANON_ITHREADS
	{ "ithread_s",      0,  1, ithread_s_banner,  	  print_ithread_s  	     }, 
#endif
	{ "kthread",        0,  1, kthread_banner,  	  print_kthread   	     },	
	{ "mbstat",         0,  0, 0,                     0                	     },
	{ "mbuf",           0,  1, mbuf_banner,     	  print_mbuf       	     },	
	{ "ml_info",        0,  1, ml_info_banner,     	  print_ml_info       	 },	
	{ "mntinfo",        0,  1, mntinfo_banner,        print_mntinfo		 },
	{ "mrlock_s",       0,  0, mrlock_s_banner,       print_mrlock_s         },
	{ "nodepda_s",      0,  0, nodepda_s_banner,      print_nodepda_s  	     },
	{ "pda_s",          0,  0, pda_s_banner,          print_pda_s      	     },
	{ "pde",            0,  0, pde_banner,            0                	     },
	{ "pfdat",          0,  1, pfdat_banner,    	  print_pfdat      	     },	
	{ "pid_entry",      0,  1, pid_entry_banner,      print_pid_entry  	     },	
	{ "pmap",           0,  0, pmap_banner,           print_pmap       	     },
	{ "pregion",        0,  1, pregion_banner,  	  print_pregion    	     },
	{ "proc",           0,  1, proc_banner,     	  print_proc       	     },	
	{ "queue",          0,  1, queue_banner,    	  0                	     },
	{ "region",         0,  1, region_banner,   	  print_region     	     },	
	{ "rnode",          0,  1, rnode_banner,    	  print_rnode      	     },
	{ "sema_s",         0,  1, sema_s_banner,         print_sema_s     	     },
	{ "lsnode",          0,	1, lsnode_banner,    	  print_lsnode      	     },
	{ "socket",         0, 	1, socket_banner,   	  0                	     },
	{ "sthread_s",      0,  1, sthread_s_banner,  	  print_sthread_s  	     },
	{ "stdata",         0,	1, stream_banner,   	  0                	     },
	{ "swapinfo",       0,  0, swapinfo_banner,       0                	     },
	{ "tcpcb",          0,  0, tcpcb_banner,          print_tcpcb      	     },
	{ "unpcb",          0,	1, unpcb_banner,    	  print_unpcb      	     },
	{ "uthread_s",      0,  1, uthread_s_banner,  	  print_uthread_s  	     }, 
	{ "vfs",            0,  0, vfs_banner,            0                	     },
	{ "vfssw",          0,  0, vfssw_banner,          0                	     },
	{ "vnode",          0,	1, vnode_banner,    	  print_vnode      	     },
	{ "vproc",          0,	1, vproc_banner,    	  print_vproc      	     },
	{ "vsocket",        0,	1, vsocket_banner,    	  print_vsocket    	     },
	{ "xfs_inode",      0,	1, xfs_inode_banner,      print_xfs_inode  	     },
	{ "zone",           0,	1, zone_banner,     	  print_zone       	     },
	{ (char *)0,        0,  0, 0,               	  0                	     }
};

/*
 * init_struct_table()
 */
void
init_struct_table()
{
	int i = 0;

	/* All we need to do is set the stuct sizes
	 */
	while(Struct[i].name) {
		Struct[i].size = STRUCT(Struct[i].name);
		i++;
	}
}

/*
 * struct_index()
 */
int
struct_index(char *s)
{
	int i = 0;

	while(Struct[i].name) {
		if (!strcmp(s, Struct[i].name)) {
			return(i);
		}
		i++;
	}
	return(-1);
}

/*
 * struct_banner()
 */
void
struct_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(ofp, "NAME                              MEMBER      "
				"NEXT  SIZE  OFFSET\n");
		}
		else {
			fprintf(ofp, "NAME                                    SIZE\n");
		}
	}

	if (flags & SMAJOR) {
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(ofp, "===================================================="
				"============\n");
		}
		else {
			fprintf(ofp, "============================================\n");
		}
	}

	if (flags & SMINOR) {
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(ofp, "----------------------------------------------------"
				"----------------------\n");
		}
		else {
			fprintf(ofp, "----------------------------------\n");
		}
	}
}

/*
 * print_struct()
 */
void
print_struct(symdef_t *sdp, int flags, FILE *ofp)
{
	symdef_t *sfp;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(ofp, "print_struct: sdp=0x%x\n", sdp);
	}

	if (DEBUG(DC_GLOBAL, 1)) {
		fprintf(ofp, "%-30s  %8x  %8x  %4d  %6d\n", sdp->sd_name, 
			sdp->sd_member, sdp->sd_next, sdp->sd_size, 
			sdp->sd_offset);
	}
	else {
		fprintf(ofp, "%-38s  %4d\n", 
			sdp->sd_name, sdp->sd_size);
	}

	if (flags & C_FULL) {
		if (sfp = sdp->sd_member) {

			fprintf(ofp, "\n");
			fprintf(ofp, "                    FIELD NAME  SIZE  OFFSET\n");
			fprintf(ofp, "                    ----------  ----  ------\n");

			while (sfp) {
				if (DEBUG(DC_GLOBAL, 1)) {
					fprintf(ofp, "%30s  %8x  %8x  %4d  %6d\n", 
						sfp->sd_name, sfp->sd_member, sfp->sd_next, 
						sfp->sd_size, sfp->sd_offset);
				}
				else {
					fprintf(ofp, "%30s  %4d  %6d\n", 
						sfp->sd_name, sfp->sd_size, sfp->sd_offset);
				}
				sfp = sfp->sd_member;
			}
		}
		else {
			fprintf(ofp, "\n");
		}
	}
}

/*
 * structlist_banner()
 */
void
structlist_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "NUM  NAME               SIZE  LINKS\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "===================================\n");
	}
}

/*
 * structlist()
 */
void
structlist(FILE *ofp)
{
	int i = 0;

	structlist_banner(ofp, BANNER|SMAJOR);
	while(Struct[i].name) {
		fprintf(ofp, "%3d  %-16s  %5d  ", i, Struct[i].name, Struct[i].size);
		if (Struct[i].links) {
			fprintf(ofp, "  YES\n");
		}
		else {
			fprintf(ofp, "   NO\n");
		}

		i++;
	}
	structlist_banner(ofp, SMAJOR);
}

/*
 * struct_print() -- print out an arbitrary struct at addr
 */
int
struct_print(char *s, kaddr_t addr, int flags, FILE *ofp)
{
	int sid, size, count, firsttime = 1;
	k_ptr_t ptr;

	/* Make sure we have the right number of paramaters
	 */
	if (!s || !addr) {
		KL_SET_ERROR(KLE_BAD_STRUCT);
		return(1);
	}
	
	/* Get the index for struct name 
	 */
	if ((sid = struct_index(s)) == -1) {
		KL_SET_ERROR_CVAL(KLE_BAD_STRUCT, s);
		return(1);
	}

	/* Get the struct size 
	 */
	size = Struct[sid].size;

	if (!Struct[sid].banner) {
		fprintf(ofp, "%s: can't print struct banner\n", Struct[sid].name);
		return(1);
	}
	(*Struct[sid].banner)(ofp, BANNER|SMAJOR);

	/* Get an appropriately sized block of memory and load the contents
	 * of the structure into it.
	 */
	ptr = alloc_block(size, B_TEMP);
	kl_get_struct(K, addr, size, ptr, Struct[sid].name);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_STRUCT;
		return(1);
	}
	if (Struct[sid].print) {
		(*Struct[sid].print)(addr, ptr, flags, ofp);
	}
	else {
		fprintf(ofp, "%s: can't print struct\n", Struct[sid].name);
	}
	(*Struct[sid].banner)(ofp, SMAJOR);
	free_block(ptr);
	return(0);
}

/*
 * walk_structs() -- walk linked lists of kernel data structures 
 */
int
walk_structs(char *s, char *f, int offset, kaddr_t addr, int flags, FILE *ofp)
{
	int sid, size, count, firsttime = 1;
	kaddr_t last, next;
	k_ptr_t ptr;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(ofp, "walk_structs: s=%s, f=%s, offset=%d, addr=0x%llx, "
				"flags=0x%x, ofp=0x%x", s, f, offset, addr, flags, ofp);
	}

	/* Make sure we have the right number of paramaters
	 */
	if (!s || (!f && (offset == -1)) || !addr) {
		KL_SET_ERROR(KLE_BAD_STRUCT);
		return(1);
	}

	/* If we were passed a field name, determine its offset in the
	 * struct.
	 */
	if (offset == -1) {
		if ((offset = FIELD(s, f)) == -1) {
			KL_SET_ERROR_CVAL(KLE_BAD_FIELD, f);
			return(1);
		}
	}

	/* If the C_STRUCT flag is set, then print the structure in C-like 
	 * struct format.
	 */
	if (flags & C_STRUCT) {

		/* Get the struct size 
		 */
		if ((size = STRUCT(s)) == 0) {
			KL_SET_ERROR(KLE_BAD_STRUCT);
			return(1);
		}

		next = addr;
		ptr = alloc_block(size, B_TEMP);
		while(next) {
			kl_get_struct(K, next, size, ptr, s);
			if (KL_ERROR) {
				/* XXX -- should get error from struct_error() routine 
				 */
				KL_ERROR |= KLE_BAD_STRUCT;
				return(1);
			}
			dw_print_struct(s, next, ptr, flags, ofp);
			last = next;
			kl_get_kaddr(K, next+offset, &next, Struct[sid].name);
			if ((next == addr) || (next == last)) {
				break;
			}
		}
		free_block(ptr);
		return(0);
	}
	
	/* Get the index for struct name 
	 */
	if ((sid = struct_index(s)) == -1) {
		KL_SET_ERROR_CVAL(KLE_BAD_STRUCT, s);
		return(1);
	}

	/* Make sure the struct we selected has links we can follow
	 */
	if (!Struct[sid].links) {
		KL_SET_ERROR_CVAL(E_NO_LINKS, Struct[sid].name);
		return(1);
	}

	/* Get the struct size 
	 */
	size = Struct[sid].size;
	if (size == 0) {
		if ((size = STRUCT(s)) == 0) {
			KL_SET_ERROR(KLE_BAD_STRUCT);
			return(1);
		}
		/* We must have added this type after startup (via another namelist).
		 * So, set the size value for future reference.
		 */
		Struct[sid].size = size;
	}

	if (DEBUG(DC_STRUCT, 1)) {
		fprintf(ofp, "struct=%s, offset=%d, addr=0x%llx", 
			s, offset, addr);
	}

	if (next = addr) {
		(*Struct[sid].banner)(ofp, BANNER|SMAJOR);
	}
	while(next) {
		ptr = alloc_block(size, B_TEMP);
		kl_get_struct(K, next, size, ptr, Struct[sid].name);
		if (KL_ERROR) {
			/* XXX -- should get error from struct_error() routine 
			 */
			KL_ERROR |= KLE_BAD_STRUCT;
			return(1);
		}
		else {
			if (DEBUG(DC_GLOBAL, 1) || (flags & C_FULL)) {
				if (!firsttime) {
					(*Struct[sid].banner)(ofp, BANNER|SMAJOR);
				} 
				else {
					firsttime = 0;
				}
			}
		}
		if (Struct[sid].print) {
			(*Struct[sid].print)(next, ptr, flags, ofp);
		}
		else {
			fprintf(ofp, "%s: can't print struct\n", Struct[sid].name);
		}
		last = next;
		kl_get_kaddr(K, next+offset, &next, Struct[sid].name);
		if ((next == addr) || (next == last)) {
			break;
		}
	}
	(*Struct[sid].banner)(ofp, SMAJOR);
	free_block(ptr);
	return(0);
}

/*
 * Print the flags with correct indentation
 */
void print_flags(int *firsttime,const char *str,char *indent,FILE *ofp)
{
	if(!*firsttime)
	{
		fprintf(ofp,"%s",str);
		*firsttime = 1;
	}
	else
	{
		fprintf(ofp,"|%s",str);
	}
	if(!(*firsttime%5))
	{
		fprintf(ofp,"\n%s\t",indent);
	}
}


/*
 * avlnode_banner() -- Print out banner information for avlnode structure
 */
void
avlnode_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "         AVLNODE              FORW              BACK  "
			"          PARENT\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "======================================================"
			"================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "------------------------------------------------------"
			"----------------\n");
	}
}

/*
 * print_avlnode() -- Print out specific information in an avlnode sturct
 */
int
print_avlnode(kaddr_t avl, k_ptr_t avlp, int flags, FILE *ofp)
{
	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_avlnode: avl=0x%llx, avlp=0x%x, flags=0x%x\n",
			avl, avlp, flags);
	}

	fprintf(ofp, "%16llx  %16llx  %16llx  %16llx\n",
		avl, 
		kl_kaddr(K, avlp, "avlnode", "avl_forw"),
		kl_kaddr(K, avlp, "avlnode", "avl_back"),
		kl_kaddr(K, avlp, "avlnode", "avl_parent"));

	if (flags & C_FULL) {
		fprintf(ofp, "AVL_NEXTINO=0x%llx, AVL_BALANCE=%lld\n",
			kl_kaddr(K, avlp, "avlnode", "avl_nextino"),
			KL_INT(K, avlp, "avlnode", "avl_balance"));
		fprintf(ofp, "\n");
	}
	return(1);
}

/*
 * bhv_desc_banner()
 */
void
bhv_desc_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "             BHV             PDATA              "
			"VOBJ              NEXT\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "================================================"
			"======================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "------------------------------------------------"

			"----------------------\n");
	}
}

/*
 * print_bhv_desc() 
 */
int
print_bhv_desc(kaddr_t bhv, k_ptr_t bhvp, int flags, FILE *ofp)
{
	fprintf(ofp, "%16llx  %16llx  %16llx  %16llx\n",
		bhv, 
		kl_kaddr(K, bhvp, "bhv_desc", "bd_pdata"),
		kl_kaddr(K, bhvp, "bhv_desc", "bd_vobj"),
		kl_kaddr(K, bhvp, "bhv_desc", "bd_next"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		fprintf(ofp, "  BD_OPS=0x%llx\n", 
			kl_kaddr(K, bhvp, "bhv_desc", "bd_ops"));
		fprintf(ofp, "\n");
	}
	return(1);
}

/*
 * buf_banner()
 */
void
buf_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "             BUF      EDEV           PROC/VP    "
			"BCOUNT     FLAGS\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "================================================"
			"================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "------------------------------------------------"
			"----------------\n");
	}
}

/*
 * print_buf() 
 */
int
print_buf(kaddr_t buf, k_ptr_t bufp, int flags, FILE *ofp)
{
	fprintf(ofp, "%16llx  %8llx  %16llx  %8llu  %8llx\n",
		buf, 
		KL_UINT(K, bufp, "buf", "b_edev"),
		kl_kaddr(K, bufp, "buf", "b_obj"),
		KL_UINT(K, bufp, "buf", "b_bcount"),
		KL_INT(K, bufp, "buf", "b_flags"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		fprintf(ofp, "  B_FORW=0x%llx, B_BACK=0x%llx\n", 
			kl_kaddr(K, bufp, "buf", "b_forw"),
			kl_kaddr(K, bufp, "buf", "b_back"));
		fprintf(ofp, "  AV_FORW=0x%llx, AV_BACK=0x%llx\n", 
			kl_kaddr(K, bufp, "buf", "av_forw"),
			kl_kaddr(K, bufp, "buf", "av_back"));
		fprintf(ofp, "  B_DFORW=0x%llx, B_DBACK=0x%llx\n", 
			kl_kaddr(K, bufp, "buf", "b_dforw"),
			kl_kaddr(K, bufp, "buf", "b_dback"));
		fprintf(ofp, "  B_UN=0x%llx, B_PAGES=0x%llx\n",
			kl_kaddr(K, bufp, "buf", "b_un"),
			kl_kaddr(K, bufp, "buf", "b_pages"));
		fprintf(ofp, "  B_BLKNO=%lld\n", KL_INT(K, bufp, "buf", "b_blkno"));
		fprintf(ofp, "\n");
	}
	return(1);
}

/*
 * eframe_s_banner() -- Print out a banner around an exception frame.
 */
void
eframe_s_banner(FILE *ofp, int flags) 
{
	if (flags & SMAJOR) {
		fprintf(ofp,
			"======================================="
			"=======================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"---------------------------------------"
			"---------------------------------------\n");
	}
}

/* 
 * print_eframe_s()
 */
int
print_eframe_s(kaddr_t eframe, k_ptr_t e, int flags, FILE *ofp)
{
	if (DEBUG(DC_TRACE, 1) || DEBUG(DC_FUNCTRACE, 3)) {
		  fprintf(KL_ERRORFP, "print_eframe_s: e=0x%x, ofp=0x%x\n", e, ofp);
	}

	fprintf(ofp, "     r0/zero:0000000000000000   r1/at:%016llx   "
			"r2/v0:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_at"), 
		kl_reg(K, e, "eframe_s", "ef_v0"));
	fprintf(ofp, "       r3/v1:%016llx   r4/a0:%016llx   r5/a1:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_v1"), 
		kl_reg(K, e, "eframe_s", "ef_a0"), 
		kl_reg(K, e, "eframe_s", "ef_a1"));
	fprintf(ofp, "       r6/a2:%016llx   r7/a3:%016llx   r8/a4:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_a2"), 
		kl_reg(K, e, "eframe_s", "ef_a3"), 
		kl_reg(K, e, "eframe_s", "ef_a4"));
	fprintf(ofp, "       r9/a5:%016llx  r10/a6:%016llx  r11/a7:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_a5"), 
		kl_reg(K, e, "eframe_s", "ef_a6"), 
		kl_reg(K, e, "eframe_s", "ef_a7"));
	fprintf(ofp, "      r12/t0:%016llx  r13/t1:%016llx  r14/t2:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_t0"), 
		kl_reg(K, e, "eframe_s", "ef_t1"), 
		kl_reg(K, e, "eframe_s", "ef_t2"));
	fprintf(ofp, "      r15/t3:%016llx  r16/s0:%016llx  r17/s1:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_t3"), 
		kl_reg(K, e, "eframe_s", "ef_s0"), 
		kl_reg(K, e, "eframe_s", "ef_s1"));
	fprintf(ofp, "      r18/s2:%016llx  r19/s3:%016llx  r20/s4:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_s2"), 
		kl_reg(K, e, "eframe_s", "ef_s3"), 
		kl_reg(K, e, "eframe_s", "ef_s4"));
	fprintf(ofp, "      r21/s5:%016llx  r22/s6:%016llx  r23/s7:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_s5"), 
		kl_reg(K, e, "eframe_s", "ef_s6"), 
		kl_reg(K, e, "eframe_s", "ef_s7"));
	fprintf(ofp, "      r24/t8:%016llx  r25/t9:%016llx  r26/k0:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_t8"), 
		kl_reg(K, e, "eframe_s", "ef_t9"), 
		kl_reg(K, e, "eframe_s", "ef_k0"));
	fprintf(ofp, "      r27/k1:%016llx  r28/gp:%016llx  r29/sp:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_k1"), 
		kl_reg(K, e, "eframe_s", "ef_gp"), 
		kl_reg(K, e, "eframe_s", "ef_sp"));
	fprintf(ofp, "      r30/s8:%016llx  r31/ra:%016llx     EPC:%016llx\n",
		kl_reg(K, e, "eframe_s", "ef_fp"), 
		kl_reg(K, e, "eframe_s", "ef_ra"), 
		kl_reg(K, e, "eframe_s", "ef_epc")); 
	fprintf(ofp, "          CAUSE=%llx, SR=%llx, BADVADDR=%llx\n",
		kl_reg(K, e, "eframe_s", "ef_cause"),
		kl_reg(K, e, "eframe_s", "ef_sr"), 
		kl_reg(K, e, "eframe_s", "ef_badvaddr"));
	return(1);
}

/*
 * get_vfile() 
 * 
 *   This routine returns a pointer to a file struct containing the
 *   kernel file pointed to by addr if f_count is greater than zero or
 *   the C_ALL flag is set.
 */
k_ptr_t
get_vfile(kaddr_t addr, k_ptr_t vfbufp, int flags)
{
	k_ptr_t vfp;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_vfile: addr=0x%llx, vfbufp=0x%x, flags=0x%x\n",
			addr, vfbufp, flags);
	}

	vfp = kl_get_struct(K, addr, VFILE_SIZE(K), vfbufp, "vfile");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_VFILE;
		return ((k_ptr_t)NULL);
	}
	else {
		if ((KL_INT(K, vfbufp, "vfile", "vf_count")) &&
		    kl_kaddr(K, vfbufp, "vfile", "__vf_data__")) {
				return (vfp);
		}
		else if (flags & C_ALL) {
				return (vfp);
		}
		else {
			KL_SET_ERROR_NVAL(KLE_BAD_VFILE, addr, 2);
			return ((k_ptr_t)NULL);
		}
	}
}

/*
 * vfile_banner() -- Print out the banner information for each fp.
 */
void
vfile_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp,
			"           VFILE  RCNT              DATA                BH"
			"     FLAGS\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"=========================================================="
			"==========\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"----------------------------------------------------------"
			"----------\n");
	}
}

/*
 * print_vfile() -- Print out the data in the vfile pointer.
 */
int
print_vfile(kaddr_t addr, k_ptr_t vfilep, int flags, FILE *ofp)
{
	k_ptr_t vobjp;
	kaddr_t data;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(ofp, "print_vfile: addr=0x%llx, vfilep=0x%x, flags=0x%x\n",
			addr, vfilep, flags);
	}

	data = kl_kaddr(K, vfilep, "vfile", "__vf_data__");

	fprintf(ofp, "%16llx  %4lld  %16llx  %16llx  %8llx\n",
		addr, 
		KL_INT(K, vfilep, "vfile", "vf_count"),
		data,
		kl_kaddr(K, vfilep, "vfile", "vf_bh"),
		KL_UINT(K, vfilep, "vfile", "vf_flag"));

	if (flags & C_NEXT) {
		if (KL_UINT(K, vfilep, "vfile", "vf_flag") & FSOCKET) {
			vobjp = alloc_block(VSOCKET_SIZE(K), B_TEMP);
			kl_get_struct(K, data, VSOCKET_SIZE(K), vobjp, "vsocket");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VSOCKET;
				free_block(vobjp);
				fprintf(ofp, "\n");
				return(-1);
			}
			fprintf(ofp, "\n");
			fprintf(ofp, "  ");
			vsocket_banner(ofp, BANNER);
			fprintf(ofp, "  ");
			vsocket_banner(ofp, SMINOR);
			flags |= C_INDENT;
			print_vsocket(data, vobjp, flags, ofp);
		}
		else {
			vobjp = alloc_block(VNODE_SIZE(K), B_TEMP);
			get_vnode(data, vobjp, flags);
			if (KL_ERROR) {
				free_block(vobjp);
				fprintf(ofp, "\n");
				return (-1);
			}
			fprintf(ofp, "\n");
			fprintf(ofp, "  ");
			vnode_banner(ofp, BANNER);
			fprintf(ofp, "  ");
			vnode_banner(ofp, SMINOR);
			flags |= C_INDENT;
			print_vnode(data, vobjp, flags, ofp);
		}
		free_block(vobjp);
		fprintf(ofp, "\n");
	}
	return(0);
}

/*
 * graph_edge_s_banner() -- Print out the banner information for each 
 *                          graph_edge_s struct.
 */
void
graph_edge_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"EDGE             LABEL  NAME                      VERTEX\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"========================================================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, 
			"--------------------------------------------------------\n");
	}
}

/*
 * print_graph_edge_s() -- Print out the data in the graph_edge_s struct.
 *
 *  Note that this print routine cannot be called by the struct or walk
 *  commands. That is because it has an extra paramater (i). This was 
 *  necessary because there was no other way to determine the edge index.
 *  You have to know what the vertex is in order to determine that (which
 *  would also have required a non-standard set of parameters).
 */
int
print_graph_edge_s(int i, kaddr_t addr, k_ptr_t gep, int flags, FILE *ofp)
{
	kaddr_t labelp;
	char label[256];

	/* Get the pointer to the label string
	 */
	labelp = kl_kaddr(K, gep, "graph_edge_s", "e_label");

	/* Get the label string
	 */
	kl_get_block(K, labelp, 24, label, "graph_info_label");

	indent_it(flags, ofp);
	fprintf(ofp, "%4d  %16llx  %-24s  %6llu\n", i, labelp, label, 
			KL_UINT(K, gep, "graph_edge_s", "e_vertex"));
	return(0);
}

/*
 * graph_info_s_banner() -- Print out the banner information for each 
 *                          graph_info_s struct.
 */
void
graph_info_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"INFO            LABEL NAME                        "
				"INFO_DESC             INFO\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"===================================================="
				"==========================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, 
			"----------------------------------------------------"
				"--------------------------\n");
	}
}

/*
 * print_graph_info_s() -- Print out the data in the graph_info_s struct.
 *
 *  Note that this print routine cannot be called by the struct or walk
 *  commands. That is because it has an extra paramater (i). This was 
 *  necessary because there was no other way to determine the edge index.
 *  You have to know what the vertex is in order to determine that (which
 *  would also have required a non-standard set of parameters).
 */
int
print_graph_info_s(int i, kaddr_t addr, k_ptr_t gip, int flags, FILE *ofp)
{
	kaddr_t labelp;
	char label[256];

	/* Get the pointer to the label string
	 */
	labelp = kl_kaddr(K, gip, "graph_edge_s", "e_label");

	/* Get the label string
	 */
	kl_get_block(K, labelp, 24, label, "graph_info_label");

	indent_it(flags, ofp);
	fprintf(ofp, "%4d %16llx %-20s %16llx %16llx\n", 
		i, 
		labelp, 
		label, 
		KL_UINT(K, gip, "graph_info_s", "i_info_desc"),
		KL_UINT(K, gip, "graph_info_s", "i_info"));
	return(0);
}

/*
 * graph_vertex_s_banner() -- Print out the banner information for each 
 *                            graph_vertex_s struct.
 */
void
graph_vertex_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp,
			"HNDL            VERTEX  REFCNT  NUM_EDGE  NUM_INFO         "
				"INFO_LIST\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"==========================================================="
				"=========\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"-----------------------------------------------------------"
				"---------\n");
	}
}

/*
 * print_graph_vertex_s() -- Print out the data in the file pointer.
 */
int
print_graph_vertex_s(kaddr_t addr, k_ptr_t gp, int flags, FILE *ofp)
{
	k_ptr_t infop;
	k_uint_t hndl, num_edge, num_info;
	kaddr_t info_list, next_info;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(ofp, "print_graph_vertex_s: addr=0x%llx, "
			"gp=0x%x, flags=0x%x, ofp=0x%x\n", addr, gp, flags, ofp);
	}

	kl_is_valid_kaddr(K, addr, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		hndl = addr;
		addr = handle_to_vertex(K, hndl);
		kl_reset_error();
	}
	else {
		hndl = vertex_to_handle(K, addr);
	}

	num_edge = KL_UINT(K, gp, "graph_vertex_s", "v_num_edge");
	num_info = KL_UINT(K, gp, "graph_vertex_s", "v_num_info_LBL");
	info_list = kl_kaddr(K, gp, "graph_vertex_s", "v_info_list");

	fprintf(ofp, "%4lld  %16llx  %6lld  %8lld  %8lld  %16llx\n", 
		hndl, 
		addr, 
		KL_INT(K, gp, "graph_vertex_s", "v_refcnt"), 
		num_edge, num_info, info_list);
	
	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		fprintf(ofp, "CONNECT_POINT=%llu\n",
			kl_uint(K, gp, "graph_vertex_s", "v_info_list", (2 * K_NBPW(K))));
		fprintf(ofp, "INDEX[0]=0x%llx, INDEX[1]=0x%llx, INDEX[2]=0x%llx\n",
			kl_uint(K, gp, "graph_vertex_s", "v_info_list", (K_NBPW(K))),
			kl_uint(K, gp, "graph_vertex_s", "v_info_list", (2 * K_NBPW(K))),
			kl_uint(K, gp, "graph_vertex_s", "v_info_list", (3 * K_NBPW(K))));
	}
	
	if (flags & C_NEXT) {
		int i;
		kaddr_t labelp;
		char label[256];

		fprintf(ofp, "\n");
		next_info = info_list;

		if (num_edge) {
			infop = alloc_block(GRAPH_EDGE_S_SIZE(K), B_TEMP);

			fprintf(ofp, "EDGES:\n\n");
			graph_edge_s_banner(ofp, C_INDENT|BANNER|SMINOR);

			for (i = 0; i < num_edge; i++) {
				kl_get_graph_edge_s(K, gp, i, 0, infop, flags);
				if (KL_ERROR) {
					break;
				}
				print_graph_edge_s(i, next_info, infop, C_INDENT, ofp);
				next_info += GRAPH_EDGE_S_SIZE(K);	
			}
			free_block(infop);
			fprintf(ofp, "\n");
		}

		if (num_info) {
			fprintf(ofp, "INFOS:\n\n");
			graph_info_s_banner(ofp, C_INDENT|BANNER|SMINOR);

			infop = alloc_block(GRAPH_INFO_S_SIZE(K), B_TEMP);
			for (i = 0; i < num_info; i++) {
				kl_get_graph_info_s(K, gp, i, 0, infop, flags);
				if (KL_ERROR) {
					break;
				}
				print_graph_info_s(i, next_info, infop, C_INDENT, ofp);
				next_info += GRAPH_EDGE_S_SIZE(K);	
			}
			free_block(infop);
			fprintf(ofp, "\n");
		}
	}
	return(0);
}

/*
 * get_inode() -- Try and get a patched inode pointer.  
 * 
 *   The best we can do here is try and check the vnode pointer for
 *   validity.
 */
k_ptr_t
get_inode(kaddr_t addr, k_ptr_t ibufp, int flags)
{
	k_ptr_t ip;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_inode: addr=0x%llx, ibufp=0x%x, flags=0x%x\n",
			addr, ibufp, flags);
	}

	ip = kl_get_struct(K, addr, INODE_SIZE(K), ibufp, "inode");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_INODE;
	}
	else {
		if (addr == kl_bhvp_pdata(K, INODE_TO_BHP(ip))) {
			return (ip);
		}
		else if (flags & C_ALL) {
			return (ip);
		}
		else {
			KL_SET_ERROR_NVAL(KLE_BAD_INODE, addr, 2);
		}
	}
	return ((k_ptr_t)NULL);
}

/*
 * inode_banner() -- Print out banner information for inode structure.
 */
void
inode_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp, "           INODE    UID    GID    NUMBER  "
					"              BH               SIZE\n");
		}
		else {
			fprintf(ofp, "   INODE            UID    GID    NUMBER  "
					"              BH               SIZE\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"==========================================================="
			"==================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"-----------------------------------------------------------"
			"------------------\n");
	}
}

/*
 * print_inode() -- Print out the inode information.
 */
int
print_inode(kaddr_t addr, k_ptr_t ip, int flags, FILE *ofp)
{
	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_inode: addr=0x%llx, ip=0x%x, flags=0x%x\n",
				addr, ip, flags);
	}

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp, "  ", addr);
	fprintf(ofp, "%5lld  %5lld  %8llu  ", 
		KL_INT(K, ip, "inode", "i_uid"),
		KL_INT(K, ip, "inode", "i_gid"), 
		KL_UINT(K, ip, "inode", "i_number"));
	fprintf(ofp, "%16llx           ", kl_kaddr(K, ip, "inode", "i_bhv_desc"));
	fprintf(ofp, "%8lld\n", KL_INT(K, ip, "inode", "i_size"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "NEXT: %llx, PREVP: %llx\n",
			kl_kaddr(K, ip, "inode", "i_next"),
			kl_kaddr(K, ip, "inode", "i_prevp"));

		indent_it(flags, ofp);
		fprintf(ofp, "NLINK: %lld, GEN #: %llu, FLAGS: 0x%llx\n",
			KL_INT(K, ip, "inode", "i_nlink"),
			KL_UINT(K, ip, "inode", "i_gen"),
			KL_UINT(K, ip, "inode", "i_flags"));

		indent_it(flags, ofp);
		fprintf(ofp, "ATIME: %lld, MTIME: %lld, CTIME: %lld\n",
			KL_INT(K, ip, "inode", "i_atime"),
			KL_INT(K, ip, "inode", "i_mtime"),
			KL_INT(K, ip, "inode", "i_ctime"));

		indent_it(flags, ofp);
		fprintf(ofp, "# EXTENTS: %lld, # INDIRS: %lld\n",
			KL_INT(K, ip, "inode", "i_numextents"),
			KL_INT(K, ip, "inode", "i_numindirs"));

		indent_it(flags, ofp);
		fprintf(ofp, "EXTENT LIST: %llx, INDIRECT LIST: %llx\n",
			kl_kaddr(K, ip, "inode", "i_extents"),
			kl_kaddr(K, ip, "inode", "i_indirbytes"));

		indent_it(flags, ofp);
		fprintf(ofp, "BLOCKS: %lld, ",
			kl_kaddr(K, ip, "inode", "i_blocks"));
		fprintf(ofp, "LOCKTRIPS: %lld\n", 
			KL_INT(K, ip, "inode", "i_locktrips"));
		fprintf(ofp, "\n");
	}
	return (1);
}

/*
 * inpcb_banner() -- Print out banner information for inpcb structure.
 */
void
inpcb_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"           INPCB            SOCKET  "
				"       LOCAL ADDRESS      FOREIGN ADDRESS\n");
		}
		else {
			fprintf(ofp,
				"   INPCB                    SOCKET  "
				"       LOCAL ADDRESS      FOREIGN ADDRESS\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"==========================================="
			"==================================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------"
			"----------------------------------\n");
	}
}

/*
 * print_inpcb() -- Print out an address as an incpb structure.
 */
int
print_inpcb(kaddr_t in, k_ptr_t inp, int flags, FILE *ofp)
{
	int i, proto;
	short port;
	uint addr;
	k_uint_t tcpaddr;
	k_ptr_t tmp1, tmp2, tcpp;
	char ipaddr[256];

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_inpcb: in=0x%llx, inp=0x%x, flags=0x%x\n",
			in, inp, flags);
	}

	ipaddr[0] = 0;
	proto = flags >> 16;

	FPRINTF_KADDR(ofp, "  ", in);
	fprintf(ofp, "%16llx  ", kl_kaddr(K, inp, "inpcb", "inp_u"));

	/*
	 * Print out the local address.
	 */
	tmp1 = (k_ptr_t)((uint)inp + FIELD("inpcb", "inp_iap"));
	tmp2 = (k_ptr_t)((unsigned)tmp1 + FIELD("inaddrpair", "iap_laddr"));
	tcpaddr = KL_UINT(K, tmp2, "in_addr", "s_addr");
	get_tcpaddr(tcpaddr, KL_UINT(K, tmp1, "inaddrpair", "iap_lport"), ipaddr);
	fprintf(ofp, "%+20s ", ipaddr);

	/* Print out the foreign address.
	 */
	ipaddr[0] = 0;
	tmp2 = (k_ptr_t)((unsigned)tmp1 + FIELD("inaddrpair", "iap_faddr"));
	tcpaddr = KL_UINT(K, tmp2, "in_addr", "s_addr");
	get_tcpaddr(tcpaddr, KL_UINT(K, tmp1, "inaddrpair", "iap_fport"), ipaddr);
	fprintf(ofp, "%+20s\n", ipaddr);
	return(1);
}

/*
 * inventory_s_banner() -- Print out banner information for inventory_s 
 *                         structure.
 */
void
inventory_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
				"       INVENTORY              NEXT  CLASS  TYPE  CONTROLLER  "
				"UNIT  STATE\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, 
				"============================================================="
				"===========\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
				"-------------------------------------------------------------"
				"-----------\n");
	}
}

/*
 * print_inventory_s() -- Print out an address as an inventory_s structure.
 */
int
print_inventory_s(kaddr_t inv, k_ptr_t invp, int flags, FILE *ofp)
{
	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_inventory_s: inv=0x%llx, invp=0x%x, flags=0x%x\n",
			inv, invp, flags);
	}
	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %16llx  %5lld  %4lld  %10llu  %4llu  %5lld\n",
		inv, 
		kl_kaddr(K, invp, "inventory_s", "inv_next"),
		KL_INT(K, invp, "inventory_s", "inv_class"),
		KL_INT(K, invp, "inventory_s", "inv_type"),
		KL_UINT(K, invp, "inventory_s", "inv_controller"),
		KL_UINT(K, invp, "inventory_s", "inv_unit"),
		KL_INT(K, invp, "inventory_s", "inv_state"));
	return(1);

}

#ifdef ANON_ITHREADS
/* 
 * ithread_s_banner()
 */
void
ithread_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"         ITHREAD              NEXT              PREV  NAME\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"================================================="
			"============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------------"
			"----------------------------\n");
	}
}

/*
 * print_ithread_s() 
 *
 *   Print out selected fields from the ithread struct and check the flags
 *   options for full print status. 
 */
int
print_ithread_s(kaddr_t ithread, k_ptr_t itp, int flags, FILE *ofp)
{
	k_ptr_t kt_name;

    if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(ofp, "print_ithread_s: ithread=0x%llx, itp=0x%x, flags=0x%x, "
			"ofp=0x%x\n", ithread, itp, flags, ofp);
	}

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %16llx  %16llx  %s\n",
		ithread, 
		kl_kaddr(K, itp, "ithread_s", "it_next"),
		kl_kaddr(K, itp, "ithread_s", "it_prev"),
	    kl_kthread_name(K, itp));
	return(1);
}

/*
 * list_active_ithreads()
 */
int
list_active_ithreads(int flags, FILE *ofp)
{
	int ithread_cnt = 0, first_time = 1;
	k_ptr_t itp;
	kaddr_t ithread;

	itp = alloc_block(ITHREAD_S_SIZE(K), B_TEMP);
	kl_get_struct(K, K_ITHREADLIST(K), ITHREAD_S_SIZE(K), itp, "ithread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_ITHREAD_S;
		kl_print_error(K);
		return(1);
	}
	ithread = kl_kaddr(K, itp, "ithread_s", "it_next");
	free_block(itp);
	if (flags & C_KTHREAD) {
		kthread_banner(ofp, BANNER|SMAJOR);
	}
	else {
		ithread_s_banner(ofp, BANNER|SMAJOR);
	}
	while(ithread != K_ITHREADLIST(K)) {
		itp = kl_get_ithread_s(K, ithread, 2, flags);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_ITHREAD_S;
			kl_print_error(K);
			return(ithread_cnt);
		}
		else {
			if (first_time == TRUE) {
				first_time = FALSE;
			}
			else if (flags & (C_FULL|C_NEXT)) {
                if (flags & C_KTHREAD) {
                    kthread_banner(ofp, BANNER|SMAJOR);
                }
                else {
                    ithread_s_banner(ofp, BANNER|SMAJOR);
                }
            }
			if (flags & C_KTHREAD) {
				print_kthread(ithread, itp, flags, ofp);
			}
			else {
				print_ithread_s(ithread, itp, flags, ofp);
			}
			if (KL_ERROR) {
				kl_print_error(K);
			}
			free_block(itp);
			ithread_cnt++;
			ithread = kl_kaddr(K, itp, "ithread_s", "it_next");
		}
	}
	return(ithread_cnt);
}
#endif /* ANON_ITHREADS */

/* 
 * xthread_s_banner()
 */
void
xthread_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"         XTHREAD              NEXT              PREV  NAME\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"================================================="
			"============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------------"
			"----------------------------\n");
	}
}

/*
 * print_xthread_s() 
 *
 *   Print out selected fields from the xthread struct and check the flags
 *   options for full print status. 
 */
int
print_xthread_s(kaddr_t xthread, k_ptr_t itp, int flags, FILE *ofp)
{
	k_ptr_t kt_name;

    if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(ofp, "print_xthread_s: xthread=0x%llx, itp=0x%x, flags=0x%x, "
			"ofp=0x%x\n", xthread, itp, flags, ofp);
	}

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %16llx  %16llx  %s\n",
		xthread, 
		kl_kaddr(K, itp, "xthread_s", "xt_next"),
		kl_kaddr(K, itp, "xthread_s", "xt_prev"),
	    kl_kthread_name(K, itp));
	return(1);
}

/*
 * list_active_uthreads()
 */
int
list_active_uthreads(int flags, FILE *ofp)
{
	int i, first_time = 1, uthread_cnt = 0, kthread_cnt;
	k_ptr_t pidp, vprocp, bhvdp, procp, utp;
	kaddr_t pid, vproc, bhv, proc, kthread;

	pidp = alloc_block(PID_ENTRY_SIZE(K), B_TEMP);
	vprocp = alloc_block(VPROC_SIZE(K), B_TEMP);
	pid = K_PIDACTIVE(K);

	/* We have to step over the first entry as it is just
	 * the list head.
	 */
	kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");

	if (flags & C_KTHREAD) {
		kthread_banner(ofp, BANNER|SMAJOR);
	}
	else {
		uthread_s_banner(ofp, BANNER|SMAJOR);
	}
	do {
		kl_get_struct(K, pid, PID_ENTRY_SIZE(K), pidp, "pid_entry");
		vproc = kl_kaddr(K, pidp, "pid_entry", "pe_vproc");
		kl_get_struct(K, vproc, VPROC_SIZE(K), vprocp, "vproc");
		bhv = kl_kaddr(K, vprocp, "vproc", "vp_bhvh");
		proc = kl_bhv_pdata(K, bhv);
		if (KL_ERROR) {
			kl_print_error(K);
			kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
			continue;
		}
	
		procp = kl_get_proc(K, proc, 2, flags);
		if (KL_ERROR) {
			kl_print_error(K);
			kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
			continue;
		}
	
		kthread = kl_proc_to_kthread(K, procp, &kthread_cnt);
		free_block(procp);
	
		for (i = 0; i < kthread_cnt; i++) {
			utp = kl_get_uthread_s(K, kthread, 2, flags);
			if (KL_ERROR) {
				/* XXX - print error msg? */
				break;
			}
			if (first_time == TRUE) {
				first_time = FALSE;
			}
			else if (flags & (C_FULL|C_NEXT)) {
				if (flags & C_KTHREAD) {
					kthread_banner(ofp, BANNER|SMAJOR);
				}
				else {
					uthread_s_banner(ofp, BANNER|SMAJOR);
				}
			}
			if (flags & C_KTHREAD) {
				print_kthread(kthread, utp, flags, ofp);
			}
			else {
				print_uthread_s(kthread, utp, flags, ofp);
			}
			uthread_cnt++;
			kthread = kl_kaddr(K, utp, "uthread_s", "ut_next");
			free_block(utp);
		}
		kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
	} while (pid != K_PIDACTIVE(K));
	free_block(pidp);
	free_block(vprocp);
	return(uthread_cnt);
}

/*
 * list_active_xthreads()
 */
int
list_active_xthreads(int flags, FILE *ofp)
{
	int xthread_cnt = 0, first_time = 1;
	k_ptr_t xtp;
	kaddr_t xthread;

	xtp = alloc_block(XTHREAD_S_SIZE(K), B_TEMP);
	kl_get_struct(K, K_XTHREADLIST(K), XTHREAD_S_SIZE(K), xtp, "xthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_XTHREAD_S;
		kl_print_error(K);
		return(1);
	}
	xthread = kl_kaddr(K, xtp, "xthread_s", "xt_next");
	free_block(xtp);
	if (flags & C_KTHREAD) {
		kthread_banner(ofp, BANNER|SMAJOR);
	}
	else {
		xthread_s_banner(ofp, BANNER|SMAJOR);
	}
	while(xthread != K_XTHREADLIST(K)) {
		xtp = kl_get_xthread_s(K, xthread, 2, flags);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_XTHREAD_S;
			kl_print_error(K);
			return(xthread_cnt);
		}
		else {
			if (first_time == TRUE) {
				first_time = FALSE;
			}
			else if (flags & (C_FULL|C_NEXT)) {
				if (flags & C_KTHREAD) {
					kthread_banner(ofp, BANNER|SMAJOR);
				}
				else {
					xthread_s_banner(ofp, BANNER|SMAJOR);
				}
			}
			if (flags & C_KTHREAD) {
				print_kthread(xthread, xtp, flags, ofp);
			}
			else {
				print_xthread_s(xthread, xtp, flags, ofp);
			}
			if (KL_ERROR) {
				kl_print_error(K);
			}
			free_block(xtp);
			xthread_cnt++;
			xthread = kl_kaddr(K, xtp, "xthread_s", "xt_next");
		}
	}
	return(xthread_cnt);
}

/* 
 * kthread_banner()
 */
void
kthread_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"         KTHREAD TYPE                ID  "
				"           WCHAN NAME\n");
		} 
		else {
			fprintf(ofp, 
				"         KTHREAD TYPE                ID     "
				"WCHAN NAME\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"================================================="
			"============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------------"
			"----------------------------\n");
	}
}

/*
 * print_kthread() 
 *
 *   Print out selected fields from the kthread struct and check the flags
 *   options for full print status. Note that the buff pointer ktp really 
 *   points to a proc struct sized buffer. 
 */
int
print_kthread(kaddr_t kthread, k_ptr_t ktp, int flags, FILE *ofp)
{
    kaddr_t tp, procp;
    k_ptr_t p, cp, kt_name;
    k_ptr_t buf=NULL;

    if(!ktp || !kthread || !ofp)		/* silly check. */
        return 0;

    if (IS_UTHREAD(K, ktp))      {
        buf=alloc_block(UTHREAD_S_SIZE(K), B_TEMP);
	kl_get_struct(K, kthread, UTHREAD_S_SIZE(K), buf, "uthread_s");    
    }
#ifdef ANON_ITHREADS
    else if (IS_ITHREAD(K, ktp)) {
	buf=alloc_block(ITHREAD_S_SIZE(K), B_TEMP);
	kl_get_struct(K, kthread, ITHREAD_S_SIZE(K), buf, "ithread_s");    
    }
#endif
    else if (IS_STHREAD(K,ktp))  {
	buf=alloc_block(STHREAD_S_SIZE(K),B_TEMP);
	kl_get_struct(K, kthread, STHREAD_S_SIZE(K), buf, "sthread_s");
    }
    else if (IS_XTHREAD(K,ktp))  {
	buf=alloc_block(XTHREAD_S_SIZE(K),B_TEMP);
	kl_get_struct(K, kthread, XTHREAD_S_SIZE(K), buf, "xthread_s");    
    }

    if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(ofp, "print_kthread: kthread=0x%llx, ktp=0x%x, flags=0x%x, "
			"ofp=0x%x\n", kthread, ktp, flags, ofp);
	}

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx %4llu  %16llx  ", 
		kthread, 
		KL_UINT(K, ktp, "kthread", "k_type"),
		KL_UINT(K, ktp, "kthread", "k_id"));

	if (kl_kaddr(K, ktp, "kthread", "k_wchan")) {
		if (PTRSZ64(K)) {
			fprintf(ofp, "%16llx ", kl_kaddr(K, ktp, "kthread", "k_wchan"));
		} 
		else {
			fprintf(ofp, "%8llx ", kl_kaddr(K, ktp, "kthread", "k_wchan"));
		}
	} 
	else {
		if (PTRSZ64(K)) {
			fprintf(ofp, "0000000000000000 ");
		}
		else {
			fprintf(ofp, "00000000 ");
		}
	}
	if (buf && (kt_name = kl_kthread_name(K, buf))) {
		fprintf(ofp, "%s", kt_name);
	}
	fprintf(ofp, "\n");

	if (flags & C_FULL) {
		print_kthread_full(ktp, flags, ofp);
	}

	if (buf && (flags & C_NEXT)) {
		if (IS_STHREAD(K, ktp)) {
			fprintf(ofp, "\n");
			indent_it(flags, ofp);
			sthread_s_banner(ofp, BANNER|SMINOR);
			indent_it(flags, ofp);
			print_sthread_s(kthread, buf, flags, ofp);
		}
#ifdef ANON_ITHREADS
		else if (IS_ITHREAD(K, ktp)) {
			fprintf(ofp, "\n");
			indent_it(flags, ofp);
			ithread_s_banner(ofp, BANNER|SMINOR);
			indent_it(flags, ofp);
			print_ithread_s(kthread, buf, flags, ofp);
		}
#endif
		else if (IS_UTHREAD(K, ktp)) {
			fprintf(ofp, "\n");
			indent_it(flags, ofp);
			uthread_s_banner(ofp, BANNER|SMINOR);
			indent_it(flags, ofp);
			print_uthread_s(kthread, buf, flags, ofp);
		}
		else if (IS_XTHREAD(K, ktp)) {
			fprintf(ofp, "\n");
			indent_it(flags, ofp);
			xthread_s_banner(ofp, BANNER|SMINOR);
			indent_it(flags, ofp);
			print_xthread_s(kthread, buf, flags, ofp);
		}
		fprintf(ofp, "\n");
	}
    if(buf)
      {
	  free_block(buf);
      }
    return 1;
}

/*
 *   print_k_flags()
 *
 *   Print out the k_flags in a user-understandable format.
 */
#define NUM_K_FLAGS_PER_LINE  5            /* Number we print per line. */

/* XXX -- These were lifted from the sys/kthread.h file. They need to
 * be kept synced up.
 */
#define KT_STACK_MALLOC         0x00000001
#define KT_WMUTEX               0x00000002 /* waiting on mutex */
#define KT_LOCK                 0x00000004 /* lock for k_flags */
#define KT_WACC                 0x00000008 /*  waiting for mr(acc) */
/*UNUSED                        0x00000010 */
#define KT_WSV                  0x00000020 /* waiting on sv */
#define KT_WSVQUEUEING          0x00000040 /* about to go on an SV */
#define KT_INDIRECTQ            0x00000080 /* about to go on an SV */
#define KT_WSEMA                0x00000100 /* waiting on a sema */
#define KT_WMRLOCK              0x00000200 /* waiting on a mrlock */
#define KT_ISEMA                0x00000400 /* waiting on an interrupt sema */
#define KT_WUPD                 0x00000800 /* waiting for mr(upd)lock */
#define KT_PS                   0x00001000 /* priority scheduled */
#define KT_BIND                 0x00002000 /* eligible for binding */
#define KT_NPRMPT               0x00004000 /* do not kick scheduler */
#define KT_BASEPS               0x00008000 /* orig priority scheduled */
#define KT_NBASEPRMPT           0x00010000 /* orig non-preemptive */
#define KT_PSMR                 0x00020000 /* ps because kernel mustrun */
#define KT_HOLD                 0x00040000 /* keep on this cpu */
#define KT_PSPIN                0x00080000 /* ps because kernel pin_thread */
#define KT_INTERRUPTED          0x00100000 /* thread was interrupted */
#define KT_SLEEP                0x00200000 /* thread asleep */
#define KT_NWAKE                0x00400000 /* thread cannot be interrupted */
#define KT_LTWAIT               0x00800000 /* long term wait */
#define KT_BHVINTR              0x01000000 /* thread interrupted for BHV lock*/
#define KT_SERVER               0x02000000 /* server thread */
#define KT_NOAFF                0x04000000 /* thread has no affinity */
#define KT_TIMER                0x08000000 /* interrupt thread has own timer */

int
print_k_flags(int k_flags, int flags, FILE *ofp)
{
	int i=0;
	int unknown = 0;
	char *Pchar;

	fprintf(ofp,"K_FLAGS(0x%x)=",k_flags);

	while(k_flags)
	{
		Pchar =
			((k_flags &   KT_TIMER)       ?
			 (k_flags &= ~KT_TIMER,       "KT_TIMER")        :
			 (k_flags &   KT_NOAFF)       ?
			 (k_flags &= ~KT_NOAFF,       "KT_NOAFF")        :
			 (k_flags &   KT_SERVER)      ?
			 (k_flags &= ~KT_SERVER,      "KT_SERVER")       :
			 (k_flags &   KT_BHVINTR)     ?
			 (k_flags &= ~KT_BHVINTR,     "KT_BHVINTR")      :
			 (k_flags &   KT_LTWAIT)      ?
			 (k_flags &= ~KT_LTWAIT,      "KT_LTWAIT")       :
			 (k_flags &   KT_NWAKE)       ?
			 (k_flags &= ~KT_NWAKE,       "KT_NWAKE")        :
			 (k_flags &   KT_SLEEP)       ?
			 (k_flags &= ~KT_SLEEP,       "KT_SLEEP")        :
			 (k_flags &   KT_INTERRUPTED) ?
			 (k_flags &= ~KT_INTERRUPTED, "KT_INTERRUPTED")  :
			 (k_flags &   KT_PSPIN)       ?
			 (k_flags &= ~KT_PSPIN,       "KT_PSPIN")        :
			 (k_flags &   KT_HOLD)        ?
			 (k_flags &= ~KT_HOLD,        "KT_HOLD")         :
			 (k_flags &   KT_PSMR)        ?
			 (k_flags &= ~KT_PSMR,        "KT_PSMR")         :
			 (k_flags &   KT_NBASEPRMPT)  ?
			 (k_flags &= ~KT_NBASEPRMPT,  "KT_NBASEPRMPT")   :
			 (k_flags &   KT_BASEPS)      ?
			 (k_flags &= ~KT_BASEPS,      "KT_BASEPS")       :
			 (k_flags &   KT_NPRMPT)      ?
			 (k_flags &= ~KT_NPRMPT,      "KT_NPRMPT")       :
			 (k_flags &   KT_BIND)        ?
			 (k_flags &= ~KT_BIND,        "KT_BIND")         :
			 (k_flags &   KT_PS)          ?
			 (k_flags &= ~KT_PS,          "KT_PS")           :
			 (k_flags &   KT_WUPD)        ?
			 (k_flags &= ~KT_WUPD,        "KT_WUPD")         :
			 (k_flags &   KT_ISEMA)       ?
			 (k_flags &= ~KT_ISEMA,       "KT_ISEMA")        :
			 (k_flags &   KT_WMRLOCK)     ?
			 (k_flags &= ~KT_WMRLOCK,     "KT_WMRLOCK")      :
			 (k_flags &   KT_WSEMA)       ?
			 (k_flags &= ~KT_WSEMA,       "KT_WSEMA")        :
			 (k_flags &   KT_INDIRECTQ)   ?
                         (k_flags &= ~KT_INDIRECTQ,   "KT_INDIRECTQ")    :
			 (k_flags &   KT_WSVQUEUEING) ?
			 (k_flags &= ~KT_WSVQUEUEING, "KT_WSVQUEUEING")  :
			 (k_flags &   KT_WSV)         ?
			 (k_flags &= ~KT_WSV,         "KT_WSV")          :
			 (k_flags &   KT_WACC)        ?
			 (k_flags &= ~KT_WACC,        "KT_WACC")         :
			 (k_flags &   KT_LOCK)        ?
			 (k_flags &= ~KT_LOCK,        "KT_LOCK")         :
			 (k_flags &   KT_WMUTEX)      ?
			 (k_flags &= ~KT_WMUTEX,      "KT_WMUTEX")       :
			 (k_flags &   KT_WSEMA)       ?
			 (k_flags &= ~KT_WSEMA,       "KT_WSEMA")        :
			 (k_flags &   KT_STACK_MALLOC)?
			 (k_flags &= ~KT_STACK_MALLOC,"KT_STACK_MALLOC") :
			 (k_flags)                    ?
			 (unknown = 1,                "*UNKNOWN FLAG*")  :
			 "");
		
		if(i && i%NUM_K_FLAGS_PER_LINE == 0)
		{
			fprintf(ofp,"\n                  ");
			indent_it(flags, ofp);
		}
		
		fprintf(ofp,Pchar);
		
		if(k_flags)
                        fprintf(ofp,"|");
		i++;
		
		if(unknown)
			break;
	}
	
	fprintf(ofp,"\n");
	return 0;
}


/*
 * print_kthread_full()
 */
int
print_kthread_full(k_ptr_t ktp, int flags, FILE *ofp)
{
	fprintf(ofp, "\n");

	indent_it(flags, ofp);
	print_k_flags((int)KL_UINT(K, ktp, "kthread", "k_flags"),
		      flags,ofp);

	indent_it(flags, ofp);
	fprintf(ofp, "K_W2CHAN=0x%llx,K_STACK=0x%llx, K_STACKSIZE=%llu\n", 
		kl_kaddr(K, ktp, "kthread", "k_w2chan"),
		kl_kaddr(K, ktp, "kthread", "k_stack"),
		KL_UINT(K, ktp, "kthread", "k_stacksize"));

	indent_it(flags, ofp);
	fprintf(ofp, "K_PRTN=%lld, K_PRI=%lld, K_BASEPRI=%lld, ",
		KL_INT(K, ktp, "kthread", "k_prtn"),
		KL_INT(K, ktp, "kthread", "k_pri"),
		KL_INT(K, ktp, "kthread", "k_basepri"));
	fprintf(ofp, "K_SQSELF=%lld, K_ONRQ=%lld\n",
		KL_INT(K, ktp, "kthread", "k_sqself"),
		KL_INT(K, ktp, "kthread", "k_onrq"));

	indent_it(flags, ofp);
	fprintf(ofp, "K_SONPROC=%lld, K_BINDING=%lld, K_MUSTRUN=%lld\n",
		KL_INT(K, ktp, "kthread", "k_sonproc"),
		KL_INT(K, ktp, "kthread", "k_binding"),
		KL_INT(K, ktp, "kthread", "k_mustrun"));

	indent_it(flags, ofp);
	fprintf(ofp, "K_LASTRUN=%lld, K_CPUSET=%lld, ",
		KL_INT(K,ktp,"kthread", "k_lastrun"),
		KL_INT(K,ktp,"kthread", "k_cpuset"));
		
	fprintf(ofp, "K_EFRAME=0x%llx, K_LINK=0x%llx\n", 
		kl_kaddr(K, ktp, "kthread", "k_eframe"),
		kl_kaddr(K, ktp, "kthread", "k_link"));

	indent_it(flags, ofp);
	fprintf(ofp, "K_INHERIT=0x%llx, K_INDIRECTWAIT=0x%llx\n",
		kl_kaddr(K, ktp, "kthread", "k_inherit"),
		kl_kaddr(K, ktp, "kthread", "k_indirectwait"));

	indent_it(flags, ofp);
	fprintf(ofp, "K_RFLINK=0x%llx, K_RBLINK=0x%llx\n",
		kl_kaddr(K, ktp, "kthread", "k_rflink"),
		kl_kaddr(K, ktp, "kthread", "k_rblink"));

	indent_it(flags, ofp);
	fprintf(ofp, "K_FLINK=0x%llx, K_BLINK=0x%llx\n",
		kl_kaddr(K, ktp, "kthread", "k_flink"),
		kl_kaddr(K, ktp, "kthread", "k_blink"));
	fprintf(ofp, "\n");
	return(1);
}

/*
 * mbuf_banner() -- Print out banner information for mbuf structure.
 */
void
mbuf_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, "            MBUF              NEXT               OFF    "
			"LEN  TYPE  FLAGS\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "========================================================"
			"================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "--------------------------------------------------------"
			"----------------\n");
	}
}

/*
 * print_mbuf() -- Print out contents of mbuf structure
 */
int
print_mbuf(kaddr_t addr, k_ptr_t m, int flags, FILE *ofp)
{
	k_int_t offset;

	offset = KL_INT(K, m, "mbuf", "m_off");
	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %16llx  %16lld  %5llu  %4llu  %5llx\n", 
		addr, 
		kl_kaddr(K, m, "mbuf", "m_next"), 
		offset,
		KL_UINT(K, m, "mbuf", "m_len"),
		KL_UINT(K, m, "mbuf", "m_type"),
		KL_UINT(K, m, "mbuf", "m_flags"));

	if (flags & C_FULL) {
	
		fprintf(ofp, "\n");
			indent_it(flags, ofp);
			fprintf(ofp, "  DATA=0x%llx\n", 
				(offset ? (addr + (long long)offset) : (k_uint_t)NULL));

		if (KL_UINT(K, m, "mbuf", "m_flags") & MCL_CLUSTER) {

			/* XXX - Need to convert the following to access specific
			 * members of union. For now just brute force it.
			 */
			indent_it(flags, ofp);
			fprintf(ofp, "  FREEFUNC=0x%llx, ", 
				kl_kaddr(K, m, "mbuf", "m_u"));
			fprintf(ofp, "FARG=0x%llx\n", 
				*(kaddr_t*)(ADDR(K, m, "mbuf", "m_u") + K_NBPW(K)));
			indent_it(flags, ofp);
			fprintf(ofp, "  DUPFUNC=0x%llx, ", 
				*(kaddr_t*)(ADDR(K, m, "mbuf", "m_u") + (2 * K_NBPW(K))));
			fprintf(ofp, "DARG=0x%llx\n", 
				*(kaddr_t*)(ADDR(K, m, "mbuf", "m_u") + (3 * K_NBPW(K))));
		}
	}
	return(1);
}

/*
 * ml_info_banner() -- Print out banner information for ml_info structure.
 */
void
ml_info_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, "         ML_INFO  FLAGS              NEXT              "
			"TEXT               END\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "======================================================"
			"=======================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "------------------------------------------------------"
			"-----------------------\n");
	}
}

/* 
 * get_ml_info() -- Get a particular ml_info struct (depending on mode). 
 *					Load the contents of structure into buffer pointed to 
 *					by mp and return kernel address of the ml_info struct.
 *					The value of mode determines how value is treated.
 *                  If mode is equal to two, then value is assumed to be 
 *                  a virtual address of ml_info struct. If mode is equal 
 *                  to three, then it is assumed to be a virtual address to 
 *                  match in the symbol table. And, if the mode is equal to
 *                  four, then value is a pointer to a string to be
 *                  matched in the string table.
 */
k_ptr_t
get_ml_info(kaddr_t value, int mode, k_ptr_t mp)
{
	int mp_alloced = 0;
	k_int_t id;
	kaddr_t ml, cfg;
	k_ptr_t mlp, cfgp;

	if (!mp) {
		mp = alloc_block(ML_INFO_SIZE(K), B_TEMP);
		mp_alloced++;
	}

	if (mode == 2) {  
		/* value is virtual address of ml_info struct 
		 */
		kl_get_struct(K, value, ML_INFO_SIZE(K), mp, "ml_info");
        if (KL_ERROR) {
            if (mp_alloced) {
                free_block(mp);
            }
            return((k_ptr_t)NULL);
        }
		return(mp);
	}

	ml = K_MLINFOLIST(K);
	while (ml) {
		kl_get_struct(K, ml, ML_INFO_SIZE(K), mp, "ml_info");
		if (KL_ERROR) {
			if (mp_alloced) {
				free_block(mp);
				mp = (k_ptr_t)NULL;
			}
			return((k_ptr_t)NULL);
		}
		else if (mode == 3) {
			/* value is an addr to be matched in symbol table
			 */
			if (KL_INT(K, mp, "ml_info", "ml_nsyms") &&
					(value >= kl_kaddr(K, mp, "ml_info", "ml_text")) &&
					(value < kl_kaddr(K, mp, "ml_info", "ml_end"))) {
				return(mp);
			}
		}
#ifdef NOT
		else if (mode == 4) {
			/* value is a char pointer containing the name of a symbol
			 * to be matched in symbol table
			 */
			if (ml->mp.ml_nsyms && (find_symr((char*)value, ml))) {
				return(ml);
			}
		}
#endif
		ml = kl_kaddr(K, mp, "ml_info", "ml_next");
	}
	return(mp);
}

/*
 * print_ml_info() -- Print out contents of ml_info structure
 */
int
print_ml_info(kaddr_t addr, k_ptr_t mlp, int flags, FILE *ofp)
{
	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %5lld  %16llx  %16llx  %16llx\n", 
		addr,
		KL_INT(K, mlp, "ml_info", "flags"),
		kl_kaddr(K, mlp, "ml_info", "ml_next"),
		kl_kaddr(K, mlp, "ml_info", "ml_text"),
		kl_kaddr(K, mlp, "ml_info", "ml_end"));

	if (flags & C_FULL) {
		int i, nsyms;
		kaddr_t sym, addr;
		k_ptr_t symp;

		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "ML_OBJ=0x%llx, ", kl_kaddr(K, mlp, "ml_info", "ml_obj"));
		fprintf(ofp, "ML_SYMTAB=0x%llx, ", 
			kl_kaddr(K, mlp, "ml_info", "ml_symtab"));
		fprintf(ofp, "ML_STRINGTAB=0x%llx\n", 
			kl_kaddr(K, mlp, "ml_info", "ml_stringtab"));

		indent_it(flags, ofp);
		fprintf(ofp, "ML_DESC=0x%llx, ", 
			kl_kaddr(K, mlp, "ml_info", "ml_desc"));
		fprintf(ofp, "ML_BASE=0x%llx\n", 
			kl_kaddr(K, mlp, "ml_info", "ml_base"));

		indent_it(flags, ofp);
		nsyms = KL_INT(K, mlp, "ml_info", "ml_nsyms");
		fprintf(ofp, "ML_NSYMS=%d, ", nsyms);
		fprintf(ofp, "ML_STRTABSZ=%lld\n", 
			KL_INT(K, mlp, "ml_info", "ml_strtabsz"));
		fprintf(ofp, "\n");

		/* If there are any symbols for this module, dump their
		 * information out here. We have to check to make sure
		 * that if nsyms is > 0 that there are ml_symtab and
		 * ml_streamtab pointers (there have been cases where
		 * there wasn't).
		 */
		sym = kl_kaddr(K, mlp, "ml_info", "ml_symtab");
		if (!sym || (kl_kaddr(K, mlp, "ml_info", "ml_stringtab") == 0)) {
			return(0);
		}
		symp = alloc_block(ML_SYM_SIZE(K), B_TEMP);
		for (i = 0; i < nsyms; i++) {
			kl_get_block(K, sym, ML_SYM_SIZE(K), symp, "ml_sym");
			if (PTRSZ64(K)) {
				addr = *(uint64*)symp;
			}
			else {
				addr = *(uint*)symp;
			}
			print_ml_sym(addr, symp, mlp, ofp);
			sym += ML_SYM_SIZE(K);
		}
		free_block(symp);
	}
	return(1);
}

/*
 * print_ml_sym()
 */
int
print_ml_sym(kaddr_t addr, k_ptr_t symp, k_ptr_t mlp, FILE *ofp)
{
    int strtabsz;
    char *str, *strtab;
    kaddr_t stringtab;

    if (DEBUG(DC_GLOBAL, 3)) {
        fprintf(stderr, "addr=0x%llx, symp=0x%x, mlp=0x%x\n", addr, symp, mlp);
    }
    stringtab = kl_kaddr(K, mlp, "ml_info", "ml_stringtab");
    strtabsz = KL_INT(K, mlp, "ml_info", "ml_strtabsz");
    strtab = (char*)alloc_block(strtabsz, B_TEMP);
    kl_get_block(K, stringtab, strtabsz, strtab, "ml_sym");
    if (PTRSZ64(K)) {
        str = strtab + *(uint*)((uint)symp + 8);
    }
    else {
        str = strtab + *(uint*)((uint)symp + 4);
    }
    fprintf(ofp, "%30s  %16llx\n", str, addr);
    free_block((k_ptr_t)strtab);
	return(1);
}

/*
 * mntinfo_banner() -- Print out banner information for mntinfo structure.
 */
void
mntinfo_banner(FILE *ofp, int flags)
{
        if (flags & BANNER) {
                fprintf(ofp,
                        "                              HOST            ROOTVP"
                        "         REFCT   BASETYPE\n");
        }

        if (flags & SMAJOR) {
                fprintf(ofp,
                        "============================================="
                        "==================================\n");
        }

        if (flags & SMINOR) {
                fprintf(ofp,
                        "---------------------------------------------"
                        "----------------------------------\n");
        }

}

/*
 * get_mntinfo() -- This routine returns a pointer to an mntinfo.
 */
k_ptr_t
get_mntinfo(kaddr_t addr, k_ptr_t rbufp, int flags)
{
        k_ptr_t rp;

        if (DEBUG(DC_FUNCTRACE, 2)) {
                fprintf(KL_ERRORFP, "get_mntinfo: addr=0x%llx, rbufp=0x%x, flags=0x%x\n",
                        addr, rbufp, flags);
        }

        rp = kl_get_struct(K, addr, MNTINFO_SIZE(K), rbufp, "mntinfo");
        if (KL_ERROR) {
                KL_ERROR |= KLE_BAD_MNTINFO;
        }
        else {
                return (rp);
        }
        return ((k_ptr_t)NULL);
}

/*
 * print_mntinfo() -- Print out information inside an mntinfo.
 */
int
print_mntinfo(kaddr_t addr, k_ptr_t rp, int flags, FILE *ofp)
{
        int mntinfo_cnt = 0;
        k_ptr_t nnp, bufp;
        kaddr_t np;
        char hostname[32];
        char basetype[16];

        bcopy(K_PTR(K, rp, "mntinfo", "mi_hostname"), hostname, 32);
        bcopy(K_PTR(K, rp, "mntinfo", "mi_basetype"), basetype, 16);

        indent_it(flags, ofp);
        FPRINTF_KADDR(ofp, "  ", addr);
        fprintf(ofp, "%16s  %16llx  %10lld %10s\n",
                hostname,
                kl_kaddr(K, rp, "mntinfo", "mi_rootvp"),
                KL_UINT(K, rp, "mntinfo", "mi_refct"),
                basetype);

        if (flags & C_FULL) {
                fprintf(ofp, "\n");
                indent_it(flags, ofp);
                fprintf(ofp, "NEXT=0x%llx, PREV=0x%llx\n",
                        kl_kaddr(K, rp, "mntinfo", "mi_next"),
                        kl_kaddr(K, rp, "mntinfo", "mi_prev"));

                indent_it(flags, ofp);
                fprintf(ofp, "LOCK=0x%llx, NLMFLAG=0x%llx\n",
                        kl_kaddr(K, rp, "mntinfo", "mi_lock"),
                        KL_UINT(K, rp, "mntinfo", "mi_nlm"));

                indent_it(flags, ofp);
                fprintf(ofp, "TSIZE=%lld, STSIZE=%lld, BSIZE=%lld, TIMEO=%lld\n",
                        KL_UINT(K, rp, "mntinfo", "mi_tsize"),
                        KL_UINT(K, rp, "mntinfo", "mi_stsize"),
                        KL_UINT(K, rp, "mntinfo", "mi_bsize"),
                        KL_UINT(K, rp, "mntinfo", "mi_timeo"));

                indent_it(flags, ofp);
                fprintf(ofp, "RETRANS=%lld, MNTNO =%lld\n",
                        KL_UINT(K, rp, "mntinfo", "mi_retrans"),
                        KL_UINT(K, rp, "mntinfo", "mi_mntno"));

                indent_it(flags, ofp);
		fprintf(ofp, "ROOTFSID=0x%llx, FSIDMAPS=0x%llx, RNODE=0x%llx\n",
                	kl_kaddr(K, rp, "mntinfo", "mi_rootfsid"),
                        kl_kaddr(K, rp, "mntinfo", "mi_fsidmaps"),
                        kl_kaddr(K, rp, "mntinfo", "mi_rnodes"));

                indent_it(flags, ofp);
                fprintf(ofp, "ASYNC_REQS=0x%llx, THREADS=%lld, ASYNCCNT=%lld\n",
			kl_kaddr(K, rp, "mntinfo", "mi_async_reqs"),
                        KL_UINT(K, rp, "mntinfo", "mi_threads"),
                        KL_UINT(K, rp, "mntinfo", "mi_mi_async_count"));

        }

        mntinfo_cnt++;

        if (flags & C_NEXT) {

                np = kl_kaddr(K, rp, "mntinfo", "mi_next");
                while ( np ) {
                        bufp = alloc_block(STRUCT("mntinfo"), B_TEMP);
                        nnp = kl_get_struct(K, np, MNTINFO_SIZE(K), bufp, "mntinfo");
                        if (KL_ERROR) {
                                KL_ERROR |= KLE_BAD_MNTINFO;
                                free_block(bufp);
                                fprintf(ofp, "\n");
                                return(-1);
                        }
                        else {

                                fprintf(ofp, "\n");
                                mntinfo_banner(ofp, BANNER|SMINOR);
                                bcopy(K_PTR(K, nnp, "mntinfo", "mi_hostname"), hostname, 32);
                                bcopy(K_PTR(K, nnp, "mntinfo", "mi_basetype"), basetype, 16);
                                FPRINTF_KADDR(ofp, "  ", np);
                                fprintf(ofp, "%16s  %16llx  %10lld %10s\n",
                                        hostname,
                                        kl_kaddr(K, nnp, "mntinfo", "mi_rootvp"),
                                        KL_UINT(K, nnp, "mntinfo", "mi_refct"),
                                        basetype);

                                mntinfo_cnt++;
                        }
                        np = kl_kaddr(K, nnp, "mntinfo", "mi_next");
                        free_block(bufp);
                }
                fprintf(ofp, "\n");
        }

        return(mntinfo_cnt);
}

/*
 * mrlock_s_banner() -- Print out banner information for mrlock_s structure.
 */
void
mrlock_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"          MRLOCK             LBITS  QCOUNT  QFLAGS  "
			"           QBITS\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, 
			"===================================================="
			"================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, 
			"----------------------------------------------------"
			"----------------\n");
	}
}

/* Some #define's "borrowed" from sys/sema_private.h
 */

/* LBITS
 */
#define MR_ACCINC   	0x00000001
#define MR_ACC      	0x00003fff  /* # of current access locks */
#define MR_ACCMAX   	0x00002000  /* no more accesses beyond here */
#define MR_UPD      	0x00004000  /* have update lock */
#define MR_BARRIER  	0x00008000  /* there are access barriers */
#define MR_WAITINC  	0x00010000  /* increment waiters by this */
#define MR_WAIT     	0x3fff0000  /* waiters of any kind */
#define MR_WAITSHFT 	16

/* QBITS
 */
#define MR_QLOCK        0x0001
#define MR_V            0x0002
#define MR_BEHAVIOR     0x0004
#define MR_DBLTRIPPABLE 0x0008

/* XXX - This define is here only so we can look at older dumps
 */
#define OLD_MRI

/*
 * mri_to_mria_kt_p()
 */
kaddr_t
mri_to_mria_kt_p(kaddr_t mri, k_ptr_t mrip)
{
	kaddr_t mria_kt, kthread;

#ifdef OLD_MRI
	if (KL_INT(K, mrip, "mri_s", "mri_offset")) {
		mria_kt = mri - KL_INT(K, mrip, "mri_s", "mri_offset");
	}
	else {
#endif
	mria_kt = mri -
		(KL_INT(K, mrip, "mri_s", "mri_index") * STRUCT("mri_s")) - K_NBPW(K);
#ifdef OLD_MRI
	}
#endif
	kl_get_kaddr(K, mria_kt, (k_ptr_t)&kthread, "kthread");
	return(kthread);
}

#define PRINT_COMMA(bcount) \
		if (bcount) { \
			fprintf(ofp, ", "); \
		}

/*
 * print_mrlock_s() -- Print the contents of a mrlock structure.
 */
int
print_mrlock_s(kaddr_t addr, k_ptr_t mrlockp, int flags, FILE *ofp)
{
	int i;
	k_uint_t lbits, qbits;
	short ishorttmp;
	k_ptr_t unp;

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  ", addr);
	lbits = KL_UINT(K, mrlockp, "mrlock_s", "mr_lbits");
	fprintf(ofp, "%16llx  ", lbits);
	unp = (k_ptr_t)((uint)mrlockp + FIELD("mrlock_s", "mr_un"));

    /* Because the mrlock_s struct contains a union, and that union 
     * contains a struct that does not have a struct type (it is defined
     * in-line), it is necessary to get the qcount, and qflags values
     * by brute force.
     */
    ishorttmp = *(short *)(unp);
    fprintf(ofp, "%6hd  ", ishorttmp);

    ishorttmp = *(short *)((uint_t)unp + 2);
    fprintf(ofp, "%6hd  ", ishorttmp);

	qbits = KL_UINT(K, mrlockp, "mrlock_s", "mr_un");

	fprintf(ofp, "%16llx\n", KL_UINT(K, mrlockp, "mrlock_s", "mr_un"));

	if (flags & C_FULL) {

		int bcount = 0, k_flags;
		kaddr_t waiters, holders, pg_pq;
		kaddr_t firstkt, nextkt, kt;
		kaddr_t firstmri, nextmri, mri;
		k_ptr_t ktp, mrip = 0, kt_name;

		fprintf(ofp, "\n");

		fprintf(ofp, "LBITS:\n");
		fprintf(ofp, "  ");
		if (lbits & MR_ACCINC) {
			fprintf(ofp, "MR_ACCINC");
			bcount++;
		}
		if (lbits & MR_ACC) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_ACC=%lld", (lbits & MR_ACC));
			bcount++;
		}
		if (lbits & MR_ACCMAX) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_ACCMAX");
			bcount++;
		}
		if (lbits & MR_UPD) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_UPD");
			bcount++;
		}
		if (lbits & MR_BARRIER) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_BARRIER");
			bcount++;
		}
		if (lbits & MR_WAITINC) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_WAITINC");
			bcount++;
		}
		if (lbits & MR_WAIT) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "WAITERS=%lld", (lbits & MR_WAIT) >> MR_WAITSHFT);
			bcount++;
		}
		if (bcount) {
			fprintf(ofp, "\n");
		}
		fprintf(ofp, "\n");

		bcount = 0;
		fprintf(ofp, "QBITS:\n");
		fprintf(ofp, "  ");
		if (qbits & MR_QLOCK) {
			fprintf(ofp, "MR_QLOCK");
			bcount++;
		}
		if (qbits & MR_V) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_V");
			bcount++;
		}
		if (qbits & MR_BEHAVIOR) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_BEHAVIOR");
			bcount++;
		}
		if (qbits & MR_DBLTRIPPABLE) {
			PRINT_COMMA(bcount);
			fprintf(ofp, "MR_DBLTRIPPABLE");
			bcount++;
		}
		if (bcount) {
			fprintf(ofp, "\n");
		}
		fprintf(ofp, "\n");

		indent_it(flags, ofp);
    	fprintf(ofp, "WAITERS:\n");
		waiters = (kaddr_t)(addr + FIELD("mrlock_s", "mr_waiters"));
    	for (i = 0; i < NSCHEDCLASS; i++) {
        	fprintf(ofp, "  CLASS %2d: ", -i);
			pg_pq = (kaddr_t)(waiters + (i * K_NBPW(K)));
			kl_get_kaddr(K, pg_pq, (k_ptr_t)&firstkt, "kthread");
			if (flags & C_NEXT) {
				if (kt = firstkt) {
					fprintf(ofp, "\n\n");
					fprintf(ofp, "           KTHREAD TYPE MRQPRI BASEPRI "
						"PRI FLAG NAME\n");
					fprintf(ofp, "  -------------------------------------"
					             "-----------------------------\n");
				}
				while (kt) {
					ktp = kl_get_kthread(K, kt, 0);
					if (!ktp) {
						break;
					}

					/* Print out the kthread information
					 */
					fprintf(ofp, "  %16llx %4llu ", 
						kt, KL_UINT(K, ktp, "kthread", "k_type"));

					fprintf(ofp, "%6lld %7lld %3lld ",
						KL_INT(K, ktp, "kthread", "k_mrqpri"),
						KL_INT(K, ktp, "kthread", "k_basepri"),
						KL_INT(K, ktp, "kthread", "k_pri"));

					k_flags = KL_UINT(K, ktp, "kthread", "k_flags");
					if (k_flags & KT_WACC) {
						fprintf(ofp, "WACC ");
					}
					else if (k_flags & KT_WUPD) {
						fprintf(ofp, "WUPD ");
					}
					else {
						fprintf(ofp, "     ");
					}

					if (kt_name = kl_kthread_name(K, ktp)) {
						fprintf(ofp, "%s", kt_name);
					}
					fprintf(ofp, "\n");

					kt = kl_kaddr(K, ktp, "kthread", "k_flink");
					free_block(ktp);
					if (kt == firstkt) {
						break;
					}
				}
				if (firstkt) {
					fprintf(ofp, "  -------------------------------------"
					             "------------------------\n");
				}
			}
			else {
				fprintf(ofp, "0x%llx", firstkt);
			}
        	fprintf(ofp, "\n");
    	}

		fprintf(ofp, "\n");
		indent_it(flags, ofp);
    	fprintf(ofp, "HOLDERS:\n");
		holders = (kaddr_t)(addr + FIELD("mrlock_s", "mr_holders"));
    	for (i = 0; i < NSCHEDCLASS; i++) {
        	fprintf(ofp, "  CLASS %2d: ", -i);
			pg_pq = (kaddr_t)(holders + (i * K_NBPW(K)));
			kl_get_kaddr(K, pg_pq, (k_ptr_t)&firstmri, "kthread");
			if (flags & C_NEXT) {
				if (mri = firstmri) {
					fprintf(ofp, "\n\n");
					fprintf(ofp, "               MRI PRI COUNT           "
						"KTHREAD BASEPRI PRI NAME\n");
					fprintf(ofp, "  --------------------------  ---------"
						"----------------------------------------\n");

					/* Allocate a block for the mri_s struct
					 */
					mrip = alloc_block(STRUCT("mri_s"), B_TEMP);
				}
				while (mri) {
					kl_get_struct(K, mri, STRUCT("mri_s"), mrip, "mri_s");
					if (KL_ERROR) {
						break;
					}

					/* Print out the mri_s information
					 */
					fprintf(ofp, "  %16llx %3lld %5lld  ", 
						mri, KL_INT(K, mrip, "mri_s", "mri_pri"),
						KL_UINT(K, mrip, "mri_s", "mri_count"));

					kt = mri_to_mria_kt_p(mri, mrip);
					fprintf(ofp, "%16llx ", kt);
					ktp = kl_get_kthread(K, kt, 0);
					if(!ktp || KL_ERROR)
					  {
					    fprintf(ofp, "\n");
					    kl_reset_error();
					    break;
					  }

					fprintf(ofp, "%7lld %3lld ",
						KL_INT(K, ktp, "kthread", "k_basepri"),
						KL_INT(K, ktp, "kthread", "k_pri"));
					if (kt_name = kl_kthread_name(K, ktp)) {
						fprintf(ofp, "%s", kt_name);
					}
					if (ktp) {
						free_block(ktp);
					}
					fprintf(ofp, "\n");

					mri = kl_kaddr(K, mrip, "mri_s", "mri_flink");
					if (mri == firstmri) {
						break;
					}
				}
				if (mrip) {
					free_block(mrip);
					mrip = 0;
				}
				if (firstmri) {
					fprintf(ofp, "  --------------------------  ---------"
						"----------------------------------------\n");
				}
			}
			else {
				fprintf(ofp, "0x%llx", firstmri);
			}
        	fprintf(ofp, "\n");
	}
	}
	return(1);
}

/*
 * nodepda_s_banner() -- Print out a nodepda_s banner
 */
void
nodepda_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "NODE           NODEPDA  \n");
	}

	if (flags & SMAJOR) {
		fprintf (ofp, "================================================"
			"==============================\n");
	}

	if (flags & SMINOR) {
		fprintf (ofp, "------------------------------------------------"
			"------------------------------\n");
	}
}

/*
 * print_nodepda_s() -- Print out the node PDA information
 */
int
print_nodepda_s(kaddr_t nodepda, k_ptr_t npdap, int flags, FILE *ofp)
{
	int nodeid;
	k_ptr_t npgdp;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(ofp, "print_nodepda_s: nodepda=0x%llx, npdap=0x%x, "
			"flags=0x%x\n", nodepda, npdap, flags);
	}

	nodeid = kl_get_nodeid(K, nodepda);
	if (KL_ERROR) {
		kl_print_error(K);
		return(0);
	}

	fprintf(ofp, "%4d  %16llx\n", nodeid, nodepda);

	if (flags & C_FULL) {
		fprintf(ofp, "\n");

		fprintf(ofp, "NODE_PG_DATA:\n\n");
		npgdp = (k_ptr_t)((unsigned)npdap + FIELD("nodepda_s", "node_pg_data"));

		fprintf(ofp, "PG_FREELIST=0x%llx\n", 
				kl_kaddr(K, npgdp, "pg_data", "pg_freelst"));

		fprintf(ofp, "NODE_FREEMEM=%lld, NODE_FUTURE_FREEMEM=%lld\n",
			KL_INT(K, npgdp, "pg_data", "node_freemem"),
			KL_INT(K, npgdp, "pg_data", "node_future_freemem"));

		fprintf(ofp, "NODE_EMPTYMEM=%lld, NODE_TOTAL_MEM=%lld\n",
			KL_INT(K, npgdp, "pg_data", "node_emptymem"),
			KL_INT(K, npgdp, "pg_data", "node_total_mem"));
		fprintf(ofp, "\n");
	}
	return(1);
}

/*
 * pda_s_banner() -- Print out a pda_s banner for 32 and 64 bit machines.
 */
void
pda_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "CPU               PDA  STKFLG             STACK  "
			"      CURKTHREAD       LTICKS\n");
	}

	if (flags & SMAJOR) {
		fprintf (ofp, "================================================"
			"==============================\n");
	}

	if (flags & SMINOR) {
		fprintf (ofp, "------------------------------------------------"
			"------------------------------\n");
	}
}

/*
 * print_pda_s() -- Print out the internal PDA information.
 */
int
print_pda_s(kaddr_t pda, k_ptr_t pdap, int flags, FILE *ofp)
{
	int stkflg;
	kaddr_t curkthread;
	k_ptr_t k;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_pda_s: pda=0x%llx, pdap=0x%x, flags=0x%x\n",
			pda, pdap, flags);
	}

	stkflg = KL_UINT(K, pdap, "pda_s", "p_kstackflag");
	curkthread = kl_kaddr(K, pdap, "pda_s", "p_curkthread");
	fprintf(ofp, "%3lld  ", KL_INT(K, pdap, "pda_s", "p_cpuid"));
	fprintf(ofp, "%16llx  ", pda);
	fprintf(ofp, "%6d  ", stkflg);

	switch (stkflg) {
		case 0 : /* process user mode stack */
			fprintf(ofp, "            USER  ");
			break;

		case 1 : /* process kthread stack */
			k = kl_get_kthread(K, curkthread, flags);
			if (!k) {
				fprintf(ofp, "          ERROR!  ");
			} 
			else {
				fprintf(ofp, "%16llx  ", kl_kaddr(K, k, "kthread", "k_stack"));
				free_block(k);
			}
			break;

		case 2 : /* interrupt stack */
			fprintf(ofp, "%16llx  ", kl_kaddr(K, pdap, "pda_s", "p_intstack"));
			break;

		case 3 : /* boot stack (idle) */
			fprintf(ofp, "%16llx  ", kl_kaddr(K, pdap, "pda_s", "p_bootstack"));
			break;
	}

	fprintf(ofp, "%16llx   ", kl_kaddr(K, pdap, "pda_s", "p_curkthread"));
	fprintf(ofp, "%10lld\n", KL_INT(K, pdap, "pda_s", "p_lticks"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		fprintf(ofp, "    P_CURLOCK=0x%llx, P_LASTLOCK=0x%llx\n",
			kl_kaddr(K, pdap, "pda_s", "p_curlock"), 
			kl_kaddr(K, pdap, "pda_s", "p_lastlock"));
		fprintf(ofp, "    P_RUNRUN=%llu, P_FLAGS=0x%llx\n",
			KL_UINT(K, pdap, "pda_s", "p_runrun"), 
			KL_INT(K, pdap, "pda_s", "p_flags"));
		fprintf(ofp, "    P_INTSTACK=0x%llx, P_INTLASTFRAME=0x%llx\n",
			kl_kaddr(K, pdap, "pda_s", "p_intstack"), 
			kl_kaddr(K, pdap, "pda_s", "p_intlastframe"));
		fprintf(ofp, "    P_BOOTSTACK=0x%llx, P_BOOTLASTFRAME=0x%llx\n",
			kl_kaddr(K, pdap, "pda_s", "p_bootstack"), 
			kl_kaddr(K, pdap, "pda_s", "p_bootlastframe"));
		fprintf(ofp, "    P_NESTED_INTR=%llu, P_IDLESTACKDEPTH=%llu\n",
			KL_UINT(K, pdap, "pda_s", "p_nested_intr"), 
			KL_UINT(K, pdap, "pda_s", "p_idlstkdepth"));
		fprintf(ofp, "    P_SWITCHING=%llu, P_IDLETKN=%lld\n", 
			KL_UINT(K, pdap, "pda_s", "p_switching"), 
			KL_INT(K, pdap, "pda_s", "p_idletkn"));
		fprintf(ofp, "    P_PDALO=0x%llx\n",
			kl_get_pgi(K, K_PTR(K, pdap, "pda_s", "p_pdalo"))); 
	}
	return(1);
}

/*
 * pde_banner() -- Print out banner information for pde structure.
 */
void
pde_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf (ofp, "             PDE               PGI       PFN   N  M  "
			"VR  G  CC  D  SV  NR  EOP\n");
	}

	if (flags & SMAJOR) {
		fprintf (ofp, "====================================================="
			"=========================\n");
	}

	if (flags & SMINOR) {
		fprintf (ofp, "-----------------------------------------------------"
			"-------------------------\n");
	}
}

/*
 * print_pde() -- Print out the information in a pde_t structure.
 */
int
print_pde(kaddr_t pde, k_ptr_t pp, FILE *ofp)
{
	k_uint_t pgi, value;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_pde: pde=0x%llx, pp=0x%x\n", pde, pp);
	}

	pgi = kl_get_pgi(K, pp);

	if (DEBUG(DC_PAGE, 1)) {
		fprintf(ofp, "print_pde: pgi=0x%llx\n", pgi);
	}

	fprintf(ofp, "%16llx  %16llx  %8d   ", pde, pgi, kl_pte_to_pfn(K, pp));

	if (base_value(pp, "pte", "pte_n", &value) == -1) {
		fprintf(ofp, "-  ");
	}
	else {
		fprintf(ofp, "%lld  ", value);
	}

	if (base_value(pp, "pte", "pte_m", &value) == -1) {
		fprintf(ofp, "-   ");
	}
	else {
		fprintf(ofp, "%lld   ", value);
	}

	if (base_value(pp, "pte", "pte_vr", &value) == -1) {
		fprintf(ofp, "-  ");
	}
	else {
		fprintf(ofp, "%lld  ", value);
	}

	if (base_value(pp, "pte", "pte_g", &value) == -1) {
		fprintf(ofp, "-   ");
	}
	else {
		fprintf(ofp, "%lld   ", value);
	}

	if (base_value(pp, "pte", "pte_cc", &value) == -1) {
		fprintf(ofp, "-  ");
	}
	else {
		fprintf(ofp, "%lld  ", value);
	}

	if (base_value(pp, "pte", "pte_dirty", &value) == -1) {
		fprintf(ofp, "-   ");
	}
	else {
		fprintf(ofp, "%lld   ", value);
	}

	if (base_value(pp, "pte", "pte_sv", &value) == -1) {
		fprintf(ofp, "-   ");
	}
	else {
		fprintf(ofp, "%lld   ", value);
	}

	if (base_value(pp, "pte", "pte_nr", &value) == -1) {
		fprintf(ofp, "-    ");
	}
	else {
		fprintf(ofp, "%lld    ", value);
	}

	if (base_value(pp, "pte", "pte_eop", &value) == -1) {
		fprintf(ofp, "-\n");
	}
	else {
		fprintf(ofp, "%lld\n", value);
	}
	return(1);
}

/*
 * pfdat_banner() -- Print out banner information for pfdat structure
 */
void
pfdat_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, 
			"           PFDAT USE    FLAGS  PGNO           VP/TAG"
			"             HASH       PFN\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, 
			"========================================================="
			"======================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"---------------------------------------------------------"
			"----------------------\n");
	}
}

void decode_pfdat_flags(k_ptr_t pfd,FILE *ofp)
{
	int vce_avoidance = IS_FIELD("pfdat","pf_vcolor");
	int i,firsttime;
	k_uint_t pf_flags,pf_vcolor;
	const char **Ptable;
	const char    *tab_flags[] = {
		"Q",            /* 0x000001 */
		"BAD",          /* 0x000002 */
		"HASH",         /* 0x000004 */
		"DONE",         /* 0x000008 */
		"SWAP",         /* 000x0010 */
		"WAIT",         /* 0x000020 */
		"DIRTY",        /* 0x000040 */
		"DQ",           /* 0x000080 */
		"ANON",         /* 0x000100 */
		"SQ",           /* 0x000200 */
		"CW",           /* 0x000400 */
		"DUMP",         /* 0x000800 */
		"HOLE",         /* 0x001000 */
		"CSTALE",       /* 0x002000 */
		"LPGINDEX1",    /* 0x004000 */
		"LPGINDEX2",    /* 0x008000 */
		"LPGINDEX3",    /* 0x010000 */
		"LPGPOISON",    /* 0x020000 */
		"SANON",        /* 0x040000 */
		"RECYCLE",      /* 0x080000 */
		"ECCSTALE",     /* 0x100000 */
		"REPLICA",      /* 0x200000 */
		0
	};
	const char *tab_no_vceavoidflags[] = {
		"POISONED",     /* 0x400000 */
		"MIGRATING",    /* 0x800000 */
		"ERROR",        /* 0x1000000*/
		"HWBAD",        /* 0x2000000*/
		"XP",           /* 0x4000000*/
		"LOCK",         /* 0x8000000*/
		"PROXY",        /* 0x10000000*/
		"BULKDATA",     /* 0x20000000*/
		0
	};
	const char *tab_vceavoidflags[] = {
		"MULTICOLOR",   /* 0x400000 */
		"MIGRATING",    /* 0x800000 */
		0
	};

	if (base_value(pfd, "pfdat", "pf_flags",&pf_flags)) {
		return;
	}

	if(pf_flags)
	{
		fprintf(ofp,"  PF_FLAGS : ");
	}
	else
	{
		return;
	}
	for(i=0,firsttime=0;tab_flags[i];i++)
	{
		if(pf_flags & (1 << i))
		{
			pf_flags &= ~(1 << i);
			print_flags(&firsttime,tab_flags[i],"  ",ofp);
		}
	}
	if(vce_avoidance)
	{
		Ptable = tab_vceavoidflags;
	} 
	else
	{
		Ptable = tab_no_vceavoidflags;
	}
	for(i=0;Ptable[i];i++)
	{
		if(pf_flags & (0x400000 << i))
		{
			pf_flags &= ~(0x400000 << i);
			print_flags(&firsttime,Ptable[i],"  ",ofp);
		}
	}
	if(pf_flags)
	{
		fprintf(ofp,"|Unknown(%lld)",pf_flags);
	}
	if (base_value(pfd, "pfdat", "pf_vcolor",&pf_vcolor)) {
		fprintf(ofp,"\n");
		return;
	}
#define PE_COLOR_SHIFT 1	
#define PE_RAWWAIT     1
	(pf_vcolor >> PE_COLOR_SHIFT) ? 
		fprintf(ofp,"  VCOLOR : %llx",pf_vcolor >> PE_COLOR_SHIFT) : 1;
	(pf_vcolor & PE_RAWWAIT) ? 
		fprintf(ofp,"  RAWWAIT : %lld",pf_vcolor & PE_RAWWAIT) : 1;
	fprintf(ofp,"\n");
}

/*
 * print_pfdat() -- Print page frame data information.
 */
int
print_pfdat(kaddr_t pf, k_ptr_t pfp, int flags, FILE *ofp)
{
	fprintf(ofp, "%16llx %3d %8llx %5llu %16llx %16llx %9d\n",
		pf,
		(short)KL_INT(K, pfp, "pfdat", "pf_use"),
		KL_UINT(K, pfp, "pfdat", "pf_flags"),
		KL_UINT(K, pfp, "pfdat", "pf_pageno"),
		kl_kaddr(K, pfp, "pfdat", "p_un"),
		kl_kaddr(K, pfp, "pfdat", "pf_hchain"),
		kl_pfdattopfn(pf));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		decode_pfdat_flags(pfp,ofp);
		fprintf(ofp, "  PDEP1 : 0x%llx, PDEP2 : 0x%llx\n",
			kl_kaddr(K, pfp, "pfdat", "pf_pdep1"),
			kl_kaddr(K, pfp, "pfdat", "p_rmapun"));
		fprintf(ofp, "  NEXT : 0x%llx, PREV : 0x%llx,",
			kl_kaddr(K, pfp, "pfdat", "pf_next"),
			kl_kaddr(K, pfp, "pfdat", "pf_prev"));
		fprintf(ofp, " RAWCNT : 0x%x\n",
			(short)KL_UINT(K, pfp, "pfdat", "pf_rawcnt"));
	}
	return(1);
}

/*
 * pid_entry_banner() -- Print out banner information for pid_entry structure
 */
void
pid_entry_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, "       PID_ENTRY       PID  BUSY             "
			"VPROC              NEXT\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "==============================================="
			"=====================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "-----------------------------------------------"
			"---------------------\n");
	}
}

/*
 * print_pid_entry() -- Print page frame data information.
 */
int
print_pid_entry(kaddr_t pid, k_ptr_t pidp, int flags, FILE *ofp)
{
	fprintf(ofp, "%16llx  %8lld  %4lld  %16llx  %16llx\n",
		pid,
		KL_INT(K, pidp, "pid_entry", "pe_pid"),
		KL_UINT(K, pidp, "pid_entry", "pe_ubusy"), 
		kl_kaddr(K, pidp, "pid_entry", "pe_vproc"),
		kl_kaddr(K, pidp, "pid_entry", "pe_next"));

	if (flags & C_NEXT) {
		kaddr_t vproc;
		k_ptr_t vprocp;

		fprintf(ofp, "\n");
		if (!KL_UINT(K, pidp, "pid_entry", "pe_ubusy")) {
			fprintf(ofp, "0x%llx: pid_entry on freelist\n", pid);
			fprintf(ofp, "\n");
			return(0);
		}
		else {
			vproc = kl_kaddr(K, pidp, "pid_entry", "pe_vproc");
			vprocp = alloc_block(VPROC_SIZE(K), B_TEMP);
			kl_get_struct(K, vproc, VPROC_SIZE(K), vprocp, "vproc");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VPROC;
				return(0);
			}
			fprintf(ofp, "  ");
			vproc_banner(ofp, BANNER);
			fprintf(ofp, "  ");
			vproc_banner(ofp, SMINOR);
			flags |= C_INDENT;
			print_vproc(vproc, vprocp, flags, ofp);
			free_block(vprocp);
			fprintf(ofp, "\n");
		}
	}
	return(1);
}

/*
 * pmap_banner()
 */
void
pmap_banner(FILE *ofp, int flags)
{
    if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf (ofp, "            PMAP           PREGION  TYPE  "
				"FLAGS  SCOUNT               PTR\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf (ofp, "=========================================="
				"===============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf (ofp, "------------------------------------------"
				"-------------------------------\n");
	}
}

/*
 * print_pmap() -- Print out the information in a pmap_t struct.
 */
int
print_pmap(kaddr_t p, k_ptr_t pmp, int flags, FILE *ofp)
{
	fprintf(ofp, "%16llx  %16llx   %3llu   %4llx      %2lld  %16llx\n", 
		p,
		kl_kaddr(K, pmp, "pmap", "pmap_preg"),
		KL_UINT(K, pmp, "pmap", "pmap_type"),
		KL_UINT(K, pmp, "pmap", "pmap_flags"),
		KL_INT(K, pmp, "pmap", "pmap_scount"),
		kl_kaddr(K, pmp, "pmap", "pmap_ptr"));
	return(1);
}

/*
 * pregion_banner() -- Print out banner information for pregion structure.
 */
void
pregion_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"         PREGION            REGION  PGLEN  "
				" VALID  TYPE    FLAGS\n");
		}
		else {
			fprintf(ofp,
				" PREGION                    REGION  PGLEN  "
				" VALID  TYPE    FLAGS\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"====================================="
			"===========================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"-------------------------------------"
			"---------------------------\n");
	}
}

/*
  Taken from region.h. Only used by pregion_type... So we define it here.
 */
#define PT_UNUSED	0x00		/* Unused region.		*/
#define PT_STACK	0x03		/* Stack region.		*/
#define PT_SHMEM	0x04		/* Shared memory region.	*/
#define PT_MEM		0x05		/* Generic memory region.	*/
/* Space was Double mapped memory.*/
#define PT_GR		0x08		/* Graphics region		*/
#define PT_MAPFILE	0x09		/* Memory mapped file region	*/
#define PT_PRDA		0x0a		/* PRDA region			*/

const char *pregion_type(int type)
{
  return ((type == PT_MAPFILE) ? "MAPFILE" : 
	  (type == PT_STACK)   ? "STACK  " :
	  (type == PT_MEM)     ? "GEN MEM" :
	  (type == PT_SHMEM)   ? "SH MEM " :
	  (type == PT_GR)      ? "GRAPHIC" :
	  (type == PT_PRDA)    ? "PRDA   " :
	  (type == PT_UNUSED)  ? "UNUSED " :
	  "UNKNOWN");
}

void decode_pregion_flags(k_ptr_t prp,FILE *ofp)
{
	int p_flags = KL_UINT(K, prp, "pregion", "p_flags");
	int i,firsttime;
	const char *tab_flags[] = {
		"PF_DUP",        /* 0x1    Dup on fork.                 */
		"PF_NOPTRES",    /* 0x2    No pmap reservations made    */
		"PF_NOSHARE",    /* 0x4    Do not share on sproc        */
		"PF_SHARED",     /* 0x8    It is possible that this pregion's
				  *        pmap is shared with someone  */
		"PF_AUDITR",     /* 0x10   Read audit                   */
		"PF_AUDITW",     /* 0x20   Write audit                  */
		"PF_FAULT",      /* 0x40   Vfault logging enabled       */
		"PF_TSAVE",      /* 0x80   if a tsave'd  pregion        */
		"PF_PRIVATE",    /* 0x100  pregion is on private list   */
		"PF_PRIMARY",    /* 0x200  pregion is 'primary' exec'd object
				  *                                     */
		"PF_TXTPVT",     /* 0x400  privatized text pregion      */
		"PF_LCKNPROG",   /* 0x800  pregion lockdown in progress */
		0
	};
	if(p_flags)
	{
		fprintf(ofp,"\n  P_FLAGS : ");
	}
	else
	{
		return;
	}
	for(i=0,firsttime=0;tab_flags[i];i++)
	{
		if(p_flags & (1<<i))
		{
			p_flags &= ~(1<<i);
			print_flags(&firsttime,tab_flags[i],"  ",ofp);
		}
	}
	fprintf(ofp,"\n");
}

/*
 * print_pregion() -- Print out specific information in a pregion structure.
 */
int
print_pregion(kaddr_t preg, k_ptr_t prp, int flags, FILE *ofp)
{
	k_ptr_t Pattr,Pptr,rp;
	int firsttime;
	kaddr_t KPattr;
	k_uint_t Itmp,Itmp1;
	const char prot[] = "rwx";

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, 
			"print_pregion: preg=0x%llx, prp=0x%x, flags=0x%x\n",
			preg, prp, flags);
	}

	FPRINTF_KADDR(ofp, "  ", preg);
	fprintf(ofp, "%16llx  ", kl_kaddr(K, prp, "pregion", "p_reg"));
	fprintf(ofp, "%5lld   %5lld  %s %5llx\n", 
		KL_INT(K, prp, "pregion", "p_pglen"),
		KL_INT(K, prp, "pregion", "p_nvalid"),
		pregion_type(KL_UINT(K, prp, "pregion", "p_type")),
		KL_UINT(K, prp, "pregion", "p_flags"));

	if(flags & C_FULL)
	{
		decode_pregion_flags(prp,ofp);
	}


	if (flags & C_FULL && 
	    (Pattr = alloc_block(kl_struct_len(K,"pageattr"),B_TEMP))) {
		

		/*
		 * There can be more than one attribute for a pregion.
		 */
		firsttime = 0;
		
		KPattr = preg + kl_member_offset(K,"pregion","p_attrs");
		while(KPattr && kl_get_struct(K,KPattr,
					      kl_struct_len(K,"pageattr"),
					      Pattr,"p_attrs"))
		{

			if(!firsttime)
			{
				fprintf(ofp, 
					"        START         END "
					"PROT CC UC WATCHPT LOCKCNT\n");
			}
			fprintf(ofp,
				"  %11llx %11llx",
				kl_kaddr(K, Pattr, "pageattr", "attr_start"),
				kl_kaddr(K, Pattr, "pageattr", "attr_end"));
			Pptr = (k_ptr_t *)((__psunsigned_t)Pattr +
				 kl_member_offset(K,
						  "pageattr",
						  "at"));
			
			if(base_value(Pptr,"pattribute","prot",&Itmp) == -1) 
			{
				fprintf(ofp," -   ");
			}
			else 
			{
				fprintf(ofp," ");
				for(Itmp1=0;Itmp1 < 3;Itmp1++)
				{
					if(Itmp & (1<<Itmp1))
					{
						fprintf(ofp,"%c",
							prot[(int)Itmp1]);
					}
					else
					{
						fprintf(ofp,"-");
					}
					Itmp   &= ~(1<<Itmp1);
				}
				fprintf(ofp," ");
			}	
			if(base_value(Pptr,"pattribute","cc",&Itmp) == -1)
			{
				fprintf(ofp," - ");
			}
			else
			{
				fprintf(ofp," %2d",(unsigned)Itmp);
			}
			if(base_value(Pptr,"pattribute","uc",&Itmp) == -1)
			{
				fprintf(ofp," - ");
			}
			else
			{
				fprintf(ofp," %2d",(unsigned)Itmp);
			}
			if(base_value(Pptr,"pattribute","watchpt",&Itmp) 
			   == -1)
			{
				fprintf(ofp," -      ");
			}
			else
			{
				fprintf(ofp," %7d",(unsigned)Itmp);
			}
			if(base_value(Pptr,"pattribute","lockcnt",&Itmp) 
			   == -1)
			{
				fprintf(ofp," -      ");
			}
			else
			{
				fprintf(ofp," %7d",(unsigned)Itmp);
			}
			fprintf(ofp,"\n");
			KPattr = kl_kaddr(K,Pattr, "pageattr", "attr_next");
		}
		fprintf(ofp, "\n");
		K_BLOCK_FREE(K)(NULL,Pattr);
	}

	if (flags & C_NEXT) {
		rp=alloc_block(kl_struct_len(K,"region"),B_TEMP);
		if(kl_get_struct(K,kl_kaddr(K, prp, "pregion", "p_reg"),
				 kl_struct_len(K,"region"),rp,"region"))
		{
			region_banner(ofp,BANNER|SMAJOR);
			print_region(kl_kaddr(K, prp, "pregion", "p_reg"),rp,
				     flags,ofp);
			region_banner(ofp,SMAJOR);
		}
		K_BLOCK_FREE(K)(NULL,rp);

		list_mapped_pages(prp, flags, ofp);
		fprintf(ofp, "\n");
	}
	return(1);
}


/*
 * list_mapped_pages() 
 */
void
list_mapped_pages(k_ptr_t prp, int flags, FILE *ofp)
{
	int i;
	kaddr_t pde, addr;
	k_ptr_t pmp, pmpbuf; /* pmap_t */
	k_ptr_t pp, ppbuf; /* pde_t */
	k_ptr_t pattrs;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "list_mapped_pages: prp=0x%x, flags=0x%x\n", 
			prp, flags);
	}

	pmpbuf = alloc_block(PMAP_SIZE(K), B_TEMP);
	if (!(pmp = kl_get_struct(K, kl_kaddr(K, prp, "pregion", "p_pmap"), 
				  PMAP_SIZE(K), pmpbuf, "pmap"))) 
	{
		if (DEBUG(DC_GLOBAL, 1)) 
		{
			fprintf(ofp,
				"list_mapped_pages:could not get pmap "
				"0x%llx\n",
				kl_kaddr(K, prp, "pregion", "p_pmap"));
		}

		free_block(pmpbuf);

		return;
	}

	pattrs = (k_ptr_t)((uint)prp + FIELD("pregion", "p_attrs"));
	addr = kl_kaddr(K, pattrs, "pageattr", "attr_start");
	if ((KL_INT(K, prp, "pregion", "p_nvalid")) || (flags & C_ALL)) 
	{ 
		fprintf(ofp, "\n             VADDR               PDE"
			"               PGI       PFN\n");
		fprintf(ofp, "  ---------------------------------"
			"-----------------------------\n");
	}

	ppbuf = alloc_block(sizeof(kaddr_t), B_TEMP);
	for (i = 0; 
	     i < KL_INT(K, prp, "pregion", "p_pglen"); 
	     i++, addr += K_NBPC(K)) 
	{
		if ((pp = kl_pmap_to_pde(K, pmp, addr, ppbuf, &pde)) == 0) 
		{
			fprintf(ofp, "Could not locate pde 0x%llx/%lld\n",
				addr, KL_INT(K, prp, "pregion", "p_pglen"));

			free_block(ppbuf);
			free_block(pmpbuf);

			return;
		}
		/*
		 * If this pde is not valid then don't print it out unless the 
		 * C_ALL flag is specified... We don't check the pde being 
		 * valid the right way.. the correct way is to look at the 
		 * pte_vr bit...
		 */
		if ((int)pp && ((flags & C_ALL) || kl_get_pgi(K, pp)))
		{
			fprintf(ofp, "  %16llx  %16llx  %16llx  %8d\n", 
				addr, pde, kl_get_pgi(K, pp), 
				kl_pte_to_pfn(K, pp));
		}
	}
	fprintf(ofp, "\n");

	free_block(ppbuf);
	free_block(pmpbuf);

	return;
}

/*
 * Print out all the shared and private pregions of a uthread. This function
 * replaces list_proc_pregions after the address space information moved from
 * the proc_t structure to the uthread_s structure. So now instead of having a
 * pregion pointer in the proc structure... we now have an asid (address space
 * id) structure in the uthread_s struct. This move to the asid structure is
 * part of the move to Cellular Irix where the asid structure instead of 
 * pointing directly to the process address space will point to an intermediate
 * distributed client or server side address space structure (dsas_t or dcas_t)
 * Also the proc structure seems to be losing a lot of its importance/size and
 * seems to be getting smaller...
 *
 * This code is straight out of idbg_pregion... 
 */
int list_uthread_pregions(kaddr_t value, int mode, int flags, FILE *ofp)
{
	kaddr_t ut,vas,bhv,pas,prp;
	k_ptr_t utp,asid,vasp,bhvp,pasp,prpp;
	int32 struct_length;
	int firsttime = 1;

	if (mode == 1) 
	{
		/* 
		 * This is ambiguous, as one proc can have more than one uthread.
		 * So we don't like pid as an argument.  We took a conscious 
		 * decision not to support -- following the proc to all its 
		 * uthreads and printing out their address spaces.
		 */
		return 1;
	} 
	else
	{
		ut = value;
	}
	kl_reset_error();

	utp=kl_get_uthread_s(K,value,mode,flags);
	if (KL_ERROR) 
	{
		kl_print_error(K);
		return(1);
	}
  
	asid=UT_ASID(utp);

	if(KL_AS_ISNULL(asid))
	{
		if (DEBUG(DC_GLOBAL, 1)) 
		{
			fprintf(ofp,"uthread (0x%llx) does not have an address "
				"space assigned to it\n.",ut);
		}
		goto error;
	}

	/*
	 * Let's get the vas_s structure... in vasp.
	 */
	if((vas=KL_ASID_TO_VAS(asid)) == (kaddr_t)NULL)
	{
		goto error;
	}
	if(K_STRUCT_LEN(K)(NULL,"vas_s",&struct_length) < 0)
	{
		goto error;
	}

	if(!(vasp = kl_get_struct(K,vas,struct_length,NULL,"vas_s")))
	{
		if (DEBUG(DC_GLOBAL, 1))
		{
			fprintf(ofp,"Could not read vas_s at 0x%llx\n",vas);
		}
		goto error;
	}

	/*
	 * Let's get the bhv_desc_t structure... in bhvp
	 */
	if((bhv=KL_BHV_HEAD_FIRST(VAS_TO_BHVH(vasp))) == (kaddr_t)NULL)
	{
		goto error;
	}
	K_BLOCK_FREE(K)(NULL,vasp);/* We don't need it anymore. throw it away. */
	if(K_STRUCT_LEN(K)(NULL,"bhv_desc",&struct_length) < 0)
	{
		goto error;
	}
	if(!(bhvp = kl_get_struct(K,bhv,struct_length,NULL,"bhv_desc")))
	{
		if (DEBUG(DC_GLOBAL, 1))
		{
			fprintf(ofp,"Could not read bhv_desc_t at 0x%llx\n",bhv);
		}
		goto error;
	}
  
	/*
	 * We will not do the sanity check ... for pas_ops. We go straight for 
	 * the pas structure. In case of cellular Irix there will be an 
	 * intermediate layer... from the vas_t structure to the pas. We will
	 * need to check for cells before we traverse down to the pas structure 
         * then.
	 */
	if((pas=KL_BHV_TO_PAS(bhvp)) == (kaddr_t)NULL)
	{
		goto error;
	}
	K_BLOCK_FREE(K)(NULL,bhvp);/* We don't need it anymore. throw it away. */
	if(K_STRUCT_LEN(K)(NULL,"pas_s",&struct_length) < 0)
	{
		goto error;
	}

	if(!(pasp = kl_get_struct(K,pas,struct_length,NULL,"pas_s")))
	{
		if (DEBUG(DC_GLOBAL, 1))
			fprintf(ofp,"Could not read pas_s at 0x%llx\n",pas);
		goto error;
	}

	/*
	 * Big Sigh!!!!. We at last are close to having the pregion pointers.. 
	 * Now each uthread has 
	 * shared pregions   -- in the case of pthreads or sprocs 
	 * private pregions  -- each uthreads modified instructions/stack/(data?)
	 *
	 * We print out the shared pregions first and then go to the private 
	 * pregions...again as per idbg_pregion.
	 */
	if(K_STRUCT_LEN(K)(NULL,"pregion",&struct_length) < 0)
	{
		goto error;
	}
	for(prpp=NULL,prp=KL_PREG_FIRST(PAS_TO_PREG_SET(pasp));
	    prp;
	    prp=KL_PREG_NEXT(prpp))
	{
		if(prpp)
		{
			K_BLOCK_FREE(K)(NULL,prpp);
		}
		if(!(prpp = kl_get_struct(K,prp,struct_length,NULL,"pregion")))
		{
			if (DEBUG(DC_GLOBAL, 1))
			{
				fprintf(ofp,"Could not read pregion at 0x%llx\n",
					prp);
			}
			K_BLOCK_FREE(K)(NULL,pasp);
			goto error;
		}
		if(firsttime)
		{
			fprintf(ofp,"\nShared pregions for uthread 0x%llx\n",ut);
			pregion_banner(ofp, BANNER|SMAJOR);
			firsttime = 0;
		}
		else if(flags & (C_FULL|C_NEXT))
		{
			pregion_banner(ofp, SMAJOR);	
			fprintf(ofp,"\n");
			pregion_banner(ofp, BANNER|SMAJOR);	
		}
		print_pregion(prp,prpp,flags,ofp);
	}
	if(!firsttime)
	{
		pregion_banner(ofp, SMAJOR);
	}
	K_BLOCK_FREE(K)(NULL,pasp);
	if(prpp)
	{
		K_BLOCK_FREE(K)(NULL,prpp);
	}
	
	/*
	 * Now lets get the ppas structure. We re-use the pas and pasp pointers.
	 */
	if((pas=KL_ASID_TO_PASID(asid)) == (kaddr_t)NULL)
	{
		goto error;
	}
	if(K_STRUCT_LEN(K)(NULL,"ppas_s",&struct_length) < 0)
	{
		goto error;
	}
	if(!(pasp = kl_get_struct(K,pas,struct_length,NULL,"ppas_s")))
	{
		if (DEBUG(DC_GLOBAL, 1))
		{
			fprintf(ofp,"Could not read ppas_s at 0x%llx\n",pas);
		}
		goto error;
	}
  
	if(K_STRUCT_LEN(K)(NULL,"pregion",&struct_length) < 0)
	{
		goto error;
	}
	for(firsttime=1,prpp=NULL,prp=KL_PREG_FIRST(PPAS_TO_PREG_SET(pasp));
	    prp;
	    prp=KL_PREG_NEXT(prpp))
	{
		if(prpp)
		{
			K_BLOCK_FREE(K)(NULL,prpp);
		}

		if(!(prpp = kl_get_struct(K,prp,struct_length,NULL,"pregion")))
		{
			if (DEBUG(DC_GLOBAL, 1))
			{
				fprintf(ofp,"Could not read pregion at 0x%llx\n",
					prp);
			}
			K_BLOCK_FREE(K)(NULL,pasp);
			goto error;
		}

		if(firsttime)
		{
			fprintf(ofp,
				"\nPrivate pregions for uthread 0x%llx\n",ut);
			pregion_banner(ofp, BANNER|SMAJOR);
			firsttime = 0;
		}
		else if(flags & (C_FULL|C_NEXT))
		{
			pregion_banner(ofp, SMAJOR);
			fprintf(ofp,"\n");
			pregion_banner(ofp, BANNER|SMAJOR);	
		}

		print_pregion(prp,prpp,flags,ofp);
	}
	if(!firsttime)
	{
		pregion_banner(ofp, SMAJOR);
	}
  
	if(prpp)
	{
		K_BLOCK_FREE(K)(NULL,prpp);
	}
	K_BLOCK_FREE(K)(NULL,pasp);
	K_BLOCK_FREE(K)(NULL,utp);
	return 0;
error:
	K_BLOCK_FREE(K)(NULL,utp);
	return 1;

}


/* 
 * proc_banner()
 */
void
proc_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp,
					"            PROC ST   PID  PPID  PGID   UID  "
					"          WCHAN NAME\n");
		}
		else {
			fprintf(ofp,
					"            PROC ST   PID  PPID  PGID   UID  "
					"  WCHAN NAME\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"================================================="
			"============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------------"
			"----------------------------\n");
	}
}

/*
 * print_proc() 
 *
 *   Print out the proc structure information, and check the flags
 *   options for full print status.
 */
int
print_proc(kaddr_t proc, k_ptr_t procp, int flags, FILE *ofp)
{
	int pgrp;
	kaddr_t uthread;
	k_ptr_t name, vpgrpp, utp, cp, pde;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(ofp, "print_proc: proc=0x%llx, procp=0x%x, flags=0x%x, "
			"ofp=0x%x\n", proc, procp, flags, ofp);
	}

	if (!procp) { 
		KL_SET_ERROR_NVAL(KLE_BAD_PROC, proc, 2);
		kl_print_error(K);
		return(0);
	}

	/* Get the process group ID 
	 */
	vpgrpp = alloc_block(STRUCT("vpgrp"), B_TEMP);
	kl_get_struct(K, kl_kaddr(K, procp, "proc", "p_vpgrp"), 
			STRUCT("vpgrp"), vpgrpp, "vprgp");
	if (KL_ERROR) {
		pgrp = -1;
	}
	else {
		pgrp = KL_INT(K, vpgrpp, "vpgrp", "vpg_pgid");
	}
	free_block(vpgrpp);

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx %2lld %5lld %5lld %5d ",
		proc, 
		KL_INT(K, procp, "proc", "p_stat"),
		KL_INT(K, procp, "proc", "p_pid"),
		KL_INT(K, procp, "proc", "p_ppid"),
		pgrp);

	cp = alloc_block(CRED_SIZE(K), B_TEMP);
	kl_get_struct(K, kl_kaddr(K, procp, "proc", "p_cred"), 
		CRED_SIZE(K), cp, "cred");
	if (KL_ERROR) {
		fprintf(ofp, "      ");
	}
	else {
		fprintf(ofp, "%5lld ", KL_INT(K, cp, "cred", "cr_uid"));
	}
	free_block(cp);

	/* Now get the kthread
	 *
	 * XXX hack in os/pproc/pproc_private.h file
	 *
	 * #define p_kthread   p_proxy.prxy_threads->ut_kthread
	 *
	 * We don't have to get the proc_proxy struct since it is
	 * the first element in the proc struct.
	 */
	uthread = kl_kaddr(K, procp, "proc_proxy_s", "prxy_threads");

	/* If there is no uthread pointer, check to see if the proc is
	 * a zombie. We can do this by checking the p_stat filed in 
	 * the proc struct. If it is a zombile, print the appropriate 
	 * string for wchan and proc name respectively and then return.
	 */
	if (uthread == (kaddr_t)NULL) {
		if (PTRSZ64(K)) {
			fprintf(ofp, "0000000000000000 <defunct>\n");
		}
		else {
			fprintf(ofp, "00000000 <defunct>\n");
		}
		return(0);
	}

	utp = kl_get_uthread_s(K, uthread, 2, flags);
	if (KL_ERROR) {
		kl_print_error(K);
		return(1);
	}
	if (kl_kaddr(K, utp, "kthread", "k_wchan")) {
		if (PTRSZ64(K)) {
			fprintf(ofp, "%16llx ", kl_kaddr(K, utp, "kthread", "k_wchan"));
		}
		else {
			fprintf(ofp, "%8llx ", kl_kaddr(K, utp, "kthread", "k_wchan"));
		}
	}
	else {
		if (PTRSZ64(K)) {
			fprintf(ofp, "0000000000000000 ");
		}
		else {
			fprintf(ofp, "00000000 ");
		}
	}

	name = kl_kthread_name(K, utp);
	if (KL_ERROR) {
		fprintf(ofp, "ERROR\n");
	}
	else if (name) {
		fprintf(ofp, "%s\n", name);
	}
	else {
		fprintf(ofp, "<defunct>\n");
	}

	if (flags & C_FULL) {

		fprintf(ofp, "\n");
		fprintf(ofp, "SELECTED FIELDS FROM THE KTHREAD STRUCT AT 0x%llx:\n",
			uthread);
		print_kthread_full(utp, flags, ofp);

		fprintf(ofp, "\n");
		fprintf(ofp, "SELECTED FIELDS FROM THE PROC STRUCT:\n");

		fprintf(ofp, "\n");

		indent_it(flags, ofp);
		fprintf(ofp, "P_CHILDPIDS=0x%llx, ", 
			kl_kaddr(K, procp, "proc", "p_childpids"));

#ifdef XXX
		indent_it(flags, ofp);
		pde = (k_ptr_t)(ADDR(K, procp, "proc", "p_upgs") + 
				PDE_SIZE(K) * kstkidx);
		fprintf(ofp, "KSTACK: P_UPGS[%d]=0x%llx (0x%llx -- PFN: %d)\n", 
			kstkidx, kl_get_pgi(K, pde), K_KERNELSTACK(K), 
			kl_pte_to_pfn(K, pde));
		if (extusize) {
			pde = (k_ptr_t)(ADDR(K, procp, "proc", "p_upgs") + 
					PDE_SIZE(K) * extstkidx);
			indent_it(flags, ofp);
			fprintf(ofp, "KSTACK: P_UPGS[%d]=0x%llx (0x%llx -- PFN: "
				"%d)\n", extstkidx, kl_get_pgi(K, pde), 
				kextstack, kl_pte_to_pfn(K, pde));
		}

		/* XXX - The following fields are now part of the rusage struct
	     *		 (e.g., p_utime --> rusage.ru_utime).
		 */
		indent_it(flags, ofp);
		fprintf(ofp, "P_UTIME=%lld, ", KL_UINT(K, procp, "proc", "p_utime"));

		indent_it(flags, ofp);
		fprintf(ofp, "P_SIZE=%lld, P_RSS=%lld, P_MAXRSS=%lld, P_NLOCKPG=%lld\n",
			KL_UINT(K, procp, "proc", "p_size"),
			KL_UINT(K, procp, "proc", "p_rss"),
			KL_UINT(K, procp, "proc", "p_maxrss"),
			KL_UINT(K, procp, "proc", "p_nlockpg"));

		indent_it(flags, ofp);
		fprintf(ofp, "P_RESIDENT=%lld\n", 
			KL_UINT(K, procp, "proc", "p_resident"));
#endif

		indent_it(flags, ofp);
		fprintf(ofp, "P_SLINK=0x%llx, P_SHADDR=0x%llx\n\n",
			kl_kaddr(K, procp, "proc", "p_slink"),
			kl_kaddr(K, procp, "proc", "p_shaddr"));
		list_proc_files(proc, 2, (flags & (~C_FULL)), ofp);
		fprintf(ofp, "\n");
	}
	free_block(utp);
	return(1);
}

/*
 * list_active_procs()
 */
int
list_active_procs(int flags, FILE *ofp)
{
	int i, first_time = 1, proc_cnt = 0;
	k_ptr_t pidp, bhvdp, vprocp, procp;
	kaddr_t pid, vproc, proc, bhv;

	pidp = alloc_block(PID_ENTRY_SIZE(K), B_TEMP);
	vprocp = alloc_block(VPROC_SIZE(K), B_TEMP);
	pid = K_PIDACTIVE(K);

	/* We have to step over the first entry as it is just
	 * the list head.
	 */
	kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");

	do {
		kl_get_struct(K, pid, PID_ENTRY_SIZE(K), pidp, "pid_entry");
		vproc = kl_kaddr(K, pidp, "pid_entry", "pe_vproc");
		kl_get_struct(K, vproc, VPROC_SIZE(K), vprocp, "vproc");
        bhv = kl_kaddr(K, vprocp, "vproc", "vp_bhvh");
        proc = kl_bhv_pdata(K, bhv);
        if (KL_ERROR) {
			kl_print_error(K);
			kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
			continue;
        }

		/* If proc turns out to be invalid, print an error message and 
		 * continue. It's hard to say what might happen. We might just 
		 * get the next proc struct. Or, there might have been some
		 * corruption in the pidactive list. We'll just hope for the best...
		 */ 
		procp = kl_get_proc(K, proc, 2, flags);
		if (KL_ERROR) {
			kl_print_error(K);
			kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
			continue;
		}
		if ((first_time == TRUE) || (flags & C_FULL)) {
			proc_banner(ofp, BANNER|SMAJOR);
			first_time = FALSE;
		}
		print_proc(proc, procp, flags, ofp);
		free_block(procp);
		proc_cnt++;
		kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
	} while (pid != K_PIDACTIVE(K));
	free_block(pidp);
	free_block(vprocp);
	return(proc_cnt);
}

/* queue.c -- XXX TODO
 */


/*
 * region_banner() -- Print out banner information for region structure.
 */
void
region_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"          REGION  TYPE  RCNT   PGSZ  "
				"  NOFREE             VNODE     FLAGS\n");
		}
		else {
			fprintf(ofp,
				"  REGION          TYPE  RCNT   PGSZ  "
				"  NOFREE             VNODE     FLAGS\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"==============================================="
			"==========================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"-----------------------------------------------"
			"--------------------------\n");
	}
}

void decode_region_flags(k_ptr_t prp,FILE *ofp)
{
	int r_flags = KL_UINT(K, prp, "region", "r_flags");
	int i,firsttime;
	const char *tab_flags[] = {
		"RG_NOFREE",     /* 0x0001 Don't free region on last detach    */
		"RG_LOCKED",     /* 0x0002 region is locked for user DMA WAR   */
		"RG_LOADV",      /* 0x0004 region has a loadvnode              */
		"RG_CW",         /* 0x0008 cw region                           */
		"RG_DFILL",      /* 0x0010 Demand fill pages (not demand zero) */
		"RG_AUTOGROW",   /* 0x0020 Pages become autogrow after ip 
				  *        shrinks                             */
		"RG_PHYS",       /* 0x0040 Region describes physical memory    */
		"RG_ISOLATE",    /* 0x0080 Region is isolated for real-time,   
				  *        no aging                            */
		"RG_ANON",       /* 0x0100 (some) pages mapped anonymously     */
		"RG_USYNC",      /* 0x0200 Region aspace represents usync 
				  *        objects                             */
		"RG_HASSANON",   /* 0x0400 region has SANON pages              */
		"RG_TEXT",       /* 0x0800 region contains pure, read-only text
				  *        (unmodified with respect to a.out)  */
		"RG_AUTORESRV",  /* 0x1000 reserve availsmem on demand when 
				  *        page goes anonymous                 */
		"UNK(0x2000)",   /* 0x2000 UNKNOWN!!.                          */
		"RG_PHYSIO",     /* 0x4000 Region maps a physical device space */
		"RG_MAPZERO",    /* 0x8000 region is maps dev zero pages       */
		0
	};
	if(r_flags)
	{
		fprintf(ofp,"\n  R_FLAGS : ");
	}
	else
	{
		return;
	}
	for(i=0,firsttime=0;tab_flags[i];i++)
	{
		if(r_flags & (1<<i))
		{
			r_flags &= ~(1<<i);
			print_flags(&firsttime,tab_flags[i],"  ",ofp);
		}
	}
	fprintf(ofp,"\n");
}

/*
 * print_region() -- Print information in a region structure.
 */
int
print_region(kaddr_t reg, k_ptr_t rp, int flags, FILE *ofp)
{
	int firsttime;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_region: reg=0x%llx, rp=0x%x, flags=0x%x\n",
			reg, rp, flags);
	}

	FPRINTF_KADDR(ofp, "  ", reg);
	fprintf(ofp, "%4llu  %4llu  %5lld  %8llu  ",
		KL_UINT(K, rp, "region", "r_type"), 
		KL_UINT(K, rp, "region", "r_refcnt"),
		KL_INT(K, rp, "region", "r_pgsz"), 
		KL_UINT(K, rp, "region", "r_nofree"));
	fprintf(ofp, "%16llx  ", kl_kaddr(K, rp, "region", "r_vnode"));
	fprintf(ofp, "   %5llx\n", KL_UINT(K, rp, "region", "r_flags"));
	
	if(flags & C_FULL)
	{
		decode_region_flags(rp,ofp);
		fprintf(ofp,"  SWAPRES = 0x%x, FILEOFF = 0x%x, "
			"MAXFILESZ = 0x%x\n",
			(__int32_t)KL_UINT(K, rp, "region", "r_swapres"),
			(__int32_t)KL_UINT(K, rp, "region", "r_fileoff"),
			(__int32_t)KL_UINT(K, rp, "region", "r_maxfsize"));
		fprintf(ofp,"  ANON = 0x%llx\n",
			kl_kaddr(K, rp, "region", "r_anon"));
		fprintf(ofp,"\n");
	}
	return(1);
}

/*
 * list_region_pfdats()
 */
int
list_region_pfdats(k_ptr_t rp, int flags, FILE *ofp)
{
	int i, first_time = 1;
	kaddr_t value = 0, map;
	k_ptr_t pfp, pfbuf;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "list_region_pfdats: rp=0x%x, flags=0x%x\n",
			rp, flags);
	}

	if (kl_kaddr(K, rp, "region", "r_anon")) {
		map = kl_kaddr(K, rp, "region", "r_anon");
	} 
	else if (kl_kaddr(K, rp, "region", "r_vnode")) {
		map = kl_kaddr(K, rp, "region", "r_vnode");
	} 
	else {
		fprintf(ofp, "Don't know how to map this region!\n");
		return(1);
	}

	pfbuf = alloc_block(PFDAT_SIZE(K), B_TEMP);

	for (i = 0; i < KL_INT(K, rp, "region", "r_pgsz"); i++) {
		if (pfp = kl_locate_pfdat(K, (k_uint_t)i, map, pfbuf, &value)) {
			if (first_time) {
				fprintf(ofp, "\n");
				pfdat_banner(ofp, flags|(BANNER|SMINOR|C_INDENT));
				first_time = 0;
			}
			indent_it((flags|C_INDENT), ofp);
			print_pfdat(value, pfp, flags, ofp);
		}
	}
	free_block(pfbuf);
	return(0);
}

/*
 * get_rnode() -- This routine returns a pointer to an rnode.
 */
k_ptr_t
get_rnode(kaddr_t addr, k_ptr_t rbufp, int flags)
{
	k_ptr_t rp;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_rnode: addr=0x%llx, rbufp=0x%x, flags=0x%x\n",
			addr, rbufp, flags);
	}

	rp = kl_get_struct(K, addr, RNODE_SIZE(K), rbufp, "rnode");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_RNODE;
	}
	else {
		return (rp);
	}
	return ((k_ptr_t)NULL);
}

/*
 * rnode_banner() -- Print out banner information for rnode structure.
 */
void
rnode_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"           RNODE        SIZE             VNODE  "
				"IOCOUNT  ERROR  FLAGS\n");
		}
		else {
			fprintf(ofp,
				"   RNODE                SIZE             VNODE  "
				"IOCOUNT  ERROR  FLAGS\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf (ofp,
			"======================================="
			"==============================\n");
	}

	if (flags & SMINOR) {
		fprintf (ofp,
			"---------------------------------------"
			"------------------------------\n");
	}
}

/*
 * print_rnode() -- Print out information inside an rnode.
 */
int
print_rnode(kaddr_t addr, k_ptr_t rp, int flags, FILE *ofp)
{
	int rnode_cnt=0;
        k_ptr_t nnp, bufp;
        kaddr_t np;

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp, "  ", addr);
	fprintf(ofp, "%10lld  %16llx  %7lld  %5lld  %5llx\n", 
		KL_INT(K, rp, "rnode", "r_size"), 
		kl_bhvp_vobj(K, RNODE_TO_BHP(rp)),
		kl_kaddr(K, rp, "rnode", "r_bhv_desc"),
		KL_INT(K, rp, "rnode", "r_iocount"), 
		KL_INT(K, rp, "rnode", "r_error"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "NEXT=0x%llx, PREVP=0x%llx\n",
			kl_kaddr(K, rp, "rnode", "r_next"), 
			kl_kaddr(K, rp, "rnode", "r_prevp"));

		indent_it(flags, ofp);
		fprintf(ofp, "MNEXT=0x%llx, MPREVP=0x%llx\n",
			kl_kaddr(K, rp, "rnode", "r_mnext"), 
			kl_kaddr(K, rp, "rnode", "r_mprevp"));

		indent_it(flags, ofp);
		fprintf(ofp, "LASTR=0x%llx, LOCKTRIPS=%lld\n",
			kl_kaddr(K, rp, "rnode", "r_lastr"),
			KL_INT(K, rp, "rnode", "r_locktrips"));

		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "\nIOWAIT:\n\n");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
		print_sema_s(addr + FIELD("rnode", "r_iowait"),
			K_PTR(K, rp, "rnode", "r_iowait"), flags, ofp);

		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "RWLOCK:\n\n");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
		print_sema_s(addr + FIELD("rnode", "r_rwlock"),
			K_PTR(K, rp, "rnode", "r_rwlock"), flags, ofp);

		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "STATELOCK:\n\n");
		indent_it(flags, ofp);
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
		print_sema_s(addr + FIELD("rnode", "r_statelock"),
			K_PTR(K, rp, "rnode", "r_statelock"), flags, ofp);
		fprintf(ofp, "\n");
	}

        rnode_cnt++;

	if (flags & C_NEXT) {

                np = kl_kaddr(K, rp, "rnode", "r_next");
                while ( np ) {
                        bufp = alloc_block(STRUCT("rnode"), B_TEMP);
                        nnp = kl_get_struct(K, np, RNODE_SIZE(K), bufp, "rnode");
                        if (KL_ERROR) {
                                KL_ERROR |= KLE_BAD_RNODE;
                                free_block(bufp);
                                fprintf(ofp, "\n");
                                return(-1);
                        }
                        else {

                                fprintf(ofp, "\n");
                                rnode_banner(ofp, BANNER|SMINOR);
                                FPRINTF_KADDR(ofp, "  ", np);
                                fprintf(ofp, "%10lld  %16llx  %7lld  %5lld  0x%5llx\n",
                                        KL_INT(K, nnp, "rnode", "r_size"),
                                        kl_bhvp_vobj(K, RNODE_TO_BHP(nnp)),
                                        KL_INT(K, nnp, "rnode", "r_iocount"),
                                        KL_INT(K, nnp, "rnode", "r_error"),
                                        KL_UINT(K, nnp, "rnode", "r_flags"));
                                rnode_cnt++;

                        }
                        np = kl_kaddr(K, nnp, "rnode", "r_next");
                        free_block(bufp);
                }
                fprintf(ofp, "\n");
        }

        return(rnode_cnt);

}

/*
 * sema_s_banner() -- Print out banner information for sema structure.
 */
void
sema_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"            SEMA  COUNT  FLAGS      LOCK             QUEUE\n");
		}
		else {
			fprintf(ofp,
				"    SEMA  COUNT  FLAGS      LOCK     QUEUE\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp, "=================================================="
				"========\n");
		} else {
			fprintf(ofp,
				"==================================================\n");
		}
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp, "--------------------------------------------------"
				"--------\n");
		} else {
			fprintf(ofp,
				"--------------------------------------------------\n");
		}
	}
}

/*
 * print_sema_s() -- Print the contents of a semaphore structure.
 */
int
print_sema_s(kaddr_t addr, k_ptr_t semap, int flags, FILE *ofp)
{
	short ishorttmp;
	uint32_t i32tmp;
	uint64_t i64tmp;
	k_ptr_t ktp;
	kaddr_t squeue;

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp, "  ", addr);

	/* Because the sema_s contains a union, and that union contains
	 * a struct that do not have a struct type (they are defined
	 * in-line), it is necessary to get the s_count, s_flags, and
	 * s_lock values by brute force.
	 */
	ishorttmp = *(short *)(semap);
	fprintf(ofp, "%5hd", ishorttmp);

	ishorttmp = *(short *)((uint_t)semap + 2);
	fprintf(ofp, "  %5hd", ishorttmp);

	i32tmp = *(uint32_t *)((uint_t)semap);
	fprintf(ofp, "  %8x", i32tmp);

	if (PTRSZ64(K)) {
		fprintf(ofp, "  %16llx\n", 
			squeue=kl_kaddr(K, semap, "sema_s", "s_queue"));
	} else {
		fprintf(ofp, "  %8llx\n", 
			squeue=kl_kaddr(K, semap, "sema_s", "s_queue"));
	}

	/*
	 * Follow the kthread queue only if the -n option was used.
	 */
	if(flags & C_FULL)
	{			
	    flags &= ~C_FULL;
	    ktp=alloc_block(KTHREAD_SIZE(K),B_TEMP);
	    if(addr=squeue)	/* if not NULL. */
	    {
		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp,"WAITERS:\n");
		indent_it(flags, ofp);
		kthread_banner(ofp, BANNER|SMINOR);
		do
		{
		    /*
		     * Keep going until kthread queue loops around.
		     */
		    kl_get_struct(K,addr,KTHREAD_SIZE(K),ktp,"kthread");
		    if(KL_ERROR) {
			KL_SET_ERROR(KLE_BAD_KTHREAD);
			free_block(ktp);
			return(0);
		    }
		    indent_it(flags,ofp);
		    print_kthread(addr,ktp,flags,ofp);
		    addr=kl_kaddr(K,ktp,"kthread","k_flink");
		}
		while(addr && addr != squeue);
	    }
	    free_block(ktp);
	}

	return(1);
}

/*
 * get_lsnode()
 *
 *   This routine only returns a pointer to a "local" snode if the lsnode
 *   at addr is valid (there is a positive reference count) or the C_ALL
 *   flag is set.
 */
k_ptr_t
get_lsnode(kaddr_t addr, k_ptr_t sbufp, int flags)
{
  if (DEBUG(DC_FUNCTRACE, 2)) {
    fprintf(KL_ERRORFP, "get_lsnode: addr=0x%llx, sbufp=0x%x, flags=0x%x\n",
	    addr, sbufp, flags);
  }

  if (sbufp == (k_ptr_t)NULL) {
    KL_SET_ERROR(KLE_NULL_BUFF|KLE_BAD_LSNODE);
    return((k_ptr_t)NULL);
  }

  kl_get_struct(K,addr, LSNODE_SIZE(K), sbufp, "lsnode");
  if (KL_ERROR) {
    KL_ERROR |= KLE_BAD_LSNODE;
  }
  else {
    if (addr == kl_bhvp_pdata(K,LSNODE_TO_BHP(sbufp))) {
      if (KL_UINT(K, sbufp, "lsnode", "ls_opencnt")) {
	return(sbufp);
      }
      else if (flags & C_ALL) {
	return(sbufp);
      }
    }
    KL_SET_ERROR_NVAL(KLE_BAD_LSNODE, addr, 2);
  }
  return((k_ptr_t)NULL);
}

/*
 * lsnode_banner() -- Print out banner information for an lsnode structure.
 */
void
lsnode_banner(FILE *ofp, int flags)
{
  if (flags & BANNER) {
    if (PTRSZ64(K)) {
      fprintf(ofp,
	      "          LSNODE  RCNT       DEV              NEXT  "
	      "           VNODE    FLAGS\n");
    }
    else {
      fprintf(ofp,
	      "  LSNODE          RCNT       DEV              NEXT  "
	      "           VNODE    FLAGS\n");
    }
  }

  if (flags & SMAJOR) {
    fprintf(ofp,
	    "=============================================="
	    "===============================\n");
  }

  if (flags & SMINOR) {
    fprintf(ofp,
	    "----------------------------------------------"
	    "-------------------------------\n");
  }
}

/*
 * print_lsnode() -- Print out lsnode structure specifics.
 */
int
print_lsnode(kaddr_t addr, k_ptr_t sp, int flags, FILE *ofp)
{
  if (DEBUG(DC_GLOBAL, 3)) {
    fprintf(ofp, "print_lsnode: addr=0x%llx, sp=0x%x, flags=0x%x\n",
	    addr, sp, flags);
  }

  indent_it(flags, ofp);
  FPRINTF_KADDR(ofp, "  ", addr);
  fprintf(ofp, "%4lld  %8llx  ",
	  KL_UINT(K,sp, "lsnode", "ls_opencnt"),
	  KL_UINT(K,sp, "lsnode", "ls_dev"));
  fprintf(ofp, "%16llx  ", kl_kaddr(K,sp, "lsnode", "ls_next"));
  fprintf(ofp, "%16llx  ", kl_bhvp_vobj(K,LSNODE_TO_BHP(sp)));
  fprintf(ofp, "%7llx\n", KL_INT(K,sp, "lsnode", "ls_flag"));

  if (flags & C_FULL) {
    fprintf(ofp, "\n");

    indent_it(flags, ofp);
    fprintf(ofp, "CVP_HANDLE = 0x%llx\n",
	    kl_kaddr(K,sp, "lsnode", "ls_cvp_handle"));

    indent_it(flags, ofp);
    fprintf(ofp, "ATIME = %lld, ", KL_INT(K,sp, "lsnode", "ls_atime"));
    fprintf(ofp, "MTIME = %lld, ", KL_INT(K,sp, "lsnode", "ls_mtime"));
    fprintf(ofp, "CTIME = %lld\n", KL_INT(K,sp, "lsnode", "ls_ctime"));

    indent_it(flags, ofp);
    fprintf(ofp, "FSID = 0x%llx, ", KL_UINT(K,sp, "lsnode", "ls_fsid"));
    fprintf(ofp, "SIZE = %lld\n", KL_UINT(K,sp, "lsnode", "ls_size"));
    indent_it(flags, ofp);

#ifdef NOTTHIS
    fprintf(ofp, "LOCKPID = %lld, ", kl_kaddr(sp, "lsnode", "s_lockid"));
    fprintf(ofp, "LOCKTRIPS = %lld\n",
	    KL_UINT(sp, "lsnode", "s_locktrips"));

    indent_it(flags, ofp);
    fprintf(ofp, "LOCK:\n");
    sema_s_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
    print_sema_s(addr + kl_member_offset("lsnode", "s_lock"),
		 K_PTR(sp, "lsnode", "s_lock"), flags, ofp);
#endif /* NOTTHIS */
    fprintf(ofp, "\n");
  }
  return(1);
}

/*
 * print_lsnode() -- Print out lsnode structure specifics.
 */
int
print_csnode(kaddr_t addr, k_ptr_t sp, int flags, FILE *ofp)
{
  if (DEBUG(DC_GLOBAL, 3)) {
    fprintf(ofp, "print_csnode: addr=0x%llx, sp=0x%x, flags=0x%x\n",
	    addr, sp, flags);
  }

  indent_it(flags, ofp);
  FPRINTF_KADDR(ofp, "  ", addr);
  fprintf(ofp, "%4lld  %8llx  ",
	  KL_UINT(K,sp, "csnode", "cs_opencnt"),
	  KL_UINT(K,sp, "csnode", "cs_dev"));
  fprintf(ofp, "%16llx  ", kl_kaddr(K,sp, "csnode", "cs_next"));
  fprintf(ofp, "%16llx  ", kl_bhvp_vobj(K,CSNODE_TO_BHP(sp)));
  fprintf(ofp, "%7llx\n", KL_INT(K,sp, "csnode", "cs_flag"));

  if (flags & C_FULL) {
    fprintf(ofp, "\n");

    indent_it(flags, ofp);
    fprintf(ofp, "ATIME = %lld, ", KL_INT(K,sp, "csnode", "cs_atime"));
    fprintf(ofp, "MTIME = %lld, ", KL_INT(K,sp, "csnode", "cs_mtime"));
    fprintf(ofp, "CTIME = %lld\n", KL_INT(K,sp, "csnode", "cs_ctime"));

    indent_it(flags, ofp);
    fprintf(ofp, "FSID = 0x%llx, ", KL_UINT(K,sp, "csnode", "cs_fsid"));
    fprintf(ofp, "SIZE = %lld\n", KL_UINT(K,sp, "csnode", "cs_size"));
    indent_it(flags, ofp);

    fprintf(ofp, "\n");
  }
  return(1);
}

/*
 * get_socket() -- Get a socket structure.
 */
k_ptr_t
get_socket(kaddr_t soc, k_ptr_t sp, int flags)
{
	k_ptr_t sfp;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_socket: soc=0x%llx, sp=0x%x, flags=0x%x\n",
			soc, sp, flags);
	}

	if (sfp = kl_get_struct(K, soc, SOCKET_SIZE(K), sp, "socket")) {
		return (sfp);
	}
	KL_ERROR |= KLE_BAD_SOCKET;
	return ((k_ptr_t)NULL);
}

/*
 * socket_banner() -- Print banner information for socket structure.
 */
void
socket_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"          SOCKET  DOMAIN    TYPE  STATE               PCB    "
				"RECV-Q    SEND-Q\n");
		}
		else {
			fprintf(ofp,
				"  SOCKET          DOMAIN    TYPE  STATE               PCB    "
				"RECV-Q    SEND-Q\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"=========================================================="
			"===================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"----------------------------------------------------------"
			"-------------------\n");
	}
}

/*
 * print_socket() -- Print out contents of a socket structure
 */
int
print_socket(kaddr_t soc, k_ptr_t sp, int flags, FILE *ofp)
{
	k_ptr_t unpcp, protop;
	k_ptr_t rcvp, sndp, sophp, domainp;
	kaddr_t so_rcv, so_snd;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_socket: soc=0x%llx, sp=0x%x, flags=0x%x\n", 
			soc, sp, flags);
	}

	/*
	 * Determine the socket domain (AF_UNIX, AF_INET, etc.)
	 */
	protop = alloc_block(PROTOSW_SIZE(K), B_TEMP);
	kl_get_struct(K, kl_kaddr(K, sp, "socket", "so_proto"), 
					PROTOSW_SIZE(K), protop, "protosw");
	KL_CHECK_ERROR_RETURN;

	domainp = alloc_block(DOMAIN_SIZE(K), B_TEMP);
	kl_get_struct(K, kl_kaddr(K, protop, 	
		"protosw", "pr_domain"), DOMAIN_SIZE(K), domainp, "domain");
	KL_CHECK_ERROR_RETURN;

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp, "  ", soc);
	switch (KL_INT(K, domainp, "domain", "dom_family")) {
		case AF_UNIX : 
			fprintf(ofp, "  UNIX  ");
			break;

		case AF_INET : 
			fprintf(ofp, "  INET  ");
			break;

		default :
			fprintf(ofp, " OTHER  ");
	}

	switch (KL_INT(K, sp, "socket", "so_type")) {
		case SOCK_STREAM : 
			fprintf(ofp, "STREAM  ");
			break;

		case SOCK_DGRAM : 
			fprintf(ofp, " DGRAM  ");
			break;

		case SOCK_RAW : 
			fprintf(ofp, "   RAW  ");
			break;

		case SOCK_RDM : 
			fprintf(ofp, "   RDM  ");
			break;

		case SOCK_SEQPACKET : 
			fprintf(ofp, "SEQPKT  ");
			break;

		default :
			fprintf(ofp, "   BAD  ");
	}

	/* Get the pointer (in the socket struct) for so_rcv and so_snd.
	 */
	rcvp = (k_ptr_t)((uint)sp + FIELD("socket", "so_rcv"));
	sndp = (k_ptr_t)((uint)sp + FIELD("socket", "so_snd"));

	/* XXX - could be a problem with the next fprintf() !?!
	 */
	fprintf(ofp, "%5llx  ", KL_INT(K, sp, "socket", "so_state") & 0xffff);
	fprintf(ofp, "%16llx  ", kl_kaddr(K, sp, "socket", "so_pcb"));
	fprintf(ofp, "%8llu  %8llu\n", 
		KL_UINT(K, rcvp, "sockbuf", "sb_cc"), 
		KL_UINT(K, sndp, "sockbuf", "sb_cc"));

	if (flags & C_FULL) { 

		/* Get the kernel address of the rcv and snd sockbuf structures
		 * (actually part of the socket struct itself). 
		 */
		so_rcv = soc + FIELD("socket", "so_rcv");
		so_snd = soc + FIELD("socket", "so_snd");

		fprintf(ofp, "\n");

		indent_it(flags, ofp);
		fprintf(ofp, "SO_OPTIONS=0x%llx, SO_LINGER=0x%llx, SO_PGID=%lld, "
			"SO_PROTO=0x%llx\n", 
			KL_INT(K, sp, "socket", "so_options") & 0xffff,
			KL_INT(K, sp, "socket", "so_linger") & 0xffff,
			KL_INT(K, sp, "socket", "so_pgid") & 0xffff,
			kl_kaddr(K, sp, "socket", "so_proto"));

		indent_it(flags, ofp);
		fprintf(ofp, "SO_HEAD=0x%llx, SO_TIMO=%lld, SO_ERROR=%llu, "
				"SO_OOBMARK=%llu\n",
			kl_kaddr(K, sp, "socket", "so_head"),
			KL_INT(K, sp, "socket", "so_timeo"),
			KL_UINT(K, sp, "socket", "so_error"),
			KL_UINT(K, sp, "socket", "so_oobmark"));

		indent_it(flags, ofp);
		fprintf(ofp, "SO_Q0=0x%llx, SO_Q0LEN=%lld, SO_Q=0x%llx, SO_QLEN=%lld\n",
			kl_kaddr(K, sp, "socket", "so_q0"),
			KL_INT(K, sp, "socket", "so_q0len"),
			kl_kaddr(K, sp, "socket", "so_q"),
			KL_INT(K, sp, "socket", "so_qlen"));

		indent_it(flags, ofp);
		fprintf(ofp, "SO_QLIMIT=%lld, SO_LABEL=0x%llx, SO_SENDLABEL=0x%llx\n", 
			KL_INT(K, sp, "socket", "so_qlimit"),
			kl_kaddr(K, sp, "socket", "so_label"),
			kl_kaddr(K, sp, "socket", "so_sendlabel"));

		indent_it(flags, ofp);
		fprintf(ofp, "SO_ACL=0x%llx, SO_CALLOUT=0x%llx, "
			"SO_CALLOUT_ARG=0x%llx\n",
			kl_kaddr(K, sp, "socket", "so_acl"),
			kl_kaddr(K, sp, "socket", "so_callout"),
			kl_kaddr(K, sp, "socket", "so_callout_arg"));

		sophp = K_PTR(K, sp, "socket", "so_ph");

		indent_it(flags, ofp);
		fprintf(ofp, "SO_SEM:\n");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
		print_sema_s(
			soc + FIELD("socket", "so_sem"),
			K_PTR(K, sp, "socket", "so_sem"), 
			(flags & C_INDENT)|(flags & ~(C_FULL)), 
			ofp);
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);

		indent_it(flags, ofp);
		fprintf(ofp, "SO_PH:\n");
		indent_it(flags, ofp);
		fprintf(ofp, "  PH_LIST=0x%llx, PH_LOCK=0x%llx\n",
			kl_kaddr(K, sophp, "pollhead", "ph_list"),
			KL_UINT(K, sophp, "pollhead", "ph_lock"));

		indent_it(flags, ofp);
		fprintf(ofp, "  PH_GEN=0x%llx, PH_EVENTS=0x%lld, PH_USER=%lld\n",
			KL_UINT(K, sophp, "pollhead", "ph_gen"),
			KL_INT(K, sophp, "pollhead", "ph_events"),
			KL_INT(K, sophp, "pollhead", "ph_user"));

		if (DEBUG(DC_SOCKET, 1)) {
			fprintf(ofp, "print_socket: so_rcv=0x%llx, rcvp=0x%x\n",
				so_rcv, rcvp);	
			fprintf(ofp, "print_socket: so_snd=0x%llx, sndp=0x%x\n",
				so_snd, sndp);
		}

		indent_it(flags, ofp);
		fprintf(ofp, "SO_RCV:\n");
		indent_it(flags, ofp);
		fprintf(ofp, "  SB_CC=%llu, SB_LOWAT=%llu, SB_HIWAT=%llu, "
				"SB_MB=0x%llx\n",
			KL_UINT(K, rcvp, "sockbuf", "sb_cc"),
			KL_UINT(K, rcvp, "sockbuf", "sb_lowat"), 
			KL_UINT(K, rcvp, "sockbuf", "sb_hiwat"), 
			kl_kaddr(K, rcvp, "sockbuf", "sb_mb"));

		indent_it(flags, ofp);
		fprintf(ofp, "  SB_TIMEO=%lld, SB_FLAGS=0x%llx\n",
			KL_INT(K, rcvp, "sockbuf", "sb_timeo"), 
			KL_INT(K, rcvp, "sockbuf", "sb_flags"));

		indent_it(flags, ofp);
		fprintf(ofp, "  SB_WANTSEM:\n");
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);
		fprintf(ofp, "  ");
		print_sema_s(
			so_rcv + FIELD("sockbuf", "sb_wantsem"),
			K_PTR(K, rcvp, "sockbuf", "sb_wantsem"), 
			(flags & C_INDENT)|(flags & ~(C_FULL)), 
			ofp);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);

		indent_it(flags, ofp);
		fprintf(ofp, "  SB_WAITSEM:\n");
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);
		fprintf(ofp, "  ");
		print_sema_s(
			so_rcv + FIELD("sockbuf", "sb_waitsem"),
			K_PTR(K, rcvp, "sockbuf", "sb_waitsem"), 
			(flags & C_INDENT)|(flags & ~(C_FULL)), 
			ofp);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);

		indent_it(flags, ofp);
		fprintf(ofp, "SO_SND:\n");
		indent_it(flags, ofp);
		fprintf(ofp, "  SB_CC=%llu, SB_LOWAT=%llu, SB_HIWAT=%llu, "
			"SB_MB=0x%llx\n",
			KL_UINT(K, sndp, "sockbuf", "sb_cc"),
			KL_UINT(K, sndp, "sockbuf", "sb_lowat"), 
			KL_UINT(K, sndp, "sockbuf", "sb_hiwat"), 
			kl_kaddr(K, sndp, "sockbuf", "sb_mb"));

		indent_it(flags, ofp);
		fprintf(ofp, "  SB_TIMEO=%lld, SB_FLAGS=0x%llx\n",
			KL_INT(K, sndp, "sockbuf", "sb_timeo"), 
			KL_UINT(K, sndp, "sockbuf", "sb_flags"));

		indent_it(flags, ofp);
		fprintf(ofp, "  SB_WANTSEM:\n");
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);
		fprintf(ofp, "  ");
		print_sema_s(
			so_snd + FIELD("sockbuf", "sb_wantsem"),
			K_PTR(K, sndp, "sockbuf", "sb_wantsem"), 
			(flags & C_INDENT)|(flags & ~(C_FULL)), 
			ofp);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);

		indent_it(flags, ofp);
		fprintf(ofp, "  SB_WAITSEM:\n");
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);
		fprintf(ofp, "  ");
		print_sema_s(
			so_rcv + FIELD("sockbuf", "sb_waitsem"),
			K_PTR(K, rcvp, "sockbuf", "sb_waitsem"), 
			(flags & C_INDENT)|(flags & ~(C_FULL)), 
			ofp);
		fprintf(ofp, "  ");
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);

		fprintf(ofp, "\n");
		fprintf(ofp, "SO_CPU=0x%llx\n", 
			KL_UINT(K, sp, "socket", "so_cpu") & 0xffff);

	}

	if (flags & C_NEXT) {
		fprintf(ofp, "\n");

		/* stuff the protocol type into the upper part of flags 
		 */
		flags |= (KL_INT(K, protop, "protosw", "pr_protocol") << 16);
		flags |= C_INDENT;
		switch (KL_INT(K, domainp, "domain", "dom_family")) {
			case AF_UNIX :
				unpcp = alloc_block(UNPCB_SIZE(K), B_TEMP);
				kl_get_struct(K, kl_kaddr(K, sp, "socket", "so_pcb"), 
									UNPCB_SIZE(K), unpcp, "unpcb");
				if (!KL_ERROR) {
					unpcb_banner(ofp, flags|BANNER|SMINOR);
					indent_it(flags, ofp);
					print_unpcb(kl_kaddr(K, sp, "socket", "so_pcb"),
						unpcp, flags, ofp);
					fprintf(ofp, "\n");
				}
				break;

			case AF_INET :
				unpcp = alloc_block(INPCB_SIZE(K), B_TEMP);
				kl_get_struct(K, kl_kaddr(K, sp, "socket", "so_pcb"), 
									INPCB_SIZE(K), unpcp, "inpcb");
				if (!KL_ERROR) {
					inpcb_banner(ofp, flags|BANNER|SMINOR);
					indent_it(flags, ofp);
					print_inpcb(kl_kaddr(K, sp, "socket", "so_pcb"),
						unpcp, flags, ofp);
					fprintf(ofp, "\n");
				}
				break;

			default :
				break;
		}
	}
	free_block(protop);
	free_block(domainp);
	return(0);
}

/*
 * list_sockets() -- List out sockets open on certain file structures.
 */
int
list_sockets(int flags, FILE *ofp)
{
	int i, first_time = 1, soc_cnt = 0;
	kaddr_t nextfile;
	k_ptr_t kfp, vp, sp;

	vp = alloc_block(VNODE_SIZE(K), B_TEMP);
	kfp = alloc_block(VFILE_SIZE(K), B_TEMP);
	sp = alloc_block(SOCKET_SIZE(K), B_TEMP);
	nextfile = K_ACTIVEFILES(K);
	while (nextfile) {
		get_vfile(nextfile, kfp, flags);
		if (KL_ERROR) {
			if (DEBUG(DC_SOCKET, 1)) {
				kl_print_debug(K, "list_sockets");
			}
			/*
			 * XXXX: The "file" struct does not exist anymore.
			 *       It's changed to "vfile". What should we
			 *       be doing here ?.
			 */
			nextfile = kl_kaddr(K, kfp, "file", "f_next");
			continue;
		}

		if (KL_UINT(K, kfp, "vfile", "vf_count")) {
			get_vnode(kl_kaddr(K, kfp, "vfile", "__vf_data__"), vp, flags);
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug(K, "list_sockets");
				}
				nextfile = kl_kaddr(K, kfp, "vfile", "vf_next");
				continue;
			}
			if (KL_UINT(K, vp, "vnode", "v_type") == VSOCK) {
				get_socket(kl_kaddr(K, vp, "vnode", "v_data"), sp, flags);
				if (!KL_ERROR) {
					if (flags & (C_FULL|C_NEXT)) {
						if (!first_time) {
							socket_banner(ofp, BANNER|SMAJOR);
						} 
						else {
							first_time = 0;
						}
					}
					print_socket(kl_kaddr(K, vp, "vnode", "v_data"), 
						sp, flags, ofp);
					soc_cnt++;
				}
			}
		}
		nextfile = kl_kaddr(K, kfp, "vfile", "vf_next");
	}
	free_block(vp);
	free_block(kfp);
	free_block(sp);
	return (soc_cnt);
}

/* 
 * sthread_s_banner()
 */
void
sthread_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"         STHREAD              NEXT              PREV  NAME\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"================================================="
			"============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-------------------------------------------------"
			"----------------------------\n");
	}
}

/*
 * print_sthread_s() 
 *
 *   Print out selected fields from the sthread struct and check the flags
 *   options for full print status. 
 */
int
print_sthread_s(kaddr_t sthread, k_ptr_t stp, int flags, FILE *ofp)
{
	k_ptr_t kt_name;

    if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(ofp, "print_sthread_s: sthread=0x%llx, stp=0x%x, flags=0x%x, "
			"ofp=0x%x\n", sthread, stp, flags, ofp);
	}

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %16llx  %16llx  %s\n",
		sthread, 
		kl_kaddr(K, stp, "sthread_s", "st_next"),
		kl_kaddr(K, stp, "sthread_s", "st_prev"),
	    kl_kthread_name(K, stp));
	return(1);
}

/*
 * list_active_sthreads()
 */
int
list_active_sthreads(int flags, FILE *ofp)
{
	int sthread_cnt = 0, first_time = 1;
	k_ptr_t stp;
	kaddr_t sthread;

	stp = alloc_block(STHREAD_S_SIZE(K), B_TEMP);
	kl_get_struct(K, K_STHREADLIST(K), STHREAD_S_SIZE(K), stp, "sthread_s");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_STHREAD_S;
		kl_print_error(K);
		return(1);
	}
	sthread = kl_kaddr(K, stp, "sthread_s", "st_next");
	free_block(stp);
	if (flags & C_KTHREAD) {
		kthread_banner(ofp, BANNER|SMAJOR);
	}
	else {
		sthread_s_banner(ofp, BANNER|SMAJOR);
	}
	while(sthread != K_STHREADLIST(K)) {
		stp = kl_get_sthread_s(K, sthread, 2, flags);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_STHREAD_S;
			kl_print_error(K);
			return(sthread_cnt);
		}
		else {
			if (first_time == TRUE) {
				first_time = FALSE;
			}
			else if (flags & (C_FULL|C_NEXT)) {
				if (flags & C_KTHREAD) {
					kthread_banner(ofp, BANNER|SMAJOR);
				}
				else {
					sthread_s_banner(ofp, BANNER|SMAJOR);
				}
			}
			if (flags & C_KTHREAD) {
				print_kthread(sthread, stp, flags, ofp);
			}
			else {
				print_sthread_s(sthread, stp, flags, ofp);
			}
			if (KL_ERROR) {
				kl_print_error(K);
			}
			free_block(stp);
			sthread_cnt++;
			sthread = kl_kaddr(K, stp, "sthread_s", "st_next");
		}
	}
	return(sthread_cnt);
}

/* cmd_stream.c -- XXX TODO
 */

/*
 * tcpcb_banner() -- Print out banner information for tcpcb structure.
 */
void
tcpcb_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"             TCPCB    STATE  FLAGS             INPCB\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"  ==================================================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"  --------------------------------------------------\n");
	}
}

/*
 * print_tcpcb() -- Print out an address as an tccpb structure.
 */
int
print_tcpcb(kaddr_t tcp, k_ptr_t tcpp, int flags, FILE *ofp)
{
	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_tcpcb: tcp=0x%llx, tcpp=0x%x, flags=0x%x\n",
			tcp, tcpp, flags);
	}

	fprintf(ofp, "  %16llx     %4llx     %2llx  %16llx\n", 
		tcp, 
		KL_INT(K, tcpp, "tcpcb", "t_state") & 0xffff,
		KL_UINT(K, tcpp, "tcpcb", "t_flags") & 0xff,
		kl_kaddr(K, tcpp, "tcpcb", "t_inpcb"));
	return(1);
}

/*
 * unpcb_banner() -- Print out banner information for unpcb structure.
 */
void
unpcb_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"           UNPCB            SOCKET             VNODE      "
				"INUM\n");
		}
		else {
			fprintf(ofp,
				"   UNPCB                    SOCKET             VNODE      "
				"INUM\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"========================================="
			"=====================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"-----------------------------------------"
			"---------------------\n");
	}
}

/*
 * print_unpcb() -- Print out an address as an uncpb structure.
 */
int
print_unpcb(kaddr_t un, k_ptr_t unp, int flags, FILE *ofp)
{
	if (DEBUG(DC_GLOBAL, 3))
		fprintf(ofp, "print_unpcb: un=0x%llx, unp=0x%x, flags=0x%x\n",
			un, unp, flags);

	FPRINTF_KADDR(ofp, "  ", un);
	fprintf(ofp, "%16llx  ", kl_kaddr(K, unp, "unpcb", "unp_socket"));
	fprintf(ofp, "%16llx  ", kl_kaddr(K, unp, "unpcb", "unp_vnode"));
	fprintf(ofp, "%8lld\n", KL_INT(K, unp, "unpcb", "unp_ino"));
	return(1);
}

/*
 * get_vfs() -- Get a vfs structure.  
 * 
 *   We assume that vbufp is already allocated to the caller.
 */
k_ptr_t
get_vfs(kaddr_t addr, k_ptr_t vbufp, int flags)
{
	int type;
	k_ptr_t vfp;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_vfs: addr=0x%llx, vbufp=0x%x, flags=0x%x\n",
			addr, vbufp, flags);
	}

	vfp = kl_get_struct(K, addr, VFS_SIZE(K), vbufp, "vfs");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_VFS;
	}
	else {
		type = KL_INT(K, vbufp, "vfs", "vfs_fstype");
		if ((type >= 0) && (type < nfstype)) {
			return (vfp);
		}
		KL_SET_ERROR_NVAL(KLE_BAD_VFS, addr, 2);
	}
	return ((k_ptr_t)0);
}

/*
 * vfs_banner() -- Print out vfs structure banner information.
 */
void
vfs_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"             VFS  INDEX      TYPE          VNODECOV     "
				"  DEV                BH\n");
		} else {
			fprintf(ofp,
				"     VFS          INDEX      TYPE          VNODECOV     "
				"  DEV                BH\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"============================================="
			"==================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "---------------------------------------------"
			"----------------------------------\n");
	}
}

/*
 * print_vfs() -- Print the specific fields in a vfs structure
 */
int
print_vfs(kaddr_t addr, k_ptr_t vfsp, int flags, FILE *ofp)
{
	int type;
	k_ptr_t tmpptr;
	kaddr_t tmpaddr;
	char vfs_name[128];

	type = KL_INT(K, vfsp, "vfs", "vfs_fstype");
	tmpptr = (k_ptr_t)((unsigned)_vfssw + (type * VFSSW_SIZE(K)));
	tmpaddr = kl_kaddr(K, tmpptr, "vfssw", "vsw_name");
	kl_get_block(K, tmpaddr, 128, vfs_name, "vfs_name");

	if (vfs_name[0] == '\0') {
		FPRINTF_KADDR(ofp, "  ", addr);
		fprintf(ofp, "%5lld  %8s  %16llx  %8llx  %16llx\n",
			KL_INT(K, vfsp, "vfs", "vfs_fstype"), 
			"unknown",
			kl_kaddr(K, vfsp, "vfs", "vfs_vnodecovered"), 
			KL_INT(K, vfsp, "vfs", "vfs_dev"), 
			kl_kaddr(K, vfsp, "vfs", "vfs_data"));
	} else {
		FPRINTF_KADDR(ofp, "  ", addr);
		fprintf(ofp, "%5lld  %8s  %16llx  %8llx  %16llx\n",
			KL_INT(K, vfsp, "vfs", "vfs_fstype"), 
			vfs_name,
			kl_kaddr(K, vfsp, "vfs", "vfs_vnodecovered"), 
			KL_INT(K, vfsp, "vfs", "vfs_dev"), 
			kl_kaddr(K, vfsp, "vfs", "vfs_bh"));
	}

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		fprintf(ofp, "VFS_NEXT=0x%llx, VFS_PREVP=0x%llx\n",
			kl_kaddr(K, vfsp, "vfs", "vfs_next"),
			kl_kaddr(K, vfsp, "vfs", "vfs_prevp"));
		fprintf(ofp, "VFS_FSID=0x%llx, VFS_OP=0x%llx\n",
			kl_kaddr(K, vfsp, "vfs", "vfs_fsid"),
			kl_kaddr(K, vfsp, "vfs", "vfs_op"));
		fprintf(ofp, "VFS_DCOUNT=%lld, VFS_FLAG=0x%llx, VFS_NSUBMOUNTS=%llu\n", 
			KL_INT(K, vfsp, "vfs", "vfs_dcount"),
			KL_UINT(K, vfsp, "vfs", "vfs_flag"),
			KL_UINT(K, vfsp, "vfs", "vfs_nsubmounts"));
		fprintf(ofp, "VFS_BUSYCNT=%lld, VFS_BSIZE=%llu, VFS_BCOUNT=%llu\n",
			KL_INT(K, vfsp, "vfs", "vfs_busycnt"),
			KL_UINT(K, vfsp, "vfs", "vfs_bsize"),
			KL_UINT(K, vfsp, "vfs", "vfs_bcount"));
		fprintf(ofp, "VFS_WAIT:\n");
		sema_s_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
		print_sema_s(addr + FIELD("vfs", "vfs_wait"),
			K_PTR(K, vfsp, "vfs", "vfs_wait"), (flags & ~(C_FULL)), ofp);
		sema_s_banner(ofp, (flags & C_INDENT)|SMINOR);
		fprintf(ofp, "VFS_MAC=0x%llx, VFS_ACL=0x%llx\n",
			kl_kaddr(K, vfsp, "vfs", "vfs_mac"),
			kl_kaddr(K, vfsp, "vfs", "vfs_acl"));
		fprintf(ofp, "VFS_CAP=0x%llx, VFS_INF=0x%llx\n",
			kl_kaddr(K, vfsp, "vfs", "vfs_cap"),
			kl_kaddr(K, vfsp, "vfs", "vfs_inf"));
		fprintf(ofp, "VFS_ALTFSID=0x%llx\n",
			kl_kaddr(K, vfsp, "vfs", "vfs_altfsid"));
		fprintf(ofp, "\n");
	}
	return (1);
}

/*
 * get_vnode()
 *
 *   This routine only returns a pointer to a vnode if the vnode at
 *   addr is valid (there is a positive reference count) or the C_ALL
 *   flag is set.
 */
k_ptr_t
get_vnode(kaddr_t addr, k_ptr_t vbufp, int flags)
{
	k_ptr_t vp;

	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_vnode: addr=0x%llx, vbufp=0x%x, flags=0x%x\n",
			addr, vbufp, flags);
	}

	vp = kl_get_struct(K, addr, VNODE_SIZE(K), vbufp, "vnode");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_VNODE;
	}
	else {
		if (!KL_ERROR) {
			if (KL_INT(K, vp, "vnode", "v_count")) {
				return (vp);
			}
			else if (flags & C_ALL) {
				return (vp);
			}
		}
		addr = kl_bhv_vobj(K, kl_kaddr(K, vbufp, "vnode", "v_bh"));
		KL_SET_ERROR_NVAL(KLE_BAD_VNODE, addr, 2);;
	}
	return ((k_ptr_t)NULL);
}

/*
 * vnode_banner() -- Print out the banner for vnode information.
 */
void
vnode_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"           VNODE  RCNT   TYPE              VFSP       DEV  "
				"                BH\n");
		}
		else {
			fprintf(ofp,
				"   VNODE          RCNT   TYPE              VFSP       DEV  "
				"                BH\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"============================================="
			"================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"---------------------------------------------"
			"--------------------------------\n");
	}
}

/*
 * print_vnode() -- Print out the contents of a vnode at a certain address
 */
int
print_vnode(kaddr_t addr, k_ptr_t vp, int flags, FILE *ofp)
{
	int vfs_type;
	char vfs_name[128];
	kaddr_t vfs_data;
	k_ptr_t vfs_ptr = 0, vfsp, vfsswp;

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp, "  ", addr);
	fprintf(ofp, "%4lld  ", KL_INT(K, vp, "vnode", "v_count"));
	switch (KL_INT(K, vp, "vnode", "v_type")) {
		case 0 :
			fprintf(ofp, " VNON  ");
			break;
		case 1 :
			fprintf(ofp, " VREG  ");
			break;
		case 2 :
			fprintf(ofp, " VDIR  ");
			break;
		case 3 :
			fprintf(ofp, " VBLK  ");
			break;
		case 4 :
			fprintf(ofp, " VCHR  ");
			break;
		case 5 :
			fprintf(ofp, " VLNK  ");
			break;
		case 6 :
			fprintf(ofp, "VFIFO  ");
			break;
		case 7 :
			fprintf(ofp, "VXNAM  ");
			break;
		case 8 :
			fprintf(ofp, " VBAD  ");
			break;
		case 9 :
			fprintf(ofp, "VSOCK  ");
			break;
		default :
			fprintf(ofp, "       ");
			break;
	}
	fprintf(ofp, "%16llx  ", kl_kaddr(K, vp, "vnode", "v_vfsp"));
	fprintf(ofp, "%8llx    ", KL_INT(K, vp, "vnode", "v_rdev"));
	fprintf(ofp, "%16llx\n", kl_kaddr(K, vp, "vnode", "v_bh")); 

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		indent_it(flags, ofp);
		fprintf(ofp, "V_LIST:\n");
		indent_it(flags, ofp);
		fprintf(ofp, "  VL_NEXT=0x%llx, VL_PREV=0x%llx\n",
			kl_kaddr(K, vp, "vnlist", "vl_next"), 
			kl_kaddr(K, vp, "vnlist", "vl_prev"));
		indent_it(flags, ofp);
		fprintf(ofp, "V_FLAG=0x%llx, V_NAMECAP=0x%llx\n",
			KL_UINT(K, vp, "vnode", "v_flag"), 
			KL_UINT(K, vp, "vnode", "v_namecap"));
		indent_it(flags, ofp);
		fprintf(ofp, "V_VFSMOUNTEDHERE=0x%llx, V_STREAM=0x%llx\n",
			kl_kaddr(K, vp, "vnode", "v_vfsmountedhere"),
			kl_kaddr(K, vp, "vnode", "v_stream"));
		indent_it(flags, ofp);
		fprintf(ofp, "V_FILOCKS=0x%llx, V_FILOCKSEM=0x%llx\n",
			kl_kaddr(K, vp, "vnode", "v_filocks"),
			kl_kaddr(K, vp, "vnode", "v_filocksem"));
		indent_it(flags, ofp);
		fprintf(ofp, "V_NUMBER=%llu, V_LISTID=%lld, V_INTPCOUNT=%lld\n",
			KL_UINT(K, vp, "vnode", "v_number"),
			KL_INT(K, vp, "vnode", "v_listid"),
			KL_INT(K, vp, "vnode", "v_intpcount"));
		indent_it(flags, ofp);
		fprintf(ofp, "V_HASHP=0x%llx, V_HASHN=0x%llx\n",
			kl_kaddr(K, vp, "vnode", "v_hashp"), 
			kl_kaddr(K, vp, "vnode", "v_hashn"));
		indent_it(flags, ofp);
		fprintf(ofp, "  V_PGCNT=%lld, V_DBUF=%lld, V_DPAGES=0x%llx\n",
			KL_INT(K, vp, "vnode", "v_pgcnt"), 
			KL_INT(K, vp, "vnode", "v_dbuf"),
			kl_kaddr(K, vp, "vnode", "v_dpages"));
		indent_it(flags, ofp);
		fprintf(ofp, "  V_BUF=0x%llx, V_BUF_LOCK=0x%llx, V_BUFGEN=%lld\n",
			kl_kaddr(K, vp, "vnode", "v_buf"), 
			kl_kaddr(K, vp, "vnode", "v_buf_lock"),
			KL_INT(K, vp, "vnode", "v_bufgen"));
	}

	if (flags & C_NEXT) {
		fprintf(ofp, "\n");
		vfs_type = KL_INT(K, vp, "vnode", "v_type");
		if (!(vfs_data = kl_bhv_pdata(K, kl_kaddr(K, vp, "vnode", "v_bh")))) {
			return(0);
		}
		flags |= C_INDENT;

		switch (vfs_type) {

			case VCHR:
			  {
			    k_uint_t flag;
			    
#define SCOMMON 0x20 /* from fs/specfs/spec_csnode.h */
			    
			    kl_get_block(K,vfs_data + 
					 kl_member_offset(K,"csnode", 
							  "cs_flag"),
					 8, (k_ptr_t)&flag, 
					 "cs_flag");
			    if (KL_ERROR) {
			      return(0);
			    }
			    if (flag & SCOMMON) {
			      vfs_ptr = alloc_block(CSNODE_SIZE(K), B_TEMP);
			      kl_get_struct(K,vfs_data, CSNODE_SIZE(K), 
					    vfs_ptr,"csnode");
			      /* 
			       * XXX - Need to have  print_csnode() func
			       */
			      lsnode_banner(ofp, SMINOR);
			      print_csnode(vfs_data, vfs_ptr, flags, ofp);
			    }
			    else {
			      vfs_ptr = alloc_block(LSNODE_SIZE(K), B_TEMP);
			      kl_get_struct(K, vfs_data, LSNODE_SIZE(K), 
					    vfs_ptr, "lsnode");
			      fprintf(ofp, "  ");
			      lsnode_banner(ofp, BANNER);
			      fprintf(ofp, "  ");
			      lsnode_banner(ofp, SMINOR);
			      print_lsnode(vfs_data, vfs_ptr, flags, ofp);
			    }
			    free_block(vfs_ptr);

			    if (vfs_data = kl_kaddr(K, vp, "vnode", 
						    "v_stream")) {
			      vfs_ptr = alloc_block(STDATA_SIZE(K), B_TEMP);
			      kl_get_struct(K, vfs_data, STDATA_SIZE(K), 
					    vfs_ptr, "stdata");
			      fprintf(ofp, "\n");
			      stream_banner(ofp, BANNER|SMINOR|C_INDENT);
			      print_stream(vfs_data, vfs_ptr, 0, 
					   flags|C_INDENT, ofp);
			      free_block(vfs_ptr);
			    }
			    break;
			  }
			default:

				vfsp = alloc_block(VFS_SIZE(K), B_TEMP);
				get_vfs(kl_kaddr(K, vp, "vnode", "v_vfsp"), vfsp, flags);
				if (KL_ERROR) {
					kl_print_error(K);
					free_block(vfsp);
					return(0);
				}

				vfsswp = (k_ptr_t)((unsigned)_vfssw + 
					(VFSSW_SIZE(K) * KL_INT(K, vfsp, "vfs", "vfs_fstype")));
				free_block(vfsp);

				kl_get_block(K, kl_kaddr(K, vfsswp, "vfssw", "vsw_name"), 128,
					vfs_name, "vfs_name");

				if (!strcmp(vfs_name, FSID_NFS)) {
					vfs_ptr = alloc_block(RNODE_SIZE(K), 
							      B_TEMP);
					kl_get_struct(K, vfs_data, 
						RNODE_SIZE(K), vfs_ptr, "rnode");
					fprintf(ofp, "  ");
					rnode_banner(ofp, BANNER);
					fprintf(ofp, "  ");
					rnode_banner(ofp, SMINOR);
					print_rnode(vfs_data, vfs_ptr, flags, ofp);
					free_block(vfs_ptr);
				} 
				else if (!strcmp(vfs_name, FSID_EFS)) {
					vfs_ptr = alloc_block(INODE_SIZE(K), B_TEMP);
					kl_get_struct(K, vfs_data, 
						INODE_SIZE(K), vfs_ptr, "inode");
					fprintf(ofp, "  ");
					inode_banner(ofp, BANNER);
					fprintf(ofp, "  ");
					inode_banner(ofp, SMINOR);
					print_inode(vfs_data, vfs_ptr, flags, ofp);
					free_block(vfs_ptr);
				} 
				else if (!strcmp(vfs_name, FSID_XFS)) {
					vfs_ptr = alloc_block(XFS_INODE_SIZE(K), B_TEMP);
					kl_get_struct(K, vfs_data, 
						XFS_INODE_SIZE(K), vfs_ptr, "inode");
					fprintf(ofp, "  ");
					xfs_inode_banner(ofp, BANNER);
					fprintf(ofp, "  ");
					xfs_inode_banner(ofp, SMINOR);
					print_xfs_inode(vfs_data, vfs_ptr, flags, ofp);
					free_block(vfs_ptr);
				} 
				else if (!strcmp(vfs_name, FSID_FD)) {
				} 
				else if (!strcmp(vfs_name, FSID_NAMEFS)) {
				} 
				else if (!strcmp(vfs_name, FSID_FIFOFS)) {
				} 
				else if (!strcmp(vfs_name, FSID_DOS)) {
				} 
				else if (!strcmp(vfs_name, FSID_ISO9660)) {
				} 
				else if (!strcmp(vfs_name, FSID_PROCFS)) {
				} 
				else if (!strcmp(vfs_name, FSID_MAC)) {
				} 
				break;
		}
	}
	return(1);
}

/*
 * Look through the pfdat chain to see if the pgno exists. Print the
 * pfdat if it matches pgno.
 */
int print_pfdat_hchain(kaddr_t *KPpfd, k_ptr_t Ppfd, unsigned int pgno, 
		       int flags, FILE *ofp)
{
	int pfdat_count = 0;
	int Itemp;

	if(Ppfd == NULL)
		return -1;

	while(*KPpfd && 
	      kl_get_struct(K,*KPpfd,PFDAT_SIZE(K),Ppfd,"pfdat"))
	{
		Itemp = KL_UINT(K, Ppfd, "pfdat", "pf_pageno");
		if(Itemp == pgno)
		{
			indent_it(flags, ofp);
			indent_it(flags, ofp);
			fprintf(ofp,"PFDAT=");
			FPRINTF_KADDR(ofp," ",*KPpfd);
			fprintf(ofp,"PGNO=%d\n",pgno);
			if(flags & C_NEXT)
			{
				pfdat_banner(ofp,SMINOR|BANNER);
				print_pfdat(*KPpfd,Ppfd,flags,ofp);
				pfdat_banner(ofp,SMINOR);
			}
			pfdat_count++;
		}
		*KPpfd = kl_kaddr(K,Ppfd,"pfdat","pf_hchain");
	}
	
	return pfdat_count;
}

/*
 * Look through the pfdat chain to see if the pgno exists. Print the
 * pfdat if it matches pgno.
 */
int print_pfdat_hchain_ignorepgno(kaddr_t *KPpfd, k_ptr_t Ppfd, 
				   unsigned int pgno, 
				   int flags, FILE *ofp)
{
	int pfdat_count = 0;
	int Itemp;

	if(Ppfd == NULL)
		return -1;

	while(*KPpfd && 
	      kl_get_struct(K,*KPpfd,PFDAT_SIZE(K),Ppfd,"pfdat"))
	{
		Itemp = KL_UINT(K, Ppfd, "pfdat", "pf_pageno");
		indent_it(flags, ofp);
		indent_it(flags, ofp);
		fprintf(ofp,"PFDAT=");
		FPRINTF_KADDR(ofp," ",*KPpfd);
		fprintf(ofp,"PGNO=%d\n",Itemp);
		if(flags & C_NEXT)
		{
			pfdat_banner(ofp,SMINOR|BANNER);
			print_pfdat(*KPpfd,Ppfd,flags,ofp);
			pfdat_banner(ofp,SMINOR);
		}
		pfdat_count++;
		*KPpfd = kl_kaddr(K,Ppfd,"pfdat","pf_hchain");
	}
	
	return pfdat_count;
}


/*
 * Hash table stuff for the page cache in the anonymous object. Given an 
 * index into the hash table array return the head of the linked list.
 */
kaddr_t get_head_addr(kaddr_t KPcache, k_ptr_t Pcache,kaddr_t KPptr,int index)
{
	
	int Itemp = KL_INT(K,Pcache,"pcache","pc_size");


	KPptr = KPcache + FIELD("pcache","pc_un");

	if(Itemp == 1)
	{
		return KPptr;
	}
	else
	{
		if(kl_get_kaddr(K,KPptr,&KPptr,"pc_bucket"))
		{
			return KPptr + index * K_NBPW(K);
		}
	}
				
	return 0;
}

/*
 * Hash table stuff for the page cache in the anonymous object. Given a
 * page no. (pgno in the pfdat) return the head of the linked list containing
 * the page no.
 */
kaddr_t phash_head_addr(kaddr_t KPcache, k_ptr_t Pcache,kaddr_t KPptr,int pgno)
{
	int Itemp = KL_INT(K,Pcache,"pcache","pc_size");
	
	return get_head_addr(KPcache,Pcache,KPptr,(pgno & (Itemp-1)));
}


/*
 * print_pcache() -- Print out the contents of a pcache at a certain address
 * Like idbg ... we also print pfdat's in ascending order of their pgno's.
 */
#define MAX_PCACHE_PAGES_TO_DISPLAY 25
#define IS_LINK_INDEX(ptr)      (((__psunsigned_t)ptr) <= MAX_BUCKETS)
#define LINK_INDEX(ptr)         ((__psint_t)(ptr) - 1)
#define MAX_BUCKETS             (16*1024*1024)  /* 16 million */
#define GET_HEAD_ADDR           get_head_addr
#define PHASH_HEAD_ADDR         phash_head_addr	
int
print_pcache_ignorepgno(kaddr_t addr, k_ptr_t vp, int flags, FILE *ofp)
{
	kaddr_t KPptr0,KPptr1;
	k_ptr_t Pptr0, Pptr1;
	int i=0,pcache_size=kl_struct_len(K,"pcache"),pgno;
	int pfdat_count=0;
	int max_pgno = 0;
	int maxpages;
	int Itemp;

	indent_it(flags, ofp);
	fprintf(ofp,"PCACHE=");
	FPRINTF_KADDR(ofp,"\n",addr);
	/*
	 * Do we have more than we can handle... If so, just print a nice 
	 * message and return.
	 */
	if(!((max_pgno=KL_INT(K,vp,"pcache","pc_count")) < 
	     MAX_PCACHE_PAGES_TO_DISPLAY) &&
	   !(flags & C_FULL))
	{
		indent_it(flags, ofp);
		fprintf(ofp,
			"...\n");
		indent_it(flags, ofp);
		fprintf(ofp,
			"Too many pages, Use -a flag to display all of "
			"them.\n");
		indent_it(flags, ofp);
		fprintf(ofp,
			"...\n");
		return 0;
	}

	if(max_pgno)
	{
		indent_it(flags, ofp);
		fprintf(ofp,"Printing pfdat's in non-sorted order w.r.t "
			"pfdat.pf_pageno.\n");
	}

	Pptr0=alloc_block(PFDAT_SIZE(K),B_TEMP);
	if(KL_INT(K,vp,"pcache","pc_size") == 1)
	{
		/*
		 * Linked list.
		 */
		
		/*
		 * First pfdat in chain.
		 */
		KPptr0 = kl_kaddr(K,vp,"pcache","pc_un");
		KPptr1 = KPptr0;
		Itemp  = print_pfdat_hchain_ignorepgno(&KPptr1,
						       Pptr0,0,
						       flags,ofp);
		max_pgno    -= Itemp;
		pfdat_count += Itemp;
	}
	else
	{
	       /*
		* Hash table.
		*/
		maxpages = (PTRSZ64(K) ?
			    Btoc(K,KL_HIUSRATTACH_64) :
			    Btoc(K,KL_HIUSRATTACH_32));
		for(pgno=0;pgno < KL_INT(K,vp,"pcache","pc_size");pgno++)
		{
			KPptr0 = get_head_addr(addr,vp,KPptr0,pgno);
			kl_get_kaddr(K,KPptr0,&KPptr0,"pfdat");
			if(KPptr0 && 
			   (Itemp=print_pfdat_hchain_ignorepgno(&KPptr0,Pptr0,
								pgno,
								flags,ofp)))
			{
				/*
				 * We don't get out of the loop as 
				 * soon as we find our pfdat 
				 * because there may be pfdat's with
				 * duplicate pgno's.. -- see
				 * comment in function pcache_next.
				 */
				pfdat_count +=Itemp;
				max_pgno    -=Itemp;
			}
			if (DEBUG(DC_FUNCTRACE,3))
			{
				fprintf(ofp,"MAX_PGNO=%lld, PFDAT_COUNT=%d, "
					"PGNO=%d\n",
					KL_INT(K,vp,"pcache","pc_count"),
					pfdat_count,pgno);
			}

		} /* max_pgno */
	}

	free_block(Pptr0);

	if(max_pgno > 0)
	{
		indent_it(flags|C_INDENT, ofp);
		fprintf(ofp,"ERROR: Could not find %d pfdat's\n",max_pgno);
	}
	indent_it(flags|C_INDENT, ofp);
	PLURAL("pfdat", pfdat_count, ofp);
	return pfdat_count;
}

int
print_pcache(kaddr_t addr, k_ptr_t vp, int flags, FILE *ofp)
{
	kaddr_t KPptr0,KPptr1;
	k_ptr_t Pptr0, Pptr1;
	int i=0,pcache_size=kl_struct_len(K,"pcache"),pgno;
	int pfdat_count=0;
	int max_pgno = 0;
	int maxpages;
	int Itemp;

	indent_it(flags, ofp);
	fprintf(ofp,"PCACHE=");
	FPRINTF_KADDR(ofp,"\n",addr);
	/*
	 * Do we have more than we can handle... If so, just print a nice 
	 * message and return.
	 */
	if(!((max_pgno=KL_INT(K,vp,"pcache","pc_count")) < 
	     MAX_PCACHE_PAGES_TO_DISPLAY) &&
	   !(flags & C_ALL))
	{
		indent_it(flags, ofp);
		fprintf(ofp,
			"...\n");
		indent_it(flags, ofp);
		fprintf(ofp,
			"Too many pages, Use -a flag to display all of "
			"them.\n");
		indent_it(flags, ofp);
		fprintf(ofp,
			"...\n");
		return 0;
	}

	if(max_pgno)
	{
		indent_it(flags, ofp);
		fprintf(ofp,"Printing pfdat's in sorted order w.r.t "
			"pfdat.pf_pageno.\n");
	}

	Pptr0=alloc_block(PFDAT_SIZE(K),B_TEMP);
	if(KL_INT(K,vp,"pcache","pc_size") == 1)
	{
		/*
		 * Linked list.
		 */
		
		/*
		 * First pfdat in chain.
		 */
		KPptr0 = kl_kaddr(K,vp,"pcache","pc_un");
		for(pgno=0;max_pgno;pgno++)
		{
			KPptr1 = KPptr0;
			Itemp  = print_pfdat_hchain(&KPptr1,Pptr0,pgno, flags,
						    ofp);
			max_pgno    -= Itemp;
			pfdat_count += Itemp;
		}
	}
	else
	{
	       /*
		* Hash table.
		*/
		maxpages = (PTRSZ64(K) ?
			    Btoc(K,KL_HIUSRATTACH_64) :
			    Btoc(K,KL_HIUSRATTACH_32));
		for(pgno=0;(max_pgno && (pgno != (maxpages + 1)));pgno++)
		{
			KPptr0 = PHASH_HEAD_ADDR(addr,vp,KPptr0,pgno);
pfdat_is_link:
			kl_get_kaddr(K,KPptr0,&KPptr0,"pfdat");
			if(KPptr0 && 
			   (Itemp=print_pfdat_hchain(&KPptr0,Pptr0,pgno,
						     flags,ofp)))
			{
				/*
				 * We don't get out of the loop as 
				 * soon as we find our pfdat 
				 * because there may be pfdat's with
				 * duplicate pgno's.. -- see
				 * comment in function pcache_next.
				 */
				pfdat_count +=Itemp;
				max_pgno    -=Itemp;
			}
			/*
			 * print_pfdat_hchain changes KPptr0.
			 */
			if(KPptr0 && IS_LINK_INDEX(KPptr0))
			{
				Itemp  = LINK_INDEX(KPptr0);
				KPptr0 = GET_HEAD_ADDR(addr,vp,KPptr0,
						       Itemp);
				goto pfdat_is_link;
			}
			if (DEBUG(DC_FUNCTRACE,3))
			{
				fprintf(ofp,"MAX_PGNO=%lld, PFDAT_COUNT=%d, "
					"PGNO=%d\n",
					KL_INT(K,vp,"pcache","pc_count"),
					pfdat_count,pgno);
			}

		} /* max_pgno */
	}

	free_block(Pptr0);

	indent_it(flags|C_INDENT, ofp);
	PLURAL("pfdat", pfdat_count, ofp);

	return pfdat_count;
}


#define MAX_SCACHE_PAGES_TO_DISPLAY 25
/*
 * Below comes straight from scache_mgr.h
 */
#define SC_NONE            0       /* no cache allocated yet               */
#define SC_ARRAY           1       /* cache is organized as linear array   */
#define SC_HASH            2       /* cache is organized as hash table     */
#define SM_SWAPHANDLE_SIZE 4       /* 
				    *  Not visible to us, but typedef'd to 
				    *  __uint32_t .. sizeof(__uint32_t)
				    */
#define HANDLES_PER_BLOCK       ((2*1024)/SM_SWAPHANDLE_SIZE)
#define LPN_TO_BLOCK_INDEX(lpn) ((lpn) & (HANDLES_PER_BLOCK - 1))

/*
 * The following macro and function are exact copies of their namesakes 
 * in the kernel.
 */
#define LPN_IN_BLOCK(block, lpn)                                        \
   (((lpn) >= KL_INT(K,block,"hashblock","hb_lpn")) &&                  \
    ((lpn) <  KL_INT(K,block,"hashblock","hb_lpn")+HANDLES_PER_BLOCK))
#define LPN_TO_BLOCK_START(lpn) ((lpn) & ~((HANDLES_PER_BLOCK - 1)))
	
kaddr_t scache_find_block(kaddr_t KPscache, k_ptr_t Pscache, k_ptr_t Pblock,
			  int lpn)
{
	kaddr_t     KPblock;
	k_uint_t    sc_size;
	int hash_index;

	if((Pblock == NULL) || 
	   base_value(Pscache,"scache","sc_size",&sc_size) == -1)
	{
		/*
		 * Called sc_maxlpn in scache_mgr.h
		 */
		return -1;
	}
	
	/*
	 * SHASH...
	 */
	if(sc_size == 1) 
	{
		KPblock = kl_kaddr(K,Pscache,"scache","sc_un");
	}
	else
	{
		KPblock=kl_kaddr(K,Pscache,"scache","sc_un");
		hash_index = ((LPN_TO_BLOCK_START(lpn) / HANDLES_PER_BLOCK) &
			      (sc_size -1));
		KPblock += (__uint64_t)(hash_index * K_NBPW(K));
		if(!kl_get_kaddr(K,KPblock,&KPblock,"hashblock"))
		{
			return -1;
		}
	}
	
	while (KPblock && kl_get_struct(K,KPblock,
					kl_struct_len(K,"hashblock"),
					Pblock,"hashblock"))
	{
		if (LPN_IN_BLOCK(Pblock, lpn))
			return KPblock;
		
		KPblock = kl_kaddr(K,Pblock,"hashblock","hb_next");
	}
	
	return NULL;
}

/*
 * Handle printing the hash table for the scache.. once we have the 
 * hashblock pointer...
 *
 * Mainly moved it to a separate function because indentation was getting
 * to be a pain.
 */
int print_scache_bucket(kaddr_t KPblock, k_ptr_t Pblock, int lpn, int flags, 
			FILE *ofp)
{
	int swap_handle_count = 0;
	kaddr_t KPptr;
	__uint32_t swaphandle;
	int j;
	int hb_count;

	if(Pblock == NULL)
		return -1;

	hb_count = KL_INT(K,Pblock,"hashblock","hb_count");
	for(j=0;j<HANDLES_PER_BLOCK;j++)
	{
		
		KPptr=ADDR(K,KPblock, "hashblock", "hb_sh");
		KPptr=(kaddr_t)
			((__uint64_t)KPptr + 
			 (LPN_TO_BLOCK_INDEX(lpn+j) * SM_SWAPHANDLE_SIZE));
		
		if(kl_get_block(K,KPptr,SM_SWAPHANDLE_SIZE,&swaphandle,
				"sm_swaphandle_t")
		   && swaphandle)
		{
			indent_it(flags, ofp);
			indent_it(flags, ofp);
			fprintf(ofp,"LPN=%d SWAP_HANDLE=%x\n",
				(lpn+j),swaphandle);
			swap_handle_count++;
		}
	}
	if (DEBUG(DC_FUNCTRACE,3))
	{
		fprintf(ofp,"Block = %llx HB_COUNT= %d, OUR_COUNT=%d\n",
			KPblock,hb_count,swap_handle_count);
	}
	return swap_handle_count;
}

/*
 * print_scache() -- Print out the contents of a scache at a certain address
 */

int
print_scache(kaddr_t addr, k_ptr_t vp, int flags, FILE *ofp)
{
	kaddr_t KPptr0,KPptr1;
	k_ptr_t Pptr0, Pptr1;
	int i=0,j,lpn;
	k_uint_t scache_mode,sc_size;
	int swap_handle_count=0;
	k_uint_t pgno,max_bucket,max_pgno;
	__uint32_t swaphandle;

	indent_it(flags, ofp);
	fprintf(ofp,"SCACHE=");
	FPRINTF_KADDR(ofp,"\n",addr);

	if ((base_value(vp,"scache","sc_count",&max_bucket) == -1) ||
	    (base_value(vp,"scache","sc_mode",&scache_mode) == -1) ||
	    (base_value(vp,"scache","sc_size",&sc_size) == -1))
	{
		/*
		 * Error!.
		 *
		 * sc_size is called sc_maxlpn in scache_mgr.h
		 */
		return -1;
	}

	/*
	 * Display all the handles...
	 */
	if(scache_mode == SC_NONE)
	{
		goto end;
	}
	else if(scache_mode == SC_ARRAY)
	{
		if(!(sc_size< MAX_SCACHE_PAGES_TO_DISPLAY) && 
		   !(flags & C_ALL || flags & C_FULL))
		{
			/*
			 * Is this a valid check for SC_ARRAY ?..
			 */
			indent_it(flags, ofp);
			fprintf(ofp,
				"...\n");
			indent_it(flags, ofp);
			fprintf(ofp,
				"Too many handles, "
				"Use -a flag to display all of them.\n");
			indent_it(flags, ofp);
			fprintf(ofp,
				"...\n");
			return 0;
		}

		/*
		 * We do not have to worry about sorting here.. since we 
		 * look at it here in sorted order.
		 */
		for(i=0;i<sc_size;i++)
		{
			KPptr0=kl_kaddr(K,vp,"scache","sc_un");
			KPptr0 = (kaddr_t)((__uint64_t)KPptr0 +
					   i*SM_SWAPHANDLE_SIZE);
			/*
			 * kl_get_block returns the 4-byte quantity in the
			 * upper 4-bytes.. like in a character buffer... 
			 * that's why we have to do the shifting and stuff..
			 */
			if(!kl_get_block(K,KPptr0,SM_SWAPHANDLE_SIZE,
					 &swaphandle,"sm_swaphandle_t")
			   || (swaphandle == 0))
			{
				continue;
			}
			indent_it(flags, ofp);
			indent_it(flags, ofp);
			fprintf(ofp,"LPN=%d SWAP_HANDLE=%x\n",
				i,(__uint32_t)swaphandle);
			swap_handle_count++;
		}
	}
	else if(scache_mode == SC_HASH)
	{
		if((max_bucket == 0)) 
		{
			return -1;
		}
		if(!(flags & C_ALL || flags & C_FULL))
		{
			/*
			 * We assume the hash table was created only because
			 * the array got too large.
			 */
			indent_it(flags, ofp);
			fprintf(ofp,
				"...\n");
			indent_it(flags, ofp);
			fprintf(ofp,
				"scache is a hash table, "
				"Use -a flag to display it's entries.\n");
			indent_it(flags, ofp);
			fprintf(ofp,
				"...\n");
			return 0;
		}

		Pptr0=alloc_block(kl_struct_len(K,"hashblock"),B_TEMP);
		max_pgno = (PTRSZ64(K) ? 
			    Btoc(K,KL_HIUSRATTACH_64) : 
			    Btoc(K,KL_HIUSRATTACH_32));
			     
		for(pgno=0;pgno<=max_pgno;)
		{
			KPptr0 = scache_find_block(addr,vp,Pptr0,pgno);
			if(KPptr0 &&
			   kl_get_struct(K,KPptr0,
					 kl_struct_len(K,"hashblock"),Pptr0,
					 "hashblock"))
			{
				swap_handle_count +=
					print_scache_bucket(KPptr0,Pptr0,
							    pgno,flags,
							    ofp);
			}
			pgno += HANDLES_PER_BLOCK;
		} /* max_pgno */
		free_block(Pptr0);
	}
	else
	{
		fprintf(KL_ERRORFP,"Bad scache mode = %llu\n",scache_mode);
	}
	
end:
	indent_it(flags|C_INDENT, ofp);
	PLURAL("swap handle",swap_handle_count, ofp);

	return swap_handle_count;
}

/*
 * Same as find_anonroot.... in vmidbg.c
 */
kaddr_t find_anonroot(kaddr_t anon)
{
	k_ptr_t Panon;
	int anon_size = kl_struct_len(K,"anon");
	kaddr_t KPanon;

	Panon = alloc_block(anon_size,B_TEMP);
	while(kl_get_struct(K,anon,anon_size,Panon,"anon") && 
	      (KPanon = kl_kaddr(K,Panon,"anon","a_parent")))
	{
		anon = KPanon;
	}
	free_block(Panon);
	
	return anon;
		
}


/*
 * get_anon()
 *
 *   This routine only returns a pointer to a anon if the anon at
 *   addr is valid (there is a positive reference count) or the C_ALL
 *   flag is set.
 */
k_ptr_t
get_anon(kaddr_t KPanon, k_ptr_t Panon, int flags)
{
	k_ptr_t vp;

#ifdef ICRASH_DEBUG
	assert(Panon);
#endif	
	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, 
			"get_anon: KPanon=0x%llx, Panon=0x%x, flags=0x%x\n",
			KPanon, Panon, flags);
	}

	vp = kl_get_struct(K, KPanon, kl_struct_len(K,"anon"), Panon, "anon");
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_ANON;
	}
	else {
		if (!KL_ERROR) {
			if (KL_INT(K, vp, "anon", "a_refcnt") > 0) {
				return (vp);
			}
			else if (flags & C_ALL) {
				return (vp);
			}
		}
		KL_SET_ERROR_NVAL(KLE_BAD_ANON, KPanon, 2);
	}
	return ((k_ptr_t)NULL);
}

/*
 * anon_banner() -- Print out the banner for anon information.
 */
void
anon_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"            ANON             LOCK REF  "
				"DEPTH           PARENT PC_COUNT PC_SIZE\n");
		}
		else {
			fprintf(ofp,
				"    ANON             LOCK         REF  "
				"DEPTH   PARENT         PC_COUNT PC_SIZE\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"============================================="
			"==================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"---------------------------------------------"
			"----------------------------------\n");
	}
}


/*
 * print_anon() -- Print out the contents of a anon at a certain address
 */
int
print_anon(kaddr_t addr, k_ptr_t vp, int flags, FILE *ofp)
{
	kaddr_t KPptr0,KPptr1;
	k_ptr_t Pptr0, Pptr1;
	int Itemp;

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp," ",addr);
	FPRINTF_KADDR(ofp," ",kl_kaddr(K,vp,"anon","a_lock"));
	fprintf(ofp,"%3lld %6llx ",
		KL_INT(K,vp,"anon","a_refcnt"),
		KL_INT(K,vp,"anon","a_depth"));
	FPRINTF_KADDR(ofp," ",kl_kaddr(K,vp,"anon","a_parent"));
	fprintf(ofp,"%8lld %7lld\n",
		KL_INT(K,(k_ptr_t)((__psunsigned_t)vp+
				   kl_member_offset(K,"anon",
						    "a_pcache")),
		       "pcache","pc_count"),
		KL_INT(K,(k_ptr_t)((__psunsigned_t)vp+
				   kl_member_offset(K,"anon",
						    "a_pcache")),
		       "pcache","pc_size")
		);
	
	if(flags & (C_FULL|C_NEXT|C_ALL))
	{
		indent_it(flags|C_INDENT, ofp);
		fprintf(ofp,"LEFT=%-llx  ",kl_kaddr(K,vp,"anon","a_left"));
		fprintf(ofp,"RIGHT=%-llx\n",kl_kaddr(K,vp,"anon","a_right")); 

		/*
		 * If both the -a and -f flags are specified... then we will
		 * do the -a route i.e., print the pfdat's in sorted order.
		 */
		if(flags & C_ALL)
		{
			Itemp=print_pcache((kaddr_t)
					   ((__uint64_t)addr +
					    kl_member_offset(K,"anon",
							     "a_pcache")),
					   (k_ptr_t)
					   ((__psunsigned_t)vp+
					    kl_member_offset(K,"anon",
							     "a_pcache")),
					   (flags|C_INDENT),ofp);
		}
		else
		{
			Itemp=print_pcache_ignorepgno((kaddr_t)
						      ((__uint64_t)addr +
					    kl_member_offset(K,"anon",
							     "a_pcache")),
					   (k_ptr_t)
					   ((__psunsigned_t)vp+
					    kl_member_offset(K,"anon",
							     "a_pcache")),
					   (flags|C_INDENT),ofp);
		}


		Itemp=print_scache((kaddr_t)((__uint64_t)addr +
					     kl_member_offset(K,"anon",
							      "a_scache")),
				   (k_ptr_t)((__psunsigned_t)vp+
					     kl_member_offset(K,"anon",
							      "a_scache")),
				   (flags|C_INDENT),ofp);


	}
	return 0;
}

int print_anontree(kaddr_t anon,int *anon_cnt,int flags,FILE *ofp)
{
	k_ptr_t Panon;
	kaddr_t KPleft,KPright;
	
	Panon = alloc_block(kl_struct_len(K,"anon"),B_TEMP);
	
	if(flags & (C_FULL|C_ALL|C_NEXT))
	{
		anon_banner(ofp, BANNER|SMINOR);
	}
	if(!kl_get_struct(K,anon,kl_struct_len(K,"anon"),Panon,"anon"))
	{
		free_block(Panon);
		return -1;
	}
	(*anon_cnt)++;
	print_anon(anon,Panon,flags,ofp);
	if(flags & (C_FULL|C_ALL|C_NEXT))
	{
		anon_banner(ofp, SMINOR);
	}
	KPleft  = kl_kaddr(K,Panon,"anon","a_left");
	KPright = kl_kaddr(K,Panon,"anon","a_right");
	free_block(Panon);

	if(KPleft)
	{
		print_anontree(KPleft,anon_cnt,flags,ofp);
	}
	if(KPright)
	{
		print_anontree(KPright,anon_cnt,flags,ofp);
	}

	return 0;
}

/*
 * zone_banner()
 */
void
zone_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp, "            ZONE                 NAME    SIZE  TOTAL  "
				" FREE  PAGES NODE INDEX\n");
		}
		else {
			fprintf(ofp, "    ZONE                         NAME    SIZE  TOTAL  "
				" FREE  PAGES NODE INDEX\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "================================================"
			"==============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "------------------------------------------------"
			"------------------------------\n");
	}
}


/*
 * Ahem!.... defines from kern_heap.h
 *
 *  Std. reminder: All the below address space macros take arguments which are
 *  pointers in icrash process address space and not in kernel address space.
 *  -- which is the same as k_ptr_t.
 *  We use KL_* for any macro which is also defined by the kernel.
 */

/* types */
#define KL_ZONE_GLOBAL             1 /* global shared zone list    */
#define KL_ZONE_GLOBAL_PRIVATE     2 /* global private zone list   */
#define KL_ZONE_SET                3 /* Zone set type zones        */

/* modes */
#define KL_ZONE_MODEMASK           (0xf0)
#define KL_ZONE_TYPEMASK           (0x0f)
#define KL_ZONE_NOPAGEALLOC        (1 << 4)
#define KL_ZONE_ENABLE_SHAKE       (1 << 5)
#define KL_ZONE_MODE(z)            \
           (kl_uint(K,z,"zone","zone_type",0) & KL_ZONE_MODEMASK)
#define KL_ZONE_TYPE(z)            \
	(kl_uint(K,z,"zone","zone_type",0) & KL_ZONE_TYPEMASK)
	
#define KL_ZONE_MODESET(z, mode)   \
	(kl_uint(K,z,"zone","zone_type") |= (mode & KL_ZONE_MODEMASK))
	
	
/*
 * print_zone() -- Print out specific zone information.
 */
int
print_zone(kaddr_t z, k_ptr_t zbuf, int flags, FILE *ofp)
{
	int i, count = 0, unitsize, freesize;
	kaddr_t bp, nbp;
	char zname[128];

	if(!kl_get_block(K, kl_kaddr(K, zbuf, "zone", "zone_name"), 
			 128, &zname, "zname"))
	{
		return -1;
	}

	if(!(unitsize = KL_INT(K, zbuf, "zone", "zone_unitsize")))
	{
		return -1;
	}
	freesize = KL_INT(K, zbuf, "zone", "zone_free_count");
	FPRINTF_KADDR(ofp, " ", z);
	zname[20]=0;
	fprintf(ofp, "%20s   %5d  ", zname, unitsize);
	fprintf(ofp, "%5lld  %5d  %5lld %4lld  %4lld",
		(KL_INT(K, zbuf, "zone", "zone_total_pages") * K_PAGESZ(K)) / unitsize, 
		freesize, 
		KL_INT(K, zbuf, "zone", "zone_total_pages"),
		KL_INT(K, zbuf, "zone", "zone_node"),
		KL_INT(K, zbuf, "zone", "zone_index"));
	fprintf(ofp,"\n");
	if (flags & C_FULL) {
		/*
		 * Only compare the number of free blocks agianst the block free 
		 * count on non-active systems 
		 */
		fprintf(ofp, "\n");
		fprintf(ofp,"ZONE_TYPE = %x ",(int)KL_ZONE_TYPE(zbuf));

		if (bp = kl_kaddr(K, zbuf, "zone", "zone_free")) {
			count = 1;
			i = 1;
			fprintf(ofp, "\nFREE %d BYTE BLOCKS :\n  ", unitsize); 
			while (bp) {
				if (PTRSZ64(K)) {
					fprintf(ofp, "0x%16llx ", bp);
					if ((i % 4) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				else {
					fprintf(ofp, "0x%8llx ", bp);
					if ((i % 6) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				i++;
				if (!kl_get_kaddr(K, bp, &nbp, "free block")) { 
					if ((i % 4) != 1) {
						fprintf(ofp, "\n");
					}
					fprintf(ofp,
						"  ZONE 0x%llx: 0x%llx is a bad block address!\n",
						z, bp);
					break;
				}
				if (nbp) {
					if (nbp != bp) {
						count++;
						bp = nbp;
					}
					else {
						if ((i % 4) != 1) {
							fprintf(ofp, "\n");
						}
						fprintf(ofp,
							"  ZONE 0x%llx: block 0x%llx points to itself!\n", 
									z, bp);
						break;
					}
				}
				else {
					bp = 0;
				}
			}
		}

		if ((i % 4) != 1) {
			fprintf(ofp, "\n");
		}

	       /*
		* Direct-mapped stuff.
		*/
		if (bp = kl_kaddr(K, zbuf, "zone", "zone_free_dmap")) {
			count = 1;
			i = 1;
			fprintf(ofp, "\nFREE %d BYTE BLOCKS (DIRECT-MAPPED):\n"
				"  ", unitsize); 
			while (bp) {
				if (PTRSZ64(K)) {
					fprintf(ofp, "0x%16llx ", bp);
					if ((i % 4) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				else {
					fprintf(ofp, "0x%8llx ", bp);
					if ((i % 6) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				i++;
				if (!kl_get_kaddr(K, bp, &nbp, "free block")) { 
					if ((i % 4) != 1) {
						fprintf(ofp, "\n");
					}
					fprintf(ofp,
						"  ZONE 0x%llx: 0x%llx is a bad block address!\n",
						z, bp);
					break;
				}
				if (nbp) {
					if (nbp != bp) {
						count++;
						bp = nbp;
					}
					else {
						if ((i % 4) != 1) {
							fprintf(ofp, "\n");
						}
						fprintf(ofp,
							"  ZONE 0x%llx: block 0x%llx points to itself!\n", 
									z, bp);
						break;
					}
				}
				else {
					bp = 0;
				}
			}
		}

		if (freesize != count) {
			if (DEBUG(DC_GLOBAL, 1)) {
				fprintf(ofp,
					"  ZONE 0x%llx: number of free blocks (%d) does not "
					"match free count (%d)\n", z, freesize, count);
			}
			fflush(ofp);
		}


		if ((i % 4) != 1) {
			fprintf(ofp, "\n");
		}

	       /* 
		* pfdat's allocated to this zone.
		*/
		if (bp = kl_kaddr(K, zbuf, "zone", "zone_page_list")) {
			count = 1;
			i = 1;
			fprintf(ofp, "\nLIST OF PFDATS :\n  "); 
			while (bp) {
				if (PTRSZ64(K)) {
					fprintf(ofp, "0x%16llx ", bp);
					if ((i % 4) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				else {
					fprintf(ofp, "0x%8llx ", bp);
					if ((i % 6) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				i++;
				if (!kl_get_kaddr(K, bp, &nbp, "pfdat")) { 
					if ((i % 4) != 1) {
						fprintf(ofp, "\n");
					}
					fprintf(ofp,
						"  ZONE 0x%llx: 0x%llx is a bad pfdat address!\n",
						z, bp);
					break;
				}
				if (nbp) {
					if (nbp != bp) {
						count++;
						bp = nbp;
					}
					else {
						if ((i % 4) != 1) {
							fprintf(ofp, "\n");
						}
						fprintf(ofp,
							"  ZONE 0x%llx: pfdat 0x%llx points to itself!\n", 
									z, bp);
						break;
					}
				}
				else {
					bp = 0;
				}
			}
		}

		if ((i % 4) != 1) {
			fprintf(ofp, "\n");
		}


	       /* 
		* direct-mapped pfdat's allocated to this zone.
		*/
		if (bp = kl_kaddr(K, zbuf, "zone", "zone_dmap_page_list")) {
			count = 1;
			i = 1;
			fprintf(ofp, "\nLIST OF PFDATS (DIRECT-MAPPED):\n  "); 
			while (bp) {
				if (PTRSZ64(K)) {
					fprintf(ofp, "0x%16llx ", bp);
					if ((i % 4) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				else {
					fprintf(ofp, "0x%8llx ", bp);
					if ((i % 6) == 0) {
						fprintf(ofp, "\n  ");
					}
				}
				i++;
				if (!kl_get_kaddr(K, bp, &nbp, "pfdat")) { 
					if ((i % 4) != 1) {
						fprintf(ofp, "\n");
					}
					fprintf(ofp,
						"  ZONE 0x%llx: 0x%llx is a bad pfdat address!\n",
						z, bp);
					break;
				}
				if (nbp) {
					if (nbp != bp) {
						count++;
						bp = nbp;
					}
					else {
						if ((i % 4) != 1) {
							fprintf(ofp, "\n");
						}
						fprintf(ofp,
							"  ZONE 0x%llx: pfdat 0x%llx points to itself!\n", 
									z, bp);
						break;
					}
				}
				else {
					bp = 0;
				}
			}
		}

		if ((i % 4) != 1) {
			fprintf(ofp, "\n");
		}

		fprintf(ofp, "\n");
	}
	return(1);
}

/*
 * vfssw_banner() -- Print out banner information for vfssw structures.
 */
void
vfssw_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp,
			"INDEX      NAME              INIT            "
			"VFSOPS          VNODEOPS      FLAG\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"============================================="
			"==================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"---------------------------------------------"
			"----------------------------------\n");
	}
}

/*
 * print_vfssw() -- Print the VFS table of file system names.
 */
int
print_vfssw(int index, int flags, FILE *ofp)
{
	kaddr_t vfs_ptr;
	k_ptr_t vfsswp;
	char vfs_name[128];

	vfsswp = (k_ptr_t)((unsigned)_vfssw + (VFSSW_SIZE(K) * index));
	vfs_ptr = kl_kaddr(K, vfsswp, "vfssw", "vsw_name");
	kl_get_block(K, vfs_ptr, 128, vfs_name, "vfs_name");
	if (vfs_name[0] != '\0') {
		fprintf(ofp, "%5d  %8s  %16llx  %16llx  %16llx  %8llx\n", 
			index, 
			vfs_name,
			kl_kaddr(K, vfsswp, "vfssw", "vsw_init"),
			kl_kaddr(K, vfsswp, "vfssw", "vsw_vfsops"),
			kl_kaddr(K, vfsswp, "vfssw", "vsw_vnodeops"),
			KL_INT(K, vfsswp, "vfssw", "vsw_flag"));
		return (1);
	}
	else {
		return (0);
	}
}

/*
 * vproc_banner()
 */
void
vproc_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "           VPROC       PID  REF_CNT                BH\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "=====================================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "-----------------------------------------------------\n");
	}
}

/*
 * print_vproc()
 */
int
print_vproc(kaddr_t vproc, k_ptr_t vpbuf, int flags, FILE *ofp)
{
	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %8lld  %7lld  %16llx\n",
		vproc,
		KL_INT(K, vpbuf, "vproc", "vp_pid"),
		KL_INT(K, vpbuf, "vproc", "vp_refcnt"),
		kl_kaddr(K, vpbuf, "vproc", "vp_bhvh"));
	
	if (flags & C_NEXT) {
		kaddr_t bhv, proc;
		k_ptr_t bhvdp, procp;

		fprintf(ofp, "\n");

		bhv = kl_kaddr(K, vpbuf, "vproc", "vp_bhvh");

		proc = kl_bhv_pdata(K, bhv);
		if (KL_ERROR) {
			kl_print_error(K);
			return(0);
		}

		procp = kl_get_proc(K, proc, 2, flags);
		if (KL_ERROR) {
			kl_print_error(K);
			return(0);
		}

		fprintf(ofp, "  ");
		proc_banner(ofp, BANNER);
		fprintf(ofp, "  ");
		proc_banner(ofp, SMINOR);
		flags |= C_INDENT;
		print_proc(proc, procp, flags, ofp);
		fprintf(ofp, "\n");
		free_block(procp);
	}
	return(1);
}

/*
 * vsocket_banner()
 */
void
vsocket_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "         VSOCKET  TYPE                BH     FLAGS\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "==================================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "--------------------------------------------------\n");
	}
}

/*
 * print_vsocket()
 */
int
print_vsocket(kaddr_t vsock, k_ptr_t vbuf, int flags, FILE *ofp)
{
	kaddr_t socket, bhv;
	k_ptr_t sp;

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %4lld  %16llx  %8llx\n",
		vsock,
		KL_INT(K, vbuf, "vsocket", "vs_type"),
		kl_kaddr(K, vbuf, "vsocket", "vs_bh"),
		KL_UINT(K, vbuf, "vsocket", "vs_flags"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
	}

	if (flags & C_NEXT) {
		fprintf(ofp, "\n");
		bhv = kl_kaddr(K, vbuf, "vsocket", "vs_bh");
		socket = kl_bhv_pdata(K, bhv);
		if (KL_ERROR) {
			kl_print_error(K);
			return(0);
		}

		sp = alloc_block(SOCKET_SIZE(K), B_TEMP);
		get_socket(socket, sp, flags);
		if (KL_ERROR) {
			kl_print_error(K);
			free_block(sp);
			return(0);
		}
		fprintf(ofp, "  ");
		socket_banner(ofp, BANNER);
		fprintf(ofp, "  ");
		socket_banner(ofp, SMINOR);
		flags |= C_INDENT;
		print_socket(socket, sp, flags, ofp);
		fprintf(ofp, "\n");
		free_block(sp);
	}
	return(1);
}

/*
 * get_swapinfo() -- Return an allocated swapinfo block.
 */
k_ptr_t
get_swapinfo(kaddr_t addr, k_ptr_t swapbuf, int flags)
{
	if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(KL_ERRORFP, "get_swapinfo: addr=0x%llx, swapbuf=0x%x, "
			"flags=0x%x\n", addr, swapbuf, flags);
	}

    kl_get_struct(K, addr, SWAPINFO_SIZE(K), swapbuf, "swapinfo");
	if (KL_ERROR) {
		KL_SET_ERROR_NVAL(KLE_BAD_SWAPINFO, addr, 2);
    	return ((k_ptr_t)0);
	}
	else {
        return (swapbuf);
    }
}

/*
 * swapinfo_banner()
 */
void
swapinfo_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp,
			"         SWAPDEV            VNODE PRI DEV     "
			"NPGS    NFPGS   MAXPGS     VPGS\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"======================================="
			"======================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"---------------------------------------"
			"--------------------------------------\n");
	}
}

/* 
 * print_swapinfo()
 */
int
print_swapinfo(kaddr_t addr, k_ptr_t swapbuf, int flags, FILE *ofp)
{
	char swapname[128];

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_swapinfo: addr=0x%llx, flags=0x%x\n",
				addr, flags);
	}

	fprintf(ofp, "%16llx %16llx   %lld %lld   %8lld %8lld %8lld %8lld\n",
		addr, 
		kl_kaddr(K, swapbuf, "swapinfo", "st_vp"),
		KL_INT(K, swapbuf, "swapinfo", "st_pri"),
		KL_INT(K, swapbuf, "swapinfo", "st_lswap"),
		KL_INT(K, swapbuf, "swapinfo", "st_npgs"),
		KL_INT(K, swapbuf, "swapinfo", "st_nfpgs"),
		KL_INT(K, swapbuf, "swapinfo", "st_maxpgs"),
		KL_INT(K, swapbuf, "swapinfo", "st_vpgs"));

	if (flags & C_FULL) {
		bzero(&swapname, 128);
		if (kl_get_block(K, kl_kaddr(K, swapbuf, "swapinfo", "st_name"), 128,
					swapname, "swapname")) {
			fprintf(ofp, "\nNAME = %s\n", swapname);
		}
	}
	return (1);
}

/* 
 * uthread_s_banner()
 */
void
uthread_s_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		fprintf(ofp, "         UTHREAD         PID              "
					 "NEXT              PREV  NAME\n");
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"=================================================="
			"============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp,
			"---------------------------------------------------"
			"----------------------------\n");
	}
}

/*
 * print_uthread_s() 
 *
 *   Print out selected fields from the uthread_s struct and check the flags
 *   options for full print status. 
 */
/*
 * Scheduling Policies
 */
#define SCHED_FIFO      1
#define SCHED_RR        2
#define SCHED_TS        3
#define SCHED_NP        4
int
print_uthread_s(kaddr_t uthread, k_ptr_t utp, int flags, FILE *ofp)
{
	kaddr_t proc;
	k_ptr_t kt_name, procp;
	char c;

    if (DEBUG(DC_FUNCTRACE, 2)) {
		fprintf(ofp, "print_uthread_s: uthread=0x%llx, utp=0x%x, flags=0x%x, "
			"ofp=0x%x\n", uthread, utp, flags, ofp);
	}

	indent_it(flags, ofp);
	fprintf(ofp, "%16llx  %10d  %16llx  %16llx  %s\n",
		uthread, 
		kl_uthread_to_pid(K, utp),
		kl_kaddr(K, utp, "uthread_s", "ut_next"),
		kl_kaddr(K, utp, "uthread_s", "ut_prev"),
		kl_kthread_name(K, utp));

	if (flags & C_FULL) {
		fprintf(ofp,"\n");
		indent_it(flags, ofp);
		c = *CHAR(K,utp,"uthread_s","ut_policy");
		fprintf(ofp,"UT_POLICY(%1d)=%s, ",c,
			((c == SCHED_FIFO) ? "SCHED_FIFO" :
			 (c == SCHED_RR)   ? "SCHED_RR"   :
			 (c == SCHED_TS)   ? "SCHED_TS"   :
			 (c == SCHED_NP)   ? "SCHED_NP"   :
			 "UNKNOWN"));
		fprintf(ofp,"UT_TSLICE=%lld, UT_RTICKS=%lld, ",
			KL_INT(K,utp,"uthread_s","ut_tslice"),
			KL_INT(K,utp,"uthread_s","ut_rticks"));
		fprintf(ofp,"UT_JOB=%llx, ",
			kl_kaddr(K, utp, "uthread_s", "ut_job"));
		fprintf(ofp,"UT_STACK=%llx\n",
			KL_PHYS_TO_K0(kl_virtop(K, K_KERNELSTACK(K), utp)));

		list_uthread_pregions(uthread, 2, (flags & (~C_FULL)), ofp);
		fprintf(ofp, "\n");
	}

	if (flags & C_NEXT) {
		if (proc = kl_kaddr(K, utp, "uthread_s", "ut_proc")) {
			procp = kl_get_proc(K, proc, 2, flags);
			if (KL_ERROR) {
				kl_print_error(K);
			}
			else {
				fprintf(ofp, "\n");
				proc_banner(ofp, BANNER|SMINOR);
				print_proc(proc, procp, flags, ofp);
			}
			free_block(procp);
		}
		fprintf(ofp, "\n");
	}
	return(1);
}

/*
 * xfs_inode_banner() -- Print out banner information for inode structure.
 */
void
xfs_inode_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp, "       XFS_INODE    UID    GID"
				"             VNODE               SIZE\n");
		}
		else {
			fprintf(ofp, "   XFS_INODE        UID    GID"
				"             VNODE               SIZE\n");
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "=================================="
			"=================================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "----------------------------------"
			"---------------------------------\n");
	}
}

/*
 * print_xfs_inode() -- Print out the xfs_inode information.
 */
int
print_xfs_inode(kaddr_t addr, k_ptr_t xip, int flags, FILE *ofp)
{
	k_ptr_t dip;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_inode: addr=0x%llx, xip=0x%x, flags=0x%x\n",
				addr, xip, flags);
	}

	indent_it(flags, ofp);
	FPRINTF_KADDR(ofp, "  ", addr);
	dip = (k_ptr_t)((unsigned)xip + FIELD("xfs_inode", "i_d"));

	fprintf(ofp, "%5llu  %5llu  ", 
		KL_UINT(K, dip, "xfs_dinode_core", "di_uid"),
		KL_UINT(K, dip, "xfs_dinode_core", "di_gid"));

	fprintf(ofp, "%16llx           ", kl_bhvp_vobj(K, XFS_INODE_TO_BHP(xip)));
	fprintf(ofp, "%8lld\n", KL_INT(K, dip, "xfs_dinode_core", "di_size"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
	}
	return (1);
}


/*
 * Found something on the net. Seems to work pretty well too.
 */
uint64_t
fnv_hash(uchar_t *key, int len)
{
	uint32_t  val;                    /* current hash value */
	uchar_t   *key_end = key + len;
	
	/*
	 * Fowler/Noll/Vo hash - hash each character in the string
	 *
	 * The basis of the hash algorithm was taken from an idea
	 * sent by Email to the IEEE Posix P1003.2 mailing list from
	 * Phong Vo (kpv@research.att.com) and Glenn Fowler
	 * (gsf@research.att.com).
	 * Landon Curt Noll (chongo@toad.com) later improved on their
	 * algorithm to come up with Fowler/Noll/Vo hash.
	 *
	 */
	for (val = 0; key < key_end; ++key) {
		val *= 16777619;
		val ^= *key;
	}
	
	/* our hash value, (was: mod the hash size) */
	return (uint64_t) val;
}


int vfile_hash(kaddr_t key)
{
	return (fnv_hash((uchar_t *) (PTRSZ32(K) ? 
				      ((__psunsigned_t)&key+4) : 
				      ((__psunsigned_t)&key)),
			 K_NBPW(K))) % VFILE_HASHSIZE;
}

/*
 * We use this for binary trees. Once the btnode stuff changes are made.
 */
int val64_compare(list_of_ptrs_t *list1,list_of_ptrs_t *list2) {
	if(list1->val64 == list2->val64)
		return 0;

        /*
	 * For our use it doesn't matter when we return -1 or 1...
	 */
	else if(list1->val64 < list2->val64)
		return -1;
	else 
		return 1;
}


/*
 * At a high level, 
 * This function returns all vsockets it can find in the corefile .... each 
 * vsocket which returns true for the given function test_vsocket.
 *
 * At a lower level,
 * It returns a pointer to a hash table. The hash table is basically an array
 * of pointers to the root of binary trees. Each key in the binary tree is
 * a pointer to a kaddr_t (kernel pointer to a vsocket). The hash table itself 
 * is also keyed on the same kaddr_t (value of the kernel pointer to the 
 * vsocket). 
 * The lower level implementation will change soon to use the generic 
 * find_btnode, insert_btnode functions.
 *
 */
int find_all_vsockets(int flags, 
		      FILE *ofp,
		      /*
		       * This is the function, which if specified, will be 
		       * called on every vsocket we find.
		       * The vsocket will be included in the list of 
		       * vsockets only if this function returns true.
		       *
		       */
		      int (*test_vsocket)(kaddr_t,kaddr_t,char *,int ,int ,
					  FILE *,list_of_ptrs_t *,int *),
		      k_ptr_t **PPhashbase)
{
	int i,kthread_cnt;
	k_ptr_t pidp, vprocp, procp, utp;
	kaddr_t pid, vproc, bhv, proc, kthread;

	*PPhashbase=(k_ptr_t *)alloc_block(VFILE_HASHSIZE*sizeof(k_ptr_t *),
					   B_TEMP);
	bzero(*PPhashbase,VFILE_HASHSIZE*sizeof(k_ptr_t *));

	pidp = alloc_block(PID_ENTRY_SIZE(K), B_TEMP);
	vprocp = alloc_block(VPROC_SIZE(K), B_TEMP);
	pid = K_PIDACTIVE(K);

	/* We have to step over the first entry as it is just
	 * the list head.
	 */
	kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");

	do {
		kl_get_struct(K, pid, PID_ENTRY_SIZE(K), pidp, "pid_entry");
		vproc = kl_kaddr(K, pidp, "pid_entry", "pe_vproc");
		kl_get_struct(K, vproc, VPROC_SIZE(K), vprocp, "vproc");
		bhv = kl_kaddr(K, vprocp, "vproc", "vp_bhvh");
		proc = kl_bhv_pdata(K, bhv);
		if (KL_ERROR) {
			K_BLOCK_FREE(K)(NULL,pidp);
			K_BLOCK_FREE(K)(NULL,vprocp);
			kl_print_error(K);
			return -1;
		}
		
		/*
		 * Let's find the vsockets..
		 */
		findall_proc_vfiles(proc,2,flags,ofp,*PPhashbase,
				    test_vsocket);
		kl_get_kaddr(K, pid, (k_ptr_t)&pid, "pid_entry");
	} while (pid != K_PIDACTIVE(K));
	K_BLOCK_FREE(K)(NULL,pidp);
	K_BLOCK_FREE(K)(NULL,vprocp);
	return 0;
}
