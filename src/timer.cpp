#include "timer.h"
#include "debug.h"
#include <vector>
#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sstream>
#include <string>

using namespace std;

string timevalToString(timeval& t){
	stringstream sstr;

	if((t.tv_sec + (t.tv_usec/uS_IN_SEC)) < 5){
		if(t.tv_usec < 1000 && t.tv_sec==0){
			sstr<<TIMEVAL_TO_uS_64(t)<<"us";
			return sstr.str();
		}
		sstr<<TIMEVAL_TO_MS(t)<<"ms";
		return sstr.str();
	}

	if(t.tv_sec > 3000){
		int minutes = (t.tv_sec/60) % 60;
		int hours = t.tv_sec/(60*60);
		float secs = (float)(t.tv_sec%60) + ((float)t.tv_usec/(float)uS_IN_SEC);

		sstr<<hours<<":"<<minutes<<":"<<secs<<"hms";
	}
	else{
		float secs=TIMEVAL_TO_SEC_F(t);
		sstr<<secs<<"sec";
	}

	return sstr.str();
}

/*void printTimeval(timeval& t){

}*/

int getNow(struct timeval &tv) {
	timerclear(&tv);
	if (gettimeofday(&tv, NULL)) {
		perror("getNow() gettimeofday()");
		return 0;
	}
	return 1;
}

// Returns (now - tv_prior) in microseconds
uint64_t getSince(struct timeval &tv_prior) {
	struct timeval tv;
	struct timeval result;
	timerclear(&tv);
	if (gettimeofday(&tv, NULL)) {
		perror("getSince() gettimeofday()");
		return 0;
	}
	timersub(&tv, &tv_prior, &result);
	return TIMEVAL_TO_uS_64(result);
}

uint64_t getSinceMs(struct timeval &tv_prior) {
	return getSince(tv_prior)/1000;
}


Timer::Timer(bool auto_start){
	reset();
	if(auto_start){
		start();
	}
	running=auto_start;
}

bool Timer::isGood(){
	return !overflow;
}

bool Timer::isRunning(){
	return running;
}

void Timer::start(){
	if(running || overflow){
		return;
	}

	running=true;
	struct rusage usageTemp;

	gettimeofday(&real_start, NULL);
	getrusage(RUSAGE_SELF, &usageTemp);
	user_start=usageTemp.ru_utime;
	sys_start=usageTemp.ru_stime;
	vcsw_start=usageTemp.ru_nvcsw;
	ivcsw_start=usageTemp.ru_nivcsw;
}

void Timer::stop(){
	if(running){
		running=false;
		overflow = updateElapsed();
		if(overflow){
			ERROR("ERROR, Elapsed time is not valid!"<<endl);
			ABORT_NOW();
		}
	}
}

void Timer::resume(){
	if(!running && !overflow){
		start();
	}
}

void Timer::reset(){
	running=false;
	overflow=false;
	memset(&real_start, 0, sizeof(timeval));
	memset(&user_start, 0, sizeof(timeval));
	memset(&sys_start, 0, sizeof(timeval));
	memset(&real_elapsed, 0, sizeof(timeval));
	memset(&user_elapsed, 0, sizeof(timeval));
	memset(&sys_elapsed, 0, sizeof(timeval));
	vcsw_elapsed=0;
	vcsw_start=0;
	ivcsw_elapsed=0;
	ivcsw_start=0;
}

void Timer::restart(){
	reset();
	start();
}

void Timer::print(){
	PRINT_MSG("real\t"<<timevalToString(real_elapsed)<<endl);
	PRINT_MSG("user\t"<<timevalToString(user_elapsed)<<endl);
	PRINT_MSG("sys\t"<<timevalToString(sys_elapsed)<<endl);
	PRINT_MSG("vcsw\t"<<vcsw_elapsed<<endl);
	PRINT_MSG("ivcsw\t"<<ivcsw_elapsed<<endl);
}

void Timer::printLine(){
	PRINT_MSG("["<<timevalToString(real_elapsed)	\
		  <<", "<<timevalToString(user_elapsed)	\
		  <<", "<<timevalToString(sys_elapsed)  \
		  <<", "<<vcsw_elapsed                  \
		  <<", "<<ivcsw_elapsed<<"]"<<endl);
}

string Timer::toString(){
	stringstream sstr;
	sstr<<"["<<timevalToString(real_elapsed)	\
		  <<", "<<timevalToString(user_elapsed)	\
		  <<", "<<timevalToString(sys_elapsed)  \
		  <<", "<<vcsw_elapsed                  \
		  <<", "<<ivcsw_elapsed<<"]";

	return sstr.str();
}

void Timer::toXml(ostream& stream){
	stream<<"<timer>"<<endl;
	stream<<"<time type=\"real\" sec=\""<<real_elapsed.tv_sec<<"\" "<<"usec=\""<<real_elapsed.tv_usec<<"\"/>"<<endl;
	stream<<"<time type=\"user\" sec=\""<<user_elapsed.tv_sec<<"\" "<<"usec=\""<<user_elapsed.tv_usec<<"\"/>"<<endl;
	stream<<"<time type=\"sys\" sec=\""<<sys_elapsed.tv_sec<<"\" "<<"usec=\""<<sys_elapsed.tv_usec<<"\"/>"<<endl;
	stream<<"</timer>"<<endl;
}

timeval Timer::getElapsedSys() {
	stop();
	resume();
	return sys_elapsed;
}

timeval Timer::getElapsedReal() {
	stop();
	resume();
	return real_elapsed;
}

timeval Timer::getElapsedUser() {
	stop();
	resume();
	return user_elapsed;
}

bool Timer::updateElapsed() {
	bool overflow=false;
	struct rusage usageTemp;
	struct timeval timeTemp, real_now;

	gettimeofday(&real_now, NULL);
	getrusage(RUSAGE_SELF, &usageTemp);

	//Calculate context switches
	vcsw_elapsed=usageTemp.ru_nvcsw-vcsw_start;
	ivcsw_elapsed=usageTemp.ru_nivcsw-ivcsw_start;

	//Calculate elapsed real time
	overflow |= !subTimeval(real_now, real_start, timeTemp);
	overflow |= !Timer::addTimeval(real_elapsed, timeTemp, real_elapsed);

	//Calculate elapsed user time
	overflow |= !subTimeval(usageTemp.ru_utime, user_start, timeTemp);
	overflow |= !addTimeval(user_elapsed, timeTemp, user_elapsed);

	//Calculate elapsed system time
	overflow |= !subTimeval(usageTemp.ru_stime, sys_start, timeTemp);
	overflow |= !addTimeval(sys_elapsed, timeTemp, sys_elapsed);

	return overflow;
}


bool Timer::addTimeval(timeval initial, timeval delta, timeval& result) {
	bool overflow=false;
	overflow |= !factorTimeval(initial);
	overflow |= !factorTimeval(delta);

	result.tv_sec = initial.tv_sec + delta.tv_sec;
	result.tv_usec = initial.tv_usec + delta.tv_usec;
	overflow |= !factorTimeval(result);

	//Check for overflows
	if(overflow || result.tv_sec < initial.tv_sec || result.tv_sec < delta.tv_sec){
		ERROR("overflow!" << overflow << endl);
		DEBUG_MSG("initial = " << initial.tv_sec <<"sec, " << initial.tv_usec <<"usec" << endl);
		DEBUG_MSG("delta = " << delta.tv_sec <<"sec, " << delta.tv_usec <<"usec" << endl);
		DEBUG_MSG("result = " << result.tv_sec <<"sec, " << result.tv_usec <<"usec" << endl);
		ABORT_NOW();
		return false;
	}
	return true;
}

bool  Timer::subTimeval(const timeval final, const timeval initial, timeval& result) {
	result.tv_sec = final.tv_sec - initial.tv_sec;
	result.tv_usec = final.tv_usec - initial.tv_usec;

	//Check for underflow
	if((initial.tv_sec+result.tv_sec) != final.tv_sec || (initial.tv_usec+result.tv_usec) != final.tv_usec){
		ERROR("underflow!"<<endl);
		ABORT_NOW();
		return false;
	}
	return true;
}


bool Timer::factorTimeval(timeval& t) {
	if(t.tv_usec >= uS_IN_SEC ){
		__time_t seconds = t.tv_usec / uS_IN_SEC;

		t.tv_usec -= seconds*uS_IN_SEC;

		seconds = t.tv_sec+seconds;

		//Check for overflow of seconds
		if(seconds < t.tv_sec){
			ERROR("overflow!"<<endl);
			ABORT_NOW();
			return false;
		}

		t.tv_sec = seconds;
	}
	else if(t.tv_usec < 0 && t.tv_sec > 0){
		//Need to borrow from seconds

		__time_t seconds = ((-t.tv_usec) / uS_IN_SEC) + 1;
		t.tv_sec -= seconds;
		t.tv_usec += seconds * uS_IN_SEC;

		if(t.tv_sec < 0 || t.tv_usec < 0){
			ERROR("Underflow!"<<endl);
			ABORT_NOW();
			return false;
		}
	}
	return true;
}

int Timer::timevalToMs(timeval t){
	return (uint32_t)((t.tv_sec*1000) + (t.tv_usec/1000));
}
