/*
 * Handle metrics for cluster sysinfo (10)
 *
 * Code built by newcluster on Fri Jun  3 01:18:23 EST 1994
 */

#ident "$Id: sysinfo.c,v 1.24 1998/11/04 23:24:34 tes Exp $"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./xpc.h"

static struct sysinfo	sysinfo;

static pmMeta	meta[] = {
/* irix.swap.pagesin */
  { (char *)&sysinfo.bswapin, { PMID(1,10,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.swap.pagesout */
  { (char *)&sysinfo.bswapout, { PMID(1,10,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.pswitch */
  { (char *)&sysinfo.pswitch, { PMID(1,10,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.swap.procout */
  { (char *)&sysinfo.pswapout, { PMID(1,10,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.swap.in */
  { (char *)&sysinfo.swapin, { PMID(1,10,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.swap.out */
  { (char *)&sysinfo.swapout, { PMID(1,10,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.cpu.idle */
  { (char *)&sysinfo.cpu[CPU_IDLE], { PMID(1,10,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.intr */
  { (char *)&sysinfo.cpu[CPU_INTR], { PMID(1,10,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.sys */
  { (char *)&sysinfo.cpu[CPU_KERNEL], { PMID(1,10,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.sxbrk */
  { (char *)&sysinfo.cpu[CPU_SXBRK], { PMID(1,10,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.user */
  { (char *)&sysinfo.cpu[CPU_USER], { PMID(1,10,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.wait.total */
  { (char *)&sysinfo.cpu[CPU_WAIT], { PMID(1,10,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.io.iget */
  { (char *)&sysinfo.iget, { PMID(1,10,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.readch */
  { (char *)&sysinfo.readch, { PMID(1,10,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.kernel.all.runocc */
  { (char *)&sysinfo.runocc, { PMID(1,10,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.all.runque */
  { (char *)&sysinfo.runque, { PMID(1,10,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.all.swap.swpocc */
  { (char *)&sysinfo.swpocc, { PMID(1,10,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.all.swap.swpque */
  { (char *)&sysinfo.swpque, { PMID(1,10,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,0,0,0,0} } },
/* irix.kernel.all.syscall */
  { (char *)&sysinfo.syscall, { PMID(1,10,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.sysexec */
  { (char *)&sysinfo.sysexec, { PMID(1,10,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.sysfork */
  { (char *)&sysinfo.sysfork, { PMID(1,10,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.sysread */
  { (char *)&sysinfo.sysread, { PMID(1,10,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.syswrite */
  { (char *)&sysinfo.syswrite, { PMID(1,10,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.cpu.wait.gfxc */
  { (char *)&sysinfo.wait[W_GFXC], { PMID(1,10,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.wait.gfxf */
  { (char *)&sysinfo.wait[W_GFXF], { PMID(1,10,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.wait.io */
  { (char *)&sysinfo.wait[W_IO], { PMID(1,10,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.wait.pio */
  { (char *)&sysinfo.wait[W_PIO], { PMID(1,10,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.cpu.wait.swap */
  { (char *)&sysinfo.wait[W_SWAP], { PMID(1,10,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.kernel.all.writech */
  { (char *)&sysinfo.writech, { PMID(1,10,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.kernel.all.io.bread */
  { (char *)&sysinfo.bread, { PMID(1,10,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.bwrite */
  { (char *)&sysinfo.bwrite, { PMID(1,10,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.lread */
  { (char *)&sysinfo.lread, { PMID(1,10,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.lwrite */
  { (char *)&sysinfo.lwrite, { PMID(1,10,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.phread */
  { (char *)&sysinfo.phread, { PMID(1,10,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.phwrite */
  { (char *)&sysinfo.phwrite, { PMID(1,10,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.wcancel */
  { (char *)&sysinfo.wcancel, { PMID(1,10,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.io.namei */
  { (char *)&sysinfo.namei, { PMID(1,10,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.io.dirblk */
  { (char *)&sysinfo.dirblk, { PMID(1,10,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.kernel.all.tty.recvintr */
  { (char *)&sysinfo.rcvint, { PMID(1,10,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.tty.xmitintr */
  { (char *)&sysinfo.xmtint, { PMID(1,10,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.tty.mdmintr */
  { (char *)&sysinfo.mdmint, { PMID(1,10,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.tty.out */
  { (char *)&sysinfo.outch, { PMID(1,10,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.tty.raw */
  { (char *)&sysinfo.rawch, { PMID(1,10,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.tty.canon */
  { (char *)&sysinfo.canch, { PMID(1,10,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#ifndef IRIX5_3 /* obsolete after Irix5.3 */
/* irix.gfx.ioctl */
  { (char *)0, { PMID(1,10,45), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.gfx.ctxswitch */
  { (char *)0, { PMID(1,10,46), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.swapbuf */
  { (char *)0, { PMID(1,10,47), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.intr */
  { (char *)0, { PMID(1,10,48), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.fifonowait */
  { (char *)0, { PMID(1,10,49), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.gfx.fifowait */
  { (char *)0, { PMID(1,10,50), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#else
/* irix.gfx.ioctl */
  { (char *)&sysinfo.griioctl, { PMID(1,10,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.gfx.ctxswitch */
  { (char *)&sysinfo.gswitch, { PMID(1,10,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.gfx.swapbuf */
  { (char *)&sysinfo.gswapbuf, { PMID(1,10,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.gfx.intr */
  { (char *)&sysinfo.gintr, { PMID(1,10,48), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.gfx.fifonowait */
  { (char *)&sysinfo.fifonowait, { PMID(1,10,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.gfx.fifowait */
  { (char *)&sysinfo.fifowait, { PMID(1,10,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
/* irix.kernel.all.intr.vme */
  { (char *)&sysinfo.vmeintr_svcd, { PMID(1,10,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.intr.non_vme */
  { (char *)&sysinfo.intr_svcd, { PMID(1,10,52), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.ipc.msg */
  { (char *)&sysinfo.msg, { PMID(1,10,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.ipc.sema */
  { (char *)&sysinfo.sema, { PMID(1,10,54), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.pty.masterch */
  { (char *)&sysinfo.ptc, { PMID(1,10,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.pty.slavech */
  { (char *)&sysinfo.pts, { PMID(1,10,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.flock.alloc */
  { (char *)&sysinfo.rectot, { PMID(1,10,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.flock.inuse */
  { (char *)&sysinfo.reccnt, { PMID(1,10,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.sysother - derived */
  { (char *)0, { PMID(1,10,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.xpc.kernel.all.cpu.idle */
  { (char *)&sysinfo.cpu[CPU_IDLE], { PMID(1,10,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.intr */
  { (char *)&sysinfo.cpu[CPU_INTR], { PMID(1,10,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.sys */
  { (char *)&sysinfo.cpu[CPU_KERNEL], { PMID(1,10,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.sxbrk */
  { (char *)&sysinfo.cpu[CPU_SXBRK], { PMID(1,10,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.user */
  { (char *)&sysinfo.cpu[CPU_USER], { PMID(1,10,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.wait.total */
  { (char *)&sysinfo.cpu[CPU_WAIT], { PMID(1,10,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.wait.gfxc */
  { (char *)&sysinfo.wait[W_GFXC], { PMID(1,10,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.wait.gfxf */
  { (char *)&sysinfo.wait[W_GFXF], { PMID(1,10,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.wait.io */
  { (char *)&sysinfo.wait[W_IO], { PMID(1,10,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.wait.pio */
  { (char *)&sysinfo.wait[W_PIO], { PMID(1,10,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.cpu.wait.swap */
  { (char *)&sysinfo.wait[W_SWAP], { PMID(1,10,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} } },
/* irix.xpc.kernel.all.io.bread */
  { (char *)&sysinfo.bread, { PMID(1,10,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.bwrite */
  { (char *)&sysinfo.bwrite, { PMID(1,10,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.lread */
  { (char *)&sysinfo.lread, { PMID(1,10,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.lwrite */
  { (char *)&sysinfo.lwrite, { PMID(1,10,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.phread */
  { (char *)&sysinfo.phread, { PMID(1,10,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.phwrite */
  { (char *)&sysinfo.phwrite, { PMID(1,10,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.wcancel */
  { (char *)&sysinfo.wcancel, { PMID(1,10,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
/* irix.xpc.kernel.all.io.dirblk */
  { (char *)&sysinfo.dirblk, { PMID(1,10,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_KBYTE,0} } },
#if BEFORE_IRIX6_4
/* irix.kernel.all.kswitch */
  { (char *)0, { PMID(1,10,79), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.all.kpreempt */
  { (char *)0, { PMID(1,10,80), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.all.sysioctl */
  { (char *)0, { PMID(1,10,81), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
#else
/* irix.kernel.all.kswitch */
  { (char *)&sysinfo.kswitch, { PMID(1,10,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.kpreempt */
  { (char *)&sysinfo.kpreempt, { PMID(1,10,80), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.sysioctl */
  { (char *)&sysinfo.sysioctl, { PMID(1,10,81), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
#if BEFORE_IRIX6_5
/* irix.kernel.all.idl.mesgsnt */
  { (char *)0, { PMID(1,10,82), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.all.idl.mesgrcv */
  { (char *)0, { PMID(1,10,83), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.all.waitio.queue */
  { (char *)0, { PMID(1,10,84), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
/* irix.kernel.all.waitio.occ */
  { (char *)0, { PMID(1,10,85), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } }, 
#else
/* irix.kernel.all.idl.mesgsnt */
  { (char *)&sysinfo.mesgsnt, { PMID(1,10,82), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.idl.mesgrcv */
  { (char *)&sysinfo.mesgrcv, { PMID(1,10,83), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.waitio.queue */
  { (char *)&sysinfo.wioque, { PMID(1,10,84), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.kernel.all.waitio.occ */
  { (char *)&sysinfo.wioocc, { PMID(1,10,85), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif
};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	direct_map = 1;
static int	fetched;

void
sysinfo_init(int reset)
{
    int		i;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&sysinfo));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,10,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, "sysinfo_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void
sysinfo_fetch_setup(void)
{
    fetched = 0;
}

int
sysinfo_desc(pmID pmid, pmDesc *desc)
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

/*ARGSUSED*/
int
sysinfo_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
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
	    if (fetched == 0) {
		if (sysmp(MP_SAGET, MPSA_SINFO, &sysinfo, sizeof(sysinfo)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "sysinfo_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
	    }
	    vpcb->p_nval = 1;
	    vpcb->p_vp[0].inst = PM_IN_NULL;
	    if (meta[i].m_desc.pmid == PMID(1,10,59)) {
		/* irix.kernel.all.sysother - derived */
#if 1 /* pv#627398 - qa/1006 */
		av.ul = sysinfo.syscall
			- sysinfo.syswrite - sysinfo.sysread
			- sysinfo.sysfork - sysinfo.sysexec;
#else
		av.ul = sysinfo.syscall
			- sysinfo.syswrite - sysinfo.sysread
			- sysinfo.sysfork - sysinfo.sysexec
			- sysinfo.sysioctl;
#endif
	    }
	    else {
		vp = (void *)&((char *)&sysinfo)[(ptrdiff_t)meta[i].m_offset];
		avcpy(&av, vp, PM_TYPE_U32);
	    }

	    /* Do any special case value conversions */

	    /* Convert ticks to millisecs for CPU and WAIT times */
	    if (meta[i].m_desc.units.scaleTime == PM_TIME_MSEC) {
		__uint64_t	*cp;
		cp = XPCincr(meta[i].m_desc.pmid, PM_IN_NULL, av.ul);
		if (meta[i].m_desc.type == PM_TYPE_U32)
		    av.ul = (__uint32_t)((*cp * 1000) / HZ);
		else
		    av.ull = (*cp * 1000) / HZ;
	    }

	    /* Convert basic blocks to Kilobytes where needed */
	    else if (meta[i].m_desc.units.scaleSpace == PM_SPACE_KBYTE) {
		__uint64_t	*cp;
		cp = XPCincr(meta[i].m_desc.pmid, PM_IN_NULL, av.ul);
		/* see sys/param.h */
		if (meta[i].m_desc.type == PM_TYPE_U32)
		    av.ul = (__uint32_t)((*cp * BBSIZE) / 1024);
		else
		    av.ull = (*cp * BBSIZE) / 1024;
	    }

	    return vpcb->p_valfmt = __pmStuffValue(&av, 0, vpcb->p_vp, meta[i].m_desc.type);
	}
    }
    return PM_ERR_PMID;
}
