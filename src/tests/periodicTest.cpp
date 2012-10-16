#include "../debug.h"
#include "../timer.h"
#include "../periodic.h"
#include <time.h>
#include <sys/time.h>

#include<map>
#include<string>

using namespace std;

class PeriodicTester : public periodic_t{
	public:
		PeriodicTester(){
		}

		~PeriodicTester(){
			delete periodic_service::instance();
		}

		void setupAlarm(bool callBackA, timeval period, timeval delay, string name, int maxTriggers=5){
			int id = trigger_count.size();

			periodic* p = NULL;

			if(callBackA){
				p = new periodic(this, (periodic_t_func)&PeriodicTester::alarmCallbackA, (void*)id, (periodic_t_func)&PeriodicTester::destoryCallback);
			}
			else{
				p = new periodic(this, (periodic_t_func)&PeriodicTester::alarmCallbackB, (void*)id, (periodic_t_func)&PeriodicTester::destoryCallback);
			}

			trigger_count[id]=0;
			max_triggers[id]=maxTriggers;

			p->install(period, delay, name);

			timeval now;
			gettimeofday(&now, NULL);
			DEBUG_MSG(timevalToString(now)<<" TriggerID="<<id<<" trigger_count="<<trigger_count[id]<<" installed"<<endl);
		}

		void destoryCallback(void* arg, periodic* p){
			timeval now;
			gettimeofday(&now, NULL);
			int id = *((int*) &arg);
			DEBUG_MSG(timevalToString(now)<<" TriggerID="<<id<<" trigger_count="<<trigger_count[id]<<" p.name="<<p->name<<endl);
		}

		void alarmCallbackA(void* arg, periodic* p){
			int id = *((int*) &arg);
			trigger_count[id]++;
			timeval now;
			gettimeofday(&now, NULL);
			DEBUG_MSG(timevalToString(now)<<" TriggerID="<<id<<" trigger_count="<<trigger_count[id]<<" p.name="<<p->name<<endl);


			if(id == 0 && trigger_count[id] == 16){
				setupAlarm(false, (timeval){0,100000}, (timeval){1,0}, "second", 50);
			}

			if(trigger_count[id] >= max_triggers[id]){
				p->remove();
			}
		}

		void alarmCallbackB(void* arg, periodic* p){
			int id = *((int*) &arg);
			trigger_count[id]++;
			timeval now;
			gettimeofday(&now, NULL);
			DEBUG_MSG(timevalToString(now)<<" TriggerID="<<id<<" trigger_count="<<trigger_count[id]<<" p.name="<<p->name<<endl);

			if(trigger_count[id] >= max_triggers[id]){
				p->remove();
			}
		}

		void updatePeriodic(string name, timeval period, timeval delay){

		}

		map<int, int> trigger_count;
		map<int, int> max_triggers;
};


int main(int argc, char** argv){

	PeriodicTester* tester = new PeriodicTester;
	periodic_service* service = periodic_service::instance();
	Timer timer(true);

	tester->setupAlarm(true, (timeval){4,0}, (timeval){5,0}, "first", 100);
	//tester->setupAlarm(true, (timeval){0,100000}, (timeval){1,0}, "second");

	cout<<endl<<"----------Testing----------"<<endl;

	while(TIMEVAL_TO_MS(timer.getElapsedReal()) < 30000){
		sleep(2);
	}

	cout<<endl<<"----------Pausing----------"<<endl;
	service->pause();

	timer.reset();
	timer.start();
	while(TIMEVAL_TO_MS(timer.getElapsedReal()) < 30000){
		sleep(2);
	}

	cout<<endl<<"----------Resuming----------"<<endl;
	service->resume();

	timer.reset();
	timer.start();
	while(TIMEVAL_TO_MS(timer.getElapsedReal()) < 30000){
		sleep(2);
	}

	cout<<endl<<"----------Cleanup Up----------"<<endl;
	delete tester;
	cout<<endl<<"----------Exiting----------"<<endl;
}
