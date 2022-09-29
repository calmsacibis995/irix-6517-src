#ident	"lib/libsk/io/mem.c:  $Revision: 1.28 $"

/*
 * mem.c -- memory pseudo-device
 * Add mem(addr,width,am) device
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/vmereg.h>
#include <saio.h>

#include <arcs/eiob.h>
#include <arcs/cfgtree.h>

#include <libsc.h>
#include <libsk.h>

static struct am_table {
	unsigned at_am;			/* address modifier */
	__scunsigned_t at_base;		/* physical base address */
	__scunsigned_t at_maxaddr;	/* max address in region, 0 => eot */
} am_table[] = {
#if _MIPS_SIM != _ABI64
	{ 0,		0,		0xffffffff	}, /* default */
#else
	{ 0,		0,	0xffffffffffffffff	}, /* default */
#endif
	{ 0,		0,		0		}
};

static struct am_table *find_at(LONG);

static int mem_copy(__scunsigned_t src, __scunsigned_t dst,
		unsigned bcnt, int width);

static int
_memopen(IOBLOCK *iob)
{
	register struct am_table *at;
	register int width;

	at = find_at(iob->Partition);
	if (at == NULL) {
		printf("unsupported address modifier\n");
		goto bad;
	}
	width = iob->Unit;
	if (width == 0)
		iob->Unit = width = sizeof(int);
	if (width != sizeof(char) && width != sizeof(short)
	    && width != sizeof(int)) {
		printf("illegal width\n");
		goto bad;
	}
	if (iob->Controller > at->at_maxaddr) {
		printf("base address outside region\n");
		goto bad;
	}
	iob->DevPtr = (void *)at;
	return(ESUCCESS);

bad:
	return(iob->ErrorNumber = ENXIO);
}

static int
_memstrategy(IOBLOCK *iob, int func)
{
	register struct am_table *at;
	int cc;

	at = (struct am_table *)iob->DevPtr;
	if ((__scunsigned_t)iob->Controller + iob->Count > at->at_maxaddr) {
		return(iob->ErrorNumber = EIO);
	}

	if (func == READ)
		cc = mem_copy(iob->Controller + at->at_base,
			      (__scunsigned_t)iob->Address,
			      iob->Count, iob->Unit);
	else if (func == WRITE)
		cc = mem_copy((__scunsigned_t)iob->Address,
			      iob->Controller + at->at_base,
			      iob->Count, iob->Unit);
	else {
		return(iob->ErrorNumber = EINVAL);
	}
	if (cc > 0)
		iob->Controller += cc;
	return(ESUCCESS);
}

static int
mem_copy(__scunsigned_t src, __scunsigned_t dst, unsigned bcnt, int width)
{
	volatile unsigned bytes = bcnt;

	switch (width) {

	case sizeof(int):
		if (((dst ^ src) & (sizeof(int)-1)) == 0) { /* alignable */
			while (bytes && (src & (sizeof(int)-1))) {
				*(char *)dst = *(char *)src;
				wbflush();
				src += sizeof(char);
				dst += sizeof(char);
				bytes--;
			}
			while (bytes >= sizeof(int)) {
				*(int *)dst = *(int *)src;
				wbflush();
				src += sizeof(int);
				dst += sizeof(int);
				bytes -= sizeof(int);
			}
		}
		/* fall into */

	case sizeof(short):
		if (((dst ^ src) & (sizeof(short)-1)) == 0) {
			while (bytes && (src & (sizeof(short)-1))) {
				*(char *)dst = *(char *)src;
				wbflush();
				src += sizeof(char);
				dst += sizeof(char);
				bytes--;
			}
			while (bytes >= sizeof(short)) {
				*(short *)dst = *(short *)src;
				wbflush();
				src += sizeof(short);
				dst += sizeof(short);
				bytes -= sizeof(short);
			}
		}
		/* fall into */

	case sizeof(char):
		while (bytes) {
			*(char *)dst = *(char *)src;
			wbflush();
			src += sizeof(char);
			dst += sizeof(char);
			bytes--;
		}
		break;

	default:
		return(0);
	}
	return(bcnt);
}

static struct am_table *
find_at(LONG am)
{
	register struct am_table *at;

	for (at = am_table; at->at_maxaddr; at++)
		if (at->at_am == am)
			return(at);
	return(NULL);
}

/* ARCS - new stuff */

STATUS
_mem_strat(COMPONENT *self, IOBLOCK *iob)
{
	switch (iob->FunctionCode) {
	case FC_INITIALIZE:
		return (ESUCCESS);
	case FC_OPEN:
		return (_memopen (iob));
	case FC_READ:
		return (_memstrategy (iob, READ)); 
	case FC_WRITE:
		return (_memstrategy (iob, WRITE)); 
	case FC_CLOSE:	
		free(self);
		return (ESUCCESS);
	default:
		return (iob->ErrorNumber = EINVAL);
	};
}

int
memsetup(cfgnode_t **cfg, struct eiob *eiob, char *arg)
{
	__psint_t val[3];
	cfgnode_t *n;
	char *p;

	/* parse "addr,width,accessmode)" */
	p = parseargs(arg,3,val);
	if ((p == 0) || (*p != '\0'))
		return(EINVAL);

	n = (cfgnode_t *)malloc(sizeof(cfgnode_t));
	if (!n) return(ENOMEM);

	bzero(n,sizeof(cfgnode_t));
	n->driver = _mem_strat;
	n->comp.Class = PeripheralClass;
	n->comp.Type = OtherPeripheral;
	n->comp.Flags = (IDENTIFIERFLAG)(Input|Output);
	n->comp.Version = SGI_ARCS_VERS;
	n->comp.Revision = SGI_ARCS_REV;

	*cfg = n;
	eiob->iob.Controller = val[0];
	eiob->iob.Unit = val[1];
	eiob->iob.Partition = val[2];

	return(0);
}
