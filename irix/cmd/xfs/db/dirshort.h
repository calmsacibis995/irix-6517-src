#ident "$Revision: 1.3 $"

extern const field_t	dir_sf_entry_flds[];
extern const field_t	dir_sf_hdr_flds[];
extern const field_t	dir_shortform_flds[];
extern const field_t	dirshort_hfld[];

extern int	dir_sf_entry_size(void *obj, int startoff, int idx);
extern int	dirshort_size(void *obj, int startoff, int idx);
