
#include <sys/types.h>
#include <sys/systm.h>
/*
 * Mandatory Access Control stubs.
 */
#ident "$Revision: 1.25 $"

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */

int mac_access()
{
	DOPANIC("mac_access stub");
	/* NOTREACHED */
}

int mac_cat_equ()
{
	DOPANIC("mac_cat_equ stub");
	/* NOTREACHED */
}

int mac_clntkudp_soattr()
{
	DOPANIC("mac_clntkudp_soattr stub");
	/* NOTREACHED */
}

int mac_copy()
{
	DOPANIC("mac_copy stub");
	/* NOTREACHED */
}

int mac_copyin_label()
{
	DOPANIC("mac_copyin_label stub");
	/* NOTREACHED */
}

int mac_dom()
{
	DOPANIC("mac_dom stub");
	/* NOTREACHED */
}

int mac_autofs_attr_get()
{
        DOPANIC("mac_autofs_attr_get stub");
        /* NOTREACHED */
}

int mac_autofs_attr_set()
{
        DOPANIC("mac_autofs_attr_set stub");
        /* NOTREACHED */
}

int mac_autofs_attr_list()
{
        DOPANIC("mac_autofs_attr_list stub");
        /* NOTREACHED */
}


int mac_eag_getlabel()
{
	DOPANIC("mac_eag_getlabel stub");
	/* NOTREACHED */
}

int mac_efs_attr_get()
{
	DOPANIC("mac_efs_attr_get stub");
	/* NOTREACHED */
}

int mac_efs_attr_set()
{
	DOPANIC("mac_efs_attr_set stub");
	/* NOTREACHED */
}

int mac_fdfs_attr_get()
{
	DOPANIC("mac_fdfs_attr_get stub");
	/* NOTREACHED */
}

int mac_fifo_attr_get()
{
	DOPANIC("mac_fifo_attr_get stub");
	/* NOTREACHED */
}

int mac_pipe_attr_get()
{
	DOPANIC("mac_pipe_attr_get stub");
	/* NOTREACHED */
}

int mac_pipe_attr_set()
{
	DOPANIC("mac_pipe_attr_set stub");
	/* NOTREACHED */
}

int mac_proc_attr_get()
{
	DOPANIC("mac_proc_attr_get stub");
	/* NOTREACHED */
}

int mac_efs_iaccess()
{
	DOPANIC("mac_efs_iaccess stub");
	/* NOTREACHED */
}

int mac_efs_setlabel()
{
	DOPANIC("mac_efs_setlabel stub");
	/* NOTREACHED */
}

int mac_xfs_ext_attr_fetch()
{
	DOPANIC("mac_xfs_ext_attr_fetch stub");
	/* NOTREACHED */
}

int mac_xfs_attr_get()
{
	DOPANIC("mac_xfs_attr_get stub");
	/* NOTREACHED */
}

int mac_spec_attr_get()
{
	DOPANIC("mac_spec_attr_get stub");
	/* NOTREACHED */
}

int mac_xfs_attr_set()
{
	DOPANIC("mac_xfs_attr_set stub");
	/* NOTREACHED */
}

int mac_xfs_iaccess()
{
	DOPANIC("mac_xfs_iaccess stub");
	/* NOTREACHED */
}

int mac_xfs_setlabel()
{
	DOPANIC("mac_xfs_setlabel stub");
	/* NOTREACHED */
}

int  mac_hwg_iaccess()	{ DOPANIC("mac_hwg_iaccess stub"); /* NOTREACHED */ }
int  mac_hwg_get()	{ DOPANIC("mac_hwg_get stub"); /* NOTREACHED */ }
int  mac_hwg_match()	{ DOPANIC("mac_hwg_match stub"); /* NOTREACHED */ }

int mac_nfs_iaccess()
{
	DOPANIC("mac_nfs_iaccess stub");
	/* NOTREACHED */
}

int mac_nfs_default()
{
	DOPANIC("mac_nfs_default stub");
	/* NOTREACHED */
}

int mac_nfs_get()
{
	DOPANIC("mac_nfs_get stub");
	/* NOTREACHED */
}

int mac_equ()
{
	DOPANIC("mac_equ stub");
	/* NOTREACHED */
}

int mac_get()
{
	DOPANIC("mac_get stub");
	/* NOTREACHED */
}

int mac_getplabel()
{
	DOPANIC("mac_getplabel stub");
	/* NOTREACHED */
}

int mac_inrange()
{
	DOPANIC("mac_inrange stub");
	/* NOTREACHED */
}

int mac_invalid()
{
	DOPANIC("mac_invalid stub");
	/* NOTREACHED */
}

int mac_is_moldy()
{
	DOPANIC("mac_is_moldy stub");
	/* NOTREACHED */
}

int mac_mint_equ()
{
	DOPANIC("mac_mint_equ stub");
	/* NOTREACHED */
}

int mac_moldy_path()
{
	DOPANIC("mac_moldy_path stub");
	/* NOTREACHED */
}

int mac_initial_path()
{
	DOPANIC("mac_initial_path stub");
	/* NOTREACHED */
}

int mac_msg_access()
{
	DOPANIC("mac_msg_access stub");
	/* NOTREACHED */
}

int mac_revoke()
{
	DOPANIC("mac_revoke stub");
	/* NOTREACHED */
}

int mac_same()
{
	DOPANIC("mac_same stub");
	/* NOTREACHED */
}

int mac_sem_access()
{
	DOPANIC("mac_sem_access stub");
	/* NOTREACHED */
}

int mac_vsetlabel()
{
	DOPANIC("mac_vsetlabel stub");
	/* NOTREACHED */
}

int mac_set()
{
	DOPANIC("mac_set stub");
	/* NOTREACHED */
}

int mac_setplabel()
{
	DOPANIC("mac_setplabel stub");
	/* NOTREACHED */
}

int mac_shm_access()
{
	DOPANIC("mac_shm_access stub");
	/* NOTREACHED */
}

ssize_t mac_size()
{
	DOPANIC("mac_size stub");
	/* NOTREACHED */
}

int mac_vaccess()
{
	DOPANIC("mac_vaccess stub");
	/* NOTREACHED */}

struct mac_label;

struct mac_label *mac_low_high_lp;
struct mac_label *mac_high_low_lp;
struct mac_label *mac_admin_high_lp;
struct mac_label *mac_equal_equal_lp;

struct cred;

struct cred *mac_process_cred;

struct mac_label *mac_vtolp()
{
	DOPANIC("mac_vtolp stub");
	/* NOTREACHED */
}

struct mac_label *mac_add_label()
{
	DOPANIC("mac_add_label stub");
	/* NOTREACHED */
}

struct mac_label *mac_unmold()
{
	DOPANIC("mac_unmold stub");
	/* NOTREACHED */
}

int msen_valid()
{
	DOPANIC("msen_valid stub");
	/* NOTREACHED */
}

int mint_valid()
{
	DOPANIC("mint_valid stub");
	/* NOTREACHED */
}

struct mac_b_label *msen_add_label()
{
	DOPANIC("msen_add_label stub");
	/* NOTREACHED */
}

struct mac_b_label *mint_add_label()
{
	DOPANIC("mint_add_label stub");
	/* NOTREACHED */
}

ssize_t msen_size()
{
	DOPANIC("msen_size stub");
	/* NOTREACHED */
}

ssize_t mint_size()
{
	DOPANIC("mint_size stub");
	/* NOTREACHED */
}

struct mac_label *mac_demld()
{
	DOPANIC("mac_demld stub");
	/* NOTREACHED */
}

struct mac_label *mac_dup()
{
	DOPANIC("mac_dup stub");
	/* NOTREACHED */
}

struct mac_label *mac_efs_getlabel()
{
	DOPANIC("mac_efs_getlabel stub");
	/* NOTREACHED */
}

struct mac_label *mac_xfs_getlabel()
{
	DOPANIC("mac_xfs_getlabel stub");
	/* NOTREACHED */
}

struct mac_label *mac_set_moldy()
{
	DOPANIC("mac_set_moldy stub");
	/* NOTREACHED */
}


void mac_init()	{}	/* Do not put a DOPANIC in this stub! */
void mac_eag_enable()	{ DOPANIC("mac_eag_enable stub"); }
void mac_confignote()	{ DOPANIC("mac_confignote stub"); }
void mac_mount()	{ DOPANIC("mac_mount stub"); }
void mac_mountroot()	{ DOPANIC("mac_mountroot stub"); }
void mac_init_cred()	{ DOPANIC("mac_init_cred stub"); }
void mac_msg_init()	{ DOPANIC("mac_msg_init stub"); }
void mac_msg_setlabel()	{ DOPANIC("mac_msg_setlabel stub"); }
void mac_sem_init()	{ DOPANIC("mac_sem_init stub"); }
void mac_sem_setlabel()	{ DOPANIC("mac_sem_setlabel stub"); }
void mac_shm_init()	{ DOPANIC("mac_shm_init stub"); }
void mac_shm_setlabel() { DOPANIC("mac_shm_setlabel stub"); }
