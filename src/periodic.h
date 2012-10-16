#ifndef _PERIODIC_H_
#define _PERIODIC_H_

#include "timer.h"
#include "mutex.h"
#include <pthread.h>
#include <string>
#include <map>

using namespace std;

#define TIMER_WINDOW_MS 5
#define BACKOFF_WINDOW_MS 1
#define WATCHDOG_PERIOD_MS 10000

class periodic_t { };

class periodic;

typedef void (periodic_t::*periodic_t_func)(void* arg, periodic* p);


class periodic {

	/**	This class can be used to call a given function periodically and
	  * also can issue a callback when it is deleted by the periodic servicer.
	  */

	public:
		/**
		  *	Constructs a periodic with the provided callbacks. The periodic
		  *	timing does not begin until the install function is called.
		  *
		  *	@param	parent		Parent object where function resides
		  *	@param	exePtr		Function to call when periodic event is triggered
		  *	@param	arg			Argument to pass callback functions
		  *	@param	destroyFunc	Function to call when this periodic is deleted
		  */
		periodic(periodic_t* parent, periodic_t_func exePtr, void* arg=NULL, periodic_t_func destroyFunc=NULL);

		/**
		  *	Sets the value of the strict execution flag.
		  *	See discussion in documentation of periodic::execute() for more
		  *	details.
		  *
		  *	@param	enable	Enable or disable strict execution, default is true.
		  */
		inline void setStrictExec(bool enable=true);

		/**
		  *	Installs this periodic, once this function has been entered the
		  * periodic callback routines may be triggered at any point. This
		  *	function is re-entrant therefore it is safe to call from within
		  *	a periodic exection or destroy callback.
		  *
		  *	@param	interval	Interval the exe callback will be triggered
		  *	@param	delay		Delay period before the first interval begins
		  *	@param	debug		String that gets printed in debug messages
		  *						associated with this periodic
		  */
		void install(timeval interval, timeval delay, string debug=string());

		/**
		  *	Flags the periodic for defered deletion. After calling this function
		  * it is not safe to reference this periodic any more. Depending upon
		  *	the number of remaining periodics this instance will be free'd
		  *	during the next periodic service routine. If there are no more
		  *	active periodics this instance may not be free'd until the program
		  *	exits.
		  */
		void remove();


		//! Compare the intervals of two periodics
		bool operator<(const periodic &other) const;

		//! Test for equality between the interval of two periodics
		bool operator==(const periodic &other) const;

		//! Test for inequality between the interval of two periodics
		bool operator!=(const periodic &other) const;

		string name;
		timeval interval;
		struct timeval next;
		bool strictExec;
		void* argument;
		periodic_t *obj;
		//!Execution callback function pointer
		periodic_t_func exe;
		//!Destruction callback function pointer
		periodic_t_func destroy;

	protected:

		bool removed;

		/**
		  *	Calls the exe callback and updates the time that it should run next.
		  *	If strict execution is enabled the callback will be called the
		  * correct number of times for a given time period even if this means
		  *	that it must be called multiple times to make up for one or more
		  *	missed calls.
		  *	If strict execution is not enabled the callback will be scheduled to
		  *	be called at a time in the future equal to the current time plus the
		  *	interval period.
		  */
		bool execute();

		/**
		  *	BE WARE: Instances of this class should always have the install
		  *	function called at least once and should never be deleted anywhere
		  *	other than by the periodic servicer.
		  */
		~periodic();

		friend class periodic_service;
		friend void periodic_service_alarm_handler(int i);
};


void installTimer(int timeoutMs=200, int intervalMs=0);

void installTimer(timeval timeout, timeval interval=(timeval){0, 0});

void periodic_service_alarm_handler(int i=0);

class periodic_service{
	public:
		~periodic_service();

		static periodic_service* instance();

		/**
		  *	Installs a periodic instance and reschedules periodic servicing
		  *	if the scheduling lock could be acquired. This call is re-entrant
		  *	and can be called from within a periodic exection handler.
		  */
		void install(periodic* p);
		void remove(periodic* p);

		bool isPaused() const;
		bool isActive() const;

		/*
		 *	BE WARE: The following functions are NOT reentrant! Therefore they
		 *	should never be called from any periodic callback functions, it will
		 *	cause a deadlock!
		 */


		bool isValid(periodic* p);

		//! Temporarily suspend periodic servicing
		void pause();

		/**
		  *	 Resume servicing periodic timers
		  *
		  *	@param	recompute	Force servicing schedule to be recomputed
		  */
		void resume(bool recompute=true);

		//! Reschedule alarms used for servicing periodics
		inline void reschedule();

		void printState();

	protected:
		void init();
		void teardown();

		struct periodic_cmp {
			bool operator() (const periodic* lhs, const periodic* rhs) const;
		};

		bool active;
		bool running;
		map<periodic*, periodic*, periodic_cmp> periodic_list;
		vector<periodic*> deferredInstall;
		Mutex _listMutex;

	private:
		/**
		  *	This class is a singleton so do not try to construct more than one
		  *	instance, instead use the instance() function.
		  */
		periodic_service();

		static periodic_service* _instance;

		friend void periodic_service_alarm_handler(int i);

		double avgError;
		double varError;
};





#endif
