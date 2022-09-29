#ifndef knrTESTSYSTEM__HEADER
#define knrTESTSYSTEM__HEADER
#include "knrTimer.H"
#include <iostream.h>
#include <stdarg.h>
#include <stdio.h>
#include <fstream.h>
#include <unistd.h>
/*****************************************************************
 Since I am part of the testing group, and I wanted to provide
 a clean, polymorphic interface to change output targets, I use the
 following functors. This is cleaner than pointers to functions.
*****************************************************************/
class knrOutput {
public:
  knrOutput();
  virtual void printf(char* c,...);
};
/*****************************************************************
 Does nothing, hopefully an intelligent compiler will remove it from
 the code 
*****************************************************************/
class knrNullOutput : public knrOutput{
public:
  knrNullOutput(){};
  virtual void printf(char* c, ...){};
};
/*****************************************************************
 Prints the output to stderr, prefacing it with 'ERROR :' so you can
 quickly find errors in output.
*****************************************************************/
class knrErrorOutput: public knrOutput{
public:
  knrErrorOutput(){};
  virtual void printf(char* c, ...);
};
/*****************************************************************
 Prints the output to stderr, prefacing it with 'DEBUG :' so you can 
 quickly remove debug output
*****************************************************************/
class knrDebugOutput: public knrOutput{
public:
  knrDebugOutput(){};
  virtual void printf(char* c, ...);
};
/*****************************************************************
 Prints the output to a file, prefacing it with 'MSG :' 
*****************************************************************/
class knrFileOutput: public knrOutput{
   char* msg_;
   ofstream f;
public:
  knrFileOutput(char* file_name,char* msg = "MSG :");
  virtual void printf(char* c, ...);
};

/*****************************************************************
 Prints the output to stdio, prefacing it with 'MSG :' so you can 
 quickly remove debug output
*****************************************************************/

class knrMsgOutput : public knrOutput{
public:
  knrMsgOutput(){};
  virtual void printf(char* c, ...);
};
/*****************************************************************
 all displayable class inherit from knrDisplayable. Note some classes
 may have output but are not necessarily displayable 
 
*****************************************************************/
class knrDisplayable {
protected:
  knrOutput* output_;
public:
  virtual void output() = 0;
};
/****************************************************************
 This class exists so that I can make my time class coexist with
 the knr*Output system. Notice you can add a header to the output
 by specifying str_out. The class does not delete str_out nor makes
 a copy of it.
****************************************************************/
class knrTestTime : public knrTimer, public knrDisplayable {
knrOutput* output_;
char* str_out_;
public:

  knrTestTime(knrOutput* output,char* str_out =""):knrTimer(),output_(output){
  str_out_ = str_out;}

;
  knrTestTime(const knrTestTime& time):knrTimer((knrTimer&) time){};
  const knrTestTime& operator=(const knrTestTime& time) {
    if(this == &time)     return *this;
    knrTimer::operator=((knrTimer) *this);
    return *this;
  }
  ~knrTestTime(){};
  virtual void output();
};

/*
  Standard output specifiers
*/
extern knrOutput* G_std_output;
extern knrOutput* G_nul_output;
extern knrOutput* G_err_output;
extern knrOutput* G_dbg_output;
extern knrOutput* G_msg_output;
class knrInput;
/******************************************************************
  knrTest is the interface for all test programs.
  */

class knrTest {
protected:
  ifstream ifile_;
  knrOutput* std_output_;
  knrOutput* dbg_output_;
  knrOutput* msg_output_;
public:
  knrTest(int argc, char** argv);
  virtual void Setup() = 0;
  virtual void Run() = 0;
  virtual void Queery(void* data) = 0;
  virtual void FinalResults(void* data) = 0;
  virtual void Error() = 0;
};

#endif



