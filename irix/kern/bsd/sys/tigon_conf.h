/*
 * COPYRIGHT NOTICE
 * Copyright (c) Alteon Networks, Inc. 1996
 * All rights reserved
 */
/*
 * HISTORY
 * $Log: tigon_conf.h,v $
 * Revision 1.2  1999/05/18 21:44:02  trp
 * Upgrade to firmware level 12.3.9
 *
 * Revision 1.1.5.15  1998/09/30  18:49:40  shuang
 * 	change for new 12.3 API
 * 	[1998/09/30  18:39:11  shuang]
 *
 * Revision 1.1.5.14  1998/08/27  23:22:39  shuang
 * 	add new mcast commands, mbox commands, interrupt masks
 * 	[1998/08/27  23:16:45  shuang]
 * 
 * Revision 1.1.5.13  1998/07/31  04:13:16  shuang
 * 	dynamic NIC memory support flag should exclude TIGON REV4
 * 	[1998/07/31  04:11:54  shuang]
 * 
 * Revision 1.1.5.12  1998/07/29  02:11:36  shuang
 * 	enable dynamic memory support
 * 	[1998/07/29  02:10:45  shuang]
 * 
 * Revision 1.1.5.11  1998/06/23  01:29:26  shuang
 * 	added dynamic nic memory support.
 * 	[1998/06/23  01:03:32  shuang]
 * 
 * Revision 1.1.5.10  1998/06/08  23:55:12  ted
 * 	Snapshot NT generification phase 1
 * 	[1998/06/04  19:52:00  ted]
 * 
 * Revision 1.1.5.9  1998/03/28  00:53:15  shuang
 * 	Define TG_STACK_PAGE_ALIGN_WORDS for ext stack alignment
 * 	[1998/03/28  00:47:43  shuang]
 * 
 * Revision 1.1.5.8  1998/03/27  18:24:56  shuang
 * 	Add VOLATILE for tg_mbox_t
 * 	[1998/03/27  18:19:47  shuang]
 * 
 * Revision 1.1.5.7  1998/01/28  21:27:52  hayes
 * 	add STACK_REDZONE_SIZE and NIC_MEMSIZE for stack checking
 * 	[1998/01/28  21:25:28  hayes]
 * 
 * Revision 1.1.5.6  1997/12/16  23:04:42  skapur
 * 	Increased the size of RX buffer size
 * 	[1997/12/16  22:51:51  skapur]
 * 
 * Revision 1.1.5.5  1997/11/19  00:13:42  ted
 * 	merged from release 2.0
 * 	[1997/11/19  00:10:37  ted]
 * 
 * Revision 1.1.5.4  1997/11/14  19:16:11  hayes
 * 	first checkin after mongo merge...
 * 	[1997/11/14  19:12:10  hayes]
 * 
 * Revision 1.1.5.3  1997/07/25  22:58:50  ted
 * 	Architectural performance change
 * 	[1997/07/24  21:44:55  ted]
 * 
 * Revision 1.1.5.2  1997/03/28  05:18:18  ted
 * 	Crank the receive buffer to 512K to minimize packet loss
 * 	[1997/03/28  05:18:01  ted]
 * 
 * Revision 1.1.5.1  1997/03/24  08:34:13  ted
 * 	New event ring handling, producer lives in host memory, misc. other more modest changes
 * 	[1997/03/24  08:27:27  ted]
 * 
 * Revision 1.1.2.3  1996/11/24  04:02:32  ted
 * 	Remove bogus stuff left in during debug
 * 	[1996/11/24  03:59:59  ted]
 * 
 * Revision 1.1.2.2  1996/11/22  23:15:12  ted
 * 	Put check for #if defined(U64) on hostaddr_t typedef
 * 	[1996/11/22  23:12:19  ted]
 * 
 * Revision 1.1.2.1  1996/11/20  20:04:08  ted
 * 	New file that works with the new tg.h structure and includes pieces of
 * 	tg_def.h (defunct).
 * 	[1996/11/20  19:46:44  ted]
 * 
 * Revision 1.15.2.2  1996/11/15  04:23:56  hayes
 * 	Temporarily fix so it will work with the new header files structure.
 * 	This requires the new define "NEW_H_STRUCT" which is set in nic_conf.h.
 * 	This should be fixed when the host side gets reconciled with the new header
 * 	file structure.
 * 	[1996/11/15  04:23:37  hayes]
 * 
 * Revision 1.15.2.1  1996/11/01  18:24:40  ted
 * 	Add copyright notice and history
 * 	[1996/10/30  00:49:46  ted]
 * 
 * $EndLog$
 */
/*
 * FILE nic_conf.h
 *
 * COPYRIGHT (c) Essential Communication Corp. 1995
 *
 * Confidential to Alteon Networks, Inc.
 * Do not copy without Authorization
 *
 * $Source: /proj/irix6.5.7m/isms/irix/kern/bsd/sys/RCS/tigon_conf.h,v $
 * $Revision: 1.2 $ $Locker:  $
 * $Date: 1999/05/18 21:44:02 $ $Author: trp $ $State: Exp $
 */

#ifndef _NIC_CONF_H_
#define _NIC_CONF_H_

/*
 * configuration definition file
 */
#define TG_DMA_DESCR_CHAINED	1
#define TG_DMA_DESCR_FMT	1	/* long format */

/* for REDZONE and STACK checking code */
#define NIC_MEMSIZE		0x100000	/* 1 MB */
#define STACK_REDZONE_SIZE	64		/* words */

/* dynamic nic memory support */
#define	DYNAMIC_NIC_MEM		1	/* support dynamic nic memory */

/* WARNING:
 * the size of stack_page_align_padding[] in tg.h tg_data_t structure
 * NEED to change by changing TG_STACK_PAGE_ALIGN_WORDS
 * if any of the following buffer size is modified.
 */
#if (TIGON_REV & TIGON_REV4)

#define TG_RX_BUF_SIZE		(1024 * 700)
#define TG_TX_BUF_SIZE		(1024 * 64)

#else /* (TIGON_REV & TIGON_REV4) */

#if (DYNAMIC_NIC_MEM)
/* mac buffers are dynamically allocated in run time */
#define TG_RX_BUF_SIZE		(0)
#define TG_TX_BUF_SIZE		(0)
#else /* (DYNAMIC_NIC_MEM) */
/* get better performance when TX and RX buffers reside in a single bank */
#define TG_RX_BUF_SIZE		(1024 * (512 - 64))
#define TG_TX_BUF_SIZE		(1024 * 64)
#endif /* (DYNAMIC_NIC_MEM) */

#endif /* (TIGON_REV & TIGON_REV4) */

/* padding words for stack_page_align_padding[] in tg.h */
#define	TG_STACK_PAGE_ALIGN_WORDS	(512)	/* 2KB */

typedef struct tg_mbox {
    VOLATILE U32 mbox_high;
    VOLATILE U32 mbox_low;
} tg_mbox_t;

#define mbox_command_prod_index         mailbox[1].mbox_low
#define mbox_send_prod_index            mailbox[2].mbox_low
#define mbox_recv_prod_index            mailbox[3].mbox_low
#define mbox_recv_jumbo_prod_index      mailbox[4].mbox_low
#define mbox_recv_mini_prod_index       mailbox[5].mbox_low
/* backward compatable */
#define command_prod_index		mbox_command_prod_index
#define sendProdIndex			mbox_send_prod_index


/*
 * mac address format definition...
 */

#if ALT_BIG_ENDIAN
typedef struct macaddr {
    U16 reserved;
    U8 octet[6];
} tg_macaddr_t;
#else /* ALT_BIG_ENDIAN */
typedef struct macaddr {
    U8 octet1[2];
    U16 reserved;
    U8 octet2[4];
} tg_macaddr_t;
#endif /* ALT_BIG_ENDIAN */

/*
 * host addresses can be 64 bits or 32 bits wide.  To deal with
 * these variations we have a typedef and a define.  The typedef
 * (hostaddr) is used for building the structures.  This way the
 * space allocated is always 64 bits, even if the top 32 aren't used.
 * This provides the flexibility to allow the firmware (which shares
 * this include file with the host software) to always treat the
 * addresses as 64 bit quantities.  The define is used whenever the
 * actual structure value is used.  For the firmware (since it
 * always understands U64 the HOSTADDR define doesn't need to be
 * used, but it doesn't hurt.
 *
 * Also there is a HOSTADDR_TYPE defined for ease of assignemnt in
 * casting situations.
 *
 * Here's an example:
 * struct foo {
 *     hostaddr bar_addr;
 * };
 *      
 * struct foo *fp;
 * caddr_t barp;
 * barp = (caddr_t)fp->HOSTADDR(bar_addr);
 *
 * on 64 bit pointer systems it generates
 * struct foo {
 *     U64 bar_addr;
 * };
 *
 * struct foo *fp;
 * caddr_t barp;
 * barp = (caddr_t) fp->bar_addr;
 *
 * on 32 bit pointer systems it generates
 * struct foo {
 *     struct hostaddr bar_addr;
 * };
 *
 * struct foo *fp;
 * caddr_t barp;
 * barp = (caddr_t) fp->bar_addr.addr_lo;
 *
 * Notice that on 32 bit pointer systems it is important for the host
 * (which generates all the addresses) to zero the top 32 bits.
 */

#if defined(U64) && PTRS_ARE_64
typedef U64 tg_hostaddr_t;
#define HOSTADDR(x) x
#define HOSTADDR_TYPE U64
#define HOSTZEROHI(x)
#define SHMEMZEROHI(x)
#else /* PTRS_ARE_64 */
typedef struct tg_hostaddr {
    U32 addr_hi;
    U32 addr_lo;
} tg_hostaddr_t;
#define HOSTADDR(x) x.addr_lo
#define HOSTADDR_TYPE U32
#define HOSTZEROHI(x) x.addr_hi = 0
#define SHMEMZEROHI(x) WRSHMEM(x.addr_hi, 0)
#endif /* defined(U64) && PTRS_ARE_64 */

#endif /* _NIC_CONF_H_ */
