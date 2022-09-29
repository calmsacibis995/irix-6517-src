#ident "$Revision: 1.1 $"

extern const field_t	dir2_flds[];
extern const field_t	dir2_hfld[];
extern const field_t	dir2_block_tail_flds[];
extern const field_t	dir2_data_free_flds[];
extern const field_t	dir2_data_hdr_flds[];
extern const field_t	dir2_data_union_flds[];
extern const field_t	dir2_free_hdr_flds[];
extern const field_t	dir2_leaf_entry_flds[];
extern const field_t	dir2_leaf_hdr_flds[];
extern const field_t	dir2_leaf_tail_flds[];

extern int	dir2_data_union_size(void *obj, int startoff, int idx);
extern int	dir2_size(void *obj, int startoff, int idx);
