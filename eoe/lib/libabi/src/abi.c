#ifdef  _LIBABI_ABI
#include <aio.h>
#include <ABIinfo.h>
#include <sys/mpconf.h>

int aio_read(aiocb_t *aio)
{
	return(0);
}

int aio_write(aiocb_t *aio)
{
	return(0);
}

int lio_listio(int mode, aiocb_t * const list[], int nent, sigevent_t *sig)
{
	return(0);
}

int aio_error(const aiocb_t *a)
{
	return(0);
}

ssize_t aio_return(aiocb_t *a)
{
	return(0);
}

int aio_cancel(int fd, aiocb_t *a)
{
	return(0);
}

int aio_suspend(const aiocb_t * const list[], int nent,
		const timespec_t *tim)
{
	return(0);
}

int aio_fsync(int op, aiocb_t *io)
{
	return(0);
}

int aio_hold(int sh)
{
	return(0);
}

int ABIinfo(int sel, int op)
{
	return(0);
}

int mpconf(int cmd, ...)
{
	return(0);
}

#if !_NO_LFAPI
int aio_read64(aiocb64_t *aio)
{
	return(0);
}

int aio_write64(aiocb64_t *aio)
{
	return(0);
}

int lio_listio64(int mode, aiocb64_t * const list[], int nent, sigevent_t *sig)
{
	return(0);
}

int aio_error64(const aiocb64_t *a)
{
	return(0);
}

ssize_t aio_return64(aiocb64_t *a)
{
	return(0);
}

int aio_cancel64(int fd, aiocb64_t *a)
{
	return(0);
}

int aio_suspend64(const aiocb64_t * const list[], int nent,
		const timespec_t *tim)
{
	return(0);
}

int aio_fsync64(int op, aiocb64_t *io)
{
	return(0);
}

int aio_hold64(int sh)
{
	return(0);
}

#endif	/* !_NO_LFAPI */
#else	/* !_LIBABI_ABI follows */
/*
 * In the non-MIPS ABI library case no code executes here!
 * All the "real" procedures reside in libc which support native abi
 * calls. We simply define a place holder procedure to create
 * a non-empy '.so'.
 */
void
__libabi_dummy(void)
{
}

#endif	/* _LIBABI_ABI */
