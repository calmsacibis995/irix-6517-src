#include "xp_shm_wrap.h"

int main(int argc, char *argv[])
{
    set_debug_level(10);
    dump_daem_state();
    dump_drv_state();
    return 0;
}

