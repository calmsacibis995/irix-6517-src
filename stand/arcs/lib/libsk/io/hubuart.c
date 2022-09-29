#ident "lib/libsk/io/hubuart.c: $Revision: 1.20 $"

/* SN0 IP27 HUB UART driver */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/i8251uart.h>
#include <arcs/folder.h>
#include <tty.h>
#include <saio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/hinv.h>
#include <arcs/errno.h>
#include <sys/SN/klconfig.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <promgraph.h>
#include <pgdrv.h>

#ifndef SN_PDI

extern graph_hdl_t prom_graph_hdl ;
extern vertex_hdl_t hw_vertex_hdl ;

#define HUB_WRITE_CMD(_x) 	*(volatile long *)KL_UART_CMD = (_x)
#define HUB_READ_CMD 		*(volatile long *)KL_UART_CMD
#define WRITE_DATA(_x)		*(volatile long *)KL_UART_DATA = (_x)
#define READ_DATA		*(volatile long *)KL_UART_DATA

/* Device buffer for HUB UART */
static struct device_buf dbvalue;
static struct device_buf *db;
static int hubgetc(void);
static void hubputc(int);
static int _hubuartinit(void);

int kl_hub_uart_AddChild(vertex_hdl_t, int(*)()) ;

/*
 * Initialize the IP27 HUB UART
 */
static int
_hubuartinit(void)
{
    /* Initialize HUB UART -- Probably okay, but don't do it again
    HUB_WRITE_CMD(0);
    HUB_WRITE_CMD(0);
    HUB_WRITE_CMD(0);
    HUB_WRITE_CMD(I8251_RESET);
    HUB_WRITE_CMD(I8251_ASYNC16X | I8251_NOPAR | I8251_8BITS | I8251_STOPB1);
    HUB_WRITE_CMD(I8251_TXENB | I8251_RXENB | I8251_RESETERR);
    */

    printf("Hubuart: Init ...\n") ;

    db = &dbvalue;
    bzero(db, sizeof(struct device_buf));
    CIRC_FLUSH(db);

    return 0;
}


/*
 * _hubuartopen()
 *	Opens the hub uart for I/O.  We don't even try to support multiple
 * 	HUB UARTS right now.
 */

int
_hubuartopen(IOBLOCK *iob)
{
    if (iob->Controller != 0)
	return (iob->ErrorNumber = ENXIO);

    iob->Flags |= F_SCAN;

    return ESUCCESS;
}


/*
 * _hubuartstrategy()
 *	Actually deal with I/O
 */

int 
_hubuartstrategy(IOBLOCK *iob, int func)
{
    register int c;
    long	 ocnt = iob->Count;
    char	*addr = (char*)iob->Address;


    if (func == READ) {

	while (iob->Count > 0) {
	    
	    while (c = hubgetc()) {
		_ttyinput(db, c);
	    }
	
	    if ((iob->Flags & F_NBLOCK) == 0) {
		while (CIRC_EMPTY(db))
		    _scandevs();
	    }

	    if (CIRC_EMPTY(db)) {
		iob->Count = ocnt - iob->Count;
		return ESUCCESS;
	    }

	    c = _circ_getc(db);	
	    *addr++ = c;
	    iob->Count--;
	}

	iob->Count = ocnt;
	return ESUCCESS;

    } else if (func == WRITE) {
	while (iob->Count-- > 0)
	    hubputc(*addr++);
	iob->Count = ocnt;
#ifdef TESTING
	hubputc('&') ; /* For testing IO6prom */
#endif
	return ESUCCESS;

    } else {
	return (iob->ErrorNumber = EINVAL);
    }
}


void
_hubuartpoll(IOBLOCK *iob)
{
    int c;

    while (c = hubgetc()) {
	_ttyinput(db, c);
    }
    iob->ErrorNumber = ESUCCESS;
}


/*
 * _hubuartioctl()
 *	Do nasty device-specific things 
 */

int
_hubuartioctl(IOBLOCK *iob)
{
    int retval = 0;

    switch ((long)iob->IoctlCmd) {
      case TIOCRAW:
	if (iob->IoctlArg)
	    db->db_flags |= DB_RAW;
	else
	    db->db_flags &= ~DB_RAW;
	break;
    
      case TIOCRRAW:
	if (iob->IoctlArg)
	    db->db_flags |= DB_RRAW;
	else
	    db->db_flags &= ~DB_RRAW;
	break;

      case TIOCFLUSH:
	CIRC_FLUSH(db);
	break;

      case TIOCREOPEN:
	retval = _hubuartopen(iob);
	break;

      case TIOCCHECKSTOP:
	while (CIRC_STOPPED(db))
	    _scandevs();
	break;

      default:
	return (iob->ErrorNumber = EINVAL);
    }

    return retval;
}

/*
 * _hubuartreadstat()
 *	Returns the status of the current read.
 */

static int
_hubuartreadstat(IOBLOCK *iob)
{
    iob->Count = _circ_nread(db);
    return (iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}


/*
 * _hubuart_strat
 *	ARCS-compatible strategy routine.
 */

/*ARGSUSED*/
int
_hubuart_strat(COMPONENT *self, IOBLOCK *iob)
{
    switch (iob->FunctionCode) {
      case FC_INITIALIZE:
	return _hubuartinit();

      case FC_OPEN:
	return _hubuartopen(iob);

      case FC_READ:
	return _hubuartstrategy(iob, READ);

      case FC_WRITE:
	return _hubuartstrategy(iob, WRITE);

      case FC_CLOSE:
	return 0;

      case FC_IOCTL:
	return _hubuartioctl(iob);

      case FC_GETREADSTATUS:
	return _hubuartreadstat(iob);

      case FC_POLL:
	_hubuartpoll(iob);
	return 0;

      default:
	return (iob->ErrorNumber = EINVAL);
    }
}


/*
 * hubgetc()
 *	If a character is waiting to be read, read it from the
 *	uart and return it.  Otherwise, return zero.
 */

static int
hubgetc(void)
{
    ulong data; 

    if (HUB_READ_CMD & I8251_RXRDY) {
	data = READ_DATA;
	return ((int)data);
    } else
	return 0;
}

/*
 * hubputc()
 *	Writes a character out.
 */

static void
hubputc(int c)
{
    while (CIRC_STOPPED(db))
	_scandevs();

    while (! (HUB_READ_CMD & I8251_TXRDY))
	_scandevs();

    WRITE_DATA(c);
}

/*
 * hubuart_install
 *	Add the HUB UART to the ARCS configuration tree.
 */

static COMPONENT ttytmpl = {
    PeripheralClass,			/* Class */
    SerialController,			/* Controller */
    (IDENTIFIERFLAG)(ConsoleIn|ConsoleOut|Input|Output), /* Flags */
    SGI_ARCS_VERS,			/* Version */
    SGI_ARCS_REV,			/* Revision */
    6,					/* Key */
    0x01,				/* Affinity */
    0,					/* ConfigurationDataSize */
    10,					/* IdentifierLength */
    "SN0 TTY"				/* Identifier */
};

#endif /* !SN_PDI */

int
hubuart_install(COMPONENT *top)
{
#ifndef SN_PDI

    COMPONENT *ctrl;

    (void) _hubuartinit();

    ctrl = AddChild(top,&ttytmpl,(void *)NULL);
    if (ctrl == (COMPONENT *)NULL) cpu_acpanic("serial");
    RegisterDriverStrategy(ctrl, _hubuart_strat);
#endif
    return(0);
}

#ifndef SN_PDI

#ifdef	SABLE
int
kl_hubuart_install(vertex_hdl_t hw_vhdl, vertex_hdl_t self_vhdl)
{
	graph_error_t graph_err ;
	prom_dev_info_t *dev_info;
	klinfo_t 	*klcompt ;

	printf("Hubuart: install...\n") ;

        graph_err = graph_info_get_LBL(prom_graph_hdl, 
					self_vhdl, INFO_LBL_DEV_INFO,NULL,
                        		(arbitrary_info_t *)&dev_info) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("getLBL for hubuart err %d\n", graph_err) ;
                return 0 ;
        }
	klcompt = (klinfo_t *)(dev_info->kl_comp);
	klcompt->arcs_compt  = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
	if (klcompt->arcs_compt == NULL) {
		printf("Hubuart inst : malloc failed \n") ;
		return 0 ;
	}
	bcopy((char *)&ttytmpl, (char *)klcompt->arcs_compt, 
		sizeof(COMPONENT)) ;

	kl_reg_drv_strat(dev_info, _hubuart_strat) ;

        kl_hub_uart_AddChild(self_vhdl, _hubuart_strat) ;

	return 0 ;
}

kl_hub_uart_AddChild(vertex_hdl_t devctlr_vhdl, int(*dev_strat)())
{
        klinfo_t        *klinf_ptr, *klcompt ;
        graph_error_t graph_err ;

        klinf_ptr = (klinfo_t *)init_device_graph(devctlr_vhdl, 
						  KLSTRUCT_HUB_UART) ;
        if (!klinf_ptr)
                panic("kl_hub_uart_AddChild: Cannot allocate any more memory\n") ;

        graph_err = graph_info_get_LBL(prom_graph_hdl, 
				       devctlr_vhdl, INFO_LBL_KLCFG_INFO,NULL,
                        	       (arbitrary_info_t *)&klcompt) ;
        if (graph_err != GRAPH_SUCCESS) {
                printf("getLBL for hub uart err %d\n", graph_err) ;
                return 0 ;
        }

	klinf_ptr->arcs_compt = klcompt->arcs_compt ;

        link_device_to_graph(hw_vertex_hdl, devctlr_vhdl,
                             (klinfo_t *)klinf_ptr, dev_strat) ;

        return 1 ;

}
#else /* !SABLE */
/* ARGSUSED */
int
kl_hubuart_install(vertex_hdl_t hw_vhdl, vertex_hdl_t self_vhdl)
{
	return 0;

}
#endif /* !SABLE */
#endif /* !SN_PDI */
