/*
 * This library exists only beacuse the Xopen standard says it has to.
 * All the "real" procedures, however, reside in libc which support native
 * POSIX real-time system calls. We sinply define a place holder procedure
 * to create a non-empy '.so'.
 */
void
__librt_dummy(void)
{
}
