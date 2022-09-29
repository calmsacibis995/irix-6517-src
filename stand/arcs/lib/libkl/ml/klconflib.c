/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***********************************************************************\
*	File:		libklconf.c					*
*									*
*	Contains code that manipulates the KLconfig structure in 	*
*		various useful ways.					*
*									*
\***********************************************************************/

#ident  "$Revision: 1.49 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/iograph.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/gda.h>
#include <sys/SN/slotnum.h>

#ifdef _STANDALONE
#include <libsk.h>
#include <libkl.h>
#include <libsc.h>
#include <kllibc.h>		/* for macros like isalpha etc */
#else
#include <ctype.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/systm.h>
#define printf dprintf
#endif

#define LDEBUG 0
#define NIC_UNKNOWN ((nic_t) -1)

static void sort_nic_names(lboard_t *) ;

lboard_t *
find_lboard(lboard_t *start, unsigned char brd_type)
{
	/* Search all boards stored on this node. */
	while (start) {
		if (start->brd_type == brd_type)
			return start;
		start = KLCF_NEXT(start);
	}

	/* Didn't find it. */
	return (lboard_t *)NULL;
}

lboard_t *
find_lboard_class(lboard_t *start, unsigned char brd_type)
{
	/* Search all boards stored on this node. */
	while (start) {
		if (KLCLASS(start->brd_type) == KLCLASS(brd_type))
			return start;
		start = KLCF_NEXT(start);
	}

	/* Didn't find it. */
	return (lboard_t *)NULL;
}

klinfo_t *
find_component(lboard_t *brd, klinfo_t *kli, unsigned char struct_type)
{
	int index, j;

	if (kli == (klinfo_t *)NULL) {
		index = 0;
	} else {
		for (j = 0; j < KLCF_NUM_COMPS(brd); j++) {
			if (kli == KLCF_COMP(brd, j))
				break;
		}
		index = j;
		if (index == KLCF_NUM_COMPS(brd)) {
			printf("find_component: Bad pointer: 0x%x\n", kli);
			return (klinfo_t *)NULL;
		}
		index++;	/* next component */
	}
	
	for (; index < KLCF_NUM_COMPS(brd); index++) {		
		kli = KLCF_COMP(brd, index);
		if (KLCF_COMP_TYPE(kli) == struct_type)
			return kli;
	}

	/* Didn't find it. */
	return (klinfo_t *)NULL;
}

klinfo_t *
find_first_component(lboard_t *brd, unsigned char struct_type)
{
	return find_component(brd, (klinfo_t *)NULL, struct_type);
}

#ifdef _STANDALONE

lboard_t *
find_nic_lboard(lboard_t *start, nic_t nic)
{
	while (start) {
		if (start->brd_nic == nic)
		    return start;
		start = KLCF_NEXT(start);
	}
	
	return NULL;
}

/* ARGSUSED */
lboard_t *
find_nic_type_lboard(nasid_t nasid, unsigned char brd_type, nic_t nic)
{
	lboard_t *brd;

	if (nasid == INVALID_NASID) 
	    brd = KL_CONFIG_INFO(get_nasid());
	else
	    brd = KL_CONFIG_INFO(nasid);

	if (nic == NIC_UNKNOWN)
	    return find_lboard(brd, brd_type);

	if ((brd = find_nic_lboard(brd, nic)) == NULL) {
#if LDEBUG
		printf("find_nic_type_lboard: could not find nic %x\n", nic) ;
		/* This statement went away when bringup was taken out. */
		/* return find_lboard(brd, brd_type); */
#endif
	    return NULL;
	}
	
	if (brd->brd_type == brd_type) 
	    return brd;

#if LDEBUG
	/* XXX Till this is tested with meta routers and multiple
	   routers lboards on the same KLCONFIG this return should 
	   be included. 
		return find_lboard(brd, brd_type);
	*/

	printf("find_nic_type_lboard: \
		matching nic found, but brd_types do not match\n");
#endif

        return NULL;

}

#endif /* _STANDALONE */

lboard_t *
find_lboard_modslot(lboard_t *start, moduleid_t mod, slotid_t slot)
{
	/* Search all boards stored on this node. */
	while (start) {
		if ((start->brd_module == mod) &&
		    (start->brd_slot == slot))
			return start;
		start = KLCF_NEXT(start);
	}

	/* Didn't find it. */
	return (lboard_t *)NULL;
}

#if defined (_STANDALONE)
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#endif /* STANDALONE */

#define tolower(c)	(isupper(c) ? (c) - 'A' + 'a' : (c))


/*
 * Convert a NIC name to a name for use in the hardware graph.
 */
void
nic_name_convert(char *old_name, char *new_name)
{
        int i;
        char c;
        char *compare_ptr;

	if ((old_name[0] == '\0') || (old_name[1] == '\0')) {
                strcpy(new_name, EDGE_LBL_XWIDGET);
        } else {
                for (i = 0; i < strlen(old_name); i++) {
                        c = old_name[i];

                        if (isalpha(c))
                                new_name[i] = tolower(c);
                        else if (isdigit(c))
                                new_name[i] = c;
                        else
                                new_name[i] = '_';
                }
                new_name[i] = '\0';
        }

        /* XXX -
         * Since a bunch of boards made it out with weird names like
         * IO6-fibbbed and IO6P2, we need to look for IO6 in a name and
         * replace it with "baseio" to avoid confusion in the field.
	 * We also have to make sure we don't report media_io instead of
	 * baseio.
         */

        /* Skip underscores at the beginning of the name */
        for (compare_ptr = new_name; (*compare_ptr) == '_'; compare_ptr++)
                ;

	/*
	 * Check for some names we need to replace.  Early boards
	 * had junk following the name so check only the first
	 * characters.
	 */
        if (!strncmp(new_name, "io6", 3) || 
            !strncmp(new_name, "mio", 3) || 
	    !strncmp(new_name, "media_io", 8))
		strcpy(new_name, "baseio");
	else if (!strncmp(new_name, "ip29", 4))
		strcpy(new_name,SN00_MOTHERBOARD);
        else if (!strncmp(new_name, "divo", 4))
                strcpy(new_name, "divo") ;

}

/*
 * Find the lboard structure and get the board name.
 * If we can't find the structure or it's too low a revision,
 * use default name.
 */
lboard_t *
get_board_name(nasid_t nasid, moduleid_t mod, slotid_t slot, char *name)
{
	lboard_t *brd;

	brd = find_lboard_modslot((lboard_t *)KL_CONFIG_INFO(nasid),
				  mod, slot);

#ifndef _STANDALONE
	{
		cnodeid_t cnode = NASID_TO_COMPACT_NODEID(nasid);

		if (!brd && (NODEPDA(cnode)->xbow_peer != INVALID_NASID))
			brd = find_lboard_modslot((lboard_t *)
				KL_CONFIG_INFO(NODEPDA(cnode)->xbow_peer),
				mod, slot);
	}
#endif

	if (!brd || (brd->brd_sversion < 2)) {
		strcpy(name, EDGE_LBL_XWIDGET);
	} else {
		nic_name_convert(brd->brd_name, name);
	}

        /*
         * PV # 540860
         * If the name is not 'baseio' or SN00 MOTHERBOARD
         * get the lowest of all the names in the nic string.
         * This is needed for boards like divo, which can have
         * a bunch of daughter cards, but would like to be called
         * divo. We could do this for baseio and SN00 MOTHERBOARD
         * but it has some special case names that we would not
         * like to disturb at this point.
         */

	/* gfx boards don't need any of this name scrambling */
        if (brd && (KLCLASS(brd->brd_type) == KLCLASS_GFX)) {	
		return(brd);
	}

        if (!(!strcmp(name, "baseio") || (!strcmp(name, SN00_MOTHERBOARD)))) {
		if (brd) {
                	sort_nic_names(brd) ;
                	/* Convert to small case, '-' to '_' etc */
                	nic_name_convert(brd->brd_name, name) ;
		}
        }

	return(brd);
}

cpuid_t
get_cnode_cpu(cnodeid_t cnode)
{
	int slice;
	nasid_t nasid;
	gda_t *gdap = GDA;
	klcpu_t *acpu;

	if ((nasid = gdap->g_nasidtable[cnode]) == INVALID_NASID)
	    return NULL;

	for (slice = 0; slice < CPUS_PER_NODE; slice++) {
		acpu = nasid_slice_to_cpuinfo(nasid, slice);
		if (acpu && KLCONFIG_INFO_ENABLED(&acpu->cpu_info)) {
			if (!(acpu->cpu_info.virtid < MAXCPUS))
			    printf("FATAL ERROR: get_cnode_cpu cnode 0x%x cpuid 0x%x exceeds max 0x%x\n", cnode, acpu->cpu_info.virtid, MAXCPUS);
			return acpu->cpu_info.virtid;
		}
	}
	return -1;
}


#if defined (_STANDALONE)

cnodeid_t
get_cpu_cnode(int cpu)
{
	cnodeid_t cnode;
	int slice;

	get_cpu_cnode_slice(cpu, &cnode, &slice);
	return cnode;
}

void
get_cpu_cnode_slice(cpuid_t cpu, cnodeid_t *cnodeid_p, int *slice_p)
{
	gda_t *gdap = GDA;
	cnodeid_t cnode = cpu >> CPUS_PER_NODE_SHFT;
	nasid_t nasid;
	int slice;
	klcpu_t *acpu;

	*slice_p = -1;
	*cnodeid_p = INVALID_CNODEID;
	for (; cnode < MAX_COMPACT_NODES; cnode++) {
		if ((nasid = gdap->g_nasidtable[cnode]) == INVALID_NASID)
		    return;

		for (slice = 0; slice < CPUS_PER_NODE; slice++) {
			acpu = nasid_slice_to_cpuinfo(nasid, slice);
			if (acpu) {
				if (acpu->cpu_info.virtid == cpu) {
					*slice_p = acpu->cpu_info.physid;
					*cnodeid_p = cnode;
					return;
				}
			}
		}
	}
	return;
}

#else

cnodeid_t
get_cpu_cnode(int cpu)
{
	ASSERT(cpu < MAXCPUS);
	return CPUID_TO_COMPACT_NODEID(cpu);
}


#endif	

int
get_cpu_slice(cpuid_t cpu)
{
	klcpu_t *acpu;

	if ((acpu = get_cpuinfo(cpu)) == NULL)
	    return -1;
	return acpu->cpu_info.physid;
}


#if defined (_STANDALONE)
int
get_nasid_slice(cpuid_t cpu, nasid_t *nasid, int *slice)
{
	cnodeid_t	cnode;
	int sl;

	get_cpu_cnode_slice(cpu, &cnode, slice);

	if ((cnode > MAX_COMPACT_NODES) || (cnode < 0))
		return -1;

	*nasid = GDA->g_nasidtable[cnode];
	if (*nasid == INVALID_NASID) {
		*slice = -1;
		return -1;
	}
	return 0;
}


cpuid_t
cnode_slice_to_cpuid(cnodeid_t cnode, int slice)
{
	klcpu_t *acpu;
	int sl;
	nasid_t nasid;

	if ((nasid = GDA->g_nasidtable[cnode]) == INVALID_NASID) 
	    return -1;

	for (sl = 0; sl < CPUS_PER_NODE; sl++) {
		acpu = nasid_slice_to_cpuinfo(nasid, sl);
		if (acpu && acpu->cpu_info.physid == slice)
		    return acpu->cpu_info.virtid;
	}
	return -1;
}
	
#endif

klcpu_t *
get_cpuinfo(cpuid_t cpu)
{
	nasid_t nasid;
	int slice;
	klcpu_t *acpu;
	gda_t *gdap = GDA;
	cnodeid_t cnode;

	if (!(cpu < MAXCPUS)) {
		printf ("get_cpuinfo: illegal cpuid 0x%x\n", cpu);
		return NULL;
	}
	    
	cnode = get_cpu_cnode(cpu);
	if (cnode == INVALID_CNODEID) {
		return NULL;
	}

	if ((nasid = gdap->g_nasidtable[cnode]) == INVALID_NASID)
	    return NULL;

	for (slice = 0; slice < CPUS_PER_NODE; slice++) {
		acpu = nasid_slice_to_cpuinfo(nasid, slice);
		if (acpu && acpu->cpu_info.virtid == cpu)
		    return acpu;
	}
	return NULL;
}

/*
 * get_actual_nasid
 *
 *      Completely disabled brds have their klconfig on
 *      some other nasid as they have no memory. But their
 *      actual nasid is hidden in the klconfig. Use this
 *      routine to get it. Works for normal boards too.
 */
nasid_t
get_actual_nasid(lboard_t *brd)
{
        klhub_t *hub ;

        if (!brd)
                return INVALID_NASID ;

        /* find out if we are a completely disabled brd. */

        hub  = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
        if (!hub)
                return INVALID_NASID ;
        if (!(hub->hub_info.flags & KLINFO_ENABLE))     /* disabled node brd */
                return hub->hub_info.physid ;
        else
                return brd->brd_nasid ;
}

klcpu_t *
nasid_slice_to_cpuinfo(nasid_t nasid, int slice)
{
	lboard_t *brd;
	klcpu_t *acpu;

	if (!(brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
				   KLTYPE_IP27))) {
#ifdef _STANDALONE
		printf("nasid_slice_to_cpuinfo: Can't find IP27 board on nasid %d\n",
			nasid);
#endif
		return (klcpu_t *)NULL;
	}

	if (!(acpu = (klcpu_t *)find_first_component(brd, KLSTRUCT_CPU))) {
#ifdef _STANDALONE
		printf("nasid_slice_to_cpuinfo: Can't find any CPU structures for cpu %d (nasid %d)\n",
			slice, nasid);
#endif
		return (klcpu_t *)NULL;
	}

	do {
		if ((acpu->cpu_info.physid) == slice)
			return acpu;
	} while (acpu = (klcpu_t *)find_component(brd, (klinfo_t *)acpu,
                                                  KLSTRUCT_CPU));

	return (klcpu_t *)NULL;
}

#ifdef _STANDALONE

int
config_find_nic_router(nasid_t nasid, nic_t nic, 
		       lboard_t **brd_p, klrou_t **comp_p)
{
        lboard_t *brd;
        klinfo_t *comp;

        if (nic==NIC_UNKNOWN)
                brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
                                        KLTYPE_ROUTER2);
        else
                brd = find_nic_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
                                nic);

        if (brd == NULL) return -1;

        comp = find_component(brd, NULL, KLSTRUCT_ROU);
        if (comp == NULL) return -1;

        if (brd_p) *brd_p = brd;
        if (comp_p) *comp_p = (klrou_t *)comp;

        return 0;

}


int
config_find_nic_hub(nasid_t nasid, nic_t nic, 
		       lboard_t **brd_p, klhub_t **comp_p)
{
        lboard_t *brd;
        klinfo_t *comp;

	if (nic == NIC_UNKNOWN)
        	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
			KLTYPE_IP27);
	else
        	brd = find_nic_lboard((lboard_t *)KL_CONFIG_INFO(nasid), 
				nic);

        if (brd == NULL) return -1;

        comp = find_component(brd, NULL, KLSTRUCT_HUB);
        if (comp == NULL) return -1;

        if (brd_p) *brd_p = brd;
        if (comp_p) *comp_p = (klhub_t *)comp;

        return 0;

}

int
config_find_xbow(nasid_t nasid, lboard_t **brd_p, klxbow_t **comp_p)
{
	lboard_t *brd;
	klinfo_t *comp;

	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_MIDPLANE8);
	if (brd == NULL) return -1;

	comp = find_component(brd, NULL, KLSTRUCT_XBOW);
	if (comp == NULL) return -1;

	if (brd_p) *brd_p = brd;
	if (comp_p) *comp_p = (klxbow_t *)comp;

	return 0;
}

#endif /* _STANDALONE */

int
xbow_port_io_enabled(nasid_t nasid, int link)
{
	lboard_t *brd;
	klxbow_t *xbow_p;

	if ((brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_MIDPLANE8)) == NULL)
	    return 0;
		
	if ((xbow_p = (klxbow_t *)find_component(brd, NULL, KLSTRUCT_XBOW))
	    == NULL)
	    return 0;

	if (!XBOW_PORT_TYPE_IO(xbow_p, link) || !XBOW_PORT_IS_ENABLED(xbow_p, link))
	    return 0;

	return 1;
}

void
board_to_path(lboard_t *brd, char *path)
{
	moduleid_t modnum;
	slotid_t slot;
	char *board_name;
	char slot_name[SLOTNUM_MAXLENGTH];

	ASSERT(brd);

	switch (KLCLASS(brd->brd_type)) {

		case KLCLASS_NODE:
			board_name = EDGE_LBL_NODE;
			break;
		case KLCLASS_ROUTER:
			if (brd->brd_type == KLTYPE_META_ROUTER)
				board_name = EDGE_LBL_META_ROUTER;
			else
				board_name = EDGE_LBL_ROUTER;
			break;
		case KLCLASS_MIDPLANE:
			board_name = EDGE_LBL_MIDPLANE;
			break;
		case KLCLASS_IO:
			board_name = EDGE_LBL_IO;
			break;
		default:
			board_name = EDGE_LBL_UNKNOWN;
	}
			
	modnum = brd->brd_module;

	slot = brd->brd_slot;
	get_slotname(slot, slot_name);

	ASSERT(modnum >= 0);

	sprintf(path, EDGE_LBL_MODULE "/%d/"
                EDGE_LBL_SLOT "/%s/%s", modnum, slot_name, board_name);
}

/*
 * Get the module number for a NASID.
 */
moduleid_t
get_module_id(nasid_t nasid)
{
	lboard_t *brd;

	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);

	if (!brd)
		return INVALID_MODULE;
	else
		return brd->brd_module;
}

#define	STAR_RTR_MASK	((1 << 1) | (1 << 2) | (1 << 3) | (1 << 6))

static int star_rtr(lboard_t *rtr)
{
	klrou_t		*rou_comp;
	int		i;
	
	if (rtr->brd_type != KLTYPE_ROUTER2)
		return 0;

	if (!(rou_comp = (klrou_t *) find_first_component(rtr, KLSTRUCT_ROU)))
		return 0;

	for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
		if (!(STAR_RTR_MASK & (1 << i)))
			continue;

		if (rou_comp->rou_port[i].port_nasid != INVALID_NASID) {
			lboard_t	*l;

			l = (lboard_t *) NODE_OFFSET_TO_K1(
				rou_comp->rou_port[i].port_nasid,
				rou_comp->rou_port[i].port_offset);

			if (l->brd_type == KLTYPE_IP27)
				return 1;
		}
	}

	return 0;
}

static int rtr_cnnectd_brds(lboard_t *brd, lboard_t **brd_ptrs, int port_mask)
{
        klrou_t         *router;
        int             i, n = 0;
	lboard_t	*abrd;

        router = (klrou_t *) find_first_component(brd, KLSTRUCT_ROU);
        if (!router)
                return -1;

        for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
                if (!(port_mask & (1 << i)))
                        continue;

                if (router->rou_port[i].port_nasid != INVALID_NASID) {
                        abrd = (lboard_t *) NODE_OFFSET_TO_K1(
                                router->rou_port[i].port_nasid,
                                router->rou_port[i].port_offset);
			if (abrd->brd_type == KLTYPE_IP27) {
				*brd_ptrs = abrd;
                        	brd_ptrs++;
                        	n++;
			}
                }
        }

        return n;
}

/*
 * Function:		module_brds -> finds out all IP27 brds in a module
 * Args:		nasid -> any one nasid in the module
 *			module_brds -> array of ptrs to fill up
 *			n -> size of array; 4 for SN0
 * Note:		This finds out who's in a module by following rtr
 *			links starting from a IP27 brd.
 *			Supports SN00, null rtr and normal rtr config.
 *			Unsupported: KLTYPE_META_ROUTER
 */

int module_brds(nasid_t nasid, lboard_t **module_brds, int n)
{
        lboard_t        *brd, *old_brd;
        klhub_t         *hub;
	klrou_t		*router;
        int             indx = 0, rc, port_mask;

        bzero((void *) module_brds, n * sizeof(lboard_t *));

        brd = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid), KLTYPE_IP27);
        if (!brd) {
                printf("module_brds: ERROR: No IP27 on nasid %d\n", nasid);
                return -1;
        }

	if (SN00) {
                module_brds[indx++] = brd;
		return 0;
	}

        hub = (klhub_t *) find_first_component(brd, KLSTRUCT_HUB);
        if (!hub) {
                printf("module_brds: ERROR: No hub component for nasid %d\n",
                                        nasid);
                module_brds[indx++] = brd;
                return -1;
        }

	/* If local link is down check for peer hub on xbow */
        if (hub->hub_port.port_nasid == INVALID_NASID) {
		if (peer_hub_on_xbow(brd->brd_nasid)) {
			printf("WARNING: Local network link is down on nasid "
			"%d slot n%d\n", brd->brd_nasid, 
			SLOTNUM_GETSLOT(brd->brd_slot));
			printf("WARNING: Modules may NOT be numbered properly\n");
			return -1;
		}
                module_brds[indx++] = brd;
                return 0;
        }

	old_brd = brd;

        brd = (lboard_t *) NODE_OFFSET_TO_K1(hub->hub_port.port_nasid,
                                                hub->hub_port.port_offset);

        /* null rtr case */
        if (brd->brd_type != KLTYPE_ROUTER2) {
		if (brd->brd_type == KLTYPE_IP27) {
                	module_brds[indx++] = old_brd;
                	module_brds[indx++] = brd;
                	return 0;
		}
		else {
			printf("WARNING: Brd connected to HUB nasid %d is "
				"neither IP27 nor RTR\n", old_brd->brd_nasid);
			return -1;
		}
        }

        port_mask = (1 << 4) | (1 << 5);

        if ((rc = rtr_cnnectd_brds(brd, &module_brds[indx], port_mask)) < 0)
		return -1;
        else
		indx += rc;

	/* Star rtr case */
	if (star_rtr(brd)) {
		port_mask = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 6);;
		if (rtr_cnnectd_brds(brd, &module_brds[indx], port_mask) < 0)
			return -1;
		else
			return 0;
	}

        router = (klrou_t *) find_first_component(brd, KLSTRUCT_ROU);
        if (!router)
                return -1;

	/* Link 6 appears to be down. Check for both peers */
        if (router->rou_port[6].port_nasid == INVALID_NASID) {
		int	i;

		for (i = 0; i < indx; i++) {
			if (module_brds[i]->brd_type != KLTYPE_IP27)
				continue;

			if (peer_hub_on_xbow(brd->brd_nasid)) {
                        	printf("WARNING: Network link 6 is down "
				"on nasid %d slot n%d\n", brd->brd_nasid, 
				SLOTNUM_GETSLOT(brd->brd_slot));

                        	printf("WARNING: Modules may NOT be numbered "
				"properly\n");

                        	return -1;
			}
		}
                return 0;
	}

        brd = (lboard_t *) NODE_OFFSET_TO_K1(router->rou_port[6].port_nasid,
                                router->rou_port[6].port_offset);

        if (brd->brd_type != KLTYPE_ROUTER2)
		return -1;

	port_mask = (1 << 4) | (1 << 5);

        if ((rc = rtr_cnnectd_brds(brd, &module_brds[indx], port_mask)) < 0)
		return -1;
        else
                indx += rc;

        return 0;
}

/*
 * Function:		brd_from_key -> returns brd_ptr
 * Args:		key -> takes in key
 * Note:		Can be called only aftet klconfig has been inited
 */

lboard_t *brd_from_key(ulong_t key)
{
    nasid_t 	nasid;
    int		wid_num, slot;
    lboard_t	*brd_ptr;

    nasid = GET_DEVNASID_FROM_KEY(key);
    wid_num = GET_WIDID_FROM_KEY(key);
    if (SN00 && !is_xbox_config(nasid))
	slot = get_node_slotid(nasid);
    else if (CONFIG_12P4I)
	slot = SLOTNUM_XTALK_CLASS | SLOTNUM_GETSLOT(get_node_slotid(nasid));
    else
	slot = xbwidget_to_xtslot(get_node_crossbow(nasid), wid_num);
    brd_ptr = (lboard_t *) KL_CONFIG_INFO(nasid);

    while (brd_ptr) {
        if ((KLCLASS(brd_ptr->brd_type) == KLCLASS_IO) && 
			(brd_ptr->brd_slot == slot))
            return brd_ptr;

        brd_ptr = KLCF_NEXT(brd_ptr);
    }

    return NULL;
}


/*
 * Get the serial number of the main  component of a board
 * Returns 0 if a valid serial number is found
 * 1 otherwise.
 * Assumptions: Nic manufacturing string  has the following format
 *			*Serial:<serial_number>;*
 */
static int
component_serial_number_get(lboard_t 		*board,
			    klconf_off_t 	mfg_nic_offset,
			    char		*serial_number,
			    char		*key_pattern)
{

	char	*mfg_nic_string,*str;
	char	*serial_string;
	int	i;
	char	*serial_pattern = "Serial:";

	/* We have an error on a null mfg nic offset */
	if (!mfg_nic_offset)
		return(1);
	/* Get the hub's manufacturing nic information
	 * which is in the form of a pre-formatted string
	 */
	mfg_nic_string = 
		(char *)NODE_OFFSET_TO_K1(NASID_GET(board),
					  mfg_nic_offset);
	/* There is no manufacturing nic info */
	if (!mfg_nic_string)
		return(1);

	str = mfg_nic_string;
	/* Look for the key pattern first (if it is  specified)
	 * and then print the serial number corresponding to that.
	 */
	if (strcmp(key_pattern,"") && 
	    !(str = strstr(mfg_nic_string,key_pattern)))
		return(1);


	/* There is no serial number info in the manufacturing
	 * nic info
	 */
	if (!(serial_string = strstr(str,serial_pattern)))
		return(1);

	serial_string = serial_string + strlen(serial_pattern);
	/*  Copy the serial number information from the klconfig */
	i = 0;
	while (serial_string[i] != ';') {
		serial_number[i] = serial_string[i];
		i++;
	}
	serial_number[i] = 0;
	
	return(0);
}
/*
 * Get the serial number of a board
 * Returns 0 if a valid serial number is found
 * 1 otherwise.
 */

int
board_serial_number_get(lboard_t *board,char *serial_number)
{
	ASSERT(board && serial_number);
	if (!board || !serial_number)
		return(1);

	strcpy(serial_number,"");
	switch(KLCLASS(board->brd_type)) {
	case KLCLASS_CPU: {	/* Node board */
		klhub_t	*hub;
		
		/* Get the hub component information */
		hub = (klhub_t *)find_first_component(board,
						      KLSTRUCT_HUB);
		/* If we don't have a hub component on an IP27
		 * then we have a weird klconfig.
		 */
		if (!hub)
			return(1);
		/* Get the serial number information from
		 * the hub's manufacturing nic info
		 */
		if (component_serial_number_get(board,
						hub->hub_mfg_nic,
						serial_number,
						"IP27"))
			/* Try with IP31 key if IP27 key fails */
			if (component_serial_number_get(board,
							hub->hub_mfg_nic,
							serial_number,
							"IP31"))
				return(1);
		break;
	}
	case KLCLASS_IO: {	/* IO board */
		klbri_t	*bridge;
		
		/* Get the bridge component information */
		bridge = (klbri_t *)find_first_component(board,
							 KLSTRUCT_BRI);
		/* If we don't have a bridge component on an IO board
		 * then we have a weird klconfig.
		 */
		if (!bridge)
			return(1);
		/* Get the serial number information from
		 * the bridge's manufacturing nic info
		 */
		if (component_serial_number_get(board,
						bridge->bri_mfg_nic,
						serial_number,
						""))
			return(1);
		break;
	}
	case KLCLASS_ROUTER: {	/* Router board */
		klrou_t *router;	
		
		/* Get the router component information */
		router = (klrou_t *)find_first_component(board,
							 KLSTRUCT_ROU);
		/* If we don't have a router component on a router board
		 * then we have a weird klconfig.
		 */
		if (!router)
			return(1);
		/* Get the serial number information from
		 * the router's manufacturing nic info
		 */
		if (component_serial_number_get(board,
						router->rou_mfg_nic,
						serial_number,
						""))
			return(1);
		break;
	}
	case KLCLASS_GFX: {	/* Gfx board */
		klgfx_t *graphics;
		
		/* Get the graphics component information */
		graphics = (klgfx_t *)find_first_component(board, KLSTRUCT_GFX);
		/* If we don't have a gfx component on a gfx board
		 * then we have a weird klconfig.
		 */
		if (!graphics)
			return(1);
		/* Get the serial number information from
		 * the graphics's manufacturing nic info
		 */
		if (component_serial_number_get(board,
						graphics->gfx_mfg_nic,
						serial_number,
						""))
			return(1);
		break;
	}
	default:
		strcpy(serial_number,"");
		break;
	}
	return(0);
}

/* NIC Sorting Support */

#define MAX_NICS_PER_STRING     32
#define MAX_NIC_NAME_LEN        32

static char *
get_nic_string(lboard_t *lb)
{
        int             i;
        klinfo_t        *k = NULL ;
        klconf_off_t    mfg_off = NULL ;
        char            *mfg_nic = NULL ;

        for (i = 0; i < KLCF_NUM_COMPS(lb); i++) {
                k = KLCF_COMP(lb, i) ;
                switch(k->struct_type) {
                        case KLSTRUCT_BRI:
                                mfg_off = ((klbri_t *)k)->bri_mfg_nic ;
                                break ;

                        case KLSTRUCT_HUB:
                                mfg_off = ((klhub_t *)k)->hub_mfg_nic ;
                                break ;

                        case KLSTRUCT_ROU:
                                mfg_off = ((klrou_t *)k)->rou_mfg_nic ;
                                break ;

                        case KLSTRUCT_GFX:
                                mfg_off = ((klgfx_t *)k)->gfx_mfg_nic ;
                                break ;

        		case KLSTRUCT_XTHD:
            			mfg_off = ((klxthd_t *)k)->xthd_mfg_nic ;
        			break;

                        default:
                                mfg_off = NULL ;
                                break ;
                }
                if (mfg_off)
                        break ;
        }

        if ((mfg_off) && (k))
                mfg_nic = (char *)NODE_OFFSET_TO_K1(k->nasid, mfg_off) ;

        return mfg_nic ;
}

char *
get_first_string(char **ptrs, int n)
{
        int     i ;
        char    *tmpptr ;

        if ((ptrs == NULL) || (n == 0))
                return NULL ;

        tmpptr = ptrs[0] ;

        if (n == 1)
                return tmpptr ;

        for (i = 0 ; i < n ; i++) {
                if (strcmp(tmpptr, ptrs[i]) > 0)
                        tmpptr = ptrs[i] ;
        }

        return tmpptr ;
}

int
get_ptrs(char *idata, char **ptrs, int n, char *label)
{
        int     i = 0 ;
        char    *tmp = idata ;

        if ((ptrs == NULL) || (idata == NULL) || (label == NULL) || (n == 0))
                return 0 ;

        while (tmp = strstr(tmp, label)) {
                tmp += strlen(label) ;
                /* check for empty name field, and last NULL ptr */
                if ((i < (n-1)) && (*tmp != ';')) {
                        ptrs[i++] = tmp ;
                }
        }

        ptrs[i] = NULL ;

        return i ;
}

/*
 * sort_nic_names
 *
 *      Does not really do sorting. Find the alphabetically lowest
 *      name among all the nic names found in a nic string.
 *
 * Return:
 *      Nothing
 *
 * Side Effects:
 *
 *      lb->brd_name gets the new name found
 */

static void
sort_nic_names(lboard_t *lb)
{
        char    *nic_str ;
        char    *ptrs[MAX_NICS_PER_STRING] ;
        char    name[MAX_NIC_NAME_LEN] ;
        char    *tmp, *tmp1 ;
	char	tmp_name[MAX_NIC_NAME_LEN] ;

	*name = 0 ;

        /* Get the nic pointer from the lb */

        if ((nic_str = get_nic_string(lb)) == NULL)
                return ;

        tmp = get_first_string(ptrs,
                get_ptrs(nic_str, ptrs, MAX_NICS_PER_STRING, "Name:")) ;

        if (tmp == NULL)
                return ;

        if (tmp1 = strchr(tmp, ';')) {
                strncpy(name, tmp, tmp1-tmp) ;
                name[tmp1-tmp] = 0 ;
        } else {
                strncpy(name, tmp, (sizeof(name) -1)) ;
                name[sizeof(name)-1] = 0 ;
        }

	/* gfx brds already have done nic name convert and depend on it. */

	if (KLCLASS(lb->brd_type) == KLCLASS_GFX) {
		nic_name_convert(name, tmp_name) ;
		strcpy(name, tmp_name) ;
	}

        strcpy(lb->brd_name, name) ;
}

