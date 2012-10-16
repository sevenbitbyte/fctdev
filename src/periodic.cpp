#include "debug.h"
#include "timer.h"
#include "periodic.h"
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

#ifdef BUILD_DEBUG
	#define PERIODIC_DEBUG false
#else
	#define PERIODIC_DEBUG false
#endif


periodic_service* periodic_service::_instance = NULL;



periodic::periodic(periodic_t* parent, periodic_t_func exePtr, void* arg, periodic_t_func destroyFunc){
	argument = arg;
	obj = parent;
	exe = exePtr;
	destroy = destroyFunc;
	removed = false;
	strictExec = true;
}

periodic::~periodic(){
	#if PERIODIC_DEBUG
	DEBUG_MSG(name << " next=" << timevalToString(next) << endl);
	#endif
	if(removed && destroy!=NULL){
		#if PERIODIC_DEBUG
		DEBUG_MSG("Calling cleanup funcPtr" << endl);
		#endif
		(*obj.*destroy)(argument, this);
	}
	obj = NULL;
}

void periodic::setStrictExec(bool enable){
	strictExec = enable;
}

void periodic::install(timeval interval, timeval delay, string debug){
	this->interval = interval;
	this->name = debug;

	gettimeofday(&next, NULL);

	Timer::addTimeval(next, delay, next);
	Timer::addTimeval(next, interval, next);

	periodic_service::instance()->install(this);
}


void periodic::remove(){
	DEBUG_MSG(name<<endl);
	removed = true;
}

bool periodic::execute(){
	//DEBUG_MSG(name<<endl);
	timeval now;
	timeval diff;
	timerclear(&now);
	timerclear(&diff);
	gettimeofday(&now, NULL);
	bool runFunc = false;

	timersub(&now, &next, &diff);

	runFunc = (TIMEVAL_TO_MS_S(diff) > -TIMER_WINDOW_MS);

	//If timer expired run the function and update timer
	if (runFunc && !removed) {

		#if PERIODIC_DEBUG
		//Measure execution time
		Timer t(true);
		#endif

		(*obj.*exe)(argument, this);

		#if PERIODIC_DEBUG
		t.stop();

		//DEBUG_MSG("["<<name<<"] error = "<<timevalToString(diff)<< " execTime=" <<t.toString()<< endl);
		DEBUG_MSG("["<<name<<"] error = "<<diff.tv_sec<<"s,"<<diff.tv_usec<< "us execTime=" <<t.toString()<< endl);
		#endif

		if(strictExec){
			Timer::addTimeval(next, interval, next);
		}
		else{
			Timer::addTimeval(now, interval, next);
		}
	}

	return runFunc;
}

//! Compare the intervals of two periodics
bool periodic::operator<(const periodic &other) const {
	return timercmp(&next, &other.next, <);
}


//! Test for equality between the interval of two periodics
bool periodic::operator==(const periodic &other) const {
	return (next.tv_sec == other.next.tv_sec) && (next.tv_usec == other.next.tv_usec);
}

//! Test for inequality between the interval of two periodics
bool periodic::operator!=(const periodic &other) const {
	return (next.tv_sec != other.next.tv_sec) || (next.tv_usec != other.next.tv_usec);
}






periodic_service::periodic_service(){
	init();
}

periodic_service::~periodic_service(){
	#if PERIODIC_DEBUG
	DEBUG_MSG("Have "<<periodic_list.size()<<" periodics"<<endl);
	#endif
	teardown();


	_listMutex.enter();
	while(!periodic_list.empty()){
		periodic* p = periodic_list.begin()->first;

		//#if PERIODIC_DEBUG
		cout.unsetf(ios::dec);
		cout.setf(ios::hex);
		DEBUG_MSG("Deleting periodic located at 0x" << (uint64_t)p << endl);
		DEBUG_MSG("Periodic name=" << p->name << endl);
		cout.unsetf(ios::hex);
		cout.setf(ios::dec);
		//#endif

		delete p;
		periodic_list.erase(periodic_list.begin());
	}
	#if PERIODIC_DEBUG
	DEBUG_MSG("Complete"<<endl);
	#endif
}

void periodic_service::teardown(){
	#if PERIODIC_DEBUG
	DEBUG_MSG(endl);
	#endif

	_listMutex.enter();

	struct sigaction alarmAction;
	alarmAction.sa_handler = SIG_DFL;
	sigemptyset(&alarmAction.sa_mask);
	alarmAction.sa_flags = 0;
	sigaction(SIGALRM, &alarmAction, NULL);
	running = false;

	installTimer(0, 0);

	_listMutex.leave();
	#if PERIODIC_DEBUG
	DEBUG_MSG("Complete"<<endl);
	#endif
}

periodic_service* periodic_service::instance(){

	if(_instance == NULL){
		#if PERIODIC_DEBUG
		DEBUG_MSG("Installing"<<endl);
		#endif
		_instance = new periodic_service();
	}

	return _instance;
}

void periodic_service::init(){
	#if PERIODIC_DEBUG
	DEBUG_MSG(endl);
	#endif

	_listMutex.reset();

	struct sigaction alarmAction;
	alarmAction.sa_handler = *periodic_service_alarm_handler;
	sigemptyset(&alarmAction.sa_mask);
	alarmAction.sa_flags = 0;
	sigaction(SIGALRM, &alarmAction, NULL);

	running=true;
	active=false;
}

void installTimer(int timeoutMs, int intervalMs){
	itimerval timerVal;

	timerVal.it_value.tv_sec=timeoutMs/1000;
	timerVal.it_value.tv_usec=(timeoutMs-timerVal.it_value.tv_sec*1000)*1000;

	timerVal.it_interval.tv_sec=intervalMs/1000;
	timerVal.it_interval.tv_usec=(intervalMs-timerVal.it_interval.tv_sec*1000)*1000;

	setitimer(ITIMER_REAL, &timerVal, NULL);
}

void installTimer(timeval timeout, timeval interval){
	itimerval timerVal;

	timerVal.it_value = timeout;
	timerVal.it_interval = interval;

	setitimer(ITIMER_REAL, &timerVal, NULL);
}

void periodic_service::pause(){
	_listMutex.enter();
	#if PERIODIC_DEBUG
	DEBUG_MSG(endl);
	#endif

	running = false;
	installTimer(0, 0);
	_listMutex.leave();
}


void periodic_service::resume(bool recompute){
	_listMutex.enter();
	#if PERIODIC_DEBUG
	DEBUG_MSG(endl);
	#endif

	if(recompute){
		reschedule();
	}
	running=true;
	_listMutex.leave();
}

void periodic_service::install(periodic* p){
	if(_listMutex.tryLock()){
		//We're safe
		#if PERIODIC_DEBUG
		DEBUG_MSG(endl);
		#endif

		map<periodic*, periodic*, periodic_cmp>::iterator iter = periodic_list.begin();

		for(; iter != periodic_list.end(); iter++){
			if(iter->first == p){
				#if PERIODIC_DEBUG
				DEBUG_MSG("Removing old entry from schedule" << endl);
				#endif

				periodic_list.erase(iter);
				break;
			}
		}

		periodic_list.insert(make_pair(p,p));

		if(running){
			reschedule();
		}
		_listMutex.leave();
	}
	else{
		//Do a deferred install if we cannot get the lock
		#if PERIODIC_DEBUG
		DEBUG_MSG("Deferred install of periodic[" << p->name << "]" << endl);
		#endif
		//TODO: Potential race condition
		deferredInstall.push_back(p);
	}
}

void periodic_service::remove(periodic* p){
	p->remove();

	//If we fail to acquire the lock it usually means that we would end up
	//causing a dead lock if we block. The above line ensures that the periodic
	//will be deleted eventually and that it will not issue anymore execute
	//callbacks.

	if(_listMutex.tryLock()){	//Safe to remove
		map<periodic*, periodic*, periodic_cmp>::iterator iter = periodic_list.begin();

		for(; iter != periodic_list.end(); iter++){
			if(iter->first == p){
				periodic_list.erase(iter);
				delete iter->first;

				if(running){
					reschedule();
				}
				break;
			}
		}

		_listMutex.leave();
	}
}

bool periodic_service::isPaused() const {
	return !running;
}

bool periodic_service::isActive() const {
	return active;
}

bool periodic_service::isValid(periodic* p){
	_listMutex.enter();

	map<periodic*, periodic*, periodic_cmp>::iterator iter = periodic_list.find(p);

	if(iter != periodic_list.end()){
		return true;
	}
	return false;

	_listMutex.leave();
}


void periodic_service::reschedule(){
	timeval now;
	gettimeofday(&now, NULL);
	#if PERIODIC_DEBUG
	DEBUG_MSG("Start "<<timevalToString(now)<<endl);
	#endif

	//Manage deferred installations
	vector<periodic*>::iterator viter;
	for(viter = deferredInstall.begin(); viter != deferredInstall.end(); viter++){
		map<periodic*, periodic*, periodic_cmp>::iterator iter2 = periodic_list.find(*viter);

		if(iter2 != periodic_list.end()){
			periodic_list.erase(iter2);
		}
		else{
			#if PERIODIC_DEBUG
			WARNING("Failed to find old periodic!"<<endl);
			#endif
		}

		periodic_list.insert(make_pair(*viter, *viter));
	}
	deferredInstall.clear();

	if(periodic_list.size() > 0){
		map<periodic*, periodic*, periodic_cmp>::iterator iter = periodic_list.begin();

		periodic* nextPeriodic = periodic_list.begin()->first;
		timeval zero = (timeval){0, TIMER_WINDOW_MS*1000};
		timeval now;
		timeval diff;
		timerclear(&now);
		timerclear(&diff);
		gettimeofday(&now, NULL);


		timersub(&iter->first->next, &now, &diff);

		//Walk through sorted list of periodics
		//If there are periodics that should have been serviced in
		//the past, then service them now. Loop terminates when there
		//are no more periodics that should be serviced within the
		//TIMER_WINDOW.

		active=true;
		while(timercmp(&diff, &zero, <)){
			periodic* p = periodic_list.begin()->first;
			nextPeriodic = p;

			#if PERIODIC_DEBUG
			DEBUG_MSG("erase on "<<p->name<<endl);
			#endif

			periodic_list.erase(periodic_list.begin());

			if(p->removed){
				#if PERIODIC_DEBUG
				DEBUG_MSG("calling delete on "<<p->name<<endl);
				#endif

				delete p;
				if(periodic_list.empty()){
					installTimer(WATCHDOG_PERIOD_MS, WATCHDOG_PERIOD_MS);
					#if PERIODIC_DEBUG
					DEBUG_MSG("RETURN - Empty list"<<endl);
					#endif
					active=false;
					return;
				}
				continue;
			}

			#if PERIODIC_DEBUG
			DEBUG_MSG("execute on "<<p->name<<endl);
			#endif
			p->execute();

			#if PERIODIC_DEBUG
			DEBUG_MSG("insert "<<p->name<<endl);
			#endif
			periodic_list.insert(make_pair(p,p));

			iter = periodic_list.begin();
			timersub(&iter->first->next, &now, &diff);
			//DEBUG_MSG("Start "<<timevalToString(now)<<endl);
		}
		active=false;

		#if PERIODIC_DEBUG
		DEBUG_MSG("installTimer"<<endl);
		#endif

		installTimer(diff);

		#if PERIODIC_DEBUG
		if(nextPeriodic != NULL){
			DEBUG_MSG("Set for "<<timevalToString(diff)<<" calling: "<<nextPeriodic->name<<endl);
			printState();
		}
		else{
			DEBUG_MSG("Set for "<<timevalToString(diff)<<endl);
			printState();
		}
		#endif
	}
	else{
		installTimer(WATCHDOG_PERIOD_MS, WATCHDOG_PERIOD_MS);

		#if PERIODIC_DEBUG
		DEBUG_MSG("Set for watchdog period "<<WATCHDOG_PERIOD_MS<<" ms"<<endl);
		#endif
	}
}

void periodic_service::printState(){
	timeval now;
	timerclear(&now);
	gettimeofday(&now, NULL);

	DEBUG_MSG("now = " << timevalToString(now)<<endl);
	DEBUG_MSG("running = " << running << endl);

	DEBUG_MSG("periodic_list count = " << periodic_list.size() << endl);
	map<periodic*, periodic*, periodic_cmp>::iterator iter = periodic_list.begin();

	int index = 0;
	for(; iter != periodic_list.end(); iter++, index++){
		periodic* p = iter->first;
		cout.unsetf(ios::dec);
		cout.setf(ios::hex);
		DEBUG_MSG("\tIndex="<<index << "\t next: " << timevalToString(p->next) << " name: " << p->name << " \tlocation: 0x" << (uint64_t)p << endl);
		cout.unsetf(ios::hex);
		cout.setf(ios::dec);
	}

	DEBUG_MSG("deferredInstall count = " << deferredInstall.size() << endl);
	vector<periodic*>::iterator pIter = deferredInstall.begin();

	index = 0;
	for(; pIter!=deferredInstall.end(); pIter++, index++){
		periodic* p = *pIter;
		DEBUG_MSG("\tIndex="<<index << "\t next: " << timevalToString(p->next) << " name: " << p->name << endl);
	}
}

void periodic_service_alarm_handler(int i){
	periodic_service* service = periodic_service::instance();

	if(!service->_listMutex.tryLock()){

		//Gaurd against zombie alarms
		if(service->running == false){
			WARNING("Timer zombie killed!"<<endl);
			installTimer(0 ,0);
			service->_listMutex.leave();
			return;
		}

		#if PERIODIC_DEBUG
		DEBUG_MSG("Could not lock mutex, backing off for " << BACKOFF_WINDOW_MS << " ms"<<endl);
		#endif

		installTimer(BACKOFF_WINDOW_MS);
		return;
	}

	#if PERIODIC_DEBUG
	DEBUG_MSG("Have "<< service->periodic_list.size() << " periodics to service"<<endl);
	#endif

	//Gaurd against zombie alarms
	if(service->running == false){
		WARNING("Timer zombie killed!"<<endl);
		installTimer(0 ,0);
		//TODO: Unlock mutex
		return;
	}

	//TODO: This is duplicate code, service->reschedule should take care of this
	if(service->periodic_list.size() > 0){
		service->active=true;
		while(service->periodic_list.begin()->first->execute()){
			periodic* p = service->periodic_list.begin()->first;

			service->periodic_list.erase(service->periodic_list.begin());

			if(p->removed){
				if(p->obj != NULL){
					delete p;
				}

				if(service->periodic_list.empty()){
					#if PERIODIC_DEBUG
					DEBUG_MSG("BREAK - Empty list"<<endl);
					#endif
					break;
				}
				continue;
			}

			service->periodic_list.insert(make_pair(p,p));
		}
		service->active=false;
	}

	service->reschedule();
	service->_listMutex.leave();
}

bool periodic_service::periodic_cmp::operator () (const periodic* lhs, const periodic* rhs) const {
	return *lhs < *rhs;
}
