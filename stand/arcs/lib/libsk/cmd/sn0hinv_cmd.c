
#ifdef SN0
/***********************************************************************\
*       File:           sn0hinv_cmd.c                                   *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/kldiag.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <promgraph.h>
#include <libsc.h>
#include <libkl.h>
#include <pgdrv.h>
#include <sn0hinv.h>
#include <io6fprom.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/SN0/klhwinit.h> /* nichdr_t definition */
#include <sys/SN/gda.h>
#include <sys/sbd.h>
#include <ksys/elsc.h>
#include <sys/PCI/bridge.h>

const char * board_class [] = {
"NODE",
"IO",
"ROUTER",
"MIDPLANE",
"GRAPHICS"
} ;

extern char* find_board_name(lboard_t *l);

const char *board_name[ ][8] = {
"IP27", "", "", "", "", "", "", "",
"BASEIO", "MSCSI", "MENET", "HIPPI-Serial", "GRAPHICS", "PCI_XIO", "VME", "MIO",
"ROUTER", "NULL_ROUTER", "META_ROUTER","", "", "", "", "",
"MIDPLANE", "", "", "", "", "", "", "",
"KONA", "???", "MARDIGRAS", "", "", "", "","",
} ;

const char *component_name[] = {
"NEW-Component",
"CPU",
"HUB",
"MEMBANK",
"XBOW",
"BRIDGE",
"IOC3",
"PCI",
"VME",
"ROUTER",
"GFX",
"PCI-SCSI",
"FDDI",
"MIO",
"SCSI-DISK",
"SCSI-TAPE",
"SCSI-CDROM",
"Hub-Uart",
"IOC3-ENET",
"IOC3-UART",
"???",
"IOC3-PCKBD",
"RAD",
"Hub-tty",
"IOC3-TTY",
"Adaptec FiberChannel",
"SNUM", 
"Mouse",
"TPU",
"GSN (Primary)",
"GSN (Auxiliary)",
} ;

int cname_size = (sizeof(component_name)/sizeof(char *)) ;

char *scsi_device[] = {
"DISK",
"TAPE",
"CDROM"
} ;

void print_pg_paths(graph_hdl_t, vertex_hdl_t, char *) ;
static void print_io_paths(void) ;
static void print_field_rev(char, char *) ;
static void prom_graph_list(graph_hdl_t, vertex_hdl_t, char *) ;

void hinv_print_cpu(klinfo_t *) ;
void hinv_print_mem(klinfo_t *) ;
void hinv_verbose(void) ;
void hinv_visit_vertex(int) ;

static void prcomp(klinfo_t *) ;

#define HINV_VERBOSE	0x01
#define HINV_PATHS	0x02
#define HINV_LIST	0x04
#define HINV_DEFAULT 	0x10
#define HINV_MFG     	0x20
#define HINV_MVV     	0x40

extern int  cpufreq(int) ;
extern void print_extra_nic(int) ;

#ifdef SN_PDI
extern graph_hdl_t      pg_hdl ;
extern vertex_hdl_t     snpdi_rvhdl ;
#else
extern graph_hdl_t  	prom_graph_hdl ;
extern vertex_hdl_t  	hw_vertex_hdl ;
#endif
extern int 		sm_sable ;
extern pcfg_lboard_t	*pcfg_lboard_p ;
extern int		dksc_inv_map[] ;

int flags ;
static graph_hdl_t 	hinv_pghdl ;
static vertex_hdl_t	hinv_rvhdl ;

/*ARGSUSED*/
int
klhinv(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
	char * path_name ;

	flags = 0 ;

#ifdef SN_PDI
	hinv_pghdl = pg_hdl ;
	hinv_rvhdl = snpdi_rvhdl ;
#else
	hinv_pghdl = prom_graph_hdl ;
	hinv_rvhdl = hw_vertex_hdl ;
#endif

	while (--argc) {
		++argv;
		if (!strcmp(*argv,"-v")) 
			flags |= HINV_VERBOSE;
		else if (!strcmp(*argv,"-m"))
			/* Also take -m like the kernel does */
			/* dump info suitable for MFG */
			flags |= HINV_MFG;
		else if (!strcmp(*argv,"-mvvv"))
			/* dump all NIC strings unparsed */
			flags |= HINV_MVV;
		else if (!strcmp(*argv,"-l")) {
			flags |= HINV_LIST;
			--argc ; 
			if (!argc) {
				printf("Pathname needed.\n") ;
				return 1 ;
			}
			++argv ;
			path_name = *argv ;
			prom_graph_list(hinv_pghdl, hinv_rvhdl, path_name) ;
			return 0 ;
		}
		else if (!strcmp(*argv,"-g")) {
                        --argc ;
                        if (!argc) {
				path_name = NULL ;
                        } else {
                        	++argv ;
                        	path_name = *argv ;
			}
			print_pg_paths(hinv_pghdl, hinv_rvhdl, path_name) ;
			/* There are some more io paths in the graph
	   		   that get left out. Print them also so that
	   		   users can use it to set nvram variables. */

			if ((!SN00) && (!path_name)) {
			    printf("\nIO PATHS\n\n") ;
			    print_io_paths() ;
		  	}

			return 0 ;
		} else
			return 1;
	}

	if (flags & HINV_VERBOSE) {
		/* Visit KLCONFIG */
		hinv_verbose() ;
	} else {
		/* Visit promgraph */
		flags |= HINV_DEFAULT;
		hinv_visit_vertex(flags) ;
	}
	return 0 ;
}

int
print_disable_info(klinfo_t *k, char *str)
{
        if (!(KLCONFIG_INFO_ENABLED(k))) {
 	    if (str)
            	printf("    %s: ", str) ;
	    else
		printf(", ") ;
	    printf("**Disabled") ;

            if (k->diagval)
                printf(", Reason = %s", get_diag_string(k->diagval));
            printf("\n");
	    return 0 ;
        }
	return 0 ;
}

void
visit_klinfo(lboard_t	*l, int (*fp)(), void *ap, void *rp)
{
	int 		i ;
	klinfo_t	*k;
        for (i=0; i< KLCF_NUM_COMPS(l); i++) {
                k = KLCF_COMP(l, i);
                if (!((*fp)(k, ap, rp)))
                    break ;
	}
}

int
prhubinfo(klinfo_t *k, void *ap, void *rp)
{
	klhub_t *kh ;
	__uint64_t	frac_speed ;

        if (KLCF_COMP_TYPE(k) != KLSTRUCT_HUB)
		return 1 ;
	kh = (klhub_t *)k ;
        printf("    ASIC %s ", COMPONENT_NAME(k->struct_type)) ;
        print_field_rev(k->revision, "Rev ") ;
	if (kh->hub_speed) {
	    printf(", %d", (kh->hub_speed)/1000000) ;
	    frac_speed = (kh->hub_speed)%1000000 ;
	    frac_speed /= 100000 ;
	    if (frac_speed)
		printf(".%d", frac_speed) ;
	    printf(" MHz") ;
	}
	if(k->flags & KLINFO_ENABLE)
	printf(", (nasid %d)", k->nasid) ;
	else
	printf(", (nasid %d)", k->physid) ;
        printf("\n") ;
	return 1 ;
}

int
pr_klcfg_hub(lboard_t	*l)
{
	visit_klinfo(l, prhubinfo, NULL, NULL) ;;
        return 1 ;
}

int
prcpuinfo(klinfo_t *k, void *ap, void *rp)
{
        if (KLCF_COMP_TYPE(k) != KLSTRUCT_CPU)
                return 1 ;
	hinv_print_cpu(k) ;
        return 1 ;
}

int
pr_klcfg_cpu(lboard_t   *l)
{
        visit_klinfo(l, prcpuinfo, NULL, NULL) ;;
        return 1 ;
}

int
prmeminfo(klinfo_t *k, void *ap, void *rp)
{
        if (KLCF_COMP_TYPE(k) != KLSTRUCT_MEMBNK)
                return 1 ;
        hinv_print_mem(k) ;
        return 1 ;
}

int
pr_klcfg_mem(lboard_t   *l)
{
        visit_klinfo(l, prmeminfo, NULL, NULL) ;;
        return 1 ;
}

static int
pr_klcfg_node(lboard_t *l, void *ap, void *rp)
{
	char sname[SLOTNUM_MAXLENGTH];

        get_slotname(l->brd_slot, sname);

	if (KLCLASS(l->brd_type) != KLCLASS_NODE)
		return 1 ;
	printf("%s Node Board, Module %d, Slot %s\n", 
		find_board_name(l), l->brd_module, sname) ;
	pr_klcfg_hub(l) ;
	pr_klcfg_cpu(l) ;
	pr_klcfg_mem(l) ;
	return 1 ;
}

static int
pr_klcfg_rps(lboard_t *l, void *ap, void *rp)
{
    int *mod_visited = (int *)ap;
    int retval;
    elsc_t remote_elsc;
    uint16_t status;

    /* 
     * retrieve this module's elsc based on the nasid of the hub.  Only
     * node boards' nasids are the same as the hub's nasid.
     */  
    if (KLCLASS(l->brd_type) != KLCLASS_NODE)
	return 1 ;

    if (!mod_visited[l->brd_module]) {
	elsc_init(&remote_elsc, l->brd_nasid);
	retval = elsc_rpwr_query(&remote_elsc, (l->brd_nasid == get_nasid()));
	if (retval == 1)
	    printf("Redundant Power Supply in Module %d (Enabled)\n", 
		   l->brd_module);
	else if (retval == 0)
	    printf("Redundant Power Supply in Module %d (Lost Redundancy)\n",
		   l->brd_module);
	/* Check if there is an xbox connected to this node/module in O200 */
	if (SN00) {
	    if (hubii_widget_is_xbow(l->brd_nasid)) {
		status = 
		    *(uint16_t *)((NODE_SWIN_BASE(l->brd_nasid,XBOX_BRIDGE_WID)
				   + FLASH_PROM1_BASE));
		if (status & XBOX_RPS_EXISTS)
		    printf("Redundant Power Supply in Gigachannel unit on "
			   "Module %d (%s)\n", l->brd_module, 
			   (status & XBOX_RPS_FAIL) ? "Lost Redundancy" :
			   "Enabled");
	    }
	}
	mod_visited[l->brd_module] = 1;
    }
    return 1;
}

int
prtpuinfo(klinfo_t *k, void *ap, void *rp)
{
        if (KLCF_COMP_TYPE(k) != KLSTRUCT_TPU)
                return 1 ;
        printf("    ASIC %s ", COMPONENT_NAME(k->struct_type)) ;
        print_field_rev(k->revision, "Rev ") ;
        printf(", (widget %d)", k->widid) ;
	print_disable_info(k, NULL) ;
        printf("\n") ;
        return 1 ;
}

int
pr_klcfg_tpu(lboard_t   *l)
{
        visit_klinfo(l, prtpuinfo, NULL, NULL) ;;
        return 1 ;
}

int
prgsninfo(klinfo_t *k, void *ap, void *rp)
{
        if ((KLCF_COMP_TYPE(k) != KLSTRUCT_GSN_A) && 
	    (KLCF_COMP_TYPE(k) != KLSTRUCT_GSN_B))
                return 1 ;
        printf("    ASIC %s ", COMPONENT_NAME(k->struct_type)) ;
        print_field_rev(k->revision, "Rev ") ;
        printf(", (widget %d)", k->widid) ;
	print_disable_info(k, NULL) ;
        printf("\n") ;
        return 1 ;
}

int
pr_klcfg_gsn(lboard_t   *l)
{
        visit_klinfo(l, prgsninfo, NULL, NULL) ;;
        return 1 ;
}

int
prbridgeinfo(klinfo_t *k, void *ap, void *rp)
{
        if (KLCF_COMP_TYPE(k) != KLSTRUCT_BRI)
                return 1 ;
        printf("    ASIC %s ", COMPONENT_NAME(k->struct_type)) ;
        print_field_rev(k->revision, "Rev ") ;
        printf(", (widget %d)", k->widid) ;
	print_disable_info(k, NULL) ;
        printf("\n") ;
        return 1 ;
}

int
pr_klcfg_bridge(lboard_t   *l)
{
        visit_klinfo(l, prbridgeinfo, NULL, NULL) ;;
        return 1 ;
}

int
prpcidevinfo(klinfo_t *k, lboard_t *l, int npci)
{
	klioc3_t	*ioc3ptr ;
	klscsi_t	*scsiptr ;

	switch(k->struct_type) {
            case KLSTRUCT_IOC3 :
/* XXX Check for SN00 specific info on each IOC3 */
                ioc3ptr = (klioc3_t *)k ;
                if (((l->brd_type == KLTYPE_MENET) && (npci == 3)) ||
                    (CHECK_IOC3_NIC(l, k->physid))) {
                } else {
                    printf("        controller multi function SuperIO") ;
                    print_field_rev(ioc3ptr->ioc3_superio.revision,
                                " Rev ") ;
                    printf("\n") ;
		}

		if (IS_MIO_IOC3(l, npci)) {
		    printf("        controller Keyboard/Mouse\n") ;
		    printf("        controller Parallel Port") ;
                    printf("\n") ;
		} else {
                    printf("        controller Ethernet") ;
                    print_field_rev(ioc3ptr->ioc3_enet.revision,
                                        " Rev ") ;
                    printf("\n") ;
		}

            break ;

                case KLSTRUCT_SCSI :
                {
                        klscdev_t * scsi_devptr ;
                        int i ;
			nasid_t	node = k->nasid ;

                        scsiptr = (klscsi_t *)k ;
                        for (i=0; i<scsiptr->scsi_numdevs; i++)
                        {
                                scsi_devptr = (klscdev_t *)(NODE_OFFSET_TO_K1
					(node, scsiptr->scsi_devinfo[i])) ;
                                printf("        peripheral SCSI %s, ID %d, %s\n",
					scsi_device[scsi_devptr->
					scdev_info.struct_type-KLSTRUCT_DISK],
                                    	scsi_devptr->scdev_info.physid,
                                    	scsi_devptr->scdev_info.arcs_compt->
					Identifier) ;
                        }
                }
                break ;

		default:
		break ;
	} /* switch */

	return 1 ;
}

int
prpciinfo(klinfo_t *k, void *ap, void *rp)
{
    	lboard_t	*l = (lboard_t *)ap ;
	klbri_t		*kb ;
	int32_t		pci_id = 0 ;

	kb = (klbri_t *)find_component(l, NULL, KLSTRUCT_BRI) ;

  	/* All non BRIDGE or TPU guys on IO boards are PCI components !!! */

        if ((KLCF_COMP_TYPE(k) == KLSTRUCT_BRI) ||
            (KLCF_COMP_TYPE(k) == KLSTRUCT_TPU) ||
            (KLCF_CLASS(l)     == KLCLASS_PSEUDO_GFX))
                return 1 ;

        printf("    adapter %s ", COMPONENT_NAME(k->struct_type)) ;
	if ((k->struct_type >= cname_size) ||
	    (k->struct_type <= 0)) {
	    pci_id = kb->bri_devices[k->physid].pci_device_id ;
	    printf("(Vendor %x Device %x)", (pci_id & 0xffff), 
		    ((pci_id >> 16) & 0xffff)) ;
	}

        print_field_rev(k->revision, "Rev ") ;
	printf(", (pci id %d)", k->physid) ;
        if (print_disable_info(k, NULL))
            return 1 ;
	printf("\n") ;
	prpcidevinfo(k, l, k->physid) ;
        return 1 ;
}

int
pr_klcfg_pci(lboard_t   *l)
{
        visit_klinfo(l, prpciinfo, (void *)l, NULL) ;;
        return 1 ;
}

static int
pr_klcfg_io(lboard_t *l, void *ap, void *rp)
{
        char sname[SLOTNUM_MAXLENGTH];
	char bname[32] ;

        get_slotname(l->brd_slot, sname);

	/* For hinv purposes the PSEUDO_GFX board is like IO board */
        if ((KLCLASS(l->brd_type) != KLCLASS_IO) &&
	    (KLCLASS(l->brd_type) != KLCLASS_PSEUDO_GFX))
                return 1 ;

	if (((l->brd_type&KLTYPE_MASK) > 0) && 
	    ((l->brd_type&KLTYPE_MASK) <= 8))
		strcpy(bname, BOARD_NAME(l->brd_type)) ;
	else {
	        if (*l->brd_name)
		    strcpy(bname, l->brd_name) ;
		else
		    strcpy(bname, BOARD_NAME(l->brd_type)) ;
	}

	if (SN00)
        	printf("%s Origin 200 IO Board, Module %d, Slot %s\n",
			bname, l->brd_module, sname) ;
	else
        	printf("%s IO Board, Module %d, Slot %s\n",
			bname, l->brd_module, sname) ;
        pr_klcfg_bridge(l) ;
        pr_klcfg_pci(l) ;
        pr_klcfg_tpu(l) ;
        return 1 ;
}

int
prrouterinfo(klinfo_t *k, void *ap, void *rp)
{
        if (KLCF_COMP_TYPE(k) != KLSTRUCT_ROU)
                return 1 ;
        printf("ASIC %s ", COMPONENT_NAME(k->struct_type)) ;
        print_field_rev(k->revision, "Rev ") ;
        return 1 ;
}

int
pr_klcfg_routerc(lboard_t   *l)
{
        visit_klinfo(l, prrouterinfo, NULL, NULL) ;;
        return 1 ;
}

static int
pr_klcfg_router(lboard_t *l, void *ap, void *rp)
{
        char sname[SLOTNUM_MAXLENGTH];

        if ((KLCLASS(l->brd_type) != KLCLASS_ROUTER) ||
	    (KL_CONFIG_DUPLICATE_BOARD(l)))
                return 1 ;
        get_slotname(l->brd_slot, sname);
	pr_klcfg_routerc(l) ;
	printf(", Module %d, Slot %s (nasid %d)\n", l->brd_module, sname, 
			l->brd_nasid) ;
	return 1 ;
}

int
prxbowinfo(klinfo_t *k, void *ap, void *rp)
{
        if (KLCF_COMP_TYPE(k) != KLSTRUCT_XBOW)
                return 1 ;
        printf("ASIC %s ", COMPONENT_NAME(k->struct_type)) ;
        print_field_rev(k->revision, "Rev ") ;
        return 1 ;
}

int 
pr_klcfg_xbow(lboard_t *l)
{
        visit_klinfo(l, prxbowinfo, NULL, NULL) ;
	printf(", on midplane of Module %d\n", l->brd_module) ;
        return 1 ;
}

static int
pr_klcfg_mpl(lboard_t *l, void *ap, void *rp)
{
	char backplane[8];
        if ((KLCLASS(l->brd_type) != KLCLASS_MIDPLANE) ||
	    (SN00) || (CONFIG_12P4I))
                return 1 ;
	
	if(ip27log_getenv(l->brd_nasid, BACK_PLANE, backplane,"000",0) >= 0 &&
((atoi(backplane) == 360) || (atoi(backplane) == 390)))
	{
	    printf("MIDPLANE, Module %d Frequency %d MHz\n", l->brd_module, atoi(backplane)) ;
	}
        pr_klcfg_xbow(l) ;
	return 1 ;
}

static int
pr_klcfg_gfx(lboard_t *l, void *ap, void *rp)
{
        char sname[SLOTNUM_MAXLENGTH];

        if (KLCLASS(l->brd_type) != KLCLASS_GFX)
		return 1 ;

        get_slotname(l->brd_slot, sname);

        printf("%s Graphics Board, Module %d, Slot %s\n",
                BOARD_NAME(l->brd_type),
                l->brd_module, sname) ;
	return 1 ;
}

void
hinv_verbose(void)
{
    int mod_visited[MAX_MODULES];

    bzero(mod_visited, sizeof(int) * MAX_MODULES);

    visit_lboard(pr_klcfg_node,     NULL, NULL) ;
    visit_lboard(pr_klcfg_rps, (void *)mod_visited, NULL);
    visit_lboard(pr_klcfg_io,       NULL, NULL) ;
    visit_lboard(pr_klcfg_router,   NULL, NULL) ;
    visit_lboard(pr_klcfg_mpl,      NULL, NULL) ;
/* XXX handle graphics and unclassified boards */
                visit_lboard(pr_klcfg_gfx,   NULL, NULL) ;
                /* visit_lboard(pr_klcfg_uno,   NULL, NULL) ; */
}

void
hinv_print_cpu(klinfo_t *k)
{
	klcpu_t	*kcpu_p ;
	unsigned short	temp ;
        char            cpustr[32] ;

	kcpu_p = (klcpu_t *) k ;
        sprintf(cpustr, "Processor %c", (k->physid ? 'B':'A')) ;

	if (print_disable_info(k, cpustr))
	    return ;
	if(kcpu_p->cpu_speed == 65535)
	    return;

        /*printf("    %s: %d MHz ",cpustr, cpufreq(kcpu_p->cpu_info.virtid)) ;*/
        printf("    %s: %d MHz ",cpustr, kcpu_p->cpu_speed) ;
                        /* XXX proper type */
        temp = kcpu_p->cpu_prid ;
        if (((temp>>8)&0xff) == C0_IMP_R10000)
                printf("R10000,") ;
	if (((temp>>8)&0xff) == C0_IMP_R12000)
		printf("R12000,") ;
        printf(" Rev %d.%d, ", ((temp>>4)&0xf), (temp&0xf)) ;
	if(kcpu_p->cpu_info.struct_version >= 2)
       	    printf("%dM  %dMHz secondary cache", kcpu_p->cpu_scachesz, kcpu_p->cpu_scachespeed) ;
	else
       	    printf("%dM  secondary cache", kcpu_p->cpu_scachesz) ;
        printf(", (cpu %d)", k->virtid) ;
        printf("\n");

	temp = kcpu_p->cpu_fpirr ;
	if (((temp>>8)&0xff) == 0x9) {
		printf("      R10000FPC ") ;
		printf(" Rev %d", temp&0xff) ;
	}
	if (((temp>>8)&0xff) == C0_IMP_R12000) {
		printf("      R12000FPC ") ;
		printf(" Rev %d", temp&0xff) ;
	}
	printf("\n") ;
}

void
hinv_print_mem(klinfo_t *k)
{
	klmembnk_t	*kmem_p ;
	int		dismem ;
    	char            mem_disable[MD_MEM_BANKS + 1];
    	char            disable_reason[MD_MEM_BANKS + 1];
	int 		i ;

	*mem_disable = 0 ;
	*disable_reason = 0 ;
	kmem_p = (klmembnk_t *) k ;

	printf("    Memory on board") ;
	printf(", %d MBytes", kmem_p->membnk_memsz) ;
	printf(" (%s)", (KLCONFIG_MEMBNK_PREMIUM(kmem_p, MD_MEM_BANKS) ? 
		 "Premium":"Standard")) ;
	if (dismem = dismem_size_k(k, mem_disable)) {
	    printf(", (%d Mbytes - Bank(s) ", dismem) ;
	    for (i=0; i<strlen(mem_disable); i++) {
		if(mem_disable[i] - '0')
                    printf("%d ", i) ;
	    }
	    printf("disabled)") ;
	}

	printf("\n") ;

	if (strlen(mem_disable)) {
     		ip27log_getenv(k->nasid, 
			       DISABLE_MEM_REASON, 
			       disable_reason, 
			       "00000000", 
			       0);
	}


        for (i = 0; i < MD_MEM_BANKS; i++) {
	    if (kmem_p->membnk_bnksz[i] != MD_SIZE_EMPTY) {
		printf("      Bank %d, %d MBytes", i, kmem_p->membnk_bnksz[i]) ;
		printf(" (%s) ", (KLCONFIG_MEMBNK_PREMIUM(kmem_p, i) ? 
			 "Premium":"Standard")) ;

		if (i == 0 ||  kmem_p->membnk_dimm_select) {
		    printf(" <-- (Software Bank %d) ",
			   i ^ (kmem_p->membnk_dimm_select/2));
		}

		if (strlen(mem_disable) && (mem_disable[i] - '0')) {
		    if (disable_reason[i] == '0')
			printf("Disabled, Reason: Unknown\n");
		    else if (disable_reason[i] == '0' + MEM_DISABLE_USR)
			printf("Disabled, Reason: %s\n", 
			       get_diag_string(KLDIAG_MEMORY_DISABLED));
		    else if (disable_reason[i] == '0' + MEM_DISABLE_AUTO)
			printf("Disabled, Reason: %s\n", 
			       get_diag_string(KLDIAG_MEMTEST_FAILED));
		    else if (disable_reason[i] == '0' + MEM_DISABLE_256MB)
			printf("Disabled, Reason: %s\n", 
			       get_diag_string(KLDIAG_IP27_256MB_BANK));
		}
		printf("\n") ;
	    } else if(dismem && (mem_disable[i] - '0')) {
		printf("      Bank %d, %d MBytes ", i, MD_SIZE_MBYTES(mem_disable[i] - '0')) ;
		if (disable_reason[i] == '0')
                    printf("Disabled, Reason: Unknown\n");
                else if (disable_reason[i] == '0' + MEM_DISABLE_USR)
                    printf("Disabled, Reason: %s\n", 
                                get_diag_string(KLDIAG_MEMORY_DISABLED));
                else if (disable_reason[i] == '0' + MEM_DISABLE_AUTO)
                    printf("Disabled, Reason: %s\n", 
                                get_diag_string(KLDIAG_MEMTEST_FAILED));
                else if (disable_reason[i] == '0' + MEM_DISABLE_256MB)
                    printf("Disabled, Reason: %s\n", 
                                get_diag_string(KLDIAG_IP27_256MB_BANK));
		
	    }
	}
}

/* All the code up to this point handles hinv -v only */

void
print_sys_name(void)
{
    char 		*label_name ;

    if (label_name = (char *)pg_get_lbl(hinv_rvhdl, 
		INFO_LBL_VERTEX_NAME))
    printf("System  %s\n", label_name) ;
}

extern int kl_get_num_cpus(void) ;
extern int hinv_get_num_cpus(cpu_list_t *) ;

int 
hinv_mvv(void *arg, vertex_hdl_t vhdl)
{
	lboard_t	*l ;
	char		*ns = NULL ;
	char    	sname[SLOTNUM_MAXLENGTH];
	char 		lname[64];

	if (!(l = (lboard_t *)pg_get_lbl(vhdl, INFO_LBL_BRD_INFO))) {
		return 0 ;
	}
	if (!(ns = (char *)pg_get_lbl(vhdl, INFO_LBL_NIC))) {
		return 0 ;
	}
        get_slotname(l->brd_slot, sname) ;
        sprintf(lname, "/hw/module/%d/slot/%s",
                l->brd_module, sname) ;
        printf("location: %s\n", lname) ;
	if (ns) {
	    if (flags&HINV_MVV)
               printf("%s\n", ns) ;
	    else
               prnic_fmt(ns) ;
	    printf("\n") ;
	}
	return 0 ; /* Enable it to visit all nodes */
}

int
hinv_default(void *arg, vertex_hdl_t vhdl)
{
	klinfo_t	*k ;

	if (!(k = (klinfo_t *)pg_get_lbl(vhdl, INFO_LBL_KLCFG_INFO))) {
		return 0 ;
	}
	prcomp(k) ;
	return 0 ;
}

void
hinv_visit_vertex(int flags)
{
	graph_error_t 		graph_err ;
	vertex_hdl_t 		next_vertex_hdl;
	int			i, cpu_cnt, dismem ;
        cpu_list_t              cpu_list;

        if ((flags & HINV_MVV) || (flags & HINV_MFG)) {
		graph_vertex_visit(hinv_pghdl, hinv_mvv, 
				   &flags, &graph_err, &next_vertex_hdl) ;
                print_extra_nic(flags) ;
		return ;
	} /* MVV */

	if (flags) { /* If any flag is set */
		print_sys_name() ;

                hinv_get_num_cpus(&cpu_list) ;
                for(i = 0; i < cpu_list.size ; i++)
                {
                        cpu_cnt = cpu_list.count[i];
                        printf("%d %d MHz IP27 Processors", (cpu_cnt&0xffff),
                                        cpu_list.freq[i]);
                        if (cpu_cnt>>16)
                                printf(" (%d disabled)", (cpu_cnt>>16)) ;
                        printf("\n") ;
                }
		printf("Main memory size: %d Mbytes", 
				(cpu_get_memsize()/256)) ;
		if (dismem = cpu_get_dismem())
			printf(", (%d Mbytes disabled)", dismem) ;
		printf("\n") ;
/* XXX  print instruction and data cache info */
		graph_vertex_visit(hinv_pghdl, hinv_default, 
				   &flags, &graph_err, &next_vertex_hdl) ;
		return ;
	} /* DEFAULT */
}

void
prom_graph_list(graph_hdl_t ghdl, vertex_hdl_t rvhdl, char *string)
{
	vertex_hdl_t 	vhdl, next_vertex_hdl;
	graph_error_t 	graph_err ;
	char edge_name[32], *remain ;
	graph_edge_place_t 	eplace = GRAPH_EDGE_PLACE_NONE ;

	/* special handling for '/'. */

	if ((!strcmp(string, "/")) || ((string[0] == '/') && (string[1] == ' ')))
		vhdl = hinv_rvhdl ;
	else
	if ((graph_err = hwgraph_path_lookup(rvhdl, 
             string, &vhdl, &remain)) != GRAPH_SUCCESS)
	{
		printf("Invalid path %s.\n", remain) ;
		return ;
	}

	do {
		if ((graph_err = graph_edge_get_next(ghdl, vhdl, 
                  edge_name, &next_vertex_hdl, &eplace)) == GRAPH_SUCCESS)
			printf("\t%s", edge_name) ;
	}
	while (graph_err == GRAPH_SUCCESS) ;
	printf("\n") ;
}

static void
prcomp(klinfo_t *k)
{
        char  		*str;
	lboard_t	*lbptr ;
	graph_error_t	graph_err ;
	char slotname[SLOTNUM_MAXLENGTH];
	char lname[64];
	klioc3_t	*kioc3_p ;
	nichdr_t	*nichdr_p ;

        switch(k->struct_type) {

	case KLSTRUCT_GFX:
		printf("Graphics Controller\n") ;
	break ;

        case KLSTRUCT_DISK:
             printf("    Disk drive: unit %d on SCSI Controller %d",
		k->physid, dksc_inv_map[k->virtid]) ;
	     if (dksc_inv_map[k->virtid] < 2) /* print this for 0 and 1 only */
 	         printf(", (dksc(%d,%d,0))\n",
			dksc_inv_map[k->virtid],  k->physid);
	     else
		printf("\n") ;
        break;

        case KLSTRUCT_TAPE:
             printf("    Tape drive: unit %d on SCSI Controller %d",
		k->physid, dksc_inv_map[k->virtid]) ;
             if (dksc_inv_map[k->virtid] < 2) /* print this for 0 and 1 only */
 		printf(", (tpsc(%d,%d,0))\n",
			dksc_inv_map[k->virtid],  k->physid);
	     else
                printf("\n") ;
        break;

	case KLSTRUCT_CDROM:
             printf("    CDROM: unit %d on SCSI Controller %d",
			k->physid, dksc_inv_map[k->virtid]) ;
	     if (dksc_inv_map[k->virtid] < 2) /* print this for 0 and 1 only */
 		printf(", (cdrom(%d,%d,7))\n",
			dksc_inv_map[k->virtid],  k->physid) ;
             else
                printf("\n") ;
	break ;

	case KLSTRUCT_SCSI:
                printf("Integral SCSI controller") ;
		if (k->virtid + 1) /* virtid != -1, driver installed */
			printf(" %d", dksc_inv_map[k->virtid]) ;
		printf("\n") ;
	break;

	case KLSTRUCT_IOC3:
#if 0
                printf("%*s: pci %d",HINVCENTER, "PCI IOC3",k->physid);
		if (k->virtid + 1) /* virtid != -1, driver installed */
			printf(", (adapter %d)\n", k->virtid) ;
		else
			printf("\n") ;
#endif

		kioc3_p = (klioc3_t *) k ;
		k = (klinfo_t *)&kioc3_p->ioc3_enet ;
                printf("Integral Fast Ethernet") ;
		if (k->virtid + 1) /* virtid != -1, driver installed */
			printf(" (adapter %d)\n", k->virtid) ;
		else
			printf("\n") ;

		k = (klinfo_t *)&kioc3_p->ioc3_superio ;
		printf("IOC3 serial port") ;
		if (k->virtid + 1) /* virtid != -1, driver installed */
			printf(", (adapter %d)\n", k->virtid) ;
		else
			printf("\n") ;
	break ;
        }
}

static void 
print_field_rev(char value, char *fname)
{
        if (value != 0xff)
                printf("%s%x", fname, value) ;

}

static char *
get_next_nic_str(char *src_nic, char *next_nic)
{
	char *c ;
	int len = 0 ;

	c = strstr(src_nic, "Laser:") ;
	if (c) {
		while (*c)
			if (*c++ == ';') break ;
		len = c - src_nic ;
		strncpy(next_nic, src_nic, len) ;
		next_nic[len] = 0 ;
		return c ;
	}
	return NULL ;
}

static void
prnicfield(char *c)
{
	while ((*c) && (*c++ != ':')) ;
	if (*c)
		while ((*c) && (*c != ';'))
			putchar(*c++) ;
	putchar('\t') ;
}

void
prnic_fmt(char *n)
{
	char next_nic[512] ;
	char *next_nicp = n , *c ;
	int  flag ;

	while (next_nicp = get_next_nic_str(next_nicp, next_nic)) {
		flag = 0 ;
		printf("\t\t") ;
		if (c = strstr(next_nic, "Name:")) {
			prnicfield(c) ;
			printf(" Board: ") ;
			flag = 1 ;
		}
		if (c = strstr(next_nic, "Serial:")) {
			printf("barcode ") ;
			prnicfield(c) ;
			flag = 1 ;
		}
		if (c = strstr(next_nic, "Part:")) {
			printf("part ") ;
			prnicfield(c) ;
			flag = 1 ;
		}
		if (c = strstr(next_nic, "Revision:")) {
			printf("rev ") ;
			prnicfield(c) ;
			flag = 1 ;
		}
		if (!flag)
			printf("[%s]", next_nic) ;
		printf("\n") ;
	}
}


/* Traverse the promgraph recursively, printing out each path */

char 		path[256] ;
graph_error_t	graph_err ;
char		name[32] ;
char            edge_name[32] ;
graph_edge_place_t      eplace ;
char		visited[256] ;

static int
is_vertex_leaf(graph_hdl_t ghdl, vertex_hdl_t vhdl)
{
	vertex_hdl_t	nvhdl ;

	eplace = GRAPH_EDGE_PLACE_NONE ;
	do {
        	graph_err = graph_edge_get_next(ghdl,
                                        	vhdl, name,
                                        	&nvhdl, &eplace) ;
        	if (graph_err != GRAPH_SUCCESS)
			return 1 ;
		else {
			if (visited[nvhdl])
				continue ;
			return 0 ;
		}
	} while (1) ;
}

static void
traverse_pg(graph_hdl_t ghdl, vertex_hdl_t vhdl, char *sub_path_p)
{
        graph_error_t   graph_err ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
        vertex_hdl_t    nvhdl ;

	if (is_vertex_leaf(ghdl, vhdl)) {
		printf("%s\n", path) ;
		return ;
	}

	do {
        	graph_err = graph_edge_get_next(ghdl,
                                        vhdl,
                                        edge_name,
                                        &nvhdl, &eplace) ;
        	if (graph_err != GRAPH_SUCCESS) 
			break ;
		if (visited[nvhdl])
			continue ;
		visited[nvhdl] = 1 ;
		if (strcmp(edge_name, "xbow")) {
		    strcpy(sub_path_p, "/") ;
		    strcat(sub_path_p, edge_name) ;
		}
#if 0
printf("t: /%s", edge_name) ;
#endif
		if (strcmp(edge_name, "xbow"))
		    traverse_pg(ghdl, nvhdl, 
				sub_path_p + strlen(edge_name) + 1) ;
		else
		    traverse_pg(ghdl, nvhdl, sub_path_p) ;

	} while (graph_err == GRAPH_SUCCESS) ;
}

/* For all modules, for all slots, 
   print_pg_paths of /hw/module/?/slot/ioX */

static void
print_io_paths(void)
{
        graph_error_t   graph_err, graph_err1 ;
        char            tmp_buf[64] ;
        char            ename[64], sename[64], *remain ;
        vertex_hdl_t    mod_vhdl, nextmod_vhdl, nextslot_vhdl ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE, eplace1 ;
	int		hwpath_len ;

        strcpy(tmp_buf, "/hw/module/") ;
        graph_err = hwgraph_path_lookup(hinv_rvhdl, tmp_buf,
                                        &mod_vhdl, &remain) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("Error: No Modules in HWGRAPH.\n") ;
                return ;
        }

        do {
            graph_err = graph_edge_get_next(hinv_pghdl,
                                            mod_vhdl,
                                            ename,
                                            &nextmod_vhdl, &eplace) ;
            if (graph_err != GRAPH_SUCCESS) {
                break ;
            }
	    strcat(tmp_buf, ename) ;
	    strcat(tmp_buf, "/slot/") ;

            graph_err1 = hwgraph_path_lookup(hinv_rvhdl, tmp_buf,
                                        &nextmod_vhdl, &remain) ;
            if (graph_err != GRAPH_SUCCESS) {
                printf("Error: No Slots in HWGRAPH.\n") ;
                return ;
            }

	    eplace1 = GRAPH_EDGE_PLACE_NONE ;
	    hwpath_len = strlen(tmp_buf) ;
	    do {
		graph_err1 = graph_edge_get_next(hinv_pghdl,
                                            nextmod_vhdl,
                                            sename,
                                            &nextslot_vhdl, &eplace1) ;
                if (graph_err1 != GRAPH_SUCCESS) {
                    break ;
                }
		if (strncmp(sename, "io", 2))
		    continue ;
		tmp_buf[hwpath_len] = 0 ;
                strcat(tmp_buf, sename) ;
		print_pg_paths(hinv_pghdl, hinv_rvhdl, tmp_buf) ;

	    } while (graph_err1 == GRAPH_SUCCESS) ;
	    tmp_buf[strlen("/hw/module/")] = 0 ;

        } while (graph_err == GRAPH_SUCCESS) ;


}

void
print_pg_paths(graph_hdl_t ghdl, vertex_hdl_t rvhdl, char *input_path)
{
	int 		i ;
	vertex_hdl_t	vhdl ;
	char 		*remain ;

	if (!input_path)
		input_path = "/hw/module" ;

	if (!strcmp(input_path, "/")) {
		vhdl = rvhdl ;
		*path = 0 ;
		*input_path = 0 ;
	}
	else {
		if ((graph_err = hwgraph_path_lookup(rvhdl,
             			input_path, &vhdl, &remain)) 
				!= GRAPH_SUCCESS) {
                	printf("print_pg_paths: Invalid path %s.\n", remain) ;
                	return ;
		}
		strcpy(path, input_path) ;
        }

	for (i=0; i<256; i++)
		visited[i] = 0 ;

#if 0
	strcpy(path, input_path) ;
#endif
	traverse_pg(ghdl, vhdl, path + strlen(input_path)) ;

	/* clean_pg - visit all nodes and delete lbl "visited" */
}

#endif /* SN0 */

