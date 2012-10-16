/**
 * @file siperf.cpp
 * @short A performance measurement utility similar to iperf
 **/

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
#include <math.h>
#include <arpa/inet.h>
#include <sched.h>

#include "debug.h"
#include "switched.h"
#include "si_node.h"


using namespace std;

struct bandwidth_report{
	Timer timer;
	uint32_t packets;
	uint32_t packetSize;

	bandwidth_report(){
		clear();
		packetSize=0;
	}

	void clear(){
		timer.reset();
		packets=0;
	}
};

enum AddressMode{UserDef=0x01, AddrChild, AddrParent, AddrTrunk, AddrInternal};

bool canrun=true;
bandwidth_report intervalReport;
bandwidth_report completeReport;
int reportInterval=2000;	//Time in ms
int testDuration=10000;		//Run for 10seconds by default
uint32_t rxSequenceNum=0;

void print_help(char** argv);
void shutdown(int signal);
void intervalService(int signal);
si_address determineAddress(AddressMode mode, string ifname);
si_packet* initDataPacket(si_address sender, si_address dest, int payloadSize);

map<si_address, bandwidth_report> clients;
void serverConsumePacket(raw_packet* packet);
void busyWait(int endtime, Timer& timer);

int main(int argc, char** argv){
	if(argc==1){
		print_help(argv);
		exit(0);
	}

	if (getuid() != 0) {
		cerr << "WARNING: Root access expected!\n" << endl;
	}

	si_address nodeAddress;
	si_address targetAddress;
	vector<string> interfaceList;
	bool isClient=true;
	bool ping=false;
	bool neighborWait=false;

	AddressMode autoAddressType=UserDef;

	si_node* node=NULL;
	si_socket* sock=NULL;
	si_packet* dataPacket=NULL;
	uint16_t* sequenceNum=NULL;
	int payloadSize=-1;		//Maximum size
	int txRate=-1;			//Packets per second
	uint32_t pktDelay=0;	//Interpacket tx delay in ms

	int opt = 0;
	struct option long_options[] = {
		{"client", 1, 0, 'c'},
		{"address", 1, 0, 'a'},
		{"interface", 1, 0, 'i'},
		{"interval", 1, 0, 't'},
		{"duration", 1, 0, 'd'},
		{"rate", 1, 0, 'r'},
		{"bytes", 1, 0, 'b'},
		{"server", 0, 0, 's'},
		{"wait", 0, 0, 'w'},
		{"auto_address", 1, 0, 'x'},
		{"ping", 0, 0, 'p'},
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "a:i:c:swt:pd:x:r:b:?hv", long_options, NULL)) != -1) {
		switch(opt) {
			case 'a':
				nodeAddress=si_address(optarg);
				break;
			case 'i':
				interfaceList.push_back(string(optarg));
				break;
			case 'c':
				isClient=true;
				targetAddress=si_address(optarg);
				break;
			case 'p':
				ping=true;
				break;
			case 's':
				isClient=false;
				break;
			case 't':
				reportInterval=atoi(optarg);
				break;
			case 'r':
				txRate=atoi(optarg);
				break;
			case 'd':
				testDuration=atoi(optarg);
				break;
			case 'b':
				payloadSize=atoi(optarg);
				break;
			case 'w':
				neighborWait=true;
				break;
			case 'x':
				if(strcmp(optarg, "child") == 0){
					autoAddressType=AddrChild;
				}
				else if(strcmp(optarg, "parent")==0){
					autoAddressType=AddrParent;
				}
				else if(strcmp(optarg, "trunk")==0){
					autoAddressType=AddrTrunk;
				}
				else if(strcmp(optarg, "internal")==0 || strcmp(optarg, "join")==0){
					autoAddressType=AddrInternal;
				}
				break;
			case 'v':
				cout<<VERSION_STRING<<endl;
				exit(0);
			case 'h':
			case '?':
				print_help(argv);
				exit(0);
				break;
		}
	}

	if(txRate>0){
		pktDelay=(1000*1000)/txRate;
		cout<<"Send rate="<<txRate<<"pkt/sec"<<endl;
		cout<<"Packet Delay="<<pktDelay<<"us"<<endl;
	}

	if(autoAddressType!=UserDef){
		if(interfaceList.size()==1){
			nodeAddress=determineAddress(autoAddressType, interfaceList.front());
		}
		else{
			cout<<"ERROR: Automatic address assignment requires that only one interface been used!"<<endl;
			exit(-1);
		}
	}

	//Setup SI
	if(interfaceList.size() > 1 || !isClient){	//Server mode
		node=new si_node(nodeAddress);
		while(!interfaceList.empty()){
			if(!node->addInterface(interfaceList.back())){
				cout<<"ERROR: Failed to attach to interface("<<interfaceList.back()<<")"<<endl;
				delete node;
				exit(-2);
			}
			interfaceList.pop_back();
		}
		if(neighborWait){
			cout<<"Listening for all neighbors..."<<endl;
			if(!node->neighborWait(6000)){
				cerr<<"ERROR: No SI neighbors detected!"<<endl;
				exit(-1);
			}
		}
	}
	else if(interfaceList.size()==1){	//Client mode
		//Advertise our address
		sock=new si_socket(interfaceList.back());
		if(!sock->isOpen()){
			cout<<"ERROR: Failed to attach to interface("<<interfaceList.back()<<")"<<endl;
			delete sock;
			exit(-2);
		}

		raw_packet pack;
		pack.clear();
		uint8_t rawAddr[100];
		nodeAddress.writeTo(rawAddr, 100);
		//Construct announce packet
		if(buildADVPacket(&pack, (uint8_t*)raw_linkLocal, rawAddr)){
			DEBUG_MSG("Announce packet size = "<<pack.length<<" bytes"<<endl);

			cout.unsetf(ios::dec);
			cout.setf(ios::hex);

			uint8_t* payloadPtr = pack.payloadPtr();


			uint64_t ptr = (uint64_t) &(*payloadPtr);
			DEBUG_MSG("pointer = "<<ptr<<" payload[0]="<<(uint32_t)payloadPtr[0]<<endl);

			payloadPtr+=3;

			DEBUG_MSG("Hostname = "<<parseString(skipFields(payloadPtr,2))<<endl);

			cout.unsetf(ios::hex);
			cout.setf(ios::dec);

			cout<<"Announcing address"<<endl;
			sock->send(pack);


		}
		else{
			ERROR("Failed to construct announce packet, aborting!"<<endl);
			ABORT_NOW();
		}


	}
	else{
		cerr<<"ERROR: Atleast one interface required!"<<endl<<endl;
		print_help(argv);
		exit(-1);
	}

	completeReport.clear();
	intervalReport.clear();

	//Configure CTRL+C
	signal(SIGINT, shutdown);
	signal(SIGTERM, shutdown);
	signal(SIGABRT, shutdown);

	if(isClient){
		if(ping){	//Construct Ping packet
			dataPacket=new si_packet(nodeAddress, targetAddress);
			dataPacket->expandPayload(3);
			dataPacket->getPayloadPtr()[0]=(CMP_EchoRequest<<4) & 0xf0;
			dataPacket->type=PROTO_SI_CMP;
			sequenceNum=(uint16_t*)&dataPacket->getPayloadPtr()[1];
			*sequenceNum=htons(0);
			cout<<"PING "<<targetAddress.toString()<<" "<<dataPacket->byteCount()<<" bytes of data"<<endl;
			cout<<"PINGING "<<dataPacket->getDestination().toString()<<" as "<<dataPacket->getSource().toString()<<endl;

		}
		else{	//Construct data packet
			dataPacket=initDataPacket(nodeAddress, targetAddress, payloadSize);
		}

		intervalReport.packetSize=dataPacket->byteCount();
		completeReport.packetSize=intervalReport.packetSize;
		if(sock!=NULL){

			if(ping){	//PING
				int pingCount=0;
				float msTotal=0.0f;
				float msLeast=100000;
				float msMost=-1;
				completeReport.timer.start();
				while(canrun && Timer::timevalToMs(completeReport.timer.getElapsedReal()) < testDuration){
					*sequenceNum=htons(ntohs(*sequenceNum)+1);
					intervalReport.clear();
					intervalReport.timer.start();

					sock->send(*dataPacket);
					struct timeval tv;
					gettimeofday(&tv, NULL);
					//cout<<"Pinging at Seconds:"<<tv.tv_sec<<" uSeconds:"<<tv.tv_usec<<endl;

					//cout<<"sent seq="<<ntohs(*sequenceNum)<<" proto="<<dataPacket->type<<endl;
					while(canrun && Timer::timevalToMs(completeReport.timer.getElapsedReal()) < testDuration){
						//if(sock->canRead(1000)){
							si_packet* reply=sock->recv(true);
							intervalReport.timer.stop();
							if(reply->type==PROTO_SI_CMP){
								uint8_t type=(reply->getPayloadPtr()[0] & 0xf0)>>4;
								uint16_t* seq=(uint16_t*)&reply->getPayloadPtr()[1];
								if(type==CMP_EchoReply){
									//cout<<dataPacket->byteCount()<<" bytes from "<<reply->getSource().toString()<<": ";
									*seq=ntohs(*seq);
									timeval time=intervalReport.timer.getElapsedReal();
									float ms=(((float)time.tv_sec*1000) + ((float)time.tv_usec/1000));
									msTotal+=ms;
									cout<<"seq="<<*seq<<" time="<<ms<<"ms"<<endl;
									pingCount++;
									if(ms<msLeast){
										msLeast=ms;
									}
									if(ms>msMost){
										msMost=ms;
									}
									break;
								}
								else{
									cout<<"type="<<(int)type<<endl;
								}
							}
							else{
								cout<<"proto="<<(int)reply->type<<endl;
							}

						/*}
						else{
							cout<<"can't read"<<endl;
						}*/
					}
					usleep(500);
				}
				msTotal=msTotal/(float)pingCount;
				cout<<"Avg Time: "<<msTotal<<"ms"<<endl;
				cout<<"Least Time:"<<msTotal<<"ms"<<endl;
				cout<<"Most Time:"<<msMost<<"ms"<<endl;
				delete sock;
			}
			else{	//THROUGHPUT
				completeReport.timer.start();
				intervalReport.timer.start();

				//Change process scheduler to FIFO and up our priority
				struct sched_param sp;
				sp.__sched_priority=50;
				sched_setscheduler(0, SCHED_FIFO, &sp);

				//Setup alarm handler to print transmission reports
				struct sigaction alarmAction;
				alarmAction.sa_handler = *intervalService;
				sigemptyset(&alarmAction.sa_mask);
				alarmAction.sa_flags = 0;
				sigaction(SIGALRM, &alarmAction, NULL);

				//Setup timer to trigger alarm for displaying report
				installTimer(reportInterval, reportInterval);

				Timer sendTimer(false);
				uint32_t lastPktTime=pktDelay;
				uint32_t* txSequenceNum=(uint32_t*)&(dataPacket->getPayloadPtr()[0]);
				*txSequenceNum=htonl(1);
				while(canrun && Timer::timevalToMs(completeReport.timer.getElapsedReal()) < testDuration){
					sendTimer.reset();
					sendTimer.start();
					if((uint32_t)(sock->send(*dataPacket)) == intervalReport.packetSize){
						//busyWait(pktDelay, intervalReport.timer);
						intervalReport.packets++;
						*txSequenceNum=htonl(ntohl(*txSequenceNum)+1);
						if(pktDelay>0){

							/*int deltaDelay=0;
							uint32_t avgDelay=TIMEVAL_TO_uS_F(intervalReport.timer.getElapsedReal())/intervalReport.packets;
							deltaDelay=pktDelay-avgDelay;

							if(deltaDelay>0){
								usleep(deltaDelay);
							}*/

							uint32_t usedTime=TIMEVAL_TO_uS_F(sendTimer.getElapsedReal());
							uint32_t deltaDelay=2*pktDelay-(usedTime+lastPktTime);
							//uint32_t delay=deltaDelay+pktDelay;


							if(deltaDelay>10){
								if(deltaDelay>2*pktDelay){
									deltaDelay=pktDelay;
								}

								/*if(lastPktTime > 20+deltaDelay){
									busyWait(deltaDelay/3, sendTimer);
									//cout<<"Busy: ";
								}
								else{*/
									timespec deltaSpec;
									deltaSpec.tv_sec=0;
									deltaSpec.tv_nsec=deltaDelay*1000;

									nanosleep(&deltaSpec, NULL);
								//}
								//cout<<usedTime<<" "<<lastPktTime<<" "<<deltaDelay<<" "<<endl;
							}
							sendTimer.stop();
							lastPktTime=TIMEVAL_TO_uS_F(sendTimer.getElapsedReal());

						}
						else{
							//cout<<"pktDelay<0"<<endl;
						}
					}
					else{
						cout<<"Packet send failed:"<<intervalReport.packets+1<<endl;
					}
				}
				uint8_t* dataPtr= dataPacket->getPayloadPtr();
				dataPtr[0]=0xFF;
				dataPtr[1]=0xFF;
				dataPtr[2]=0xFF;
				dataPtr[3]=0xFF;
				sock->send(*dataPacket);
				completeReport.packets++;
				completeReport.timer.stop();
				delete sock;

				cout<<endl;
				intervalService(0);
				intervalReport=completeReport;
				cout<<"--Total Report--"<<endl;
				intervalService(0);
			}
		}
		else if(node!=NULL){
			delete node;
		}
		delete dataPacket;
	}
	else{	//SERVER
		if(node!=NULL){
			node->setConsumeCallback(&serverConsumePacket);
			if(node->start()){
				while(canrun){
					sleep(2);
				}

				cout<<"Stopping si_node"<<endl;
				node->stop(true);
			}
			delete node;
		}
		else if(sock!=NULL){
			delete sock;
		}

	}

}

void print_help(char** argv) {
	cout<<"Usage: "<<argv[0]<<" [options]"<<endl<<endl;

	cout<<"\t-i [interface]"<<endl;
	cout<<endl;

	cout<<"Addressing"<<endl;
	cout<<"\t-x [mode]\t"<<"Assign address automatically based off of the first heard announce."<<endl;
	cout<<"\t\t\t"<<"Allowed modes are child, parent, trunk, or internal"<<endl;
	cout<<"\t-a [address]\t"<<"Source address"<<endl;
	cout<<endl;
	cout<<"Server"<<endl;
	cout<<"\t-s\t\t"<<"Run as a server"<<endl;
	cout<<endl;
	cout<<"Client"<<endl;
	cout<<"\t-c [dest]\t"<<"Run as a client and send traffic to dest"<<endl;
	cout<<"\t-p\t\t"<<"Ping the address specified by [dest]"<<endl;
	cout<<"\t-t [interval]\t"<<"Time between throughput reports in ms, defaults to "<<reportInterval<<"ms"<<endl;
	cout<<"\t-d [duration]\t"<<"Duration of throughput test in ms, defaults to "<<testDuration<<"ms"<<endl;
	cout<<"\t-b [bytes]\t"<<"Payload size of packets in throughput test in bytes, fills entire packet by default"<<endl;
	cout<<"\t-r [rate]\t"<<"Maximum transmission rate in packets per second, defaults to infinity"<<endl;
	cout<<"\t-v\t\t"<<"Version information"<<endl;
	cout<<endl;

}

void shutdown(int signal){
	canrun=false;
}

/*void recvFromSender(){
	while(canrun){
		if()
	}
}*/

void intervalService(int signal){
	intervalReport.timer.stop();
	float bytes=intervalReport.packets * intervalReport.packetSize;
	float MBytes=bytes/(1024*1024);

	timeval time=intervalReport.timer.getElapsedReal();
	float ms=TIMEVAL_TO_MS_F(time);
	float mb_per_sec=((float)MBytes/ms)*8000;

	cout<<intervalReport.packets<<" packets\t"<<intervalReport.packetSize<<"bytes\t"<<ms<<"ms\t";
	cout<<"\t"<<MBytes<<" MBytes\t"<<mb_per_sec<<"MBits/sec"<<endl;

	completeReport.packets+=intervalReport.packets;

	intervalReport.clear();
	intervalReport.timer.start();
}

si_packet* initDataPacket(si_address sender, si_address dest, int payloadSize=-1){
	si_packet* packet=new si_packet(sender, dest);
	packet->type=PROTO_SI_DATA;
	if(payloadSize==-1 || payloadSize>packet->freeBytes()){
		payloadSize=packet->freeBytes();
	}
	else if(payloadSize < 4){
		payloadSize=4;
	}
	cout<<"Data packet payload_len="<<payloadSize<<" header_len="<<packet->byteCount()<<endl;

	uint8_t* data=new uint8_t[payloadSize];
	for(int i=0; i<payloadSize-1; i++){
		data[i]=0x30 + (i%10);
	}
	data[payloadSize-1]=0x00;

	uint32_t* sequenceNum=(uint32_t*)&data[0];
	*sequenceNum=htonl(1);

	packet->setPayload(data, payloadSize);
	delete[] data;
	return packet;
}

si_address determineAddress(AddressMode mode, string ifname){
	cout<<"Determining node address based on address of interface:"<<ifname<<endl;
	si_socket sock(ifname);
	if(!sock.isOpen()){
		cerr<<"Could not bind socket on interface "<<ifname<<"."<<endl;
		exit(-1);
	}

	si_address newAddr;
	si_address sender;
	bool greeted=false;
	while(!greeted){
		si_packet pack;
		sock.recv(pack, true);

		if(pack.getDestination().bc == 0x3f && pack.type==PROTO_SI_CMP){
			sender=pack.getSource();
			greeted=true;
			cout<<"Found host at "<<sender.toString()<<endl;
		}
	}
	sock.close();

	newAddr=sender;
	if(mode==AddrChild){
		newAddr.bc++;
		newAddr.chunks.push_back(newAddr.chunks.back());
	}
	else if(mode==AddrParent){
		newAddr.bc--;
		newAddr.chunks.pop_back();
	}
	else if(mode==AddrTrunk){
		int chunk=newAddr.chunks.back();
		newAddr.chunks.pop_back();
		chunk+=3;
		newAddr.chunks.push_back(chunk);
	}

	cout<<"Selected address "<<newAddr.toString()<<endl;
	return newAddr;
}

void serverConsumePacket(raw_packet* packet){

	if(packet->length < 4){
		ERROR("Consumed packet is too short!"<<endl);
		delete packet;
		return;
	}
	uint32_t* sequence=(uint32_t*)&(packet->payloadPtr()[0]);
	//cout<<"Consume Packet seq="<<ntohl(*sequence)<<endl;
	if(ntohl(*sequence) == rxSequenceNum){
		rxSequenceNum++;
	}
	else if(*sequence==(uint32_t)0xFFFFFFFF){
		cout<<"End packet seq="<<ntohl(*sequence)<<endl;
		rxSequenceNum=1;
	}
	else{
		ERROR("Dropped packets ="<<(ntohl(*sequence) - rxSequenceNum)<<endl);
		rxSequenceNum=ntohl(*sequence)+1;
	}
	delete packet;
}


void busyWait(int usTimeout, Timer &timer){
	usTimeout+=TIMEVAL_TO_uS_F(timer.getElapsedReal());
	//uint32_t i=0;
	while(canrun){
		//if(i%10==0){
			if(((int)TIMEVAL_TO_uS_F(timer.getElapsedReal())) >= usTimeout){
				break;
			}
		//}
		//i++;
	}
}
