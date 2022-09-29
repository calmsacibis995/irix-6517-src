#include <sys/types.h>

int cycle_init(volatile uint64_t **counter_ret, uint64_t *cycleval_ret);
uint64_t cycle_usecs_to_ticks(uint64_t cycleval, double usecs);
double cycle_ticks_to_usecs(uint64_t cycleval, uint64_t ticks);
