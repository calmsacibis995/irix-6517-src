/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) vmem.c:3.2 5/30/92 20:19:02" };
#endif

#include "vmem.h"
#include <stdio.h>

#define SEG_ADDR(var,i) ((char *)(var->seg_tbl[i/(SEGMENT/var->el_siz)]))
#define min(x,y) ((x)<(y)?(x):(y))

char *calloc();

varray *
vcalloc(num,siz)
long num;
unsigned int siz;
{
    varray *tmp;
    long elements;
    long segment;
    unsigned int seg_num;

    if((tmp = (varray *)calloc(1,sizeof (varray))) == (varray *)NULL)
	return tmp;
    tmp->n_elem = num;
    tmp->el_siz = siz;
    if(siz == 0 || num == 0)  {
	if((tmp->seg_tbl = (char **)calloc(0,0)) == (char **)NULL)  {
	    free(tmp);
	    return((varray *)NULL); }
	return tmp;
    }
    seg_num = (num + (SEGMENT/siz) - 1) / (SEGMENT/siz);
    fflush(stdout);
    tmp->seg_tbl = (char **)calloc(seg_num,sizeof (char **));
    if(tmp->seg_tbl  == (char **)NULL)  {
	free(tmp);
	return((varray *)NULL);
    }
    for(segment = 0;segment < seg_num;segment++)  {
	elements = min((SEGMENT/siz),num - segment * (SEGMENT/siz));
	tmp->seg_tbl[segment] = calloc(elements,siz);
	if(tmp->seg_tbl[segment] == NULL)  {
	    for(segment--;segment >= 0;segment--)
		free(tmp->seg_tbl[segment]);
	    free(tmp->seg_tbl);
	    free(tmp);
	    return((varray *)NULL);
	}
	fflush(stdout);
    }
    return tmp;
}


char *
vaddr(var,el)
varray *var;
long el;
{
    char *seg;

    if(el > var->n_elem || el < 0)  {	/* outside valid range */
	return NULL;
    }
    if(var->n_elem == 0 || var->el_siz == 0)  {
	return((char *)var->seg_tbl);	/* nothing there.  better not use it */
    }
    seg = SEG_ADDR(var,el);
    return (seg + (el % (SEGMENT / var->el_siz)) * var->el_siz);
}


vfree(var)
varray *var;
{
    long index;
    long seg_num;

    seg_num = (var->n_elem + (SEGMENT/var->el_siz) - 1);
    seg_num /= (SEGMENT/var->el_siz);
    for(index = 0;index < seg_num;index++)  {
	free(var->seg_tbl[index]);
    }
    free(var->seg_tbl);
    free(var);
}

velsize(var)
varray *var;
{
    return(var->el_siz);
}

long
varsize(var)
varray *var;
{
    return(var->el_siz * var->n_elem);
}

vqsort(var,base,nel,cmp)
varray *var;
long base,nel;
int (*cmp)();
{
    long bottom,pivot,top;

    bottom = base ;top = base + nel;pivot = base;
    if(nel <= 1)
	return;
    if(nel == 2)  {
	if((*cmp)(vaddr(var,bottom),vaddr(var,top)) > 0)
	    swap(vaddr(var,bottom),vaddr(var,top),velsize(var));
	return;
    }
    while(top > bottom)  {
	do  {
	    top--;
	}  while ((*cmp)(vaddr(var,top),vaddr(var,pivot)) > 0);
	do  {
	    bottom++;
	}  while ((*cmp)(vaddr(var,bottom),vaddr(var,pivot)) < 0);
	if(top > bottom)
	    swap(vaddr(var,top),vaddr(var,bottom),velsize(var));
    }
    swap(vaddr(var,top),vaddr(var,pivot),velsize(var));
    top++ ; bottom-- ;
    vqsort(var,base,(bottom-base)+1,cmp);
    vqsort(var,top,(base + nel) - top,cmp);
}

swap(ptr1,ptr2,siz)
register char *ptr1,*ptr2;
int siz;
{
    register unsigned int i;
    register char tmp;

    if(ptr1 == ptr2)
	return;
    for(i = 0;i < siz;i++)  {
	tmp = *ptr1;
	*ptr1++ = *ptr2;
	*ptr2++ = tmp;
    }
}
