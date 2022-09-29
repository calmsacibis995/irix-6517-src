#include "uif.h"
#include <sys/IP32.h>

int WalkingZero(volatile long long *reg, int width)
{
    long long value, data;
    int i;

    /* Walk a zero through the register */
    msg_printf(DBG,"WalkingZero: reg==0x%x,width==0x%x\n",
							reg, width);
    value = ~1;
    for (i = 0; i < width; i++) {
	WRITE_REG64(value,reg,long long);
	data = READ_REG64(reg,long long);
	if (data != value) {
    	    msg_printf(ERR, "WalkingZero for 0x%x error bit %d\n", reg, i);
	    return(i+1);
	}
	value = (value << 1) | 1;
    }
    msg_printf(DBG, "WalkingZero for 0x%x OK\n", reg);
    return(0);
}

int WalkingOne(volatile long long *reg, int width)
{
    long long value, data;
    int i;

    /* Walk a one through the register */
    msg_printf(DBG,"WalkingOne: reg==0x%x,width==0x%x\n",
							reg, width);
    value = 1;
    for (i = 0; i < width; i++) {
	WRITE_REG64(value,reg,long long);
	data = READ_REG64(reg,long long);
	if (data != value) {
    	    msg_printf(ERR, "WalkingOne for 0x%x error bit %d\n", reg, i);
	    return(i+1);
	}
	value = (value << 1);
    }
    msg_printf(DBG, "WalkingOne for 0x%x OK\n", reg);
    return(0);
}

int PartialWalkingZero(volatile long long *reg, int width, long long bits)
{
    long long value, data, mask;
    int i;

    /* Walk a zero through the register */
    msg_printf(DBG,"PartialWalkingZero: reg==0x%x,width==0x%x,bits==0x%llx\n",
							reg, width, bits);
    mask = 1;
    value = ~1;
    for (i = 0; i < width; i++) {
	if (bits & mask) {
	    WRITE_REG64(value,reg,long long);
/*	    data = (READ_REG64(reg,long long) | bits);*/
	    data = READ_REG64(reg,long long);
	    if (data != value) {
    	    	msg_printf(ERR, "PartialWalkingZero for 0x%x error bit %d\n", reg, i);
		msg_printf(ERR, "data==0x%llx, value==0x%llx\n", data, value);
	    	return(i+1);
	    }
	}
	value = (value << 1) | 1;
	mask = (mask << 1);
    }
    msg_printf(DBG, "PartialWalkingZero for 0x%x OK\n", reg);
    return(0);
}

int PartialWalkingOne(volatile long long *reg, int width, long long bits)
{
    long long value, data, mask;
    int i;

    /* Walk a one through the register */
    msg_printf(DBG,"PartialWalkingOne: reg==0x%x,width==0x%x,bits==0x%llx\n",
							reg, width, bits);
    mask = value = 1;
    for (i = 0; i < width; i++) {
	if (bits & mask) {
	    WRITE_REG64(value,reg,long long);
	    data = READ_REG64(reg, long long);
	    if (data != value) {
    	    	msg_printf(ERR, "PartialWalkingOne for 0x%x error bit %d\n", reg, i);
	    	return(i+1);
	    }
	}
	value = (value << 1);
	mask = (mask << 1);
    }
    msg_printf(DBG, "PartialWalkingOne for 0x%x OK\n", reg);
    return(0);
}
