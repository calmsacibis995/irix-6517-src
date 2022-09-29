#ident "$Header: "

/* PTE bit masks and shift values (handles 32-bit and 64-bit PTEs)
 */
typedef struct pdeinfo_s {
	uint64     		pfn_mask;       /* to mask out all but pfn in pte        */
	int32        	pfn_shift;      /* to right align pfn                    */
	uint64     		Pde_pg_vr;      /* pte "valid" mask                      */
	int32      	  	pg_vr_shift;    /* to right align "valid" bits           */
	uint64     		Pde_pg_g;       /* pte "global" mask                     */
	int32        	pg_g_shift;     /* to right align "global" bits          */
	uint64     		Pde_pg_m;       /* pte "modified" mask                   */
	int32        	pg_m_shift;     /* to right align "modified" bits        */
	uint64     		Pde_pg_n;       /* pte "noncached" mask                  */
	int32        	pg_n_shift;     /* to right align "noncached" bits       */
	uint64     		Pde_pg_sv;      /* pte "page valid" mask                 */
	int32        	pg_sv_shift;    /* to right align "page valid" bits      */
	uint64     		Pde_pg_d;       /* pte "page dirty" mask                 */
	int32        	pg_d_shift;     /* to right align "page dirty" bits      */
	uint64     		Pde_pg_eop;     /* pte "eop" mask                        */
	int32        	pg_eop_shift;   /* to right align "eop" bits             */
	uint64     		Pde_pg_nr;      /* pte "reference count" mask            */
	int32        	pg_nr_shift;    /* to right align "reference count" bits */
	uint64     		Pde_pg_cc;      /* pte "cache coherency" mask            */
	int32        	pg_cc_shift;    /* to right align "cache coherency" bits */
} pdeinfo_t;

/* From the pdeinfo_s struct
 */
#define K_PFN_MASK   			(LIBKERN_DATA->k_pdeinfo->pfn_mask)
#define K_PFN_SHIFT   			(LIBKERN_DATA->k_pdeinfo->pfn_shift)
#define K_PDE_PG_VR   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_vr)
#define K_PG_VR_SHIFT   		(LIBKERN_DATA->k_pdeinfo->pg_vr_shift)
#define K_PDE_PG_G   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_g)
#define K_PG_G_SHIFT   			(LIBKERN_DATA->k_pdeinfo->pg_g_shift)
#define K_PDE_PG_M   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_m)
#define K_PG_M_SHIFT   			(LIBKERN_DATA->k_pdeinfo->pg_m_shift)
#define K_PDE_PG_N   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_n)
#define K_PG_N_SHIFT   			(LIBKERN_DATA->k_pdeinfo->pg_n_shift)
#define K_PDE_PG_SV   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_sv)
#define K_PG_SV_SHIFT   		(LIBKERN_DATA->k_pdeinfo->pg_sv_shift)
#define K_PDE_PG_D   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_d)
#define K_PG_D_SHIFT   			(LIBKERN_DATA->k_pdeinfo->pg_d_shift)
#define K_PDE_PG_EOP   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_eop)
#define K_PG_EOP_SHIFT   		(LIBKERN_DATA->k_pdeinfo->pg_eop_shift)
#define K_PDE_PG_NR   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_nr)
#define K_PG_NR_SHIFT   		(LIBKERN_DATA->k_pdeinfo->pg_nr_shift)
#define K_PDE_PG_CC   			(LIBKERN_DATA->k_pdeinfo->Pde_pg_cc)
#define K_PG_CC_SHIFT   		(LIBKERN_DATA->k_pdeinfo->pg_cc_shift)
