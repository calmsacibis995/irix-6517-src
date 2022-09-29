#include "knrTimer.H"
#include <sys/time.h>
#include <iomanip.h>
#include <assert.h>
extern "C" gettmeofday(struct timeval *tp,...);
knrTime::knrTime(){
  gettimeofday(&time_);
};

knrTime::~knrTime(){}


knrTime::knrTime(const knrTime& knr_time){
  memcpy(&time_,&knr_time.time_,sizeof(struct timeval));
};

const knrTime& knrTime::operator=(const knrTime& knr_time){
  if(this == &knr_time){
    return *this;
  }
  memcpy(&time_,&knr_time.time_,sizeof(struct timeval));
  return *this;
};

double knrTime::sec() const {
 return time_.tv_sec + (double)time_.tv_usec/1000000.0;
};

double knrTime::diff_sec(const knrTime& knr_time) const {
  return sec() - knr_time.sec();
};
  
ostream& operator<<(ostream& os, const knrTime& time){
  
  os << setprecision(20) << "t " << time.sec() - 800000000;
  return os;
};


knrTimer::knrTimer():start_(new knrTime),end_(0){;}
knrTimer::knrTimer(const knrTimer& knr_time){
  *start_= *knr_time.start_;
  if(knr_time.end_ == 0) {
    end_ = 0;
  }
  else {
    end_ = new knrTime(*knr_time.end_);
  };
};

const knrTimer& knrTimer::operator=(const knrTimer& time){
  if(this == &time){
    return *this;
  };
  *start_ = *time.start_;
  if(time.end_ ==0){
    end_ = 0;
  }
  else{
    end_ = new knrTime(*time.end_);
  }
  return *this;
};

knrTimer::~knrTimer(){
  delete start_;
  if(end_ != 0){
    delete end_;
  }
};
void knrTimer::ReStart(){
  delete start_;
  start_= new knrTime;
  if (end_)
    delete end_;
  end_ = 0;
};

void knrTimer::Stop(){
  end_ = new knrTime;
};

double knrTimer::ElapsedTime() const
{
  assert(end_ != 0);
  return end_->diff_sec( *start_);
};

knrTime knrTimer::EndTime(){
  return *end_;
};

ostream& operator<<(ostream& os, const knrTimer& time){
  assert(time.end_ != 0);
  os <<  " start " << *time.start_ << " end " << *time.end_ << " diff " << time.ElapsedTime();
  return os;
};

double knrTimer::diff_sec(const knrTimer& timer)const {
  return ElapsedTime() - timer.ElapsedTime();
};

  
