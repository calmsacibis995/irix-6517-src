/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved         *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 * $Ordinal-Id: fakeaio.c,v 1.6 1996/10/02 22:44:16 charles Exp $
 */

#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include <unistd.h>
#include <aio.h>

#define aio_errno	aio_sigevent.sigev_pad[0]
#define aio_ret		aio_sigevent.sigev_pad[1]
#define aio_nobytes	aio_sigevent.sigev_pad[2]

int aio_suspend(const aiocb_t * const aiocb[], int count, const timespec_t *timeout)
{
    return (0);
}

int aio_error(const aiocb_t *aio)
{
    return (aio->aio_errno);
}

ssize_t aio_return(aiocb_t *aio)
{
    return (aio->aio_ret);
}

int aio_read(aiocb_t *aio)
{
    aio->aio_errno = lseek(aio->aio_fildes, aio->aio_offset, SEEK_SET);
    if (aio->aio_errno != aio->aio_offset && errno != ESPIPE)
	return (-12345);
    aio->aio_nobytes = read(aio->aio_fildes, (void *) aio->aio_buf, aio->aio_nbytes);
    if ((int) aio->aio_nobytes < 0)
    {
	aio->aio_ret = -1;
	aio->aio_errno = errno;
    }
    else
    {
	aio->aio_ret = aio->aio_nobytes;
	aio->aio_errno = 0;
    }
    return (0);
}

int aio_write(aiocb_t *aio)
{
    aio->aio_ret = EINPROGRESS;
    aio->aio_errno = lseek(aio->aio_fildes, aio->aio_offset, SEEK_SET);
    if (aio->aio_errno != aio->aio_offset && errno != ESPIPE)
	return (-123456);
    aio->aio_nobytes = write(aio->aio_fildes, (void *) aio->aio_buf, aio->aio_nbytes);
    if ((int) aio->aio_nobytes < 0)
    {
	aio->aio_ret = -1;
	aio->aio_errno = errno;
    }
    else
    {
	aio->aio_ret = aio->aio_nobytes;
	aio->aio_errno = 0;
    }
    return (0);
}

int aio_cancel(int fd, aiocb_t *aio)
{
    return (AIO_ALLDONE);
}

void aio_sgi_init(aioinit_t *aio)
{}
