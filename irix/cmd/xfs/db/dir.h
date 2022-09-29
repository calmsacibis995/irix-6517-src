#ident "$Revision: 1.4 $"

extern const field_t	dir_flds[];
extern const field_t	dir_hfld[];
extern const field_t	dir_blkinfo_flds[];
extern const field_t	dir_leaf_entry_flds[];
extern const field_t	dir_leaf_hdr_flds[];
extern const field_t	dir_leaf_map_flds[];
extern const field_t	dir_leaf_name_flds[];
extern const field_t	dir_node_entry_flds[];
extern const field_t	dir_node_hdr_flds[];

extern int	dir_leaf_name_size(void *obj, int startoff, int idx);
extern int	dir_size(void *obj, int startoff, int idx);
