#include "arraystream.h"
#include "switched.h"
#include "debug.h"
#include <errno.h>
#include <vector>
#include <iostream>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>


using namespace std;

void printHelp(char* argv0);

int main(int argc, char **argv) {
	if (getuid() != 0) {
		cerr << "Warning! This program must be run as root! Don't be surprised if bad things happen." << endl;
	}

	if(argc==1){
		printHelp(argv[0]);
		exit(1);
	}

	int opt = 0;
	string ifname = "lo";
	string source = si_address::linkLocal().toString();
	string dest = si_address::linkLocal().toString();
	string data = "Hello World";
	int type=0;
	while ((opt = getopt(argc, argv, "hs:d:ri:t:v")) != -1) {
		switch(opt) {
			case 'h':
				printHelp(argv[0]);
				exit(1);
			case 's':	//Source
				source=optarg;
				break;
			case 'd':	//Destination
				dest=optarg;
				break;
			case 'i':	//Interface
				ifname=optarg;
				break;
			case 't':	//Type
				type=atoi(optarg);
				break;
			case 'v':
				cout<<VERSION_STRING<<endl;
				exit(0);
			default:
				printHelp(argv[0]);
				exit(-1);
		}
	}

	si_socket sock(ifname);
	si_packet pkt;
	pkt.setDestination( dest );
	pkt.setSource( source );
	pkt.type = type;
	pkt.setPayload((uint8_t*)data.c_str(), data.length());
	int response = sock.send(pkt);
	if (response == pkt.byteCount())
		cout << "Sent packet: " << pkt.toString() << endl;
	else
		perror("Error sending packet");
	sock.close();
}

void printHelp(char* argv0){
	cout<<"Usage: "<< argv0 << " <options>"<<endl<<endl;
	cout<<"\t-t <type>\t\t"<<"Packet type number"<<endl;
	cout<<"\t-s <source>\t\t"<<"Source address"<<endl;
	cout<<"\t-d <destination>\t"<<"Destination address"<<endl;
	cout<<"\t-i <interface>\t\t"<<"Interface name, defaults to lo"<<endl;
	cout<<"\t-h\t\t\t"<<"Display this help message"<<endl;
	cout<<"\t-v\t\t\t"<<"Version information"<<endl;
}
