/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: Thread.C                                            
*    ASGN:                                                     
*    DATE: Sun Jul 16 18:19:18 1995                            
****************************************************************/

#include "Thread.H"
#include <ulocks.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
usptr_t* Semaphore::S_mem_area_ = 0;
pthread_once_t Semaphore::S_once_ = PTHREAD_ONCE_INIT;

/* XXX All these assertions should be replaced by  
       but the SGI compiler handles the stl and does not
   give me exceptions :-( 
*/
Mutex::Mutex(){
   int error;
   error= pthread_mutex_init(&mutex_,NULL);
   assert(error == 0);
};

Mutex::~Mutex(){
   int error;
   error = pthread_mutex_destroy(&mutex_);
   assert(error == 0);
};

int Mutex::lock(){
   int error;
   error = pthread_mutex_lock(&mutex_);
   assert(error == 0);
   return error;
};

int Mutex::unlock(){
   int error;
   error =  pthread_mutex_unlock(&mutex_);
   assert(error == 0);
   return error;
};


/****************************************************************
 * Function Name: Thread                                        
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: Creates a thread object but does not start it.      
            Starting a thread is a two step process because of the
  race condition that exists between setting up the object and calling
  the pure virtual function 'startup'. So you call makeRunnable which guarantees
  that everything is set up correctly!
****************************************************************/

Thread::Thread(ThreadType t){
   semaphore_.lock(); 
   switch (t){
   case Thread::detachable:
      attr_ = new pthread_attr_t;
//      pthread_attr_setdetachstate(attr_,PTHREAD_CREATE_JOINABLE);
      break;
   };
  pthread_create(&thr_,NULL,(start_addr)realStartup,this);
};

Thread::~Thread(){
};

void Thread::exit(){
   pthread_exit((void*) 1);
};

void Thread::realStartup(void* thisp){
   ((Thread*)thisp)->semaphore_.lock(); // wait until make runnable is called
   ((Thread*)thisp)->startup(); 
};

void Thread::makeRunnable(){
   semaphore_.unlock(); // let the thread rip!
};

JoinableThread::JoinableThread():Thread(Thread::joinable){;}

JoinableThread::~JoinableThread(){
   delete attr_;
};

int JoinableThread::join(){
   return pthread_join(thr_,NULL);
}

Semaphore::Semaphore (int max_in) :  max_in_ (max_in) {
   pthread_once(&Semaphore::S_once_,S_init);
   semaphore_ = usnewsema(Semaphore::S_mem_area_,max_in_);
}

void Semaphore::unlock() {
   usvsema(semaphore_);
}

void Semaphore::lock() {
   uspsema(semaphore_);
}

Semaphore::~Semaphore() {
   usfreesema(semaphore_,S_mem_area_);
}

/****************************************************************
 * Function Name: S_init()                                      
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: This initializes the Semaphore. It creates the shared
            memory from which all future semaphores will allocate
   space from.
****************************************************************/



void Semaphore::S_init(){
   char* name = tmpnam(NULL);
   char* real_name = new char[strlen(name+1)];
   strcpy(real_name,name);
   assert(usconfig(CONF_ARENATYPE,US_SHAREDONLY) != -1);
   S_mem_area_ = usinit(real_name);
};

   
RWLock_reader::RWLock_reader(){
   pthread_mutex_init(&mutex_,NULL);
   pthread_cond_init(&readers_q_,NULL);
   pthread_cond_init(&writers_q_,NULL);
   readers_ = 0;
   writers_ = 0;
};

RWLock_reader::~RWLock_reader(){
   pthread_mutex_destroy(&mutex_);
   pthread_cond_destroy(&readers_q_);
   pthread_cond_destroy(&writers_q_);
};
RWLock_writer::~RWLock_writer(){
   pthread_mutex_destroy(&mutex_);
   pthread_cond_destroy(&readers_q_);
   pthread_cond_destroy(&writers_q_);
};

void RWLock_reader::wlock(){
   pthread_mutex_lock(&mutex_);
   while((readers_ > 0) || (writers_ > 0))
      pthread_cond_wait(&writers_q_,&mutex_);
   writers_++;
   pthread_mutex_unlock(&mutex_);
};

void RWLock_reader::rlock(){
   pthread_mutex_lock(&mutex_);
   while(writers_ > 0)
      pthread_cond_wait(&readers_q_,&mutex_);
   readers_++;
   pthread_mutex_unlock(&mutex_);
};

void RWLock_reader::unlock(){
   pthread_mutex_lock(&mutex_);
   if(writers_ > 0){
      writers_--;
      pthread_cond_signal(&writers_q_);
      pthread_cond_broadcast(&readers_q_);
   } else {
      if (--readers_ <= 0)
         pthread_cond_signal(&writers_q_);
   }
   pthread_mutex_unlock(&mutex_);
}

RWLock_writer::RWLock_writer(){
   pthread_mutex_init(&mutex_,NULL);
   pthread_cond_init(&readers_q_,NULL);
   pthread_cond_init(&writers_q_,NULL);
   writers_ = 0;
   readers_ = 0;
   waiting_writers_ = 0;
};

void RWLock_writer::rlock(){
   pthread_mutex_lock(&mutex_);
   while((writers_ > 0) || 
         (waiting_writers_ > 0))
      pthread_cond_wait(&readers_q_,&mutex_);
   readers_++;
   pthread_mutex_unlock(&mutex_);
}
void RWLock_writer::wlock(){
   pthread_mutex_lock(&mutex_);
   waiting_writers_++;
   while((readers_ > 0) || (writers_ > 0))
      pthread_cond_wait(&writers_q_,&mutex_);
   waiting_writers_--;
   writers_++;
   pthread_mutex_unlock(&mutex_);
};

void RWLock_writer::unlock(){
   pthread_mutex_lock(&mutex_);
   if(writers_ > 0){
      writers_--;
      if(waiting_writers_)
         pthread_cond_signal(&writers_q_);
      else
         pthread_cond_signal(&readers_q_);
   } else {
      if(--readers_ <= 0)
         pthread_cond_signal(&writers_q_);
   }
   pthread_mutex_unlock(&mutex_);
};

      
   
   





