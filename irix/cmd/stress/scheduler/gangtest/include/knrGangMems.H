

#ifndef knrPROCESS__HEADER
#define knrPROCESS__HEADER


#include "knrTestSystem.H"
#include <stdio.h>
#include <ulocks.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/param.h>
#include <sys/schedctl.h>
#include <stdarg.h>
#include <unistd.h>
/*************************************************************
  knrGangMaster 
  This class represents the gang master in a gang of processes.
      
*************************************************************/
class knrGangMember;
class knrGangRealMaster;
class knrGangMaster{
public:
  enum Mode {FREE= SGS_FREE,
	     EQUAL = SGS_EQUAL,
	     SINGLE = SGS_SINGLE,
	     GANG = SGS_GANG};
  knrGangRealMaster* master_;
public:
  knrGangMaster(int num_mems, Mode mode);
  void InitMember(knrGangMember* member, int index);
  void start();
  void wait();
  int NumMems() const;
  int SetMode(Mode mode);
  int terminate(int thread);
};

class knrGangRealMaster{
protected:
   knrGangMaster::Mode mode_;
   int num_mems_;
public:
   virtual void start() = 0;
   virtual void InitMember(knrGangMember* member, int index) = 0;
   virtual void wait() = 0;
   virtual int terminate(int thread) = 0;
   int NumMems() const {return num_mems_;}
   int SetMode(knrGangMaster::Mode mode);
protected:
   knrGangRealMaster(knrGangMaster::Mode mode, int num_mems);
private:
   knrGangRealMaster();
   knrGangRealMaster(knrGangRealMaster& k);
   knrGangRealMaster& operator=(knrGangRealMaster& k);
};

class knrGangSingleMaster : public knrGangRealMaster{
protected:
   knrGangMember* gang_member_;
public: 
   knrGangSingleMaster(knrGangMaster::Mode, int num_mems);
   virtual void start();
   virtual void InitMember(knrGangMember* member, int index);
   virtual int terminate(int thread = 0);
   virtual void wait();
};
class knrGangMultiMaster : public knrGangRealMaster {
protected:
   knrGangMember** gang_members_;
public:
   knrGangMultiMaster(int num_mems,
                      knrGangMaster::Mode mode);
   virtual void start();
   virtual void InitMember(knrGangMember* member, int index);
   virtual void wait();
   virtual int terminate(int thread);
private:
   knrGangMultiMaster();
   knrGangMultiMaster(knrGangMultiMaster& k);
   knrGangMultiMaster& operator=(knrGangMultiMaster& k);
};
   

   
/*************************************************************
  knrGangMember
*************************************************************/
class knrGangMember{
protected:
  int id_;
  int gang_id_;
public:
  knrGangMember(){};
  knrGangMember(int id,int gang_id);
  static void start(void* thisp);
  virtual void work_func() = 0; // this function is called by the 
                                // static start function
  virtual void terminate() = 0; // this is called by the gang master
                                // when you want to terminate a gang

};
/*************************************************************
 ItGangMember
 This subclass of knrGangMember demonstrates how to use the 
 knrGang* classes. The work_func here is defined and computes something
 that takes a long time.
*************************************************************/
class ItGangMember : public knrGangMember{
  int num_;
  knrOutput* output_; // where output from this function is to go
public:
  enum {SOME_BIG_NUMBER = 1};
  ItGangMember(int id, int gang_id,int num = SOME_BIG_NUMBER, 
	       knrOutput* output = G_std_output );
  virtual void work_func(); 
  virtual void terminate();
};
	       
#endif







