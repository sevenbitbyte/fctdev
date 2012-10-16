#include <iostream>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <getopt.h>
#include <list>
#include <string>
#include <algorithm>


#include "debug.h"
#include "switched.h"
#include "si_node.h"
#include "emulabutils.h"

using namespace std;

bool canrun=true;
bool multiThreaded = true;
si_node* myNode=NULL;
timeval startTime;


void print_help(char** argv);
void shutdown(int signal);

/** CALLBACKS **/
void consumeCallback(raw_packet* p){
	uint64_t deltaMs = getSinceMs(startTime);

	INFO("["<< deltaMs << "ms] Got data packet: " << packetToString(p) << endl);
}


void newLinkCallback(link_info* link){
	uint64_t deltaMs = getSinceMs(startTime);
	INFO("["<< deltaMs << "ms] New link: " << link->getName() << endl);
}

void deadLinkCallback(link_info* link){
	uint64_t deltaMs = getSinceMs(startTime);
	INFO("["<< deltaMs << "ms] Dead link: " << link->getName() << endl);
}

void altLinkCallback(link_info* link, link_info* alt){
	uint64_t deltaMs = getSinceMs(startTime);
	INFO("["<< deltaMs << "ms] Link: " << link->getName() << " using alt path through: " << alt->getName() << endl);
}

void addressChangeCallback(si_address newAddr, si_address oldAddr){
	uint64_t deltaMs = getSinceMs(startTime);
	INFO("["<< deltaMs << "ms] Address changed to: " << newAddr.toString() << " old address was: " << oldAddr.toString() << endl);
}

void orphanedCallback(bool orphaned){
	uint64_t deltaMs = getSinceMs(startTime);
	INFO("["<< deltaMs << "ms] Orphaned state: " << boolToString(orphaned) << endl);
}

void missedHelloCallback(link_info* link){
	uint64_t deltaMs = getSinceMs(startTime);
	INFO("["<< deltaMs << "ms] Missed " << (int)link->missed_announcements << " annoucements from node: " << link->getName() << endl);
}

int main(int argc, char** argv){

	if (getuid() != 0) {
		cerr << "WARNING: Root access expected!\n" << endl;
	}


	si_address nodeAddress("3.1:4:5");
	list<string> interfaces;
	list<string> excludeInterfaces;
	bool allInterfaces=false;
	bool mmtNode=false;
	int mmtUid=0;

	//Default to automatic address assignment
	bool automaticAddr = true;
	bool automaticMMT = true;

	bool listenForNeighbors = false;
	uint32_t listenTimeoutMs = 4 * ANNOUNCE_PERIOD_SEC * 1000;
	uint8_t autoAddressType = DOWNLINK;

	int opt = 0;
	struct option long_options[] = {
		//{"config", 1, 0, 'c'},
		{"address", 1, 0, 'a'},
		{"auto-conf", 1, 0, 'A'},
		{"address-conf", 1, 0, 'c'},
		{"address-listen", 1, 0, 'l'},	//Listen for neighbors then select address
		{"interface", 1, 0, 'i'},
		{"all_interfaces", 0,0, 'I'},
		{"mmt", 1, 0, 'm'},
		{"exclude", 1, 0, 'x'},
		{"help", 0, 0, '?'},
		{"version", 0, 0, 'v'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "Ac:a:i:l:x:h?Im:vsx:", long_options, NULL)) != -1) {
		switch(opt){
			case 'A':
			case 'c':
				automaticAddr = true;
				automaticMMT = true;
				if(optarg != NULL){
					setConfigPath(optarg);
				}
				cout<<"Debug: Using config file: "<<getConfigPath()<<endl;
				break;
			case 'a':
				nodeAddress=si_address(optarg);
				automaticAddr = false;
				break;
			case 'i':
				interfaces.push_back(optarg);
				break;
			case 'l':
				if(strcmp(optarg, "child")==0 || strcmp(optarg, "downlink")==0){
					autoAddressType=DOWNLINK;
				}
				else if(strcmp(optarg, "parent")==0 || strcmp(optarg, "uplink")==0){
					autoAddressType=UPLINK;
				}
				else if(strcmp(optarg, "trunk")==0 || strcmp(optarg, "sibling")==0){
					autoAddressType=TRUNK;
				}
				else if(strcmp(optarg, "internal")==0 || strcmp(optarg, "join")==0){
					autoAddressType=INTERNAL;
				}
				else{
					cout << "ERROR: Unknown address type [" << optarg << "]" << endl;
					print_help(argv);
					if(myNode != NULL){
						delete myNode;
					}
					exit(0);
				}
				automaticAddr = false;
				listenForNeighbors = true;
				break;
			case 'm':
				mmtNode=true;
				mmtUid=atoi(optarg);
				automaticMMT = false;
				break;
			case 'x':
				excludeInterfaces.push_back(optarg);
				break;
			case 'I':
				allInterfaces=true;
				break;
			case 's':
				multiThreaded = false;
				break;
			case 'v':
				cout<<VERSION_STRING<<endl;
				exit(0);
			case '?':
			case 'h':
				print_help(argv);
				if(myNode != NULL){
					delete myNode;
				}
				exit(0);
				break;
		}
	}


	signal(SIGINT, shutdown);
	signal(SIGTERM, shutdown);
	signal(SIGABRT, shutdown);

	if(automaticAddr){
		string addressStr = getAssignedAddress();

		if(!addressStr.empty()){
			nodeAddress = si_address(addressStr);

			cout << "INFO: Using assigned address " << nodeAddress.toString() << endl;
		}
		else{
			cout << "ERROR: Failed to determine assigned address!" << endl;
			return -1;
		}
	}

	if(automaticMMT){
		if(isMMTEnabled() == 1){
			mmtNode = true;
			mmtUid = getMMTUID();
			if(mmtUid < 0){
				cout << "ERROR: Failed to determine assigned MMT UID!" << endl;
				return -1;
			}
		}
		else{
			mmtNode = false;
		}
	}

	myNode=new si_node(nodeAddress);

	//Set MMT parameters
	myNode->setMMTEnable(mmtNode);
	myNode->setMMTUid(mmtUid);

	if(!allInterfaces){
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

			if(!myNode->addInterface(interfaces.back())){
				cout<<"Error opeing interface: "<<interfaces.back()<<endl;
				exit(-1);
			}
			interfaces.pop_back();
		}
	}
	else{
		excludeInterfaces.push_back(getControlIFName());
		if(!myNode->addAllInterfaces(excludeInterfaces)){
			cout<<"Error opening interfaces!"<<endl;
			exit(-1);
		}
	}

	//Set up callback handlers
	myNode->setAddressChangeCallback(addressChangeCallback);
	myNode->setAltLinkCallback(altLinkCallback);
	myNode->setConsumeCallback(consumeCallback);
	myNode->setDeadLinkCallback(deadLinkCallback);
	myNode->setMissedHelloCallback(missedHelloCallback);
	myNode->setNewLinkCallback(newLinkCallback);
	myNode->setOrphanedCallback(orphanedCallback);

	gettimeofday(&startTime, NULL);

	if(listenForNeighbors){
		cout<<"Listening for neighbors..."<<endl;
		myNode->setAnnounceEnable(false);
		if(myNode->neighborWait(listenTimeoutMs) > 0){
			vector<link_info*> links = myNode->getAllLinks();

			DEBUG_MSG("Found " << links.size() << " links" << endl);

			link_info* target = links.front();
			for(uint i=0; i<links.size(); i++){
				link_info* link = links[i];

				if(link->address < target->address){
					target = link;
				}
			}

			DEBUG_MSG("Selected target link " << target->getName() << endl);
		}
		else{
			cout << "ERROR: Failed to detect any neighboring nodes!" << endl;
			//delete myNode;
			return -1;
		}

		cout<<"Listening for neighbors...DONE"<<endl;
		myNode->setAnnounceEnable(true);
	}

	cout<<"SI Address="<<myNode->getLocalAddress().toString()<<endl;

	sleep(2);

	if(multiThreaded){
		if(myNode->start()){
			cout<<"Node started"<<endl;
			while(canrun && myNode->isRunning()){
				sleep(2);
			}

			if(canrun){
				cout<<"Node terminated unexpectedly"<<endl;
			}
			else{
				cout<<"Stopping node"<<endl;
				if(!myNode->stop(true)){
					cout<<"Failed to stop routing thread"<<endl;
				}
			}
		}
		else{
			cout<<"Failed to start routing thread"<<endl;
		}
	}
	else{
		cout<<"Node started single threaded..."<<endl;

		myNode->run( (void*)myNode );

	}

	cout<<"Exiting"<<endl;
	delete myNode;
	return 0;
}

void print_help(char** argv) {
	cout<<"Usage: "<<argv[0]<<" [options]"<<endl<<endl;
	cout<<"\t-a [address]\t"<<"Set node address"<<endl;
	cout<<"\t-A\t\t"<<"Set address using defualt expirement configuration file path"<<endl;
	cout<<"\t-c [config]\t"<<"Set FCT and MMT parameters using configuration file"<<endl;
	cout<<"\t-l [addr_type]\t" << "Listen for neighboring nodes and select node with the"<<endl \
							   << "\t\t\t" "lowest address as the target to generate an address"<<endl \
							   << "\t\t\t" "relative to. Address type may be downlink, uplink, trunk or internal." <<endl;
	cout<<"\t-i [ifname]\t"<<"Add interface to listen on"<<endl;
	cout<<"\t-I\t\t"<<"Adds all expirement interfaces"<<endl;
	cout<<"\t-x [ifname]\t"<<"Interface to exclude from communication"<<endl;
	cout<<"\t-m [uid]\t"<<"Start an MMT enabled node with supplied uid"<<endl;
	cout<<"\t"<<"-s\t\t"<<"Run in a single thread"<<endl;
	cout<<"\t-h\t\t"<<"Print this help"<<endl;
	cout<<"\t-v\t\t"<<"Version information"<<endl;
}

void shutdown(int signal){
	if(myNode!=NULL){
		cout<<"Stopping node"<<endl;
		if(!myNode->stop(true)){
			cout<<"Failed to stop routing thread"<<endl;
		}
	}
	canrun=false;
}
