/*
* Copyright 1998 - PsiTech, Inc.
* THIS DOCUMENT CONTAINS CONFIDENTIAL AND PROPRIETARY
* INFORMATION OF PSITECH.  Its receipt or possession does
* not create any right to reproduce, disclose its contents,
* or to manufacture, use or sell anything it may describe.
* Reproduction, disclosure, or use without specific written
* authorization of Psitech is strictly prohibited.
*
*/

#include <sys/cmn_err.h>
			
#define MAX_RAD1S 4	/* this driver will support no more then MAX_RAD1S per2s */

#if 0
#define DEBUGGING_SLEEP_LOCKS	/* */
#define DEBUGGING_LOCKS	/* */
#endif

#ifdef DEBUGGING_SLEEP_LOCKS
#define SLP_LOCK_TYPE struct{sleep_t lk; int the_line;}
#define SLP_LOCK_INIT(p) SLEEP_INIT(&((p)->lk),(int)0,(lkinfo_t *)-1)
#define SLP_LOCK_LOCK(p) if(SLEEP_TRYLOCK(&((p)->lk))==FALSE) \
						{ \
							cmn_err(CE_DEBUG,"%d sleep on %x by %d\n", __LINE__, p, (p)->the_line); \
							SLEEP_LOCK(&((p)->lk),(int)-1); \
							cmn_err(CE_DEBUG,"%d got %x\n", __LINE__, p); \
						} \
						(p)->the_line = __LINE__
#define SLP_LOCK_UNLOCK(p) SLEEP_UNLOCK(&((p)->lk))
#else /* DEBUGGING_SLEEP_LOCKS */
#define SLP_LOCK_TYPE sleep_t
#define SLP_LOCK_INIT(p) SLEEP_INIT((p),(int)0,(lkinfo_t *)-1)
#define SLP_LOCK_LOCK(p) SLEEP_LOCK((p),(int)-1)
#define SLP_LOCK_UNLOCK(p) SLEEP_UNLOCK((p))
#endif /* DEBUGGING_SLEEP_LOCKS */

#define INT_LOCK_INIT(p) LOCK_INIT(&((p)->lk),(uchar_t)-1,plhi,(lkinfo_t *)-1)
#define INT_LOCK_INIT_LO(p) LOCK_INIT(&((p)->lk),(uchar_t)-1,plbase,(lkinfo_t *)-1)
#ifdef DEBUGGING_LOCKS
#define INT_LOCK_TYPE struct{lock_t lk; int cookie; int the_line;}
#define INT_LOCK_LOCK(p) ((p)->cookie=TRYLOCK(&((p)->lk),plhi)); \
						if((p)->cookie==INVPL) \
						{ \
							cmn_err(CE_DEBUG,"%d wait on %x by %d\n", __LINE__, p, (p)->the_line); \
							((p)->cookie=LOCK(&((p)->lk),plhi)); \
							cmn_err(CE_DEBUG,"%d got %x\n", __LINE__, p); \
						} \
						(p)->the_line = __LINE__
#define INT_LOCK_LOCK_LO(p) ((p)->cookie=TRYLOCK(&((p)->lk),plbase)); \
						if((p)->cookie==INVPL) \
						{ \
							cmn_err(CE_DEBUG,"%d wait on %x by %d\n", __LINE__, p, (p)->the_line); \
							((p)->cookie=LOCK(&((p)->lk),plbase)); \
							cmn_err(CE_DEBUG,"%d got %x\n", __LINE__, p); \
						} \
						(p)->the_line = __LINE__
#else /* DEBUGGING_LOCKS */
#define INT_LOCK_TYPE struct{lock_t lk; int cookie;}
#define INT_LOCK_LOCK(p) ((p)->cookie=LOCK(&((p)->lk),plhi))
#define INT_LOCK_LOCK_LO(p) ((p)->cookie=LOCK(&((p)->lk),plbase))
#endif /* DEBUGGING_LOCKS */
#define INT_LOCK_UNLOCK(p) UNLOCK(&((p)->lk),(p)->cookie)

#define INT_LOCK_SV_WAIT(sv,p) SV_WAIT(sv,&(p)->lk,(p)->cookie)

#ifdef USE_MUTEX_LOCKS
#ifdef IP32
#define INT_LOCK_TYPE struct{lock_t lk; int cookie;}
#define INT_LOCK_INIT(p) LOCK_INIT(&(p)->lk,(uchar_t)-1,plhi,(lkinfo_t *)-1)
#define INT_LOCK_LOCK(p) ((p)->cookie=LOCK(&(p)->lk,plhi))
#define INT_LOCK_UNLOCK(p) UNLOCK(&(p)->lk,(p)->cookie)
#define INT_LOCK_SV_WAIT(sv,p) SV_WAIT(sv,&(p)->lk,(p)->cookie)
#else /* IP32 */
#define INT_LOCK_TYPE mutex_t
#define INT_LOCK_INIT(p) MUTEX_INIT(p,MUTEX_DEFAULT,NULL)
#define INT_LOCK_LOCK(p) cmn_err(CE_DEBUG,"line %d lock %x ",__LINE__, p); \
						MUTEX_LOCK(p,-1); \
						cmn_err(CE_DEBUG,"taken\n")
#define INT_LOCK_UNLOCK(p) cmn_err(CE_DEBUG,"line %d unlock %x ",__LINE__, p); \
						MUTEX_UNLOCK(p); \
						cmn_err(CE_DEBUG,"released\n")
#define INT_LOCK_SV_WAIT(sv,p) SV_WAIT(sv,p,0)
#endif /* IP32 */
#endif /* USE_MUTEX_LOCKS */

extern int rad1pci_major; /* defined in master should be 65 */
extern int rad1pci_use_userbuffer; /* defined in master 0 = dont use maputokv*/
extern int rad1pci_max_dma_area; /* max transfer size*/
extern int rad1pci_no_dma; /* max transfer size*/
extern int rad1pci_enable_prefetch; /* 0 - prefetch off, 1 - prefetch enabled*/

typedef volatile unsigned char cfg_reg;

typedef struct /*this is for the info to release resources after a chained dma*/
{ int user; /* TRUE if user space FALSE if k space */
  int count;
  caddr_t uv_ptr;  /* virtual user space pointer */
  caddr_t kv_ptr; /* virtual kernal space pointer */
  paddr_t phys_ptr; /* physical pointer */
  pciio_dmamap_t dma_map;	/* dma map object */
} dma_mapping_desc;
 
#define MAX_LUT_ENTRIES 256
#define MAX_WID_ENTRIES 256
typedef struct
{
	int type;	/* must be first in per2_info and per2serial_info */
	vertex_hdl_t dev;	/* in vertex */
	int board_number;
	struct device_desc_s dev_desc;
	cfg_reg *cfg_ptr;  /* used for pciio_config_(set/get) */
	int *registers;
	int *ram1;
	int *ram2;
	int *tbuf;
	int *fbuf;
	int screen_base;
	unsigned int NumValidModes;
	rad4_video_info_ex v_info;
	dma_mapping_desc *map_i;
	unsigned int mask;
	unsigned int hard_mask;
	int ram_mapped;	 /* TRUE if the ram is mapped, FALSE if mapped for dma */
	int revision;
	int vendor_id;
	int device_id;
	pciio_dmamap_t dma_map;
	int dma_enabled;
	pciio_intr_t intobj;
	int opened;
	INT_LOCK_TYPE per2_global_lock;
	SLP_LOCK_TYPE vblank_mutex_lock;
	SLP_LOCK_TYPE bigx_mutex_lock;
	SLP_LOCK_TYPE mask_mutex_lock;
	SLP_LOCK_TYPE hardware_mutex_lock;
	SLP_LOCK_TYPE dma_sleep_lock;
	int bigx_daemon_stall;
	toid_t bigx_cookie;
	sv_t *sync_wait_on_it;
	toid_t sync_to_cookie;
	sv_t *dma_rd_wait_on_it;
	sv_t *dma_wr_wait_on_it;
	toid_t dma_to_cookie;
	sv_t *lut_wait_on_it;
	sv_t *wid_wait_on_it;
	int lut_flag;
	toid_t lut_to_cookie;
	int lut_shadow[MAX_LUT_ENTRIES];
	int wid_flag;
	toid_t wid_to_cookie;
	int wid_shadow[MAX_WID_ENTRIES];
	int time_happened;
	int cursor_x_offset;
	int cursor_y_offset;
	int tty_initted;
	per2_eprom_info saved_info;
	p2_intenable_reg p2_intenable;
	p2_memconfig_reg p2_memconfig;
	p2_aperture_reg p2_aperture_1;
	p2_aperture_reg p2_aperture_2;
	p2_vclkctl_reg p2_vclkctl;
	p2_videocontrol_reg p2_videocontrol;
	p2_rdcolormode_reg p2_rdcolormode;
	p2_rdmisccontrol_reg p2_rdmisccontrol;
	p2_rdcursorcontrol_reg p2_rdcursorcontrol;
	p2_fbreadmode_reg p2_fbreadmode;
	p2_fbwritemode_reg p2_fbwritemode;
	p2_filtermode_reg p2_filtermode;
	p2_dithermode_reg p2_dithermode;
	p2_alphablendmode_reg p2_alphablendmode;
	p2_bydmacontrol_reg p2_bydmacontrol;
	p2_lbreadmode_reg p2_lbreadmode;
	p2_texturemapformat_reg p2_texturemapformat;
	p2_scissormode_reg p2_scissormode;
	p2_window_reg p2_window;
	p2_areastipplemode_reg p2_areastipplemode;
	p2_depthmode_reg p2_depthmode;
	p2_stencilmode_reg p2_stencilmode;
	p2_yuvmode_reg p2_yuvmode;
	p2_colorddamode_reg p2_colorddamode;
	p2_fogmode_reg p2_fogmode;
	p2_logicalopmode_reg p2_logicalopmode;
	p2_statisticmode_reg p2_statisticmode;
	p2_rdcursormode_2v_reg p2_rdcursormode_2v;
	p2_rdcolorformat_2v_reg p2_rdcolorformat_2v;
	p2_rddaccontrol_2v_reg p2_rddaccontrol_2v;
	p2_rdmisccontrol_2v_reg p2_rdmisccontrol_2v;
	p2_rdsynccontrol_2v_reg p2_rdsynccontrol_2v;
	p2_packeddatalimits_reg p2_packeddatalimits;
} per2_info;
 
typedef struct
{
	int type;	/* must be first in per2_info and per2serial_info */
	per2_info *p2;
} per2serial_info;
#define RAD1_TYPE 0
#define RAD1BIG_TYPE 3
#define RAD1TTY_TYPE 4

/* Function prototypes */
void rad1_pinit(per2_info *p2, int console_flag);
void rad1_set_video_timing(per2_info *p2, int index, unsigned int sync_mode,
	unsigned int h_polarity, unsigned int v_polarity, int console_flag);
void rad1_init_RAMDAC(per2_info *p2);
void rad1_load_lut(per2_info *p2);

void RAD1BoardTPInit(per2_info *p2, int board);
void RAD1Reset(volatile int *registers, int board);
int RAD1_baseindex(per2_info *p2);
void RAD1InitBoard(per2_info *p2);
void RAD1InitCursor(per2_info *p2);
