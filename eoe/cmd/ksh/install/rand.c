
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * limit random numbers to the range 0 to 2^15
 * some pre-processors require an argument so a dummy one is used
 */

main()
{
	unsigned  max=0;
	int n=100;
	unsigned x =0;
	if(sizeof(int)==2)
	{
		printf("#define sh_rand(x) rand(x)\n");
		exit(0);
	}
	srand(getpid());
	while(n--)
	{
		x=rand();
		if(x>max)
			max=x;
	}
	if(max > 077777)
		printf("#define sh_rand(x) ((x),(rand()>>3)&077777)\n");
	else
		printf("#define sh_rand(x) ((x),rand())\n");
	exit(0);
}
