#include <sys/types.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/callo.h>
#include <sys/timers.h>
#include <sys/kmem.h>
#include <sys/iobus.h>
#include <sys/SN/agent.h>
#include <sys/stream.h>
#include <sys/groupintr.h>
#include <sys/nodemask.h>
#include <sys/immu.h>
#include <sys/SN/slotnum.h>
#if defined (SN1)
#include "../os/numa/memfit.h"
#endif



/* Miscellaneous architecture-related routines */
void add_ioboard(void) { }
void set_pwr_fail(void) { }

/* I/O subsystem arch-specific stuff */
/* ARGSUSED */
int readadapters(uint x) { return 0; }

struct kmem_ioaddr kmem_ioaddr[] = {
        { 0, 0 },
};

/* Frame Scheduler binary compatibility VME routine stubs */
/*ARGSUSED*/
int vme_frs_install(int x, int y, intrgroup_t *z) { return 0; }
/*ARGSUSED*/
int vme_frs_uninstall(int x, int y, int z) { return 0; }

#if SABLE
void speedup_counter(void) { }
#endif

#if defined (PSEUDO_SN1)
extern print_trap(char *);
#if 1

#define FIXME(s) printf("%s at %s:%d\n", s, __FILE__, __LINE__)

#define FIXME1(s) print_trap(s)
#else

#define FIXME(s) print_trap(s);
#endif
/*ARGSUSED */
int is_fine_dirmode() 		{FIXME1("Fixme: is_fine_dirmode"); return 0;  }
/* ARGSUSED */
void flush_tlb(int a) 			{FIXME1("Fixme: flush_tlb");  }
/* ARGSUSED */
void flush_cache(void)			{FIXME1("Fixme: flush_cache"); }
/* ARGSUSED */
int get_dir_ent()			{FIXME("Fixme: get_dir_ent"); return 0;  }
/* ARGSUSED */
void aspm_print(void)			{FIXME("Fixme: aspm_print");}
/* ARGSUSED */
int intr(struct eframe_s *x, uint y, uint z, uint a)	{FIXME("Fixme: intr"); return 0;}
/* ARGSUSED */
void uli_setasids(void)			{FIXME("Fixme: uli_setasids");}
/* ARGSUSED */
void setrtvector(void)			{FIXME("Fixme: setrtvector");}
/* ARGSUSED */
void setkgvector(void (*x)())		{FIXME("Fixme: setkgvector");}
/* ARGSUSED */
void nvram_baseinit(void)		{FIXME1("Fixme: nvram_baseinit");}
/* ARGSUSED */
void hub_widget_id(void)		{FIXME("Fixme: hub_widget_id");}
/* ARGSUSED */
void router_map_init(void)		{FIXME("Fixme: router_map_init");}
/* ARGSUSED */
void intr_init(void)			{FIXME("Fixme: intr_init");}
/* ARGSUSED */
void install_cpuintr(void)		{FIXME("Fixme: install_cpuintr");}
/* ARGSUSED */
void install_dbgintr(void)		{FIXME("Fixme: install_dbgintr");}
/* ARGSUSED */
void install_tlbintr(void)		{FIXME("Fixme: install_tlbintr");}
/* ARGSUSED */
void get_region(void)			{FIXME1("Fixme: get_region");}
/* ARGSUSED */
void sysctlr_power_off(void)		{FIXME("Fixme:  ");}
/* ARGSUSED */
void ni_reset_port(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void set_dir_state(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void memory_set_access(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void get_slotname(slotid_t x, char *y)	{FIXME("Fixme: ");}
/* ARGSUSED */
void ULIsystrap(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void intr_clear_all(void)		{FIXME("Fixme: intr_clear_all");}
/* ARGSUSED */
void ecc_init(void)			{FIXME("Fixme: ecc_init");}
/* ARGSUSED */
uint getcachesz(cpuid_t x)		{FIXME("Fixme: "); return 0;}
/* ARGSUSED */
void reset_leds(void)			{FIXME1("Fixme: reset_leds");}
/* ARGSUSED */
void probe_routers(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void sn0_error_dump(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
slotid_t hub_slot_to_crossbow(slotid_t x)	{FIXME("Fixme: hub_slot_to_xbow"); return 0;}
/* ARGSUSED */
void router_guardians_set(void)		{FIXME("Fixme: router_guardians_set ");}
/* ARGSUSED */
void elsc_init(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void keruliframe(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void idle_err(inst_t *x, uint a, void *b, void *c)	{FIXME("Fixme: ");}
/* ARGSUSED */
void intr_reserve_level(void)		{FIXME("Fixme: intr_reserve_level");}
/* ARGSUSED */
void intr_connect_level(void)		{FIXME("Fixme: intr_connect_level");}
/* ARGSUSED */
void intr_disconnect_level(void)	{FIXME("Fixme: ");}
/* ARGSUSED */
void intr_unreserve_level(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void idbg_pfms(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void nodepda_mcdprint(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void nodepda_rcdprint(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void debug_stop_all_cpus(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void kl_log_hw_state(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void kl_error_show_idbg(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void router_init(void)		{FIXME("Fixme: router_init");}
/* ARGSUSED */
slotid_t get_node_crossbow(nasid_t a) {FIXME("Fixme: get_node_xbow");return 0;}
/* ARGSUSED */
slotid_t get_widget_slotnum(int a, int b) {FIXME("Fixme: get_widget_slotnum");return 0;}
/* ARGSUSED */
slotid_t get_node_slotid(nasid_t a)  {FIXME("Fixme: get_node_slotid");return 0;}
/* ARGSUSED */
cpuid_t intr_heuristic(vertex_hdl_t dev, device_desc_t c)  {FIXME("Fixme: intr_heuristic");return 0;}
/* ARGSUSED */
slotid_t get_my_slotid(void)  {FIXME("Fixme: get_node_slotid");return 0;}
/* ARGSUSED */
void get_widget_slotname(int a, int b, char *c) {FIXME("Fixme: get_widget_slotname");}
/* ARGSUSED */
int hub_error_devenable(vertex_hdl_t a, int b, int c)	{FIXME("Fixme: "); return 0;}
/* ARGSUSED */
void hub_widget_reset(vertex_hdl_t a, xwidgetnum_t b)		{FIXME("Fixme: ");}
/* ARGSUSED */
int sendintr(cpuid_t x, unchar y)			{FIXME("Fixme: ");return 0;}
/* ARGSUSED */
void set_leds(int x)			{FIXME("Fixme: set_leds");}
/* ARGSUSED */
int dobuserre(struct eframe_s *a, inst_t *b, uint c)	
/* ARGSUSED */
{FIXME("Fixme: "); return 0;}
/* ARGSUSED */
void checkfp(void)			{FIXME("Fixme: checkfp");}
/* ARGSUSED */
int earlynofault(struct eframe_s *a, uint b)	{FIXME("Fixme: "); return 0;}
/* ARGSUSED */
uint tlbdropin(unsigned char *a, caddr_t b, pte_t c)			{FIXME("Fixme: ");return 0;}
/* ARGSUSED */
void bump_leds(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void invaltlb(int a)			{FIXME("Fixme: invaltlb");}
/* ARGSUSED */
void set_tlbpid(unsigned char x)			{FIXME("Fixme: ");}
/* ARGSUSED */
#ifdef DEBUG
#define unmaptlb _unmaptlb
#endif
void unmaptlb(unsigned char *x, __psunsigned_t b)	{FIXME("Fixme: unmaptlb");}
/* ARGSUSED */
void cpu_waitforreset(void)		{FIXME("Fixme: cpu_waitforreset");}
/* ARGSUSED */
void clean_icache(void *a, unsigned int b, unsigned int c, unsigned int d){FIXME("Fixme: clean_icache");}
/* ARGSUSED */
int sync_dcaches(void *a, unsigned int b, pfn_t c, unsigned int d){FIXME("Fixme: sync_dcaches"); return 0;}
/* ARGSUSED */
void clean_dcache(void *a, unsigned int b, unsigned int c, unsigned int d){FIXME("Fixme: clean_dcache ");}
/* ARGSUSED */
void uli_getasids(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void uli_eret(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void enable_ithreads(void)		{FIXME("Fixme: enable_ithreads");}
/* ARGSUSED */
void tlbdropin_lpgs(unsigned char a, caddr_t b, pte_t c, pte_t d, uint e)		{FIXME("Fixme: tlbdropin_lpgs");}
/* ARGSUSED */
void set_nvram(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void sys_setled(void)			{FIXME("Fixme: ");}
/* ARGSUSED */
void migr_periodic_bounce_control(void)	{FIXME("Fixme: ");}
/* ARGSUSED */
void migr_periodic_unpegging_control(void)	{FIXME("Fixme: ");}
/* ARGSUSED */
void migr_pageref_disable(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void migr_pageref_reset(void)		{FIXME("Fixme: ");}
/* ARGSUSED */
void migr_pageref_reset_abs(void)	{FIXME("Fixme: ");}
/* ARGSUSED */
unsigned char tlbgetpid(void)			{FIXME("Fixme: tlbgetpid "); return 0;}
/* ARGSUSED */
__psint_t tlb_probe(unsigned char a, caddr_t b, uint *c) {FIXME("Fixme: tlb_probe "); return 0;}
/* ARGSUSED */
void utlbmiss1(void)			{FIXME("Fixme: utlbmiss1 ");}
/* ARGSUSED */
void eutlbmiss1(void)			{FIXME("Fixme: eutlbmiss1");}
/* ARGSUSED */
void utlbmiss2(void)			{FIXME("Fixme: utlbmiss2");}
/* ARGSUSED */
void eutlbmiss2(void)			{FIXME("Fixme: eutlbmiss2");}
/* ARGSUSED */
void utlbmiss3(void)			{FIXME("Fixme: utlbmiss3");}
/* ARGSUSED */
void eutlbmiss3(void)			{FIXME("Fixme: eutlbmiss3");}
/* ARGSUSED */
int migr_init(void)			{FIXME("Fixme: migr_init"); return 0;}
/* ARGSUSED */
int mem_profiler_init(void)		{FIXME("Fixme: mem_profiler_init"); return 0;}
/* ARGSUSED */
void nmi_dump(void)			{FIXME("Fixme: nmi_dump");}
void idbg_mldprint(void)			{FIXME("Fixme: ");}
void local_neighborhood_traffic_byroute(void)	{FIXME("Fixme: ");}
void remote_neighborhood_traffic_byroute(void)	{FIXME("Fixme: ");}
#endif /* PSEUDO_SN1 */


