#include "parse.h"

typedef __uint64_t	i64;

void exec_setgpr(parse_data_t *pd);
void exec_getgpr(parse_data_t *pd);
void exec_setcop0(parse_data_t *pd, i64 which_reg);
void exec_getcop0(parse_data_t *pd, i64 which_reg);
void exec_delay(parse_data_t *pd);
void exec_sleep(parse_data_t *pd);
void exec_echo(parse_data_t *pd, i64 nargs);
void exec_modt(parse_data_t *pd, i64 topbyte);
void exec_modn(parse_data_t *pd);
void exec_modw(parse_data_t *pd);
void exec_modb(parse_data_t *pd);
void exec_modd(parse_data_t *pd, i64 type);
void exec_arith(parse_data_t *pd, i64 op);
void exec_print(parse_data_t *pd, i64 use_hex);
void exec_printreg(parse_data_t *pd, i64 nargs);
void exec_printregall(parse_data_t *pd);
void exec_printcop0(parse_data_t *pd, i64 which_reg);
void exec_printcop1(parse_data_t *pd, i64 nargs);
void exec_setcop1(parse_data_t *pd, i64 reg_or_stk);
void exec_getcop1(parse_data_t *pd);
void exec_printaddr(parse_data_t *pd, i64 nargs);
void exec_load(parse_data_t *pd, i64 opts);
void exec_store(parse_data_t *pd, i64 opts);
void exec_sdv(parse_data_t *pd);
void exec_jump(parse_data_t *pd, i64 nargs);
void exec_call(parse_data_t *pd, i64 nargs);
void exec_help(parse_data_t *pd, i64 nargs);

/* Print Hub reg */
void exec_phreg_value(parse_data_t *pd, hwreg_t *hwreg);
void exec_phreg_defl(parse_data_t *pd, hwreg_t *hwreg);
void exec_phreg_actual(parse_data_t *pd, hwreg_t *hwreg);
void exec_phreg_remote(parse_data_t *pd, hwreg_t *hwreg);

/* Print Router reg */
void exec_prreg_value(parse_data_t *pd, hwreg_t *hwreg);
void exec_prreg_defl(parse_data_t *pd, hwreg_t *hwreg);

/* Print XBOW reg */
void exec_pxreg_value(parse_data_t *pd, hwreg_t *hwreg);
void exec_pxreg_defl(parse_data_t *pd, hwreg_t *hwreg);
void exec_pxreg_actual(parse_data_t *pd, hwreg_t *hwreg);
void exec_pxreg_remote(parse_data_t *pd, hwreg_t *hwreg);

void exec_reset(parse_data_t *pd);
void exec_softreset(parse_data_t *pd);
void exec_nmi(parse_data_t *pd, i64 nargs);
void exec_resume(parse_data_t *pd);
void exec_inval(parse_data_t *pd, i64 nargs);
void exec_flush(parse_data_t *pd);
void exec_tlbc(parse_data_t *pd, i64 nargs);
void exec_tlbr(parse_data_t *pd, i64 nargs);
void exec_dtag(parse_data_t *pd, i64 nargs);
void exec_itag(parse_data_t *pd, i64 nargs);
void exec_stag(parse_data_t *pd, i64 nargs);
void exec_dline(parse_data_t *pd);
void exec_iline(parse_data_t *pd);
void exec_sline(parse_data_t *pd);
void exec_adtag(parse_data_t *pd);
void exec_aitag(parse_data_t *pd);
void exec_astag(parse_data_t *pd);
void exec_adline(parse_data_t *pd);
void exec_ailine(parse_data_t *pd);
void exec_asline(parse_data_t *pd);
void exec_sscache(parse_data_t *pd);
void exec_sstag(parse_data_t *pd, i64 nargs);
void exec_go(parse_data_t *pd);
void exec_hubsde();
void exec_rtrsde();
void exec_chklink();
void exec_bist(parse_data_t *pd, i64 opt);
void exec_enable(parse_data_t *pd, i64 opt);
void exec_dirinit(parse_data_t *pd);
void exec_meminit(parse_data_t *pd);
void exec_dirtest(parse_data_t *pd);
void exec_memtest(parse_data_t *pd);
void exec_santest(parse_data_t *pd);
void exec_slave(parse_data_t *pd);
void exec_segs(parse_data_t *pd, i64 nargs);
void exec_exec(parse_data_t *pd, i64 nargs);
void exec_why(parse_data_t *pd);
void exec_btrace(parse_data_t *pd, i64 nargs);
void exec_reconf(parse_data_t *pd);
void exec_clear(parse_data_t *pd);
void exec_error(parse_data_t *pd);
void exec_ioc3(parse_data_t *pd);
void exec_junk(parse_data_t *pd);
void exec_elsc(parse_data_t *pd);
void exec_nm(parse_data_t *pd);
void exec_vecr(parse_data_t *pd);
void exec_vecw(parse_data_t *pd);
void exec_vecx(parse_data_t *pd);
void exec_disc(parse_data_t *pd);
void exec_pcfg(parse_data_t *pd, i64 nargs);
void exec_node(parse_data_t *pd, i64 nargs);
void exec_crb(parse_data_t *pd, i64 nargs);
void exec_crbx(parse_data_t *pd, i64 nargs);
void exec_route(parse_data_t *pd, i64 nargs);
void exec_flash(parse_data_t *pd, i64 op);
void exec_nic(parse_data_t *pd, i64 nargs);
void exec_rnic(parse_data_t *pd, i64 nargs);
void exec_cfg(parse_data_t *pd, i64 nargs);
void exec_sc(parse_data_t *pd, i64 nargs);
void exec_scw(parse_data_t *pd, i64 nargs);
void exec_scr(parse_data_t *pd, i64 nargs);
void exec_dips(parse_data_t *pd);
void exec_dbg(parse_data_t *pd, i64 nargs);
void exec_pas(parse_data_t *pd, i64 nargs);
void exec_module(parse_data_t *pd, i64 nargs);
void exec_partition(parse_data_t *pd, i64 nargs);
void exec_modnic(parse_data_t *pd);
void exec_qual(parse_data_t *pd, i64 nargs);
void exec_ecc(parse_data_t *pd, i64 nargs);
void exec_cpu(parse_data_t *pd, i64 nargs);
void exec_dis(parse_data_t *pd, i64 nargs);
void exec_dmc(parse_data_t *pd, i64 nargs);
void exec_im(parse_data_t *pd, i64 nargs);
void exec_error_dump(parse_data_t *pd);
void exec_edump_bri(parse_data_t *pd, i64 nargs);
void exec_maxerr(parse_data_t *pd, i64 nargs);
void exec_dumpspool(parse_data_t *pd, i64 nargs);
void exec_reset_dump(parse_data_t *pd);
void exec_rtab(parse_data_t *pd, i64 nargs);
void exec_rstat(parse_data_t *pd);
void exec_credits(parse_data_t *pd);
void exec_version(parse_data_t *pd);
void exec_memop(parse_data_t *pd, i64 op);
void exec_scandir(parse_data_t *pd, i64 nargs);
void exec_dirstate(parse_data_t *pd, i64 nargs);
void exec_verbose(parse_data_t *pd, i64 nargs);
void exec_altregs(parse_data_t *pd, i64 nargs);
void exec_kdebug(parse_data_t *pd, i64 nargs);
void exec_kern_sym(parse_data_t *pd);
void exec_initlog(parse_data_t *pd, i64 op);
void exec_initalllogs(parse_data_t *pd, i64 op);
void exec_setenv(parse_data_t *pd, i64 op);
void exec_unsetenv(parse_data_t *pd, i64 op);
void exec_printenv(parse_data_t *pd, i64 op);
void exec_log(parse_data_t *pd, i64 op);
void exec_fru(parse_data_t *pd, i64 op);
void exec_setpart(parse_data_t *pd);

/* command line parser entry points for diagnostic routines */

#define DIAG_PCI	4
#define DIAG_IO6	5
#define DIAG_BRI	6
#define SRL_DMA		7
#define SRL_PIO		8

void exec_xbowtest(parse_data_t *pd, i64 nargs);
void exec_Diag(parse_data_t *pd, i64 nargs);
