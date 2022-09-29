#include <sys/types.h>
#include <sys/PCI/ioc3.h>
#include <sys/serialio.h>
#include <sys/mman.h>
#include <malloc.h>
#include <unistd.h>
#include "usio_common.h"

/* a ring buffer entry */
struct ring_entry {
    union {
	struct {
	    __uint32_t alldata;
	    __uint32_t allsc;
	} all;
	struct {
	    char data[4];	/* data bytes */
	    char sc[4];		/* status/control */
	} s;
    } u;
};

/* test the valid bits in any of the 4 sc chars using "allsc" member */
#define RING_ANY_VALID \
	((__uint32_t)(RXSB_MODEM_VALID | RXSB_DATA_VALID) * 0x01010101)

#define ring_sc u.s.sc
#define ring_data u.s.data
#define ring_allsc u.all.allsc

struct usio_ioc3_private {
    struct usio_vec *usio_vec; /* must be first member! */
    
    caddr_t write_ring;
    caddr_t read_ring;
    int ring_size;
    int ncs;

    ioc3_sregs_t *sregs;
};

static int usio_ioc3_read(void *, char *, int);
static int usio_ioc3_write(void *, char *, int);
static int usio_ioc3_get_status(void *);

static struct usio_vec ioc3_vec = {
    usio_ioc3_read,
    usio_ioc3_write,
    usio_ioc3_get_status
};

#define PVT(p) ((struct usio_ioc3_private*)(p))

void *
usio_ioc3_init(int fd)
{
    struct ioc3_mapid mapid;
    caddr_t dmamap, piomap;
    struct usio_ioc3_private *private;

    if (ioctl(fd, SIOC_USIO_GET_ARGS, &mapid) < 0)
	return(0);

    if ((dmamap = (caddr_t)mmap(0, getpagesize(),
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE, fd,
				USIO_MAP_IOC3_DMABUF)) == (caddr_t)-1L ||
	(piomap = (caddr_t)mmap(0, getpagesize(),
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE, fd,
				USIO_MAP_IOC3_REG)) ==	(caddr_t)-1L)
	return(0);

    if ((private = (struct usio_ioc3_private*)
	 malloc(sizeof(struct usio_ioc3_private))) == 0)
	return(0);

    private->usio_vec = &ioc3_vec;

    if (mapid.port == 0) {
	/* port A */
	private->write_ring = dmamap;
	private->read_ring = dmamap + mapid.size;
    }
    else {
	/* port B */
	private->write_ring = dmamap + 2 * mapid.size;
	private->read_ring = dmamap + 3 * mapid.size;
    }
    private->ring_size = mapid.size;
    private->sregs = (ioc3_sregs_t*)piomap;
    private->ncs = 0;

    /* set up initial hardware state: enable DMA */
    /* need to use ioctl to keep driver software copy in sync */
    if (ioctl(fd, SIOC_USIO_SET_SSCR, SSCR_DMA_EN) < 0)
	return(0);

    /* RX timer should be zero so we never leave any data hanging in
     * the construction buffers
     */
    private->sregs->srtr = 0;

    /* clear out the TX and RX rings */
    private->sregs->stpir = private->sregs->stcir;
    private->sregs->srcir = private->sregs->srpir;

    return(private);
}

static int
usio_ioc3_read(void *p, char *buf, int len)
{
    struct usio_ioc3_private *pvt = PVT(p);
    ioc3_sregs_t *regs = pvt->sregs;
    caddr_t ring = pvt->read_ring;
    struct ring_entry *entry;
    int mask = pvt->ring_size - 1;
    int prod, cons, x, total = 0;

    prod = regs->srpir & mask;
    cons = regs->srcir & mask;

    pvt->ncs = 0;

    while(prod != cons && len > 0) {
	entry = (struct ring_entry*) (ring + cons);

	/* If no bits are valid, the PIO may have bypassed the DMA.
	 * Since this is by definition strictly polled input, we can
	 * just wait for the next call.
	 */
	if ((entry->ring_allsc & RING_ANY_VALID) == 0)
	    break;

	for(x = 0; x < 4 && len > 0; x++) {
	    char *sc = &(entry->ring_sc[x]);

	    if (*sc & (RXSB_OVERRUN | RXSB_PAR_ERR |
		       RXSB_FRAME_ERR | RXSB_BREAK)) {
		if (total > 0) {
		    len = 0;
		    break;
		}
		else {
		    if ((*sc & (RXSB_OVERRUN | RXSB_MODEM_VALID)) ==
			(RXSB_OVERRUN | RXSB_MODEM_VALID))
			pvt->ncs |= USIO_ERR_OVERRUN;
		    if ((*sc & (RXSB_PAR_ERR | RXSB_DATA_VALID)) ==
			(RXSB_PAR_ERR | RXSB_DATA_VALID))
			pvt->ncs |= USIO_ERR_PARITY;
		    if ((*sc & (RXSB_FRAME_ERR | RXSB_DATA_VALID)) ==
			(RXSB_FRAME_ERR | RXSB_DATA_VALID))
			pvt->ncs |= USIO_ERR_FRAMING;
		    if ((*sc & (RXSB_BREAK | RXSB_DATA_VALID)) ==
			(RXSB_BREAK | RXSB_DATA_VALID))
			pvt->ncs |= USIO_BREAK;
		    len = 1;
		}
	    }

	    if (*sc & RXSB_DATA_VALID) {
		*buf++ = entry->ring_data[x];
		len--;
		total++;
	    }

	    *sc &= ~(RXSB_MODEM_VALID | RXSB_DATA_VALID);
	}

	/* if we used up this entry entirely, go on to the next one,
	 * otherwise we must have run out of buffer space, so
	 * leave the consumer pointer here for the next read in case
	 * there are still unread bytes in this entry
	 */
	if ((entry->ring_allsc & RING_ANY_VALID) == 0)
	    cons = (cons + (int)sizeof(struct ring_entry)) & mask;
    }

    /* if we didn't read anything, there may be some trailing data
     * left in the construction buffers. Drain it now.
     */
    if (total == 0)
	regs->sscr |= SSCR_RX_DRAIN;

    regs->srcir = cons;
    return(total);
}

/*ARGSUSED*/
static int
usio_ioc3_write(void *p, char *buf, int len)
{
    ioc3_sregs_t *regs = PVT(p)->sregs;
    caddr_t ring = PVT(p)->write_ring;
    struct ring_entry *entry;
    int mask = PVT(p)->ring_size - 1;
    int prod, cons, olen = len, x;

    prod = regs->stpir & mask;
    cons = regs->stcir & mask;

    /* maintain a 1-entry red-zone. The ring buffer is full when
     * (cons - prod) % ring_size is 1. Rather than do this subtraction
     * in the body of the loop, I'll do it now.
     */
    cons = (cons - (int)sizeof(struct ring_entry)) & mask;

    while (prod != cons && len > 0) {
	entry = (struct ring_entry*) (ring + prod);

	/* invalidate all entries */
	entry->ring_allsc = 0;

	/* copy in some bytes */
	for(x = 0; x < 4 && len > 0; x++) {
	    entry->ring_data[x] = *buf++;
	    entry->ring_sc[x] = TXCB_VALID;
	    len--;
	}
    
	/* go on to next entry */
	prod = (prod + (int)sizeof(struct ring_entry)) & mask;
    }

    regs->stpir = prod;
    return(olen - len);
}

static int
usio_ioc3_get_status(void *p)
{
    return(PVT(p)->ncs);
}
