/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: libkl.h
 *	prototypes and definitions for libkl.
 */

#ifndef _LIBKL_H
#define _LIBKL_H

#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>

#include <setjmp.h>

#define NIC_UNKNOWN ((nic_t) -1)

lboard_t 	*init_klcfg_ip27(__psunsigned_t, int flag);
lboard_t 	*init_klcfg_midplane(__psunsigned_t, lboard_t *) ;
lboard_t 	*init_klcfg_baseio(__psunsigned_t) ;
lboard_t 	*init_klcfg_4chscsi(__psunsigned_t) ;
lboard_t 	*init_klcfg_graphics(__psunsigned_t) ;
lboard_t 	*init_klcfg_xthd(__psunsigned_t) ;
lboard_t 	*init_klcfg_menet(__psunsigned_t) ;
lboard_t 	*init_klcfg_brd(__psunsigned_t, int) ;
lboard_t 	*init_klcfg_routerb(pcfg_t *, nasid_t, pcfg_router_t *, int) ;
lboard_t 	*init_klcfg_tpu(__psunsigned_t) ;
lboard_t 	*init_klcfg_gsn(__psunsigned_t, int) ;

void 		init_klcfg_cpu(__psunsigned_t, lboard_t *, int, int) ;
void 		init_klcfg_hub(__psunsigned_t, lboard_t *, int) ;
klinfo_t 	*init_klcfg_ioc3(__psunsigned_t, lboard_t *, int) ;
void 		init_klcfg_qscsi(__psunsigned_t, lboard_t *, int) ;

#ifdef SABLE
void 		init_klcfg_sscsi(__psunsigned_t, lboard_t *, int) ;
#endif

klbri_t 	*init_klcfg_bridge(__psunsigned_t, lboard_t *, int) ;
void 		init_klcfg_xbow(__psunsigned_t, lboard_t *) ;
void 		init_klcfg_uvme(__psunsigned_t, lboard_t *, int) ;
void 		init_klcfg_membnk(__psunsigned_t, lboard_t *, int) ;
klinfo_t	*init_klcfg_compt(__psunsigned_t, lboard_t *, int, int, int) ;

extern void init_klcfg_routerc(pcfg_router_t *, lboard_t *, int, int) ;
extern klscdev_t * init_klcfg_scsi_drive(klscsi_t *, int) ;
extern klttydev_t *init_klcfg_tty(klioc3_t *);
extern klenetdev_t *init_klcfg_enet(klioc3_t *);
extern klkbddev_t *init_klcfg_kbd(klioc3_t *, int);
extern klmsdev_t *init_klcfg_ms(klioc3_t *);

extern int 	klconfig_discovery_update(pcfg_t *, partid_t);
extern void 	microsec_delay(int);
extern int	meta_in_partition(pcfg_t *, pcfg_router_t *, partid_t, __uint64_t *);
extern void	kl_disable_board(lboard_t *);
extern int	klconfig_failure_update(pcfg_t *, partid_t);
extern void	klcfg_hubii_stat(nasid_t);

extern void	init_klcfg(nasid_t);
extern int	init_klcfg_hw(nasid_t);

extern __uint64_t translate_hub_mcreg(__uint64_t);
extern void swap_memory_banks(nasid_t, int);
extern void unswap_some_memory_banks(void);
extern void swap_some_memory_banks(void);

extern int	get_local_console(console_t *, int, int, int, int);
extern nasid_t	get_nasid(void);
extern int	get_next_console(pcfg_t *, moduleid_t *, int *, nasid_t *);

extern int kl_sable;

int kl_bridge_type(char *, char *, char *);
char *parse_bridge_nic_info(char *, char *, char *) ;
char *get_laser_from_nic(char *, int , char *) ;

extern __psunsigned_t get_ioc3_base(nasid_t);
extern __psunsigned_t	kl_ioc3nvram_base(nasid_t);
extern __psunsigned_t	kl_ioc3uart_base(nasid_t);
extern int check_nvram(__psunsigned_t, nic_t);
extern int add_router_config(pcfg_t *, partid_t);
extern void add_klcfg_xbow_port(lboard_t *, int, int, lboard_t *);
extern int io_discover(lboard_t *, int);
extern void discover_baseio_console(volatile __psunsigned_t, int,
			console_t *, console_t *, int, nic_t);

extern int hubii_init(__psunsigned_t);
extern void hubii_llp_enable(nasid_t);
extern int hubii_llp_is_enabled(nasid_t);
extern void hubii_warm_reset(nasid_t);
extern int hubii_link_up(nasid_t);
extern void hubii_wcr_widset(nasid_t, int);
extern void hubii_setup_bigwin(nasid_t, int, int);
extern int hubii_widget_is_xbow(nasid_t);

extern nasid_t	get_nasid(void);
extern int get_pcilink_status(__psunsigned_t, int, int, __uint32_t *, int);
extern void set_console_node(console_t *, nasid_t);

extern void kl_error_show_log(char *, char *);
extern void kl_error_show_dump(char *, char *);
extern void kl_save_hw_state(void);

extern void dump_error_spool(nasid_t nasid, int slice,
			     int (*prf) (const char *, ...));
extern void crbx(nasid_t nasid, int (*prf)(const char *, ...));
extern int xtalk_init(nasid_t, int, int);
extern void ioc3_init(__psunsigned_t, int, int, int);
extern void xbow_init(__psunsigned_t, int);
extern void xbow_link_init(__psunsigned_t, int);
extern void xbow_cleanup_init(__psunsigned_t, int);
extern nic_t xbow_get_nic(__psunsigned_t) ;

extern void xbow_link_credits(__psunsigned_t, int, int);
extern void xbow_set_link_credits(__psunsigned_t, int, int);
extern void bridge_init(__psunsigned_t, int);
extern int xbow_get_active_link(__psunsigned_t, int);
extern int xbow_check_master_hub(__psunsigned_t, int);
extern int xbow_get_master_hub(__psunsigned_t);
extern int xbow_check_link_hub(__psunsigned_t, int);
extern int xbow_check_hub_net(__psunsigned_t, int);
extern nasid_t xbow_get_link_nasid(__psunsigned_t, int);
extern void xbow_reset_arb_register(__psunsigned_t, int) ;
extern void xbow_reset_arb_nasid(nasid_t) ;
extern int peer_hub_on_xbow(nasid_t);

extern int io_get_widgetid(nasid_t, int);
extern void klconfig_nasid_update(nasid_t, int);

extern int hub_widget_id(nasid_t);
extern int hub_xbow_link(nasid_t);
extern void kl_init_pci(__psunsigned_t, int, int, int, int);
extern void kl_zero_crbs(nasid_t nasid);
extern void kl_hubcrb_cleanup(nasid_t nasid);

extern char *net_errmsg(int rc);
extern void rstat(net_vec_t);
extern net_vec_t klcfg_discover_route(lboard_t *, lboard_t *, int);

extern nasid_t xbow_peer_hub_nasid(klxbow_t *, nasid_t, int);
extern int xbow_link_status(volatile __psunsigned_t, int);
extern int xbow_init_arbitrate(nasid_t);

extern void _cpu_get_nvram_buf(__psunsigned_t , int , int , char *) ;
extern int check_console_path(char *) ;
extern int get_nvram_modid(__psunsigned_t , nic_t ) ;

extern char get_scsi_war_value(void);
extern void set_scsi_war_value(char);

int update_old_ioprom(nasid_t);
void update_brd_module(lboard_t *brd_ptr) ;

int ed_cpu_mem(nasid_t , char *, char *, char *, int, int);
void pcfg_syslog_info(pcfg_t *p) ;

off_t 		kl_config_info_alloc(nasid_t, int, unsigned int);
void 		init_config_malloc(nasid_t);
lboard_t 	*kl_config_alloc_board(nasid_t);
klinfo_t 	*kl_config_alloc_component(nasid_t, lboard_t *);
lboard_t 	*kl_config_add_board(nasid_t, lboard_t *);

klconf_off_t 	klmalloc_nic_off(nasid_t, int) ;
klconf_off_t 	get_klcfg_router_nicoff(nasid_t, int, int) ;
void        	init_klmalloc_device(nasid_t) ;

void sn00_copy_mod_snum(__psunsigned_t, int) ;

int	setfault(jmp_buf, void **);
int	ignorefault(void **);
void	restorefault(void **);

lboard_t *get_board_name(nasid_t, moduleid_t, slotid_t, char*);
void get_module_slot(__psunsigned_t, moduleid_t *, int *);
short get_nvram_dbaud(volatile __psunsigned_t ) ;
__psunsigned_t getnv_base_lb(lboard_t *) ;
void make_hwg_path(moduleid_t, slotid_t, char *) ;

int	ip31_pimm_psc(nasid_t, char *, char *);

void	dump_r10k_mode(__uint64_t, int);

char 	*get_diag_string(unsigned int);

unsigned short hled_get_kldiag(char);
int hled_tdisable(char);

int	diag_get_mode(void);
void	diag_set_mode(int);

void	diag_set_lladdr(int);
int	diag_get_lladdr(void);

char 	diag_get_reboot(void) ;

void	diag_set_tlbextctxt(int);
int	diag_get_tlbextctxt(void);

void	diag_print_exception(void);

int	db_printf(const char *fmt, ...);           /* If verbose on */

#if defined (HUB_ERR_STS_WAR)
extern int do_hub_errsts_war(void);
#endif
#endif /* _LIBKL_H */
