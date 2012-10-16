#include <iostream>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <getopt.h>
#include <string>
#include <map>
#include <arpa/inet.h>
#include <sched.h>
#include <math.h>
#include <net/if.h>

#include "debug.h"
#include "switched.h"

#define _EMULAB false

// Protocol types
#define PROTO_SI_CMP 1
#define PROTO_MMT    2
#define PROTO_SI_DATA 5

enum CMPTypes{CMP_EchoReply=0, CMP_DestUnreachable, CMP_EchoRequest, CMP_CloudAd, CMP_Trace, CMP_ForcedCloudAd};

vector<string> interfaceList;
vector<si_socket*> sockets;
bool verbose=false;
int delayTime=500;	//Inter-packet TX delay

void openSockets();
void addAllInterfaces();
void print_help(char** argv);
void transmitAll(si_packet& packet);

int main(int argc, char** argv){

	if(argc==1){
		print_help(argv);
		exit(1);
	}

	if (getuid() != 0) {
		cerr << "WARNING: Root access expected!\n" << endl;
	}

	int offset=1;
	int downLinks=2000;
	int trunkLinks=2000;
	int upLinks=2000;
	si_address address=si_address::broadCast();
	char local_hostname[50];

	gethostname(local_hostname, 50);
	int hostname_len=fmin(50, strlen(local_hostname)+1);
	if(hostname_len==50){
		local_hostname[49]=0x00;
	}



	int opt = 0;
	struct option long_options[] = {
		{"address", 1, 0, 'a'},
		{"delay", 1, 0, 'D'},
		{"interface", 1, 0, 'i'},
		{"hostname", 1, 0, 'n'},
		{"offset", 1, 0, 'o'},
		{"downlinks", 1, 0, 'd'},
		{"uplinks", 1, 0, 'u'},
		{"trunklinks", 1, 0, 't'},
		{"all_interfaces", 1, 0, 'I'},
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{"verbose", 0, 0, 'V'},
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "a:D:i:n:o:d:u:t:I?hvV", long_options, NULL)) != -1) {
		switch(opt) {
			case 'a':
				address=si_address(optarg);
				break;
			case 'D':
				delayTime=atoi(optarg);
				break;
			case 'i':
				interfaceList.push_back(string(optarg));
				break;
			case 'I':
				addAllInterfaces();
				break;
			case 'n':
				strcpy(local_hostname, optarg);
				break;
			case 'o':
				offset=atoi(optarg);
				break;
			case 'd':
				downLinks=atoi(optarg);
				break;
			case 't':
				trunkLinks=atoi(optarg);
				break;
			case 'u':
				upLinks=atoi(optarg);
				break;
			case 'V':
				verbose=true;
				break;
			case 'v':
				cout<<VERSION_STRING<<endl;
				exit(0);
			case 'h':
			case '?':
				print_help(argv);
				exit(0);
		}
	}

	//Configuration error checking
	if(address==si_address::broadCast() || address==si_address::linkLocal()){
		ERROR("Base address must be specified and my not be broadcast or linklocal!"<<endl<<endl);
		print_help(argv);
		exit(0);
	}

	openSockets();
	if(sockets.size()<1){
		ERROR("At least one valid interface is required!"<<endl<<endl);
		print_help(argv);
		exit(0);
	}

	if(address.bc==0 || address.bc==1){
		upLinks=0;
	}

	si_packet packet(address, si_address::linkLocal(), (uint8_t*)local_hostname, hostname_len);
	packet.type=PROTO_SI_CMP;

	packet.expandPayload(3);
	uint8_t* payload=packet.getPayloadPtr();
	payload[0]=(CMP_ForcedCloudAd<<4) | 0x00;
	payload[1]=0;
	payload[2]=0;

	if(verbose){
		INFO("Hostname="<<local_hostname<<endl);
		INFO("Base Address="<<address.toString()<<endl);
	}

	//Downlink announcements
	address.chunks.push_back(offset);
	address.bc++;
	packet.setSource(address);
	transmitAll(packet);
	usleep(delayTime);
	for(int i=1; i<downLinks; i++){
		int val=address.chunks.back()+1;
		address.chunks.pop_back();
		address.chunks.push_back(val);
		packet.setSource(address);
		transmitAll(packet);
		usleep(delayTime);
	}

	//Trunk announcements
	address.chunks.pop_back();
	address.bc--;
	for(int i=0; i<trunkLinks; i++){
		int val=address.chunks.back()+1;
		address.chunks.pop_back();
		address.chunks.push_back(val);
		packet.setSource(address);
		transmitAll(packet);
		usleep(delayTime);
	}

	//Uplink announcements
	address.chunks.pop_back();
	address.bc--;
	for(int i=0; i<upLinks; i++){
		int val=address.chunks.front()+1;
		address.chunks.pop_front();
		address.chunks.push_front(val);
		packet.setSource(address);
		transmitAll(packet);
		usleep(delayTime);
	}


	INFO("Announced UpLinks="<<upLinks<<" DownLinks="<<downLinks<<" TrunkLinks="<<trunkLinks<<endl<<endl);


	while(!sockets.empty()){
		delete sockets.back();
		sockets.pop_back();
	}
}



void addAllInterfaces(){
	struct if_nameindex *inames=if_nameindex();
	int i=0;
	while(1){
		if(inames[i].if_index==0 && inames[i].if_name==NULL){
			break;
		}
#if _EMULAB
		if(strcmp(inames[i].if_name, "lo")!=0 && strcmp(inames[i].if_name, "eth2")!=0){
#else
		if(strcmp(inames[i].if_name, "lo")!=0 && strcmp(inames[i].if_name, "eth0")!=0){
#endif
			DEBUG_MSG("Index:"<<inames[i].if_index<<" Name:"<<inames[i].if_name<<endl);
			interfaceList.push_back(inames[i].if_name);
		}
		i++;
	}
	if_freenameindex(inames);;
}

void openSockets(){
	for(uint32_t i=0; i<interfaceList.size(); i++){
		string name=interfaceList.at(i);
		si_socket* s = new si_socket(name);
		if (!s->isOpen()) {
			ERROR("Failed to open socket on interface "<<name<<"!"<<endl);
			delete s;
			continue;
		}
		sockets.push_back(s);
	}
}

void transmitAll(si_packet& packet){
	if(verbose){
		INFO("Sending as "<<packet.getSource().toString()<<endl);
	}

	for(uint32_t i=0; i<sockets.size(); i++){
		si_socket* sock=sockets.at(i);
		sock->send(packet);
	}
}

void print_help(char** argv) {
	cout<<"Usage: "<<argv[0]<<" [options]"<<endl<<endl;

	cout<<"\t-i [interface]\t"<<"Interface to broadcast on"<<endl;
	cout<<"\t-I \t\t"<<"Broadcast on all interfaces"<<endl;
	cout<<"\t-a [address]\t"<<"Base source address *REQUIRED*"<<endl;
	cout<<"\t-D [delay]\t"<<"Time to wait between sending packets, in microseconds. Default is 500us"<<endl;
	cout<<"\t-n [hostname]\t"<<"Hostname used in announcements"<<endl;
	cout<<"\t-o [offset]\t"<<"Address offset"<<endl;
	cout<<"\t-d [downlinks]\t"<<"Number of downlinks to announce, default is 2000"<<endl;
	cout<<"\t-t [trunklinks]\t"<<"Number of trunklinks to announce, default is 2000"<<endl;
	cout<<"\t-u [uplinks]\t"<<"Number of uplinks to announce, default is 2000"<<endl;
	cout<<"\t-V\t\t"<<"Verbose output"<<endl;
	cout<<"\t-v\t\t"<<"Version information"<<endl;
	cout<<endl;

}
