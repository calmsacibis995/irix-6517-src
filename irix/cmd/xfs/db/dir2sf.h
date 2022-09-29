#ident "$Revision: 1.1 $"

extern const field_t	dir2sf_flds[];
extern const field_t	dir2_inou_flds[];
extern const field_t	dir2_sf_hdr_flds[];
extern const field_t	dir2_sf_entry_flds[];

extern int	dir2sf_size(void *obj, int startoff, int idx);
extern int	dir2_inou_size(void *obj, int startoff, int idx);
extern int	dir2_sf_entry_size(void *obj, int startoff, int idx);
extern int	dir2_sf_hdr_size(void *obj, int startoff, int idx);
