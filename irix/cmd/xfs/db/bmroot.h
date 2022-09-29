#ident "$Revision: 1.5 $"

struct field;

#if VERS >= V_62
extern const struct field	bmroota_flds[];
extern const struct field	bmroota_key_flds[];
#endif
extern const struct field	bmrootd_flds[];
extern const struct field	bmrootd_key_flds[];

#if VERS >= V_62
extern int	bmroota_size(void *obj, int startoff, int idx);
#endif
extern int	bmrootd_size(void *obj, int startoff, int idx);
