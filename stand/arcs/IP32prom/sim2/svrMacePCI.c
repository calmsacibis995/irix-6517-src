#include <assert.h>
#include "sim.h"
#include "simSvr.h"
#include <sys/mace.h>
#include <stdlib.h>
#include <string.h>

#define MIPS_BE
#include "../lib/libmsk/adp_HIM/him_equ.h"

#define UNIMPLEMENTED 0

/* PCI bus configuration maximums */
#define MAXPCIBUS 1
#define MAXPCIDEV 8
#define MAXPCIFUN 8


/*****************************************************************************
 *                          Generic PCI Support                              *
 *****************************************************************************/

/*
 * Simulated PCI configuration space entry.
 */
typedef union{
  struct{
    __uint32_t
      vendorID : 16,
      deviceID : 16;
    __uint32_t
      cmd : 16,
      sts : 16;
    unsigned char class[3];
    unsigned char rev;
    __uint32_t
      BIST : 8,
      HdrT : 8,
      LatT : 8,
      CLS  : 8;
    __uint32_t
      baddr[6];
    __uint32_t
      res1[2];
    __uint32_t
      ExROMBase;
    __uint32_t
      res2[2];
    __uint32_t
      MaxLat : 8,
      MinGnt : 8,
      IntPin : 8,
      IntLin : 8;
  } cfg;
  __int32_t regs[16];
}PCICfg_t;


/* Per "slot" data */
struct pci_dev{
  void       (*service)(struct pci_dev*,int);
  PCICfg_t   cfg;
  void*      data;
};
typedef struct pci_dev PCIDev_t;

/* Per PCI state */
typedef struct{
  __int32_t 	  config_addr;	/* Config space address register */
  __int32_t 	  config_data;	/* Config space data register */
  SimAddrMap_t    map;		/* Simulation mapping */
  PCIDev_t*       dev[MAXPCIBUS][MAXPCIDEV][MAXPCIFUN];  /* Slots */
} PCI_t;

PCI_t* pci;

void pciRegister(int,int,int,void (*)(PCIDev_t*));
void pciHostInit(PCIDev_t*);
void pciHostCfgService(PCIDev_t*,int);
void pciADP78Init(PCIDev_t*);
void pciADP78CfgService(PCIDev_t*,int);


/*************
 * swap_word32
 */
static __int32_t
swap_word32(__int32_t i){
  return
    ( (i&0xFF)<<24 ) |
      (((i>>8)&0xff)<<16) |
	(((i>>16)&0xff)<<8) |
	  ((i>>24)&0xff);
}


/*********************
 * simPCIConfigService	Dispatach config space service
 */
void
simPCIConfigService(void){
  int       addr= swap_word32(pci->config_addr);
  int       bus = (addr >> 16) & 0x7f;
  int       dev = (addr >> 11) & 0x1f;
  int       fun = (addr >> 8)  & 0x7;
  int       r   = (addr >> 2)  & 0x3f;
  int       isStore= simControl->matchCode==SIM_loadStore_fwd;
  PCIDev_t* pcidev;

  if (bus>=MAXPCIBUS || fun>=MAXPCIFUN || dev >=MAXPCIDEV){
    pci->config_data = -1;
    return;
  }
  pcidev = pci->dev[bus][dev][fun];
  if ((pcidev) && (pcidev->service))
    (*pcidev->service)(pcidev,isStore);
  else
    pci->config_data = -1;
}


/************
 * simPCIInit
 */
void
simPCIInit(void){
  int  targetCode = simRegisterService(simPCIConfigService);

  pci  = (PCI_t*)usmemalign(8,sizeof(PCI_t),arena);

  /* Initialize address map structure */

  pci->map.base  = (char*)pci;
  pci->map.stride= 4;
  pci->map.mask  = 0x4;
  pci->map.shift = 2;

  /* Read/Writes to the config_addr are direct */
  simRegisterAddrMatch(-1 & (~ADDR_MATCH_STORE),
		       PCI_CONFIG_ADDR,
		       SIM_loadStore,
		       0,
		       &pci->map);

  /* Reads and writes of config_data are forwarded */
  simRegisterAddrMatch(-1,					/* Read */
		       PCI_CONFIG_DATA ,
		       SIM_fwd_loadStore,
		       targetCode,
		       &pci->map);
  simRegisterAddrMatch(-1,
		       PCI_CONFIG_DATA | ADDR_MATCH_STORE,	/* Write */
		       SIM_loadStore_fwd,
		       targetCode,
		       &pci->map);

  /* Register particular PCI devices */
  pciRegister(0,0,0,pciHostInit);
  pciRegister(0,1,0,pciADP78Init);
  pciRegister(0,2,0,pciADP78Init);
}


/*************
 * pciRegister	Register a PCI device
 */
void
pciRegister(int bus, int dev, int fun, void(*init)(PCIDev_t*)){
  PCIDev_t* pcidev = pci->dev[bus][dev][fun];
  assert(pcidev==0);

  pcidev = pci->dev[bus][dev][fun] = (PCIDev_t*)malloc(sizeof(PCIDev_t*));
  memset(pcidev,0,sizeof(PCIDev_t));
  assert(pcidev);
  (*init)(pcidev);
}


/****************************************************************************
 *                          PCI HOST BRIDGE                                 *
 ****************************************************************************/


/*************
 * pciHostInit	Initialize PCI Host bridge service
 */
void
pciHostInit(PCIDev_t* pcidev){
  static PCICfg_t cfg = { 0x10a9, 0x04,0,0,6,0,0,0};
  pcidev->service = pciHostCfgService;
  memcpy(&pcidev->cfg,&cfg,sizeof(cfg));
}


/****************
 * pciHostService Service PCI Host bridge config space accesses
 *---------------
 * Host bridge config space for now behaves just like RAM.
 */
void
pciHostCfgService(PCIDev_t *pcidev, int isStore){
  int addr= swap_word32(pci->config_addr);
  int r   = (addr >> 2)  & 0x3f;

  if (!isStore)
    pci->config_data = swap_word32(pcidev->cfg.regs[r]);
  else
    pcidev->cfg.regs[r] = swap_word32(pci->config_data);
  return;
}



/****************************************************************************
 *                         ADP78 SCSI Configuration                         *
 ****************************************************************************/

/* Adaptec 7870 simulation state */
typedef struct{
  u_char       regs[256];
  u_char       seq[2048];
  SimAddrMap_t map;
} ADP78_t;

#define MAXADPS 32
ADP78_t* adps[MAXADPS];

void
adp78Service(void);

/********************
 * pciADP78AllocateIO	Allocate an IO space chunk for the ADP78
 */
void
pciADP78AllocateIO(PCIDev_t* pcidev){
  __int32_t ioaddr  = swap_word32(pci->config_data) & 0xffffff00;
  __int32_t oldaddr = pcidev->cfg.regs[4] & 0xffffff00;
  ADP78_t*  adp     = usmalloc(sizeof(ADP78_t),arena);
  int       service = simRegisterService(adp78Service);

  assert(service<MAXADPS);
  adps[service] = adp;
  assert(adp);
  memset(adp,0,sizeof(*adp));

  /* Set up map */
  adp->map.base   = adp->regs;
  adp->map.stride = 1;
  adp->map.mask   = 0xff;
  adp->map.shift  = 0;

  /*
   * We require that the old address be 0 (unallocated) or the same
   * as the new address because we can't unregister an address range once
   * registered.
   */
  assert( oldaddr == 0 || oldaddr == ioaddr );
  
  if (oldaddr !=0 )
    return;

  /*
   * Register a 256 byte chunk of memory for the interface.
   * All R/W are handled by the server.
   *
   * Note: The service code depends on SIM_fwd_loadStore implying read
   *       and SIM_loadStore_fwd implying write.
   */
  simRegisterAddrMatch(0xFFFFFFFFFFFFFF00LL,
		       ioaddr,				/* Read  */
		       SIM_fwd_loadStore,
		       service,
		       &adp->map);
  simRegisterAddrMatch(0xFFFFFFFFFFFFFF00LL,
		       ioaddr | ADDR_MATCH_STORE,	/* write  */
		       SIM_loadStore_fwd,
		       service,
		       &adp->map);

  /* Record address in config register */
  pcidev->cfg.cfg.baddr[0] = ioaddr;
}


/**************
 * pciADP78Init	Initialize ADP78 service
 */
void
pciADP78Init(PCIDev_t* pcidev){
  static PCICfg_t cfg = {0x7078,0x9004, 0,0, 1,0,0,0, 1,0,0,0,0,0};
  pcidev->service = pciADP78CfgService;
  memcpy(&pcidev->cfg,&cfg,sizeof(cfg));
}


/*****************
 * pciADP78Service	Service ADP78 config space accesses
 *----------------
 * Respond to ADP78 config space accesses.
 */
void
pciADP78CfgService(PCIDev_t* pcidev, int isStore){
  int addr= swap_word32(pci->config_addr);
  int r   = (addr >> 2)  & 0x3f;

  switch(r){

    /* Base address */
  case 4:
    if (!isStore)
      pci->config_data = swap_word32((pcidev->cfg.regs[4] & 0xffffff00) | 1);
    else
      pciADP78AllocateIO(pcidev);
    break;

    /* Read only registers */
  case 0:			/* VendorID/DeviceID */
  case 2:			/* Class/Revision */
  case 3:			/* BIST/... */
    if (isStore)
      break;

    /* Read/Write with no side effects */
  case 1:			/* Command/Status */
    if (!isStore)
      pci->config_data = swap_word32(pcidev->cfg.regs[r]);
    else
      pcidev->cfg.regs[r] = swap_word32(pci->config_data);
    return;

    /* Registers we don't implement */
  default:
    if (!isStore)
      pci->config_data = -1;
  }
}

/****************************************************************************
 *                      ADP78 SCSI Chip Simulation                          *
 ****************************************************************************/

static
traceReg(int idx, int reg, int isStore){
  if (isStore)
    printf(" SIM adp78[%d]: %0x (write)\n", idx, reg);
  else
    printf(" SIM adp78[%d]: %0x (read)\n", idx, reg);

}

/**************
 * adp78Service		Service access to ADP78 registers
 */
void
adp78Service(void){
  SimAddrMatch_t* am      = &simControl->addrTbl[simControl->matchIndex];
  int             isStore = simControl->matchCode==SIM_loadStore_fwd;
  unsigned char*  regs    = am->mapping->base;
  int             r       = simControl->paddr & 0xff;
  int             idx     = simControl->matchIndex;
  ADP78_t*        adp     = adps[simControl->addrTbl[idx].targetCode];

  assert(idx<MAXADPS);

  /* Process register accesses */
  switch(r){

  case SEEPROM:
    /* Don't return chip select.  No chip select means not EEROM present */
    if (!isStore)
      regs[r] &= ~SEECS;
    break;
    
  case SEQRAM:
    /*
     * Access the sequencer RAM.  We don't really do anything with the
     * sequencer stuff but the software reads back what it writes so we can't
     * just ignore it.
     */
    idx = regs[SEQADDR1]<<8 | regs[SEQADDR0];
    if (isStore)
      adp->seq[idx++] = regs[r];
    else
      regs[r] = adp->seq[idx++];
    regs[SEQADDR0] = idx & 0xff;
    regs[SEQADDR1] = idx >> 8;
    break;

  }
}






