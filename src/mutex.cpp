#include "mutex.h"
#include <pthread.h>

Mutex::Mutex()
{
	pthread_mutex_init(&_mutex, NULL);
}


Mutex::~Mutex(){
	pthread_mutex_destroy(&_mutex);
}

void Mutex::enter(){
	pthread_mutex_lock(&_mutex);
}

void Mutex::leave(){
	pthread_mutex_unlock(&_mutex);
}

bool Mutex::tryLock(){
	return pthread_mutex_trylock(&_mutex)==0;
}

void Mutex::reset(){
	pthread_mutex_init(&_mutex, NULL);
}

MutexLock::MutexLock(Mutex* m):_mutex(m),_pt_mutex(NULL){
	_mutex->enter();
}

MutexLock::MutexLock(pthread_mutex_t* ptMutex):_mutex(NULL),_pt_mutex(ptMutex){
	pthread_mutex_lock(_pt_mutex);
}

MutexLock::~MutexLock(){
	if(_pt_mutex == NULL){
		_mutex->leave();
	}
	else{
		pthread_mutex_unlock(_pt_mutex);
	}
}
