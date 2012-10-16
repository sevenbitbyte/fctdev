#include "periodic.h"

#include <iostream>

class periodic_tester{

	periodic_tester(){
		callCount = 0;
		
		timeval fiveSec;
		fiveSec.sec = 5;
		fiveSec.usec = 0;
		
		timeval zeroSec;
		zeroSec.sec = 5;
		zeroSec.usec = 0;
		
		periodic* p = new periodic(this, (periodic_t_func)&periodic_tester::callbackFunc, &this->callCount);
		p->install(fiveSec, zeroSec, "periodic_tester");
	}
	
	void callbackFunc(void* arg, periodic* p){
		std::cout << "Callback called" << std::endl;
		
		if(arg != NULL){
			int* count = arg;
			
			*count++;
			if(*count > 2){
				std::cout << "Deleting periodic" << endl;
				p->remove();
			}
		}
	}
	
	void wait(){
		while(callCount < 3);
	}
	
	private:
		int callCount;
};

int main(int argc, char** argv){

	periodic_tester test;
	
	test.wait();

}

