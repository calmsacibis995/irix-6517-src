#include <sys/types.h>

char *cpustrings[] = {
    "R4600",
    "R4600SC",
    "R5000",
    "R5000SC",
    "R5000-LM",
    "R5000SC-LM",
    "R10000",
    "R10000MP",
    "R10000-LM",
    "R10000MP-LM",
    NULL,
};

int cpubits[] = {
    0x1,        /* R4600       */
    0x2,        /* R4600SC     */
    0x4,        /* R5000       */
    0x8,        /* R5000SC     */
    0x10,       /* R5000-LM    */
    0x20,       /* R5000SC-LM  */
    0x40,       /* R10000      */
    0x80,       /* R10000MP    */
    0x100,      /* R10000-LM   */
    0x200,      /* R10000MP-LM */
};

int
gensum(char *fp, int len)
{
    long xsum;
    long *xsp;

    for (xsum = 0, xsp = (long *)fp; (uint)xsp < (uint)fp + len; xsp++)
	xsum += *xsp;
    return(-xsum);
}

int
checksum(char *fp, int len)
{
    long xsum;
    long *xsp;

    for (xsum = 0, xsp = (long *)fp; (uint)xsp < (uint)fp + len; xsp++)
	xsum += *xsp;
    return(xsum);
}
