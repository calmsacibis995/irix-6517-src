#if !defined(__DATAPATH_H__)

#define __DATAPATH_H__

int WalkingZero(volatile long long *reg, int width);
int WalkingOne(volatile long long *reg, int width);

int PartialWalkingZero(volatile long long *reg, int width, long long bits);
int PartialWalkingZero(volatile long long *reg, int width, long long bits);

#endif
