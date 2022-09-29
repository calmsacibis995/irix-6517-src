#ifdef description

Date: Fri, 8 Jan 93 20:01:08 -0800
Message-Id: <9301090401.AA23169@midas.wpd.sgi.com>
From: pv@relay.sgi.com   (geoff@shamuneko.asd)
To: yohn@asd.sgi.com
Reply-To: sgi.bugs.sherwood@sgi.com
Subject: ADMIN 124315 - BUG - file locking error at max file size


Assigned Engineer : yohn              Submitter : paulm  Status : open      
Priority : 2       

Description :
If you attempt to lock a file with fcntl(F_SETLK) using a flock
struct with the following parameters:

l_whence == 0
l_start == 0x7fffffff
l_len == 1

it succeeds on EFS and fails on NFS.  I claim that it should
fail on EFS, since the specified offset is actually one bigger
than the maximum permissible offset.  If you use offset 0x7ffffffe
it works on both EFS and NFS.  Here's a sample program:

#endif

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

extern int errno;

main(argc, argv)
int argc;
char **argv;
{
	int	fd, ret;
	static struct flock flk;

#ifdef notdef
	if ((fd = open("foo.c", O_RDWR)) < 0) {
		perror("open failed for foo.c");
		exit(1);
	}

	flk.l_type   = F_WRLCK ;
#else
	if ((fd = open(argv[0], O_RDONLY)) < 0) {
		printf("INFO: test %s: open of file %s failed, must abort before performing flock test\n",
			argv[0], argv[0], errno);
		perror("open failed");
		exit(1);
	}

	flk.l_type   = F_RDLCK ;
#endif
	flk.l_whence = 0;
	flk.l_start  = 0x7FFFFFFFL;  /* max offset */
	flk.l_len    = 0x00000001L;

	ret = fcntl(fd, F_SETLKW, &flk);
	
	if (ret == 0) {
		flk.l_type = F_UNLCK;
		fcntl(fd, F_SETLKW, &flk);
	} else {
		printf("ERROR: test %s: fcntl F_SETLKW (offset 0x%x, len %d) failed, errno %d\n",
			argv[0], flk.l_start, flk.l_len, errno);
	}

	return ret;
}


#ifdef description

Paul Mielke (paulm@sgi.com)

==========================
ADDITIONAL INFORMATION (ADD)
From: paulm@slip-kestrel
Date: Nov 24, 1992
==========================

Sigh.  I have spoken directly with the person from Siemens who reported the
original bug on which this is based.  I rejected the original saying that
NFS was justified in rejecting the lock request at 0x7fffffff as a bad offset.

They (SNI) admit that we are right about this, but plead that it is an
interoperability issue.  Apparently C-ISAM does locking at this offset
and it works with other vendor's NFS and local file systems.  We can't
force Informix and all other purveyors of C-ISAM to change their code.
Thus we are forced to make our NFS accept this bogus lock request.

This is a serious issue for SNI and I promised them that we'd break (ahem, fix)
our code for them in Sherwood.

#endif
