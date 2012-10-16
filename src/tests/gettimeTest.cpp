#define BUILD_DEBUG

#include "../debug.h"
#include "../timer.h"
#include <sys/time.h>

using namespace std;

int main(int argc, char** argv){


	timeval current;

	cout<<"Testing"<<endl;

	Timer loopTimer(true);

	for(int i=0; i<5000; i++){
		gettimeofday(&current, NULL);
	}

	loopTimer.stop();
	loopTimer.print();
}
