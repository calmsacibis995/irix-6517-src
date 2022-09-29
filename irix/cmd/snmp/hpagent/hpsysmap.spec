_hpSysUsers {
	irix.kernel.all.users		NUSERS
}

_hpSysAvgJobs {
	irix.kernel.all.load		LOADAV
}

_hpSysMaxProcs {
	irix.resource.nproc		NPROCS
}

_hpSysFreeMem {
	irix.mem.freemem		FREEMEM
}

_hpSysPhysMem {
	hinv.physmem			PHYSMEM
}

_hpSysMaxUserMem {
	hinv.physmem			USERMEM
}

_hpSysSwapConfig {
	irix.swap.maxswap		MAXSWAP
}

_hpSysEnabledSwap {
	irix.swap.maxswap		SWAPENABLED
}

_hpSysFreeSwap {
	irix.swap.free			FREESWAP
}

_hpSysCPUCounts {
        irix.kernel.all.cpu.user        CPU_USER
        irix.kernel.all.cpu.sys         CPU_KERNEL
        irix.kernel.all.cpu.sxbrk       CPU_SXBRK
        irix.kernel.all.cpu.intr        CPU_INTR
        irix.kernel.all.cpu.idle        CPU_IDLE
        irix.kernel.all.cpu.wait.total  CPU_WAIT
}

