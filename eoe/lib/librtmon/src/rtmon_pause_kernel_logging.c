#pragma weak rtmon_pause_kernel_logging = _rtmon_pause_kernel_logging
#define rtmon_pause_kernel_logging _rtmon_pause_kernel_logging

rtmon_pause_kernel_logging(int cpu)
{
   if (syssgi(SGI_RT_TSTAMP_STOP, cpu) < 0) {
        perror("[rtmon_start_kernel_sampling]: syssgi SGI_RT_TSTAMP_STOP");
        exit(0);
    }
}
