/**
 * @file sitest.cpp
 * @short Tests of library functions
 **/

#define DEBUG 1


#include <iostream>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "switched.h"
#include "si_node.h"
#include "rawutils.h"

using namespace std;

#define EXIT_TEST_FAIL(msg) cout<<endl<<"TEST FAILED: "<<msg<<endl; exit(-1)
#define EXIT_TEST_SUCCESS(msg) cout<<endl<<"SUCCESS: "<<msg<<endl; return 0;

#define WAKE_UP_STR "Wake up!"
#define WAKE_UP_STR_LEN strlen(WAKE_UP_STR)

struct sigaction alarmAction;

si_socket* master=NULL;
si_node* testNode=NULL;

enum TestIndex{TimerTest=1, AddressTest, PacketTest, SocketTest, NodeTest};
enum SITestPhases{Idle, LinkDiscover=1, TrunkRoute, DownRoute, ConsumeRoute, UpRoute, HaltSITest};

int siTestPhase=Idle;

void setTimer(int timeoutMs, int intervalMs);

void alarmHandler(int i){
	if(siTestPhase==HaltSITest){
		cout<<"::alarmHandler() - WARNING, Stopping si_node run loop"<<endl;
		testNode->stop();
	}
	else if(siTestPhase==LinkDiscover){
		si_address sender("1.3");
		cout<<"::alarmHandler() - WARNING, Sending announce packet from "<<sender.toString()<<endl;
		si_packet packet(sender, si_address::linkLocal(), (uint8_t*)"HELLO\0", 6);
		packet.type=PROTO_SI_CMP;
		int bytes=master->send(packet);
		cout<<"::alarmHandler() - WARNING, Sent "<<bytes<<"bytes"<<endl;
		siTestPhase++;
		setTimer(2000, 0);
	}
	else if(siTestPhase==TrunkRoute){
		si_address sender("3.2:4:9");
		si_address dest("3.3:4:2");

		si_packet packet(sender, dest, (uint8_t*)"DATA\0", 5);
		cout<<"::alarmHandler() - WARNING, Sending packet that should go to a TRUNK link from "<<packet.getSource().toString();
		cout<<" to "<<packet.getDestination().toString()<<endl;
		master->send(packet);
		siTestPhase++;
		setTimer(500, 0);
	}
	else if(siTestPhase==DownRoute){
		si_address sender("1.3");
		si_address dest("3.2:4:10");

		si_packet* packet=new si_packet(sender, dest, (uint8_t*)"MOREDATA\0", 9);
		cout<<"::alarmHandler() - WARNING, Sending packet that should go to a DOWN link from "<<packet->getSource().toString();
		cout<<" to "<<packet->getDestination().toString()<<endl;
		testNode->injectPacket(packet);
		siTestPhase++;
		setTimer(1000, 0);
	}
	else if(siTestPhase==ConsumeRoute){
		si_address sender("2.3:9");
		si_address dest("1.2");

		si_packet packet(sender, dest, (uint8_t*)"EVENMOREDATA\0", 13);
		cout<<"::alarmHandler() - WARNING, Sending packet that should go to a DOWN link from "<<packet.getSource().toString();
		cout<<" to "<<packet.getDestination().toString()<<endl;
		master->send(packet);
		siTestPhase=HaltSITest;
		setTimer(1000, 0);
	}
	else{
		cout<<"::alarmHandler() - WARNING, Sending wakeup packet"<<endl;
		if(master!=NULL){
			si_packet packet;
			packet.setPayload((uint8_t*)WAKE_UP_STR, WAKE_UP_STR_LEN);
			if(master->send(packet) == packet.byteCount()){
				cout<<"::alarmHandler() - WARNING, Packet sent"<<endl;
			}
			else{
				cout<<"::alarmHandler() - ERROR, Sending wakeup packet failed!"<<endl;
			}
		}
	}
}


void setTimer(int timeoutMs=200, int intervalMs=0){
	itimerval timerVal;

	alarmAction.sa_handler = *alarmHandler;
	sigemptyset(&alarmAction.sa_mask);
	alarmAction.sa_flags = 0;
	sigaction(SIGALRM, &alarmAction, NULL);

	timerVal.it_value.tv_sec=timeoutMs/1000;
	timerVal.it_value.tv_usec=(timeoutMs-timerVal.it_value.tv_sec*1000)*1000;

	timerVal.it_interval.tv_sec=intervalMs/1000;
	timerVal.it_interval.tv_usec=(intervalMs-timerVal.it_interval.tv_sec*1000)*1000;

	setitimer(ITIMER_REAL, &timerVal, NULL);
}


int main(int argc, char** argv){
	Timer totalTime(true);

	int testLevel=10;
	if(argc == 2){
		testLevel=atoi(argv[1]);
	}

	/*cout<<"==BIT MANIPULATION TEST=="<<endl;
	uint8_t src_buf[]={0xff, 0xff, 0xf0, 0xff};
	uint8_t dest_buf[]={0x00, 0x00, 0x0f, 0x00};

	bit_desc src_desc={1, 7, src_buf};
	bit_desc dest_desc={0, 7, dest_buf};

	bitCopy(&dest_desc, &src_desc, 24);

	printBits(src_buf, 32);
	printBits(dest_buf, 32);

	exit(1);*/

	// TIMER TEST
	cout<<"==TIMER TEST=="<<endl;
	Timer timer0(true);
	Timer timer1(false);
	Timer timer2(false);

	timer1.print();
	timer1.start();
	timer0.stop();

	for(int i=0; i<2147483646/4; i++){
	}

	timer1.stop();
	timer1.print();

	timer0.resume();
	sleep(2);
	sleep(1);
	sleep(1);
	timer0.stop();


	timer2.start();
	for(int i=0; i<2147483646/4; i++){
	}
	timer2.stop();
	timer2.print();

	timer0.stop();
	timer0.print();
	cout<<"==TIMER TEST COMPLETE=="<<endl<<endl;
	if(testLevel == TimerTest){ EXIT_TEST_SUCCESS("Timer Test Success"); }



	cout<<"==ADDRESS TEST=="<<endl;
	cout<<"Link Local = " << si_address::linkLocal().toString() <<endl;
	cout<<"Broadcast = " << si_address::broadCast().toString() <<endl;

	si_address addr1("4.f.aad");
	si_address addr2("4.f.aad.de.2");

	uint8_t* data=new uint8_t[addr2.byteCount()];

	for(int i=0; i<addr2.byteCount();i++){data[i]=0x00;}

	cout<<"bits: "<<addr2.writeTo(data, addr2.byteCount()+1)<<endl;

	cout<<addr2.toString()<<endl;

	raw_address::printAddressBits(data);


	cout<<endl<<"SI_RAW_ADDRESS Functionality Test"<<endl;

	int chunkCount=raw_address::chunkCount(data);
	cout<<"chunkCount="<<chunkCount<<endl;

	raw_address::printAddress(data);
	cout<<endl;

	cout<<"Bit Count="<<raw_address::bitCount(data)<<endl;
	cout<<"Byte Count="<<raw_address::byteCount(data)<<endl;

	raw_address::incrementTier(data, -2);

	cout<<endl<<"Removing field"<<endl;
	raw_address::removeChunk(data,0);
	int removed=raw_address::removeChunk(data,0);
	cout<<"Removed "<<removed<<"bits"<<endl;

	raw_address::printAddressBits(data);


	chunkCount=raw_address::chunkCount(data);
	cout<<"chunkCount="<<chunkCount<<endl;

	raw_address::printAddress(data);
	cout<<endl;

	cout<<"Bit Count="<<raw_address::bitCount(data)<<endl;
	cout<<"Byte Count="<<raw_address::byteCount(data)<<endl;


	//exit(1);




	si_address blank;
	cout<<"Blank address = "<<blank.toString()<<endl;
	cout<<"==ADDRESS TEST COMPLETE=="<<endl<<endl;
	if(testLevel == AddressTest){ EXIT_TEST_SUCCESS("Address Test Success"); }


	cout<<"==PACKET TEST=="<<endl;
	cout<<"Constructing packet from "<<addr1.toString() << " to " << addr2.toString()<<endl;

	string packData="Brown Fox";
	string prependStr="The Quick ";

	si_packet pack(addr1, addr2, (uint8_t*)packData.c_str(), packData.length());
	pack.prependPayload((uint8_t*)prependStr.c_str(), prependStr.size());

	cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;
	cout<<"Packet Payload = "<<(char*)pack.getPayloadPtr()<<endl;
	pack.setSource(si_address::linkLocal());
	cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;
	pack.setSource(addr1);
	cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;


	raw_packet testRawPkt(pack);
	testRawPkt.printPacket();
	cout<<endl;
	testRawPkt.removeFirstDestAF();
	testRawPkt.printPacket();
	cout<<endl;

	cout<<"==PACKET TEST COMPLETE=="<<endl;
	if(testLevel == PacketTest){ EXIT_TEST_SUCCESS("Packet Test Success"); }


	//exit(1);

	/*pack.updateDest();
	cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;
	pack.restoreDest();
	cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;
	pack.updateDest();
	cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;
	pack.setSource(blank);*/
	//pack.updateDest();
	/*cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;

	pack.setSource(addr1);
	cout<<"pack.setDestination(addr1);"<<endl;
	pack.setDestination(addr1);
	cout<<"pack.updateDest();"<<endl;*/
	//pack.updateDest();
	//cout<<"Packet from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;

	/*cout<<"Chunks = "<<pack.getDestination().chunks.size();
	cout<<"\tDestination addr="<<pack.getDestination().toString()<<endl;*/
	cout<<endl;

	cout<<"==SOCKET TEST=="<<endl;
	si_socket socket("lo");
	if(!socket.isOpen()){
		EXIT_TEST_FAIL("SOCKET NOT OPEN");
	}

	cout<<"Sending packet on interface "<<socket.getName()<<" until response is recieved..."<<endl;
	while(1){
		int sentBytes=socket.send(pack);
		if(sentBytes<0){
			cout<<"ERROR, si_socket.send() failed with code "<<sentBytes<<"!"<<endl;
			cout<<"\tCheck your permissions for using interface "<<socket.getName()<<endl;
			EXIT_TEST_FAIL("SOCKET SEND PACKET");
		}


		if(sentBytes != pack.byteCount()){
			cout<<"ERROR, Sent bytes("<<sentBytes<<") does not equal length of packet(";
			cout<<pack.byteCount()<<")!"<<endl;
			EXIT_TEST_FAIL("SOCKET SEND PACKET");
		}
		cout<<"DEBUG, Sent "<<sentBytes<<" bytes of "<<pack.byteCount();
		cout<<" from "<<pack.getSource().toString() << " to " << pack.getDestination().toString()<<endl;
		si_packet* rx_pack = socket.recv(false);
		if(rx_pack != NULL){
			cout<<"DEBUG, Recieved packet with "<<rx_pack->byteCount()<<" bytes";
			cout<<" from "<<rx_pack->getSource().toString() << " to " << rx_pack->getDestination().toString()<<endl;
			if(*rx_pack == pack){
				cout << "DEBUG, Sent packet is identical to recieved."<<endl;
			}
			else{
				cout << "ERROR, Sent packet differs from recieved."<<endl;
				delete rx_pack;
				EXIT_TEST_FAIL("SOCKET RECV PACKET");
			}

			//cout<<pack.toString()<<endl;
			//cout<<rx_pack->toString()<<endl;

			if(rx_pack->encapsulateSIHeader()){
				if(rx_pack->byteCount() != pack.byteCount()+pack.siHeaderByteCount()){
					cout<<"ERROR, Encapsulated packet length is "<<rx_pack->byteCount()<<" bytes";
					cout<<" when "<<pack.byteCount()+pack.siHeaderByteCount()<<" bytes were expected"<<endl;
					delete rx_pack;
					EXIT_TEST_FAIL("PACKET ENCAPSULATION");
				}
				else{
					cout<<"DEBUG, Encapsulated packet is "<<rx_pack->byteCount()<<" bytes, as expected"<<endl;
				}

				if(!rx_pack->decapsulate()){
					cout<<"ERROR, Call to si_packet::decapsulate() returned FALSE!"<<endl;
					delete rx_pack;
					EXIT_TEST_FAIL("PACKET DECAPSULATION");
				}
				if(*rx_pack == pack){
					cout << "DEBUG, Decapsulated packet is identical to sent."<<endl;
				}
				else{
					cout << "ERROR, Sent packet differs from recieved."<<endl;
					delete rx_pack;
					EXIT_TEST_FAIL("PACKET DECAPSULATION");
				}
			}
			else{
				cout<<"ERROR, Call to si_packet::encapsulate() returned FALSE!"<<endl;
				delete rx_pack;
				EXIT_TEST_FAIL("PACKET ENCAPSULATION");
			}


			delete rx_pack;
			break;
		}
	}

	socket.close();
	if(socket.isOpen()){
		EXIT_TEST_FAIL("SOCKET DID NOT CLOSE");
	}
	cout<<"==SOCKET TEST COMPLETE=="<<endl<<endl;
	if(testLevel == SocketTest){ EXIT_TEST_SUCCESS("Socket Test Success"); }


	cout<<"==SI_NODE TEST=="<<endl;
	si_node node(si_address("1.2"));

	if(!node.addInterface("lo")){
		cout<<"ERROR, si_node::addInterface(\"lo\") returned false!"<<endl;
		EXIT_TEST_FAIL("SI_NODE ADD INTERFACE");
	}

	master=node.getInterface("lo");

	if(master==NULL){
		cout<<"ERROR, si_node::getInterface(\"lo\") return NULL!"<<endl;
		EXIT_TEST_FAIL("SI_NODE GET INTERFACE");
	}

	cout<<"Created node with address "<<node.getLocalAddress().toString()<<endl;
	Timer readyLinksTimer(true);
	setTimer(1500, 0);
	vector<si_socket*> readySockets = node.getReadySockets(5000);
	readyLinksTimer.stop();
	cout<<"DEBUG, Waited for ready link for ";
	timeval elapsed=readyLinksTimer.getElapsedReal();
	cout<<TIMEVAL_TO_SEC_F(elapsed)<<"s"<<endl;

	cout<<"DEBUG, Ready links.size = "<<readySockets.size()<<endl;

	for(uint32_t i=0; i<readySockets.size(); i++){
		si_socket* socket=readySockets.at(i);

		if(socket==NULL){
			cout<<"ERROR, si_node::getReadySockets() returned a NULL socket pointer at index"<<i<<endl;
			EXIT_TEST_FAIL("SI_NODE GET READY LINKS");
		}

		si_packet* wake_pack = socket->recv(true);
		if(wake_pack==NULL){
			cout<<"ERROR, Call to socket::recv did not return a packet on interface "<<socket->getName()<<endl;
			EXIT_TEST_FAIL("SI_NODE LINK NOT READY");
		}

		if(wake_pack->payloadLength() != WAKE_UP_STR_LEN){
			cout<<"ERROR, Recieved packet is not the expected length!"<<endl;
			delete wake_pack;
			EXIT_TEST_FAIL("INCORRECT PACKET LENGTH");
		}

		if(strncmp((char*)wake_pack->getPayloadPtr(), WAKE_UP_STR, WAKE_UP_STR_LEN) != 0){
			cout<<"ERROR, Recieved wake up packet does not contain expected payload!"<<endl;
			delete wake_pack;
			EXIT_TEST_FAIL("INCORRECT PACKET PAYLOAD");
		}

		cout<<"DEBUG, Wakeup packet contained expected payload."<<endl;
		delete wake_pack;
	}

	cout<<endl<<"Testing si_node routing behaviour."<<endl<<endl;

	siTestPhase=LinkDiscover;
	setTimer(200, 0);
	testNode=&node;
	node.runOnce();
	while(siTestPhase!=HaltSITest){}
	cout<<"==SI_NODE TEST COMPLETE=="<<endl<<endl;
	//node.stop(true, 2000);
	if(testLevel == NodeTest){ EXIT_TEST_SUCCESS("Node Test Success"); }

	cout<<"==SI_NODE SPEED TEST=="<<endl;
	int packets=2*51200;
	int packetSize=1024;
	si_address sendAddr("2.3.4");
	si_address trunkAddr("1.3");
	si_address consumeAddr("1.2");
	si_address downAddr("2.1.2");

	node.removeInterface("lo");

	cout<<"Injecting "<<packets<<" packets totalling "<<(packets*packetSize)/(1024*1024)<<"Mb of payload"<<endl;
	sleep(2);
	Timer injectTime(true);
	uint8_t payloadBuf[packetSize];
	for(int i=0; i<packets; i++){
		si_packet* p=new si_packet(sendAddr, downAddr, payloadBuf, packetSize);
		node.injectPacket(p);
	}
	injectTime.stop();
	cout<<"Packet injection time";
	injectTime.printLine();
	cout<<endl;
	sleep(10);

	cout<<"Routing..."<<endl;
	node.runOnce();


	totalTime.stop();
	totalTime.print();
	node.reportRouteRate();

	EXIT_TEST_SUCCESS("All test cases worked");
}

