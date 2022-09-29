/*
 * Handle metrics for cluster percpu sysinfo (28)
 *
 * Code copied from sysinfo.c
 */

#ident "$Id: sysinfo_mp.c,v 1.36 1998/11/04 23:24:34 tes Exp $"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include <sgidefs.h>
#include <errno.h>
#if !BEFORE_IRIX6_4
#include <sys/iograph.h>
#include <sys/attributes.h>
#endif
#include <invent.h>
#include <ftw.h>
#include <paths.h>
#include "pmapi.h"
#include "impl.h"
#include "./cpu.h"
#include "./cluster.h"
#include "./xpc.h"

static struct sysinfo	sysinfo;

#define MAX_WALK_DEPTH	32	/* see hinv.c */

static pmMeta	meta[] = {
/* irix.kernel.percpu.swap.pagesin */
  { (char *)&sysinfo.bswapin, { PMID(1,28,1), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.swap.pagesout */
  { (char *)&sysinfo.bswapout, { PMID(1,28,2), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.pswitch */
  { (char *)&sysinfo.pswitch, { PMID(1,28,3), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.swap.procout */
  { (char *)&sysinfo.pswapout, { PMID(1,28,4), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.swap.in */
  { (char *)&sysinfo.swapin, { PMID(1,28,5), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.swap.out */
  { (char *)&sysinfo.swapout, { PMID(1,28,6), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.cpu.idle */
  { (char *)&sysinfo.cpu[CPU_IDLE], { PMID(1,28,7), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.intr */
  { (char *)&sysinfo.cpu[CPU_INTR], { PMID(1,28,8), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.sys */
  { (char *)&sysinfo.cpu[CPU_KERNEL], { PMID(1,28,9), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.sxbrk */
  { (char *)&sysinfo.cpu[CPU_SXBRK], { PMID(1,28,10), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.user */
  { (char *)&sysinfo.cpu[CPU_USER], { PMID(1,28,11), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.wait.total */
  { (char *)&sysinfo.cpu[CPU_WAIT], { PMID(1,28,12), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.io.iget */
  { (char *)&sysinfo.iget, { PMID(1,28,13), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.readch */
  { (char *)&sysinfo.readch, { PMID(1,28,14), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.kernel.percpu.runocc */
  { (char *)&sysinfo.runocc, { PMID(1,28,15), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.percpu.runque */
  { (char *)&sysinfo.runque, { PMID(1,28,16), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.percpu.swap.swpocc */
  { (char *)&sysinfo.swpocc, { PMID(1,28,17), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.percpu.swap.swpque */
  { (char *)&sysinfo.swpque, { PMID(1,28,18), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.percpu.syscall */
  { (char *)&sysinfo.syscall, { PMID(1,28,19), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.sysexec */
  { (char *)&sysinfo.sysexec, { PMID(1,28,20), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.sysfork */
  { (char *)&sysinfo.sysfork, { PMID(1,28,21), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.sysread */
  { (char *)&sysinfo.sysread, { PMID(1,28,22), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.syswrite */
  { (char *)&sysinfo.syswrite, { PMID(1,28,23), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.cpu.wait.gfxc */
  { (char *)&sysinfo.wait[W_GFXC], { PMID(1,28,24), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.wait.gfxf */
  { (char *)&sysinfo.wait[W_GFXF], { PMID(1,28,25), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.wait.io */
  { (char *)&sysinfo.wait[W_IO], { PMID(1,28,26), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.wait.pio */
  { (char *)&sysinfo.wait[W_PIO], { PMID(1,28,27), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.cpu.wait.swap */
  { (char *)&sysinfo.wait[W_SWAP], { PMID(1,28,28), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.percpu.writech */
  { (char *)&sysinfo.writech, { PMID(1,28,29), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.kernel.percpu.io.bread */
  { (char *)&sysinfo.bread, { PMID(1,28,30), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.bwrite */
  { (char *)&sysinfo.bwrite, { PMID(1,28,31), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.lread */
  { (char *)&sysinfo.lread, { PMID(1,28,32), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.lwrite */
  { (char *)&sysinfo.lwrite, { PMID(1,28,33), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.phread */
  { (char *)&sysinfo.phread, { PMID(1,28,34), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.phwrite */
  { (char *)&sysinfo.phwrite, { PMID(1,28,35), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.wcancel */
  { (char *)&sysinfo.wcancel, { PMID(1,28,36), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.io.namei */
  { (char *)&sysinfo.namei, { PMID(1,28,37), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.io.dirblk */
  { (char *)&sysinfo.dirblk, { PMID(1,28,38), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.percpu.tty.recvintr */
  { (char *)&sysinfo.rcvint, { PMID(1,28,39), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.tty.xmitintr */
  { (char *)&sysinfo.xmtint, { PMID(1,28,40), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.tty.mdmintr */
  { (char *)&sysinfo.mdmint, { PMID(1,28,41), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.tty.out */
  { (char *)&sysinfo.outch, { PMID(1,28,42), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.tty.raw */
  { (char *)&sysinfo.rawch, { PMID(1,28,43), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.tty.canon */
  { (char *)&sysinfo.canch, { PMID(1,28,44), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#ifndef IRIX5_3
/* irix.gfx.ioctl - obsolete after Irix5.3 */
  { (char *)&sysinfo, { PMID(1,28,45), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.gfx.ctxswitch */
  { (char *)&sysinfo, { PMID(1,28,46), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.swapbuf */
  { (char *)&sysinfo, { PMID(1,28,47), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.intr */
  { (char *)&sysinfo, { PMID(1,28,48), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.fifonowait */
  { (char *)&sysinfo, { PMID(1,28,49), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.fifowait */
  { (char *)&sysinfo, { PMID(1,28,50), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#else
/* irix.percpu.gfx.ioctl */
  { (char *)&sysinfo.griioctl, { PMID(1,28,45), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.percpu.gfx.ctxswitch */
  { (char *)&sysinfo.gswitch, { PMID(1,28,46), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.percpu.gfx.swapbuf */
  { (char *)&sysinfo.gswapbuf, { PMID(1,28,47), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.percpu.gfx.intr */
  { (char *)&sysinfo.gintr, { PMID(1,28,48), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.percpu.gfx.fifonowait */
  { (char *)&sysinfo.fifonowait, { PMID(1,28,49), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.percpu.gfx.fifowait */
  { (char *)&sysinfo.fifowait, { PMID(1,28,50), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
/* irix.kernel.percpu.intr.vme */
  { (char *)&sysinfo.vmeintr_svcd, { PMID(1,28,51), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.intr.non_vme */
  { (char *)&sysinfo.intr_svcd, { PMID(1,28,52), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.ipc.msg */
  { (char *)&sysinfo.msg, { PMID(1,28,53), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.ipc.sema */
  { (char *)&sysinfo.sema, { PMID(1,28,54), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.pty.masterch */
  { (char *)&sysinfo.ptc, { PMID(1,28,55), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.pty.slavech */
  { (char *)&sysinfo.pts, { PMID(1,28,56), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.flock.alloc */
  { (char *)&sysinfo.rectot, { PMID(1,28,57), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.flock.inuse */
  { (char *)&sysinfo.reccnt, { PMID(1,28,58), PM_TYPE_U32, PM_INDOM_CPU, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.sysother - derived */
  { (char *)0, { PMID(1,28,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/*
 * xpc metrics
 */
/* irix.xpc.kernel.percpu.cpu.idle */
  { (char *)&sysinfo.cpu[CPU_IDLE], { PMID(1,28,60), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.intr */
  { (char *)&sysinfo.cpu[CPU_INTR], { PMID(1,28,61), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.sys */
  { (char *)&sysinfo.cpu[CPU_KERNEL], { PMID(1,28,62), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.sxbrk */
  { (char *)&sysinfo.cpu[CPU_SXBRK], { PMID(1,28,63), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.user */
  { (char *)&sysinfo.cpu[CPU_USER], { PMID(1,28,64), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.wait.total */
  { (char *)&sysinfo.cpu[CPU_WAIT], { PMID(1,28,65), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.wait.gfxc */
  { (char *)&sysinfo.wait[W_GFXC], { PMID(1,28,66), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.wait.gfxf */
  { (char *)&sysinfo.wait[W_GFXF], { PMID(1,28,67), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.wait.io */
  { (char *)&sysinfo.wait[W_IO], { PMID(1,28,68), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.wait.pio */
  { (char *)&sysinfo.wait[W_PIO], { PMID(1,28,69), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.cpu.wait.swap */
  { (char *)&sysinfo.wait[W_SWAP], { PMID(1,28,70), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.percpu.io.bread */
  { (char *)&sysinfo.bread, { PMID(1,28,71), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.bwrite */
  { (char *)&sysinfo.bwrite, { PMID(1,28,72), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.lread */
  { (char *)&sysinfo.lread, { PMID(1,28,73), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.lwrite */
  { (char *)&sysinfo.lwrite, { PMID(1,28,74), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.phread */
  { (char *)&sysinfo.phread, { PMID(1,28,75), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.phwrite */
  { (char *)&sysinfo.phwrite, { PMID(1,28,76), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.wcancel */
  { (char *)&sysinfo.wcancel, { PMID(1,28,77), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.percpu.io.dirblk */
  { (char *)&sysinfo.dirblk, { PMID(1,28,78), PM_TYPE_U64, PM_INDOM_CPU, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
#if BEFORE_IRIX6_4
/* irix.kernel.percpu.kswitch */
  { (char *)0, { PMID(1,28,79), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.percpu.kpreempt */
  { (char *)0, { PMID(1,28,80), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.percpu.sysioctl */
  { (char *)0, { PMID(1,28,81), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
#else
/* irix.kernel.percpu.kswitch */
  { (char *)&sysinfo.kswitch, { PMID(1,28,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.kpreempt */
  { (char *)&sysinfo.kpreempt, { PMID(1,28,80), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.sysioctl */
  { (char *)&sysinfo.sysioctl, { PMID(1,28,81), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
#if BEFORE_IRIX6_5
/* irix.kernel.percpu.idl.mesgsnt */
  { (char *)0, { PMID(1,28,82), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.percpu.idl.mesgrcv */
  { (char *)0, { PMID(1,28,83), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.percpu.waitio.queue */
  { (char *)0, { PMID(1,28,84), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.percpu.waitio.occ */
  { (char *)0, { PMID(1,28,85), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
#else
/* irix.kernel.percpu.idl.mesgsnt */
  { (char *)&sysinfo.mesgsnt, { PMID(1,28,82), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.idl.mesgrcv */
  { (char *)&sysinfo.mesgrcv, { PMID(1,28,83), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.waitio.queue */
  { (char *)&sysinfo.wioque, { PMID(1,28,84), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.percpu.waitio.occ */
  { (char *)&sysinfo.wioocc, { PMID(1,28,85), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		direct_map = 1;
static char		*fetched = NULL;
static unsigned int	nval = 0;
static unsigned int	collected = 0;		/* sysinfo structures fetched? */
static int		n_cpus_sys;		/* cpu count in sysmp calls */

int			n_cpus;			/* number of cpus */
int			n_cpus_inv;		/* cpu count in inventory filter */
int			max_cpuclock = 0;	/* fastest CPU clock rate */
int			min_cpuclock = INT_MAX;	/* slowest CPU clock rate */
_pm_cpu			*cpus = (_pm_cpu *)0;

struct _pm_cpu_id {
    unsigned int	module:16;
    unsigned int	slot:8;
    unsigned int	slice:8;
};

extern char		*hinv_cpu_type(unsigned int);

#if !BEFORE_IRIX6_4
/*ARGSUSED*/
static int
percpu_filter(const char *p, const struct stat *s, int i, struct FTW *f)
{
    char		info[256];		/* inventory should fit in 256 bytes */
    int			len = sizeof(info);	/* this is checked below */
    int			rc;
    invent_cpuinfo_t	*cpu_invent;
    _pm_cpu		*pcpu;
    struct _pm_cpu_id	cpu_id;
    
    if (i != FTW_D)
	return 0;
    rc = attr_get((char *)p, INFO_LBL_DETAIL_INVENT, info, &len, 0);
    if (rc == 0) {
	if (((invent_generic_t *)info)->ig_invclass == INV_PROCESSOR) {
	    cpu_invent = (invent_cpuinfo_t *)info;
	    if (cpu_invent->ic_gen.ig_flag & INVENT_ENABLED) {
		if (n_cpus_inv < n_cpus) {
		    pcpu = &cpus[n_cpus_inv];
		    cpu_id.module = cpu_invent->ic_gen.ig_module;
		    cpu_id.slot = cpu_invent->ic_gen.ig_slot;
		    cpu_id.slice = cpu_invent->ic_slice;
		    pcpu->id = *(int *)(&cpu_id);
		    sprintf(pcpu->extname, "cpu:%d.%d.%c",
			    cpu_id.module, cpu_id.slot, 
			    (char)(cpu_id.slice+'a'));
		    pcpu->hwgname = strdup(p);
		    pcpu->freq = (int)cpu_invent->ic_cpu_info.cpufq;
		    pcpu->sdcache = 1024 * cpu_invent->ic_cpu_info.sdsize;
		    pcpu->type = hinv_cpu_type(cpu_invent->ic_cpu_info.cpuflavor);
		    if (pcpu->type == NULL)
			pcpu->type = "unknown";

		    pcpu->map = cpu_invent->ic_cpuid;

		    if (pcpu->freq > max_cpuclock)
			max_cpuclock = pcpu->freq;
		    if (pcpu->freq < min_cpuclock)
			min_cpuclock = pcpu->freq;

#ifdef PCP_DEBUG
		    if (pmIrixDebug & DBG_IRIX_CPU)
			__pmNotifyErr(LOG_DEBUG, 
				     "percpu_filter: Found CPU %d (id=%d, ext=%s, freq=%d, sdcache=%d, map=%d, type=%s) at %s\n", 
				     n_cpus_inv,
				     pcpu->id, pcpu->extname, pcpu->freq, 
				     pcpu->sdcache, pcpu->map, pcpu->type,
				     pcpu->hwgname);
#endif
		}
		else {
		    __pmNotifyErr(LOG_WARNING, 
			     "percpu_filter: Unexpected extra CPU? (module=%d slot=%d slice=%d)\n",
			     cpu_invent->ic_gen.ig_module,
			     cpu_invent->ic_gen.ig_slot,
			     cpu_invent->ic_slice);
		}
	    n_cpus_inv++;
	    }
	}
    }
    else if (errno == E2BIG)
	__pmNotifyErr(LOG_ERR, 
		     "percpu_filter: CPU %d: Inventory for CPU requires %d byte result buffer\n",
		     n_cpus_inv, len);

    return 0;
}
#endif /* !defined(IRIX6_2) && !defined(IRIX6_3) */

/* get other attributes from hinv */
/* Warning ... will break when more than 255 CPUs supported! */
#define ALL_8BIT 0xff

static void
hinv_setup(void)
{
    int		n;
    inventory_t	*hinv;
    int		seen_sdcache = 0;

    setinvent();

    while ((hinv = getinvent()) != NULL) {
	if (hinv->inv_class == INV_MEMORY) {
	    if (hinv->inv_type == INV_SIDCACHE) {
		if ((hinv->inv_unit & ALL_8BIT) == ALL_8BIT) {
		    /* all the same */
		    for (n = 0; n < n_cpus; n++) {
			if (cpus[n].sdcache == -1)
			    cpus[n].sdcache = (int)(hinv->inv_state/1024);
		    }
		}
		else {
		    n = (int)hinv->inv_controller;
		    cpus[n].sdcache = (int)(hinv->inv_state/1024);
		    if (seen_sdcache == 0)
			seen_sdcache = cpus[n].sdcache;
		    else
			seen_sdcache = -1;
		}
	    }
	}
	else if (hinv->inv_class == INV_PROCESSOR) {
	    if (hinv->inv_type == INV_CPUCHIP) {
		char	*type = hinv_cpu_type((unsigned int)hinv->inv_state);
		for (n = 0; n < n_cpus; n++) {
		    if (cpus[n].type == NULL)
			cpus[n].type = type;
		}
	    }
	    else if (hinv->inv_type == INV_CPUBOARD) {
		if ((hinv->inv_unit & ALL_8BIT) == ALL_8BIT) {
		    /* all the same */
		    for (n = 0; n < n_cpus; n++) {
			if (cpus[n].freq == -1)
			    cpus[n].freq = (int)hinv->inv_controller;
		    }
		    max_cpuclock = min_cpuclock = (int)hinv->inv_controller;
		}
		else {
		    n = (int)hinv->inv_unit;
		    if (cpus[n].freq == -1) {
			cpus[n].freq = (int)hinv->inv_controller;
			if (hinv->inv_controller > max_cpuclock)
			    max_cpuclock = (int)hinv->inv_controller;
			if (hinv->inv_controller < min_cpuclock)
			    min_cpuclock = (int)hinv->inv_controller;
		    }
		}
	    }
	}
    }

    endinvent();

    if (seen_sdcache > 0) {
	/*
	 * special speedracer hack ... only one SIDCACHE invent record,
	 * applies to all CPUs
	 * other values for seen_sdcache are
	 *	 0	no records seen
	 *	-1	more than one record seen (a la origin, challenge)
	 */
	for (n = 0; n < n_cpus; n++)
	    cpus[n].sdcache = seen_sdcache;
    }

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_CPU) {
	for (n = 0; n < n_cpus; n++) {
	    __pmNotifyErr(LOG_DEBUG, 
		     "hinv_setup: Found CPU %d (freq=%d, sdcache=%d, type=%s)\n", 
		     n, cpus[n].freq, cpus[n].sdcache, cpus[n].type);
	}
    }
#endif

    return;
}

int
reload_percpu(void)
{
    int		i = 0;
    int		new_ncpu = 0;

    new_ncpu = (int) sysmp(MP_NPROCS);
    if (new_ncpu < 0) {
	__pmNotifyErr(LOG_ERR, "reload_percpu: sysmp(MP_NPROCS): %s\n", strerror(new_ncpu));
	new_ncpu = 0;
    }

    for (i = 0; i < n_cpus; i++) {
	if (cpus[i].hwgname != NULL) {
	    free(cpus[i].hwgname);
	    cpus[i].hwgname = NULL;
	}
	cpus[i].type = NULL;
    }

    n_cpus = new_ncpu;

    if (new_ncpu == 0)
	return new_ncpu;

    cpus = (_pm_cpu *)realloc(cpus, n_cpus * sizeof(_pm_cpu));

    if (cpus == NULL) {
	__pmNotifyErr(LOG_ERR, "reload_percpu: cpus malloc failed (%d bytes)\n",
		     n_cpus * sizeof(_pm_cpu));
	nmeta = 0;
	n_cpus = 0;
	return 0;
    }

    for (i = 0; i < n_cpus; i++) {
	cpus[i].extname[0] = '\0';
	cpus[i].hwgname = NULL; 
	cpus[i].sdcache = cpus[i].freq = -1;
	cpus[i].type = NULL;
    }

    n_cpus_inv = 0;

#if !BEFORE_IRIX6_4
    /* scan the cpu inventory to get path names */
    nftw(_PATH_HWGFS, percpu_filter, MAX_WALK_DEPTH, FTW_PHYS);

    if (n_cpus_inv != n_cpus) {
    	if (n_cpus_inv == 0) {
	    if (_pm_has_nodes)
		__pmNotifyErr(LOG_WARNING, "reload_percpu: No CPU inventory found via /hw\n");
	}
	else
	    __pmNotifyErr(LOG_WARNING, "reload_percpu: CPU count mismatch: %d (via /hw) != %d (sysmp), former will be ignored\n",
		     n_cpus_inv, n_cpus);
	n_cpus_inv = 0;
    }
#endif /* !defined(IRIX6_2) && !defined(IRIX6_3) */

    /* old way, and extras ... */
    hinv_setup();

    return n_cpus;
}

void
percpu_init(int reset)
{
    int		i;
    int		cachedi = 0;
    int		physcpu;
    int		sts;
    _pm_cpu	tmpcpu;
    static int	init_done = 0;
    extern int	errno;

    if (init_done) {
	if (reset) {
	    /* do some things again */
	    indomtab[PM_INDOM_CPU].i_numinst = reload_percpu();
	}
	else {
	    __pmNotifyErr(LOG_WARNING, "percpu_init: sysinfo_mp stats already initialized");
	    return;
	}
    }    
    else {
	/* first time */
	for (i = 0; i < nmeta; i++) {
	    init_done = 1;
	    if (meta[i].m_offset != NULL)
		meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&sysinfo));
	    meta[i].m_desc.indom = indomtab[PM_INDOM_CPU].i_indom;
	    if (direct_map && meta[i].m_desc.pmid != PMID(1,28,i+1)) {
		direct_map = 0;
		__pmNotifyErr(LOG_WARNING, "percpu_init: direct map disabled @ meta[%d]", i);
	    }
	}
	fetched = NULL;
    }

    fetched = (char *)realloc(fetched, n_cpus * sizeof(char));
    if (fetched == NULL) {
	__pmNotifyErr(LOG_ERR, "percpu_init: fetched malloc failed (%d bytes)\n",
		     n_cpus * sizeof(char));
	nmeta = 0;
	return;
    }

    for (physcpu = 0, n_cpus_sys = 0; n_cpus_sys < indomtab[PM_INDOM_CPU].i_numinst; physcpu++) {
	sts = (int)sysmp(MP_SAGET1, MPSA_SINFO, &sysinfo, sizeof(sysinfo), physcpu);
	if (sts < 0) {
	    if (errno == EINVAL) {
		__pmNotifyErr(LOG_WARNING, 
			     "percpu_init: physical CPU %d disabled", physcpu);
	    }
	    else {
		__pmNotifyErr(LOG_WARNING, 
			     "percpu_init: sysmp(MP_SAGET1, MPSA_SINFO, %d): %s",
			     physcpu, pmErrStr(-errno));

		n_cpus_sys = 0;
		return;
	    }
	}

	/* real CPU at this physical position */
	else if (n_cpus_inv) {
	    if (cpus[n_cpus_sys].map != physcpu) {
                /* search for it through the remaining CPUs */
                if (cachedi && cpus[cachedi].map == physcpu) {
                    /* swap places */
                    tmpcpu = cpus[n_cpus_sys];
                    cpus[n_cpus_sys] = cpus[cachedi];
                    cpus[cachedi] = tmpcpu;
                    n_cpus_sys++;
                    if (cachedi+1 < n_cpus_inv)
                        cachedi++;
#ifdef PCP_DEBUG
                    if (pmIrixDebug & DBG_IRIX_CPU)
                        __pmNotifyErr(LOG_DEBUG,
                                "cached index matched CPU id %d\n", physcpu);
#endif
                }
                else {  /* linear search (find the hard way) */
                    for (i = 0; i < n_cpus_inv; i++) {
                        if (cpus[i].map == physcpu) {
                            /* swap places */
                            tmpcpu = cpus[n_cpus_sys];
                            cpus[n_cpus_sys] = cpus[i];
                            cpus[i] = tmpcpu;
                            n_cpus_sys++;
                            if (i+1 < n_cpus_inv)
                                cachedi = i+1;
#ifdef PCP_DEBUG
                            if (pmIrixDebug & DBG_IRIX_CPU)
                                __pmNotifyErr(LOG_DEBUG,
                                        "search matched CPU id %d\n", physcpu);
#endif
                            break;
                        }
                    }
                    if (i == n_cpus_inv) {      /* couldn't find it at all */
                        __pmNotifyErr(LOG_ERR,
                                "percpu_init: CPU %d (inventory id=%d) found "
                                "by sysmp, but not in inventory\n",
                                n_cpus_sys, physcpu);
                        n_cpus_inv = 0;     /* revert to other output format */
                        cpus[n_cpus_sys++].map = physcpu;
                    }
                }
            }
            else {
                n_cpus_sys++;
#ifdef PCP_DEBUG
                if (pmIrixDebug & DBG_IRIX_CPU)
                    __pmNotifyErr(LOG_DEBUG,
                        "direct match (inv-sysmp) for CPU id %d\n", physcpu);
#endif
	   }
	}
	else {
	    cpus[n_cpus_sys].id = n_cpus_sys;
	    sprintf(cpus[n_cpus_sys].extname, "cpu%d", n_cpus_sys);
	    cpus[n_cpus_sys++].map = physcpu;
	}
    }
}

int
percpu_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

void
percpu_fetch_setup(void)
{
    int		i;

    for (i = 0; i < indomtab[PM_INDOM_CPU].i_numinst; i++)
	fetched[i] = 0;
    collected = 0;
    nval = 0;
}

int
percpu_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		sts;
    int		kval;
    int		c;
    pmAtomValue	av;
    void	*vp;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
		vpcb->p_nval = PM_ERR_APPVERSION;
		return 0;
	    }

	    if (collected == 0) {
		for (c = 0; c < indomtab[PM_INDOM_CPU].i_numinst; c++) {
		    if (__pmInProfile(indomtab[PM_INDOM_CPU].i_indom, profp, cpus[c].id)) {
			if (sysmp(MP_SAGET1, MPSA_SINFO, &cpus[c].sysinfo, 
				  sizeof(sysinfo), cpus[c].map) < 0) {
			    __pmNotifyErr(LOG_WARNING, "percpu_fetch[%d]: %s", 
					 cpus[c].map, pmErrStr(-errno));
			}
			else {
			    fetched[c] = 1;
			    nval++;
			}
		    }
		    vpcb->p_nval = nval;
		}
		collected = 1;
	    }
	    else
		vpcb->p_nval = nval;

	    if (vpcb->p_nval == 0)
		return 0;

	    sizepool(vpcb);

	    for (kval = 0, c = 0; c < indomtab[PM_INDOM_CPU].i_numinst; c++) {
		if (!fetched[c])
		    continue;
		if (meta[i].m_desc.pmid == PMID(1,28,59)) {
		    /* irix.kernel.percpu.sysother - derived */
#if 1 /* pv#627398 - qa/1006 */ 
		    av.ul = cpus[c].sysinfo.syscall
			    - cpus[c].sysinfo.syswrite - cpus[c].sysinfo.sysread
			    - cpus[c].sysinfo.sysfork - cpus[c].sysinfo.sysexec;
#else
		    av.ul = cpus[c].sysinfo.syscall
			    - cpus[c].sysinfo.syswrite - cpus[c].sysinfo.sysread
			    - cpus[c].sysinfo.sysfork - cpus[c].sysinfo.sysexec
			    - cpus[c].sysinfo.sysioctl;
#endif
		}
		else {
		    vp = (void *)&((char *)&cpus[c].sysinfo)
			                         [(ptrdiff_t)meta[i].m_offset];
		    avcpy(&av, vp, PM_TYPE_U32);
		}

		/* Do any special case value conversions */

		/* Convert ticks to millisecs for CPU and WAIT times */
		if (meta[i].m_desc.units.scaleTime == PM_TIME_MSEC) {
		    __uint64_t	*cp;
		    cp = XPCincr(meta[i].m_desc.pmid, c, av.ul);
		    if (meta[i].m_desc.type == PM_TYPE_U32)
			av.ul = (__uint32_t)((*cp * 1000) / HZ);
		    else
			av.ull = (*cp * 1000) / HZ;
		}

		/* Convert basic blocks to Kilobytes where needed */
		else if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE) {
		    __uint64_t	*cp;
		    cp = XPCincr(meta[i].m_desc.pmid, c, av.ul);
		    /* see sys/param.h */
		    if (meta[i].m_desc.type == PM_TYPE_U32)
			av.ul = (__uint32_t)((*cp * BBSIZE) / 1024);
		    else
			av.ull = (*cp * BBSIZE) / 1024;
		}

		if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[kval], meta[i].m_desc.type)) < 0)
		    return sts;
		vpcb->p_vp[kval++].inst = cpus[c].id;
	    }
	    vpcb->p_valfmt = sts;
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
