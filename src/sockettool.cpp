#include "debug.h"
#include "switched.h"
#include "timer.h"

using namespace std;

int main(int argc, char** argv){

	if(argc != 3){
		cout <<"Usage: " << argv[0] << " [interface] [state]" <<endl;
		return -1;
	}

	si_socket sock;
	string ifName = argv[1];
	string ifState = argv[2];

	if(sock.open(ifName, true) == 0){

		Timer t(true);
		if(ifState == "up"){
			sock.setState(true);
		}
		else{
			sock.setState(false);
		}

		t.stop();

		t.print();

		sock.close();
	}
}
