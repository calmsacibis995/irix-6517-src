#pragma weak rtmon_resume_user_logging = _rtmon_resume_user_logging
#define rtmon_resume_user_logging _rtmon_resume_user_logging

rtmon_resume_user_logging(int cpu)
{
    user_queue[cpu]->state.enabled = 1;
}
