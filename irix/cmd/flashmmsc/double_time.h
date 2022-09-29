/*
 * Header file for timer routines.
 *
 * $Id: double_time.h,v 1.1 1997/12/11 03:01:37 jfd Exp $
 */
#ifndef _double_time_h
#define _double_time_h



#include <sys/select.h>

double	double_time(void);
void	double_sleep(double sec);
int	double_select(int nfds, fd_set *readfds, fd_set *writefds,
		      fd_set *exceptfds, double expire);

#endif /* _double_time_h */
