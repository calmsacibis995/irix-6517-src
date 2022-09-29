
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * see whether _setjmp and _longjmp can be used
 */


#include	<setjmp.h>

jmp_buf save;

main()
{
	if(_setjmp(save))
	{
		printf("extern int setjmp();\n");
		printf("#define setjmp	_setjmp\n");
		printf("#define longjmp	_longjmp\n");
		exit(0);
	}
	done();
}

done()
{
	_longjmp(save,1);
	exit(1);
}
