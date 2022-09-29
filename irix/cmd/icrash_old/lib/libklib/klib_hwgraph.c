#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_hwgraph.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/graph.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>

#include "klib.h"
#include "klib_extern.h"

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
 * init_hwgraph()
 */
void
init_hwgraph(klib_t *klp)
{
	kaddr_t taddr;

	if (K_SYM_ADDR(klp)((void*)NULL, "graph_vertex_per_group", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &graph_vertex_per_group, 
			"graph_vertex_per_group");
	}
	if (K_SYM_ADDR(klp)((void*)NULL, 
					"graph_vertex_per_group_log2", &taddr) >= 0) {
		kl_get_block(klp, taddr, 4, &graph_vertex_per_group_log2, 
			"graph_vertex_per_group_log2");
	}
}

/*
 * handle_to_vertex()
 */
kaddr_t
handle_to_vertex(klib_t *klp, k_uint_t handle)
{
	int groupid, grpidx;
	k_ptr_t vgp;
	kaddr_t vertex, vertex_array;
	
	groupid = handle_to_groupid(handle);
	grpidx = handle_to_grpidx(handle);

	vgp = (k_ptr_t)((uint)K_HWGRAPH(klp) + 
			kl_member_offset(klp, "graph_s", "graph_vertex_group") + 
			(groupid * GRAPH_VERTEX_GROUP_S_SIZE(klp)));

	vertex_array = 
		kl_kaddr(klp, vgp, "graph_vertex_group_s", "group_vertex_array");

	vertex = vertex_array + (grpidx * VERTEX_SIZE(klp));
	return(vertex);
}

/* 
 * handle_to_vertex_group()
 */
kaddr_t
handle_to_vertex_group(klib_t *klp, k_uint_t handle)
{
	int groupid;
	kaddr_t group;

	groupid = handle_to_groupid(handle);

	group = ((uint)K_HWGRAPH(klp) + kl_member_offset(klp, "graph_s", 
		"graph_vertex_group") + (groupid * GRAPH_VERTEX_GROUP_S_SIZE(klp)));
	return(group);
}

/*
 * groupid_to_group()
 */
kaddr_t
groupid_to_group(klib_t *klp, int groupid)
{
	kaddr_t group;

	group = (K_HWGRAPHP(klp) + kl_member_offset(klp, "graph_s", 
		"graph_vertex_group") + (groupid * GRAPH_VERTEX_GROUP_S_SIZE(klp)));
	return(group);
}

/*
 * grpidx_to_handle()
 */
int
grpidx_to_handle(klib_t *klp, int groupid, int grpidx)
{
	int handle;

	handle = (((groupid) << GRAPH_VERTEX_PER_GROUP_LOG2) | (grpidx));
	return(handle);
}

/*
 * vertex_to_handle()
 */
int
vertex_to_handle(klib_t *klp, kaddr_t vertex) 
{
	int i, num_vertex, handle;

	num_vertex = kl_uint(klp, K_HWGRAPH(klp), "graph_s", "graph_num_vertex", 0);

	for (i = 1; i < num_vertex; i++) {
		if (handle_to_vertex(klp, i) == vertex) {
			return(i);
		}
	}
	return(-1);
}

/* 
 * vertex_edge_ptr()
 */
kaddr_t
vertex_edge_ptr(klib_t *klp, k_ptr_t vertex)
{
	return(kl_kaddr(klp, vertex, "graph_vertex_s", "v_info_list"));
}

/* 
 * vertex_lbl_ptr()
 */
kaddr_t
vertex_lbl_ptr(klib_t *klp, k_ptr_t vertex)
{
	int num_edge;
	kaddr_t info_list;

	num_edge = kl_uint(klp, vertex, "graph_vertex_s", "v_num_edge", 0);
	info_list = kl_kaddr(klp, vertex, "graph_vertex_s", "v_info_list");
	return(info_list + num_edge * GRAPH_EDGE_S_SIZE(klp));
}

/*
 * get_edge_name()
 */
int
get_edge_name(klib_t *klp, k_ptr_t edge, char *name)
{
	kaddr_t namp;

	/* Get the pointer to the edge label string
	 */
	namp = kl_kaddr(klp, edge, "graph_edge_s", "e_label");

	/* Copy the label string into the buffer provided
	 */
	kl_get_block(klp, namp, 24, name, "graph_edge_name");
	if (KL_ERROR) {
		return(1);
	}
	return(0);
}

/*
 * get_label_name()
 */
int
get_label_name(klib_t *klp, k_ptr_t labelp, char *name)
{
	kaddr_t namp;

	/* Get the pointer to the info name string
	 */
	namp = kl_kaddr(klp, labelp, "graph_edge_s", "e_label");

	/* Copy the label string into the buffer provided
	 */
	kl_get_block(klp, namp, 24, name, "graph_info_name");
	if (KL_ERROR) {
		return(1);
	}
	return(0);
}

/*
 * vertex_edge_vhndl() -- find the edge with vhndl
 */
k_ptr_t
vertex_edge_vhndl(klib_t *klp, k_ptr_t gvp, int vhndl, k_ptr_t gep)
{
	int i, num_edge, gep_alloced = 0;
	kaddr_t labelp;
	char label[256];
	

	/* If we wern't passed an edge buffer pointer, allocate some 
	 * space now.
	 */
	if (!gep) {
		gep = klib_alloc_block(klp, GRAPH_EDGE_S_SIZE(klp), K_TEMP);
		gep_alloced++;
	}

	/* Determine how many edge entries this vertex has
	 */
	num_edge = kl_uint(klp, gvp, "graph_vertex_s", "v_num_edge", 0);

	for (i = 0; i < num_edge; i++) {
		kl_get_graph_edge_s(klp, gvp, (k_uint_t)i, 0, gep, 0);
		if (KL_ERROR) {
			return((k_ptr_t)NULL);
		}

		/* Get the pointer to the label string
		 */
		labelp = kl_kaddr(klp, gep, "graph_edge_s", "e_label");

		/* Get the label string
		 */
		kl_get_block(klp, labelp, 24, label, "graph_edge_label");

		if (vhndl == kl_uint(klp, gep, "graph_edge_s", "e_vertex", 0)) {
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
		klib_free_block(klp, gep);
	}
	return((k_ptr_t)NULL);
}

/*
 * vertex_edge_name() -- find the named edge in vertex
 */
k_ptr_t
vertex_edge_name(klib_t *klp, k_ptr_t gvp, char *name, k_ptr_t gep)
{
	int i, num_edge, gep_alloced = 0;
	kaddr_t labelp;
	char label[256];
	

	/* If we wern't passed an edge buffer pointer, allocate some 
	 * space now.
	 */
	if (!gep) {
		gep = klib_alloc_block(klp, GRAPH_EDGE_S_SIZE(klp), K_TEMP);
		gep_alloced++;
	}

	/* Determine how many edge entries this vertex has
	 */
	num_edge = kl_uint(klp, gvp, "graph_vertex_s", "v_num_edge", 0);

	for (i = 0; i < num_edge; i++) {
		kl_get_graph_edge_s(klp, gvp, (k_uint_t)i, 0, gep, 0);
		if (KL_ERROR) {
			if (gep_alloced) {
				klib_free_block(klp, gep);
			}
			return((k_ptr_t)NULL);
		}

		/* Get the pointer to the label string
		 */
		labelp = kl_kaddr(klp, gep, "graph_edge_s", "e_label");

		/* Get the label string
		 */
		kl_get_block(klp, labelp, 24, label, "graph_edge_label");

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
		klib_free_block(klp, gep);
	}
	return((k_ptr_t)NULL);
}

/*
 * vertex_info_name() -- find the named label in vertex
 */
k_ptr_t
vertex_info_name(klib_t *klp, k_ptr_t gvp, char *name, k_ptr_t gip)
{
	int i, num_info, gip_alloced = 0;
	kaddr_t labelp;
	char label[256];
	

	/* If we wern't passed an info buffer pointer, allocate some 
	 * space now.
	 */
	if (!gip) {
		gip = klib_alloc_block(klp, GRAPH_INFO_S_SIZE(klp), K_TEMP);
		gip_alloced++;
	}

	/* Determine how many info entries this vertex has
	 */
	num_info = kl_uint(klp, gvp, "graph_vertex_s", "v_num_info_LBL", 0);

	for (i = 0; i < num_info; i++) {
		kl_get_graph_info_s(klp, gvp, i, 0, gip, 0);
		if (KL_ERROR) {
			if (gip_alloced) {
				klib_free_block(klp, gip);
			}
			return((k_ptr_t)NULL);
		}

		/* Get the pointer to the label string
		 */
		labelp = kl_kaddr(klp, gip, "graph_info_s", "i_label");

		/* Get the label string
		 */
		kl_get_block(klp, labelp, 24, label, "graph_info_label");

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
		klib_free_block(klp, gip);
	}
	return((k_ptr_t)NULL);
}

/*
 * connect_point() -- return connect vhndl for vertex
 */
int
connect_point(klib_t *klp, int vhndl)
{
	k_uint_t conn_vhndl;	
	k_ptr_t gvp, idx_ptr;

	gvp = klib_alloc_block(klp, VERTEX_SIZE(klp), K_TEMP);

	kl_get_graph_vertex_s(klp, vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		klib_free_block(klp, gvp);
		return(0);
	}

	idx_ptr = (k_ptr_t)((uint)gvp + 
			(GRAPH_VERTEX_S_SIZE(klp) + (HWGRAPH_CONNECTPT * K_NBPW(klp))));

	bcopy(idx_ptr, &conn_vhndl, 8);
	if (PTRSZ32(klp)) {
		conn_vhndl = conn_vhndl >> 32;
	}

	klib_free_block(klp, gvp);
	return((int)conn_vhndl);
}

/* 
 * is_symlink() -- Determine if an edge is a "file" or a "symlink"
 *
 * The first edge from a vertex's parent to the parent is considered
 * by the hwgfs file system code to be a "file." Any subsequent edges
 * to the same vertex are considered "symlinks."
 */
int
is_symlink(klib_t *klp, k_ptr_t gvp, int edge, int hndl)
{
	int i, num_edge, vhndl;
	k_ptr_t gep;

	kl_reset_error();

	num_edge = kl_uint(klp, gvp, "graph_vertex_s", "v_num_edge", 0);
	if (edge >= num_edge) {
		KL_SET_ERROR_NVAL(KLE_BAD_VERTEX_EDGE, edge, 0);
		return(0);
	}

	gep = klib_alloc_block(klp, GRAPH_EDGE_S_SIZE(klp), K_TEMP);
	for (i = 0; i < edge; i++) {
		kl_get_graph_edge_s(klp, gvp, (k_uint_t)i, 0, gep, 0);
		if (KL_ERROR) {
			klib_free_block(klp, gep);
			return(0);
		}
		vhndl = kl_uint(klp, gep, "graph_edge_s", "e_vertex", 0);
		if (hndl == vhndl) {
			klib_free_block(klp, gep);
			return(1);
		}
	}
	klib_free_block(klp, gep);
	return(0);
}

/*
 * add_path_chunk()
 */
void
add_path_chunk(klib_t *klp, path_t *path)
{
	path_chunk_t *p, *pcp;

	pcp = (path_chunk_t*)klib_alloc_block(klp, sizeof(path_chunk_t), K_TEMP);
	ENQUEUE(&path->pchunk, pcp);
}

/*
 * init_path_table()
 */
path_t *
init_path_table(klib_t *klp)
{
	path_t *p;
	path_rec_t *pr;

	/* Allocate the path table and the string table.
	 */
	p = (path_t*)klib_alloc_block(klp, sizeof(path_t), K_TEMP);
	p->st = kl_init_string_table(klp, K_TEMP);

	/* Allocate the first path_chunk
	 */
	add_path_chunk(klp, p);

	/* Allocate the first path element (hwgraph_root)
	 */
	pr = (path_rec_t*)klib_alloc_block(klp, sizeof(path_rec_t), K_TEMP);
	pr->vhndl = 1;
	pr->name = kl_get_string(klp, p->st, "hw", K_TEMP);
	pr->next = pr->prev = pr;
	PATH(p) = pr;
	return(p);
}

/*
 * add_to_path()
 */
void
add_to_path(klib_t *klp, path_rec_t *path, path_rec_t *new)
{
	ENQUEUE(&path, new);
}

/*
 * free_path_records()
 */
void
free_path_records(klib_t *klp, path_rec_t *path)
{
	path_rec_t *element, *next;

	element = path;
	do {
		next = element->next;
		klib_free_block(klp, (k_ptr_t)element);
		element = next;
	} while (element != path);
}

/*
 * free_path()
 */
void 
free_path(klib_t *klp, path_t *p)
{
	int i;
	path_chunk_t *pc, *pcnext;
	
	pc = p->pchunk;
	do {
		for (i = 0; i <= pc->current; i++) {
			free_path_records(klp, pc->path[i]);
		}
		pcnext = pc->next;
		klib_free_block(klp, (k_ptr_t)pc);
		pc = pcnext;
	} while (pc != p->pchunk);
	kl_free_string_table(klp, p->st);
	klib_free_block(klp, (k_ptr_t)p);
}

/*
 * push_path()
 */
void
push_path(klib_t *klp, path_t *path, path_rec_t *pr, char *name, int vhndl)
{
	path_rec_t *p;
	
	p = (path_rec_t*)klib_alloc_block(klp, sizeof(path_rec_t), K_TEMP);
	p->name = kl_get_string(klp, path->st, name, K_TEMP);
	p->vhndl = vhndl;
	add_to_path(klp, pr, p);
}

/*
 * pop_path()
 */
void
pop_path(klib_t *klp, path_rec_t *path)
{
	path_rec_t *p = path->prev;

	REMQUEUE(&path, p);
	klib_free_block(klp, (k_ptr_t)p);
}

/*
 * dup_current_path()
 */
void
dup_current_path(klib_t *klp, path_t *pl)
{
	path_rec_t *p, *np, *new_path = (path_rec_t*)NULL;
	path_chunk_t *pcp;

	p = PATH(pl);

	do {
		np = (path_rec_t *)klib_alloc_block(klp, sizeof(path_rec_t), K_TEMP);
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
		add_path_chunk(klp, pl);
		PATH(pl) = new_path;
	}
	else {
		pcp->path[pcp->current + 1] = new_path;
		pcp->current++;
	}
	pl->count++;
}

/*
 * hw_find_pathname()
 */
int
hw_find_pathname(klib_t *klp, int level, char *name, path_t *p, int flags)
{
	int e, i, ret, cur_vhndl, vhndl, num_edge, num_info;
	k_ptr_t gvp, gep, gip;
	path_rec_t *np;
	kaddr_t edgep, infop, labelp;
	char label[256];

	/* Allocate some buffers
	 */
	gvp = klib_alloc_block(klp, VERTEX_SIZE(klp), K_TEMP);
	gep = klib_alloc_block(klp, GRAPH_EDGE_S_SIZE(klp), K_TEMP);
	gip = klib_alloc_block( klp, GRAPH_INFO_S_SIZE(klp), K_TEMP);

	/* Get the vertex for the current path level
	 */
	cur_vhndl = PATH(p)->prev->vhndl;
	kl_get_graph_vertex_s(klp, cur_vhndl, 0, gvp, 0);
	if (KL_ERROR) {
		if (gvp) {
			klib_free_block(klp, gvp);
		}
		klib_free_block(klp, gep);
		klib_free_block(klp, gip);
		return(1);
	}

	/* Check to see if name matches with an label for the current 
	 * vertex
	 */
	if (name && vertex_info_name(klp, gvp, name, gip)) {
		dup_current_path(klp, p);
	}

	/* Walk the edges for this vertex. We don't have to pop the
	 * element off the path before we return because it wasn't 
	 * added here.
	 */
	num_edge = kl_uint(klp, gvp, "graph_vertex_s", "v_num_edge", 0);
	if (num_edge == 0) {
		if (!name) {
			dup_current_path(klp, p);
		}
		klib_free_block(klp, gvp);
		klib_free_block(klp, gep);
		klib_free_block(klp, gip);
		return(0);
	}
	for (e = 0; e < num_edge; e++) {

		kl_get_graph_edge_s(klp, gvp, (k_uint_t)e, 0, gep, flags);
		if (KL_ERROR) {
			klib_free_block(klp, gvp);
			klib_free_block(klp, gep);
			klib_free_block(klp, gip);
			return(1);
		}

		/* Get the edge name and vertex handle and add the element
		 * to the path.
		 */
		vhndl = kl_uint(klp, gep, "graph_edge_s", "e_vertex", 0);
		get_edge_name(klp, gep, label);
		push_path(klp, p, PATH(p), label, vhndl);

		/* Check to see if the current edge is the one we are looking 
		 * for. If it is, dup the current path and continue. Otherwise, 
		 * recursively call hw_find_pathname() to walk to the next path 
		 * element.
		 */
		if (name && !strcmp(name, label)) {
			dup_current_path(klp, p);
			pop_path(klp, PATH(p));
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
		if ((connect_point(klp, vhndl) != cur_vhndl) || 
										is_symlink(klp, gvp, e, vhndl)) {
			if (!name) {
				dup_current_path(klp, p);
			}
			pop_path(klp, PATH(p));
			continue;
		}

		ret = hw_find_pathname(klp, level + 1, name, p, flags);
		pop_path(klp, PATH(p));

		if (ret) {
			/* XXX -- what happens when the return value is '1' ???
			 */
			if (DEBUG(KLDC_GLOBAL, 1)) {
				fprintf(KL_ERRORFP, "ERROR!!! cur_vhndl=%d, level=%d, "
					"ret=%d\n", cur_vhndl, level, ret);
			}
		}
	}

	/* If this is the last level we are returning from, we have to
	 * remove the /hw entry except when name is "hw".
	 */
	if ((level == 0) && (!name || strcmp(name, "hw"))) {
		free_path_records(klp, PATH(p));
		PATH(p) = (path_rec_t*)NULL;
		p->pchunk->prev->current--;
	}
	klib_free_block(klp, gvp);
	klib_free_block(klp, gep);
	klib_free_block(klp, gip);
	return(0);
}

/*
 * kl_pathname_to_vertex()
 */
int
kl_pathname_to_vertex(klib_t *klp, char *path, int start)
{
	return(0);
}

/*
 * kl_vertex_to_pathname()
 */
path_rec_t *
kl_vertex_to_pathname(klib_t *klp, kaddr_t value, int mode)
{
	int e, handle, cp, num_edge;
	kaddr_t vertex;
	k_ptr_t gvp, gep;
	path_rec_t *path, *p;
	char label[256];

	gvp = klib_alloc_block(klp, VERTEX_SIZE(klp), K_TEMP);

	/* Before we go any further, we have to make sure we were given
	 * a valid vertex handle/address. We don't really need the sturct,
	 * except as a check to make sure the vertex is valid. It's not big
	 * deal since we need the gvp block later anyway...
	 */
	kl_get_graph_vertex_s(klp, value, mode, gvp, 0);
	if (KL_ERROR) {
		klib_free_block(klp, gvp);
		return((path_rec_t *)NULL);
	}

	/* Depending on mode value, set vertex handle.
	 */
	if (mode == 0) {
		handle = value;
	}
	else if (mode == 2) {
		handle = vertex_to_handle(klp, value);
		if (handle == -1) {
			klib_free_block(klp, gvp);
			return((path_rec_t *)NULL);
		}
	}
	else {
		klib_free_block(klp, gvp);
		return((path_rec_t *)NULL);
	}

	/* Allocate the first (last) element in the pathname
	 */
	path = (path_rec_t *)klib_alloc_block(klp, sizeof(path_rec_t), K_TEMP);
	path->vhndl = handle;
	cp = connect_point(klp, path->vhndl);
	gep = klib_alloc_block(klp, GRAPH_EDGE_S_SIZE(klp), K_TEMP);

	while (cp) {
		kl_get_graph_vertex_s(klp, cp, 0, gvp, 0);
		if (KL_ERROR) {
			goto done;
		}

		/* Walk the edges for this vertex and look for the edge that 
		 * contains this vertex handle.
		 */
		num_edge = kl_uint(klp, gvp, "graph_vertex_s", "v_num_edge", 0);
		if (num_edge == 0) {
			goto done;
		}
		for (e = 0; e < num_edge; e++) {
			kl_get_graph_edge_s(klp, gvp, (k_uint_t)e, 0, gep, 0);
			if (KL_ERROR) {
				goto done;
			}

			/* Get the edge vertex handle and check to see if it matches
			 * the one in our current element.
			 */
			handle = kl_uint(klp, gep, "graph_edge_s", "e_vertex", 0);
			if (handle == path->vhndl) {
				break;
			}
		}
		if (e == num_edge) {
			/* We didn't find the edge we were looking for!
			 */
			goto done;
		}
		if (get_edge_name(klp, gep, label)) {
			goto done;
		}
		path->name = (char *)klib_alloc_block(klp, strlen(label) +1, K_TEMP);
		strcat(path->name, label);
		p = (path_rec_t *)klib_alloc_block(klp, sizeof(path_rec_t), K_TEMP);
		p->next = path;
		path = p;
		path->vhndl = cp;
		cp = connect_point(klp, path->vhndl);
	}

done:

	path->name = (char *)klib_alloc_block(klp, 3, K_TEMP);
	if (path->vhndl == 1) {
		strcat(path->name, "hw");
	}
	else {
		strcat(path->name, "??");
	}
	klib_free_block(klp, gvp);
	klib_free_block(klp, gep);
	return(path);
}

/*
 * kl_free_pathname()
 *
 * Routine that can be called from the outside. It will free all
 * of the memory associated with a particular pathname (it assumes that
 * the memory blocks containing the name of each path element are allocated 
 * indifidually). All pathnames that use string table space must be
 * freed using free_path_records().
 */
void
kl_free_pathname(klib_t *klp, path_rec_t *path)
{
	path_rec_t *element, *next;

	element = path;
	while (element) {
		next = element->next;
		if (element->name) {
			klib_free_block(klp, (k_ptr_t)element->name);
		}
		klib_free_block(klp, (k_ptr_t)element);
		element = next;
	}
}

/*
 * vertex_get_next() -- walk the entire list of vertices
 *
 * When there are no more vertex handles to return, get_next returns
 * GRAPH_HIT_LIMIT.
 */
int
vertex_get_next(klib_t *klp, int *vertex_found, int *placeptr)
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
    grpidx = handle_to_grpidx(handle);

	ggp = klib_alloc_block(klp, GRAPH_VERTEX_GROUP_S_SIZE(klp), K_TEMP);
	handle_limit = 
		kl_uint(klp, K_HWGRAPH(klp), "graph_s", "graph_vertex_handle_limit", 0);

    for (; groupid < GRAPH_NUM_GROUP; groupid++) {

        group = groupid_to_group(klp, groupid);
		kl_get_struct(klp, group, GRAPH_VERTEX_GROUP_S_SIZE(klp), 
								ggp, "graph_vertex_group_s");

        handle = grpidx_to_handle(klp, groupid, grpidx);

        if (kl_uint(klp, ggp, "graph_vertex_group_s", "group_count", 0) != 0) {

            for (; grpidx < GRAPH_VERTEX_PER_GROUP; grpidx++, handle++) {

				if (handle > handle_limit) {
					klib_free_block(klp, ggp);
                    return(2);
				}

				vertex = handle_to_vertex(klp, handle);
				if (KL_ERROR) {
					klib_free_block(klp, ggp);
					return(1);
				}
                *vertex_found = handle;
                *placeptr = handle;
				klib_free_block(klp, ggp);
				return(0);
            }
        }
        grpidx = 0;
    }	
	klib_free_block(klp, ggp);
    return(2);
}

/*
 * print_mfg_info()
 */
int
print_mfg_info(klib_t *klp, char *module, char *slot, char 
				*board, char *s, int flags, FILE *ofp)
{
	char *Part = "";
	char *Name = "";
	char *Serial = "";
	char *Revision = "";
	char *Group = "";
	char *Capability = "";
	char *Variety = "";
	char *Laser = "";
	char *Location = "";
	char *NIC = "";
	char *t;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(ofp, "module=%s, slot=%s board=%s\n", module, slot, board);
		fprintf(ofp, "%s\n\n", NIC);
	}

	/* Get the first token
	 */
	t = strtok(s, ":;");

	if (s) {
		s = (char *)0;
	}

	while (t) {

		if (!strcmp(t,"Part")) {
			if (!(Part = strtok(s,":;"))) {
				break;
			}
		}
				
		if (!strcmp(t,"Name")) {
			if (!(Name = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"Serial")) {
			if (!(Serial = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"Revision")) {
			if (!(Revision = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"Group")) {
			if (!(Group = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"Capability")) {
			if (!(Capability = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"Variety")) {
			if (!(Variety = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"Laser")) {

			if (module) {
				fprintf(ofp, "%6s  ", module);
			}
			else {
				fprintf(ofp, "    --  ");
			}

			if (Name[0]) {
				fprintf(ofp, "%-14s  ", Name);
			}
			else if (board) {
				fprintf(ofp, "%-14s  ", board);
			}
			else {
				fprintf(ofp, "NA              ");
			}

			if (slot) {
				fprintf(ofp, "%-12s  ", slot);
			}
			else {
				fprintf(ofp, "              ");
			}
			
			if (Serial[0]) {
				fprintf(ofp, "%12s  ", Serial);
			}
			else {
				fprintf(ofp, "          NA  ");
			}
			
			if (Part[0]) {
				fprintf(ofp, "%13s  ", Part);
			}
			else {
				fprintf(ofp, "           NA  ");
			}
			
			if (Revision[0]) {
				fprintf(ofp, "%3s\n", Revision);
			}
			else {
				fprintf(ofp, " NA\n");
			}
			
			if (DEBUG(KLDC_GLOBAL, 1)) {
				if (strcmp(NIC,"")) {
					printf("[missing info: %s] Laser %12s\n", NIC, Laser);
				}
			}

			/* Reset the field variables to NULL
			 */
			Part = "";
			Name = "";
			Serial = "";
			Revision = "";
			Group = "";
			Capability = "";
			Variety = "";
			Laser = "";
			Location = "";
			NIC = "";

			if (!(Laser = strtok(s,":;"))) {
				break;
			}
		}

		if (!strcmp(t,"NIC")) {
			if (!(NIC = strtok(s,":;"))) {
				break;
			}
		}

		/* Get the next token
		 */
		t = strtok(s, ":;");
	}
	return(0);
}
