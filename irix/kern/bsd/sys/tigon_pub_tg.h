/*
 * COPYRIGHT NOTICE
 * Copyright (c) Alteon Networks, Inc. 1996
 * All rights reserved
 */
/*
 * HISTORY
 * $Log: tigon_pub_tg.h,v $
 * Revision 1.2  1999/05/18 21:44:02  trp
 * Upgrade to firmware level 12.3.9
 *
 * Revision 1.1.6.70  1998/10/13  03:33:16  ted
 * 	Support Mini ring, compiles on Solaris
 * 	[1998/10/13  03:30:50  ted]
 *
 * Revision 1.1.6.69  1998/09/28  18:33:43  hayes
 * 	fix pciClass for little endian machines too...
 * 	[1998/09/28  18:33:33  hayes]
 * 
 * Revision 1.1.6.68  1998/09/12  19:48:25  wayne
 * 	Add comments that EVENT_SW0 cannot change; see libhw_api.h
 * 	[1998/09/12  19:46:08  wayne]
 * 
 * Revision 1.1.6.67  1998/09/02  21:00:28  ted
 * 	Add other mailboxes into publicly available section
 * 	[1998/09/02  20:51:15  ted]
 * 
 * Revision 1.1.6.66  1998/06/23  00:29:38  shuang
 * 	moved tigon revision definitions to tg_rev.h
 * 	[1998/06/23  00:25:01  shuang]
 * 
 * Revision 1.1.6.65  1998/06/16  16:45:22  ted
 * 	NT initialization cleanup/rewrite
 * 	[1998/06/16  16:42:44  ted]
 * 
 * Revision 1.1.6.64  1998/05/18  20:09:17  ted
 * 	Make TG_PCI_STATE_DMA_WR_ALL_ALIGN bit public
 * 	[1998/05/18  19:59:47  ted]
 * 
 * Revision 1.1.6.63  1998/04/11  20:04:05  wayne
 * 	Teach SUN_MP about TG_STACK_START_B for REV5
 * 	[1998/04/11  19:55:18  wayne]
 * 
 * Revision 1.1.6.62  1998/04/11  05:14:56  shuang
 * 	swap TG_FW_EVENT_NUM_XXX between TIGON_REV5 and !TIGON_REV5
 * 	[1998/04/11  04:54:20  shuang]
 * 
 * Revision 1.1.6.61  1998/03/28  00:53:13  shuang
 * 	Replace hard coded size of stack_page_align_padding[]
 * 	[1998/03/28  00:43:37  shuang]
 * 
 * Revision 1.1.6.60  1998/03/26  01:34:14  shuang
 * 	Added optional stack_page_align_padding[] in tg_data structure
 * 	[1998/03/26  01:06:56  shuang]
 * 
 * Revision 1.1.6.59  1998/03/09  18:47:31  hayes
 * 	remove pre-rev4 defines, make TAG_MASK visible
 * 	[1998/03/09  18:47:22  hayes]
 * 
 * Revision 1.1.6.58  1998/02/24  17:41:43  shuang
 * 	Modified mac_rx_mcast_filt[1234] fields with alias
 * 	[1998/02/24  17:30:43  shuang]
 * 
 * Revision 1.1.6.57  1998/01/28  21:27:51  hayes
 * 	add stack redzone area
 * 	[1998/01/28  21:24:34  hayes]
 * 
 * Revision 1.1.6.56  1998/01/24  04:06:39  venkat
 * 	added a shifting constant used in window manipulation
 * 	[1998/01/24  04:06:08  venkat]
 * 
 * Revision 1.1.6.55  1998/01/20  04:39:18  hayes
 * 	add tigon Rev6 support
 * 	[1998/01/20  04:37:10  hayes]
 * 
 * Revision 1.1.6.54  1998/01/07  18:49:05  ted
 * 	change class in tg_pci_conf to pciClass to avoid C++ reserved word
 * 	[1998/01/07  18:48:55  ted]
 * 
 * Revision 1.1.6.53  1998/01/06  20:03:51  ted
 * 	Fix public-only version of tg_mac_control
 * 	[1998/01/06  20:03:36  ted]
 * 
 * Revision 1.1.6.52  1997/12/19  07:21:48  hayes
 * 	add pci subsystem id and subsystem vendor id
 * 	[1997/12/19  07:21:32  hayes]
 * 
 * Revision 1.1.6.51  1997/11/25  23:59:18  ted
 * 	Fix overzealous public_only problem
 * 	[1997/11/25  23:59:09  ted]
 * 
 * Revision 1.1.6.50  1997/11/25  22:06:53  ted
 * 	Merged with changes from 1.1.6.49
 * 	[1997/11/25  22:06:37  ted]
 * 
 * 	Rearrange public only areas
 * 	[1997/11/25  21:57:21  ted]
 * 
 * Revision 1.1.6.49  1997/11/24  19:23:33  taylor
 * 	Rev6: disabled rev5 workarounds for rev6
 * 	[1997/11/24  19:22:52  taylor]
 * 
 * Revision 1.1.6.48  1997/11/19  19:22:08  ted
 * 	Support Tigon and Tigon2 in the same driver
 * 	[1997/11/19  19:15:08  ted]
 * 
 * Revision 1.1.6.47  1997/11/18  19:18:44  hayes
 * 	fix MISC bit change
 * 	[1997/11/18  19:18:32  hayes]
 * 
 * Revision 1.1.6.46  1997/11/18  01:33:48  hayes
 * 	swap definitions of MISC_0 and MISC_1 bits in Misc Local Control
 * 	register.
 * 	[1997/11/18  01:29:26  hayes]
 * 
 * Revision 1.1.6.45  1997/11/14  17:58:25  kyung
 * 	Rev5: fixed macstats layout
 * 	[1997/11/14  17:57:17  kyung]
 * 
 * Revision 1.1.6.44  1997/11/09  23:27:23  hayes
 * 	change name of LINK_READY bit to LINK_POLARITY
 * 	[1997/11/09  23:25:16  hayes]
 * 
 * Revision 1.1.6.43  1997/10/28  18:35:48  hayes
 * 	Remove bogus #defines...
 * 	[1997/10/28  18:35:16  hayes]
 * 
 * Revision 1.1.6.42  1997/10/27  23:13:17  taylor
 * 	Merged with changes from 1.1.6.41
 * 	[1997/10/27  23:12:59  taylor]
 * 
 * 	SLB: implemented web request redirection
 * 	[1997/10/27  23:09:12  taylor]
 * 
 * Revision 1.1.6.41  1997/10/27  22:59:53  hayes
 * 	Merged with changes from 1.1.6.40
 * 	[1997/10/27  22:59:40  hayes]
 * 
 * 	change TG_DATA_END for Tigon 2
 * 	[1997/10/27  22:58:31  hayes]
 * 
 * Revision 1.1.6.40  1997/10/24  02:22:28  taylor
 * 	SLB: additional hot-standby (failover) work
 * 	[1997/10/24  02:17:26  taylor]
 * 
 * Revision 1.1.6.39  1997/10/22  21:13:30  hayes
 * 	Fix up REV5 defines to work when both REV4 and REV5 are defined.
 * 	Add hack to support scratch pad access on second CPU via gdb.
 * 	[1997/10/22  21:13:15  hayes]
 * 
 * Revision 1.1.6.38  1997/10/21  00:56:13  ted
 * 	Add instruction cache flush bit to CPU_STATE bit definitions
 * 	[1997/10/21  00:56:02  ted]
 * 
 * Revision 1.1.6.37  1997/10/16  23:42:04  hayes
 * 	cleanup REV4 Tigon defines
 * 	[1997/10/16  23:40:14  hayes]
 * 
 * Revision 1.1.6.36  1997/10/11  16:57:03  hayes
 * 	fixed up public definitions so that pub_tg.h works for HOST code
 * 	[1997/10/11  16:56:10  hayes]
 * 
 * Revision 1.1.6.35  1997/10/10  20:33:17  hayes
 * 	fix compiler warnings, cleanup board dependant defines
 * 	[1997/10/10  20:32:25  hayes]
 * 
 * Revision 1.1.6.34  1997/10/10  08:43:40  tibor
 * 	need to keep LADDR def
 * 	[1997/10/10  08:43:22  tibor]
 * 
 * Revision 1.1.6.33  1997/10/10  06:37:51  tibor
 * 	changed misc-bits define names for rev5
 * 	[1997/10/10  06:24:00  tibor]
 * 
 * Revision 1.1.6.32  1997/09/12  01:46:30  taylor
 * 	For Rev5: TG_END_SRAM and TG_DATA_END now 0x800000
 * 	[1997/09/12  01:33:53  taylor]
 * 
 * Revision 1.1.6.31  1997/09/08  21:39:09  hayes
 * 	add definitions for mac_serdes_config regisater
 * 	[1997/09/08  21:38:55  hayes]
 * 
 * 	Added defines for rev5 tx mac descrs
 * 	[1997/09/05  01:15:46  taylor]
 * 
 * Revision 1.1.6.30  1997/09/05  01:17:14  taylor
 * 	Added defines for rev5 tx mac descrs
 * 
 * Revision 1.1.6.29  1997/08/15  23:51:51  taylor
 * 	Changed TG_MAC_TX_STATE_ENA_BUF_CMP to TG_MAC_TX_STATE_DIS_BUF_CMP
 * 	[1997/08/15  18:40:49  taylor]
 * 
 * Revision 1.1.6.28  1997/07/25  19:08:29  taylor
 * 	Tigon rev5 support
 * 	[1997/07/25  18:49:01  taylor]
 * 
 * Revision 1.1.6.27  1997/07/19  20:39:22  ted
 * 	Remove #error pragma because cppp doesn't understand it.
 * 	[1997/07/19  20:39:10  ted]
 * 
 * Revision 1.1.6.26  1997/07/18  17:14:25  tibor
 * 	Another off by 12 byte problem
 * 	[1997/07/18  17:14:02  tibor]
 * 
 * Revision 1.1.6.25  1997/07/18  02:38:28  tibor
 * 	Removed extra ifdef
 * 	[1997/07/18  02:38:02  tibor]
 * 
 * Revision 1.1.6.24  1997/07/18  00:08:17  tibor
 * 	Merged with changes from 1.1.6.23
 * 	[1997/07/18  00:08:05  tibor]
 * 
 * 	TIGON_REV4/5 mods by skapur
 * 	[1997/07/18  00:00:26  tibor]
 * 
 * Revision 1.1.6.23  1997/07/09  19:32:31  skapur
 * 	Changed the mac_control structure; the element mac_mcast_addr is
 * 	     changed to mac_mcast_filter; In addition to changing the name, its
 * 	     type is changed from tg_macaddr_t to U32. The reserved_1 filed is
 * 	     appropriatly adjusted.
 * 	[1997/07/09  19:30:39  skapur]
 * 
 * Revision 1.1.6.22  1997/06/03  01:35:04  davis
 * 	Added cpu_priority defines.
 * 	[1997/06/03  01:34:02  davis]
 * 
 * Revision 1.1.6.21  1997/05/21  16:37:23  ted
 * 	Got carried away with #ifdefing.  Sorry switch guys.
 * 	[1997/05/21  16:37:11  ted]
 * 
 * Revision 1.1.6.20  1997/05/21  03:35:32  ted
 * 	Publification of tg.h
 * 	[1997/05/21  02:59:50  ted]
 * 
 * Revision 1.1.6.19  1997/04/25  23:59:15  hayes
 * 	fix VOLATILE definitions properly this time...
 * 	[1997/04/25  23:58:19  hayes]
 * 
 * Revision 1.1.6.18  1997/04/25  22:19:24  hayes
 * 	Changed any registers that have a hardware wrap to VOLATILE since a hardware
 * 	wrap is a change to the value without the compiler knowing about it.
 * 	[1997/04/25  22:19:15  hayes]
 * 
 * Revision 1.1.6.17  1997/04/19  23:25:26  wayne
 * 	Add #defines for CPU_WATCHDOG_ENA bit and watchdog_clear register
 * 	[1997/04/19  23:25:20  wayne]
 * 
 * Revision 1.1.6.16  1997/04/15  05:46:54  hayes
 * 	Removed old link negotiation defines, they are now in the new file
 * 	zconf.h along with the other defines for running the gigabit
 * 	autoconfiguration state machine.
 * 	[1997/04/15  05:45:31  hayes]
 * 
 * Revision 1.1.6.15  1997/04/09  22:54:54  davis
 * 	Added threshold defines and mac_threshold register to mac_control.
 * 	[1997/04/09  22:39:46  davis]
 * 
 * Revision 1.1.6.14  1997/04/03  23:24:14  hayes
 * 	replace the status/command union...
 * 	[1997/04/03  23:24:02  hayes]
 * 
 * Revision 1.1.6.13  1997/04/03  22:33:15  ted
 * 	Under duress, I have built the pci_config to be different depending on
 * 	the way the ALT_BIG_ENDIAN define is set.  Sigh.
 * 	[1997/04/03  22:32:59  ted]
 * 
 * Revision 1.1.6.12  1997/04/03  22:05:25  ted
 * 	Really fix pci_config structure (still need to update the equivalent
 * 	#defines, but this requires coordination with the software).
 * 	[1997/04/03  22:05:05  ted]
 * 
 * Revision 1.1.6.11  1997/03/28  02:46:48  skapur
 * 	Added TG_DMA_STATE_UPDATE_TX_BUF_PROD bit definition and corrected the address for DMA ASSIST READ/WRITE HI/LO Addresses at the bottom of the file
 * 	[1997/03/28  02:35:36  skapur]
 * 
 * Revision 1.1.6.10  1997/03/27  22:18:28  hayes
 * 	fixed pci_conf region, added rom_base
 * 	[1997/03/27  22:07:41  hayes]
 * 
 * Revision 1.1.6.9  1997/03/25  23:51:33  ted
 * 	New defines for scratch pad area.
 * 	[1997/03/25  23:21:31  ted]
 * 
 * Revision 1.1.6.8  1997/03/19  16:44:28  wayne
 * 	Changed some TG_DMA_STATE defines for tigon rev.
 * 	[1997/03/19  16:05:15  wayne]
 * 
 * Revision 1.1.6.7  1997/03/16  20:28:30  hayes
 * 	added mac_stats
 * 	[1997/03/16  20:28:19  hayes]
 * 
 * Revision 1.1.6.6  1997/03/12  18:03:06  hayes
 * 	Added TG_MLC_UART_MASK
 * 	[1997/03/12  18:02:50  hayes]
 * 
 * Revision 1.1.6.5  1997/03/08  21:00:43  wayne
 * 	Merged with changes from 1.1.6.4
 * 	[1997/03/08  21:00:34  wayne]
 * 
 * 	Updated several DMA_STATE bits and MAC_RX bits.
 * 	[1997/03/08  20:09:22  wayne]
 * 
 * 	Added MAC_RX_STATE bits (0x00600000).
 * 	[1997/03/06  22:59:03  wayne]
 * 
 * Revision 1.1.6.4  1997/03/07  23:48:41  hayes
 * 	Added define for TG_MLC_SER_EEPROM_MASK
 * 	[1997/03/07  23:48:30  hayes]
 * 
 * Revision 1.1.6.3  1997/03/02  18:36:41  ted
 * 	redundancy support and a few bug fixes
 * 	[1997/03/02  18:25:26  ted]
 * 
 * Revision 1.1.6.2  1997/02/28  22:59:04  ted
 * 	Change parallel EEPROM accesses to flash accesses
 * 	[1997/02/28  22:58:53  ted]
 * 
 * Revision 1.1.6.1  1997/02/11  04:08:02  ted
 * 	Add missing define
 * 	[1997/02/11  03:58:29  ted]
 * 
 * 	Change some addresses for internal hardware locations used by gdb support
 * 	code.
 * 	[1997/02/08  22:23:55  ted]
 * 
 * 	Play with starting addresses of various hardware components (mostly for
 * 	gdb support).
 * 	[1997/02/08  22:21:19  ted]
 * 
 * Revision 1.1.2.17  1997/02/01  17:04:31  wayne
 * 	SUN_MP debug: Define non-hardware-oriented TRP, TSP, and TDP
 * 	[1997/02/01  16:52:30  wayne]
 * 
 * Revision 1.1.2.16  1997/01/11  22:52:22  ted
 * 	Change order of byte stuff in pci_config area to be correct.
 * 	[1997/01/11  22:51:58  ted]
 * 
 * Revision 1.1.2.15  1996/12/22  01:54:32  hayes
 * 	Change the TG_DMA_STATE_OVRIDE_BYTE_ALIGN bit definition
 * 	to TG_DMA_STATE_FORCE_32_BIT.  The reflects the current state of the
 * 	tigon.  ASYNC has been gone since RR days...
 * 	Add define for TG_PCI_STATE_USE_32_BITS_ONLY to allow override of
 * 	64 bit wide systems.
 * 	[1996/12/22  01:54:21  hayes]
 * 
 * Revision 1.1.2.14  1996/12/20  03:07:08  hayes
 * 	Added reserved area to tg_regs region.  It is now an accurate
 * 	reflection of the Tigon ASIC.
 * 	Added more meaningful defined for DMA_STATE_THRESH_XXX for
 * 	what he threshholds actually are.
 * 	[1996/12/20  03:06:58  hayes]
 * 
 * Revision 1.1.2.13  1996/12/17  00:01:31  hayes
 * 	Added DMA_ASSIST defines for SHORT and MINI descriptors and also for
 * 	CHAINED assist rings.
 * 	[1996/12/16  23:57:19  hayes]
 * 
 * Revision 1.1.2.12  1996/12/16  18:51:21  hayes
 * 	Move write_chan_xxx to proper place for CHAINED descriptors, add
 * 	prod, cons and ref pointers for unused DMA rings, so they can be
 * 	initialized properly even though they are unused in the CHAINED
 * 	case.
 * 	[1996/12/16  18:50:20  hayes]
 * 
 * Revision 1.1.2.11  1996/11/29  22:31:37  hayes
 * 	Merged with changes from 1.1.2.10
 * 	[1996/11/29  22:31:30  hayes]
 * 
 * 	Merge back into tg.h.
 * 	[1996/11/29  22:29:41  hayes]
 * 
 * Revision 1.1.2.10  1996/11/29  20:03:39  skapur
 * 	Merged with changes from 1.1.2.9
 * 	[1996/11/29  20:03:33  skapur]
 * 
 * 	updated the dma descriptor structure to match with Tigon HW
 * 	[1996/11/29  20:01:04  skapur]
 * 
 * 	Fixed len and cksum fields in the tgDmaDescr_t structure.
 * 	[1996/11/26  21:51:59  skapur]
 * 
 * Revision 1.1.2.9  1996/11/29  17:43:14  hayes
 * 	Reordered hardware specific structures in TDP.  Removed uar TX and RX
 * 	buffers, there are no longer gaps in high memory, so we don't have to
 * 	fill them.  Uart TX and RX buffers can be places elsewhere.
 * 	[1996/11/29  17:33:22  hayes]
 * 
 * Revision 1.1.2.9  1996/11/29  17:43:14  hayes
 * 	Reordered hardware specific structures in TDP.  Removed uar TX and RX
 * 	buffers, there are no longer gaps in high memory, so we don't have to
 * 	fill them.  Uart TX and RX buffers can be places elsewhere.
 * 	[1996/11/29  17:33:22  hayes]
 * 
 * Revision 1.1.2.8  1996/11/25  22:50:05  davis
 * 	Modified TG_DMA_ASSIST_DESCRS calculation to use TG_DMA_DESCR_CHAINED.
 * 	[1996/11/25  22:41:52  davis]
 * 
 * Revision 1.1.2.7  1996/11/20  20:04:03  ted
 * 	Merged with changes from 1.1.2.6
 * 	[1996/11/20  20:03:48  ted]
 * 
 * 	Minor prettification.
 * 	Change a few EEPROM related flag defines to match tg.h changes I'd made
 * 	when tg.h lived in nic/common.
 * 	[1996/11/20  19:44:36  ted]
 * 
 * Revision 1.1.2.6  1996/11/20  19:40:22  wayne
 * 	Redefine word 3 of mini-format DMA descriptor as union
 * 	[1996/11/20  19:40:08  wayne]
 * 
 * Revision 1.1.2.10  1996/11/29  20:03:39  skapur
 * 	Merged with changes from 1.1.2.9
 * 	[1996/11/29  20:03:33  skapur]
 * 
 * 	updated the dma descriptor structure to match with Tigon HW
 * 	[1996/11/29  20:01:04  skapur]
 * 
 * 	Fixed len and cksum fields in the tgDmaDescr_t structure.
 * 	[1996/11/26  21:51:59  skapur]
 * 
 * Revision 1.1.2.9  1996/11/29  17:43:14  hayes
 * 	Reordered hardware specific structures in TDP.  Removed uar TX and RX
 * 	buffers, there are no longer gaps in high memory, so we don't have to
 * 	fill them.  Uart TX and RX buffers can be places elsewhere.
 * 	[1996/11/29  17:33:22  hayes]
 * 
 * Revision 1.1.2.9  1996/11/29  17:43:14  hayes
 * 	Reordered hardware specific structures in TDP.  Removed uar TX and RX
 * 	buffers, there are no longer gaps in high memory, so we don't have to
 * 	fill them.  Uart TX and RX buffers can be places elsewhere.
 * 	[1996/11/29  17:33:22  hayes]
 * 
 * Revision 1.1.2.8  1996/11/25  22:50:05  davis
 * 	Modified TG_DMA_ASSIST_DESCRS calculation to use TG_DMA_DESCR_CHAINED.
 * 	[1996/11/25  22:41:52  davis]
 * 
 * Revision 1.1.2.7  1996/11/20  20:04:03  ted
 * 	Merged with changes from 1.1.2.6
 * 	[1996/11/20  20:03:48  ted]
 * 
 * 	Minor prettification.
 * 	Change a few EEPROM related flag defines to match tg.h changes I'd made
 * 	when tg.h lived in nic/common.
 * 	[1996/11/20  19:44:36  ted]
 * 
 * Revision 1.1.2.6  1996/11/20  19:40:22  wayne
 * 	Redefine word 3 of mini-format DMA descriptor as union
 * 	[1996/11/20  19:40:08  wayne]
 * 
 * Revision 1.1.2.5  1996/11/15  04:23:54  hayes
 * 	Fix TG_CHAINED_DESCRS area; rings still swapped.
 * 	[1996/11/15  04:09:12  hayes]
 * 
 * Revision 1.1.2.4  1996/11/13  01:07:10  hayes
 * 	More changes to make things work... :-)
 * 	[1996/11/13  01:07:00  hayes]
 * 
 * Revision 1.1.2.3  1996/11/13  00:36:18  wayne
 * 	Finally got one that builds...
 * 	[1996/11/13  00:11:14  wayne]
 * 
 * Revision 1.1.2.2  1996/11/12  21:44:42  wayne
 * 	Updated with new structure etc etc etc
 * 	[1996/11/12  21:44:31  wayne]
 * 
 * Revision 1.1.2.1  1996/11/11  23:43:39  skapur
 * 	Moving tg.h from src/nic/common to /src/common
 * 	[1996/11/11  23:42:27  skapur]
 * 
 * Revision 1.7.2.2  1996/11/05  00:04:19  ted
 * 	Correct spello for TG_CPU_BAD_MEM_ALIGN
 * 	Delete unused TG_ASSIST_BASE
 * 	Add a second MCAST address to mac area
 * 	Redo mailbox region (for 8 byte mailboxes and new addressing)
 * 	Add memory window size #define
 * 	[1996/11/05  00:03:25  ted]
 * 
 * Revision 1.7.2.1  1996/10/31  22:04:05  ted
 * 	Add PCI_STATE masks and shift values
 * 	[1996/10/30  19:05:24  ted]
 * 
 * 	Add copyright notice and history
 * 	Add and correct Tigon hardware info
 * 	[1996/10/30  00:49:20  ted]
 * 
 * $EndLog$
 */

/*
 * FILE tg.h
 *
 * COPYRIGHT (c) Essential Communication Corp. 1995
 * $Source: /proj/irix6.5.7m/isms/irix/kern/bsd/sys/RCS/tigon_pub_tg.h,v $
 */

#ifndef _TG_H_
#define _TG_H_

#include "tigon_rev.h"	/* tigon revisions */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *  This file defines the Tigon hardware -- basically a #includable manual.
 *
 *  It requires previous inclusion of alt_def.h as well as the following
 *  defines and #typedefs, which are typically defined in xx_conf.h files:
 */


/*  typedefs:
 *
 *    tg_gencom_t      512-byte general communications area
 *
 *    tg_hostaddr_t    64-bit host address for long-form DMA descriptors
 *                     [only referenced if TG_DMA_DESCR_FMT is LONG]
 *
 *    tg_macaddr_t     MAC address (how 48 bits appear in 64-bit field)
 *
 *    tg_mbox_t        mailbox format, probably U32[2] or U64
 */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                               REGISTERS
 */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                       PCI CONFIGURATION REGISTERS
 */
typedef struct tg_pci_conf {
#if ALT_BIG_ENDIAN
    U16 device_id;
    U16 vendor_id;
    union {
        struct {
            U16 status;
            U16 command;
        } st_cmd;
        U32 status_command;             /* see PCI_CONF_CMD_XXXX below */
    } un;
    U8  pciClass[3];		/* class is a C++ reserved word */
    U8  revision;
    U8  bist;                           /* see BIST_XXXX below */
    U8  header_type;
    U8  latency_timer;
    U8  cache_line_size;
    U32 shared_mem_base;
    U32 reserved_0[6];
    U16 subsystem_id;
    U16 subsystem_vendor_id;
    U32 rom_base;
    U32 reserved_1[2];
    U8  max_latency;
    U8  min_grant;
    U8  interrupt_pin;
    U8  interrupt_line;
#else /* ALT_BIG_ENDIAN */
    U16 vendor_id;
    U16 device_id;
    union {
        struct {
            U16 command;
            U16 status;
        } st_cmd;
        U32 status_command;             /* see PCI_CONF_CMD_XXXX below */
    } un;
    U8  revision;
    U8  pciClass[3];		/* class is a C++ reserved word */
    U8  cache_line_size;
    U8  latency_timer;
    U8  header_type;
    U8  bist;                           /* see BIST_XXXX below */
    U32 shared_mem_base;
    U32 reserved_0[6];
    U16 subsystem_vendor_id;
    U16 subsystem_id;
    U32 rom_base;
    U32 reserved_1[2];
    U8  interrupt_line;
    U8  interrupt_pin;
    U8  min_grant;
    U8  max_latency;
#endif /* ALT_BIG_ENDIAN */
} tgPciConf_t;

#define PCI_CONF_CMD_BUS_MASTER  0x0004      /* Enable bus master capability */
#define PCI_CONF_CMD_MEMORY      0x0002      /* Window is in memory space */
#if (TIGON_REV & TIGON_REV5)
#define PCI_CONF_CMD_MEM_WRITE_INVALIDATE 0x0010 /* Enable Mem Write Invalidate*/
#endif /* (TIGON_REV & TIGON_REV5) */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                       GENERAL CONTROL REGISTERS
 */

typedef struct tg_gen_control {
    VOLATILE U32 misc_host_control;     /* see TG_MHC_XXXX below */
    VOLATILE U32 misc_local_control;    /* see TG_MLC_XXXX below */
    U32          reserved_0[2];
    VOLATILE U32 misc_config;   /* Was timer_state in rev4; changed in rev5 */
    				/* see TG_MSC_XXXX below for bit definitione */
    U32          reserved_1[2];
    VOLATILE U32 pci_state;             /* see TG_PCI_STATE_XXXX below */
    U32          reserved_2[2];
    U32          window_base;
    VOLATILE U32 window_data;
    U32          reserved_3[4];
} tgGenControl_t;


/* Misc. Host Control Bit definitions */
#define TG_MHC_TIGON_VERSION_MASK       0xf0000000
#define TG_MHC_CHIP_EXPRESS_VERS        0x30000000
#define TG_MHC_LSI_GATE_VERS            0x40000000
#define TG_MHC_LSI_STANDARD_CELL_VERS   0x50000000
#define TG_MHC_LSI_STANDARD_CELL_VERS_B 0x60000000
#if (TIGON_REV & TIGON_REV5)
#define TG_MHC_MASK_OFF_RUPT            0x00000040
#endif /* (TIGON_REV & TIGON_REV5) */
#define TG_MHC_WORD_SWAP                0x00000020
#define TG_MHC_BYTE_SWAP                0x00000010
#define TG_MHC_RESET                    0x00000008
#define TG_MHC_CLEAR_RUPT               0x00000002
#define TG_MHC_RUPT_STATE               0x00000001

/* Misc. Local Control Bit definitions */
#define TG_MLC_SRAM_LADDR_22            0x00008000
#define TG_MLC_SRAM_LADDR_21            0x00004000

#if (TIGON_REV & TIGON_REV4)
#define TG_MLC_MORE_SRAM_BANK_ENA       0x00000100
#endif /* (TIGON_REV & TIGON_REV4) */

#if (TIGON_REV & TIGON_REV5)
#define TG_MLC_SRAM_BANK_DISA		0x00000000
#define TG_MLC_SRAM_BANK_1024K		0x00000100
#define TG_MLC_SRAM_BANK_512K		0x00000200
#define TG_MLC_SRAM_BANK_256K		0x00000300
#endif /* (TIGON_REV & TIGON_REV5) */


#define TG_MLC_SER_EEPROM_DATA_IN      	0x00800000
#define TG_MLC_SER_EEPROM_DATA_OUT     	0x00400000
#define TG_MLC_SER_EEPROM_WRITE_ENA 	0x00200000
#define TG_MLC_SER_EEPROM_CLK_OUT      	0x00100000
#define TG_MLC_SER_EEPROM_MASK      	0x00f00000

#define TG_MLC_MORE_SRAM_256K           0x00000200

#define TG_MLC_EEPROM_WRITE_ENA	        0x00000010

/* Misc config bits definition. This is new for Rev5 */
#if (TIGON_REV & TIGON_REV5)
#define TG_MSC_ENA_SYNC_SRAM_TIMING     0x00100000
#endif /* (TIGON_REV & TIGON_REV5) */


/* PCI state Bit definitions */

#define TG_PCI_STATE_FORCE_RESET        0x00000001
#define TG_PCI_STATE_PROVIDE_LEN        0x00000002

#define TG_PCI_STATE_DMA_READ_MAX_DISA  0x00000000
#define TG_PCI_STATE_DMA_READ_MAX_4     0x00000004
#define TG_PCI_STATE_DMA_READ_MAX_16    0x00000008
#define TG_PCI_STATE_DMA_READ_MAX_32    0x0000000c
#define TG_PCI_STATE_DMA_READ_MAX_64    0x00000010
#define TG_PCI_STATE_DMA_READ_MAX_128   0x00000014
#define TG_PCI_STATE_DMA_READ_MAX_256   0x00000018
#define TG_PCI_STATE_DMA_READ_MAX_1K    0x0000001c
#define TG_PCI_STATE_DMA_READ_MASK      0x0000001c
#define TG_PCI_STATE_DMA_READ_SHIFT     2

#define TG_PCI_STATE_DMA_WRITE_MAX_DISA 0x00000000
#define TG_PCI_STATE_DMA_WRITE_MAX_4    0x00000020
#define TG_PCI_STATE_DMA_WRITE_MAX_16   0x00000040
#define TG_PCI_STATE_DMA_WRITE_MAX_32   0x00000060
#define TG_PCI_STATE_DMA_WRITE_MAX_64   0x00000080
#define TG_PCI_STATE_DMA_WRITE_MAX_128  0x000000a0
#define TG_PCI_STATE_DMA_WRITE_MAX_256  0x000000c0
#define TG_PCI_STATE_DMA_WRITE_MAX_1K   0x000000e0
#define TG_PCI_STATE_DMA_WRITE_MASK     0x000000e0
#define TG_PCI_STATE_DMA_WRITE_SHIFT    5

#define TG_PCI_STATE_MIN_DMA            0x0000ff00

#define TG_PCI_STATE_USE_MEM_RD_MULT	0x00020000
#if (TIGON_REV & TIGON_REV5)
#define TG_PCI_STATE_NO_WORD_SWAP	0x00040000 /*one bit for rd and write*/
#define TG_PCI_STATE_66_MH_BUS		0x00080000 /* READ ONLY */
#endif /* (TIGON_REV & TIGON_REV5) */
#if (TIGON_REV & TIGON_REV4)
#define TG_PCI_STATE_NO_READ_WORD_SWAP	0x00040000
#define TG_PCI_STATE_NO_WRITE_WORD_SWAP	0x00080000
#endif /* (TIGON_REV & TIGON_REV4) */

#define TG_PCI_STATE_USE_32_BITS_ONLY	0x00100000

#if (TIGON_REV & TIGON_REV5)
#define TG_PCI_STATE_DMA_WR_ALL_ALIGN   0x00800000
#endif /* (TIGON_REV & TIGON_REV5) */

#define TG_PCI_STATE_READ_CMD_MASK	0x0f000000
#define TG_PCI_STATE_WRITE_CMD_MASK	0xf0000000

#define TG_PCI_STATE_READ_CMD_MEM_READ	0x06000000
#define TG_PCI_STATE_READ_CMD_CONF_READ	0x0a000000

#define TG_PCI_STATE_WRITE_CMD_MEM_WRITE	0x70000000
#define TG_PCI_STATE_WRITE_CMD_CONF_WRITE	0xb0000000


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                      HOST DMA CONTROL REGISTERS
 */
typedef struct tg_host_dma {
    U32               reserved_0[8];
    VOLATILE U32      dma_wr_state;     /* see TG_DMA_STATE_XXXX below */
    U32               reserved_1[3];
    VOLATILE U32      dma_rd_state;     /* see TG_DMA_STATE_XXXX below */
    U32               reserved_2[3];
} tgHostDma_t;

/* 
 * DMA State bits 
 */

#define TG_DMA_STATE_ERROR_MASK         0x7f000000
#if (TIGON_REV & TIGON_REV5)
#define TG_DMA_STATE_THRESH_32W         0x00000000 /* 0 in REV5 = 32 words */
#endif /* (TIGON_REV & TIGON_REV5) */
#define TG_DMA_STATE_THRESH_16W         0x00000100
#define TG_DMA_STATE_THRESH_8W          0x00000080
#define TG_DMA_STATE_THRESH_4W          0x00000040
#define TG_DMA_STATE_THRESH_2W          0x00000020
#define TG_DMA_STATE_THRESH_1W          0x00000010

#define TG_DMA_STATE_FORCE_32_BIT  	0x00000008
#define TG_DMA_STATE_ACTIVE             0x00000004
#define TG_DMA_STATE_NO_SWAP            0x00000002
#define TG_DMA_STATE_RESET              0x00000001

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                   LOCAL MEMORY CONFIGURATION REGISTERS
 */


typedef struct tg_local_mem_conf {
    U32                    reserved[16];
} tgLocalMemConf_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                        HOST DMA ASSIST CONTROL
 */

typedef struct tg_host_dma_assist {
    U32                    reserved_0[7];
    VOLATILE U32           assist_state;     /* see TG_DMA_ASSIST_XXXX below */
    U32                    reserved_1[8];
} tgHostDmaAssist_t;


#define TG_DMA_ASSIST_PAUSE             0x00000002
#define TG_DMA_ASSIST_ENABLE            0x00000001


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                        CPU CONTROL REGISTERS
 */
typedef struct tg_cpu_control {
    VOLATILE U32 cpu_state;             /* see TG_CPU_XXXX below */
    VOLATILE U32 pc;
    U32          reserved_0[3];
    U32          sram_addr;
    VOLATILE U32 sram_data;
    U32          reserved_1[9];
} tgCpuControl_t;

/* 
 * CPU state Bit Definitions 
 */

#define TG_CPU_RESET		0x00000001
#define TG_CPU_SINGLE		0x00000002
#define TG_CPU_PROM_FAILED	0x00000010
#define TG_CPU_HALT		0x00010000
#define TG_CPU_HALTED           0xffff0000 /* processor halted mask */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                        CPU'S INTERNAL REGISTERS
 */
typedef struct tg_cpu_reg {
    VOLATILE U32 cpu_reg[32];
} tgCpuReg_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                          MAC CONTROL REGISTERS
 *
 *  Then the MAC control ring pointers, again in only one format...
 */
typedef struct tg_mac_control {
    U32      reserved_0[8];
    VOLATILE U32 mac_rx_state;          /* see TG_MAC_RX_STATE_XXXX below */
    U32      reserved_1[7];
} tgMacControl_t;


#define TG_MAC_RX_STATE_STOP_NEXT	0x00000004


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                     TOTAL LAYOUT OF REGISTER AREA
 */

typedef struct tg_regs {
    tgPciConf_t       pci_conf;
    tgGenControl_t    gen_control;
    tgHostDma_t       host_dma;
    tgLocalMemConf_t  local_mem_conf;
    tgHostDmaAssist_t host_dma_assist;
    tgCpuControl_t    cpu_control;
    tgCpuReg_t        cpu_reg;
    tgMacControl_t    mac_control;
#if (TIGON_REV & TIGON_REV5)
    tgCpuControl_t    cpu_control_b;
    tgCpuReg_t        cpu_reg_b;
    U32		      reserved[64];
#else /* (TIGON_REV & TIGON_REV5) */
    U32		      reserved[112];
#endif /* (TIGON_REV & TIGON_REV5) */
} tg_regs_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 *                         SHARED MEMORY REGION
 */

typedef struct mac_stats {
    U32 excess_colls;
    U32 coll_1;
    U32 coll_2;
    U32 coll_3;
    U32 coll_4;
    U32 coll_5;
    U32 coll_6;
    U32 coll_7;
    U32 coll_8;
    U32 coll_9;
    U32 coll_10;
    U32 coll_11;
    U32 coll_12;
    U32 coll_13;
    U32 coll_14;
    U32 coll_15;
    U32 late_coll;
    U32 defers;
    U32 crc_err;
    U32 underrun;
    U32 crs_err;
    U32 res[3];
    U32 drop_ula;
    U32 drop_mc;
    U32 drop_fc;
    U32 drop_space;
    U32 coll;
#if (TIGON_REV & TIGON_REV5)
    U32 kept_bc;
    U32 kept_mc;
    U32 kept_uc;
#else /* (TIGON_REV & TIGON_REV5) */
    U32 res2[3];
#endif /* (TIGON_REV & TIGON_REV5) */
} mac_stats_t;


typedef struct tg_shmem {
    tg_regs_t regs;
    U32 keep_out[32];                   /* bottom 128 bytes inaccessible */
    mac_stats_t macstats;               /* MAC stats */
    tg_mbox_t mailbox[32];              /* mailboxes -- typedef elsewhere */
    tg_gencom_t gen_com;                /* gen comm area -- typedef elsewhere*/
                                        /*  must be exactly 512 bytes */
    /* NOTE: Following is *NOT* visible in local memory, only over PCI bus! */
    U32 loc_mem_window[512];            /* 2K local memory window */
    U32 host_dma_fifo[3072];            /* 12K host DMA FIFO access */
} tg_shmem_t;



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * NOTE: Following used only by HOST drivers.
 */
/* internal processor memory mapping, used by ALT_READ_TG_MEM and
 * ALT_WRITE_TG_MEM to figure out how to map this into the host.  The
 * calls to the ioctl() always work with the Tigon processor 
 * outlook, not the host outlook 
 */
#define TG_BEG_SRAM     0x0             /* host thinks it's here */
#define TG_BEG_SCRATCH  0xc00000        /* beg of scratch pad area */
#if (TIGON_REV & TIGON_REV5)
#define TG_END_SRAM     0x800000        /* end of SRAM, for 2 MB stuffed */
#define TG_END_SCRATCH  0xc04000        /* end of scratch pad CPU A (16KB) */
#define TG_END_SCRATCH_B 0xc02000       /* end of scratch pad CPU B (8KB) */
#define TG_BEG_SCRATCH_B_DEBUG 0xd00000 /* beg of scratch pad for ioctl */
#define TG_END_SCRATCH_B_DEBUG 0xd02000 /* end of scratch pad for ioctl */
#define TG_SCRATCH_DEBUG_OFF 0x100000	/* offset for ioctl usage */
#else /* (TIGON_REV & TIGON_REV5) */
#define TG_END_SRAM     0x200000        /* end of SRAM, for 2 MB stuffed */
#define TG_END_SCRATCH  0xc00800        /* end of scratch pad area (2KB) */
#endif /* (TIGON_REV & TIGON_REV5) */
#define TG_BEG_PROM   	0x40000000      /* beg of PROM, special access */
#define TG_BEG_FLASH    0x80000000      /* beg of EEPROM, special access */
#define TG_END_FLASH    0x80100000      /* end of EEPROM for 1 MB stuff */
#define TG_BEG_SER_EEPROM 0xa0000000    /* beg of Serial EEPROM (fake out) */
#define TG_END_SER_EEPROM 0xa0002000    /* end of Serial EEPROM (fake out) */
#define TG_BEG_REGS     0xc0000000      /* beg of register area */
#define TG_END_REGS     0xc0000400      /* end of register area */
#define TG_END_WRITE_REGS 0xc0000180    /* can't write GPRs currently */
#define TG_BEG_REGS2    0xc0000200      /* beg of second writeable reg area */
/* the EEPROM is byte addressable in a pretty odd way */
#define EEPROM_BYTE_LOC 0xff000000

/* board addresses (mapped starting at 0x0) */
#define TG_DEV_ID      0
#define TG_VENDOR_ID   2
#define TG_STATUS      4
#define TG_COMMAND     6
#define TG_CLASS       8
#define TG_BIST        0xc
#define TG_LAT_TIMER   0xd
#define TG_SHARED_BASE 0x10
#define TG_SUBSYS_ID	0x2c
#define TG_SUBSYS_VENDOR_ID	0x2e
#define TG_MAX_LAT     0x3c

#define TG_MISC_HOST_CONTROL  0x40
#define TG_MISC_LOCAL_CONTROL 0x44


#if (TIGON_REV & TIGON_REV5)
#define TG_MISC_CONFIG        0x50
#endif /* (TIGON_REV & TIGON_REV5) */


#define TG_PCI_STATE          0x5c


#define TG_WINDOW_BASE        0x68
#define TG_WINDOW_DATA        0x6c


#define TG_DMA_WRITE_STATE    0xa0


#define TG_DMA_READ_STATE     0xb0


#define TG_ASSIST_STATE       0x11c


#define TG_CPU_CONTROL        0x140
#define TG_PROG_CTR           0x144


#define TG_SRAM_ADDR          0x154
#define TG_SRAM_DATA          0x158


#define TG_GPR0               0x180
#define TG_CPU_REGS           TG_GPR0
#define TG_GPR1               0x184
#define TG_GPR2               0x188
#define TG_GPR3               0x18c
#define TG_GPR4               0x190
#define TG_GPR5               0x194
#define TG_GPR6               0x198
#define TG_GPR7               0x19c
#define TG_GPR8               0x1a0
#define TG_GPR9               0x1a4
#define TG_GPR10              0x1a8
#define TG_GPR11              0x1ac
#define TG_GPR12              0x1b0
#define TG_GPR13              0x1b4
#define TG_GPR14              0x1b8
#define TG_GPR15              0x1bc
#define TG_GPR16              0x1c0
#define TG_GPR17              0x1c4
#define TG_GPR18              0x1c8
#define TG_GPR19              0x1cc
#define TG_GPR20              0x1d0
#define TG_GPR21              0x1d4
#define TG_GPR22              0x1d8
#define TG_GPR23              0x1dc
#define TG_GPR24              0x1e0
#define TG_GPR25              0x1e4
#define TG_GPR26              0x1e8
#define TG_GPR27              0x1ec
#define TG_GPR28              0x1f0
#define TG_GPR29              0x1f4
#define TG_GPR30              0x1f8
#define TG_FRAME_PTR          TG_GPR30
#define TG_GPR31              0x1fc

#if (TIGON_REV & TIGON_REV5)
#define TG_PROCESSOR_A		0
#define TG_PROCESSOR_B		1
#define TG_CPU_A		TG_PROCESSOR_A
#define TG_CPU_B		TG_PROCESSOR_B
/*
 * Following macro can be used to access to any of the CPU registers
 * It will adjust the address appropriately.
 * Parameters:
 * 	reg - The register to access, e.g TG_CPU_CONTROL
 *      cpu - cpu, i.e PROCESSOR_A or PROCESSOR_B (or TG_CPU_A or TG_CPU_B)
 */
#define CPU_REG(reg, cpu) ((reg) + (cpu) * 0x100)
#endif /* (TIGON_REV & TIGON_REV5) */


#define TG_MAC_RX_STATE       0x220


#define TG_MAX_REG            0x3ff      /* max addr accessable via reg */ 

#define TG_SHMEM_START        0x400      /* start of shmem_t structure */
				         /* from host perspective */
#define TG_SW_REG_AREA        0x500
#define TG_MBOX0_HIGH         0x500
#define TG_MBOX0_LOW          0x504
#define TG_MBOX1_HIGH         0x508
#define TG_MBOX1_LOW          0x50c
#define TG_MBOX2_HIGH         0x510
#define TG_MBOX2_LOW          0x514
#define TG_MBOX3_HIGH         0x518
#define TG_MBOX3_LOW          0x51c
#define TG_MBOX4_HIGH         0x520
#define TG_MBOX4_LOW          0x524
#define TG_MBOX5_HIGH         0x528
#define TG_MBOX5_LOW          0x52c


#define TG_MEM_WINDOW         0x800
#define TG_MEM_WINDOW_SIZE    0x800
#define TG_MEM_WINDOW_SHIFT   7




/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* _TG_H_ */


