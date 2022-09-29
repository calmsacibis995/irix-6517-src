#include <stdarg.h>
#include "knrTestSystem.H"
#include "knrTimer.H"
#include <stdio.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>
#include <assert.h>
knrOutput* G_nul_output = new knrNullOutput;
knrOutput* G_std_output = new knrOutput;
knrOutput* G_err_output = new knrErrorOutput;
knrOutput* G_dbg_output = new knrDebugOutput;
knrOutput* G_msg_output = new knrMsgOutput;

 knrOutput::knrOutput(){}
void knrOutput::printf(char* c, ...){
  va_list args;
  char vpbuf[BUFSIZ];
  
  va_start(args,c);
  vsprintf(vpbuf,c,args);
  cout  << vpbuf;
  va_end(args);
}
void knrErrorOutput::printf(char* c, ...){
  va_list args;
  char vpbuf[BUFSIZ];
  char *buf;
  strstream a;
  va_start(args,c);
  vsprintf(vpbuf,c,args);
  a << "ERROR : "<< vpbuf << endl;
  buf =a.str();
  cerr << buf;
  va_end(args);
  delete buf;
}

void knrDebugOutput::printf(char* c, ...){
  va_list args;
  char vpbuf[BUFSIZ];
  char *buf;
  strstream a;
  va_start(args,c);
  vsprintf(vpbuf,c,args);
  a << "DEBUG : "<< vpbuf << endl;
  buf =a.str();
  cerr << buf;
  va_end(args);
  delete buf;
}

knrFileOutput::knrFileOutput(char* file_name,char* msg) 
 : f(file_name){
    msg_ = msg;
 if(!f){
    G_dbg_output->printf("Could not open %s for standard output.",file_name);
    exit(1);
 };
};
   
void knrFileOutput::printf(char* c,...){
  va_list args;
  char vpbuf[BUFSIZ];
  char *buf;
  strstream a;
  va_start(args,c);
  vsprintf(vpbuf,c,args);
  a << msg_ << vpbuf;
  buf = a.str();
  f << buf << endl;
  va_end(args);
  delete buf;
}

void knrMsgOutput::printf(char* c,...){
  va_list args;
  char vpbuf[BUFSIZ];
  char *buf;
  strstream a;
  va_start(args,c);
  vsprintf(vpbuf,c,args);
  a << "MSG : "<< vpbuf << endl;
  buf =a.str();
  cout << buf;
  va_end(args);
  delete buf;
}
void knrTestTime::output(){
  char *buf;
  strstream a;
  assert(end_ != 0);
  a << " start " << *start_ << " end " << *end_ ;
  buf = a.str();
  output_->printf("%s %s diff %10.10f ",str_out_,buf,ElapsedTime());
  delete buf;

};

knrTest::knrTest(int argc, char** argv){
/*  if(argc < 1) {
    G_err_output->printf("no input file specified\n");
    exit(0);
    };
  ifile_.open(argv[1],ios::in,filebuf::openprot);
  if(!ifile_){
    G_err_output->printf("input file specified could not be opened\n");
    exit(0);
  };

  char c;
  std_output_ = G_std_output;
  dbg_output_ = G_dbg_output;
  msg_output_ = G_msg_output;
  while((c = getopt(argc,argv,"s:d:m")) != EOF){
    switch(c) {
    case 's':
      std_output_ = G_nul_output;
      break;
    case 'n':
      dbg_output_ = G_dbg_output;
      break;
    case 'm':
      msg_output_ = G_msg_output;
      break;
    };
  } */
};
