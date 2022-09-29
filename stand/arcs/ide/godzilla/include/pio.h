#if !defined(__PIO_H__)
#define  __PIO_H__

/* The following addresses are unmapped and uncached */

#define PLP_BASE        0xBF380000  /* Point at ls byte */
#define ALT_PLP_BASE    0xBF388000  /* Point at ls byte */

/* Valid for all modes, except ECP */
#ifdef notdef
#define PLP_DATA        (*(volatile unsigned char*)(PLP_BASE))
#else /* notdef */
#define PLP_DATA_ADDR	((volatile unsigned char*)(PLP_BASE))
#define PLP_DATA	(READ_REG64(PLP_DATA_ADDR, long long))
#define SET_PLP_DATA(data)	(WRITE_REG64(data,PLP_DATA_ADDR, long long))
#endif /* notdef */

/* Valid for ECP mode only */
#ifdef notdef
#define ECP_ADDR_FIFO   (*(volatile unsigned char*)(PLP_BASE))
#endif /* notdef */

/* Parallel Port Status Register, valid for all modes */
#define PLP_STATUS      (*(volatile long long*) (PLP_BASE + 0x100))
#define PLP_BUSY        0x80
#define PLP_ACK         0x40
#define PLP_PE          0x20
#define PLP_SLCT        0x10
#define PLP_ERR         0x08
#define PLP_PRINT       0x04
#define PLP_WRITE_FIFO  0x02 /* For testing only, actually rsvd */
#define PLP_READ_FIFO   0x01 /* For testing only, actually rsvd */

/* Parallel Port Control Register, valid for all modes */
#ifdef notdef
#define PLP_CONTROL	(*(volatile unsigned char*)(PLP_BASE + 0x200))
#else /* notdef */
#define PLP_CONTROL_ADDR	((volatile unsigned char*)(PLP_BASE + 0x200))
#define PLP_CONTROL	(READ_REG64(PLP_CONTROL_ADDR,long long))
#define SET_PLP_CONTROL(data)	(WRITE_REG64(data,PLP_CONTROL_ADDR,long long))
#endif /* notdef */
#define PLP_DIR         0x20
#define PLP_INT2EN      0x10
#define PLP_SLIN        0x08
#define PLP_INIT        0x04
#define PLP_AFD         0x02
#define PLP_STB         0x01

#ifdef notdef
/* Valid for EPP mode only */
#define EPP_ADDRESS     (*(volatile unsigned char*)(PLP_BASE + 0x300))
#define EPP_DATA        (*(volatile unsigned char*)(PLP_BASE + 0x400))

/* Valid for ECP mode only */
#define ECP_DATA_FIFO   (*(volatile unsigned char*)(ALT_PLP_BASE))

/* Valid for Configuration mode only */ 
#define PLP_CFNG_A      (*(volatile unsigned char*)(ALT_PLP_BASE))
#define PLP_CFNG_B      (*(volatile unsigned char*)(ALT_PLP_BASE + 0x100))
#endif /* notdef */

/* Parallel Port Extended Control Register */
#ifdef notdef
#define PLP_EXT_CNTRL   (*(volatile unsigned char*)(ALT_PLP_BASE + 0x200))
#else /* notdef */
#define PLP_EXT_CNTRL_ADDR	((volatile unsigned char*)(ALT_PLP_BASE + 0x200))
#define PLP_EXT_CNTRL	(READ_REG64(PLP_EXT_CNTRL_ADDR, long long))
#define SET_PLP_EXT_CNTRL(data)	(WRITE_REG64(data,PLP_EXT_CNTRL_ADDR,long long))
#endif /* notdef */
#define PLP_MODE        (PLP_EXT_CNTRL >> 5) 
#define PLP_SET_MODE(m) PLP_EXT_CNTRL=(PLP_EXT_CNTRL|((m&7)<<5))
#ifdef notdef
#define PLP_STD_MODE    0x0
#define PLP_BI_DIR_MODE 0x1
#define PLP_FIFO_MODE   0x2
#define PLP_ECP_MODE    0x3
#define PLP_EPP_MODE    0x4
#define PLP_TEST_MODE   0x6
#define PLP_CNFG_MODE   0x7
#else /*notdef */
#define PLP_STD_MODE    0x00
#define PLP_BI_DIR_MODE 0x20
#define PLP_FIFO_MODE   0x40
#define PLP_ECP_MODE    0x60
#define PLP_EPP_MODE    0x80
#define PLP_TEST_MODE   0xc0
#define PLP_CNFG_MODE   0xe0
#endif /*notdef*/
#define PLP_NERR_INTR   0x10
#define PLP_DMA_EN      0x08
#define PLP_SERV_INTR   0x04
#define PLP_FULL        0x02
#define PLP_EMPTY       0x01


#endif





































