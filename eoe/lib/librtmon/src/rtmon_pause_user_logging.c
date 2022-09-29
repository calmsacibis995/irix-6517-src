#pragma weak rtmon_pause_user_logging = _rtmon_pause_user_logging
#define rtmon_pause_user_logging _rtmon_pause_user_logging

rtmon_pause_user_logging(int cpu)
{
    user_queue[cpu]->state.enabled = 0;
}
