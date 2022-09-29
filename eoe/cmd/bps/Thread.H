/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: Thread.H                                            
*    ASGN:                                                     
*    DATE: Sun Jul 16 18:16:07 1995                            
     Special thanks to twd@cs.brown.edu.
****************************************************************/


#ifndef THREAD_HEADER
#define THREAD_HEADER
#include <pthread.h>
#include <ulocks.h>
#include <assert.h>
#include <stdlib.h>

/****************************************************************
 * CLASS NAME : Semaphore
   Wrapper around the 'c semaphore' found in libC. Handles the 
   initialization. Uses lock and unlock rather than P and V, because
   frankly P and V are too cryptic.
   Note that a Semaphore can not be initialized until *AFTER* main.
   So it can not be a member of any static objects.
****************************************************************/


class Semaphore {
private:
   int max_in_;
   static void S_init();
   static pthread_once_t S_once_;
   usema_t *semaphore_;
   static usptr_t* S_mem_area_;
   friend class Thread;
   Semaphore(char){;} // need this because of
                       // initialization problems
public:
   void lock();
   Semaphore(int max_in=1);
   ~Semaphore();
   void unlock();
};

/****************************************************************
 * CLASS NAME : Thread                             
   Wrapper around the 'basic detacheable thread'. Note that the
   current pthread library does not support the detacheable thread
   so this thread acts as joinable threads without a join member 
   function.                   
  NOTE: Why a Thread class? Well a thread object has the advantage that thread
        specific data is member data and access is very cheap.
****************************************************************/


class Thread {
   Semaphore semaphore_;
   static void realStartup(void* thisp);
   Thread& operator=(Thread&);
   Thread(Thread&);
protected:
   pthread_attr_t* attr_;
   enum ThreadType{detachable, joinable};
   pthread_t thr_;
   virtual void startup() = 0;
   Thread(int):semaphore_('a'){;}
   Thread(ThreadType t);
public:
   ~Thread();
   void exit();
   void makeRunnable(); // need this because all constructors + vtbl
                               // needs to be constructed
};

/****************************************************************
 * CLASS NAME :JoinableThread
    C++ wrapper on top of the pthread joinable thread.
****************************************************************/


class JoinableThread : public Thread{
public:
   JoinableThread();
   ~JoinableThread();
   int join();
};


 /****************************************************************
 * CLASS NAME :  FirstThread
   This class guarantees that the pthreads library init structure
   is created before main. This should be removed once the pthread
   library is fixed.
****************************************************************/


template <class T>
class FirstThread {
   static int do_once_;
public:
   FirstThread<T>()
      {
         if(do_once_ == 0){
            first_thread_ = new T;
            FirstThread<T>::real_thread_ = first_thread_;
         }
         else
            {
               assert(FirstThread<T>::real_thread_ != 0);
               first_thread_ = (T*) FirstThread<T>::real_thread_;
            }
         FirstThread<T>::do_once_ ++;
      }
   static Thread* real_thread_;
   T* operator->(){return first_thread_;}
protected:
   T* first_thread_;
};

template<class T>
int FirstThread<T>::do_once_ = 0;

template<class T>
Thread* FirstThread<T>::real_thread_ = 0;

class FirstThreadInstance 
: public Thread {
   struct pthread_init init_area;
   sproc_t sid;
public:
   FirstThreadInstance();
   void startup(){};
};
static FirstThread<FirstThreadInstance> FT;

   
class Mutex {
   pthread_mutex_t mutex_;
   Mutex(Mutex& mutex);
   Mutex& operator=(Mutex& mutex);
public:
   Mutex();
   ~Mutex();
   int lock();
   int unlock();
};


/****************************************************************
 * CLASS NAME : RWLock_reader
   Readers writers lock that has a 'reader' preference
****************************************************************/


class RWLock_reader {
   pthread_mutex_t mutex_;
   pthread_cond_t readers_q_;
   pthread_cond_t writers_q_;
   int writers_;
   int readers_;

public:
   RWLock_reader();
   ~RWLock_reader();
   void rlock();
   void wlock();
   void unlock();

};
/****************************************************************
 * CLASS NAME :  RWLock_writer                                  
   Readers-writers lock that has a 'writer' preference
****************************************************************/



class RWLock_writer {
   pthread_mutex_t mutex_;
   pthread_cond_t readers_q_;
   pthread_cond_t writers_q_;
   int writers_;
   int readers_;
   int waiting_writers_;
public:
   RWLock_writer();
   ~RWLock_writer();
   void rlock();
   void wlock();
   void unlock();
};



#endif


