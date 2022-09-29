#ifndef __TESTBITS_H__
#define __TESTBITS_H__

boolean_t DataPathOk(volatile long long *reg, int width);
boolean_t PartialDataPathOk(volatile long long *reg, int width, 
			  long long mask);

#endif
