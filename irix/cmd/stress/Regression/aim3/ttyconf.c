/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) ttyconf.c:3.2 5/30/92 20:18:55" };
#endif

#include "ttytest.h"
#include "testerr.h"
#include "suite3.h"

#ifndef SYS5

setraw(x)
ttyinf *x;
{
    x->sg_flags |= EVENP;
    return (x->sg_flags |= RAW);
}

unsetraw(x)
ttyinf *x;
{
    x->sg_flags |= EVENP;
    return(x->sg_flags&=~RAW);
}

echoff(x)
ttyinf *x;
{
    return(x->sg_flags&=~ECHO);
}

mapoff(x)
ttyinf *x;
{
    return(x->sg_flags&=~CRMOD);
}

setspeed(arg,speed)
ttyinf *arg;
int speed;
{
    arg->sg_ispeed = arg->sg_ospeed = speed;
}

#else		/* is SYS5 */

echoff(x)
ttyinf *x;
{
    return(x->c_lflag&=~ECHO);
}

mapoff(x)
ttyinf *x;
{
    return(x->c_oflag&=~OPOST);
}

setspeed(arg,speed)
ttyinf *arg;
int speed;
{
    arg->c_cflag &= ~CBAUD;
#ifdef SYS5
    if(speed == B110)
	arg->c_cflag |= CSTOPB;
#endif
    arg->c_cflag |= speed;
}

setraw(arg)
ttyinf *arg;
{
    arg->c_lflag &= ~(ICANON|ISIG);
    arg->c_oflag &= ~OPOST;
    arg->c_iflag &= ~(IGNPAR|INLCR|ICRNL|IUCLC|IXON|IXOFF|BRKINT);
    arg->c_iflag |= (IGNBRK|ISTRIP|PARMRK);
    arg->c_cflag |= (CREAD|HUPCL|CS8|CLOCAL);
    arg->c_cflag &= ~PARENB;
    arg->c_lflag &= ~ECHO;
    arg->c_cc[4] = 1;			/* EOF char becomes the MIN count */
    arg->c_cc[5] = 0;			/* EOL char becomes the TIME val */
}

unsetraw(arg)				/* not really cooked */
ttyinf *arg;
{
    arg->c_lflag |= (ICANON|ISIG|ECHO);
    arg->c_oflag &= ~OPOST;
    arg->c_iflag &= ~(IGNPAR|INLCR|ICRNL|IUCLC|IXON|IXOFF|BRKINT);
    arg->c_iflag |= (IGNBRK|ISTRIP|PARMRK);
    arg->c_cflag |= (CREAD|HUPCL|CS8|CLOCAL);
    arg->c_cflag &= ~PARENB;
    arg->c_cc[4] = '\04';		/* reset the End Of File char */
    arg->c_cc[5] = '\0';		/* reset the End Of Line char */
}

#endif		/* SYS5 */

tflush(fd)				/* this should flush queues on any */
int fd;					/* version			   */
{
    ioctl(fd,TERMFLUSH,2);
}
