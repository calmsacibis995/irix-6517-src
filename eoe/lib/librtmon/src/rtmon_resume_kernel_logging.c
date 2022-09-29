#pragma weak rtmon_resume_kernel_logging = _rtmon_resume_kernel_logging
#define rtmon_resume_kernel_logging _rtmon_resume_kernel_logging

rtmon_resume_kernel_logging(int cpu)
{
    if (syssgi(SGI_RT_TSTAMP_START, cpu) < 0) {
        perror("[rtmon_start_kernel_sampling]: syssgi SGI_RT_TSTAMP_START");
        exit(0);
    }
}
