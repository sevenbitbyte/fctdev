#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <ctime>

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <getopt.h>
#include <stdlib.h>

#include "debug.h"
#include "switched.h"
#include "rawutils.h"
#include "packets.h"
#include "emulabutils.h"

using namespace std;

bool canrun=true;

#define MAX_TIMEOUT 10

void shutdown(int signal);
void print_help(char** argv);

vector<si_socket*> getReadySockets(vector<si_socket*> sockets, int msTimeout);

int main(int argc, char** argv){

	if (getuid() != 0) {
		cerr << "WARNING: Root access expected!\n" << endl;
	}

	list<string> interfaces;
	list<string> excludeInterfaces;
	bool allInterfaces=false;
	bool verbose=false;

	int opt = 0;
	struct option long_options[] = {
		{"interface", 1, 0, 'i'},
		{"all_interfaces", 0,0, 'I'},
		{"exclude", 1, 0, 'x'},
		{"help", 0, 0, '?'},
		{"version", 0, 0, 'v'},
		{"verbose", 0, 0, 'V'},
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "i:x:h?IvV", long_options, NULL)) != -1) {
		switch(opt) {
			case 'i':
				interfaces.push_back(optarg);
				break;
			case 'x':
				excludeInterfaces.push_back(optarg);
				break;
			case 'I':
				allInterfaces=true;
				break;
			case 'v':
				cout<<VERSION_STRING<<endl;
				exit(0);
			case 'V':
				verbose=true;
				break;
			case '?':
			case 'h':
				print_help(argv);
				exit(0);
				break;
		}
	}

	//Setup network interfaces
	vector<si_socket*> sockets;

	if(allInterfaces){
		list<string> allExpIfs = getExperimentIFNames();
		interfaces.insert(interfaces.end(), allExpIfs.begin(), allExpIfs.end());
	}

	//Remove excluded interfaces
	std::list<string>::iterator iter = excludeInterfaces.begin();
	for(; iter!=excludeInterfaces.end(); iter++){
		interfaces.remove(*iter);
	}

	string controlIf = getControlIFName();

	while(!interfaces.empty()){
		if(interfaces.back() == controlIf){
			cout << "WARNING: "<<controlIf<<" is the control interface!"<<endl;
		}

		si_socket* s = new si_socket();

		if(s->open(interfaces.back()) != 0){
			cout<<"Error opeing interface: "<<interfaces.back()<<endl;
			delete s;
			exit(-1);
		}

		if(verbose){
			cout<<"Opened interface: "<<s->getName()<<endl;
		}

		sockets.push_back(s);
		interfaces.pop_back();
	}


	//Setup signal handlers
	signal(SIGINT, shutdown);
	signal(SIGTERM, shutdown);
	signal(SIGABRT, shutdown);

	timeval startTime;
	gettimeofday(&startTime, NULL);

	while(canrun){
		//Check for packets
		vector<si_socket*> readySockets = getReadySockets(sockets, MAX_TIMEOUT);

		if(readySockets.size() > 0){
			while(!readySockets.empty()){
				si_socket* sock = readySockets.back();


				int rxStatus = 0;

				while(true){
					raw_packet pkt;

					rxStatus = sock->recv(pkt);
					if(rxStatus < 0){
						break;
					}

					//timeval currentTime;
					//gettimeofday(&currentTime, NULL);

					uint64_t deltaMs = getSinceMs(startTime);

					cout<<"["<< deltaMs << "ms]";
					cout<<" - "<<sock->getName() << " ";
					cout<<packetToString(&pkt)<<endl;

					if(verbose){
						pkt.printPacket();
						cout<<endl;
					}
				}

				readySockets.pop_back();
			}
		}
	}

	//Cleanup
	while(!sockets.empty()){
		if(verbose){
			cout<<"Closing interface: "<<sockets.back()->getName()<<endl;
		}

		delete sockets.back();
		sockets.pop_back();
	}

	exit(0);
}



vector<si_socket*> getReadySockets(vector<si_socket*> sockets, int msTimeout){
	vector<si_socket*> readySockets;
	fd_set socket_fds;
	int fds_max=0;


	FD_ZERO(&socket_fds);
	for(uint32_t i=0; i<sockets.size(); i++){
		if(sockets.at(i)==NULL){
			continue;
		}

		int sock=sockets.at(i)->sock;
		if(sock >= fds_max){
			fds_max=sock+1;
			//cout<<"WARNING, fds_max="<<fds_max<<endl;
		}
		if(!FD_ISSET(sock, &socket_fds)){
			FD_SET(sock, &socket_fds);
			if(!FD_ISSET(sock, &socket_fds)){
				ERROR("FD_SET failed for interface "<<sockets.at(i)->getName()<<endl);
			}
		}
	}

	timeval timeout;
	timeout.tv_sec=msTimeout/1000;
	timeout.tv_usec=(msTimeout-timeout.tv_sec*1000)*1000;


	Timer elapsed(true);
	int retVal=select(fds_max, &socket_fds, NULL, NULL, &timeout);
	elapsed.stop();

	if(retVal < 0){
		if(errno == EINTR){	//Unexpected signal caused select to return prematurely
			timeout=elapsed.getElapsedReal();
			msTimeout-=TIMEVAL_TO_MS(timeout);
			//cout<<"si_node::getReadyLinks - WARNING, call to select was interrupted with "<<msTimeout<<"ms remaining."<<endl;
			if(msTimeout > 1){
				return getReadySockets(sockets, msTimeout);
			}
		}
		else{
			ERROR("select returned "<<retVal<<endl);
			perror("select");
		}
	}
	else{
		//Create list of sockets ready for reading
		for(uint32_t i=0; i<sockets.size(); i++){
			if(sockets.at(i)==NULL){
				continue;
			}
			if(FD_ISSET(sockets.at(i)->sock, &socket_fds)){
				readySockets.push_back(sockets.at(i));
			}
		}
	}
	return readySockets;
}



void print_help(char** argv) {
	cout<<"Usage: "<<argv[0]<<" [options]"<<endl<<endl;
	cout<<"\t-i [ifname]\t"<<"Add interface to listen on"<<endl;
	cout<<"\t-I\t\t"<<"Adds all interfaces except for lo and eth0"<<endl;
	cout<<"\t-x [ifname]\t"<<"Interface to exclude from communication"<<endl;
	cout<<"\t-h\t\t"<<"Print this help"<<endl;
	cout<<"\t-v\t\t"<<"Version information"<<endl;
	cout<<"\t-V\t\t"<<"Verbose packet dumping"<<endl;
}


void shutdown(int signal){
	canrun=false;
}
