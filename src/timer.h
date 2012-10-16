#ifndef TIMER_H
#define TIMER_H

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdint.h>
#include <string>

#define uS_IN_SEC 1000000

/** Difference between real, user and system time

  *The tms_utime structure member is the CPU time charged for the execution of user instructions of the calling process.

  *The tms_stime structure member is the CPU time charged for execution by the system on behalf of the calling process.

  *The tms_cutime structure member is the sum of the tms_utime and tms_cutime times of the child processes.

  *The tms_cstime structure member is the sum of the tms_stime and tms_cstime times of the child processes.

*/




using namespace std;

#define TIMEVAL_TO_SEC(t) ((uint32_t)t.tv_sec + ((uint32_t)t.tv_usec/uS_IN_SEC))
#define TIMEVAL_TO_SEC_F(t) (((float)t.tv_sec) + ((float)t.tv_usec/(float)uS_IN_SEC))
#define TIMEVAL_TO_MS(t) (uint32_t)((t.tv_sec*1000) + (t.tv_usec/1000))
#define TIMEVAL_TO_MS_S(t) (int32_t)((t.tv_sec*1000) + (t.tv_usec/1000))
#define TIMEVAL_TO_MS_F(t) (((float)t.tv_sec*1000) + ((float)t.tv_usec/1000))
#define TIMEVAL_TO_uS(x) ((uint32_t)x.tv_sec * 1000 * 1000 + (uint32_t)x.tv_usec)
#define TIMEVAL_TO_uS_F(x) ((float)x.tv_sec * 1000 * 1000 + (float)x.tv_usec)
#define TIMEVAL_TO_uS_64(x) ((uint64_t)x.tv_sec * 1000 * 1000 + (uint64_t)x.tv_usec)

//! Generates a string representation of a timeval
string timevalToString(timeval& t);
//void printTimeval(timeval* t);

//! Returns (now - tv_first) in microseconds
int getNow(struct timeval &tv);
uint64_t getSince(struct timeval &tv_prior);
uint64_t getSinceMs(struct timeval &tv_prior);

/**
  *		Records the elapsed time along with the real/user/system times for
  *		the measured period. Additionally the number of voluntary and
  *		involuntary context switches are also counted. Pausing and resuming
  *		of timers is supported.
  *
  *		@short	A multishot execution timer
  */
class Timer{
	public:
		Timer(bool auto_start=false);

		bool isGood();

		bool isRunning();

		//! Starts the timer, does nothing if the timer is already running
		void start();

		//! Stops the timer and updates the elapsed time buffers
		void stop();

		//! Returns the timer to a running state from the stopped state
		void resume();

		/**
		  *	Clears all member variables and stops timer
		  */
		void reset();

		/**
		  *	Clears all member variables then starts the timer again
		  */
		void restart();

		void print();

		void printLine();

		void toXml(ostream& stream);

		//! Generates a string representation of the timer
		string toString();

		//! Returns the elapsed user time as a timeval
		timeval getElapsedUser();

		//! Returns the elapsed sys time as a timeval
		timeval getElapsedSys();

		//! Returns the elapsed real time as a timeval
		timeval getElapsedReal();

		/**	Adds the time delta to the initial time storing the sum
		  *	in the provided result timeval
		  *	@param	initial	Starting time
		  *	@param	delta	Time delta
		  *	@param	result	Stores resulting sum
		  *	@return Returns false if adding the two timevals will
		  *		result in an overflow causing incorrect time
		  *		representation.
		  */
		static bool addTimeval(timeval initial, timeval delta, timeval& result);

		/**	Subtracts initial time from final time storing the difference
		  *	in the result timeval.
		  *	@param	final	Ending time
		  *	@param	initial	Starting time
		  *	@param	result	Stores resulting time difference here
		  *	@return Returns false if subtraction causes underflow
		  */
		static bool subTimeval(const timeval final, const timeval initial, timeval& result);

		/**	Factors out whole seconds from the tv_usec field
		  *	adding any whole seconds to the tv_sec field instead.
		  *	If factoring will cause an overflow t is not updated
		  *	and false is returned.
		  *	@param	t	Timeval to factor
		  *	@return Returns false if factoring will cause an error,
		  *		returns true other wise.
		  */
		static bool factorTimeval(timeval& t);

		/**
		  *	Returns the value of the timeval in milliseconds
		  *
		  *	@param	t	Timeval to convert to ms
		  *	@return	Value of timeval in ms
		  */
		static int timevalToMs(timeval t);

	protected:
		/**
		  *	Updates the internal elapsed time buffers
		  *
		  *	@return Returns true if no underflows or overflows occured
		  *			during the buffer update.
		  */
		bool updateElapsed();

	private:

		bool running;	//Timer status
		bool overflow;	//Indicates that some part of the time has overflowed

		//Timestamps of user/system/real times when timer started
		struct timeval real_start;
		struct timeval user_start;
		struct timeval sys_start;
		int vcsw_start;     //Voluntary context switches
		int ivcsw_start;    //Involuntary context switches


		//Elapsed user/system/real times when timer stopped or paused
		struct timeval real_elapsed;
		struct timeval user_elapsed;
		struct timeval sys_elapsed;
		int vcsw_elapsed;    //Voluntary context switches
		int ivcsw_elapsed;   //InVoluntary context switches
};

#endif //TIMER_H
