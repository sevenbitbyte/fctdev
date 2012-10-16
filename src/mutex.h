#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

class Mutex
{
	public:
		Mutex();
		~Mutex();
		void reset();
		bool tryLock();
		void enter();
		void leave();

	private:
		pthread_mutex_t _mutex;
};

class MutexLock{
	public:
		MutexLock(Mutex* m);
		MutexLock(pthread_mutex_t* ptMutex);
		~MutexLock();
	private:
		Mutex* _mutex;
		pthread_mutex_t* _pt_mutex;
};

#endif // MUTEX_H
