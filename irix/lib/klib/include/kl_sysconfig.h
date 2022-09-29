#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_sysconfig.h,v 1.1 1999/02/23 20:38:33 tjm Exp $"

/* This struct is the header for system configuration information for all
 * system types. The hwcmp_root field points to all hardware components 
 * installed in the system (stored in an htree structure). Exactly what 
 * type of components are included is dependent on the type of system.
 */
typedef struct sysconfig_s {
	int						flags;		/* K_PERM/K_TEMP, etc. */
	k_uint_t				sys_id;
	int						sys_type;	/* IP type of system */
	time_t					date; 		/* if 0 then sysconfig is current */
	hwconfig_t			   *hwconfig;
	swconfig_t			   *swconfig;
	hw_component_t		   *hwcmp_archive;
	sw_component_t		   *swcmp_archive;
} sysconfig_t;

#define hwcmp_root hwconfig->h_hwcmp_root
#define hwcmp_cnt hwconfig->h_hwcmp_cnt
#define swcmp_root swconfig->s_swcmp_root
#define swcmp_cnt swconfig->s_swcmp_cnt

sysconfig_t *alloc_sysconfig(
	int		/* Flag value for block allocation (K_TEMP/K_PERM) */);

void free_sysconfig(
	sysconfig_t *	/* pointer to sysconfig_s struct */);

void update_sysconfig(
	sysconfig_t *	/* pointer to sysconfig_s struct */);
