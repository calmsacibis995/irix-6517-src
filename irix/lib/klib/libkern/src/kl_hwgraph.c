#ident  "$Header: "

#define _KERNEL  1
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/graph.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#include <klib/klib.h>

/* Variables to hold global values defining the number of vertexes
 * per group. On some machines, these values are defines. On later
 * versions of the kernel, they are tunable values. We will set the
 * KLIB variables to be the old devault values and then change them
 * if the graph_vertex_per_group and graph_vertex_per_group_log2
 * symbols exist.
 */
int graph_vertex_per_group = 128;
int graph_vertex_per_group_log2 = 7;

/*
 * kl_init_hwgraph()
 */
void
kl_init_hwgraph()
{
	int numgroups;
	kaddr_t taddr;

	if (taddr = kl_sym_addr("graph_vertex_per_group")) {
		kl_get_block(taddr, 4, &graph_vertex_per_group, 
			"graph_vertex_per_group");
	}
	if (taddr = kl_sym_addr("graph_vertex_per_group_log2")) {
		kl_get_block(taddr, 4, &graph_vertex_per_group_log2,
			"graph_vertex_per_group_log2");
	}

	/* We have to read in the hwgraph struct, adjust graph_size by the
	 * number of groups and then read it in again. Note that if the
	 * hwgraph symbol doesn't exist, just skip this section.
	 */
	if (K_HWGRAPHP = kl_sym_pointer("hwgraph")) {
		if (GRAPH_S_SIZE) {
			K_HWGRAPH = kl_alloc_block(GRAPH_S_SIZE, K_PERM);
			kl_get_struct(K_HWGRAPHP, GRAPH_S_SIZE, K_HWGRAPH, "hwgraph");
			K_VERTEX_SIZE = KL_UINT(K_HWGRAPH, "graph_s", "graph_vertex_size");
			numgroups = KL_UINT(K_HWGRAPH, "graph_s", "graph_num_group");
			K_GRAPH_SIZE = GRAPH_S_SIZE +
				((numgroups - 1) * GRAPH_VERTEX_GROUP_S_SIZE);
			kl_free_block(K_HWGRAPH);
			K_HWGRAPH = kl_alloc_block(K_GRAPH_SIZE, K_PERM);
			kl_get_struct(K_HWGRAPHP, K_GRAPH_SIZE, K_HWGRAPH, "hwgraph");
		}
	}
}

int
max_groupid()
{
	int numgroups;

	numgroups = KL_UINT(K_HWGRAPH, "graph_s", "graph_num_group");
	return(numgroups - 1);
}

int
max_vhndl()
{
	int num_vertex;

	num_vertex = KL_UINT(K_HWGRAPH, "graph_s", "graph_num_vertex");
	return(num_vertex);
}

/*
 * kl_handle_to_vertex()
 */
kaddr_t
kl_handle_to_vertex(k_uint_t handle)
{
	int groupid, grpidx;
	k_ptr_t vgp;
	kaddr_t vertex, vertex_array;
	
	if (handle > max_vhndl()) {	
		KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_HNDL, handle, 0);
		return((kaddr_t)NULL);
	}
	groupid = handle_to_groupid(handle);
	grpidx = handle_to_grpidx(handle);

	vgp = (k_ptr_t)((uint)K_HWGRAPH + 
			kl_member_offset("graph_s", "graph_vertex_group") + 
			(groupid * GRAPH_VERTEX_GROUP_S_SIZE));

	vertex_array = kl_kaddr(vgp, "graph_vertex_group_s", "group_vertex_array");
	vertex = vertex_array + (grpidx * K_VERTEX_SIZE);
	return(vertex);
}

/* 
 * kl_handle_to_vertex_group()
 */
kaddr_t
kl_handle_to_vertex_group(k_uint_t handle)
{
	int groupid;
	kaddr_t group;

	if (handle > max_vhndl()) {
		KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_HNDL, handle, 0);
		return((kaddr_t)NULL);
	}
	groupid = handle_to_groupid(handle);
	group = ((uint)K_HWGRAPH + kl_member_offset("graph_s", 
		"graph_vertex_group") + (groupid * GRAPH_VERTEX_GROUP_S_SIZE));
	return(group);
}

/*
 * kl_groupid_to_group()
 */
kaddr_t
kl_groupid_to_group(int groupid)
{
	kaddr_t group;

	if (groupid > max_groupid()) {
		KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_GRPID, groupid, 0);
		return((kaddr_t)NULL);
	}
	group = (K_HWGRAPHP + kl_member_offset("graph_s", 
		"graph_vertex_group") + (groupid * GRAPH_VERTEX_GROUP_S_SIZE));
	return(group);
}

/*
 * kl_grpidx_to_handle()
 */
int
kl_grpidx_to_handle(int groupid, int grpidx)
{
	int handle;

	if (groupid > max_groupid()) {
		KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_GRPID, groupid, 0);
		return(0);
	}
	handle = (((groupid) << GRAPH_VERTEX_PER_GROUP_LOG2) | (grpidx));
	return(handle);
}

/*
 * kl_vertex_to_handle()
 */
int
kl_vertex_to_handle(kaddr_t vertex) 
{
	int i, num_vertex;

	num_vertex = kl_uint(K_HWGRAPH, "graph_s", "graph_num_vertex", 0);
	if (vertex > num_vertex) {
		return(-1);
	}

	for (i = 1; i < num_vertex; i++) {
		if (kl_handle_to_vertex(i) == vertex) {
			return(i);
		}
	}
	return(-1);
}

/* 
 * kl_vertex_edge_ptr()
 */
kaddr_t
kl_vertex_edge_ptr(k_ptr_t vertex)
{
	return(kl_kaddr(vertex, "graph_vertex_s", "v_info_list"));
}

/* 
 * kl_vertex_lbl_ptr()
 */
kaddr_t
kl_vertex_lbl_ptr(k_ptr_t vertex)
{
	int num_edge;
	kaddr_t info_list;

	num_edge = kl_uint(vertex, "graph_vertex_s", "v_num_edge", 0);
	info_list = kl_kaddr(vertex, "graph_vertex_s", "v_info_list");
	return(info_list + num_edge * GRAPH_EDGE_S_SIZE);
}

/*
 * kl_get_edge_name()
 */
int
kl_get_edge_name(k_ptr_t edge, char *name)
{
	kaddr_t namp;

	/* Get the pointer to the edge label string
	 */
	namp = kl_kaddr(edge, "graph_edge_s", "e_label");

	/* Copy the label string into the buffer provided
	 */
	kl_get_block(namp, 24, name, "graph_edge_name");
	if (KL_ERROR) {
		return(1);
	}
	return(0);
}

/*
 * kl_get_label_name()
 */
int
kl_get_label_name(k_ptr_t labelp, char *name)
{
	kaddr_t namp;

	/* Get the pointer to the info name string
	 */
	namp = kl_kaddr(labelp, "graph_edge_s", "e_label");

	/* Copy the label string into the buffer provided
	 */
	kl_get_block(namp, 24, name, "graph_info_name");
	if (KL_ERROR) {
		return(1);
	}
	return(0);
}

/*
 * kl_vertex_edge_vhndl() -- find the edge with vhndl
 */
k_ptr_t
kl_vertex_edge_vhndl(k_ptr_t gvp, int vhndl, k_ptr_t gep)
{
	int i, num_edge, gep_alloced = 0;
	kaddr_t labelp;
	char label[256];
	

	/* If we wern't passed an edge buffer pointer, allocate some 
	 * space now.
	 */
	if (!gep) {
		gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
		gep_alloced++;
	}

	/* Determine how many edge entries this vertex has
	 */
	num_edge = kl_uint(gvp, "graph_vertex_s", "v_num_edge", 0);

	for (i = 0; i < num_edge; i++) {
		kl_get_graph_edge_s(gvp, (k_uint_t)i, 0, gep, 0);
		if (KL_ERROR) {
			return((k_ptr_t)NULL);
		}

		/* Get the pointer to the label string
		 */
		labelp = kl_kaddr(gep, "graph_edge_s", "e_label");

		/* Get the label string
		 */
		kl_get_block(labelp, 24, label, "graph_edge_label");

		if (vhndl == kl_uint(gep, "graph_edge_s", "e_vertex", 0)) {
			break;
		}
	}

	/* If we found the edge, return it. Otherwise, return NULL (free
	 * up gep if it was allocated here).
	 */
	if (i < num_edge) {
		return(gep);
	}
	if (gep_alloced) {
		kl_free_block(gep);
	}
	return((k_ptr_t)NULL);
}

/*
 * kl_vertex_edge_name() -- find the named edge in vertex
 */
k_ptr_t
kl_vertex_edge_name(k_ptr_t gvp, char *name, k_ptr_t gep)
{
	int i, num_edge, gep_alloced = 0;
	kaddr_t labelp;
	char label[256];
	

	/* If we wern't passed an edge buffer pointer, allocate some 
	 * space now.
	 */
	if (!gep) {
		gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
		gep_alloced++;
	}

	/* Determine how many edge entries this vertex has
	 */
	num_edge = kl_uint(gvp, "graph_vertex_s", "v_num_edge", 0);

	for (i = 0; i < num_edge; i++) {
		kl_get_graph_edge_s(gvp, (k_uint_t)i, 0, gep, 0);
		if (KL_ERROR) {
			if (gep_alloced) {
				kl_free_block(gep);
			}
			return((k_ptr_t)NULL);
		}

		/* Get the pointer to the label string
		 */
		labelp = kl_kaddr(gep, "graph_edge_s", "e_label");

		/* Get the label string
		 */
		kl_get_block(labelp, 24, label, "graph_edge_label");

		if (!strcmp(label, name)) {
			break;
		}
	}

	/* If we found the edge, return it. Otherwise, return NULL (free
	 * up gep if it was allocated here).
	 */
	if (i < num_edge) {
		return(gep);
	}
	if (gep_alloced) {
		kl_free_block(gep);
	}
	return((k_ptr_t)NULL);
}

/*
 * kl_vertex_info_name() -- find the named label in vertex
 */
k_ptr_t
kl_vertex_info_name(k_ptr_t gvp, char *name, k_ptr_t gip)
{
	int i, num_info, gip_alloced = 0;
	kaddr_t labelp;
	char label[256];
	

	/* If we wern't passed an info buffer pointer, allocate some 
	 * space now.
	 */
	if (!gip) {
		gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);
		gip_alloced++;
	}

	/* Determine how many info entries this vertex has
	 */
	num_info = kl_uint(gvp, "graph_vertex_s", "v_num_info_LBL", 0);

	for (i = 0; i < num_info; i++) {
		kl_get_graph_info_s(gvp, i, 0, gip, 0);
		if (KL_ERROR) {
			if (gip_alloced) {
				kl_free_block(gip);
			}
			return((k_ptr_t)NULL);
		}

		/* Get the pointer to the label string
		 */
		labelp = kl_kaddr(gip, "graph_info_s", "i_label");

		/* Get the label string
		 */
		kl_get_block(labelp, 24, label, "graph_info_label");

		if (!strcmp(label, name)) {
			break;
		}
	}

	/* If we found the info, return it. Otherwise, return NULL (free
	 * up gip if it was allocated here.
	 */
	if (i < num_info) {
		return(gip);
	}
	if (gip_alloced) {
		kl_free_block(gip);
	}
	KL_SET_ERROR_CVAL(KLE_BAD_VERTEX_INFO, name);
	return((k_ptr_t)NULL);
}

/*
 * kl_connect_point() -- return connect vhndl for vertex
 */
int
kl_connect_point(int vhndl)
{
	k_uint_t conn_vhndl;	
	k_ptr_t gvp, idx_ptr;

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);

	kl_get_graph_vertex_s(vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(0);
	}

	idx_ptr = (k_ptr_t)((uint)gvp + 
			(GRAPH_VERTEX_S_SIZE + (HWGRAPH_CONNECTPT * K_NBPW)));

	bcopy(idx_ptr, &conn_vhndl, 8);
	if (PTRSZ32) {
		conn_vhndl = conn_vhndl >> 32;
	}

	kl_free_block(gvp);
	return((int)conn_vhndl);
}

/* 
 * kl_is_symlink() -- Determine if an edge is a "file" or a "symlink"
 *
 * The first edge from a vertex's parent to the parent is considered
 * by the hwgfs file system code to be a "file." Any subsequent edges
 * to the same vertex are considered "symlinks."
 */
int
kl_is_symlink(k_ptr_t gvp, int edge, int hndl)
{
	int i, num_edge, vhndl;
	k_ptr_t gep;

	kl_reset_error();

	num_edge = kl_uint(gvp, "graph_vertex_s", "v_num_edge", 0);
	if (edge >= num_edge) {
		KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_EDGE, edge, 0);
		return(0);
	}

	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	for (i = 0; i < edge; i++) {
		kl_get_graph_edge_s(gvp, (k_uint_t)i, 0, gep, 0);
		if (KL_ERROR) {
			kl_free_block(gep);
			return(0);
		}
		vhndl = kl_uint(gep, "graph_edge_s", "e_vertex", 0);
		if (hndl == vhndl) {
			kl_free_block(gep);
			return(1);
		}
	}
	kl_free_block(gep);
	return(0);
}

/*
 * kl_add_path_chunk()
 */
static void
kl_add_path_chunk(path_t *path)
{
	path_chunk_t *p, *pcp;

	pcp = (path_chunk_t*)kl_alloc_block(sizeof(path_chunk_t), K_TEMP);
	ENQUEUE(&path->pchunk, pcp);
}

/*
 * kl_init_path_table()
 */
path_t *
kl_init_path_table()
{
	path_t *p;
	path_rec_t *pr;

	/* Allocate the path table and the string table.
	 */
	if (p = (path_t*)kl_alloc_block(sizeof(path_t), K_TEMP)) {
		p->st = init_string_table(K_TEMP);

		/* Allocate the first path_chunk
		 */
		kl_add_path_chunk(p);

		/* Allocate the first path element (hwgraph_root)
		 */
		pr = (path_rec_t*)kl_alloc_block(sizeof(path_rec_t), K_TEMP);
		pr->vhndl = 1;
		pr->name = get_string(p->st, "hw", K_TEMP);
		pr->next = pr->prev = pr;
		PATH(p) = pr;
	}
	return(p);
}

/*
 * kl_add_to_path()
 */
static void
kl_add_to_path(path_rec_t *path, path_rec_t *new)
{
	ENQUEUE(&path, new);
}

/*
 * kl_free_path_records()
 */
void
kl_free_path_records(path_rec_t *path)
{
	/* Make sure there actually is a path
	 */
	path_rec_t *element, *next;

	if (!(element = path)) {
		return;
	}

	do {
		next = element->next;
		kl_free_block((k_ptr_t)element);
		element = next;
	} while (element && (element != path));
}

/*
 * kl_free_path()
 */
void 
kl_free_path(path_t *p)
{
	int i;
	path_chunk_t *pc, *pcnext;
	
	pc = p->pchunk;
	do {
		for (i = 0; i <= pc->current; i++) {
			kl_free_path_records(pc->path[i]);
		}
		pcnext = pc->next;
		kl_free_block((k_ptr_t)pc);
		pc = pcnext;
	} while (pc != p->pchunk);
	free_string_table(p->st);
	kl_free_block((k_ptr_t)p);
}

/*
 * kl_clone_path()
 */
path_rec_t *
kl_clone_path(path_rec_t *path, int aflg)
{
	path_rec_t *new_path, *next, *p, *np;

	new_path = (path_rec_t*)kl_alloc_block(sizeof(path_rec_t), aflg);
	new_path->name = kl_str_to_block(path->name, aflg);
	new_path->vhndl = path->vhndl;
	new_path->next = new_path->prev = new_path;

	np = new_path;
	p = path->next;

	while (p != path) {
		next = (path_rec_t*)kl_alloc_block(sizeof(path_rec_t), aflg);
		next->vhndl = p->vhndl;
		next->name = kl_str_to_block(p->name, aflg);
		kl_add_to_path(new_path, next);
		p = p->next;
	}
	return(new_path);
}

/*
 * kl_push_path()
 */
static void
kl_push_path(path_t *path, path_rec_t *pr, char *name, int vhndl)
{
	path_rec_t *p;
	
	p = (path_rec_t*)kl_alloc_block(sizeof(path_rec_t), K_TEMP);
	p->name = get_string(path->st, name, K_TEMP);
	p->vhndl = vhndl;
	kl_add_to_path(pr, p);
}

/*
 * kl_pop_path()
 */
static void
kl_pop_path(path_rec_t *path)
{
	path_rec_t *p = path->prev;

	REMQUEUE(&path, p);
	kl_free_block((k_ptr_t)p);
}

/*
 * kl_dup_current_path()
 */
static void
kl_dup_current_path(path_t *pl)
{
	path_rec_t *p, *np, *new_path = (path_rec_t*)NULL;
	path_chunk_t *pcp;

	p = PATH(pl);

	do {
		np = (path_rec_t *)kl_alloc_block(sizeof(path_rec_t), K_TEMP);
		np->vhndl = p->vhndl;
		np->name = p->name;

		if (new_path) {
			ENQUEUE(&new_path, np);
		}
		else {
			np->next = np->prev = np;
			new_path = np;
		}
		p = p->next;

	} while (p != PATH(pl));
	pcp = pl->pchunk->prev;
	if (pcp->current == (PATHS_PER_CHUNK - 1)) {
		kl_add_path_chunk(pl);
		PATH(pl) = new_path;
	}
	else {
		pcp->path[pcp->current + 1] = new_path;
		pcp->current++;
	}
	pl->count++;
}

/*
 * kl_vertex_in_path()
 */
int
kl_vertex_in_path(path_rec_t *path, int vertex) 
{
	path_rec_t *p;

	p = path;
	do {
		if (p->vhndl == vertex) {
			break;
		}
		p = p->next;
	} while (p != path);
	if ((p == path) && (vertex != 1)) {
		return(0);
	}
	return(1);
}

/*
 * kl_valid_vertex()
 *
 * Make sure we have a a valid vertex handle/address. If connect_gvp
 * is not NULL, then check to see if the vertex represented by value 
 * is one of its edges.
 */
int
kl_valid_vertex(kaddr_t value, int mode, int connect_vhndl)
{
	int vhndl, valid_vertex = 1;
	k_ptr_t gvp, cgvp, gep;

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);

	if (mode == 2) {
		vhndl = kl_vertex_to_handle(value);
	}
	else {
		vhndl = value;
	}

	kl_get_graph_vertex_s(value, mode, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return(0);
	}

	if (connect_vhndl) {
		cgvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
		gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
		kl_get_graph_vertex_s(connect_vhndl, 0, cgvp, 0);
		if (!kl_vertex_edge_vhndl(cgvp, vhndl, gep)) {
			valid_vertex = 0;
		}
		kl_free_block(cgvp);
		kl_free_block(gep);
	}
	kl_free_block(gvp);
	return(valid_vertex);
}

/*
 * kl_find_pathname()
 */
int
kl_find_pathname(int level, char *name, path_t *p, int flags, int vertex)
{
	int e, i, ret, cur_vhndl, vhndl, num_edge, num_info;
	k_ptr_t gvp, gep, gip;
	path_rec_t *np;
	kaddr_t edgep, infop, labelp;
	char label[256];

	/* Allocate some buffers
	 */
	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);
	gip = kl_alloc_block(GRAPH_INFO_S_SIZE, K_TEMP);

	/* Get the vertex for the current path level
	 */
	cur_vhndl = PATH(p)->prev->vhndl;
	kl_get_graph_vertex_s(cur_vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		if (gvp) {
			kl_free_block(gvp);
		}
		kl_free_block(gep);
		kl_free_block(gip);
		return(1);
	}

	/* Check to see if name matches with an label for the current 
	 * vertex
	 */
	if (name && kl_vertex_info_name(gvp, name, gip)) {
		if (!vertex || kl_vertex_in_path(PATH(p), vertex)) {
			kl_dup_current_path(p);
		}
	}

	/* Walk the edges for this vertex. We don't have to pop the
	 * element off the path before we return because it wasn't 
	 * added here.
	 */
	num_edge = kl_uint(gvp, "graph_vertex_s", "v_num_edge", 0);
	if (num_edge == 0) {
		if (!name) {
			kl_dup_current_path(p);
		}
		kl_free_block(gvp);
		kl_free_block(gep);
		kl_free_block(gip);
		return(0);
	}

	for (e = 0; e < num_edge; e++) {

		kl_get_graph_edge_s(gvp, (k_uint_t)e, 0, gep, flags);
		if (KL_ERROR) {
			kl_free_block(gvp);
			kl_free_block(gep);
			kl_free_block(gip);
			return(1);
		}

		/* Get the edge name and vertex handle and add the element
		 * to the path.
		 */
		vhndl = kl_uint(gep, "graph_edge_s", "e_vertex", 0);
		if (!kl_valid_vertex(vhndl, 0, cur_vhndl)) {
			/* There have been some problems with getting a hardware
			 * config on a live system during startup. It seems that
			 * the hwgraph is not exactly stable at that time. There
			 * have been cases where we get a vertex and by the time
			 * we go to get an edge, either the vertex has changed
			 * or the edge/info pointer has. From this point on, 
			 * everything will be suspect. So, we just have to fail
			 * out. On an active system, the caller can always try
			 * again. Note that this test will NOT catch the case
			 * where there is invalid data that does not happen to 
			 * cause a failure (wrong but valid vertex handle).
			 */
			kl_free_block(gvp);
			kl_free_block(gep);
			kl_free_block(gip);
			return(1);
		}

		kl_get_edge_name(gep, label);
		kl_push_path(p, PATH(p), label, vhndl);

		/* Check to see if the current edge is the one we are looking 
		 * for. If it is, dup the current path and continue. Otherwise, 
		 * recursively call kl_find_pathname() to walk to the next path 
		 * element.
		 */
		if (name && !strcmp(name, label)) {
			if (!vertex || kl_vertex_in_path(PATH(p), vertex)) {
				kl_dup_current_path(p);
				kl_pop_path(PATH(p));
			}
			continue;
		}

		/* Make sure the connect point for the next element is our
		 * current vertex. If it isn't, it means we have reached
		 * a symbolic link and should terminate the search (after 
		 * including the link). Also, if more than one edge points
		 * to the same vertex, only the first should be followed.
		 * All subsequent edges should be treated as symlinks. Only 
		 * dup the current path if name is NOT set (we checked above 
		 * to see if there was a match.
		 */
		if ((kl_connect_point(vhndl) != cur_vhndl) || 
										kl_is_symlink(gvp, e, vhndl)) {
			if (!name) {
				kl_dup_current_path(p);
			}
			kl_pop_path(PATH(p));
			continue;
		}

		ret = kl_find_pathname(level + 1, name, p, flags, vertex);
		kl_pop_path(PATH(p));

		if (ret) {
			if (DEBUG(KLDC_GLOBAL, 1)) {
				fprintf(KL_ERRORFP, "ERROR!!! cur_vhndl=%d, level=%d, "
					"ret=%d\n", cur_vhndl, level, ret);
			}
			if (ACTIVE) {
				/* On an active system, it's possible that there was some
				 * problem caused by the fact that the system is, well,
				 * active. Normally this would only be an issue during 
				 * startup...but just in case...we want to FAIL, but
				 * it's possible that another try will succeed. That's
				 * why we set KL_ERROR equal to KLE_AGAIN; With a system
				 * core dump, we'll try and just slog along in the hopes
				 * that we will get past this error point and pick up 
				 * other valid pathnames later on in the hwgraph.
				 */
				KL_ERROR = KLE_AGAIN;
				return(ret);
			}
		}
	}

	/* If this is the last level we are returning from, we have to
	 * remove the /hw entry except when name is "hw".
	 */
	if ((level == 0) && (!name || strcmp(name, "hw"))) {
		kl_free_path_records(PATH(p));
		PATH(p) = (path_rec_t*)NULL;
		p->pchunk->prev->current--;
	}
	kl_free_block(gvp);
	kl_free_block(gep);
	kl_free_block(gip);
	return(0);
}

/*
 * kl_pathname_to_vertex()
 */
int
kl_pathname_to_vertex(char *path, int start)
{
	return(0);
}

/*
 * kl_vertex_to_pathname()
 */
path_rec_t *
kl_vertex_to_pathname(kaddr_t value, int mode, int aflg)
{
	int e, handle, cp, num_edge;
	kaddr_t vertex;
	k_ptr_t gvp, gep;
	path_rec_t *path, *p;
	char label[256];

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);

	/* Before we go any further, we have to make sure we were given
	 * a valid vertex handle/address. We don't really need the sturct,
	 * except as a check to make sure the vertex is valid. It's not big
	 * deal since we need the gvp block later anyway...
	 */
	kl_get_graph_vertex_s(value, mode, gvp, 0);
	if (KL_ERROR) {
		kl_free_block(gvp);
		return((path_rec_t *)NULL);
	}

	/* Depending on mode value, set vertex handle.
	 */
	if (mode == 0) {
		handle = value;
	}
	else if (mode == 2) {
		handle = kl_vertex_to_handle(value);
		if (handle == -1) {
			kl_free_block(gvp);
			return((path_rec_t *)NULL);
		}
	}
	else {
		kl_free_block(gvp);
		return((path_rec_t *)NULL);
	}

	/* Allocate the first (last) element in the pathname
	 */
	path = (path_rec_t *)kl_alloc_block(sizeof(path_rec_t), aflg);
	path->vhndl = handle;
	cp = kl_connect_point(path->vhndl);
	gep = kl_alloc_block(GRAPH_EDGE_S_SIZE, K_TEMP);

	while (cp) {
		kl_get_graph_vertex_s(cp, 0, gvp, 0);
		if (KL_ERROR) {
			goto done;
		}

		/* Walk the edges for this vertex and look for the edge that 
		 * contains this vertex handle.
		 */
		num_edge = kl_uint(gvp, "graph_vertex_s", "v_num_edge", 0);
		if (num_edge == 0) {
			goto done;
		}
		for (e = 0; e < num_edge; e++) {
			kl_get_graph_edge_s(gvp, (k_uint_t)e, 0, gep, 0);
			if (KL_ERROR) {
				goto done;
			}

			/* Get the edge vertex handle and check to see if it matches
			 * the one in our current element.
			 */
			handle = kl_uint(gep, "graph_edge_s", "e_vertex", 0);
			if (handle == path->vhndl) {
				break;
			}
		}
		if (e == num_edge) {
			/* We didn't find the edge we were looking for!
			 */
			goto done;
		}
		if (kl_get_edge_name(gep, label)) {
			goto done;
		}
		path->name = (char *)kl_alloc_block(strlen(label) +1, aflg);
		strcat(path->name, label);
		p = (path_rec_t *)kl_alloc_block(sizeof(path_rec_t), aflg);
		p->next = path;
		path = p;
		path->vhndl = cp;
		cp = kl_connect_point(path->vhndl);
	}
done:

	path->name = (char *)kl_alloc_block(3, aflg);
	if (path->vhndl == 1) {
		strcat(path->name, "hw");
	}
	else {
		strcat(path->name, "??");
	}

	/* Now fix the backward links 
	 */
	p = path;
	for ( ;; ) {
		if (p->next) {
			p->next->prev = p;
			p = p->next;
		}
		else {
			p->next = path;
			path->prev = p;
			break;
		}
	}
	kl_free_block(gvp);
	kl_free_block(gep);
	return(path);
}

/*
 * kl_free_pathname()
 *
 * Routine that can be called from the outside. It will free all
 * of the memory associated with a particular pathname (it assumes that
 * the memory blocks containing the name of each path element are allocated 
 * indifidually). All pathnames that use string table space must be
 * freed using kl_free_path_records().
 */
void
kl_free_pathname(path_rec_t *path)
{
	path_rec_t *element, *next;

	if (!path) {
		return;
	}

	element = path;
	do {
		next = element->next;
		if (element->name) {
			kl_free_block((k_ptr_t)element->name);
		}
		kl_free_block((k_ptr_t)element);
		element = next;
	} while (element != path);
}

/*
 * kl_vertex_get_next() -- walk the entire list of vertices
 *
 * When there are no more vertex handles to return, get_next returns
 * GRAPH_HIT_LIMIT.
 */
int
kl_vertex_get_next(int *vertex_found, int *placeptr)
{
	int s, groupid, grpidx, junk_groupid, junk_grpidx;
	int handle, old_handle;
	uint handle_limit;
	kaddr_t vertex, group, junk_group;
	k_ptr_t ggp;
	graph_error_t status;

	if (vertex_found == NULL) {
		return(1);
	}

	old_handle = *placeptr;

	if (old_handle != GRAPH_VERTEX_PLACE_NONE) {
		handle = old_handle + 1; /* Advance to next vertex handle */    
	} 
	else {
		handle = 1;
	}

	groupid = handle_to_groupid(handle);
	if (KL_ERROR) {
		return(1);
	}
	grpidx = handle_to_grpidx(handle);

	ggp = kl_alloc_block(GRAPH_VERTEX_GROUP_S_SIZE, K_TEMP);
	handle_limit = 
		kl_uint(K_HWGRAPH, "graph_s", "graph_vertex_handle_limit", 0);

	for (; groupid < GRAPH_NUM_GROUP; groupid++) {

		group = kl_groupid_to_group(groupid);
		kl_get_struct(group, GRAPH_VERTEX_GROUP_S_SIZE, 
								ggp, "graph_vertex_group_s");

		handle = kl_grpidx_to_handle(groupid, grpidx);

		if (kl_uint(ggp, "graph_vertex_group_s", "group_count", 0) != 0) {

			for (; grpidx < GRAPH_VERTEX_PER_GROUP; grpidx++, handle++) {

				if (handle > handle_limit) {
					kl_free_block(ggp);
					return(2);
				}

				vertex = kl_handle_to_vertex(handle);
				if (KL_ERROR) {
					kl_free_block(ggp);
					return(1);
				}
				*vertex_found = handle;
				*placeptr = handle;
				kl_free_block(ggp);
				return(0);
			}
		}
		grpidx = 0;
	}	
	kl_free_block(ggp);
	return(2);
}

/*
 * kl_get_graph_edge_s() 
 * 
 */
k_ptr_t
kl_get_graph_edge_s(k_ptr_t v, kaddr_t value, 
	int mode, k_ptr_t gep, int flags)
{
	int num_edge;
	kaddr_t edge, info_list;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_graph_edge_s: v=0x%x, value=%lld, "
				"mode=%d, ", v, value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_graph_edge_s: value=0x%llx, "
				"mode=%d, ", value, mode);
		}
		fprintf(KL_ERRORFP, "gvp=0x%x, flags=0x%x\n", gep, flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if (mode == 2) {
		if (!value) {
			KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
			return ((k_ptr_t)NULL);
		}
		edge = value;
	}
	else if (mode == 0) {

		/* We have to get the numbered edge from vertex v
		 */

		num_edge = KL_UINT(v, "graph_vertex_s", "v_num_edge");
		if (value >= num_edge) {
			KL_SET_ERROR_NVAL(KLE_BAD_GRAPH_VERTEX_S, value, mode);
			return ((k_ptr_t)NULL);
		}
		edge = kl_vertex_edge_ptr(v) + (value * GRAPH_EDGE_S_SIZE);
	}

	kl_get_struct(edge, GRAPH_EDGE_S_SIZE, gep, "graph_edge_s");
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}
	return(gep);
}

/*
 * kl_get_graph_info_s() 
 * 
 */
k_ptr_t
kl_get_graph_info_s(k_ptr_t v, kaddr_t value, 
	int mode, k_ptr_t gip, int flags)
{
	int num_lbl;
	kaddr_t lbl, info_list;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_graph_info_s: v=0x%x, value=%lld, "
				"mode=%d, ", v, value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_graph_info_s: value=0x%llx, "
				"mode=%d, ", value, mode);
		}
		fprintf(KL_ERRORFP, "gip=0x%x, flags=0x%x\n", gip, flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if (mode == 2) {
		if (!value) {
			KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
			return ((k_ptr_t)NULL);
		}
		lbl = value;
	}
	else if (mode == 0) {

		/* We have to get the numbered label from vertex v
		 */
		num_lbl = KL_UINT(v, "graph_vertex_s", "v_num_info_LBL");
		if (value >= num_lbl) {
			KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_INFO, value, mode);
			return ((k_ptr_t)NULL);
		}
		lbl = kl_vertex_lbl_ptr(v) + (value * GRAPH_INFO_S_SIZE);
		if (!lbl) {
			KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_INFO, 0, 2);
			return((k_ptr_t)NULL);
		}
	}

	kl_get_struct(lbl, GRAPH_INFO_S_SIZE, gip, "graph_info_s");
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}
	return(gip);
}

/*
 * kl_get_graph_vertex_s() 
 * 
 */
k_ptr_t
kl_get_graph_vertex_s(kaddr_t value, int mode, k_ptr_t gvp, int flags)
{
	kaddr_t vertex;

	if (DEBUG(KLDC_FUNCTRACE, 2)) {
		if (mode != 2) {
			fprintf(KL_ERRORFP, "kl_get_graph_vertex_s: value=%lld, "
				"mode=%d, ", value, mode);
		} 
		else {
			fprintf(KL_ERRORFP, "kl_get_graph_vertex_s: value=0x%llx, "
				"mode=%d, ", value, mode);
		}
		fprintf(KL_ERRORFP, "gvp=0x%x, flags=0x%x\n", gvp, flags);
	}

	kl_reset_error();

	/* Just in case... 
	 */
	if (mode == 2) {
		if (!value) {
			KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
			return ((k_ptr_t)NULL);
		}
		vertex = value;
	}
	else if (mode == 0) {
		vertex = kl_handle_to_vertex(value);
		if (KL_ERROR) {
			return ((k_ptr_t)NULL);
		}
	}
	else {
		KL_SET_ERROR_NVAL(KLE_BAD_STRUCT, value, mode);
		return((k_ptr_t)NULL);
	}

	/* If this is mode 0, we need to convert the vertex handle to
	 * the actual vertex address.
	 */
	kl_get_struct(vertex, K_VERTEX_SIZE, gvp, "graph_vertex_s");
	if (KL_ERROR) {
		return((k_ptr_t)NULL);
	}
	return(gvp);
}
