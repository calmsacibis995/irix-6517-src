#ident  "IO6prom/inventory.c : $Revision: 1.42 $"

/***********************************************************************\
*	File:		inventory.c					*
*									*
*	This file contains the code for manipulating the hardware	*
*	inventory information.  It includes routines which read and	*
*	write the NVRAM inventory as well as code for checking the	*
*	validity of the current hardware state.				*
*									*
\***********************************************************************/

#include <libsc.h>
#include <libsk.h>
#include <libkl.h>

#include <sn0hinv.h>                    /* BOARD_NAME macro */
#include <promgraph.h> 			/* extern module table variables */
#include <sys/SN/slotnum.h>
#include <sys/SN/SN0/ip27config.h>         /* Definition of SN00 */
#include <sys/SN/SN0/klhwinit.h> 		/* IP27_PART_NUM */

#include "io6prom_private.h"
#include "inventory.h"

extern int   		num_modules ;
extern struct mod_tab   module_table[] ;
extern char* find_board_name(lboard_t *l);


/* Factor by which structs on stack need to be allocated.
   Due to alignment issues, we need to allocate a larger
   buffer for reading in structs from nvram.
*/

#define INVTAB_REV 		2
#define STRUCT_PAD		2

void
dump_inv_hdr(invtabhdr_t *ih)
{
    printf("inv hdr nic = %x\n", ih->nic) ;
    printf("flag	type	mod	slot	dv 	dp	") ;
    printf("cmap	physid	barcode	nic	offset\n") ;
}

/* print a record for debug purposes */

void
dump_inv_rec(invrec_t *ir, int count)
{
    ssize_t offset;

    printf("%x	%x	%d	%x	%d	%d	", 
            ir->flag,
            ir->btype,
            ir->modnum,
            ir->slotnum,
            ir->diagval,
            ir->diagparm) ;
    printf("%lx	%d	%s	", 
            ir->cmap,
            ir->physid,
	    ir->barcode) ;
    if (ir->nic+1)
        printf("%x\t", ir->nic) ;
    else
        printf("\t") ;
    offset=calc_pnt_nvoff(count);
    if (offset < 0) printf("Location invalid: PNT full\n");
    else
	printf("%x\n", offset) ;
}

void
dump_mod_map(invtabhdr_t *ih)
{
    int i = 0 ;

    for (i = 1; i <= MAX_MODULES; i++) {
        if (ih->modmap & (1 << i))
            printf("%d ", i) ;
    }
    printf("\n") ;
}

void
dump_modnum(void)
{
    int i ;

    for (i = 1; i <= num_modules; i++) {
	printf("%d ", module_table[i].module_id) ;
    }
    printf("\n") ;
}

/* dump the inventory table from nvram */

void
dump_inventory(void)
{
    char		buf[STRUCT_PAD*sizeof(invtabhdr_t)] ;
    invtabhdr_t         *ih = (invtabhdr_t *)buf ;
    char		buf1[STRUCT_PAD*sizeof(invrec_t)] ;
    invrec_t	  	*ir = (invrec_t *)buf1 ;
    int			i ;
    ssize_t		offset;

    bzero((char *)ih, sizeof(buf)) ;
    cpu_get_nvram_buf(NVOFF_INVHDR, sizeof(invtabhdr_t), (char *)ih) ;
    printf("Module numbers configured were ...\n") ;
    dump_mod_map(ih) ;
    dump_inv_hdr(ih) ;
    for (i=0; i<ih->cnt; i++) {
	offset=calc_pnt_nvoff(i);
	if (offset < 0) break;
        cpu_get_nvram_buf(offset, sizeof(invrec_t), (char *)ir) ;
        dump_inv_rec(ir, i) ;
    }

}

/*
 * set_compt_flag
 * 
 * construct a bit map of the components found on a brd.
 * Each bit represents the presence or absence of a component.
 * See inventory.h for the mapping of each bit to a component.
 * for eg: bit 0 - CPU A, bit 1 - CPU B and so on ...
 * This routine adds a bit for the component represented by k.
 */

void
set_compt_flag(klinfo_t *k, unsigned int *cmap)
{
    int 		i ;
    __psunsigned_t 	map = *cmap ;

    switch(k->struct_type) {
    	case KLSTRUCT_CPU:
 	    map |= (1 << k->physid) ;
        break ;

    	case KLSTRUCT_MEMBNK:
 	{
	    klmembnk_t	*m ;

 	    m = (klmembnk_t  *) k ;
	    for (i=0;i<MD_MEM_BANKS;i++) {
		map |= (m->membnk_bnksz[i] ? (1 << (i+CMAP_MEMBNK_SHIFT)) : 0) ;
	    }
	}
        break ;

 	/* Ignore these components, without them boards do not work */

	case KLSTRUCT_HUB:
	case KLSTRUCT_XBOW:
	case KLSTRUCT_BRI:
	case KLSTRUCT_GFX:
	case KLSTRUCT_TPU:
	case KLSTRUCT_GSN_A:
	case KLSTRUCT_GSN_B:
	break ;

	default:			/* All PCI devs */
	    map |= (1 << (k->physid + CMAP_PCI_SHIFT)) ;
	break ;
    }

    *cmap = map ;
}

/*
 * get_nicstr_info
 *
 * Given the nic string and the type of component, extract
 * the laser id and barcode for that board.
 */

void
get_nicstr_info(klinfo_t *k, nic_t *nicp, char *barcode)
{
    char *nic_string = 0, *tmp = 0 ;
    char laser[32] ;

    *laser = 0 ;

    /* Get the nicstring ptr based on the type of component.
       In certain cases, like the IP27, where there is bound 
       to be 2 or more nics, choose what looks to be the right string.
    */

    switch(k->struct_type) {
	case KLSTRUCT_HUB:
	    nic_string = 
	    (char *)NODE_OFFSET_TO_K1(k->nasid, ((klhub_t *)k)->hub_mfg_nic) ;
	    /* 
	       Try to find that part which has the IP27 part number.
	       If not, use the part that begins with NIC: 
	       Normally, there will be a midplane nic and a serial
	       number nic with the IP27 nic string.
	    */
            if (tmp = strstr(nic_string, IP27_PART_NUM))
		nic_string = tmp ;
	    else if (tmp = strstr(nic_string, "NIC:"))
		nic_string = tmp ;
	break ;
	case KLSTRUCT_BRI:
	    /* XXX Bank on the fact that the IO6 NIC appears first
	       followed by the MIO nic if any.
	    */
	    nic_string = 
	    (char *)NODE_OFFSET_TO_K1(k->nasid, ((klbri_t *)k)->bri_mfg_nic) ;
	break ;
	case KLSTRUCT_ROU:
	    nic_string = 
	    (char *)NODE_OFFSET_TO_K1(k->nasid, ((klrou_t *)k)->rou_mfg_nic) ;
	break ;
	case KLSTRUCT_GFX:
	    nic_string = 
	    (char *)NODE_OFFSET_TO_K1(k->nasid, ((klgfx_t *)k)->gfx_mfg_nic) ;
	break ;
	case KLSTRUCT_XBOW:
	    /* handle midplane nics in HUB case XXX */
	break ;
	case KLSTRUCT_TPU:
	    nic_string = 
	    (char *)NODE_OFFSET_TO_K1(k->nasid, ((kltpu_t *)k)->tpu_mfg_nic) ;
	break ;
	case KLSTRUCT_GSN_A:
	case KLSTRUCT_GSN_B:
	    nic_string = 
	    (char *)NODE_OFFSET_TO_K1(k->nasid, ((klgsn_t *)k)->gsn_mfg_nic) ;
	break ;
        case KLSTRUCT_XTHD:
            nic_string =
                (char *)NODE_OFFSET_TO_K1(k->nasid, ((klxthd_t *)k)->xthd_mfg_nic);
            break;
    }

    if (nic_string) {
	if (nicp) {
	    parse_bridge_nic_info(nic_string, "Laser", laser) ;
            *nicp = strtoull(laser, 0, 16) ;
	}
	if (barcode) {
	    /* Serial will be valid only if the string does not begin with NIC: */
	    if (strncmp(nic_string, "NIC:", 4))
	        parse_bridge_nic_info(nic_string, "Serial", barcode) ;
 	}
    }
}

/*
 * make_inv_rec
 *
 * Make a record of the stuff found on a board l.
 * This will be written into the nvram.
 */

void
make_inv_rec(lboard_t *l, invrec_t *ir, int count)
{
    klinfo_t 		*k ;
    int   		i ;

    bzero((char *)ir, sizeof(invrec_t)) ;

    ir->flag            |= NVINVREC_VALID ;
    ir->btype     	= l->brd_type ;
    ir->modnum     	= l->brd_module ;
    ir->slotnum     	= l->brd_slot ;
    ir->diagval      	= l->brd_diagval  ;
    ir->diagparm     	= l->brd_diagparm ;
    for (i=0; i<l->brd_numcompts; i++) {
    	k = KLCF_COMP(l, i) ;
        set_compt_flag(k, &ir->cmap) ;
	if (!(ir->nic))
	    get_nicstr_info(k, &ir->nic, ir->barcode) ;
    }
}

/* write a record to nvram */

void
write_inv_rec(invrec_t *ir, int count)
{
    ssize_t offset;

    offset=calc_pnt_nvoff(count);
    if (offset < 0) return;
    cpu_set_nvram_offset(offset, sizeof(invrec_t), (char *)ir) ;
}

/* make an inventory record for a board and write it to nvram */

int
write_brdinv(lboard_t *l, void *ap, void *rp)
{
    char	 	buf[STRUCT_PAD*sizeof(invrec_t)] ;
    invrec_t  		*ir = (invrec_t *)buf ;
    int			count = *(int *)ap ;

    /*
     * XXX Ignore midplanes/XBOWs for now. need to check nic and duplicates.
     * Ignore Graphics for now. Need to check component layout on the brd.
     * Ignore routers for now. There is a bug in the brd_slot assignment
     * in the router code.  Ignore TPU boards for now.  Ignore GSN boards
     * for now.
     */

    if ((KLCLASS(l->brd_type) == KLCLASS_ROUTER) ||
        (KLCLASS(l->brd_type) == KLCLASS_MIDPLANE) ||
        (KLCLASS(l->brd_type) == KLCLASS_GFX) ||
        (l->brd_type == KLTYPE_TPU) ||
        (l->brd_type == KLTYPE_GSN_A) ||
        (l->brd_type == KLTYPE_GSN_B)) 
                return 1 ;

    count++ ;
    *(int *)ap = count ;

    /* If rp != NULL, simply count the number of records. */

    if (rp!=NULL) return 1;

    make_inv_rec(l, ir, count-1) ;

#ifdef DEBUG
    dump_inv_rec(ir, count-1) ;
#endif

    write_inv_rec(ir, count-1) ;

    printf(".");

    return 1 ;
}

void
get_mod_map(invtabhdr_t *ih)
{
    int i = 0 ;
    unsigned long long m = 0 ;

    for (i = 1; i <= num_modules; i++) {
 	m |= (1 << module_table[i].module_id) ;
    }
    ih->modmap = m ;
}

void
check_mod_map(invtabhdr_t *ih)
{
    int i = 0 ; 
    unsigned long long m = 0 ;

    for (i = 1; i <= num_modules; i++) {
        if (!(ih->modmap & (1 << module_table[i].module_id)))
	    break ;
    }
    if (i > num_modules)
	return ;

    printf("***Warning: Module numbers different on re-boot\n") ;
    printf("The system previously contained the following module numbers\n") ;
    dump_mod_map(ih) ;
    printf("It now contains ...\n") ;
    dump_modnum() ;
    printf("***Warning: Since module numbers have changed you will see\n") ;
    printf("a lot of messages about missing and new hardware.\n") ;
}

/* write inventory records for all boards found in the system. */

void
write_inventory(void)
{
    char                buf[STRUCT_PAD*sizeof(invtabhdr_t)] ;
    invtabhdr_t		*ih = (invtabhdr_t *)buf ;
    int			count = 0 ;

    if (set_nvreg(NVOFF_INVENT_VALID, 0) == -1) {
        printf("Error in write_inventory: could not invalidate inventory\n");
        return ;
    }

    bzero((char *)ih, sizeof(buf)) ;
    cpu_get_nvram_buf(NVOFF_INVHDR, sizeof(invtabhdr_t), (char *)ih) ;

    /* count the number of boards */

    visit_lboard(write_brdinv, (void *)&count, (void *)1L);
    printf("Writing %d records", count);

    /* write current inventory. */

    count=0;
    visit_lboard(write_brdinv, (void *)&count, NULL) ;
    printf(" DONE\n");

    ih->cnt  = count ;
    ih->tver = INVTAB_REV ;
    ih->nic  = get_sys_nic() ;
    get_mod_map(ih) ;

#ifdef DEBUG
    dump_inv_hdr(ih) ;
#endif

    cpu_set_nvram_offset(NVOFF_INVHDR, sizeof(invtabhdr_t), (char *)ih) ;

    /* Validate the Inventory data */
    if (set_nvreg(NVOFF_INVENT_VALID, INVENT_VALID) == -1) {
        printf("Error in write_inventory: could not revalidate inventory\n");
        return ;
    }

    update_checksum();
    printf("Updated new configuration. Wrote %d records.\n", count) ;
}

/* 
   Search all records for a match on the module id and slot id.
   Return 0 if not found and 1 if found.
   Fill ir with the record found.
*/

int
search_nvram_inv(lboard_t *l, invrec_t *ir, int cnt)
{
    int i ;
    ssize_t offset;

    for (i=0; i<cnt; i++) {
	offset=calc_pnt_nvoff(i);
	if (offset < 0) return(0);
        cpu_get_nvram_buf(offset, sizeof(invrec_t), (char *)ir) ;
        if ((ir->modnum == l->brd_module) &&
            (ir->slotnum == l->brd_slot))
		return 1 ;
    }
    return 0 ;
}

/* From the lboard struct, find the component that will
   have a nic string and get the nic and barcode info
   from the nic string.
*/

void
get_nicinfo_from_lb(lboard_t *lb, nic_t *nicp, char *barcode)
{
    int 	i ;
    klinfo_t 	*k ;

    for (i=0; i<lb->brd_numcompts; i++) {
        k = KLCF_COMP(lb, i) ;
        get_nicstr_info(k, nicp, barcode) ;
        if ((nicp) && (*nicp))
	    break ;
    }

}

/* Check if the given board is present in nvram. This
   detects new boards.
*/
int
check_brdinv(lboard_t *l, void *ap, void *rp)
{
    int cnt = *(int *)ap ;
    invrec_t	ir ;
    char	sname[SLOTNUM_MAXLENGTH] ;
    char	barcode[MAX_BARCODE] ;

    if ((KLCLASS(l->brd_type) == KLCLASS_ROUTER) ||
        (KLCLASS(l->brd_type) == KLCLASS_MIDPLANE) ||
        (KLCLASS(l->brd_type) == KLCLASS_GFX) ||
        (l->brd_type == KLTYPE_TPU) ||
        (l->brd_type == KLTYPE_GSN_A) ||
        (l->brd_type == KLTYPE_GSN_B)) 
                return 1 ;

    bzero((char *)&ir, sizeof(invrec_t)) ;
    *barcode = 0 ;

    if (!search_nvram_inv(l, &ir, cnt)) {
	get_slotname(l->brd_slot, sname) ;
	get_nicinfo_from_lb(l, NULL, barcode) ;
	printf(
	"***Warning: Found a new %s board in module %d, slot %s, serial %s\n",
		find_board_name(l), l->brd_module, sname, barcode) ;
        printf(
	"Please use the \'update\' command from the PROM Monitor "
	"to update the inventory\n") ;
    }

    return 1 ;
}

/*
 * Component inventory related routines.
 */

/*
 * 
 */

void
cmap_to_str(unsigned int cdmap, char *string)
{
    char tmp[8];
    int i ;

    if (cdmap&0x3) {
	strcpy(string, "CPU ") ;
        if (cdmap&0x1)
	    strcat(string, "A") ;
        if (cdmap&0x2)
	    strcat(string, "B") ;
    }
    else if (cdmap&0x3fc) {
        strcpy(string, "MEM BANK ") ;
        for (i=0;i<MD_MEM_BANKS;i++) {
	    if ((1<<(i+2)) & cdmap) {
                sprintf(tmp, "%d ", i) ;
	        strcat(string, tmp) ;
	    }
        }
    }
    else if (cdmap&0x3fc0) {
        strcpy(string, "PCI DEV ") ;
        for (i=0;i<MAX_PCI_DEVS;i++) {
	    if ((1<<(i+10)) & cdmap) {
                sprintf(tmp, "%d ", i) ;
	        strcat(string, tmp) ;
	    }
        }
    }
}

void
check_compt_map(lboard_t *lb, int index)
{
    klinfo_t *k ;
    klmembnk_t *m ;
    int i;
    char        sname[SLOTNUM_MAXLENGTH] ;

    get_slotname(lb->brd_slot, sname) ;

    if (index<2) {
	k = NULL ;
	while (k = find_component(lb, k, KLSTRUCT_CPU)) {
	    if (index == k->physid) /* we found our guy */
		return ;
	}
	printf("/hw/module/%d/slot/%s:", 
		lb->brd_module, sname) ;
	if (!k)
	    printf("CPU %c missing\n", 'A' + index) ;
	return ;
    }

    if ((index >= 2) && (index < 10)) {
	k=NULL ;
	if (k = find_component(lb, k, KLSTRUCT_MEMBNK)) {
	    m = (klmembnk_t *)k ;
	    if (m->membnk_bnksz[index-2])
		return ;
	    else {
	 	printf("/hw/module/%d/slot/%s:", 
		    lb->brd_module, sname) ;
		printf("MEM BANK %d missing or disabled\n", index-2) ;
	    }
 	}
	return ;
    }

    if ((index >= 10) && (index < 18)) {
        for (i=0; i < KLCF_NUM_COMPS(lb); i++) {
	    k = KLCF_COMP(lb, i) ;
	    if ((k->struct_type != KLSTRUCT_BRI) && (k->physid == (index-10)))
	        return ;
        }
	printf("/hw/module/%d/slot/%s:", 
		lb->brd_module, sname) ;
	printf("PCI DEV %d missing\n", index-10) ;
    }
}

/*
 * Given a board that was present in the previous boot
 * check its components.
 */

void
check_compt_inv(lboard_t *lb, invrec_t *ir)
{
    int             i ;
    klinfo_t        *k;
    unsigned int    cmap = 0, cdmap = 0 ;
    char	    string[32] ;

    /* check for missing components */
    /* For all the bits set in the nvram cmap
       find the corresponding klinfo struct. */

    for (i=0; i<32 ; i++) {
	if ((1<<i) & ir->cmap)
 	    check_compt_map(lb, i) ;
    }

    /* check for new components */

    *string  = 0 ;
    for (i = 0; i < KLCF_NUM_COMPS(lb); i++) {
        k = KLCF_COMP(lb, i);
  	set_compt_flag(k, &cmap) ;
        if ((ir->cmap & cmap) != cmap) {
	    cdmap = (~ir->cmap & cmap) ;
	    cmap_to_str(cdmap, string) ;
	    printf("\tFound new or re-enabled component %s\n", string) ;
	}
    }
}

/*
 * Check the old inventory records in nvram to the real
 * inventory found during this boot. Report any missing
 * hardware as well as new hardware.
 */

void
check_inventory()
{
    char                buf[STRUCT_PAD*sizeof(invtabhdr_t)] ;
    invtabhdr_t         *ih = (invtabhdr_t *)buf ;
    char                buf1[STRUCT_PAD*sizeof(invrec_t)] ;
    invrec_t            *ir = (invrec_t *)buf1 ;
    int                 i ;
    lboard_t		lb, *rlb = 0 ;
    nic_t		nic = 0 ;
    char		barcode[MAX_BARCODE] ;
    char		sname[SLOTNUM_MAXLENGTH] ;
    ssize_t		offset;

    *barcode = 0 ;
    printf("Checking hardware inventory ...............\t\t");
    sysctlr_message("Chk Inv");

    /* Don't bother running this code if it doesn't look like the
     * inventory is valid.  Instead, reinitialize the inventory.
     */
    if (get_nvreg(NVOFF_INVENT_VALID) != INVENT_VALID) {
        printf("\nWARNING: hardware inventory is invalid.  Reinitializing...\n");
        write_inventory();
	return ;
    }

    bzero((char *)ih, sizeof(buf)) ;
    cpu_get_nvram_buf(NVOFF_INVHDR, sizeof(invtabhdr_t), (char *)ih) ;

    if ((!ih->nic) && !(SN00)) {
	printf("\nWarning: Inventory table ID value is 0. Check Midplane NIC\n") ;
        write_inventory();
        return ;
    }

    if (!check_sys_nic(ih->nic)) {
        printf("\nWARNING: This NVRAM did not belong to this system.\
Reinitializing...\n");
        write_inventory();
        return ;
    }

    check_mod_map(ih) ;

    for (i=0; i<ih->cnt; i++) {
	offset=calc_pnt_nvoff(i);
	if (offset < 0) continue;
        cpu_get_nvram_buf(offset, sizeof(invrec_t), (char *)ir) ;
	if (!(ir->flag & NVINVREC_VALID))
	    continue ;
	lb.brd_type   = ir->btype ;
        lb.brd_module = ir->modnum ;
        lb.brd_slot   = ir->slotnum ;
	get_slotname(ir->slotnum, sname) ;
        rlb = get_match_lb(&lb, 0) ;
        if ((!rlb) || (!(rlb->brd_flags & ENABLE_BOARD))) {
            printf("\n***Warning: Board in module %d, slot %s is missing or disabled\n",
                ir->modnum, sname) ;
	  if( ir->btype == KLTYPE_IP27)
            printf("It previously contained a node-board, barcode %s laser %x\n", 
	 	ir->barcode, ir->nic) ;
	  else
            printf("It previously contained a %s board, barcode %s laser %x\n", 
	 	BOARD_NAME(ir->btype), ir->barcode, ir->nic) ;
        } else {
            nic = 0 ;
            get_nicinfo_from_lb(rlb, &nic, barcode) ;
            if (ir->nic != nic) {
                printf("\n***Warning: Found a different board in module %d, slot %s,\n",
                        lb.brd_module, sname) ;
                printf("Previous barcode was %s, New barcode is %s\n", 
			ir->barcode, barcode) ;
            }
	    else /* The board looks good. check its components */
	        check_compt_inv(rlb, ir) ;
	}
    }

    /* Check for new boards */

    visit_lboard(check_brdinv, (void *)&ih->cnt, NULL) ;

    printf("DONE\n") ;
}

/*
 * The update command rewrites the current state of the machine
 * into the inventory nvram.
 * update -dump is a debug option to dumpout the inventory table.
 */

/*ARGSUSED*/
int
update_cmd(int argc, char **argv)
{
    if (argv[1] && (!strcmp(argv[1], "-dump"))) {
	dump_inventory() ;
	return 0 ;
    }

    (void)write_inventory();

    return 0;
}

/*
 * Print the module numbers of all modules found
 * currently.
 */

/*ARGSUSED*/
int
modnum_cmd(int argc, char **argv)
{
    dump_modnum() ;
    return 0 ;
}
 


