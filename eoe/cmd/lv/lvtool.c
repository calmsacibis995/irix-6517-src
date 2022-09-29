/* Logical volume test utility: dumps the current state of an initialized
 * logical volume & allows change. 
 * Purely for test purposes, NOT a distributed utility.
 */

#ident "$Revision: 1.2 $"

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/dkio.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <errno.h>
#include "sys/lv.h"

unsigned getnum();
struct lvioarg lio;

main(ac, av)
int ac;
char **av;
{
lvptr lv;
int fd;
int lsiz;

	if (ac < 2) exit(1);
	lsiz = (sizeof(struct lv) + (21 * sizeof(struct physdev)));
	lv = (lvptr)malloc(lsiz);
	lio.size = lsiz;
	lio.lvp = lv;

	if ((fd = open(av[1], O_RDWR)) < 0)
	{
		perror("Can't open lv device\n");
		exit(1);
	}

	if (ioctl(fd, DIOCGETLV, &lio) < 0)
	{
		perror("IOCTL error");
		exit(1);
	}

	lvprint(lv);
	if (!yes("Alter"))exit(0);
	newlv(lv);
	printf("New lv: \n");
	lvprint(lv);
	if (!yes("Write"))exit(0);
	lsiz = (sizeof(struct lv) + 
		((lv->l_ndevs - 1) * sizeof(struct physdev)));
	lio.size = lsiz;

	if (ioctl(fd, DIOCSETLV, &lio) < 0)
	{
		perror("IOCTL error");
		exit(1);
	}

}


/* lv print func */

lvprint(lv)
lvptr lv;
{
unsigned i, j;
struct physdev *pd;
printf("lv: flags 0x%x ndevs %d stripe %d gran %d size %d align %d\n",
	lv->l_flags,
	lv->l_ndevs,
	lv->l_stripe,
	lv->l_gran,
	lv->l_size,
	lv->l_align);

	if ((i = lv->l_ndevs) > 20)
	{
		printf("Ridiculous ndevs!!\n");
		i = 20;
	}
	for (j = 0, pd = &lv->pd[0]; j < i; j++, pd++)
	{
		printf("physdev %d dev 0x%x start %d size %d\n",
			j, 
			pd->dev,
			pd->start,
			pd->size);
	}
}

/* lv edit func */

#define GETNUM(x) (x = (n = getnum()) ? n : x)

newlv(lv)
lvptr lv;
{
unsigned i, j;
struct physdev *pd;
unsigned maj, min;
unsigned n;


printf("flags 0x%x : ",lv->l_flags); lv->l_flags = getnum();
printf("ndevs %d : ",lv->l_ndevs); GETNUM(lv->l_ndevs); 
printf("stripe %d : ",lv->l_stripe); GETNUM(lv->l_stripe); 
printf("gran %d : ",lv->l_gran); GETNUM(lv->l_gran); 
printf("size %d : ",lv->l_size); GETNUM(lv->l_size); 
printf("align %d : ",lv->l_align); GETNUM(lv->l_align); 

	if ((i = lv->l_ndevs) > 20)
	{
		printf("Ridiculous ndevs!!\n");
		i = 20;
	}
	for (j = 0, pd = &lv->pd[0]; j < i; j++, pd++)
	{
		printf("physdev %d:\n", j);
		maj = major(pd->dev);
		printf("major 0x%x : ",maj); GETNUM(maj);
		min = minor(pd->dev);
		printf("minor 0x%x : ",min); GETNUM(min);
		pd->dev = makedev(maj, min);
		printf("start %d :", pd->start); GETNUM(pd->start);
		printf("size %d :", pd->size); GETNUM(pd->size);
	}
	if (j >= 20) return;
	else if (!yes("More")) return;
	for (; j < 20; j++, pd++)
	{
		printf("physdev %d:\n", j);
		maj = major(pd->dev);
		printf("major 0x%x : ",maj); GETNUM(maj);
		min = minor(pd->dev);
		printf("minor 0x%x : ",min); GETNUM(min);
		pd->dev = makedev(maj, min);
		printf("start %d :", pd->start); GETNUM(pd->start);
		printf("size %d :", pd->size); GETNUM(pd->size);
		if (!yes("More")) return;
	}
}

unsigned
getnum()
{
unsigned n;
char b[100];

	gets(b);
	n = (unsigned)strtol(b, 0, 0);
	return n;
}

yes(s)
char *s;
{
char b[100];

	printf("%s? ", s);
	gets(b);
	if ((*b == 'y') || (*b == 'Y')) return 1;
	else return 0;
}

