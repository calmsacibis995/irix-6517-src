#ident "$Header: "

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/invent.h>
#include <klib/klib.h>

extern int mem_validity_check;

/* The node memory array is sized dynamically at startup based on the
 * maximum number of nodes that could be installed in a system. Note
 * that the nasid for each node, not the nodeid, is used to index into 
 * the array. This is because we want to map each node to the physical 
 * address space it controls.
 */
node_memory_t **node_memory;

/*
 * check_node_memory()
 *
 *   Check to see if the node_memory table has been initialized. If
 *   it hasn't, or if the system doesn't contain node memory, then
 *   return 1. Otherwise, return 0.
 */
int
check_node_memory()
{
	if (node_memory) {
		return(0);
	}
	fprintf(KL_ERRORFP, "Initializeing node memory table...\n");
	return(map_system_memory());
}

/*
 * map_system_memory()
 */
int
map_system_memory()
{
	if (K_IP == 27) {
		return(map_node_memory());
	}

	node_memory = (node_memory_t**)kl_alloc_block(1, K_PERM);

	/* Allocate space for the node_memory struct for our single node. 
	 * With non-IP27 systems, there will only ever be one node.
	 *
	 * XXX - For now, we will not be allocating space for any memory 
	 *       bank information (like we do with IP27 node_memory). 
	 */
	node_memory[0] = 
		(node_memory_t*)kl_alloc_block(sizeof(node_memory_t), K_PERM);

	/* Fill in the structure. All we need to do is set size as all the
	 * other values are zero...
	 */
	node_memory[0]->n_slot = strdup("na");
	node_memory[0]->n_flag = 1; /* Always enabled? */
	node_memory[0]->n_memsize = (K_PHYSMEM * K_NBPC) / 0x100000;
	return(0);
}

/*
 * map_node_memory()
 */
static int
map_node_memory()
{
	int i, j, s, len, module, flag, size, banks, bank_size;
	int vhndl, node_vhndl, slot_vhndl;
	int pages, nasid;
	k_uint_t nodeid;
	kaddr_t value, addr, nodepda, meminfo, membank;
	k_ptr_t gvp, gep, gip, meminfop, membankp, mbp, npdap;
	char label[256];

	/* Allocate space for node_memory array
	 */
	node_memory = 
		(node_memory_t**)kl_alloc_block((K_NASID_BITMASK + 1) * K_NBPW, K_PERM);

	/* Allocate some buffers...
	 */
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	meminfop = kl_alloc_block(INVENT_MEMINFO_SIZE, K_TEMP);
	npdap = kl_alloc_block(NODEPDA_S_SIZE, K_TEMP);

	for (i = 0; i < K_NUMNODES; i++) {

		/* Get the address of the nodepda_s struct for this node. We 
		 * can get the nasid and vertex handl from there.
		 */
		value = (K_NODEPDAINDR + (i * K_NBPW));
		kl_get_kaddr(value, &nodepda, "nodepda_s");
		if (KL_ERROR) {
			continue;
		}
		kl_get_struct(nodepda, NODEPDA_S_SIZE, npdap, "nodepda_s");
		if (KL_ERROR) {
			continue;
		}
		nasid = kl_addr_to_nasid(nodepda);
		module = KL_UINT(npdap, "nodepda_s", "module_id");

		/* With certain extremely short memory dumps, the page of memory
		 * that contains the hwgraph is not included in the dump. In this
		 * case, we will not be able to tell the size of a vertex nor will
		 * we be able to use any of the hwgraph related information. We
		 * can use the K_VERTEX_SIZE() macro to test for this. Note that the
		 * information we get for memory may be incomplete and/or incorrect.
		 */
		if (K_VERTEX_SIZE == 0) {
			k_ptr_t node_pg_data;

			node_pg_data = (k_ptr_t)((k_uint_t)npdap +
							kl_member_size("nodepda_s", "node_pg_data"));
			size = kl_int(node_pg_data, "pg_data", "node_total_mem", 0);

			/* Convert size to MB
			 */
			size = (size * K_NBPC) / 0x100000;

			node_memory[nasid] = 
				(node_memory_t*)kl_alloc_block(sizeof(node_memory_t), K_PERM);

			node_memory[nasid]->n_module = module;
			node_memory[nasid]->n_slot = strdup("na");
			node_memory[nasid]->n_nodeid = i; /* XXX this may not be correct */
			node_memory[nasid]->n_nasid = nasid;
			node_memory[nasid]->n_memsize = size; 
			node_memory[nasid]->n_numbanks = 0;
			node_memory[nasid]->n_flag = 1;
			continue;
		}

		/* Get the node vertex handle from the nodepda_s sturct
		 */
		node_vhndl = KL_UINT(npdap, "nodepda_s", "node_vertex");

		/* We have to walk back a couple of elements in order to get
		 * the slot name
		 */
		if (vhndl = kl_connect_point(node_vhndl)) {
			if (slot_vhndl = kl_connect_point(vhndl)) {
				if (!kl_get_graph_vertex_s(slot_vhndl, 0, gvp, 0) || KL_ERROR) {
					continue;
				}
				if (!kl_vertex_edge_vhndl(gvp, vhndl, gep) || KL_ERROR) {
					continue;
				}
				if (kl_get_edge_name(gep, label)) {
					continue;
				}
			}
		}
		else {
			continue;
		}

		if (!kl_get_graph_vertex_s(node_vhndl, 0, gvp, 0) || KL_ERROR) {
			continue;
		}
		if (kl_vertex_info_name(gvp, "_cnodeid", gip)) {
			nodeid = KL_UINT(gip, "graph_info_s", "i_info");
		}
		else {
			nodeid = -1;
		}

		/* Get the memory vertex info entry
		 */
		if (!kl_vertex_edge_name(gvp, "memory", gep) || KL_ERROR) {
			continue;
		}
		vhndl = KL_UINT(gep, "graph_edge_s", "e_vertex");
		if (!kl_get_graph_vertex_s(vhndl, 0, gvp, 0) || KL_ERROR) {
			continue;
		}

		/* Get the _detail_invent info entry
		 */
		if (!kl_vertex_info_name(gvp, "_detail_invent", gip) || KL_ERROR) {
			continue;
		}

		meminfo = kl_kaddr(gip, "graph_info_s", "i_info");
		kl_get_struct(meminfo, INVENT_MEMINFO_SIZE, meminfop, 
				"invent_meminfo");
		if (KL_ERROR) {
			continue;
		}
		module = KL_UINT(meminfop, "invent_generic_s", "ig_module");
		flag = KL_UINT(meminfop, "invent_generic_s", "ig_flag");

		size = KL_UINT(meminfop, "invent_meminfo", "im_size");
		banks = KL_UINT(meminfop, "invent_meminfo", "im_banks");

		node_memory[nasid] = 
			(node_memory_t*)kl_alloc_block(sizeof(node_memory_t) + 
			((banks - 1) * sizeof(banks_t)), K_PERM);

		node_memory[nasid]->n_module = module;

		node_memory[nasid]->n_slot = strdup(label);
		node_memory[nasid]->n_nodeid = nodeid;
		node_memory[nasid]->n_nasid = nasid;
		node_memory[nasid]->n_memsize = size; 
		node_memory[nasid]->n_numbanks = banks;
		node_memory[nasid]->n_flag = flag;
		bank_size = (banks * kl_struct_len("invent_membnkinfo"));
		membankp = kl_alloc_block(bank_size, K_TEMP);
		membank = meminfo + 
			kl_member_offset("invent_meminfo", "im_bank_info");

		kl_get_block(membank, bank_size, membankp, "invent_membnkinfo");
		mbp = membankp;
		
		for (j = 0; j < banks; j++) {
			s = KL_UINT(mbp, "invent_membnkinfo", "imb_size");
			node_memory[nasid]->n_bank[j].b_paddr = 
				((k_uint_t)nasid << K_NASID_SHIFT) + 
						((k_uint_t)j << MD_BANK_SHFT);
			if (s) {
				node_memory[nasid]->n_bank[j].b_size = s;
			}
			len = kl_struct_len("invent_membnkinfo");
			mbp = (k_ptr_t) ((uint)mbp + len);
		}
		kl_free_block(membankp);
	}

	/* We have to check to see if gvp contains a pointer to a
	 * block that needs to be freed. The size of a vertex is 
	 * derived (it's not obtained from the symbol table). If 
	 * we have problems determining the vertex size (normally 
	 * because key memory pages are missing from the dump), gvp 
	 * will be NULL. The rest of the blocks should never be
	 * NULL.
	 */
	if (gvp) {
		kl_free_block(gvp);
	}
	kl_free_block(gip);
	kl_free_block(gep);
	kl_free_block(npdap);
	kl_free_block(meminfop);
	return(0);
}

/* 
 * kl_valid_nasid()
 *
 * Returns 1 if nasid is acutally for a node installed in the system.
 * Otherwise returns zero. If no function is registered and nasid is
 * zero, return 1 (don't fail).
 */
int
kl_valid_nasid(int nasid)
{
    if (check_node_memory()) {
		return(0);
	}
	if ((K_IP == 27) && node_memory[nasid]) {
		return(1);
	}
	else if (nasid == 0) {
		return(1);
	}
	return(0);
}

/*
 * kl_valid_physmem()
 */
int
kl_valid_physmem(kaddr_t paddr)
{
	/* If this routine is called before all the proper global
	 * variables are set, we have to just return with success.
	 */
	if (!mem_validity_check || (K_FLAGS & K_IGNORE_MEMCHECK)) {
		return(0);
	}

	if (K_RAM_OFFSET) {
		if (paddr < K_RAM_OFFSET) {
			return(KLE_BEFORE_RAM_OFFSET);
		}
	}

	/* Now check for valid physmem based on the type of system 
	 * architecture
	 */
	if (K_NASID_SHIFT) {
		int nasid, slot;
		kaddr_t slot_start, slot_size;

		nasid = kl_addr_to_nasid(paddr);
		if (!node_memory[nasid]) {
			return(KLE_PHYSMEM_NOT_INSTALLED);
		}
		slot = kl_addr_to_slot(paddr);
		slot_start = node_memory[nasid]->n_bank[slot].b_paddr;
		slot_size = node_memory[nasid]->n_bank[slot].b_size * (1024 * 1024);

		if (!slot_size || 
			!((paddr >= slot_start) && (paddr < (slot_start + slot_size)))) {
			return(KLE_PHYSMEM_NOT_INSTALLED);
		}
	}
	else {
		int pfn;

		pfn = paddr >> K_PNUMSHIFT;
		if (K_RAM_OFFSET) {
			if ((pfn - (K_RAM_OFFSET >> K_PNUMSHIFT)) >= K_PHYSMEM) {
				return(KLE_AFTER_PHYSMEM);
			}
		}
		else if (pfn >= K_PHYSMEM) {
			return(KLE_AFTER_PHYSMEM);
		}
	}
	return(0);
}
