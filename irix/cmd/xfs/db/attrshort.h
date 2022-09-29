#ident "$Revision: 1.3 $"

#if VERS >= V_62
extern const field_t	attr_sf_entry_flds[];
extern const field_t	attr_sf_hdr_flds[];
extern const field_t	attr_shortform_flds[];
extern const field_t	attrshort_hfld[];

extern int	attr_sf_entry_size(void *obj, int startoff, int idx);
extern int	attrshort_size(void *obj, int startoff, int idx);
#endif
