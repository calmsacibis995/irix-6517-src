#include <pthread.h>
#include <Thread.H>
FirstThreadInstance::FirstThreadInstance()
:Thread(5){
   init_area.conf_initsize = 1048576;
   init_area.max_sproc_count = 0;
   init_area.sproc_stack_size = 0;
   init_area.os_default_priority = 0;
   init_area.os_sched_signal = 33;


   if(pthread_exec_begin(&init_area) == -1){
      perror("exec_begin");
      exit();
  
   }

  }    
