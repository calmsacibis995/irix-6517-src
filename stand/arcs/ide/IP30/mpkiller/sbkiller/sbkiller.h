#define TEMPLATE_MARKER                "CODE_INSERT_POINT"
#define MEM_INIT_FILE			"mem.init"

#define RAND_SEL_BURST           0
#define FLAT_BURST               1
#define LOOP_BURST               2

#define CPU_REG_AVAILABLE        1
#define FPU_REG_AVAILABLE        2


/* this two are use ad tmp registers */

#define DATA64                   1
#define DATA32                   2

#define TMP_REG_1               28
#define TMP_REG_2               30

// max of 16*2 processors though for now we only create 16 way code

#define MAX_NODE_ID             16
#define MAX_BASE_ADDR		16

#define MAX_CPU_MP		16
#define MAX_BASE_ADDR		16
#define MAX_ATOMIC_BASE_REG	6
#define MAX_ATOMIC_IMMD_REG	5
#define MAX_ATOMIC_COMP_REG 	6	

#define MAX_NUM_PROCESSORS	64


#define FIRST_REG_AVAIL         2
#define MAX_BASE_REG		8
#define MAX_IMMD_REG		5
#define MAX_COMP_REG		10
#define MAX_IMMD_FPU_REG        6
#define MAX_COMP_FPU_REG        26	

#define DEF_BASE_LD_ST_32       0x8a002000
#define DEF_BASE_LD_ST_64       0xffffffff8a002000LL
#define DEF_ADDR_RANGE          0x100;

#define MAX_QUEUE		64

#define MAX_RANGE		0x990
#define MIN_RANGE		0x10
#define MIN_MP_RANGE		0x80
#define MAXLOCKS		500

#define TRUE			1
#define FALSE			0

#define	MIPSII			2
#define	MIPSIII			3

#define SPACE_XKUSEG		0
#define SPACE_XKSSEG		1
#define SPACE_XKPHYS		2
#define SPACE_XKSEG		3
#define SPACE_CKSEG0		4
#define SPACE_CKSEG1		5
#define SPACE_CKSEG		6
#define SPACE_CKSEG3		7


#define	UNCACHED_OFFSET      0x00003000

#define NUM_CACHE_WRAP	     8 

#define MAX_SCACHE_SIZE      0x400

#define CACHE_WRAP1 	     0x00004000	
#define CACHE_WRAP2	     0x00008000	
#define CACHE_WRAP3	     0x0000c000	
#define CACHE_WRAP4	     0x00040000
#define CACHE_WRAP5	     0x00080000
#define CACHE_WRAP6	     0x000c0000
#define CACHE_WRAP7	     0x00100000
#define CACHE_WRAP8	     0x00200000


#define CACHE_WRAP_MIN	     0x00004000
#define CACHE_WRAP_MAX	     0x00700000
#define ADDR_FUNC_MASK	     0x007ff000

#define EXTRA_MAP_PA_BASE    0x00800000

#define MIN_EXTRA_TLB	     0x0000c000
#define MAX_EXTRA_TLB	     0x00039000
#define DWORDMASK64	     0xfffffffffffffff8ll
#define DWORDMASK 	     0xfffffff8
#define	CACHE_TO_UNCACHE     0x20000000
#define CACHED_TO_PHYSICAL   0x1fffffff
#define CACHED64_TO_PHYSICAL   0x00ffffffffffffffll

#define UNCACHED_TO_PHYSICAL 0x1fffffff
#define LOCK_BASE_SW	     0x9fff0000
#define LOCK_BASE_HW	     0x80200000

#define	MAPPED_CACHE_VA	     0x02000000
#define EXTRA_MAPPED_VA	     0x03000000	
#define	MAPPED_UNCACHE_VA    0x04000000
#define	UNMAPPED_CACHE_VA    0x80000000
#define	UNMAPPED_UNCACHE_VA  0xa0000000


#define MP_STACK_SIZE	     256

#define UNMAP_CACHED_32	     0xffffffff80000000LL
#define UNMAP_CACHED_MASK_32 0x000000001fffffffLL
#define SIGNBIT_OF_32_IN_64  0x0000000080000000LL

#define DIRECTMAP_CACHED_8   0xffffffff80000000LL
#define DIRECTMAP_CACHED_9   0xffffffff90000000LL
#define DIRECTMAP_UNCACHED_A 0xffffffffa0000000LL
#define DIRECTMAP_UNCACHED_B 0xffffffffb0000000LL

#define DIRECTMAP_UNC_MASK   0xfffffffff0000000LL

#define DIRECTMAP_64_MASK     0xf000000000000000LL
#define UNCACH64_TO_PHYS_MASK 0xff00000000000000LL

#define DIRECTMAP_64_8       0x8000000000000000LL
#define DIRECTMAP_64_9       0x9000000000000000LL
#define DIRECTMAP_64_A	     0xa000000000000000LL
#define DIRECTMAP_64_B	     0xb000000000000000LL

#define DIRECTMAP_64_VA_PA   0x0fffffffffffffffLL

#define SN0_M_MASK          0x000000ff00000000LL
#define SN0_N_MASK          0x000000ff80000000LL

// sn0 stuff

#define FETCH_OP_ADDR		0x00
#define FETCH_INC_ADDR		0x08
#define FETCH_DEC_ADDR		0x10
#define FETCH_CLR_ADDR		0x18

#define STORE_OP_ADDR		0x00
#define STORE_INC_ADDR		0x08
#define STORE_DEC_ADDR		0x10
#define STORE_AND_ADDR		0x18
#define STORE_OR_ADDR		0x20

#define FETCH_OP	     	0x00
#define FETCH_INC_OP		0x08
#define FETCH_DEC_OP		0x10
#define FETCH_CLR_OP		0x18
#define INIT_OP			0x00
#define INC_OP			0x08
#define DEC_OP			0x10
#define AND_OP			0x18
#define OR_OP			0x20

#define FUNC_LLSC_INC		0x1
#define FUNC_LLSC_DEC		0x2
#define FUNC_FETCH_OP		0x3
#define FUNC_FETCH_INC_OP	0x4
#define FUNC_FETCH_DEC_OP	0x5
#define FUNC_FETCH_CLR_OP	0x6
#define FUNC_INIT_OP		0x7
#define FUNC_INC_OP		0x8
#define FUNC_DEC_OP		0x9
#define FUNC_AND_OP		0xa
#define FUNC_OR_OP		0xb
#define FUNC_BARRIER_LLSC	0xc
#define FUNC_BARRIER_FETCH_OP	0xd


#define FUNC_INIT_ADDRESS_OP  	0x21
#define INIT_ADDRESS            0x22

#define CHECK_BIT		0x1000
#define CHECK_LLSC_INC		0x1001
#define CHECK_LLSC_DEC		0x1002
#define CHECK_FETCH_OP		0x1003
#define CHECK_FETCH_INC_OP	0x1004
#define CHECK_FETCH_DEC_OP	0x1005
#define CHECK_FETCH_CLR_OP	0x1006
#define CHECK_INIT_OP		0x1007
#define CHECK_INC_OP		0x1008
#define CHECK_DEC_OP		0x1009
#define CHECK_AND_OP		0x100a
#define CHECK_OR_OP		0x100b
#define CHECK_BARRIER_LLSC	0x100c
#define CHECK_BARRIER_FETCH_OP	0x100d


/* relative starting point of uncached access to cached access */

#define NUM_ADDR_MODES		5

#define	UNMAP_CACHED	 	0
#define UNMAP_CACHE_WRAP 	1
#define UNMAP_UNCACHED		2
#define	MAP_CACHED		3
#define	MAP_UNCACHED		4

// hints to address allocator

#define UNMAPPED_ADDR           0x0000
#define MAPPED_ADDR             0x0001
#define MAPPED_ADDR_MASK        0x0001

#define SPACE32_ADDR            0x0000
#define SPACE64_ADDR            0x0002
#define SPACE_ADDR_MASK         0x0002

#define CACHED_ADDR             0x0000
#define UNCACHED_ADDR           0x0004
#define CACHE_MASK              0x0004


#define SEL_OFFSET              0x0008
#define SEL_OFFSET_MASK         0x0008
#define NO_OFFSET               0x0000

 // recycled memory
#define CACHE_NO_WRAP           0x0000
#define CACHE_WRAP              0x4000
#define USED_MEM		0x8000

 // 4K default page size for alignment of uncached segement
#define PAGESIZE_IN_BITS        12

#define MAX_MAP_EXTRA		8	

 /* first TLB page to use for load stores. 6 pages are needed. */

#define LD_ST_FIRST_PAGE	0

#define TMP_REG		 1
#define SIGN_EXTEND_REG r31
/* instructions */

#define NUM_OF_INSTRUCTIONS  47

#define 	LB		0
#define		LH		1
#define		LW		2

#define 	SB		3
#define 	SH		4
#define 	SW		5

#define		LD		6
#define 	SD		7

#define		LWL		8
#define		LWR		9
#define		SWL		10		
#define		SWR		11	
#define		LDL		12	
#define		LDR		13	
#define		SDL		14
#define		SDR		15

#define		HIT_WRI_INV_D 	  16	
#define		INDEX_WRI_INV_D	  17	
#define		HIT_WRI_INV_SD 	  18	
#define		INDEX_WRI_INV_SD  19	

#define 	SHIFT_SLIP	  20
#define		SIGN_EXTEND_STALL 21

#define		LLSC		  22
#define		DLLSC		  23
#define 	FP		  24
#define		LWU		  25
#define		LHU		  26
#define		SC		  27
#define		LL		  28
#define		SCD		  29
#define		LLD		  30
#define 	BB_PREFETCH	  31
#define		T5_PREFETCH	  32
#define		PREFETCH_LABEL	  33
#define		ALU_OP		  34

#define         HIT_INV_I         35
#define         HIT_INV_SI        36
#define         INDEX_INV_I   	  37
#define         FILL_I  	  38 

#define         LDC1              39
#define         LWC1              40   
#define         SDC1              41
#define         SWC1              42
#define         SWXC1             43
#define         SDXC1             44
#define         LDXC1             45
#define         LWXC1             46  

#define 	REUSE_REG	  0
#define		NEW_REG		  1
#define 	SLIPSTALLS	  6

#define 	ADDR_HEX	  0
#define 	ADDR_LABEL	  1

typedef unsigned long long ULL;

class Work {
private:
    int operation;
    int type;
    ULL data;
    ULL address;
    char addrLabel[32];

public:
    Work * next;

    Work( int op, ULL addr, ULL dat );
    Work( int op, char * label, ULL dat );

    int execute();
    int getOp();
    int getData();
};

class	sharedOp {

private:

    int iterations;
    int numCpu;

    Work * initWrk;
    Work * cpuWrk[MAX_NUM_PROCESSORS];
    Work * checkWrk;

public:
    sharedOp();
    ~sharedOp();
    void randomizeWrkQueue ();
    Work * popWorkQueue( int cpuid );
    void addToInitWorkQueue ( Work * request );
    void addToCpuWorkQueue ( int cpu, Work * request );
    void addToCheckWorkQueue ( Work * request );
    void shuffle();

    void  initL( int operation, char * address, ULL data, int CpuID );
    void  initL( int operation, ULL    address, ULL data, int CpuID );
    void  init( int operation, ULL address, int count, int maxNumCpu );
    void  init( int operation, char * address, int count, int maxNumCpu );
    void prologue();
    void epilogue();
    int genCode( int cpuId );
};

typedef unsigned long long ULL;
typedef unsigned int UINT;

struct lock {
      unsigned int address;
      unsigned int count;
};

struct PrefSyncRecord {
    int			type;
    unsigned int	vaddrHi, vaddrLo;
    unsigned int	paddrHi, paddrLo;
    int			hint;
};


class Address {
public:

ULL  VAddr;
ULL  PAddr;
UINT Property;


Address ( ULL VAddr, ULL PAddr, UINT Property );
Address ();
void init ( ULL vaddr );

};

class Cell {

private:
public:
    ULL  address;
    ULL  value;
    UINT property;

    Cell * next;
    Cell * prev;

    Cell ( ULL addr, ULL val, UINT prop );
};

class Memory {

private:

    int baseUncSet;

    Cell * topOfMemory;



    Address baseLdStAddrG[MAX_BASE_ADDR];
  //ULL baseLdStPhysAddrG[MAX_BASE_ADDR];

    Address baseUncLdStAddrG[MAX_BASE_ADDR];
  //ULL baseUncLdStPhysAddrG[MAX_BASE_ADDR];



        // global flags for state of memory system

    int memHasUncached;
    int memHasCached;

      // how many base addresses set for cached and uncached 



    int queueSize;

    ULL lastAddrQ[MAX_QUEUE];

public:

    ULL cacheWrapAddr[NUM_CACHE_WRAP];
    ULL nodeID[MAX_NODE_ID];
    UINT addrRange;
    int maxCacheWrapVal;

    int cellCount;
    int memReuseRate;
    int nodeIDCnt;
    int bAddrCnt;
    int bUncAddrCnt;

    Memory();

    Cell * allocateCell( ULL addr, UINT addrMode );
    void   deAllocMem();

    Cell * locateCell( ULL Addr );
    Cell * placeCell( ULL Addr, UINT addrMode );
    void   printMemory();
    Cell * getTopOfMemory();
    UINT   getAddrRange();
    int    checkForOverlap();
    void   computeAllBases();
    void   zeroTouchedMem();


ULL    pickPhysAddr( int cpuID, UINT& hint );
ULL    mapPhysAddr ( ULL physAddr, UINT hint );
void   addBaseAddr( ULL Addr );
void   addUncBaseAddr( ULL Addr );
void   addNodeID( ULL nodeid );
void   setCacheWrapAddr( int index , ULL offset );
void   setSN0Mode( int numCpus, int shiftAmt );
void   setAddrRange( UINT range );  
void   dumpMemUsageFile();
void   usedAddr( ULL physAddr );
 
};
struct cell {

	unsigned int	address;
	unsigned int	hi;
	unsigned int	lo;

	struct cell *next;
	struct cell *prev;
};
