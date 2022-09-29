#ident "$Revision: 1.5 $"

struct field;

#if VERS >= V_62
extern const struct field	bmapbta_flds[];
extern const struct field	bmapbta_hfld[];
extern const struct field	bmapbta_key_flds[];
extern const struct field	bmapbta_rec_flds[];
#endif
extern const struct field	bmapbtd_flds[];
extern const struct field	bmapbtd_hfld[];
extern const struct field	bmapbtd_key_flds[];
extern const struct field	bmapbtd_rec_flds[];

#if VERS >= V_62
extern int	bmapbta_size(void *obj, int startoff, int idx);
#endif
extern int	bmapbtd_size(void *obj, int startoff, int idx);
