#!smake
#
# Makefile for IRIX crash CMDS library
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#
include $(ROOT)/usr/include/make/commondefs

GCDEFS    = -DICRASH_DEBUG -DI_TEMP
LCDEFS    = -I../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
LCDEFS1	  = -DIP27 -DSN0 -D_KERNEL -DHUB1_WAR -D_PAGESZ=4096
TARGETS   = ../cmds.a
CFILES    = cmds.c \
			cmd_addtype.c cmd_avlnode.c cmd_base.c cmd_block.c cmd_bucket.c \
			cmd_ctrace.c cmd_config.c cmd_curkthread.c cmd_dblock.c \
			cmd_debug.c cmd_defkthread.c cmd_die.c cmd_dis.c cmd_dump.c \
			cmd_eframe.c cmd_etrace.c cmd_file.c cmd_findpde.c cmd_findsym.c \
			cmd_from.c cmd_fstype.c cmd_func.c cmd_help.c cmd_hinv.c \
			cmd_history.c cmd_hwpath.c cmd_icrashdef.c cmd_inode.c \
			cmd_inpcb.c cmd_ithread.c cmd_kthread.c cmd_ktrace.c \
			cmd_mblock.c cmd_mbuf.c cmd_memory.c cmd_mempool.c cmd_mlinfo.c \
			cmd_mrlock.c cmd_namelist.c cmd_nodepda.c cmd_outfile.c \
			cmd_page.c cmd_pager.c cmd_pd.c cmd_pda.c cmd_pde.c cmd_pfdat.c \
			cmd_pid.c cmd_pmap.c cmd_po.c cmd_pregion.c \
			cmd_print.c cmd_proc.c cmd_ptov.c cmd_px.c cmd_queue.c \
			cmd_quit.c cmd_region.c cmd_report.c cmd_rnode.c cmd_runq.c \
			cmd_sbe.c cmd_search.c cmd_sema.c cmd_set.c cmd_shell.c \
			cmd_sizeof.c cmd_slpproc.c cmd_lsnode.c cmd_socket.c cmd_stack.c \
			cmd_stat.c cmd_sthread.c cmd_strace.c cmd_stream.c cmd_string.c \
			cmd_strstat.c cmd_struct.c cmd_swap.c cmd_symbol.c \
			cmd_tcpcb.c cmd_tlb.c cmd_trace.c cmd_type.c cmd_unpcb.c \
			cmd_unset.c cmd_uthread.c cmd_utrace.c cmd_vertex.c cmd_vfs.c \
			cmd_vnode.c cmd_vproc.c cmd_vsocket.c cmd_vtop.c cmd_walk.c \
			cmd_whatis.c cmd_zone.c cmd_fru.c cmd_xthread.c \
			cmd_anon.c cmd_anontree.c cmd_mntinfo.c

OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=cmds_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES) cmd_hubreg.o cmd_dirmem.o
	ar ccurl $(TARGETS) $(OFILES) cmd_hubreg.o cmd_dirmem.o

cmd_hubreg.o: cmd_hubreg.c
	$(CC) $(LCDEFS) $(LCDEFS1) $(GCDEFS) $(GCOPTS) $(GCINCS) -c cmd_hubreg.c

cmd_dirmem.o: cmd_dirmem.c
	$(CC) $(LCDEFS) $(LCDEFS1) $(GCDEFS) $(GCOPTS) $(GCINCS) -c cmd_dirmem.c
