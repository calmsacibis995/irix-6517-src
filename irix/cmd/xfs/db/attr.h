#ident "$Revision: 1.3 $"

#if VERS >= V_62
extern const field_t	attr_flds[];
extern const field_t	attr_hfld[];
extern const field_t	attr_blkinfo_flds[];
extern const field_t	attr_leaf_entry_flds[];
extern const field_t	attr_leaf_hdr_flds[];
extern const field_t	attr_leaf_map_flds[];
extern const field_t	attr_leaf_name_flds[];
extern const field_t	attr_node_entry_flds[];
extern const field_t	attr_node_hdr_flds[];

extern int	attr_leaf_name_size(void *obj, int startoff, int idx);
extern int	attr_size(void *obj, int startoff, int idx);
#endif
