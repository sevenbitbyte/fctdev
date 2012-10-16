#include "debug.h"
#include "mmt.h"
#include "packets.h"

#define MMT_DEBUG	true

vid_t vid_treeNode::getVid(){
	vid_t vid;

	if(parent != NULL){
		vid = parent->getVid();
	}

	vid.push_back(digit);

	return vid;
}


mmt_info::mmt_info(){
	mmtType=MMT_Relay;
	uid=0;
	link=NULL;
}

mmt_info::mmt_info(uint16_t u){
	mmtType=MMT_Relay;
	uid=u;
	link=NULL;
}

mmt_info::mmt_info(uint8_t type, uint16_t id, uint8_t* vidListPtr, link_info* link){
	mmtType = type;
	uid = id;
	vidList = parseVidList(vidListPtr);
	this->link = link;
}

void mmt_info::printInfo(){
	INFO(endl);

	cout.unsetf(ios::dec);
	cout.setf(ios::hex);

	APPEND_MSG("[mmt_info][uid=" << uid << "]" << endl);
	APPEND_MSG("\ttype=" << mmtTypeToString(mmtType) << endl);
	APPEND_MSG("\tvids=" << vidListToString(vidList) << endl);
	APPEND_MSG("\tneighbors=" << addressListToString(neighbors) << endl);

	cout.unsetf(ios::hex);
	cout.setf(ios::dec);
}

vid_treeNode::~vid_treeNode(){
	if(parent!=NULL){
		map<uint8_t, vid_treeNode*>::iterator nodeIter=parent->children.find(digit);
		if(nodeIter!=parent->children.end()){
			parent->children.erase(nodeIter);
		}
	}

	while(!children.empty()){
		map<uint8_t, vid_treeNode*>::iterator nodeIter=children.begin();
		delete nodeIter->second;
		//children.erase(nodeIter);	//Not needed
	}
}

mmt_tables::~mmt_tables(){
	//Free all vid tree nodes
	while(!vidRoots.empty()){
		map<uint8_t, vid_treeNode*>::iterator nodeIter=vidRoots.begin();
		delete nodeIter->second;
		vidRoots.erase(nodeIter);
	}

	//Free all mmt info structures
	while(!uidMap.empty()){
		map<uint16_t, mmt_info*>::iterator nodeIter=uidMap.begin();
		delete nodeIter->second;
		uidMap.erase(nodeIter);
	}
}

mmt_info* mmt_tables::getInfoByUid(uint16_t id){
	map<uint16_t,mmt_info*>::iterator iter = uidMap.find(id);

	if(iter != uidMap.end()){
		return iter->second;
	}

	return NULL;
}

mmt_info* mmt_tables::getInfoByVid(vid_t vid){
	vid_treeNode* current = NULL;

	map<uint8_t, vid_treeNode*>::iterator iter = vidRoots.find(vid[0]);

	if(iter == vidRoots.end()){
		//Terminate search
		return NULL;
	}
	else{
		current = iter->second;
	}

	for(uint i=1; i<vid.size(); i++){
		iter = current->children.find(vid[i]);

		if(iter == vidRoots.end()){
			//Terminate search
			return NULL;
		}

		//Move to next branch
		current = current->children[vid[i]];
	}

	return current->info;
}

mmt_info* mmt_tables::getInfoByRawVid(uint8_t* bufferPtr){
	uint8_t size=bufferPtr[0];
	vid_treeNode* current = NULL;

	map<uint8_t, vid_treeNode*>::iterator iter = vidRoots.find(bufferPtr[1]);

	if(iter == vidRoots.end()){
		//Terminate search
		return NULL;
	}
	else{
		current = iter->second;
	}

	for(uint i=1; i<size; i++){
		iter = current->children.find(bufferPtr[i]);

		if(iter == vidRoots.end()){
			//Terminate search
			return NULL;
		}

		//Move to next branch
		current = current->children[bufferPtr[i]];
	}

	return current->info;
}

mmt_info* mmt_tables::getInfoByRawVidList(uint8_t* listPtr){
	uint8_t vidCount = listPtr[0];
	int offset = 1;

	mmt_info* info = NULL;

	for(uint i=0; i<vidCount; i++){
		info = getInfoByRawVid(&listPtr[offset]);

		if(info != NULL){
			break;
		}

		//Update byte offset
		offset += listPtr[offset] + 1;
	}

	return info;
}

mmt_info* mmt_tables::getNearestNodeByVid(vid_t vid){
	vid_treeNode* current = NULL;
	mmt_info* info = NULL;

	map<uint8_t, vid_treeNode*>::iterator iter = vidRoots.find(vid[0]);

	if(iter == vidRoots.end()){
		//Terminate search
		return NULL;
	}
	else{
		current = iter->second;
		info = current->info;
	}

	for(uint i=1; i<vid.size(); i++){
		iter = current->children.find(vid[i]);

		if(iter == vidRoots.end()){
			//Terminate search
			return info;
		}

		//Move to next branch
		current = current->children[vid[i]];

		if(current->info != NULL){
			info = current->info;
		}
	}

	return info;
}

mmt_info* mmt_tables::getNearestNodeByRawVid(uint8_t* bufferPtr){
	uint8_t size=bufferPtr[0];
	vid_treeNode* current = NULL;
	mmt_info* info = NULL;

	map<uint8_t, vid_treeNode*>::iterator iter = vidRoots.find(bufferPtr[1]);

	if(iter == vidRoots.end()){
		//Terminate search
		return NULL;
	}
	else{
		current = iter->second;
		info = current->info;
	}

	for(int i=1; i<size; i++){
		iter = current->children.find(bufferPtr[i]);

		if(iter == vidRoots.end()){
			//Terminate search
			return info;
		}

		//Move to next branch
		current = current->children[bufferPtr[i]];

		if(current->info != NULL){
			info = current->info;
		}
	}

	return info;
}


bool mmt_tables::insertNode(mmt_info* info){
	mmt_info* temp = getInfoByUid(info->uid);

	if(temp == NULL){
		//Insert into uid map
		uidMap.insert(make_pair(info->uid, info));

		return updateNodeVids(info);
	}
	else if(temp == info){
		return true;
	}

	//There exists another mmt_info with the same UID
	return false;
}

bool mmt_tables::updateNodeVids(mmt_info* info){
	if(info==NULL){
		return false;
	}

	#if MMT_DEBUG
	vector<struct vid_treeNode*> vids = info->vids;
	#endif

	info->vids.clear();

	for(uint i=0; i<info->vidList.size(); i++){
		info->vids.push_back(createVidNode(info->vidList[i]));
	}

	#if MMT_DEBUG
	for(uint i=0; i<vids.size(); i++){
		bool found = false;
		vid_treeNode* node = vids[i];

		//Search for node in new set
		for(uint j=0; j<info->vids.size(); j++){
			if(info->vids[j] == node){
				found = true;
				break;
			}
		}

		if(!found){
			DEBUG_MSG("mmt_info.uid="<<info->uid << " Removed vid=" << vidToString(node->getVid()));
		}
	}
	#endif

	return true;
}

bool mmt_tables::updateNodeVids(uint16_t id, uint8_t* listPtr){
	mmt_info* info = getInfoByUid(id);

	if(info==NULL){
		return false;
	}

	info->vidList = parseVidList(listPtr);

	return updateNodeVids(info);
}

vid_treeNode* mmt_tables::createVidNode(vid_t vid){
	vid_treeNode* current = NULL;

	map<uint8_t, vid_treeNode*>::iterator iter = vidRoots.find(vid[0]);

	if(iter == vidRoots.end()){
		current = new vid_treeNode;
		current->depth = 0;
		current->digit = vid[0];
		current->info = NULL;
		current->parent = NULL;
		current->state = 0;

		vidRoots.insert(make_pair(vid[0], current));
	}
	else{
		current = iter->second;
	}

	for(uint i=1; i<vid.size(); i++){
		iter = current->children.find(vid[i]);

		if(iter == current->children.end()){
			//Build branch
			vid_treeNode* temp = new vid_treeNode;

			temp->parent = current;
			temp->depth = current->depth + 1;
			temp->digit = vid[i];
			temp->info = NULL;
			temp->state = 0;

			current->children.insert(make_pair(vid[i], temp));
		}

		//Move to next branch
		current = current->children[vid[i]];
	}

	return current;
}


vid_treeNode* mmt_tables::createVidNode(uint8_t* bufferPtr){
	uint8_t size=bufferPtr[0];
	vid_treeNode* current = NULL;

	map<uint8_t, vid_treeNode*>::iterator iter = vidRoots.find(bufferPtr[1]);

	if(iter == vidRoots.end()){
		current = new vid_treeNode;
		current->depth = 0;
		current->digit = bufferPtr[1];
		current->info = NULL;
		current->parent = NULL;
		current->state = 0;

		vidRoots.insert(make_pair(bufferPtr[1], current));
	}
	else{
		current = iter->second;
	}

	for(int i=1; i<size; i++){
		iter = current->children.find(bufferPtr[i]);

		if(iter == current->children.end()){
			//Build branch
			vid_treeNode* temp = new vid_treeNode;

			temp->parent = current;
			temp->depth = current->depth + 1;
			temp->digit = bufferPtr[i+1];
			temp->info = NULL;
			temp->state = 0;

			current->children.insert(make_pair(bufferPtr[i], temp));
		}

		//Move to next branch
		current = current->children[bufferPtr[i]];
	}

	return current;
}



/*mmt_node::mmt_node(si_address addr, uint16_t id):si_node(addr){
	uid=id;
}


bool mmt_node::isMMTEnabled(si_socket* interface){
	if(interface==NULL){
		return false;
	}

	map<si_socket*, link_info*>::iterator iter= socketMap.find(interface);
	if(iter != socketMap.end()){
		return iter->second->mmt_enabled;
	}
	return false;
}

void mmt_node::encapsulatePacket(si_socket* packet){
	//Determine which MMT node to transmit to
}

bool mmt_node::routePacket(raw_packet* packet){
	if(packet==NULL){
		packet=&routeBuf;
	}

	if(packet->readPacketType()!=PROTO_MMT){	//Route SI Packet
		return si_node::routePacket(packet);
	}

	//Route MMT packet
	uint8_t* destAddrPtr=packet->destinationAddrPtr();

	//All mmt packets should be sent via linkLocal
	if(!raw_address::compareAddress(destAddrPtr, (uint8_t*)raw_linkLocal)){
		DEBUG_MSG("Droppoing MMT packet with destAddr!=LinkLocal"<<endl);
		return false;
	}

	ERROR("MMT Routing not implemented"<<endl);
	return false;
}

void mmt_node::consumePacket(raw_packet *packet){
	if(packet->readPacketType()!=PROTO_MMT){
		si_node::consumePacket(packet);
		return;
	}

	uint8_t* dataPtr=packet->dataPtr();
	uint16_t siPayloadLength=packet->readPayloadLength();
	uint8_t mmtType=dataPtr[packet->length-1];
	uint8_t mmtDataLength=dataPtr[packet->length-2];
	uint8_t* mmtDataPtr=dataPtr+packet->length-(mmtDataLength+2);


}*/
