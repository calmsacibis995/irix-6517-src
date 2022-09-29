#
# HP-UX Process Table Metrics from the PCP subsystem
#

_hpproc {
    proc.nprocs			PT_NUMPROCS
    proc.psinfo.uid		PT_UID
    proc.psinfo.pid		PT_PID
    proc.psinfo.ppid		PT_PPID
    proc.memory.virtual.dat	PT_SIZE_DAT
    proc.memory.virtual.shm	PT_SIZE_SHM
    proc.memory.virtual.bss	PT_SIZE_BSS
    proc.memory.virtual.txt	PT_SIZE_TXT
    proc.memory.virtual.stack	PT_SIZE_STACK
    proc.psinfo.nice		PT_NICE
    proc.psinfo.ttyname		PT_TTYNAME
    proc.psinfo.ttymajor	PT_TTYMAJOR
    proc.psinfo.ttyminor	PT_TTYMINOR
    proc.psinfo.gid		PT_GID
    proc.psinfo.pri		PT_PRI
    proc.psinfo.addr		PT_ADDR
    proc.psinfo.cpu		PT_CPU
    proc.pstatus.utime		PT_UTIME
    proc.pstatus.stime		PT_STIME
    proc.psusage.starttime	PT_STARTTIME
# flags need to be revisited.
#    proc.psinfo.flags		PT_FLAGS
#    proc.pstatus.flags		PT_FLAGS
    proc.psinfo.state		PT_STATE
    proc.psinfo.wchan		PT_WCHAN
    proc.psinfo.psargs		PT_FNAME
# The order of the next two entries is important to
# calculate processPctCPU
    proc.psusage.tstamp		PT_TSTAMP
    proc.psinfo.time		PT_TIME
    proc.psusage.rss		PT_RSS
    proc.pscred.suid		PT_SUID
    proc.psinfo.uname		PT_UNAME
}

