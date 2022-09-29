#ident "$Id: passhooks.c,v 1.3 1997/02/08 01:15:00 doucette Exp $"

#include	<sys/types.h>
#include	<unistd.h>
struct passwd;

int
SHisuseradmin()
{
	return (getuid() == 0);
}

/*ARGSUSED*/
void
SHenterpasswd(struct passwd *pwp)
{
	return;
}

/*ARGSUSED*/
int
SHadduser1(uid_t uid, uid_t gid)
{
	return 0;
}

/*ARGSUSED*/
int
SHmoduser(uid_t old_uid, uid_t new_uid)
{
	return 0;
}

/*ARGSUSED*/
int
SHdeluser(uid_t uid)
{
	return 0;
}

void
SHcommituser()
{
	return;
}

void
SHbackoutuser()
{
	return;
}
