/*
 * Digital Media Ring Buffer Device
 *
 * $Revision: 1.4 $
 *
 */

/*
 * Info block header
 *
 * There is one of these for each element of the ring buffer
 *
 */
typedef struct _RingBufferTag {
    int nextBlock;    /* the next block number */
    int endData;      /* where the head pointer should be after this block */
    int offset;       /* the offset of the data here... */
    int size;         /* the total size of the data */
    int sizeTilWrap;  /* the amount of data before we wrap */
} RingBufferTag;

/*
 * Ring buffer header
 *
 * There is one of these per ring buffer
 */
typedef struct {
	int key;	    /* Unique ring buffer id */
	int totalLength;

	int infoBaseOffset;/* the offset to the info base for shared mem */
	int dataBaseOffset;/* the offset to the data base for shared mem */
	int refCount;     /* the reference count */

	int infoSize;       /* this just makes it easier to copy */
	int alignSize;      /* the alignment required of the data */
	int wrapAllowed;    /* can wrap be handled */
	int dataSize;       /* the total size of the data */
	int numInfoBlocks;  /* the number of frames in the buffer */
	int frameSize;      /* Size per element, if fixed */

	int dataTail;     /* the head of the free data */
	int dataHead;     /* the head of valid data */

	int infoTail;     /* the index of the first free info block */
	int infoHead;     /* the index of the first valid info block */

	int done;         /* no more data will be Put to the buffer */

	int allocTail;    /* Allocation point for free info blocks */
	int allocHead;    /* Allocation point for valid info blocks */
} RingBufferHeader;

#define InfoFree(rP)  ((rP->RBH.infoHead-rP->RBH.infoTail+rP->RBH.numInfoBlocks) % \
						(rP->RBH.numInfoBlocks<<1))
#define InfoValid(rP) ((rP->RBH.infoTail-rP->RBH.infoHead+(rP->RBH.numInfoBlocks<<1)) % \
						(rP->RBH.numInfoBlocks<<1))

#define DataFree(rP)  ((rP->RBH.dataHead-rP->RBH.dataTail+rP->RBH.dataSize) % \
						(rP->RBH.dataSize<<1))
#define DataValid(rP) ((rP->RBH.dataTail-rP->RBH.dataHead+(rP->RBH.dataSize<<1)) % \
						(rP->RBH.dataSize<<1))

#define InfoAllocFree(rP) ((rP->RBH.infoHead-rP->RBH.allocTail+rP->RBH.numInfoBlocks) % \
						(rP->RBH.numInfoBlocks<<1))

#define InfoAllocValid(rP) ((rP->RBH.infoTail-rP->RBH.allocHead+(rP->RBH.numInfoBlocks<<1)) % \
						(rP->RBH.numInfoBlocks<<1))

#define TagFromBase(rb, rP, off) ((RingBufferTag *) (rb->infoBase+\
					(off%rP->RBH.numInfoBlocks * rP->RBH.infoSize)))
#define DataFreeTilWrap(rP)      (rP->RBH.dataSize - rP->RBH.dataTail%rP->RBH.dataSize)


/*
 * DMRB Driver interface
 *
 */
#define RBIOC		('r'<<24|'b'<<16)
#define RBIOCTL(x)	(RBIOC|x)
#define RBIOCTLN(x)	(x & 0xffff)

#define RB_CREATE	RBIOCTL(1)	/* Create a new ring buffer */
#define RB_ATTACH	RBIOCTL(2)	/* Attach to an existing ring buffer */
#define RB_NOTIFY	RBIOCTL(3)	/* Signal a change in ring buffer status */

/*
 * Argument to RB_CREATE call
 */
struct dmrbcreate_s {
	int	headSize;	/* must be >= sizeof dmrbhead */
	int	mode;		/* Attachment permissions */
	int	frameSize;	/* Size per element if fixed */

	int	numInfoBlocks;
	int	infoSize;	/* Size of info struct */
	int	dataSize;	/* Total size of data if not fixed element size */
	int	wrapAllowed;
	int	alignSize;
};

#ifdef _KERNEL
int	dmrb_open(int *);
int	dmrb_close(int);
int	dmrb_create(int, struct dmrbcreate_s *, struct cred *, int *);
int	dmrb_attach(int, int, struct cred *, int *);
int	dmrb_notify(int);
void	dmrb_callback(int, void (*)(int,void*), void *,
						void (*)(int,void*), void *);
int	dmrb_getdone(int);
int	dmrb_getframesize(int);
int	dmrb_getfree(int);
int	dmrb_getinfocount(int);
int	dmrb_getnewfree(int);
int	dmrb_getnewvalid(int);
int	dmrb_getnextfree(int);
int	dmrb_getvalid(int);
void	dmrb_putvalid(int);
void *	dmrb_getaddress(int, int);
#endif
