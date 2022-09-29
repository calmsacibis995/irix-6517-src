#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_hwgraph.h,v 1.2 1999/05/08 04:07:05 tjm Exp $"

/* Some defines stolen (for obvious reasons) from graph_private.h
 */
#define GRAPH_VERTEX_FREE   (void *)0L
#define GRAPH_VERTEX_NOINFO (void *)1L
#define GRAPH_VERTEX_NEW    (void *)2L
#define GRAPH_VERTEX_SPECIAL    2L

extern int graph_vertex_per_group;
extern int graph_vertex_per_group_log2;

#define GRAPH_VERTEX_PER_GROUP graph_vertex_per_group
#define GRAPH_VERTEX_PER_GROUP_LOG2 graph_vertex_per_group_log2
#define GRAPH_VERTEX_PER_GROUP_MASK (GRAPH_VERTEX_PER_GROUP - 1)

#define GRAPH_NUM_GROUP 128
#define GRAPH_NUM_VERTEX_MAX (GRAPH_NUM_GROUP * GRAPH_VERTEX_PER_GROUP)

#define handle_to_groupid(handle) (handle >> GRAPH_VERTEX_PER_GROUP_LOG2)
#define handle_to_grpidx(handle) (handle & GRAPH_VERTEX_PER_GROUP_MASK)

#define PATH(p) (p->pchunk->prev->path[p->pchunk->prev->current])

#define PATHS_PER_CHUNK 100

typedef struct path_chunk_s {
	struct path_chunk_s	   *next;
	struct path_chunk_s	   *prev;
	int						current;
	struct path_rec_s      *path[PATHS_PER_CHUNK];
} path_chunk_t;

typedef struct path_rec_s {
	struct path_rec_s      *next;
	struct path_rec_s      *prev;
	int                     vhndl;
	char                   *name;
} path_rec_t;

typedef struct path_s {
	int             	    count;
	path_chunk_t           *pchunk;
	string_table_t	       *st;
} path_t;

/** 
 ** hwgraph operation function prototypes
 **/

/* Convert a vertex handle to the kernel address of the vertex.
 */
kaddr_t kl_handle_to_vertex(
	k_uint_t 	/* vertex handle */);

void kl_init_hwgraph();

/* Convert a vertex handle to the kernel address of the vertex
 * group.
 */
kaddr_t kl_handle_to_vertex_group(
	k_uint_t 	/* handle */);

/* Convert a vertex groupid to the kernel address of the vertex
 * group.
 */
kaddr_t kl_groupid_to_group(
	int 		/* vertex groupid */);

/* Convert the group index to a vertex handle.
 */
int kl_grpidx_to_handle(
	int 		/* vertex groupid */,
	int 		/* vertex group index */);

/* Convert the kernel address of a vertex to a vertex handle.
 */
int kl_vertex_to_handle(
	kaddr_t 	/* kernel address of vertex */);

/* Return the kernel address of the first edge entry in a vertex.
 */
kaddr_t kl_vertex_edge_ptr(
	k_ptr_t		/* pointer to block of memory containing vertex */);

/* Return the kernel address of the first lable entry in a vertex.
 */
kaddr_t kl_vertex_lbl_ptr(
	k_ptr_t		/* pointer to block of memory containing vertex */);

/* Copy the name of vertex label into the buffer provided. In the
 * event of an error, a value of one (1) will be returned. Otherwise, 
 * a value of zero (0) will returned.
 */
int kl_get_edge_name(
	k_ptr_t		/* pointer to block of memory containing edge */, 
	char*		/* name of edge we are looking for */);

/* Copy the name of a vertex edge into the buffer provided. In the
 * event of an error, a value of one (1) will be returned. Otherwise,
 * a value of zero (0) will returned.
 */
int kl_get_label_name(
	k_ptr_t		/* pointer to block of memory containing label */, 
	char*		/* name of label we are looking for */);

/* Find the vertex edge with the specified vertex handle. If the
 * edge is found, return a pointer to the buffer containing the
 * graph_edge_s struct (if no buffer pointer was passed in, a 
 * buffer will be allocated). If the edge was not found, then 
 * return an NULL pointer.
 */
k_ptr_t kl_vertex_edge_vhndl(
	k_ptr_t 	/* pointer to buffer containing vertex */, 
	int 		/* handle of edge we are looking for */, 
	k_ptr_t 	/* pointer to block where edge will be placed */);

/* Find the vertex edge having the specified name. If the name is 
 * found, returns a pointer to the buffer containing the graph_edge_s 
 * struct (if no buffer pointer was passed in, a buffer will be 
 * allocated). If the name was not found, then return an NULL pointer.
 */
k_ptr_t kl_vertex_edge_name(
	k_ptr_t		/* pointer to buffer containing vertex */, 
	char*		/* name of edge we are looking for */, 
	k_ptr_t		/* pointer to block where edge will be placed */);

/* Find the vertex info entry having the specified name. If the 
 * name is found, return a pointer to the buffer containing the 
 * graph_info_s struct (if no buffer pointer was passed in, a 
 * buffer will be allocated). If the name was not found, then 
 * return an NULL pointer.
 */
k_ptr_t kl_vertex_info_name(
	k_ptr_t		/* pointer to buffer containing vertex */, 
	char*		/* name of edge we are looking for */, 
	k_ptr_t		/* pointer to block where info will be placed */);

/* Returns the handle of the vertex the current vertex is 
 * connected to. If an error occured, a value of zero (0) will
 * be returned.
 */
int kl_connect_point(
	int 		/* vertex handle */);

/* The first edge from a vertex's parent to the vertex is 
 * considered by the hwgfs file system code to be a "file." 
 * Any subsequent edges to the same vertex are considered 
 * "symlinks." The is_symlink() function determines if an 
 * edge is a file or a symlink. Returns a value of one (1) 
 * if a particular edge is a file and a value of zero (0)
 * if it is a symlink or an error occurred.
 */
int kl_is_symlink(
	k_ptr_t		/* pointer to buffer containing vertex */, 
	int			/* handle of edge in vertex */, 
	int 		/* handle of vertex */);

/* Allocate space for a path table and first patch chunk, allocate 
 * the first path element (for /hw), and return a pointer to the
 * path table. If an error occurred, a NULL pointer will be returned.
 */
path_t *kl_init_path_table();

path_rec_t *kl_clone_path(
	path_rec_t* /* pointer to path record */,
	int			/* block allocation flag */);

/* Free all memory resources associated with a path record.
 */
void kl_free_path_records(
	path_rec_t*	/* pointer to path record */);

/* Free all memory resources associated with a path table.
 */
void kl_free_path(
	path_t*		/* pointer to path table */);

/* Find all unique pathnames (does not follow aliases) that contain 
 * a specified vertex edge or label name. Path names for all matches 
 * will be entered into the path table provided. Upon success, a
 * value of zero (0) will be returned. A value of one (1) will be 
 * returned in the event an error occurred.
 */
int kl_find_pathname(
	int			/* level of search -- must be 0 */, 
	char*		/* element name we are searching for */, 
	path_t*		/* pointer to path table */, 
	int			/* flags */,
	int			/* if NON-zero, only include paths with vertix */);

/* Return a full pathname terminating with the vertex specified by 
 * the vertex handle or address provided. Upon success, a pointer to
 * a path_rec_s struct will be returned. In the event of failure, 
 * a NULL pointer will be returned.
 */ 
path_rec_t *kl_vertex_to_pathname(
	kaddr_t		/* value -- vertex handle or address */, 
	int			/* flag that determins how value is to be treated */,
	int			/* block allocation falg */);

/* Frees all memory associated with a particular pathname (it assumes 
 * that the memory blocks containing the name of each path element are 
 * allocated individually). All pathnames that use string table space 
 * must be freed using free_path_records().
 */
void kl_free_pathname(
	path_rec_t*	/* pointer to pathname record */);

/* Walks the entire list of vertexes, in order.
 */
int kl_vertex_get_next(
	int*		/* pointer to where next vertex handle will be placed */, 
	int* 		/* pointer to int variable -- placeholder */);

k_ptr_t kl_get_graph_edge_s(
	k_ptr_t     /* pointer to a buffer containing vertex (mode == 0) */,
	kaddr_t     /* kernel address of graph_info_s struct (mode == 2) */,
	int         /* mode value (see above) */,
	k_ptr_t     /* pointer to buffer where graph_edge_s will be placed */,
	int         /* flags */);

k_ptr_t kl_get_graph_info_s(
	k_ptr_t     /* pointer to a buffer containing vertex (mode == 0) */,
	kaddr_t     /* kernel address of graph_info_s struct (mode == 2) */,
	int         /* mode value (see above) */,
	k_ptr_t     /* pointer to buffer where graph_info_s will be placed */,
	int         /* flags */);

k_ptr_t kl_get_graph_vertex_s(
	kaddr_t     /* kernel address or handle of graph_vertex_s struct */,
	int         /* mode (2 == kernel address; 0 == handle) */,
	k_ptr_t     /* pointer to buffer where graph_vertex_s will be placed */,
	int         /* flags */);

