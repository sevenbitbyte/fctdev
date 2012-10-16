/**
 * @file si_node.cpp
 * @short Implements the FCT architecture as well as MMT
 **/

#include "debug.h"
#include "si_node.h"
#include "mmt.h"
#include "timer.h"
#include "mutex.h"
#include "emulabutils.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <asm/unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <algorithm>
#include <sstream>

#ifdef BUILD_RELEASE
	#define LOG_PACKET_ACTIONS			false
	#define PRINT_PACKET_REPORT			false
	#define ROUTE_DEBUG					false
	#define PROCESS_CMP_DEBUG			false
	#define READY_LINKS_DEBUG			false
	#define PRINT_PER_PACKET_ROUTE_RATE false
	#define PRINT_ROUTE_RATE			true
	#define PRINT_ROUTING_TABLE			true
	#define PRINT_CONVERGANCE			false
	#define DEAD_LINKS_DEBUG			false
	#define ROUTE_MMT_DEBUG				false
	#define ANNOUNCE_RX_DEBUG			false
	#define ANNOUNCE_TX_DEBUG			false
#endif

#ifdef BUILD_TEST
	#define LOG_PACKET_ACTIONS			false
	#define PRINT_PACKET_REPORT			false
	#define ROUTE_DEBUG					false
	#define PROCESS_CMP_DEBUG			false
	#define READY_LINKS_DEBUG			false
	#define PRINT_PER_PACKET_ROUTE_RATE	false
	#define PRINT_ROUTE_RATE			false
	#define PRINT_ROUTING_TABLE			true
	#define PRINT_CONVERGANCE			false
	#define DEAD_LINKS_DEBUG			true
	#define ROUTE_MMT_DEBUG				false
	#define ANNOUNCE_RX_DEBUG			false
	#define ANNOUNCE_TX_DEBUG			false
	#define ADDRESS_CHANGE_DEBUG		true
#endif

#ifdef BUILD_DEBUG
	#define LOG_PACKET_ACTIONS			false
	#define PRINT_PACKET_REPORT			false
	#define ROUTE_DEBUG					true
	#define PROCESS_CMP_DEBUG			true
	#define PROCESS_CONF_DEBUG			false
	#define READY_LINKS_DEBUG			false
	#define PRINT_PER_PACKET_ROUTE_RATE	false
	#define PRINT_ROUTE_RATE			false
	#define PRINT_ROUTING_TABLE			true
	#define PRINT_CONVERGANCE			false
	#define DEAD_LINKS_DEBUG			true
	#ifdef NO_MMT
		#define ROUTE_MMT_DEBUG	false
	#else
		#define ROUTE_MMT_DEBUG	true
	#endif
	#define ANNOUNCE_RX_DEBUG			false
	#define ANNOUNCE_TX_DEBUG			false
	#define ADDRESS_CHANGE_DEBUG		true
#endif

#ifdef NO_OUTPUT
	#define LOG_PACKET_ACTIONS			false
	#define PRINT_PACKET_REPORT			false
	#define ROUTE_DEBUG					false
	#define PROCESS_CMP_DEBUG			false
	#define READY_LINKS_DEBUG			false
	#define PRINT_PER_PACKET_ROUTE_RATE	false
	#define PRINT_ROUTE_RATE			false
	#define PRINT_ROUTING_TABLE			false
	#define PRINT_CONVERGANCE			false
	#define DEAD_LINKS_DEBUG			false
	#define ROUTE_MMT_DEBUG				false
	#define ANNOUNCE_RX_DEBUG			false
	#define ANNOUNCE_TX_DEBUG			false
#endif


//#define STACK_SIZE 1048576
#define STACK_SIZE 65536

#if LOG_PACKET_ACTIONS
	#define PACKET_ACTION(p, str) p->actionLog.push_back(str)
#else
	#define PACKET_ACTION(p, str) //Disabled
#endif


link_info::link_info(){
	socket=NULL;
	type=UNCONFIGURED;
	hasFailed=false;
	direct=true;
	removable=true;
	lastSequence=0;
	hostname.clear();
	watchdog=NULL;
}

link_info::~link_info(){
	if(this->watchdog!=NULL){
		this->watchdog->remove();
	}
}

/**

	[link_info][ADDRESS]
		type=			TRUNK
		hostname=		HOSTNAME
		interface=		ethX
		mmt_enabled=	false
		hasFailed=		true
		removable=		false
		last_announce=	abcd sec ago
		last_seq=		xxx
		address_list=	{t.aa:bb:cc, }
		trunk_list=		{}

  **/

void link_info::printInfo(){
	INFO(endl);

	APPEND_MSG("[link_info]["<<address.toString()<<"]"<<endl);
	APPEND_MSG("\ttype="<<linkTypeToString(type)<<endl);
	APPEND_MSG("\tminAddress="<<minAddress.toString()<<endl);
	APPEND_MSG("\thostname="<<hostname<<endl);
	APPEND_MSG("\tinterface="<<socket->getName()<<endl);
	APPEND_MSG("\tdirect="<<boolToString(direct)<<endl);
	APPEND_MSG("\tmmt_enable="<<boolToString(mmt_enabled)<<endl);
	APPEND_MSG("\thasFailed="<<boolToString(hasFailed)<<endl);
	APPEND_MSG("\tremovable="<<boolToString(removable)<<endl);
	APPEND_MSG("\tlast_seq="<<(int)lastSequence<<endl);

	timeval currentTime;
	timeval deltaTime;

	gettimeofday(&currentTime, NULL);
	timersub(&currentTime, &last_announce, &deltaTime);

	APPEND_MSG("\tlast_announce="<<timevalToString(deltaTime)<<" ago"<<endl);
	APPEND_MSG("\tmissed_announcements="<<(int)missed_announcements<<endl);
	APPEND_MSG("\taddress_list="<<addressListToString(secondaryAddresses)<<endl);
	APPEND_MSG("\ttrunk_list="<<addressListToString(trunkList)<<endl);

}

string link_info::getName(){
	stringstream sstr;

	sstr<<"[";

	if(socket!=NULL){
		sstr<<socket->getName()<<" ";
	}

	sstr<<address.toString();

	if(hostname.size() > 0){
		sstr<<" "<<hostname;
	}

	sstr<<"]";
	return sstr.str();
}

/*bool vid_tree::insert(vid_t vid, link_info* link){
	if(vid.size()<1 || link->socket==NULL){
		return false;
	}

	map<uint8_t, vid_treeNode*>::iterator nodeIter=roots.find(vid[0]);
	vid_treeNode* node=NULL;
	if(nodeIter==roots.end()){
		node=new vid_treeNode;
		node->link=NULL;
		node->parent=NULL;
		node->digit=vid[0];
		node->info=NULL;
		roots.insert(make_pair(vid[0], node));
	}
	else{
		node=nodeIter->second;
	}


	for(unsigned int i=1; i<vid.size(); i++){
		nodeIter=node->children.find(vid[i]);
		if(nodeIter==node->children.end()){
			vid_treeNode* parent=node;
			node=new vid_treeNode;
			node->link=NULL;
			node->parent=parent;
			node->digit=vid[i];
			node->info=NULL;
			parent->children.insert(make_pair(vid[i], node));
		}
		else{
			node=nodeIter->second;
		}
	}
	node->link=link;
	return true;
}*/

/*
vid_treeNode* vid_tree::create(uint8_t* vidBuffer, uint8_t size){
	if(size<=0){
		return NULL;
	}

	vid_treeNode* node=NULL;
	map<uint8_t, vid_treeNode*>::iterator nodeIter=roots.find(vidBuffer[0]);

	if(nodeIter==roots.end()){
		node=new vid_treeNode;
		node->parent=NULL;
		node->digit=vidBuffer[0];
		node->depth = 0;
		node->info=NULL;
		roots.insert(make_pair(vidBuffer[0], node));
	}
	else{
		node=nodeIter->second;
	}


	for(unsigned int i=1; i<size; i++){
		nodeIter=node->children.find(vidBuffer[i]);
		if(nodeIter==node->children.end()){
			vid_treeNode* parent=node;
			node=new vid_treeNode;
			node->parent=parent;
			node->digit=vidBuffer[i];
			node->depth=i;
			node->info=NULL;
			parent->children.insert(make_pair(vidBuffer[i], node));
		}
		else{
			node=nodeIter->second;
		}
	}

	return node;
}

void vid_tree::remove(vid_t vid){
	removeNode(getNode(vid));
}

void vid_tree::removeNode(vid_treeNode* node){
	if(node!=NULL){

		if(node->parent != NULL){
			//Remove node from parent
			node->parent->children.erase(node->digit);
		}

		delete node;
	}
}

vid_treeNode* vid_tree::getNode(vid_t vid){
	if(vid.size()<1){
		return NULL;
	}

	map<uint8_t, vid_treeNode*>::iterator nodeIter=roots.find(vid[0]);
	vid_treeNode* node=NULL;
	if(nodeIter==roots.end()){
		return NULL;
	}
	else{
		node=nodeIter->second;
	}

	for(unsigned int i=1; i<vid.size(); i++){
		nodeIter=node->children.find(vid[i]);
		if(nodeIter==node->children.end()){
			return NULL;
		}
		node=nodeIter->second;
	}
	return node;
}

vid_treeNode* vid_tree::getNode(uint8_t* vidBuffer, uint8_t size){
	if(size <= 0 || vidBuffer==NULL){
		return NULL;
	}

	vid_treeNode* node=NULL;
	map<uint8_t, vid_treeNode*>::iterator nodeIter=roots.find(vidBuffer[0]);
	for(int i=0; i<size; i++){
		nodeIter=node->children.find(vidBuffer[i]);
		if(nodeIter==node->children.end()){
			return NULL;
		}
		node=nodeIter->second;
	}
	return node;
}



vid_treeNode* vid_tree::getNode(uint8_t* vidListPtr){
	vid_treeNode* node=NULL;

	uint8_t vids = vidListPtr[0];
}

vid_treeNode* vid_tree::nearestLink(vid_t vid){
	if(vid.size()<1){
		return NULL;
	}

	vid_treeNode* node = NULL;
	vid_treeNode* bestNode = NULL;
	int bestDistance = 127;
	int distance = 127;

	map<uint8_t, vid_treeNode*>::iterator nodeIter = roots.find(vid[0]);

	if(nodeIter==roots.end()){
		return NULL;
	}

	node = nodeIter->second;
	bestNode = node;
	bestDistance = vid.size() - node->depth;


	for(unsigned int i=1; i<vid.size(); i++){
		nodeIter=node->children.find(vid[i]);
		if(nodeIter==node->children.end()){
			break;
		}
		node=nodeIter->second;

		//Update bestlink if we are physically connected to this node
		if(node->info!=NULL){

			//Make sure distance is always positive
			if(node->depth > vid.size()){
				distance = node->depth - vid.size();
			}
			else{
				distance = vid.size() - node->depth;
			}

			if(distance < bestDistance){
				bestNode = node;
			}
		}
	}
	return bestNode;
}
*/

si_node::si_node(si_address address, bool announceEA){
	consumeCallback=NULL;
	newLinkCallback=NULL;
	deadLinkCallback=NULL;
	usingAltPathCallback=NULL;
	addressChangeCallback=NULL;
	orphanedCallback=NULL;
	missedHelloCallback=NULL;
	clusterHeadCallback=NULL;

	rawLocalAddress = raw_address();
	localAddressBuffer=NULL;
	localAddressSize=0;
	stopped=true;
	terminated=false;
	_stack=NULL;
	_tid=-1;
	avgTime=0;
	avgRate=0;
	announceEnabled=announceEA;
	totalPackets=0;
	totalBytes=0;
	mmt_enabled=false;
	orphaned=false;

	local_hostname = getNodeName();
	DEBUG_MSG("Hostname="<<local_hostname<<" hostname_len="<<local_hostname.size()<<"bytes"<<endl);

	//Clear routing buffer
	routeBuf.clear();

	//Clear SI annoucement variables
	announcePkt.clear();
	announceSeq = 0;
	announcePayload = NULL;
	announcedBytes = 0;

	//Clear MMT annoucement variables
	announceMMTPkt.clear();
	announceMMTPayload = NULL;
	announcedMMTBytes = 0;

	setLocalAddress(address);

	INFO("HALF_ANNOUNCE_PERIOD_SEC=" << HALF_ANNOUNCE_PERIOD_SEC<<endl);
	INFO("HALF_ANNOUNCE_PERIOD_uSEC=" << HALF_ANNOUNCE_PERIOD_uSEC<<endl);

	InitializeRoutingStats(&downMapTable);
	InitializeRoutingStats(&upMapTable);
	InitializeRoutingStats(&trunkMapTable);
}

si_node::si_node(string configPath){
	//
}

//Clean up all the data in the nodes
si_node::~si_node(){

	//Disable periodic service
	periodic_service* service=periodic_service::instance();
	service->pause();

	if(!stop(true, 100)){
		DEBUG_MSG("force killing child!"<<endl);
		kill(_tid, SIGKILL);
	}
	DEBUG_MSG("packet count"<<userPackets.size()<<endl);
	while(!userPackets.empty()){
		if(userPackets.back()!=NULL){
			delete userPackets.back();
		}
		userPackets.pop_back();
	}

	int interfaceCount=sockets.size();
	while(!links.empty()){
		if(links.back() != NULL){
			delete links.back();
		}
		links.pop_back();
	}

	//Clean up target announcements
	DEBUG_MSG("Has " << announceTargets.size() << " targeted announcements" << endl);
	while(!announceTargets.empty()){
		delete announceTargets.begin()->second;
		announceTargets.erase(announceTargets.begin());
	}

	INFO("Routed "<<totalPackets<<" totalPackets containing "<<byteCountToString(totalBytes)<<" on "<<interfaceCount<<" interfaces"<<endl<<endl);

	while(!sockets.empty()){
		si_socket* socket = sockets.back();
		socket_stats sockStats = socket->getStats();

		INFO(socket->getName()<<endl);
		INFO("\tRX packets: "<<sockStats.rxCount<<" errors: "<<sockStats.rxFailCount<<" bytes: "<<sockStats.rxBytes<<endl);
		INFO("\tTX packets: "<<sockStats.txCount<<" errors: "<<sockStats.txFailCount<<" bytes: "<<sockStats.txBytes<<endl);


		if(sockets.back()!=NULL){
			delete sockets.back();
		}
		sockets.pop_back();
	}

	delete service;

	if(_stack!=NULL){
		free(_stack);
		_stack=NULL;
	}

	/*if(localAddressBuffer != NULL){
		delete[] localAddressBuffer;
	}*/
}


void si_node::setAnnounceEnable(bool announceEA){
	announceEnabled = announceEA;
}



bool si_node::isAnnounceEnabled() const {
	return announceEnabled;
}


bool si_node::addInterface(string ifname) {
	if(hasInterface(ifname)){
		return true;
	}
	si_socket* s = new si_socket(ifname);
	if (!s->isOpen()) {
		ERROR("si_socket["<<ifname<<"] is not open!"<<endl);
		delete s;
		return false;
	}

	DEBUG_MSG("si_socket allocated at 0x" << (uint64_t)s << " needing " << sizeof(si_socket) << "bytes" << endl );

	periodic_service* service=periodic_service::instance();
	if(!stopped){
		//Disable periodic service to prevent race conditions
		service->pause();
	}

	MutexLock(&this->socketMutex);
	sockets.push_back(s);

	if(!stopped){
		//Enable periodic service
		service->resume();
	}

	return true;
}

bool si_node::hasInterface(string ifname){
	if(getInterface(ifname)!=NULL){
		return true;
	}
	return false;
}


bool si_node::addAllInterfaces(list<string> exclude){
	bool success=true;
	list<string> ifs = getExperimentIFNames();

	list<string>::iterator iter = exclude.begin();
	for(; iter != exclude.end(); iter++){
		ifs.remove(*iter);
	}

	iter = ifs.begin();
	for(; iter != ifs.end(); iter++){
		success &= addInterface(*iter);
	}

	return success;
}


bool si_node::removeInterface(string ifname){
	si_socket* sock=getInterface(ifname);
	if(sock!=NULL){
		//Disable periodic service to prevent race conditions
		periodic_service* service=periodic_service::instance();
		service->pause();

		MutexLock(&this->socketMapMutex);
		MutexLock(&this->linkMutex);

		multimap<si_socket*, link_info*>::iterator beginIter = socketMap.lower_bound(sock);
		multimap<si_socket*, link_info*>::iterator endIter = socketMap.upper_bound(sock);


		for(; beginIter!=endIter; beginIter++){
			deleteLink(beginIter->second);
		}

		beginIter = socketMap.lower_bound(sock);
		socketMap.erase(beginIter, endIter);

		MutexLock(&this->socketMutex);
		vector<si_socket*>::iterator socketIter=find(sockets.begin(), sockets.end(), sock);
		if(socketIter!=sockets.end()){
			sockets.erase(socketIter);
		}

		delete sock;

		rebuildAnnouncement();

		service->resume();

		return true;
	}
	return false;
}


void si_node::setLocalAddress(si_address addr){
	periodic_service* service=periodic_service::instance();
	bool pauseNeeded = !service->isPaused() && !service->isActive();
	if(!stopped && pauseNeeded){
		//Disable periodic service to prevent race conditions
		service->pause();
	}

	if(addressChangeCallback != NULL){
		(*addressChangeCallback)(addr, local_address);
	}

	local_address=addr;
	uint8_t* old_address=NULL;
	if(localAddressBuffer != NULL){
		old_address = new uint8_t[localAddressSize];
		memcpy(old_address, localAddressBuffer, localAddressSize);
	}

	#if ADDRESS_CHANGE_DEBUG
	DEBUG_MSG("Address["<< local_address.toString() <<"] will use "<<local_address.byteCount()<<"bytes"<<endl);
	#endif

	rawLocalAddress = local_address;
	localAddressBuffer = rawLocalAddress.buffer;
	localAddressSize = rawLocalAddress.bytes;

	DEBUG_MSG("rawLocalAddress=" << rawLocalAddress.toString() << " localAddressBuffer=" << raw_address::toString(localAddressBuffer) << endl);

	//localAddressBuffer=new uint8_t[local_address.byteCount()];
	//local_address.writeTo(localAddressBuffer, local_address.byteCount());
	//localAddressSize=raw_address::byteCount(localAddressBuffer);
	//local_bc=local_address.bc;

	cout.unsetf(ios::dec);
	cout.setf(ios::hex);

	#if ADDRESS_CHANGE_DEBUG
	DEBUG_MSG("RAW adress uses "<<localAddressSize<<" bytes at location = 0x"<< ((uint64_t)localAddressBuffer) <<endl);
	#endif

	cout.unsetf(ios::hex);
	cout.setf(ios::dec);

	#if ADDRESS_CHANGE_DEBUG
	DEBUG_MSG("Have " << downMap.size() << " downlinks needing new addresses" << endl);
	#endif

	if(links.size() != 0){
		#if ADDRESS_CHANGE_DEBUG
		DEBUG_MSG("Reconstructing routing tables..."<<endl);
		#endif
		vector<link_info*>::iterator linkIter;

		for(linkIter=links.begin(); linkIter != links.end(); linkIter++){

			if((*linkIter)->type == DOWNLINK){
				//Force children to change address
				link_info* downLink = *linkIter;

				//Get last address field
				address_chunk chunk = downLink->address.chunks.at(downLink->address.chunks.size() - 2);

				raw_address newAddr(local_address);
				newAddr.tierLevel++;
				newAddr.chunks.insert(newAddr.chunks.end()-1, chunk);
				newAddr.updateBuffer();

				#if ADDRESS_CHANGE_DEBUG
				DEBUG_MSG("Assigning host " << downLink->getName() << " address=[" << newAddr.toString() << "]" << endl);
				#endif

				//Update routing table
				if(!updateLink(newAddr, downLink)){
					ERROR("Failed to update down link"<<downLink->getName()<<"!"<<endl);
				}

				//Build address assignment packet
				raw_packet packet;
				uint8_t* payloadPtr = buildCMPPacket(&packet,
													(uint8_t*) raw_linkLocal,
													old_address,
													MK_CMP_TYPE(CMP_Config,CONF_SetSIAddr),
													RAND_SEQ);

				packet.appendData(NULL, newAddr.countBytes());

				newAddr.writeAddress(&payloadPtr[4]);

				#if ADDRESS_CHANGE_DEBUG
				DEBUG_MSG("Written address = " << raw_address::toString(&payloadPtr[4], true) << endl);
				#endif

				downLink->socket->send(packet);
				continue;
			}

			if(!updateLink((*linkIter)->address, *linkIter)){
				ERROR("Failed to update link!"<<endl);
			}
		}
	}

	//Construct announce packet
	rebuildAnnouncement();

	announce(NULL, NULL);

	if(old_address != NULL){
		delete old_address;
	}

	if(!stopped && pauseNeeded){
		//Enable periodic service
		service->resume();
	}
}

si_address si_node::getLocalAddress() const {
	return local_address;
}

void si_node::setMMTEnable(bool state){
	mmt_enabled=state;
	vector<link_info*>::iterator linkIter;

	MutexLock(&this->linkMutex);
	for(linkIter=links.begin(); linkIter!=links.end(); linkIter++){
		(*linkIter)->mmt_enabled = state;
	}
}

bool si_node::getMMTEnabled(){
	return mmt_enabled;
}

void si_node::setMMTUid(uint16_t uid){
	mmtInfo.uid=uid;
}

uint16_t si_node::getMMTUid() const{
	return mmtInfo.uid;
}

void si_node::addVid(vid_t newVid){
	mmtInfo.vidList.push_back(newVid);
}

void si_node::setVidList(vector<vid_t> vidList){
	mmtInfo.vidList=vidList;
}

vector<vid_t> si_node::getVidList() const{
	return mmtInfo.vidList;
}

void si_node::clearVidList(){
	mmtInfo.vidList.clear();
}

uint8_t si_node::getMMTNodeType() const{
	return mmtInfo.mmtType;
}

const mmt_info* si_node::getMMTInfo() const{
	return &mmtInfo;
}

bool si_node::isOrphaned() const {
	return orphaned;
}

void si_node::setOrphanState(bool state){
	orphaned = state;
}

si_socket* si_node::getInterface(string ifname) {
	MutexLock(&this->socketMutex);
	for(uint32_t i=0; i<sockets.size(); i++){
		si_socket* s=sockets.at(i);
		if(s!=NULL){
			if(s->getName() == ifname){
				return s;
			}
		}
	}
	return NULL;
}

vector<link_info*> si_node::getLinkInfo(string ifname){
	vector<link_info*> retList;

	si_socket* sock=getInterface(ifname);
	if(sock != NULL){
		MutexLock(&this->socketMapMutex);
		multimap<si_socket*, link_info*>::iterator start = socketMap.lower_bound(sock);
		multimap<si_socket*, link_info*>::iterator end = socketMap.upper_bound(sock);

		for(; start!=end; start++){
			retList.push_back(start->second);
		}
	}

	return retList;
}


link_info* si_node::getLinkInfo(si_socket* socket, raw_address* address) const {
	multimap<si_socket*, link_info*>::const_iterator start = socketMap.lower_bound(socket);
	multimap<si_socket*, link_info*>::const_iterator end = socketMap.upper_bound(socket);

	for(; start!=end; start++){
		link_info* link = start->second;
		if(link != NULL && link->address == *address){
			return link;
		}
	}

	return NULL;
}

vector<link_info*> si_node::getAllLinks() const {
	return links;
}

vector<link_info*> si_node::getLinksByType(uint8_t type) const {
	vector<link_info*> linkVect;

	for(uint i=0; i<links.size(); i++){
		if(links[i]->type == type){
			linkVect.push_back(links[i]);
		}
	}

	return linkVect;
}

void si_node::injectPacket(si_packet* packet){
	MutexLock(&this->packetMutex);
	userPackets.push_back(packet);
}

void si_node::broadcast(raw_packet* packet){
	if(packet==NULL){
		packet=&announcePkt;
	}

	MutexLock(&this->socketMutex);
	if(packet->readPacketType()==PROTO_MMT){
		if(!mmt_enabled){
			//Only broadcast an MMT packet if we have mmt_enabled
			return;
		}

		map<si_socket*, link_info*>::iterator linkIter;
		for(linkIter=internalLinks.begin(); linkIter!=internalLinks.end(); linkIter++){
			//Only transmit to internalLinks with mmt enabled
			if(linkIter->second->socket!=NULL && linkIter->second->mmt_enabled){
				if(packet->rx_socket != linkIter->second->socket){
					sendPacket(*packet, *linkIter->second->socket);
				}
			}
		}
		return;
	}

	removeDeadLinks();

	vector<link_info*>::iterator linkIter;
	for(linkIter=links.begin(); linkIter!=links.end(); linkIter++){
		if((*linkIter)->socket!=NULL){
			if(packet->rx_socket != (*linkIter)->socket){
				sendPacket(*packet, *(*linkIter)->socket);
			}
		}
	}

}


uint32_t si_node::neighborWait(int msTimeout){
	if(stopped){
		terminated = false;
		Timer timeout(true);
		bool foundAll=true;

		while(Timer::timevalToMs(timeout.getElapsedReal()) < msTimeout && links.size() != sockets.size() && !terminated){
			runOnce();
			foundAll = sockets.size() == links.size();

			if(foundAll){
				DEBUG_MSG("Waited for " << timeout.toString() << endl);

				terminated = true;
				stopped = true;
				return links.size();
			}
		}

		terminated = true;
		stopped = true;
		DEBUG_MSG("Waited for " << timeout.toString() << endl);
	}
	else{
		ERROR("Routing already started, cannot wait for neighbors!" << endl);
	}
	return 0;
}

vector<si_socket*> si_node::getReadySockets(int msTimeout){
	vector<si_socket*> readySockets;
	fd_set socket_fds;
	int fds_max=0;


	FD_ZERO(&socket_fds);
	socketMutex.enter();
	for(uint32_t i=0; i<sockets.size(); i++){
		if(sockets.at(i)==NULL){
			continue;
		}

#if READY_LINKS_DEBUG
		DEBUG_MSG("Adding "<<sockets.at(i)->getName()<<endl);
#endif
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
	socketMutex.leave();

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
				return getReadySockets(msTimeout);
			}
		}
		else{
			ERROR("select returned "<<retVal<<endl);
			perror("select");
		}
	}
	else{
		//Create list of sockets ready for reading
		socketMutex.enter();
		for(uint32_t i=0; i<sockets.size(); i++){
			if(sockets.at(i)==NULL){
				continue;
			}
			if(FD_ISSET(sockets.at(i)->sock, &socket_fds)){
#if READY_LINKS_DEBUG
				DEBUG_MSG("Pushing socket at index "<<i<<endl);
#endif
				readySockets.push_back(sockets.at(i));
			}
		}
		socketMutex.leave();
	}
	return readySockets;
}

vector<si_packet*> si_node::readPackets(vector<link_info*> readFrom, bool blocking){
	vector<si_packet*> packets;

	packetMutex.enter();
	if(userPackets.size() > 0){
		packets.assign(userPackets.begin(), userPackets.end());
		userPackets.clear();
	}
	packetMutex.leave();

	Timer elapsed(true);
	MutexLock(&this->linkMutex);
	for(uint32_t i=0; i<readFrom.size() && Timer::timevalToMs(elapsed.getElapsedReal()) < MAX_READTIME; i++){
		link_info* link=readFrom.at(i);
		if(link==NULL){
			continue;
		}
		else if(link->socket==NULL){

			continue;
		}

		for(int i=0; i<MAX_PACKETS && Timer::timevalToMs(elapsed.getElapsedReal()) < MAX_READTIME; i++){
			//TODO: Update the following to use raw_packet and the safer recvPacket() function
			si_packet* packet = link->socket->recv(blocking);
			if(packet == NULL){
				break;
			}
			if(gettimeofday(&link->last_seen, NULL) != 0){
				perror("gettimeofday");
			}
			packets.push_back(packet);
			link->rx_count++;
			packet->timer.stop();

			PACKET_ACTION(packet, "RECV - Read from link ["+link->socket->getName()+"]");
		}
	}

	return packets;
}

void si_node::processPackets(vector<si_socket*> readFrom, bool blocking){
	packetMutex.enter();
	while(userPackets.size() > 0){
		si_packet& packetPtr=*userPackets.back();

		//TODO: Dumb dumb dumb, DONT DO THIS, JUST DO THIS routePacket(userPackets.back())
		//		This prevents an unneeded buffer copy
		routeBuf=packetPtr;
		//routeBuf.rx_link=NULL;
		if(!routePacket()){
			//routeTime.stop();
			droppedBytes+=routeBuf.length;
			droppedPackets++;
			//INFO("DROPPED Packet readTimer="<<readTime.toString()<<" routeTimer="<<routeTime.toString()<<endl);
		}
		else{
			//routeTime.stop();

			//INFO("ROUTED Packet readTimer="<<readTime.toString()<<" routeTimer="<<routeTime.toString()<<endl);
		}

		byteCount+=routeBuf.length;
		packetCount++;
		routeBuf.length=0;
		routeBuf.offset=0;

		delete userPackets.back();
		userPackets.pop_back();
	}
	packetMutex.leave();

	#if ROUTE_DEBUG
	if(readFrom.size()==0){
		WARNING("Passed zero ready links"<<endl);
	}
	else{
		//WARNING("Passed "<<readFrom.size()<<" ready links"<<endl);
	}
	#endif

	#if PRINT_PER_PACKET_ROUTE_RATE
	Timer routeTime(false);
	Timer readTime(false);
	#endif

	Timer elapsed(true);
	MutexLock(&this->socketMutex);

	int maxLinkTime = fmin(MAX_READTIME/readFrom.size(), MAX_LINK_TIME);

	for(uint32_t i=0; i<readFrom.size() && Timer::timevalToMs(elapsed.getElapsedReal()) < MAX_READTIME; i++){
		si_socket* socket=readFrom.at(i);
		if(socket==NULL){
			WARNING("Null socket at index"<<i<<endl);
			continue;
		}

		#if ROUTE_DEBUG
		INFO("Socket="<<socket->getName()<<endl);
		#endif


		for(int j=0; j<MAX_PACKETS && Timer::timevalToMs(elapsed.getElapsedReal()) < maxLinkTime; j++){
			#if PRINT_PER_PACKET_ROUTE_RATE
			routeTime.reset();
			readTime.reset();

			readTime.start();
			#endif

			if(recvPacket(routeBuf, *socket, blocking)!=0){
				#if PRINT_PER_PACKET_ROUTE_RATE
				readTime.stop();
				//INFO("DROPPED-READFAIL Packet readTimer="<<readTime.toString()<<" routeTimer="<<routeTime.toString()<<endl);
				#endif
				break;
			}

			#if PRINT_PER_PACKET_ROUTE_RATE
			readTime.stop();
			routeTime.start();
			#endif

			if(routeBuf.length<=0){
				#if PRINT_PER_PACKET_ROUTE_RATE
				routeTime.stop();
				#endif
				break;
			}

			if(!routePacket()){				
				droppedBytes+=routeBuf.length;
				droppedPackets++;

				#if PRINT_PER_PACKET_ROUTE_RATE
				routeTime.stop();
				INFO("DROPPED Packet readTimer="<<readTime.toString()<<" routeTimer="<<routeTime.toString()<<endl);
				#endif
			}
			else{
				#if PRINT_PER_PACKET_ROUTE_RATE
				routeTime.stop();
				INFO("ROUTED Packet readTimer="<<readTime.toString()<<" routeTimer="<<routeTime.toString()<<endl);
				#endif
			}

			byteCount+=routeBuf.length;
			packetCount++;
			routeBuf.length=0;
			routeBuf.offset=0;
		}
		//gettimeofday(&link->last_seen, NULL);
	}
}

bool si_node::start(){
	if(stopped){
		terminated=false;
		DEBUG_MSG("Starting run loop"<<endl);

		if(_stack==NULL){
			_stack = (uint8_t*)malloc(STACK_SIZE);

			cout.unsetf(ios::dec);
			cout.setf(ios::hex);
			DEBUG_MSG("Stack allocated at 0x" << (uint64_t)_stack << " using "<<STACK_SIZE<<"bytes"<<endl);
			cout.unsetf(ios::hex);
			cout.setf(ios::dec);
		}

		gettimeofday(&start_Routing_Time, NULL);
		// create a child process
		_tid = clone(si_node::run, &_stack[STACK_SIZE], CLONE_FS|CLONE_PTRACE|CLONE_VM|SIGCHLD, (void*)this);


		stopped=false;
		DEBUG_MSG("pid="<<getpid()<<" child tid="<<_tid<<endl);
		if(_tid==-1){
			stopped=true;
			perror("clone");
			return false;
		}
	}
	else{
		DEBUG_MSG("Run loop already started"<<endl);
	}
	return true;
}

bool si_node::isRunning(){
	return !stopped;
}

bool si_node::stop(bool blocking, int msTimeout){
	WARNING("stopping node blocking="<<blocking<<endl);

	terminated=true;

	if(!stopped){
		if(_stack!=NULL){
			sigset_t sigs;
			sigemptyset(&sigs);
			sigaddset(&sigs, SIGCHLD);
			sigprocmask(SIG_BLOCK, &sigs, NULL);
			if(blocking){
				if(msTimeout > 0){
					//cout<<"timed blocking"<<endl;
					timespec timeout;
					timeout.tv_sec=msTimeout/1000;
					timeout.tv_nsec=(msTimeout-timeout.tv_sec*1000)*1000*1000;
					if(SIGCHLD!=sigtimedwait(&sigs, NULL, &timeout)){
						WARNING("child has not exited yet"<<endl);
						return false;
					}
				}
				else{
					//cout<<"indefinite blocking"<<endl;
					sigwaitinfo(&sigs, NULL);
				}
			}
			else{
				//cout<<"polled"<<endl);
				timespec timeout;
				timeout.tv_sec=0;
				timeout.tv_nsec=0;
				if(SIGCHLD!=sigtimedwait(&sigs, NULL, &timeout)){
					WARNING("child has not exited yet"<<endl);
					return false;
				}
			}
		}
	}
	return true;
}

void si_node::runOnce(){
	packetCount=0;
	byteCount=0;
	runloop.reset();
	runloop.start();
	vector<si_socket*> readySockets = getReadySockets();
	if(readySockets.size() > 0 || userPackets.size() > 0){
		#if READY_LINKS_DEBUG
		DEBUG_MSG("Have "<<readySockets.size()<<" links ready for reading and "<<userPackets.size()<<" user injected packets"<<endl);
		#endif
		processPackets(readySockets, false);
	}
	runloop.stop();
	totalPackets+=packetCount;
	totalBytes+=byteCount;
}

int si_node::run(void* target){
	if(target!=NULL){
		si_node* node=(si_node*)target;

		DEBUG_MSG("pid="<<getpid()<<" tid="<<(long int)syscall(__NR_gettid)<<" hostname="<<node->local_hostname<<endl);

		//Set up FCT announcements
		periodic* p = new periodic(node, (periodic_t_func)&si_node::announce, NULL, (periodic_t_func)&si_node::announceCleanup);
		p->install((timeval){ANNOUNCE_PERIOD_SEC, ANNOUNCE_PERIOD_uSEC}, (timeval){0,0}, "announceFCT");

		#if BUILD_MMT
		//Set up MMT announcements
		p = new periodic(node, (periodic_t_func)&si_node::announceMMT, NULL, (periodic_t_func)&si_node::announceMMT);
		p->install((timeval){MMT_ANNOUNCE_PERIOD_SEC, MMT_ANNOUNCE_PERIOD_uSEC}, (timeval){0,0}, "announceMMT");
		#endif

		/*struct sigaction killedAction;
		killedAction.sa_handler = *killReport;
		sigemptyset(&killedAction.sa_mask);
		killedAction.sa_flags = 0;
		sigaction(SIGKILL, &killedAction, NULL);*/

		WARNING("running="<<!node->stopped<<endl);

		//Change scheduling policy for this thread to high priority FIFO
		struct sched_param sp;
		sp.__sched_priority=50;
		sched_setscheduler(0, SCHED_FIFO, &sp);

		Timer reportTimer(true);

		while(!node->terminated){
			node->runOnce();

			//Print statistics everytime packets were processed
			if(node->packetCount > 0 || Timer::timevalToMs(reportTimer.getElapsedReal()) > MIN_REPORT_INTERVAL_MS){

				#if PRINT_CONVERGANCE
				//  cout<<"CHECKING NODE CONVERGANCE AFTER NODE FAILEURE"<<node->upMapTable.convergance<<endl;
				if((node->reportInitialConvergance(&node->downMapTable)==false)||(node->downMapTable.convergance==false))
				{
					//   INFO("Checking Initial code convergance \t "<<endl);
					node->checkInitialConvergence("Down Links ROUTING TABLE",&node->downMapTable,&node->downMap);
				}
				// cout<<"CHECKING NODE CONVERGANCE AFTER NODE FAILEURE"<<node->downMapTable.convergance<<endl;
				if((node->reportInitialConvergance(&node->upMapTable)==false)||(node->upMapTable.convergance==false))
				{
					//   INFO("Checking Initial code convergance \t "<<endl);
					node->checkInitialConvergence("UpLinks Routing Table",&node->upMapTable,&node->upMap);
				}
				//cout<<"CHECKING NODE CONVERGANCE AFTER NODE FAILEURE"<<node->trunkMapTable.convergance<<endl;
				if((node->reportInitialConvergance(&node->trunkMapTable)==false)||(node->trunkMapTable.convergance==false))
				{
					// INFO("Checking Initial code convergance \t "<<endl);
					node->checkInitialConvergence("Trunk Links Routing Table",&node->trunkMapTable,&node->trunkMap);
				}
				#endif

				#if PRINT_ROUTE_RATE
				node->reportRouteRate();
				#endif

				#if PRINT_ROUTING_TABLE
				node->mmtInfo.printInfo();
				node->printLinkState();
				node->printRoutingMaps();
				#endif

				reportTimer.restart();
			}
		}
		node->stopped=true;
		DEBUG_MSG("stopped pid="<<getpid()<<" tid="<<(long int)syscall(__NR_gettid)<<endl);

		periodic_service::instance()->pause();
		return 0;
	}
	else{
		ERROR("pid="<<getpid()<<" tid="<<(long int)syscall(__NR_gettid)<<" Passed invalid si_node pointer!"<<endl);
		ABORT_NOW();
	}

	return -1;
}

bool si_node::routePacket(raw_packet *packet){
	if(packet==NULL){
		//Use static packet buffer
		packet=&routeBuf;
	}

	#if ROUTE_DEBUG
	DEBUG_MSG("Dest: ");
	raw_address::printAddress(packet->destinationAddrPtr());
	APPEND_MSG("\t Src: ");
	raw_address::printAddress(packet->senderAddrPtr());
	APPEND_MSG(endl);
	#endif

	// Read what kind of packet we are routing
	uint16_t proto=packet->readPacketType();
	if(proto != PROTO_SI_DATA){
		//TODO: Why is this not commented out?
		if(proto==PROTO_SI_CMP){
			uint8_t* payloadPtr=packet->payloadPtr();
			uint8_t cmp_type=(payloadPtr[0]&0xf0)>>4;

			if(cmp_type == CMP_Trace){

			}
		}
		#if BUILD_MMT
		else if(proto==PROTO_MMT && packet->rx_socket!=NULL){
			map<si_socket*,link_info*>::iterator internalIter = internalLinks.find(packet->rx_socket);

			// Only route MMT packets if the interface is internal and MMT is enabled on that link
			if(internalIter != internalLinks.end() && internalIter->second->mmt_enabled == true){
				return routeMMT(packet);
			}
			else{
				//DROP, MMT packet was from external link or MMT is not enabled on link
				WARNING("MMT-DROP INACTIVE INTERFACE("<<packet->rx_socket->getName()<<")"<<endl);
				return false;
			}
		}
		#endif
	}

	uint8_t* destAddr=packet->destinationAddrPtr();
	uint8_t destBC=raw_address::readBC(destAddr);
	int32_t deltaBC = destBC - local_address.bc;
	int32_t destFirstChunk=0;
	int32_t destSecondChunk=0;
	bool processed=false;

	if(destBC == RESERVED_BC){	//Reserved BC for linklocal and broadcast address
		if(raw_address::compareAddress(destAddr, (uint8_t*) raw_linkLocal)){
			processed=true;
			#if ROUTE_DEBUG
			DEBUG_MSG("CONSUME LINKLOCAL"<<endl);
			#endif
			consumePacket(packet);
		}
		else if(raw_address::compareAddress(destAddr, (uint8_t*) raw_broadcast)){
			processed=true;
			consumePacket(packet);
			#if ROUTE_DEBUG
			DEBUG_MSG("CONSUME BROADCAST"<<endl);

			DEBUG_MSG("FORWARDING BROADCAST, From socket="<<packet->rx_socket->getName());
			#endif
			//Forward to all open sockets
			broadcast(packet);
		}
		else{
			ERROR("DROP INVALID, BC==0x3f but not sent to linklocal or broadcast"<<endl);
			#if ROUTE_DEBUG
			DEBUG_MSG(packetToString(packet)<<endl);
			#endif
		}
	}
	else if(deltaBC==0){	//Forward to a DOWNLINK
		destFirstChunk=raw_address::readChunk(destAddr);

		if(destFirstChunk==-1){
			//CONSUME PACKET
			processed=true;
			consumePacket(packet);
			#if ROUTE_DEBUG
			DEBUG_MSG("CONSUMED"<<endl);
			#endif
		}
		else{
			map<uint16_t,link_info*>::iterator linkIter=downMap.find(destFirstChunk);
			if(linkIter!=downMap.end()){
				raw_address::incrementTier(destAddr);
				packet->removeFirstDestAF();
				if(linkIter->second->socket!=NULL && linkIter->second->socket != packet->rx_socket){
					if(sendPacket(*packet, *(linkIter->second)->socket) == packet->length){
						#if ROUTE_DEBUG
						DEBUG_MSG("SEND DOWNLINK"<<endl);
						#endif
						processed=true;
					}
					else{
						#if ROUTE_DEBUG
						DEBUG_MSG("SEND DOWNLINK FAILED"<<endl);
						#endif
					}
				}
			}
			else{
				#if ROUTE_DEBUG
				DEBUG_MSG("NO DOWNLINK WITH SAME 1st AF"<<endl);
				#endif

				processed=mmtForward(packet);
			}

			if(!processed){
				#if ROUTE_DEBUG
				packet->printPacket();
				cout<<endl;
				DEBUG_MSG("DOWNLINK FORWARD FAIL"<<endl);
				#endif
			}
		}
	}
	else if(deltaBC < 0){	//Forward to an UPLINK
		destFirstChunk=raw_address::readChunk(destAddr);

		//Are we a back bone node?
		if(destBC==0 && local_address.bc==1){
			//NOTE: We are a tier one router(ie. backbone) we
			//can only route down or to a suitable trunk

			if(destFirstChunk==local_address.chunks.front()){
				//Packet is addressed to this node or to a decendent

				destSecondChunk=raw_address::readChunk(destAddr,1);
				if(destSecondChunk!=-1){	//Contains a second address field
					//Send down
					map<uint16_t,link_info*>::iterator linkIter=downMap.find(destSecondChunk);
					if(linkIter!=downMap.end()){
						link_info* link = linkIter->second;

						//Make sure the link's socket is valid
						if(link->socket!=NULL && link->socket!=packet->rx_socket){

							if(!link->direct){
								//Transform address
								raw_address::setTier(destAddr, 1);
								#if ROUTE_DEBUG
								DEBUG_MSG("ADDRESS TRANFORMED FOR INDIRECT HOST"<<endl);
								#endif
							}
							else{
								raw_address::setTier(destAddr, local_address.bc+1);
								packet->removeFirstDestAF();
							}

							packet->removeFirstDestAF();

							if(sendPacket(*packet, *(linkIter->second)->socket) == packet->length){
								#if ROUTE_DEBUG
								DEBUG_MSG("SEND DOWNLINK BC=LOCAL.BC-1"<<endl);
								/*cout<<endl;
								packet->printPacket();
								cout<<endl;*/
								#endif
								processed=true;
							}
							else{
								#if ROUTE_DEBUG
								DEBUG_MSG("SEND DOWNLINK FAILED"<<endl);
								#endif
							}
						}
					}
					else{
						#if ROUTE_DEBUG
						DEBUG_MSG("NO DOWNLINK WITH SAME 1st AF"<<endl);
						#endif

						processed=mmtForward(packet);
					}
				}
				else{
					//CONSUME PACKET
					processed=true;
					consumePacket(packet);
					#if ROUTE_DEBUG
					DEBUG_MSG("CONSUMED"<<endl);
					#endif
				}

				if(!processed){
					#if ROUTE_DEBUG
					packet->printPacket();
					cout<<endl;
					DEBUG_MSG("DOWNLINK FORWARD FAIL BC=LOCAL.BC-1"<<endl);
					#endif
				}
			}
			else{	//Trunk search

				map<uint16_t,link_info*>::iterator linkIter=trunkMap.find(destFirstChunk);
				if(linkIter!=trunkMap.end()){
					if(linkIter->second->socket!=NULL && linkIter->second->socket!=packet->rx_socket){
						if(sendPacket(*packet, *(linkIter->second)->socket) == packet->length){
							#if ROUTE_DEBUG
							DEBUG_MSG("SEND TRUNK BC=LOCAL.BC-1"<<endl);
							#endif
							processed=true;
						}
						else{
							#if ROUTE_DEBUG
							DEBUG_MSG("SEND TRUNK FAILED"<<endl);
							#endif
						}
					}
				}
				else{
					#if ROUTE_DEBUG
					DEBUG_MSG("NO TRUNK WITH SAME 1st AF"<<endl);
					#endif
				}

				if(!processed){
					#if ROUTE_DEBUG
					packet->printPacket();
					cout<<endl;
					DEBUG_MSG("TRUNK FORWARD FAIL"<<endl);
					#endif
				}
			}
		}
		else{	//Non-backbone routing


			map<uint16_t,link_info*>::iterator linkIter=trunkMap.find(destFirstChunk);
			if(deltaBC==-1 && linkIter!=trunkMap.end()){
				raw_address::incrementTier(destAddr);
				packet->removeFirstDestAF();
				if(linkIter->second->socket!=NULL && linkIter->second->socket!=packet->rx_socket){
					if(sendPacket(*packet, *(linkIter->second)->socket) == packet->length){
						#if ROUTE_DEBUG
						DEBUG_MSG("SEND TRUNK BC=LOCAL.BC-1"<<endl);
						#endif
						processed=true;
					}
					else{
						#if ROUTE_DEBUG
						DEBUG_MSG("SEND TRUNK FAILED"<<endl);
						#endif
					}
				}
			}
			else{
				#if ROUTE_DEBUG
				DEBUG_MSG("NO TRUNK WITH SAME 1st AF, TRYING UPLINKS"<<endl);
				#endif

				//Failed to find a trunk that we can forward to, search for Uplink instead

				map<uint16_t,link_info*>::iterator linkIter=upMap.find(destFirstChunk);
				if(linkIter!=upMap.end()){
					if(linkIter->second->socket!=NULL && linkIter->second->socket!=packet->rx_socket){
						if(sendPacket(*packet, *(linkIter->second)->socket) == packet->length){
							#if ROUTE_DEBUG
							DEBUG_MSG("SEND UP BC=LOCAL.BC-1"<<endl);
							#endif
							processed=true;
						}
						else{
							#if ROUTE_DEBUG
							DEBUG_MSG("SEND UP FAILED"<<endl);
							#endif
						}
					}
				}
				else if(upMap.size()>0){
					//Could not find best Uplink, send to first working uplink instead

					linkIter=upMap.begin();
					if(linkIter->second->socket!=NULL && linkIter->second->socket!=packet->rx_socket){
						if(sendPacket(*packet, *(linkIter->second)->socket) == packet->length){
							#if ROUTE_DEBUG
							DEBUG_MSG("SEND UP BC=LOCAL.BC-1"<<endl);
							#endif
							processed=true;
						}
						else{
							#if ROUTE_DEBUG
							DEBUG_MSG("SEND UP FAILED"<<endl);
							#endif
						}
					}
				}
				else{
					#if ROUTE_DEBUG
					DEBUG_MSG("DO NOT KNOW ANY UP LINKS"<<endl);
					#endif

					processed=mmtForward(packet);
				}
			}


			if(!processed){
				#if ROUTE_DEBUG
				packet->printPacket();
				cout<<endl;
				DEBUG_MSG("UPLINK/TRUNK FORWARD FAIL"<<endl);
				#endif
			}
		}
	}
	else{
		//Do NOTHING, DROP
		#if ROUTE_DEBUG
		DEBUG_MSG("DROP INVALID Address, BC greater than local BC"<<endl);
		#endif
	}

	return processed;
}

bool si_node::routeMMT(raw_packet* packet){
	#if ROUTE_MMT_DEBUG
	DEBUG_MSG("Routing packet rx'd on "<<packet->rx_socket->getName()<<endl);
	#endif

	uint16_t payloadLen = packet->readPayloadLength();
	uint8_t* ptr = packet->payloadPtr();

	raw_address address(packet->senderAddrPtr());
	link_info* rx_link = getLinkInfo(packet->rx_socket, &address);

	uint8_t action = ptr[payloadLen - 1];
	uint16_t uid = ntohs(*((uint16_t*) &ptr[payloadLen - 3]));
	uint16_t length = ntohs( *((uint16_t*) &ptr[payloadLen - MMT_HDR_LEN]));

	uint8_t* mmtPayloadPtr = &ptr[payloadLen - (MMT_HDR_LEN + length)];

	if(action == MMT_Advertise){
		mmt_info* info = mmt.getInfoByUid(uid);

		if(info == NULL){
			info = new mmt_info(uid);

			info->mmtType = mmtPayloadPtr[0];
			info->vidList = parseVidList(&mmtPayloadPtr[1]);

			if(mmt.insertNode(info) != true){
				ERROR("Failed to insert new node!!"<<endl);
			}

			DEBUG_MSG("Inserted new mmt node uid=" << info->uid << endl);
		}
		else{
			info->mmtType = mmtPayloadPtr[0];
			info->vidList = parseVidList(&mmtPayloadPtr[1]);
			if(mmt.updateNodeVids(info) != true){
				ERROR("Call to mmt.updateNodeVids failed!!"<<endl);
			}

			DEBUG_MSG("Detected known mmt node uid=" << info->uid << endl);
		}

		//TODO: Determine exact requirements for cluster joining
		//NOTE: The following is poorly defined and not as efficient as it could be, need a flowchart from Yoshi

		if(((mmtInfo.mmtType&MMT_ClusterHead) != 0 && mmtInfo.vids.size() < MAX_CLUSTER_HEAD_VIDS) ||
		   ((mmtInfo.mmtType&MMT_ClusterHead) == 0 && mmtInfo.vids.size() < MAX_GENERAL_NODE_VIDS)) {

			multimap<uint16_t, vid_treeNode*>::iterator chIter;
			multimap<uint16_t, vid_treeNode*>::iterator chIterStart = mmtInfo.clusterHeads.lower_bound(info->uid);
			multimap<uint16_t, vid_treeNode*>::iterator chIterEnd = mmtInfo.clusterHeads.upper_bound(info->uid);

			/*for(uint i=0; i<info->vidList.size(); i++){
				vid_treeNode* vidNode = info->vids[i];

				pair<uint16_t, vid_treeNode*> p = make_pair(vidNode->info->uid, vidNode);

				chIter = find(chIterStart, chIterEnd, p);


				if(chIter == chIterEnd){
					//Request vid from this vidNode
				}
			}*/
		}


	}
	else if(action == MMT_JoinRequest){
		//
	}

	//uint8_t* payloadPtr = packet->mmtPayloadPtr();
	//uint8_t packetType = packet->readMMTPacketType();
	//uint16_t payloadLength = packet->readMMTPayloadLength();


	/*if(packetType==MMTPKT_Advertise){
		#if ROUTE_MMT_DEBUG
		DEBUG_MSG("Advertisment");
		//packet->printPacket();
		#endif

		//	MMT Advertise payload layout
		//	[src_uid]	[node_type]	[vid_count]	{src_vids}
		//		2			1			1		vid_count


		vid_treeNode* treeNode=NULL;
		int offset = 4;

		//Check if we have seen this node yet
		for(int i=0; i<payloadPtr[3]; i++){
			uint8_t vidLength = payloadPtr[offset];
			offset++;

			#if ROUTE_MMT_DEBUG
			assert(offset < (4+payloadLength));
			#endif

			treeNode = vidTree.getNode(payloadPtr+offset, vidLength);

			if(treeNode != NULL){
				if(treeNode->info!=NULL){
					if(treeNode->info->uid == *((uint16_t*)payloadPtr)){
						break;
					}
				}
			}
		}*/

		/*if(treeNode->info == NULL){
			treeNode->info=new mmt_info(mmtHeaderPtr, packet->rx_link);
		}*/
	//}

	return true;
}

bool si_node::mmtForward(raw_packet* packet){
	WARNING("Not implemented"<<endl);
	return true;
}

int si_node::recvPacket(raw_packet& pkt, si_socket& socket, bool blocking){
	periodic_service::instance()->pause();

	int retVal = socket.recv(pkt, blocking);

	if(retVal < 0 && blocking){
		INFO("Failed to recv, got return value = " << retVal << endl);
	}

	periodic_service::instance()->resume();
	return retVal;
}

int si_node::sendPacket(raw_packet& pkt, si_socket& socket){
	periodic_service::instance()->pause();

	int retVal = socket.send(pkt);

	periodic_service::instance()->resume();
	return retVal;
}

void si_node::announce(void* arg, periodic* p){
	if(announceEnabled){

		if(announcePayload == NULL){
			rebuildAnnouncement();
		}

		//Update sequence number
		announceSeq++;
		CMP_SET_SEQ_NUM(announcePayload, announceSeq);

		#if ANNOUNCE_TX_DEBUG
		DEBUG_MSG(packetToString(&announcePkt)<<endl);
		#endif

		//Transmit broadcast announcement
		vector<si_socket*>::iterator sockIter;
		for(sockIter=sockets.begin(); sockIter!=sockets.end(); sockIter++){
			(*sockIter)->send(announcePkt);
		}

		announcedBytes += announcePkt.length * sockets.size();

		//Transmit targeted announcements
		map<link_info*, raw_packet*>::iterator targetIter = announceTargets.begin();
		for(; targetIter!=announceTargets.end(); targetIter++){
			link_info* link = targetIter->first;
			raw_packet* pkt = targetIter->second;

			if(link!=NULL && pkt!=NULL && link->socket!=NULL){
				if(link->direct){
					ERROR("Transmitting a targetted annoucement to ["
							<<link->socket->getName()<<":"
							<<link->hostname<<":"<<link->address.toString()
							<<"] which is now direct, something is probably wrong!"<<endl);
				}

				//Update sequence number
				uint8_t* pktPayload = pkt->payloadPtr();
				CMP_SET_SEQ_NUM(pktPayload, announceSeq);

				link->socket->send(*pkt);
				announcedBytes += pkt->length;

				#if ANNOUNCE_TX_DEBUG
				DEBUG_MSG("Targeted annoucement" << endl);
				DEBUG_MSG(packetToString(pkt)<<endl);
				#endif
			}
		}
	}
}

void si_node::announceCleanup(void* arg, periodic* p){
	if(announceEnabled && isRunning()){
		ERROR("Critical error, announcement periodic is being deleted while node is stil running!"<<endl);
		ABORT_NOW();
	}
}

void si_node::announceMMT(void* arg, periodic* p){
	if(announceEnabled && mmt_enabled){
		//Send annoucement packet

		if(announceMMTPayload == NULL){
			rebuildMMTAnnouncement();
		}

		#if MMT_ANNOUNCE_TX_DEBUG
		DEBUG_MSG(packetToString(&announceMMTPkt)<<endl);
		#endif

		uint32_t socketSends=0;
		map<si_socket*, link_info*>::iterator internalIter = internalLinks.begin();

		//Iterate over all links with the same address
		for(; internalIter != internalLinks.end(); internalIter++){
			link_info* link = internalIter->second;

			//Only transmit on direct links with MMT enabled
			if(link->direct && link->mmt_enabled){
				link->socket->send(announceMMTPkt);
				socketSends++;
			}
		}

		announcedMMTBytes += announceMMTPkt.length * socketSends;
	}
}

void si_node::announceMMTCleanup(void* arg, periodic* p){
	if(announceEnabled && isRunning() && mmt_enabled){
		ERROR("Critical error, MMT announcement periodic is being deleted while node is stil running!"<<endl);
		ABORT_NOW();
	}
}

void si_node::rebuildAnnouncement(){

	#if ANNOUNCE_TX_DEBUG
	DEBUG_MSG("begin"<<endl);
	#endif

	vector<raw_address*> trunkList;
	vector<link_info*>::iterator linkIter = links.begin();
	for(; linkIter != links.end(); linkIter++){
		link_info* link = *linkIter;

		if(link->direct){	//Direct link
			if( (link->type&TRUNK) != 0){
				trunkList.push_back(&link->address);
			}

			//Find and remove old targeted annoucement
			map<link_info*, raw_packet*>::iterator targetIter = announceTargets.find(link);
			if(targetIter != announceTargets.end()){
				#if ANNOUNCE_TX_DEBUG
				INFO("FOUND targeted annoucement to [" << link->hostname << ":" << link->address.toString()) << "]" <<endl;
				#endif

				//Delete targeted packet
				if(targetIter->second != NULL){
					delete targetIter->second;
				}

				announceTargets.erase(targetIter);
				#if ANNOUNCE_TX_DEBUG
				INFO("REMOVED target announcement to [" << link->hostname << ":" << link->address.toString()) << "]" <<endl;
				#endif
			}
		}
		else{	//Indirect link
			//Add a targeted annoucement if none exists for this link

			map<link_info*, raw_packet*>::iterator pktIter = announceTargets.find(link);

			//Is there an annoucement registered for this node?
			if(pktIter == announceTargets.end()){
				announceTargets.insert(make_pair(link, (raw_packet*)NULL));
				#if ANNOUNCE_TX_DEBUG
				INFO("ADDED targeted annoucement to [" << link->hostname << ":" << link->address.toString()) << "]" <<endl;
				#endif
			}
		}
	}

	if(buildADVPacket(&announcePkt, (uint8_t*)raw_linkLocal, localAddressBuffer, -1, addressList, trunkList, announceSeq)){
		announcePayload = announcePkt.payloadPtr();
		#if ANNOUNCE_TX_DEBUG
		DEBUG_MSG("Announce packet size = "<<announcePkt.length<<" bytes, contains "<<addressList.size()<<" addresses"<<endl);

		cout.unsetf(ios::dec);
		cout.setf(ios::hex);

		uint8_t* payloadPtr = announcePayload;


		uint64_t ptr = (uint64_t) &(*payloadPtr);
		DEBUG_MSG("pointer = 0x"<<ptr<<" payload[0]="<<(uint32_t)payloadPtr[0]<<endl);

		payloadPtr+=3;

		DEBUG_MSG("Hostname = "<<parseString(skipFields(payloadPtr,2))<<endl);


		cout.unsetf(ios::hex);
		cout.setf(ios::dec);
		#endif
	}
	else{
		ERROR("Failed to construct announce packet, aborting!"<<endl);
		ABORT_NOW();
	}


	map<link_info*, raw_packet*>::iterator pktIter = announceTargets.begin();

	for(; pktIter != announceTargets.end(); pktIter++){
		link_info* link = pktIter->first;
		raw_packet* targetedPkt = pktIter->second;
		if(targetedPkt == NULL){
			targetedPkt = new raw_packet();
			announceTargets[link] = targetedPkt;
		}

		uint8_t targetAddr[32];
		memset(targetAddr, 0x00, 32);

		raw_address::minimizeAddress(link->address.buffer, localAddressBuffer, targetAddr);

		if(buildADVPacket(targetedPkt, targetAddr, localAddressBuffer, -1, addressList, trunkList, announceSeq)){
			//
		}
		else{
			ERROR("Failed to construct target announce packet, aborting!"<<endl);
			ABORT_NOW();
		}
	}

	#if ANNOUNCE_TX_DEBUG
	DEBUG_MSG("end"<<endl);
	#endif
}


void si_node::rebuildMMTAnnouncement(){
	//MMT Announcement variables
	/*bool announceMMTEnabled;
	raw_packet announceMMTPkt;
	uint16_t announceMMTSeq;
	uint8_t* announceMMTPayload;
	uint32_t announcedMMTBytes;*/

	//Are we a cluster head?
	if((mmtInfo.mmtType & MMT_ClusterHead) != 0){

	}

	DEBUG_MSG("MMT Announce rebuild"<<endl);

	uint16_t payloadLen = getVidListByteCount(mmtInfo.vidList) + sizeof(uint8_t) + sizeof(uint8_t);


	announceMMTPayload = buildMMTPacket(&announceMMTPkt, localAddressBuffer, localAddressBuffer, MMT_Advertise, mmtInfo.uid, payloadLen);

	announceMMTPayload[0] = mmtInfo.mmtType;

	writeVidList(mmtInfo.vidList, &announceMMTPayload[1]);
}

void si_node::consumePacket(raw_packet *packet){
	uint16_t proto=packet->readPacketType();
	uint8_t* payloadPtr=packet->payloadPtr();
	uint8_t* sourceAddrPtr=packet->senderAddrPtr();

	#if PROCESS_CMP_DEBUG
	INFO(packetToString(packet)<<endl);
	#endif

	//Command packet processing
	if(proto == PROTO_SI_CMP){
		uint8_t cmp_major_type=CMP_MAJOR_TYPE(payloadPtr);
		uint8_t cmp_minor_type=CMP_MINOR_TYPE(payloadPtr);

		if(cmp_major_type==CMP_CloudAd){				//Cloud Advertisment
			#if ANNOUNCE_RX_DEBUG
			WARNING("Announce recieved on "<<packet->rx_socket->getName()<<endl);
			#endif

			bool rebuildRequired = false;	//! Indicates if the annoucement packet must be rebuilt
			link_info* rx_link=NULL;
			raw_address address(sourceAddrPtr);
			uint16_t seqNum = CMP_SEQ_NUM(payloadPtr);
			payloadPtr += CMP_HDR_LEN;

			//Disable periodic service to prevent race conditions
			periodic_service* service=periodic_service::instance();
			service->pause();

			//Lookup sender's address
			map<raw_address,link_info*>::iterator iter = addrLinkMap.find(address);


			if(iter==addrLinkMap.end() || (iter != addrLinkMap.end() && iter->second == NULL)){
				//No known link for this address
				#if ANNOUNCE_RX_DEBUG
				DEBUG_MSG("No known link for this address"<<endl);
				#endif

				//Construct new link_info
				rx_link = new link_info();
				rx_link->socket = packet->rx_socket;
				rx_link->removable = true;
				rx_link->hostname = parseString(skipFields(payloadPtr,2));
				rx_link->newNode = true;
				rx_link->mmt_enabled=false;
				if(rx_link->hostname.empty()){
					rx_link->hostname = address.toString();
				}

				#if ANNOUNCE_RX_DEBUG
				DEBUG_MSG("New node is host " << rx_link->hostname << endl);
				#endif

				rx_link->direct = (raw_address::readBC(packet->destinationAddrPtr()) == RESERVED_BC);

				//Setup deadlink watchdog
				rx_link->watchdog = new periodic(this, (periodic_t_func)&si_node::checkLinkStatus, (void*) rx_link);
				addrLinkMap.insert(make_pair(address, rx_link));
				links.push_back(rx_link);
			}
			else{
				//This host is already known
				#if ANNOUNCE_RX_DEBUG
				DEBUG_MSG("This host is already known"<<endl);
				#endif

				rx_link = iter->second;

				//Did we find the right link_info?
				if(rx_link->type == INTERNAL && rx_link->socket != packet->rx_socket){

					//Is this a new link?
					map<si_socket*,link_info*>::iterator internalIter = internalLinks.find(packet->rx_socket);

					if(internalIter == internalLinks.end() || internalIter->second == NULL){
						//New link

						//No known link for this address
						#if ANNOUNCE_RX_DEBUG
						DEBUG_MSG("No known link for this internal socket"<<endl);
						#endif

						//Construct new link_info
						rx_link = new link_info();
						rx_link->socket = packet->rx_socket;
						rx_link->removable = true;
						rx_link->hostname = parseString(skipFields(payloadPtr,2));
						rx_link->newNode = true;
						rx_link->mmt_enabled=false;
						if(rx_link->hostname.empty()){
							rx_link->hostname = address.toString();
						}

						#if ANNOUNCE_RX_DEBUG
						DEBUG_MSG("New node is host " << rx_link->hostname << endl);
						#endif

						rx_link->direct = (raw_address::readBC(packet->destinationAddrPtr()) == RESERVED_BC);

						//Setup deadlink watchdog
						rx_link->watchdog = new periodic(this, (periodic_t_func)&si_node::checkLinkStatus, (void*) rx_link);
						addrLinkMap.insert(make_pair(address, rx_link));
						links.push_back(rx_link);
					}
					else{
						rx_link = internalIter->second;
					}
				}
			}

			//Only update a link structure if the sequence has increased or if lastSequence is zero
			if(seqNum > rx_link->lastSequence || rx_link->lastSequence == 0 || rx_link->lastSequence == 0xffff){

				#if ANNOUNCE_RX_DEBUG
				DEBUG_MSG("Valid annoucement"<<endl);
				#endif

				rx_link->socket = packet->rx_socket;

				//Check if link status has changed only for existing nodes
				if(iter != addrLinkMap.end()){
					rx_link->newNode = false;
					bool oldDirect = rx_link->direct;
					rx_link->direct = (raw_address::readBC(packet->destinationAddrPtr()) == RESERVED_BC);

					#if ANNOUNCE_RX_DEBUG
					DEBUG_MSG("This is an existing node, has the status changed?"<<endl);
					#endif

					if(oldDirect==INDIRECT_LINK && rx_link->direct==DIRECT_LINK){

						#if ANNOUNCE_RX_DEBUG
						DEBUG_MSG("INDIRECT_LINK to host "<<address.toString()<<" became DIRECT_LINK"<<endl);
						#endif
						rebuildRequired = true;
					}
					else if(oldDirect==DIRECT_LINK && rx_link->direct==INDIRECT_LINK){
						//Direct link has just become indirect

						rebuildRequired = true;

						#if ANNOUNCE_RX_DEBUG
						DEBUG_MSG("DIRECT_LINK to host "<<address.toString()<<" became INDIRECT_LINK, target announcement added"<<endl);
						#endif

						/*raw_packet* directPkt=new raw_packet();
						if(buildADVPacket(directPkt, sourceAddrPtr, raw_LocalAddress)){
							announceTargets.insert(make_pair(rx_link, directPkt));

						}
						else{
							#if ANNOUNCE_RX_DEBUG
							ERROR("DIRECT_LINK to host "<<address.toString()<<" became INDIRECT_LINK, failed to build ADV packet!!!"<<endl);
							ABORT_NOW();
							#endif
						}*/
					}
					else{
						#if ANNOUNCE_RX_DEBUG
						DEBUG_MSG("Link state has not changed"<<endl);
						#endif
					}
				}
				else{
					//Configure announcements for new nodes

					#if ANNOUNCE_RX_DEBUG
					DEBUG_MSG("Configure announcements for new node"<<endl);
					#endif

					rebuildRequired = true;
				}


				rx_link->lastSequence = seqNum;
				//rx_link->hostname = parseString(skipFields(payloadPtr,2));

				if(cmp_minor_type == ADV_Alive){
					//Do nothing
					#if ANNOUNCE_RX_DEBUG
					DEBUG_MSG("Parsing ADV_Alive"<<endl);
					#endif
				}
				else if(cmp_minor_type == ADV_ForcedAdv){
					//Forced advertisements may not be deleted
					#if ANNOUNCE_RX_DEBUG
					DEBUG_MSG("Parsing ADV_ForcedAdv"<<endl);
					#endif
					rx_link->removable = false;
				}
				else if(cmp_minor_type == ADV_ListsSet){
					//Update address lists
					#if ANNOUNCE_RX_DEBUG
					DEBUG_MSG("Parsing ADV_ListsSet"<<endl);
					#endif
					uint16_t bytes = ADDR_LIST_BYTES(payloadPtr);

					//Parse secondary addresses
					if(bytes > 0){
						rx_link->secondaryAddresses = parseAddressList(payloadPtr);
					}
					payloadPtr += bytes + ADDR_LIST_HDR_LEN;


					//Parse trunk address list
					bytes = ADDR_LIST_BYTES(payloadPtr);
					if(bytes > 0){
						rx_link->trunkList = parseAddressList(payloadPtr);
					}
					payloadPtr += bytes + ADDR_LIST_HDR_LEN;
				}
				else if(cmp_minor_type == ADV_ListsRemove){
					//Update secondary address lists
					#if ANNOUNCE_RX_DEBUG
					DEBUG_MSG("Parsing ADV_ListsRemove"<<endl);
					#endif
					uint16_t bytes = ADDR_LIST_BYTES(payloadPtr);


					if(bytes > 0){
						//Parse secondary addresses
						vector<raw_address> secondary = parseAddressList(payloadPtr);

						//Remove the specified addresses
						while(!secondary.empty()){
							vector<raw_address>::iterator iter = find(rx_link->secondaryAddresses.begin(), rx_link->secondaryAddresses.end(), secondary.back());

							while(iter != rx_link->secondaryAddresses.end()){
								rx_link->secondaryAddresses.erase(iter);
								iter = find(rx_link->secondaryAddresses.begin(), rx_link->secondaryAddresses.end(), secondary.back());
							}
							secondary.pop_back();
						}

					}
					payloadPtr += bytes + ADDR_LIST_HDR_LEN;


					//Update trunk address lists
					bytes = ADDR_LIST_BYTES(payloadPtr);

					if(bytes > 0){
						//Parse trunk addresses
						vector<raw_address> trunks = parseAddressList(payloadPtr);

						//Remove the specified addresses
						while(!trunks.empty()){
							vector<raw_address>::iterator iter = find(rx_link->trunkList.begin(), rx_link->trunkList.end(), trunks.back());

							while(iter != rx_link->trunkList.end()){
								rx_link->trunkList.erase(iter);
								iter = find(rx_link->trunkList.begin(), rx_link->trunkList.end(), trunks.back());
							}
							trunks.pop_back();
						}

					}
					payloadPtr += bytes + ADDR_LIST_HDR_LEN;
				}
				else{
					ERROR("Unknown minor type(" << (int)cmp_minor_type << ")!" << endl);
				}


				if(updateLink(address, rx_link)){
					rx_link->hasFailed = false;

					if(rebuildRequired){
						rebuildAnnouncement();
					}

					if(rx_link->newNode == true){
						if(newLinkCallback != NULL){
							(*newLinkCallback)(rx_link);
						}

						//Update MMT type
						if((rx_link->type == UPLINK || rx_link->type == TRUNK) && (mmtInfo.mmtType & rx_link->type)!=rx_link->type){
							mmtInfo.mmtType |= (rx_link->type & ~(IMPROPER|INTERNAL));
							if(IS_CLUSER_HEAD(mmtInfo.mmtType) != 0 && (mmtInfo.mmtType & MMT_ClusterHead) == 0){
								mmtInfo.mmtType |= MMT_ClusterHead;

								if(clusterHeadCallback != NULL){
									//Notify userspace of ClusterHead status change
									(*clusterHeadCallback)(&mmtInfo);
								}
							}
							rebuildMMTAnnouncement();
						}

						if(rx_link->type == (IMPROPER | UPLINK) || rx_link->type == (IMPROPER | TRUNK)){

							//Make sure the target host knows about us
							if(sendPacket(announcePkt, *rx_link->socket) < 0){
								ERROR("Packet transmission error :("<<endl);
							}

							#if PROCESS_CMP_DEBUG
							INFO("Sending secondary address request to host " << rx_link->getName() << endl);
							#endif

							//Send secondary address request
							payloadPtr = buildCMPPacket(packet,
														//rx_link->minAddress.buffer,
														(uint8_t*) raw_linkLocal,
														localAddressBuffer,
														MK_CMP_TYPE(CMP_Config,CONF_SIAddrRequest),
														RAND_SEQ);

							if(sendPacket(*packet, *rx_link->socket) < 0){
								ERROR("Packet transmission error :("<<endl);
							}
						}
					}
				}
				else{
					#if ANNOUNCE_RX_DEBUG
					ERROR("Failed to process announcement packet!"<<endl);
					#endif
				}
			}
			else{
				#if ANNOUNCE_RX_DEBUG
				DEBUG_MSG("Ignoring late annoucement"<<endl);
				#endif
			}

			//Enable periodic service
			service->resume(true);
		}
		else if(cmp_major_type==CMP_EchoRequest){		//Process Echo Request
			uint16_t sequenceNum=CMP_SEQ_NUM(payloadPtr);
			#if PROCESS_CMP_DEBUG
			struct timeval tv;
			gettimeofday(&tv, NULL);
			DEBUG_MSG("Responding to echo request with seq="<<ntohs(sequenceNum)<<" Seconds:"<<tv.tv_sec<<" uSeconds:"<<tv.tv_usec<<endl);
			#endif

			uint8_t* destAddrPtr=packet->destinationAddrPtr();
			raw_address::minimizeAddress(sourceAddrPtr, localAddressBuffer, destAddrPtr);
			sourceAddrPtr=packet->senderAddrPtr();
			memcpy(sourceAddrPtr, localAddressBuffer, localAddressSize);


			payloadPtr=packet->payloadPtr();
			payloadPtr[0]=(CMP_EchoReply<<4) & 0xf0;

			CMP_SET_SEQ_NUM(payloadPtr, sequenceNum);

			packet->setPayloadLength(3);

			packet->rx_socket = NULL;

			routePacket(packet);

			//sendPacket(*packet, *packet->rx_socket);
		}
		else if(cmp_major_type==CMP_Config){			//Configuration packet
			processConfPacket(packet, cmp_minor_type, payloadPtr, sourceAddrPtr);
		}
		else{
			#if PROCESS_CMP_DEBUG
			UNSUPPORTED_MSG("Recieved unsupported command packet with major="<<(int)cmp_major_type<<" and minor="<<(int)cmp_minor_type<<endl);
			#endif
		}
	}
	else if(consumeCallback!=NULL){
		raw_packet* pack=new raw_packet(*packet);
		(*consumeCallback)(pack);

		#if ROUTE_DEBUG
		uint8_t* destAddrPtr=packet->destinationAddrPtr();

		DEBUG_MSG("Consumed packet from host ");
		raw_address::printAddress(sourceAddrPtr);
		APPEND_MSG(" to ");
		raw_address::printAddress(destAddrPtr);
		APPEND_MSG(endl);
		#endif
	}
	else{
		#if ROUTE_DEBUG
		uint8_t* destAddrPtr=packet->destinationAddrPtr();
		DEBUG_MSG("Discraded packet from host ");
		raw_address::printAddress(sourceAddrPtr);
		cout<<" to ";
		raw_address::printAddress(destAddrPtr);
		cout<<endl;
		#endif
		return;
	}
}


void si_node::processConfPacket(raw_packet* packet, uint8_t minor, uint8_t* payloadPtr, uint8_t* sourceAddrPtr){
	uint16_t seq = CMP_SEQ_NUM(payloadPtr);
	uint8_t flags = CMP_GET_FLAGS(payloadPtr);

	if(minor == CONF_SetSIAddr){
		#if PROCESS_CONF_DEBUG
		DEBUG_MSG("Executing Configuration command CONF_SetSIAddr" << endl);
		#endif

		link_info* rx_link = NULL;
		raw_address address = raw_address(sourceAddrPtr);
		periodic_service* service = periodic_service::instance();

		//Lookup sender's address
		service->pause();
		map<raw_address,link_info*>::iterator iter = addrLinkMap.find(address);
		service->resume(false);

		if(iter != addrLinkMap.end()){

			rx_link = iter->second;
		}

		if(rx_link == NULL){
			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address set command received from host["<<address.toString()<<"] with no known link" << endl);
			#endif
			return;
		}
		else if(rx_link->direct == false){
			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address set command from indirect host " << rx_link->getName() << endl);
			#endif
		}

		si_address newAddress;
		newAddress.readFrom(&payloadPtr[4], raw_address::byteCount(&payloadPtr[4]));

		INFO("Setting address to ["<<newAddress.toString()<<"] as commanded by host="<<rx_link->getName());
		setLocalAddress(newAddress);
	}
	else if(minor == CONF_SetMMTUID){
		#if PROCESS_CONF_DEBUG
		DEBUG_MSG("Executing Configuration command CONF_SetMMTUID" << endl);
		#endif
	}
	else if(minor == CONF_SIAddrRequest){
		#if PROCESS_CONF_DEBUG
		DEBUG_MSG("Executing Configuration command CONF_SIAddrRequest" << endl);
		#endif

		link_info* rx_link = NULL;
		raw_address address = raw_address(sourceAddrPtr);
		periodic_service* service = periodic_service::instance();

		//Lookup sender's address
		service->pause();
		map<raw_address,link_info*>::iterator iter = addrLinkMap.find(address);
		service->resume(false);

		if(iter != addrLinkMap.end()){
			rx_link = iter->second;
		}

		if(rx_link == NULL){
			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address request received from host with no known link" << endl);
			#endif
			return;
		}
		else if(rx_link->direct == false){
			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address request from indirect host " << rx_link->getName() << endl);
			#endif
			return;
		}
		else if(orphaned==true && rx_link->type != (IMPROPER|DOWNLINK)){
			#if PROCESS_CONF_DEBUG
			WARNING("Orphaned, rejecting address request from host"<<rx_link->getName()<<" of unsupported type["<<linkTypeToString(rx_link->type)<<"]"<<endl);
			#endif
			return;
		}
		else if(orphaned==false
				&& rx_link->type != (IMPROPER|DOWNLINK)
				&& rx_link->type != (IMPROPER|UPLINK)
				&& rx_link->type != (IMPROPER|TRUNK)) {

			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address request from host"<<rx_link->getName()<<" of unsupported type["<<linkTypeToString(rx_link->type)<<"]"<<endl);
			#endif
			return;
		}

		//Check flags
		if(flags == CMP_FLAG_ACK){
			//The sender is claiming a reserved address
			//Note: It is assumed that the sender has included the address we offered them

			raw_address ackAddress = raw_address(&payloadPtr[4]);

			//Find addrLinkMap entry for ackAddress
			service->pause();
			map<raw_address, link_info*>::iterator addrIter = addrLinkMap.find(ackAddress);

			if(addrIter == addrLinkMap.end()){
				//No such address reservation

				#if PROCESS_CONF_DEBUG
				ERROR("Failed to find address reservation for host "
					  <<rx_link->getName() <<" requesting address="
					  <<ackAddress.toString()<<endl);
				#endif

				//Let client know that an error occured, abitration should start over
				payloadPtr = buildCMPPacket(packet,
											//rx_link->minAddress.buffer,
											(uint8_t*) raw_linkLocal,
											localAddressBuffer,
											MK_CMP_TYPE(CMP_Config,CONF_SIAddrOffer),
											++seq,
											CMP_FLAG_NAK|CMP_FLAG_ERR);

				packet->appendData(NULL, ackAddress.countBytes());
				ackAddress.writeAddress(&payloadPtr[4]);

				sendPacket(*packet, *rx_link->socket);

				service->resume();
				return;
			}
			else if(addrIter->second != NULL){
				#if PROCESS_CONF_DEBUG
				ERROR("Address already in use! Host "
					  <<rx_link->getName() <<" requested address="
					  <<ackAddress.toString()<<endl);
				#endif

				//Let client know that an error occured, abitration should start over
				payloadPtr = buildCMPPacket(packet,
											//rx_link->minAddress.buffer,
											(uint8_t*) raw_linkLocal,
											localAddressBuffer,
											MK_CMP_TYPE(CMP_Config,CONF_SIAddrOffer),
											++seq,
											CMP_FLAG_NAK|CMP_FLAG_ERR);

				packet->appendData(NULL, ackAddress.countBytes());
				ackAddress.writeAddress(&payloadPtr[4]);

				sendPacket(*packet, *rx_link->socket);

				service->resume();
				return;
			}


			addrLinkMap.erase(addrIter);


			//Set rx_link as the value for the entry
			addrLinkMap.insert(make_pair(ackAddress, rx_link));

			service->resume();
		}
		else if(flags == 0x0){
			//Attempt to reserve an address for the sender

			//Get last address field
			address_chunk chunk = address.chunks.at(address.chunks.size() - 2);
			uint16_t value = chunk.value;


			raw_address* offerAddress = new raw_address(local_address);
			offerAddress->tierLevel++;
			offerAddress->chunks.insert(offerAddress->chunks.end()-1, chunk);

			service->pause();
			map<raw_address, link_info*>::iterator addrIter = addrLinkMap.find(*offerAddress);

			//Check for collision
			if(addrIter != addrLinkMap.end()){
				do{
					//Generate next address
					++chunk;
					offerAddress->chunks[offerAddress->chunks.size()-2] = chunk;

					#if PROCESS_CONF_DEBUG
					DEBUG_MSG("Checking address["<<offerAddress->toString() << "]" << endl);
					#endif

					map<raw_address, link_info*>::iterator addrIter = addrLinkMap.find(*offerAddress);

					if(addrIter == addrLinkMap.end()){
						//Found available address
						break;
					}
				} while(chunk.value != value);


				//Generated a valid address?
				if(chunk.value == value){
					//No addresses available
					#if PROCESS_CONF_DEBUG
					WARNING("Address allocation failed!" << endl);
					#endif

					//Send NAK packet
					payloadPtr = buildCMPPacket(packet,
												//rx_link->minAddress.buffer,
												(uint8_t*) raw_linkLocal,
												localAddressBuffer,
												MK_CMP_TYPE(CMP_Config,CONF_SIAddrOffer),
												++seq,
												CMP_FLAG_NAK);

					sendPacket(*packet, *rx_link->socket);

					delete offerAddress;

					service->resume();
					return;
				}
			}
			else{
				#if PROCESS_CONF_DEBUG
				cout.unsetf(ios::dec);
				cout.setf(ios::hex);
				DEBUG_MSG("Using default last AF = " << (int)chunk.value << endl);
				cout.unsetf(ios::hex);
				cout.setf(ios::dec);
				#endif
			}

			#if PROCESS_CONF_DEBUG
			DEBUG_MSG("Offering host " << rx_link->getName() << " address [" << offerAddress->toString() << "]" << endl);
			#endif

			//Build reply packet
			payloadPtr = buildCMPPacket(packet,
										//rx_link->minAddress.buffer,
										(uint8_t*) raw_linkLocal,
										localAddressBuffer,
										MK_CMP_TYPE(CMP_Config,CONF_SIAddrOffer),
										++seq,
										CMP_FLAG_ACK);

			packet->appendData(NULL, offerAddress->countBytes());

			offerAddress->writeAddress(&payloadPtr[4]);

			DEBUG_MSG("Written address = " << raw_address::toString(&payloadPtr[4], true) << endl);

			if(sendPacket(*packet, *rx_link->socket) >=0 ){
				//Packet was sent successfully

				//Setup address reservation
				addrLinkMap[*offerAddress] = NULL;

				//Configure periodic to cleanup reservation if it is not ACK'd by the client in time
				periodic* p = new periodic(this, (periodic_t_func)&si_node::checkAllocatedAddress, (void*) offerAddress);
				p->install(	(timeval){ALLOC_ADDRESS_TIMEOUT_SEC, ALLOC_ADDRESS_TIMEOUT_uSEC},
							(timeval){0, 0},
							"SIAddrRequest_CleanUp");
			}

			service->resume();
			return;
		}

		#if PROCESS_CONF_DEBUG
		cout.unsetf(ios::dec);
		cout.setf(ios::hex);
		UNSUPPORTED_MSG("Rejecting address request from host " << rx_link->getName() << " due to unsupported CMD_FLAGS=0x" << (int)flags << endl);
		cout.unsetf(ios::hex);
		cout.setf(ios::dec);
		#endif
	}
	else if(minor == CONF_SIAddrOffer){
		#if PROCESS_CONF_DEBUG
		DEBUG_MSG("Executing Configuration command CONF_SIAddrOffer" << endl);
		#endif

		link_info* rx_link = NULL;
		raw_address address = raw_address(sourceAddrPtr);

		//Lookup sender's address
		map<raw_address,link_info*>::iterator iter = addrLinkMap.find(address);

		if(iter != addrLinkMap.end()){
			rx_link = iter->second;
		}

		if(rx_link == NULL){
			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address offer received from host[" << address.toString() << "] with no known link" << endl);
			#endif
			return;
		}
		else if(rx_link->direct == false){
			#if PROCESS_CONF_DEBUG
			WARNING("Rejecting address offer from indirect host " << rx_link->getName() << endl);
			#endif
		}

		if(flags == CMP_FLAG_ACK){

			raw_address* offerAddress = new raw_address(&payloadPtr[4]);

			#if PROCESS_CONF_DEBUG
			INFO("Offered address [" << offerAddress->toString() << "] from " << rx_link->getName() << endl);
			#endif

			//TODO: Check secondary address policy HERE

			//Build address claim packet
			payloadPtr = buildCMPPacket(packet,
										//rx_link->minAddress.buffer,
										(uint8_t*) raw_linkLocal,
										localAddressBuffer,
										MK_CMP_TYPE(CMP_Config,CONF_SIAddrRequest),
										++seq,
										CMP_FLAG_ACK);
			packet->appendData(NULL, offerAddress->bytes);
			offerAddress->writeAddress(&payloadPtr[4]);

			if(sendPacket(*packet, *rx_link->socket) >= 0){
				//Packet was sent successfully

				if(orphaned){
					//Check if there are other valid parent uplinks
					bool hasParent=false;

					for(uint32_t i=0; i<links.size(); i++){
						if(links[i]->type==UPLINK){
							hasParent=true;
							break;
						}
					}

					if(!hasParent){
						//Change address to the newly offered one
						si_address address;
						address.readFrom(offerAddress->buffer, offerAddress->bytes);

						#if PROCESS_CONF_DEBUG
						INFO("NO ACTIVE PARENT, changing to newly acquired secondary address[" << address.toString() << "]" <<endl);
						#endif

						setLocalAddress(address);
						orphaned = false;
						if(orphanedCallback != NULL){
							(*orphanedCallback)(orphaned);
						}
						return;
					}
				}

				//Add address to list of addresses

				if(addressList.size() > 0){
					list<raw_address*>::iterator addrIter = addressList.begin();

					for(; addrIter != addressList.end(); addrIter++){
						raw_address* addr = *addrIter;

						if(addr->tierLevel > offerAddress->tierLevel){
							addressList.insert(addrIter, offerAddress);
							break;
						}
					}

					if(addrIter == addressList.end()){
						//Read to the end of the list and did not find a spot to insert
						addressList.push_back(offerAddress);
					}
				}
				else{
					addressList.push_back(offerAddress);
				}


				rebuildAnnouncement();

				#if PROCESS_CONF_DEBUG
				INFO("Accepted address [" << offerAddress->toString() << "] offered from " << rx_link->getName() << endl);
				#endif
			}
			else{
				ERROR("Failed to transmit CONF_SIAddrRequest ACK packet!"<<endl);

				//TODO: Design transmit error recovery system!
			}
		}
		else if(flags == CMP_FLAG_NAK){
			//Host failed to allocate address to us
			#if PROCESS_CONF_DEBUG
			ERROR("Address allocation from host " << rx_link->getName() << " failed!" << endl);
			#endif
		}
		else{

			#if PROCESS_CONF_DEBUG
			cout.unsetf(ios::dec);
			cout.setf(ios::hex);
			WARNING("Rejecting address offer from host " << rx_link->getName() << " due to unsupported CMD_FLAGS=0x" << (int)flags << endl);
			cout.unsetf(ios::hex);
			cout.setf(ios::dec);
			#endif
		}
	}
	else if(minor == CONF_MMTUIDRequest){
		#if PROCESS_CONF_DEBUG
		DEBUG_MSG("Executing Configuration command CONF_MMTUIDRequest" << endl);
		#endif
	}
	else if(minor == CONF_MMTUIDOffer){
		#if PROCESS_CONF_DEBUG
		DEBUG_MSG("Executing Configuration command CONF_MMTUIDOffer" << endl);
		#endif
	}
	else{
		#if PROCESS_CONF_DEBUG
		UNSUPPORTED_MSG("Received unsupported configuration packet with minor="<<(int)minor<<endl);
		#endif
	}
}

void si_node::checkAllocatedAddress(void* arg, periodic* p){

	if(arg != NULL){
		raw_address* offerAddress = (raw_address*) arg;

		map<raw_address, link_info*>::iterator addrIter = addrLinkMap.find(*offerAddress);

		if(addrIter != addrLinkMap.end()){
			//Found map entry
			if(addrIter->second == NULL){
				addrLinkMap.erase(addrIter);
			}
		}
		else{
			WARNING("Allocated address[" << offerAddress->toString() << "] already deallocated, this is strange" << endl);
		}

		delete offerAddress;
		p->argument = NULL;
	}

	//Clean up
	p->remove();
}

bool si_node::updateLink(raw_address address, link_info* link){
	if(link==NULL){ return false; }


	//raw_address address(addrPtr);

	if(address.chunks.size() < 1){
		ERROR("Too few chunks!"<<endl);
		return false;
	}

	link->missed_announcements = 0;
	gettimeofday(&link->last_announce, NULL);

	//Defer deadlink watchdog timer
	link->watchdog->install((timeval){ANNOUNCE_PERIOD_SEC, ANNOUNCE_PERIOD_uSEC},
							(timeval){HALF_ANNOUNCE_PERIOD_SEC, HALF_ANNOUNCE_PERIOD_uSEC},
							link->hostname);

	//Determine link type
	uint8_t type = raw_address::getRelationship(address.buffer, localAddressBuffer);


	//Check if link address changed or link type changed
	if(link->type != type || link->address != address){
		#if ANNOUNCE_RX_DEBUG
		if(link->address != address){
			DEBUG_MSG("Link address("<<link->address.toString()<<") does not match "<<address.toString()<<endl);
		}
		else{
			DEBUG_MSG("Link type("<< linkTypeToString(link->type) <<") does not match "<<linkTypeToString(type)<<endl);
		}
		#endif

		removeMapEntries(link);
		link->address=address;
		uint8_t tempAddr[link->address.bytes];
		memset(tempAddr, 0x00, link->address.bytes);
		raw_address::minimizeAddress(link->address.buffer, localAddressBuffer, tempAddr);
		link->minAddress = raw_address(tempAddr);

		addrLinkMap.insert(make_pair(link->address, link));
	}

	link->type = type;

	if(link->type == UPLINK){
		//UPLINK
		uint16_t lastValue = address.chunks.at(address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=upMap.find(lastValue);
		if(linkIter==upMap.end()){		//Not already known
			link->mmt_enabled=false;
			upMap.insert(make_pair(lastValue, link));
			upMapTable.convergance=false;
			orphaned = false;

			if(orphanedCallback != NULL){
				(*orphanedCallback)(orphaned);
			}

			#if ANNOUNCE_RX_DEBUG
			INFO("INSERT UPLINK"<<endl);
			#endif
		}
		else{
			#if ANNOUNCE_RX_DEBUG
			DEBUG_MSG("FOUND UPLINK"<<endl);
			#endif
		}
	}
	else if(link->type == DOWNLINK && address.chunks.size() > 1){
		//DOWNLINK
		uint16_t lastValue = address.chunks.at(address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=downMap.find(lastValue);
		if(linkIter==downMap.end()){	//Not already known
			link->mmt_enabled=false;
			downMap.insert(make_pair(lastValue, link));
			downMapTable.convergance=false;
			#if ANNOUNCE_RX_DEBUG
			INFO("INSERT DOWNLINK"<<endl);
			#endif
		}
		else{
			#if ANNOUNCE_RX_DEBUG
			DEBUG_MSG("FOUND DOWNLINK"<<endl);
			#endif
		}
	}
	else if(link->type == INTERNAL){
		//INTERNAL LINK
		map<si_socket*, link_info*>::iterator linkIter=internalLinks.find(link->socket);
		if(linkIter==internalLinks.end()){
			if(link->direct){
				link->mmt_enabled=this->mmt_enabled;
			}
			else{
				link->mmt_enabled=false;
			}
			internalLinks.insert(make_pair(link->socket, link));
			#if ANNOUNCE_RX_DEBUG
			INFO("INSERT INTERNAL"<<endl);
			#endif
		}
		else{
			#if ANNOUNCE_RX_DEBUG
			DEBUG_MSG("FOUND INTERNAL"<<endl);
			#endif

			if(link->direct){
				link->mmt_enabled=this->mmt_enabled;
			}
			else{
				link->mmt_enabled=false;
			}
		}
	}
	else if(link->type == TRUNK && address.chunks.size() > 1){
		//TRUNK LINK
		uint16_t lastValue = address.chunks.at(address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=trunkMap.find(lastValue);
		if(linkIter==trunkMap.end()){
			link->mmt_enabled=false;
			trunkMap.insert(make_pair(lastValue, link));

			trunkMapTable.convergance=false;
			#if ANNOUNCE_RX_DEBUG
			INFO("INSERT TRUNK"<<endl);
			#endif
		}
		else{
			#if ANNOUNCE_RX_DEBUG
			DEBUG_MSG("FOUND TRUNK"<<endl);
			#endif
		}
	}
	else if(link->type == (IMPROPER|UPLINK)){
		//Improper UPLINK
		link->mmt_enabled = false;

		uint16_t lastValue = address.chunks.at(address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=upMap.find(lastValue);
		if(linkIter==upMap.end()){		//Not already known
			link->mmt_enabled=false;

			upMap.insert(make_pair(lastValue, link));
			upMapTable.convergance=false;
			#if ANNOUNCE_RX_DEBUG
			INFO("INSERT IMPROPER|UPLINK"<<endl);
			#endif
		}
		else{
			#if ANNOUNCE_RX_DEBUG
			DEBUG_MSG("FOUND IMPROPER|UPLINK"<<endl);
			#endif
		}
	}
	else if(link->type == (IMPROPER|DOWNLINK)){
		//Improper DOWNLINK
		#if ANNOUNCE_RX_DEBUG
		DEBUG_MSG("Improper DOWNLINK"<<endl);
		#endif

		link->mmt_enabled = false;
	}
	else if(link->type == (IMPROPER|TRUNK)){
		//Improper TRUNK
		#if ANNOUNCE_RX_DEBUG
		DEBUG_MSG("Improper TRUNK"<<endl);
		#endif

		link->mmt_enabled = false;
	}
	else{
		ERROR("Failed to determine link type!"<<endl);
		return false;
	}

	return true;
}

void si_node::checkLinkStatus(void* arg, periodic* p){
	link_info* link = (link_info*) arg;

	timeval currentTime;
	timeval minTime;
	timeval maxDelta;
	maxDelta.tv_sec=HALF_ANNOUNCE_PERIOD_SEC;
	maxDelta.tv_usec=HALF_ANNOUNCE_PERIOD_uSEC;

	gettimeofday(&currentTime, NULL);
	timersub(&currentTime, &maxDelta, &minTime);

	//Check if last announce was before (currentTime - ANNOUCNE_PERIOD/2)
	if(timercmp(&minTime, &(link->last_announce), >)){
		//Past the last expected announce time
		//Increase missed annoucement count
		link->missed_announcements++;

		if(missedHelloCallback != NULL){
			(*missedHelloCallback)(link);
		}

		if(link->missed_announcements == LINK_MISSED_PKT_ALT_PATH_THRESH){
			//Attempt to setup alternate routing path
			link->hasFailed=true;

			link->lastSequence = 0;

			if(link->type == UPLINK){
				if(trunkMap.size() > 0){

					//Search for a trunk that can be used to route to the failing UPLINK
					map<uint16_t,link_info*>::iterator trunkIter = trunkMap.begin();
					for(; trunkIter != trunkMap.end(); trunkIter++){
						link_info* altLink = trunkIter->second;

						//Route through a working direct trunk
						if(altLink->direct && altLink->type==TRUNK && !altLink->hasFailed){

							#if DEAD_LINKS_DEBUG
							INFO("UPLINK FAILED OVER[" << link->socket->getName()
									  << ":" << link->hostname << ":"
									  << link->address.toString() << "], to alternate path via"
									  << altLink->getName() << endl);
							#endif

							if(usingAltPathCallback != NULL){
								(*usingAltPathCallback)(link, altLink);
							}

							link->direct = false;
							link->socket = altLink->socket;

							//Insert directed advertisement
							rebuildAnnouncement();
							return;
						}
					}
				}
			}
			else if(link->type == DOWNLINK && link->trunkList.size() > 0){

				//Loop over failing DOWNLINK's trunklist for a TRUNK
				//which is also connected to this node as a direct TRUNK.

				vector<raw_address>::iterator trunkIter = link->trunkList.begin();
				for(; trunkIter!=link->trunkList.end(); trunkIter++){

					map<raw_address,link_info*>::iterator addrIter = addrLinkMap.find(*trunkIter);

					//Is this a known TRUNK?
					if(addrIter != addrLinkMap.end()){

						//Known trunk, is it working?
						if(!addrIter->second->hasFailed && addrIter->second->socket!=NULL){

							#if DEAD_LINKS_DEBUG
							INFO("TRUNK- FAILED OVER" << link->getName()
								 << ", to alternate path via ["
								 << addrIter->second->socket->getName()
								 << ":" << addrIter->second->hostname
								 << ":" << addrIter->second->address.toString() <<"]" << endl);
							#endif

							if(usingAltPathCallback != NULL){
								(*usingAltPathCallback)(link, addrIter->second);
							}

							link->direct = false;
							link->socket = addrIter->second->socket;

							//Insert directed advertisement
							rebuildAnnouncement();
							return;
						}
					}
				}
			}

			#if DEAD_LINKS_DEBUG
			INFO("NO ALTERNATE ROUTE FOR LINK"<< link->getName() <<endl);
			#endif

		}
		else if(link->missed_announcements >= LINK_MISSED_PKT_DELETE_THRESH){
			//Too many missed announcements, link will now fail
			link->hasFailed=true;

			/*
			  TODO:	EDGE-CASE, It may be a good idea to try to find a link to
					fail over to since it is possible that a valid alternate
					path has been established since the first missed packet.
			  */


			//Must remove link from all tables before periodic_reschedule gets triggered
			removeMapEntries(link);

			//Remove link from links vector
			vector<link_info*>::iterator iter = find(links.begin(), links.end(), link);
			links.erase(iter);

			//Clean up socket map
			multimap<si_socket*, link_info*>::iterator beginIter = socketMap.lower_bound(link->socket);
			multimap<si_socket*, link_info*>::iterator endIter = socketMap.upper_bound(link->socket);

			for(; beginIter!=endIter; beginIter++){
				if(beginIter->second == link){
					socketMap.erase(beginIter);
					break;
				}
			}

			if(deadLinkCallback != NULL){
				(*deadLinkCallback)(link);
			}


			if(link->type == UPLINK){
				if(addressList.size() > 0){
					//Check if there are other valid parent uplinks
					bool hasParent=false;

					for(uint32_t i=0; i<links.size(); i++){
						if(links[i] != link && links[i]->type==UPLINK){
							hasParent=true;
							break;
						}
					}

					if(!hasParent){
						//Change address to the first one in addressList
						si_address address;
						//address.readFrom(addressList.front(), raw_address::byteCount(addressList.front()));
						raw_address* rawAddr = addressList.front();
						address.readFrom(rawAddr->buffer, rawAddr->bytes);
						delete addressList.front();
						addressList.erase(addressList.begin());

						#if DEAD_LINKS_DEBUG
						INFO("NO ALTERNATE ROUTE FOR LINK" << link->getName() << " changing to secondary address[" << address.toString() << "]" <<endl);
						#endif
						setLocalAddress(address);	//NOTE: Setting address causes an announcement rebuild
						orphaned = false;
					}
					else{
						orphaned = true;
						rebuildAnnouncement();
					}
				}

				if(upMap.size() == 0){
					//No other UpLinks
					orphaned = true;
					rebuildAnnouncement();

					//Update MMT Data structures
					mmtInfo.mmtType &= ~MMT_Up;		//Turn off UpLink bit

					//Are we still a cluster head?
					if(IS_CLUSER_HEAD(mmtInfo.mmtType) != 0){
						//Still a cluster head
						mmtInfo.mmtType |= MMT_ClusterHead;
					}
					else{
						//No longer a cluster head
						mmtInfo.mmtType &= ~MMT_ClusterHead;

						//Notify userspace of ClusterHead status change
						if(clusterHeadCallback != NULL){
							(*clusterHeadCallback)(&mmtInfo);
						}
					}
					rebuildMMTAnnouncement();
				}

				if(orphanedCallback != NULL){
					(*orphanedCallback)(orphaned);
				}
			}
			else if(link->type == TRUNK){
				if(trunkMap.size() == 0){
					//Update MMT Data structures
					mmtInfo.mmtType &= ~MMT_Trunk;		//Turn off TrunkLink bit

					//Are we still a cluster head?
					if(IS_CLUSER_HEAD(mmtInfo.mmtType) != 0){
						//Still a cluster head
						mmtInfo.mmtType |= MMT_ClusterHead;
					}
					else{
						//No longer a cluster head
						mmtInfo.mmtType &= ~MMT_ClusterHead;

						//Notify userspace of ClusterHead status change
						if(clusterHeadCallback != NULL){
							(*clusterHeadCallback)(&mmtInfo);
						}
					}
					rebuildMMTAnnouncement();
				}
			}
			else if(link->type == DOWNLINK){
				if(downMap.size() == 0){
					//Update MMT Data structures
					mmtInfo.mmtType &= ~MMT_Down;		//Turn off TrunkLink bit

					//Are we still a cluster head?
					if(IS_CLUSER_HEAD(mmtInfo.mmtType) != 0){
						//Still a cluster head
						mmtInfo.mmtType |= MMT_ClusterHead;
					}
					else{
						//No longer a cluster head
						mmtInfo.mmtType &= ~MMT_ClusterHead;

						//Notify userspace of ClusterHead status change
						if(clusterHeadCallback != NULL){
							(*clusterHeadCallback)(&mmtInfo);
						}
					}

					rebuildMMTAnnouncement();
				}
			}
			else{
				rebuildAnnouncement();
				rebuildMMTAnnouncement();
			}

			#if DEAD_LINKS_DEBUG
			timeval deltaTime;
			timersub(&currentTime, &link->last_announce, &deltaTime);
			INFO("REMOVING["<<link->socket->getName()<<":"<<link->hostname<<":"<<link->address.toString() << "], too many missed announcements[" << (int)link->missed_announcements<<"] in "<<timevalToString(deltaTime)<<endl);
			#endif

			delete link;
		}
		else{
			//Do nothing
			#if DEAD_LINKS_DEBUG
			INFO("MISSED ANNOUNCEMENT COUNT=" << (int)link->missed_announcements << " on LINK["<<link->socket->getName() << ":" << link->hostname << ":" << link->address.toString() << "]" << endl);
			#endif
		}
	}
}

void si_node::removeDeadLinks(){
	timeval currentTime;
	timeval minTime;
	timeval maxDelta;
	maxDelta.tv_sec=MAX_LINK_AGE_SEC;
	maxDelta.tv_usec=MAX_LINK_AGE_uSEC;

	gettimeofday(&currentTime, NULL);
	timersub(&currentTime, &maxDelta, &minTime);

	//DEBUG_MSG("Running minTime["<<minTime.tv_sec<<": "<<minTime.tv_usec<<"]"<<endl);

	vector<link_info*>::iterator linkIter=links.begin();
	vector<link_info*>::iterator tempIter;

	while(linkIter!=links.end()){
		if(timercmp(&minTime, &(*linkIter)->last_announce, >)){
			tempIter=linkIter;
			linkIter++;

			link_info* link = *tempIter;

			if(link->hasFailed){
				#if DEAD_LINKS_DEBUG
				DEBUG_MSG("Removing link["<<link->socket->getName()<<":"<<link->hostname<<":"<<link->address.toString() << "]"<<endl);
				#endif

				deleteLink(link);
			}
			else{
				link->hasFailed=true;
				if(link->trunkList.size()>0){
					//Update link timer
					gettimeofday(&link->last_announce, NULL);

					vector<raw_address>::iterator trunkIter = link->trunkList.begin();

					for(; trunkIter!=link->trunkList.end(); trunkIter++){
						map<raw_address,link_info*>::iterator addrIter = addrLinkMap.find(*trunkIter);
						if(addrIter != addrLinkMap.end()){
							if(!addrIter->second->hasFailed && addrIter->second->socket!=NULL){


								#if DEAD_LINKS_DEBUG
								DEBUG_MSG("FAILED OVER["<<link->socket->getName()
										  << ":"<<link->hostname<<":"
										  << link->address.toString()
										  << "], to alternate path via ["
										  << addrIter->second->socket->getName()
										  << ":" << addrIter->second->hostname
										  <<":"<< addrIter->second->address.toString() << endl);
								#endif

								link->socket = addrIter->second->socket;
								break;
							}
						}
					}

					if(trunkIter == link->trunkList.end()){

					}
				}
				else{
					#if DEAD_LINKS_DEBUG
					DEBUG_MSG("REMOVING["<<link->socket->getName()<<":"<<link->hostname<<":"<<link->address.toString() << "], no alternate path"<<endl);
					#endif
					deleteLink(link);
				}
			}
		}
	}
}

void si_node::deleteLink(link_info* link){
	removeMapEntries(link);

	//Remove link from links vector
	vector<link_info*>::iterator iter = find(links.begin(), links.end(), link);
	links.erase(iter);

	//Clean up socket map
	multimap<si_socket*, link_info*>::iterator beginIter = socketMap.lower_bound(link->socket);
	multimap<si_socket*, link_info*>::iterator endIter = socketMap.upper_bound(link->socket);

	for(; beginIter!=endIter; beginIter++){
		if(beginIter->second == link){
			socketMap.erase(beginIter);
			break;
		}
	}

	delete link;
	rebuildAnnouncement();
}

void si_node::removeMapEntries(link_info* link){

	//Remove all address link map entries that point to this link
	map<raw_address, link_info*>::iterator addrIter = addrLinkMap.begin();
	while(addrIter != addrLinkMap.end()){
		if(addrIter->second == link){
			addrLinkMap.erase(addrIter);
			addrIter = addrLinkMap.begin();
			continue;
		}

		addrIter++;
	}

	//Remove any announcement packets
	map<link_info*, raw_packet*>::iterator mapIter = announceTargets.find(link);
	if(mapIter != announceTargets.end()){
		if(mapIter->second != NULL){
			delete(mapIter->second);
		}
		announceTargets.erase(mapIter);
	}

	if(link->address.buffer != NULL){
		//Remove secondary addresses we were allocated by this host
		list<raw_address*>::iterator altIter = addressList.begin();
		while(altIter != addressList.end()){
			if((*altIter) != NULL){
				uint8_t type = raw_address::getRelationship(link->address.buffer, (*altIter)->buffer);

				if(type == UPLINK){
					//Clean up
					list<raw_address*>::iterator tempIter = altIter;

					delete (*tempIter);
					addressList.erase(tempIter);

					altIter = addressList.begin();
					continue;

					//NOTE: Announcement rebuild is needed this should already be done
					//		by the calling function so no need to worry.
				}
			}
			altIter++;
		}
	}

	//Clean routing maps
	if(link->type==UNCONFIGURED){
		//Do Nothing
	}

	if((link->type&UPLINK) != 0 && link->address.chunks.size() > 0){
		uint16_t lastValue = link->address.chunks.at(link->address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=upMap.find(lastValue);
		if(linkIter!=upMap.end()){
			upMap.erase(linkIter);
		}
	}

	if((link->type&TRUNK) != 0 && link->address.chunks.size() > 1){
		uint16_t lastValue = link->address.chunks.at(link->address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=trunkMap.find(lastValue);
		if(linkIter!=trunkMap.end()){
			trunkMap.erase(linkIter);
		}
	}

	if((link->type&DOWNLINK) != 0 && link->address.chunks.size() > 1){
		uint16_t lastValue = link->address.chunks.at(link->address.chunks.size()-2).value;
		map<uint16_t, link_info*>::iterator linkIter=downMap.find(lastValue);
		if(linkIter!=downMap.end()){
			downMap.erase(linkIter);
		}
	}

	if((link->type&INTERNAL) != 0){
		map<si_socket*, link_info*>::iterator linkIter=internalLinks.find(link->socket);
		if(linkIter!=internalLinks.end()){
			internalLinks.erase(linkIter);
		}
	}
}


void si_node::setConsumeCallback(void (*funcPtr)(raw_packet*)){
	consumeCallback=funcPtr;
}

void si_node::setNewLinkCallback(void (*funcPtr)(link_info*)){
	newLinkCallback=funcPtr;
}

void si_node::setDeadLinkCallback(void (*funcPtr)(link_info*)){
	deadLinkCallback=funcPtr;
}

void si_node::setAltLinkCallback(void (*funcPtr)(link_info*,link_info*)){
	usingAltPathCallback=funcPtr;
}

void si_node::setAddressChangeCallback(void (*funcPtr)(si_address,si_address)){
	addressChangeCallback=funcPtr;
}

void si_node::setOrphanedCallback(void (*funcPtr)(bool)){
	orphanedCallback=funcPtr;
}

void si_node::setMissedHelloCallback(void (*funcPtr)(link_info*)){
	missedHelloCallback=funcPtr;
}


void si_node::reportPacket(si_packet *packet){
	stringstream stream;


	stream<<"<packet from=\""<<packet->getSource().toString()<<"\" to=\""<<packet->getDestination().toString()<<"\" size=\""<<packet->byteCount()<<"\"";
	if(packet->rx_socket!=NULL){
		stream<<" rxSocket=\""<<packet->rx_socket->getName()<<"\"";
	}
	if(packet->tx_socket!=NULL){
		stream<<" txSocket=\""<<packet->tx_socket->getName()<<"\"";
	}
	stream<<">"<<endl;

	packet->timer.toXml(stream);

	stream<<"<actions count=\""<<packet->actionLog.size()<<"\">"<<endl;
	for(uint32_t i=0; i<packet->actionLog.size(); i++){
		stream<<"<action>"<<packet->actionLog.at(i)<<"</action>"<<endl;
	}
	stream<<"</actions>"<<endl;
	stream<<"</packet>"<<endl;


	INFO(stream.str());

	/*cout<<endl<<"START_PACKET_REPORT"<<endl;
	cout<<"Recieved from "<<packet->getSource().toString();
	if(packet->rx_socket != NULL){
		cout<<" on interface "<<packet->rx_socket->getName();
	}
	cout<<endl;
	cout<<"Destination is "<<packet->getDestination().toString()<<endl;
	if(packet->tx_socket != NULL){
		cout<<"Sent out on interface "<<packet->tx_socket->getName()<<endl;
	}
	cout<<"\t--Time Used--"<<endl;
	packet->timer.print();

	cout<<"\t--ACTIONS TAKEN--"<<endl;
	for(uint32_t i=0; i<packet->actionLog.size(); i++){
		cout<<packet->actionLog.at(i)<<endl;
	}
	cout<<"END_PACKET_REPORT"<<endl<<endl;*/
}

void si_node::reportRouteRate(){
	float MBytes=(float)byteCount/(1024*1024);

	timeval time=runloop.getElapsedReal();
	float ms=(((float)time.tv_sec*1000) + ((float)time.tv_usec/1000));
	float mb_per_sec=((float)MBytes/ms)*8000;
	avgRate=(mb_per_sec+avgRate)/2;


	PRINT_MSG(packetCount<<" packets\t"<<ms<<"ms\t"<<MBytes<<" MBytes\t"<<mb_per_sec<<"MBits/sec\t"<<avgRate<<"MBits/sec"<<endl);
	INFO("Links="<<links.size()<<" UpLinks="<<upMap.size()<<" DownLinks="<<downMap.size()<<" TrunkLinks="<<trunkMap.size()<<" InteralLinks="<<internalLinks.size()<<endl);
}


/**
  *  This Displays the Specified Routing Table
  */
void si_node::displayRoutingTable(string Table_name, map<uint16_t, link_info*> *Map)
{
	if(Map->size()!=0)
	{
		APPEND_MSG(Table_name<<"\t ROUTING TABLE \t"<<endl<<endl);

		map<uint16_t,link_info*>::iterator linkIter=Map->begin();
		while(linkIter!=Map->end())
		{
			APPEND_MSG("Interface Name-->\t"<<linkIter->second->socket->getName()<<"|"<<"Hostname-->\t"<<linkIter->second->hostname<<endl);

			linkIter++;
		}
	}
}

void si_node::printLinkState(){
	vector<link_info*>::iterator linkIter = links.begin();

	INFO(endl<<"Have " << links.size() << " links local_address=" << raw_address::toString(localAddressBuffer));
	if(addressList.size() > 0){
		APPEND_MSG(" AKA=" << addressListToString(addressList));
	}

	APPEND_MSG(" orphaned=" << boolToString(orphaned) << endl);

	for(; linkIter != links.end(); linkIter++){
		(*linkIter)->printInfo();
	}
}

void si_node::printRoutingMaps(){
	INFO("socketMap - " << socketMap.size() << " entries" << endl);
	multimap<si_socket*, link_info*>::iterator sockMapIter = socketMap.begin();
	for(; sockMapIter != socketMap.end(); sockMapIter++){
		APPEND_MSG("\t" << sockMapIter->first->toString() << "\t");

		if(sockMapIter->second == NULL){
			APPEND_MSG("NULL"<<endl);
		}
		else{
			APPEND_MSG(sockMapIter->second->getName() << endl);
		}
	}

	INFO("addrLinkMap - " << addrLinkMap.size() << " entries" << endl);
	map<raw_address, link_info*>::iterator addrIter = addrLinkMap.begin();
	for(; addrIter != addrLinkMap.end(); addrIter++){
		APPEND_MSG("\t" << addrIter->first.toString() << "\t");

		if(addrIter->second == NULL){
			APPEND_MSG("NULL"<<endl);
		}
		else{
			APPEND_MSG(addrIter->second->getName() << endl);
		}

	}

	INFO("upMap - " << upMap.size() << " entries" << endl);
	printRoutingMap(upMap);

	INFO("trunkMap - " << trunkMap.size() << " entries" << endl);
	printRoutingMap(trunkMap);

	INFO("downMap - " << downMap.size() << " entries" << endl);
	printRoutingMap(downMap);

	INFO("internalLinks - " << internalLinks.size() << " entries" << endl);
	map<si_socket*, link_info*>::iterator listIter = internalLinks.begin();
	for(; listIter != internalLinks.end(); listIter++){
		if(listIter->second == NULL){
			APPEND_MSG("\tNULL"<<endl);
		}
		else{
			APPEND_MSG("\t"<<listIter->second->getName()<<endl);
		}
	}
}

void si_node::printRoutingMap(map<uint16_t, link_info*> table){
	map<uint16_t, link_info*>::iterator mapIter = table.begin();

	cout.unsetf(ios::dec);
	cout.setf(ios::hex);

	for(; mapIter != table.end(); mapIter++){
		APPEND_MSG("\t0x" << mapIter->first << "\t");

		if(mapIter->second == NULL){
			APPEND_MSG("NULL"<<endl);
		}
		else{
			APPEND_MSG(mapIter->second->getName() << endl);
		}
	}

	cout.unsetf(ios::hex);
	cout.setf(ios::dec);
}

/**
  * Return time difference bewteen Initial and intermediate intervals. this has been implemented in CLASS Timer however this
  * doesnt need a timer object and the function returns value in milliseconds
  *
  */
float si_node::calcaulateTime(timeval initial, timeval intermediate)
{
    struct timeval result;


    timersub(&intermediate,&initial,&result);


     float time_in_milliseconds = (((float)result.tv_sec*1000) + ((float)result.tv_usec/1000));
     return time_in_milliseconds;

}

/**
  *  Function to check whether  node convergance  has taken place or not
  */

void si_node::checkInitialConvergence(string table_name, struct routing_statistics* table, map<uint16_t, link_info*> *Map)
{
    if(table->table_size!=Map->size())
    {

        table->table_changed=true;
        table->table_size=Map->size();

        table->routing_table_counter=0;
        table->initial_convergance=false;
        table->convergance=false;
    }
    // If Table size has not changed and routing_table_counter is not 3
    // Increment counter
    // Check condition

    else if(table->table_size == Map->size())
    {

        if (table->routing_table_counter==3 && table->convergance_counter<1)
        {
            gettimeofday(&table->converganceTime,NULL);

            float time = calcaulateTime(start_Routing_Time,table->converganceTime);
            cout<<endl;

            if(Map->size()!=0)
            //cout<<endl;
            cout<<"\t INITIAL CONVERGANCE OF"<<table_name<<" has taken place after \t"<<time<<"milliseconds \t "<<"Size-->"<<Map->size()<<endl;




            table->initial_convergance=true;
            table->convergance=true;
            table->convergance_counter++;



            table->lastconvergence.tv_sec=table->converganceTime.tv_sec;

            table->lastconvergence.tv_usec=table->converganceTime.tv_usec;

        }
		else if(table->convergance_counter >= 1 && table->routing_table_counter==3)
        {

            //cout<<"CONVERGANCE COUNTER"<<table->convergance_counter<<endl;
            //cout<<"ROUTING TABLE COUNTER"<<table->routing_table_counter<<endl;
            struct timeval temp;
            gettimeofday(&temp,NULL);
            float time= calcaulateTime(table->lastconvergence,temp);
            if(Map->size()!=0)
            cout<<"Convergance of "<<table_name<<"has taken place after \t"<<time<<"milliseconds \t "<<"Size"<<Map->size()<<endl;
            table->lastconvergence.tv_sec=temp.tv_sec;
            table->lastconvergence.tv_usec=temp.tv_usec;
			table->convergance_counter++;

		}


		table->routing_table_counter++;



		table->table_changed=false;
	}
}

/**
  * Returns InitialConvergance value
  */

bool si_node::reportInitialConvergance(struct routing_statistics *table)
{
	return table->initial_convergance;
}


/**
  * Initialises the Routing table statistics data associated with each Routing Table
  */

void si_node::InitializeRoutingStats(struct routing_statistics* table)
{
	// cout<<"Initializng table statistics"<<endl;

	table->table_size=0;
	table->table_changed=false;
	table->initial_convergance=false;
	table->convergance_counter=0;
}
