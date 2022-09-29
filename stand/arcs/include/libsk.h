/*
 * libsk.h - Prototypes and definitions for the standalone kernel library
 *
 *
 * $Revision: 1.128 $
 */

#ifndef _LIBSK_H
#define _LIBSK_H

#include <sys/types.h>
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <setjmp.h>
#include <arcs/signal.h>
#include <arcs/io.h>
#include <elf.h>
#ifdef SN
#include <sys/SN/klconfig.h>
#endif

struct arpcom;
struct bpb;
struct buf;
struct cfgnode_s;
struct component;
struct ct_g0inq_data_s;
struct device_buf;
struct dksoftc;
struct dmamap;
struct eiob;
struct fileinformation;
struct fsblock;
struct ifnet;
struct ioblock;
struct iobuf;
struct large;
struct memorydescriptor;
struct openinfo;
struct scsidev;
struct scsi_request;
struct scsisubchan;
struct sockaddr;
struct sockaddr_in;
struct string_list;
struct systemid;
struct tp_dir;
struct tp_entry;
struct tpsc_types;
struct timeinfo;
struct wd95request;
struct htp_state;
struct in_addr;
struct volume_header;

/* macro to see if buffer is dma aligned */
#define IS_DMAALIGNED(x) (((__psunsigned_t)(x)&3)==0)

/* Driver helper routines */
int copyin(void *src, void *dst, int len);
int copyout(void *src,void *dst, int len);
int suser(void);
void clean_icache(void *, unsigned int);
void clean_dcache(void *, unsigned int);
void clear_cache(void *, unsigned int);
void wvflush(void *, unsigned int);
void kern_free(void *);
void *kern_malloc(size_t);
void flushbus(void);

#if defined(ELF_2_COFF) || defined(LIBSL)
/*
 * ELF_2_COFF doesn't link with libsk, but it uses libsk files.  Have to
 * be careful of it.   It isn't needed on NUMA machines anyway.
 */
#define kdmtolocalphys(x)	(paddr_t)KDM_TO_PHYS(x)
#define kdmtolocal(x)		(paddr_t)(x)
#else
paddr_t kdmtolocalphys(void *);
paddr_t kdmtolocal(void *);
#endif

paddr_t kvtophys(void *);
nasid_t get_nasid(void);
void _errputs(char *);
void _errputc(char);

void cpu_show_fault(unsigned long);
void cpu_clearnofault(void);
int slave_loop(void);

#if SN0

/* Number of chans for which dksc(X will work */

#define 	NUM_DKSC_CHAN   2

unchar 		get_nvreg(uint);
int 		set_nvreg(uint, unchar);
char 		nvchecksum(void);
void		init_module_table(void);
int		init_prom_graph(void);
void		pgInit(void);
nasid_t		get_nasid(void);
klinfo_t 	*kl_find_type(CONFIGTYPE, void *) ;
int 		kl_count_type(CONFIGTYPE) ;
void 		munge(u_char *, int) ;
int 		kl_parse_path(char *, struct eiob *);
int 		sn_parse_path(char *, struct eiob*) ;
void 		kl_showlist(void *, CONFIGTYPE , void *, int);
char 		*kl_inv_find(void);
int 		kl_hinv(int, char **);
int 		kl_get_num_cpus(void);
int 		sn0_getcpuid(void);
void 		sn0_dump_diag(int);
lboard_t 	*find_gfxpipe(int);
void 		bsort_lboard_modslot(unsigned , int (*)(), int (*)());
int 		int_comp(lboard_t *, lboard_t *);
int 		lbp_swap(unsigned, unsigned);
int 		visit_lboard(int (*)(), void *, void *) ;
int 		dismem_size_k(klinfo_t *, char *) ;
int 		cpu_get_dismem(void) ;
void 		*pg_get_lbl(vertex_hdl_t, char *) ;
int 		pg_add_lbl(vertex_hdl_t, char *, void *) ;
int 		pg_add_nic_info(klinfo_t *, vertex_hdl_t) ;
void 		pg_edge_add_path(char *, vertex_hdl_t, char *) ;
void 		decode_str_serial(const char *, char *);
char 		*make_serial(int) ;
void 		kl_graphics_install(void) ;
void 		cpu_fix_dbgtty(void);
void 		update_checksum(void);
lboard_t	*get_match_lb(lboard_t *, int) ;
nic_t           get_sys_nic(void) ;
nic_t		check_sys_nic(nic_t) ;
int  		cpu_get_max_nodes(void) ;
unchar 		slv_get_nvreg(__psunsigned_t, uint) ;
int 		slv_set_nvreg(__psunsigned_t, uint, unchar) ;
void 		cpu_set_sav_cons_path(char *) ;
char		get_scsi_to_ll_value(void) ;
void		set_scsi_to_ll_value(char) ;
void		read_extra_nic(void) ;
void		do_dis_node_brds(void) ;
char		*get_nicstr_from_devinfo(lboard_t *) ;
char* 		find_board_name(lboard_t *l);
#endif

#if !SN0 && !IP30
int launch_slave(int, void(*)(int), int, void (*)(int), int, void *);
#endif

#if TFP
extern machreg_t	get_tlbhi(int, int);
extern machreg_t	get_tlblo(int, int);
extern machreg_t	get_cp0_tlbhi(void);
extern machreg_t	get_cp0_tlblo(void);
extern machreg_t	get_cp0_tlbset(void);
extern machreg_t	get_cp0_badvaddr(void);
extern machreg_t	cp0_tlb_probe(void);
extern void		cp0_tlb_write(void);
extern void		cp0_tlb_read(void);
extern void		set_cp0_badvaddr(machreg_t);
extern void		set_cp0_tlbhi(machreg_t);
extern void		set_cp0_tlblo(machreg_t);
extern void		set_cp0_tlbset(machreg_t);
extern void write_vcr(machreg_t);
extern machreg_t read_revision(void);
#else
extern machreg_t	get_tlbhi(int);
extern machreg_t	get_tlblo0(int);
#endif	/* TFP */
extern machreg_t	get_tlbpid(void);
extern long		probe_tlb(char *, unsigned int);
extern void		invaltlb(int);
extern void		tlbpurge(void);


#if R4000 || R10000
machreg_t get_tlblo1(int);
int get_pgmaski(int);
int get_pgmask(void);
void set_pgmask(int);
#endif

/*
 * protos for all libsk public functions
 */

/* lib/genpda.c */
void dump_gpda(int);
void setup_gpdas(void);

/* lib/alarm.c */
void _init_alarm(void);
unsigned int alarm(unsigned);
void check_alarm(void);

/* lib/config.c */
void init_cfgtree(void);
LONG RegisterDriverStrategy(struct component *, STATUS (*)(struct component *, struct ioblock *));

/* lib/errputs.c */
void _errputs(char *);
void _errputc(char);

/* lib/fault.c */
void _exception_handler(void);
void show_fault(void);
void mp_show_fault(int);
void dump_saved_uregs(int);

/* lib/hook.c */
void _save_exceptions(void);
void _restore_exceptions(void);
void _hook_exceptions(void);
void _default_exceptions(void);
inst_t _inst_vec(__psunsigned_t);

/* lib/loader.c */
void load_init (void);
int elf_list_init(ULONG, ULONG);
void load_error(char *filename, char *fmt, ...);

/* lib/loadelf.c */
LONG loadelf32(char *, ULONG, ULONG *, Elf32_Ehdr *, __psunsigned_t);
LONG loadelf64(char *, ULONG, ULONG *, Elf64_Ehdr *, __psunsigned_t);
void set_loadelfdynamic32(LONG(*func)(char*, ULONG, ULONG*, Elf32_Ehdr*, Elf32_Phdr*));
void set_loadelfdynamic64(LONG(*func)(char*, ULONG, ULONG*, Elf64_Ehdr*, Elf64_Phdr*));
#define ELFHDRBYTES 5

/* lib/parse.c */
char *parseargs(char *, int, __psint_t *);
int parse_path(char *, struct eiob *);

/* lib/arcsio.c */
void _init_systemid(void);
#ifdef NETDBX
LONG Bind(ULONG, u_short);
LONG GetSource(ULONG, struct sockaddr_in *);
LONG BindSource(ULONG, struct sockaddr_in);
#endif /* NETDBX */

/* lib/saio.c */
void _init_saio(void);
void init_consenv(char *);
char *_get_iobbuf(void);
void _free_iobbuf(char *);
void _scandevs(void);
int console_is_gfx(void);
void vflush(void *, unsigned);
void _init_bootenv(void);
void md_init(void);

/* lib/fault.c */
void clear_nofault(void);
void set_nofault(jmp_buf);

/* lib/setpage.c */
enum pagesize {
	_page4K,
	_page8K,	/* only valid on TFP */
	_page16K,
	_page64K,
	_page256K,	/* only valid on r4k */
	_page1M,
	_page4M,
	_page16M
};

k_machreg_t set_pagesize(enum pagesize);
void	restore_pagesize(k_machreg_t);

/* lib/skutils.c */
void delay(long);
int setup_idarr(int, int *, int *);

/* lib/spb.c */
void _init_spb(void);

/* lib/tty.c */
void _ttyinput(struct device_buf *, char);
void _intr_init(void);
int _circ_getc(struct device_buf *);
void _circ_putc(int, struct device_buf *);
int _circ_nread(struct device_buf *);

/* lib/tlb.s */
#ifndef TFP
#ifdef PTE_64BIT
extern void tlbwired(int, unsigned char, caddr_t, __uint64_t, __uint64_t);
#else
extern void tlbwired(int, unsigned char, caddr_t, __uint32_t, __uint32_t);
#endif /* PRE_64BIT */
#endif /* ~TFP */

/* lib/memops.c */
void set_memory(void *, int, long);
int fill(int, char **);

/* lib/range_check.c */
int range_check(__psunsigned_t, unsigned);

/* lib/membig.c */
void mem_biggest_free(__psint_t *, unsigned int *);
void *mem_free_chunk(void *, __psint_t *, unsigned int *);

/* lib/clientasm.s */
void client_start(int, char **, char **, ULONG, ULONG);

/* graphics/gfx.c */
int _init_htp(void);
int txPrint(char *, int);
int _gfx_strat(struct component *, struct ioblock *);

/* graphics/htport.c */
void txMap(struct htp_state *);
void txConfig(struct htp_state *, int);
void initFonts(void);
void htp_init(void);
int htp_write(char *, int);
int getHtpPs(void);
void txSetJumpbuf(jmp_buf *);
void txSetString(char *);
struct htp_state *gethtp(void);

/* nvram interface */
void cpu_get_nvram_buf(int, int, char []);
int cpu_set_nvram_offset(int, int, char *);
int cpu_set_nvram(char *, char *);
int cpu_is_nvvalid(void);
void cpu_set_nvvalid(void(*)(char *,struct string_list *),struct string_list *);
void cpu_nv_lock(int);
int nvram_is_protected(void);
void cpu_get_nvram_persist(int(*)(char *,char *, struct string_list *),
			   struct string_list *);
void cpu_set_nvram_persist(char *, char *);
void cpu_nvram_init(void);

/* ml/IP20.c */
void ip20_set_sickled(int);
void ip20_set_cpuled(int);
int ip20_board_rev(void);

/* ml/IP22.c */
void kbdbell_init(void);
void ip22_set_cpuled(int);
void ip22_set_sickled(int);
int ip22_board_rev(void);
int ip22_gio_is_33MHZ(void);
void pckm_setupmap(void);

/* misc common cpu_* routines */
void cpu_get_eaddr(u_char []);

/* ml/badaddr.c */
int badaddr(volatile void *, int);
int badaddr_val(volatile void *, int, volatile void *);

/* ml/businfo.c */
#ifdef EVEREST
char *processor_name(int);
char *board_name(unchar);
char *ioa_name(unchar, unchar);
char *evdiag_msg(unchar);
void businfo(LONG);
#else
void businfo(LONG);
#endif

/* ml/ds1286.c */
void cpu_set_tod(struct timeinfo *);
int cpu_restart_rtc(void);
LONG _rtodc(int, int, int);
int get_dayof_week(LONG);
void ip22_power_switch_off(void);
void ip22_power_switch_on(void);

/* ml/freq.c */
char *cpu_get_freq_str(int);

/* ml/hpc.c */
void hpc_set_endianness(void);

/* ml/hpc_scsi.c */
#if IP20 || IP22 || IP26 || IP28
void scsiinit(void);
void scsi_go(struct dmamap *, caddr_t, int);
struct dmamap *scsi_getchanmap(struct dmamap *);
void scsi_freechanmap(struct dmamap *);
int scsidma_flush(struct dmamap *, int, uint);
struct dmamap *dma_mapalloc(int, int, int, int);
int dma_map(struct dmamap *, caddr_t, int);
#endif

/* ml/nvcompat.c */
int cpu_is_oldnvram(void);
void cpu_update_nvram(void);

/* ml/nvram.c */
void init_env(void);
int setenv_nvram(char *, char *);
char *htoe (unsigned char *);
unsigned char *etoh (char *);
int resetenv(void);

/* ml/faultasm.s */
void nested_exception_spin(void);
void exceptnorm(void);
void exceptutlb(void);

/* ml/r?k.c */
int splerr(void);
#if !IP19 && !IP25
long long load_double(long long *);
#endif

#if (_MIPS_SIM == _ABIO32)
void store_double(long long *, long long);
#else
#ifndef store_double	
#define store_double(reg, val)	*(volatile long long *)(reg) = (long long)(val)
#endif
#endif

/* ml/s1chip.c */
#if defined(EVEREST)
struct s1chip;
int s1_find_type(struct s1chip *);
void scsi_init(void);
void scsi_go(struct dmamap *, caddr_t, int);
int s1dma_flush(struct dmamap *);
int scsidma_flush(struct dmamap *, int);
int s1_int_present(int);
int scsi_int_present(struct scsidev *);
void *scsi_getchanmap(void *);
void *scsi_freechanmap(void *);
struct dmamap *dma_mapalloc(int, int, int, int);
int dma_map(struct dmamap *, caddr_t, int);
int s1adap2slot(int);
#endif

/* ml/spinlock.c */
void initsplocks(void);
void initlock(lock_t *);
void initnlock(lock_t *, char *);
/*void freesplock(lock_t *);*/

/* ml/r4kasm.s */
int load_double_lo(long long *);
void store_double_lo(long long *, uint);
unsigned int r4k_getticker(void);
void r4k_setticker(unsigned int);
int r4k_getconfig(void);
int r4k_getprid(void);
int get_cpu_irr(void);
int get_fpu_irr(void);
int get_fpc_irr(void);


/* ml/EVERESTasm.s */
#if SN0 || IP30
void call_coroutine(void (*)(), void *, uint *);
#else
void call_coroutine(void (*)(), int, uint *);
#endif
long spl(long);
void wbflush(void);

/* ml/startup.c */
void startup(void);

/* ml/wbadaddr.c */
int wbadaddr(volatile void *, int);

/* these functions defined multiply in ml/nvram.c */
int is_protected_adrs(unsigned long, unsigned long);
#ifdef EVEREST			/* XXX -- everst specific move out of here */
unchar get_nvreg(ulong);
int set_nvreg(ulong, unchar);
char nvchecksum(void);
#endif
int cpu_set_nvram_offset(int, int, char *);
int cpu_set_nvram(char *, char *);
void cpu_get_nvram_buf(int, int, char []);
char *cpu_get_nvram_offset(int, int);
#ifdef IP22
int cpu_set_dallas_offset(int, int, char *);
int cpu_set_eeprom_offset(int, int, char *);
#endif

/* these functions defined multiply in ml/{EVEREST,IP12,IP20,IP22}.c */
struct cfgnode_s *cpu_makecfgroot(void);
LONG cpu_savecfg(void);
void cpu_errputc(char);
int cpu_errgetc(void);
void cpu_install(struct component *);
void cpu_reset(void);
void cpu_hardreset(void);
void cpu_soft_powerdown(void);
#ifndef cpuid
int cpuid(void);
#endif
#if IP30
__uint64_t cpu_get_memsize(void);
__uint64_t cpu_get_full_memsize(void);
int cpu_is_active(int);
#else
unsigned int cpu_get_memsize(void); /* SN0 back to old syntax */
#endif	/* IP30 */
char *cpu_get_disp_str(void);
char *cpu_get_kbd_str(void);
char *cpu_get_serial(void);
void cpu_mem_init(void);
void cpu_acpanic(char *);
char *cpu_get_mouse(void);
unsigned cpu_get_freq(int);
void cpu_get_tod(struct timeinfo *);
void ecc_error_decode(u_int);
void cpu_scandevs(void);


/* ml/EVEREST.c */
void cpu_clear_errors(void);
void cpu_print_errors(void);
void epc_install(struct component *);
int splock(lock_t);
void spunlock(lock_t, int);
void dump_evconfig(void);
void ev_scsiedtinit(struct component *);
int prom_rev(void);
void ev_flush_caches(void);
int flush_iocache(int);

/* ml/r?kcache.s */
void flush_cache(void);
int _read_tag(int, ulong, uint *);
int _write_tag(int, ulong, uint *);
int _read_cache_data(int, ulong, uint *);
int _write_cache_data(int, ulong, uint *, uint);
void invalidate_caches(void);
void invalidate_dcache(uint, uint);
void invalidate_scache(uint, uint);
void __icache_inval(void *, int);
#ifdef IP32
void __dcache_wb(caddr_t, uint);
void __dcache_wb_inval(caddr_t, uint);
void __dcache_inval(caddr_t, uint);
#endif /* IP32 */
void set_ecc(int);
__psunsigned_t  get_ecc(void);
__psunsigned_t  get_cache_err(void);
__psunsigned_t  size_2nd_cache(void);

void uncache_K0(void);
void cache_K0(void);

#ifdef IP32
void r5000_enable_scache(void);
void r5000_disable_scache(void);
int Read_C0_Config(void);
void Write_C0_Config(int);
void run_uncached(void);
void run_cached(void);
void invalidate_sc_page(uint *);
void invalidate_sc_line(uint *);
#endif /* IP32 */

#ifdef R10000
unsigned int iCacheSize(void);
unsigned int dCacheSize(void);
unsigned int sCacheSize(void);
unsigned int is_K0_cached(void);
#endif

/* ml/genasm.s */
ulong GetSR(void);
ulong GetFPSR(void);
ulong get_SR(void);
ulong get_sr(void);
ulong set_cause(ulong);
ulong set_SR(ulong);
ulong SetFPSR(ulong);
ulong SetSR(ulong);
ulong set_sr(ulong);
ulong get_cause(void);
long get_prid(void);

#if TFP				/* ml/tfp*.s */
k_machreg_t tfp_getconfig(void);
k_machreg_t tfp_getprid(void);
unsigned int tfp_getticker(void);
#endif

/* ml/delay.c */
int _timerticksper1024inst(void);
void us_delay(uint);

/* ml/sysctlr.c */
void	clear_incomming(void);
int	spin_read(void);
int	sysctlr_getserial(char *);
int	sysctlr_setserial(char *);

/* fs/fatutil.c */
size_t strcspn (const char *, const char *);
USHORT stosi(UCHAR *);
ULONG stoi(char *);
VOID ParseBpb(struct bpb *, UCHAR *);


/* fs/bootp.c */
int bootp_strat(struct fsblock *);

/* fs/fs.c */
void fs_init(void);
int fs_search (struct eiob *);
int (*fs_match(char *, int))();

/* fs/is_tpd.c */
int is_tpd(struct tp_dir *);
int tpd_match(char *, struct tp_dir *, struct tp_entry *);
void tpd_list(struct tp_dir *);
void swap_tp_dir (struct tp_dir *);

/* net/mbuf.c */
void _init_mbufs(void);
struct mbuf *_m_get(int, int);
void _m_freem(struct mbuf *);

/* net/socket.c */
void _init_sockets(void);
struct so_table *_get_socket(u_short);
struct so_table *_find_socket(u_short);
void _free_socket(unsigned);
int _so_append(struct so_table *, struct sockaddr *, struct mbuf *);
struct mbuf *_so_remove(struct so_table *);

/* net/arp.c */
void _init_arp(void);
void arpwhohas(struct arpcom *, struct in_addr *);
void _arpresolve(struct arpcom *, struct in_addr *, u_char *);
void _arpinput(struct arpcom *, struct mbuf *);

/* net/if.c */
int _ifinit(void);
struct if_tbl *_if_tbl_alloc(void);
int _ifopen(struct ioblock *);
void _ifpoll(struct ioblock *);
int _ifstrategy(struct ioblock *, int);
int _ifioctl(struct ioblock *);
int _ifclose(struct ioblock *);
char *ether_sprintf(u_char *);
int _if_strat(struct component *, struct ioblock *);

/* net/if_ee.c */
int ee_init(void);
int ee_probe(int);
void ee_install(struct component *);
int ee_open(struct ioblock *);
int ee_close(void);
#ifdef NETDBX
void ee_config(unsigned long, int, unsigned long);
#endif /* NETDBX */

/* net/udpip.c */
void _ip_input(struct ifnet *, struct mbuf *);
int _ip_output(struct ifnet *, struct mbuf *, struct sockaddr_in *);
void _udp_input(struct ifnet *, struct mbuf *);
int _udp_output(struct ioblock *, struct ifnet *, struct mbuf *);

/* uart prototypes */
void get_baud(int);
int consgetc(int);
void consputc(u_char, int);
void z8530cons_errputc(u_char);

/* IP20/ec2 enet routines */
void reset_enet_carrier(void);
int enet_carrier_on(void);
int du_keyboard_port(void);

/* io/null.c */
int nullsetup(struct cfgnode_s **, struct eiob *, char *);

/* io/sgi_kbd.c */
void lampon(int);
void lampoff(int);
void bell(void);
int config_keyboard(struct component *);
int kb_translate(int, char *);
void kbd_updateKeyMap(int);

/* io/sgi_ms.c */
int ms_config(struct component *);
void _init_mouse(void);
void ms_install(void);
void ms_input(int);
void _mspoll(void);

/* io/dksc.c */
int dksc_strat(struct component *, struct ioblock *);
dev_t dk_mkdev(struct ioblock *);
int dkscopen(struct ioblock *);
int dkscclose(struct ioblock *);
int dkscstrategy(struct ioblock *, int);
int dkscioctl(struct ioblock *);
void getvolhdr(struct dksoftc *, caddr_t);
void dksyncmode(struct dksoftc *, int);
void dkscinit(void);
struct dksoftc *dk_alloc_softc(void);
void dk_unalloc_softc(struct dksoftc *);
void dkfreeall(struct dksoftc *);
void _dkscopen(dev_t, int, int);
void _dkscclose(dev_t, int, int);
void _dkscioctl(dev_t, unsigned int, caddr_t, int);
void dkscread(dev_t);
void dkscwrite(dev_t);
void dkscstart(struct dksoftc *, struct buf *);
void dk_intrend(struct dksoftc *, struct buf *);
void dkscprint(dev_t, char *);
long dkscsize(dev_t);
int dkscdump(dev_t, int, daddr_t, caddr_t, int);
int dk_check_condition(struct dksoftc *, u_char, int);
int dk_interpret_sense(struct dksoftc *, char *, dev_t);
void drive_acquire(struct dksoftc *);
void drive_release(struct dksoftc *);
int dk_chkready(struct dksoftc *, int);
int dk_oktoclose(struct dksoftc *);
int is_swapvh(struct volume_header *);
void dump_vh(struct volume_header *, char *);
void dkpr(unsigned, unsigned);

/* io/cdrom.c */
void revertcdrom(void);
void revert_walk(struct component *);

/* io/ioutils.c */
char *stdname(char *, char *, int, int, int);
int copyin(void *, void *, int);
int copyout(void *, void *, int);
void disksort(struct iobuf *, struct buf *);
void iodone(struct buf *);

/* io/scsi.c */
struct scsisubchan *allocsubchannel(int, int, int);
void freesubchannel(struct scsisubchan *);
void scsireset(struct scsisubchan *);
int scsidump(int);
void doscsi(struct scsisubchan *);
void scsi_seltimeout(u_char, int);
void scsiinstall(struct component *);
void scsiedtinit(struct component *);
int scsiunit_init(int);
void scsiintr(int);
void scsi_setsyncmode(struct scsisubchan *, int);
void scsi_sendabort(struct scsisubchan *);
void scsi_nosync(int);

u_char *scsi_inq(u_char, u_char, u_char);	/* io/wd95.c & io/scsi.c */

/* io/wd95.c */
void wd95sensepr(int, int, int, int, int);
int wd95alloc(u_char, u_char, u_char, int, void(*)());
struct scsi_target_info *wd95info(u_char, u_char, u_char);
int wd95dump(u_char);
int wd95_earlyinit(int);
void wd95timeout(struct wd95request *);
void wd95command(struct scsi_request *);
void scsi_setsyncmode(struct scsisubchan *, int);
struct scsisubchan *allocsubchannel(int, int, int);
void freesubchannel(struct scsisubchan *);
void doscsi(struct scsisubchan *);
void scsiinstall(struct component *);
void scsiedtinit(struct component *);

/* io/tpsc.c */
dev_t tpsc_mkdev(struct ioblock *);
int tpscopen(struct ioblock *);
int tpscclose(struct ioblock *);
int tpscstrategy(struct ioblock *, int);
int tpsc_strat(struct component *, struct ioblock *);
int tpsctapetype(char *, struct tpsc_types *);
void tpscinit(void);
void _tpscopen(dev_t, int);
void _tpscclose(dev_t, int);
void tpscread(dev_t);
void tpscwrite(dev_t);
void tpscioctl(dev_t, uint, caddr_t, int);

/* cmd/passwd_cmd.c */
struct cmd_table;
int passwd_cmd(int, char **, char **, struct cmd_table *);
int resetpw_cmd(int, char **, char **, struct cmd_table *);
int illegal_passwd(char *);
int validate_passwd(void);
int jumper_off(void);

/* cmd/date_cmd.c */
int format_date(struct timeinfo *, char *);

#ifdef _K64PROM32
int _isK64PROM32(void);
#endif

/* cmd/ping_cmd.c */
int ping(int, char **, char **, struct cmd_table *);

/* IPXXprom/hello_tune.c */
void play_hello_tune(int);
void wait_hello_tune(void);

#if defined (SN0)
typedef struct prom_dev_info_s {
	int 		(*install)() ; 	   /* driver install */
	int 		(*driver)() ;      /* driver _strat */
	void		*kl_comp;
	COMPONENT	*arcs_comp;
	int		flags;
	moduleid_t	modid ;
	slotid_t	slotid ;
	lboard_t	*lb 	;

} prom_dev_info_t;

int hwgraph_path_lookup(vertex_hdl_t, char *, vertex_hdl_t *, char **) ;
__psunsigned_t get_pci64_dma_addr(__psunsigned_t addr, ULONG key) ;
#endif

/* The prom needs to know what size pages we'll use to load a mapped
 * kernel.  For now, we're using 16M pages so all bits above 16M-1 should
 * be masked off.
 */
#define MAPPED_KERNEL_PAGE_MASK	0xffffff

#endif /* _LIBSK_H */
