#include <QtCore>
#include "netnode.h"
#include <rawutils.h>

NetNode::NetNode(QString name, NetworkDesc *parent) :
    QObject(parent)
{
	this->name = name;
	autoAssign = true;
	parentNode = NULL;
	network = parent;
	mmtUID = 0;
	mmtEnabled = network->isMmtEnabled();

	if(mmtEnabled){
		//mmtUID = qrand();
		int id = 1;

		while(network->isMmtUidUsed(id)){
			id++;
		}

		mmtUID = id;
	}
}

void NetNode::addLink(NetLink *link){
	links.push_back(link);
}

void NetNode::setAddress(raw_address addr, bool emitSignal){
	localAddress = addr;
	localAddress.updateBuffer();

	qDebug() << "NetNode::setAddress - Name=" << name <<" address set to " << localAddress.toString().c_str();

	if(emitSignal){
		emit addressChanged();
	}
}


void NetNode::setAutoAddress(bool state){
	this->autoAssign = state;
}

void NetNode::setMmtEnable(bool state){
	mmtEnabled = state;

}

void NetNode::setMmtUid(int id){
	if(id < 0 ){
		mmtUID = 0;
		int id = 1;

		while(network->isMmtUidUsed(id)){
			id++;
		}

		mmtUID = id;
	}
	else{
		mmtUID = id;
	}
}

QString NetNode::getName() const {
	return name;
}

raw_address NetNode::getAddress() const {
	return raw_address(localAddress);
}

QList<raw_address> NetNode::getSecondaryAddresses() const {
	return secondaryAddresses;
}

QList<NetLink*> NetNode::getLinks() const {
	return links;
}

int NetNode::getMmtUid() const {
	return mmtUID;
}

bool NetNode::isAutoAddress() const {
	return autoAssign;
}

bool NetNode::isMmtEnabled() const {
	return mmtEnabled;
}

QList<NetNode*> NetNode::getNeighbors() {
	QList<NetNode*> neighbors;

	for(int i=0; i<links.size(); i++){
		NetLink* link = links[i];

		NetNode* n = link->getOther(this);

		if(n != NULL){
			neighbors.push_back(n);
		}
	}

	return neighbors;
}

QList<raw_address> NetNode::getNeighborAddresses() {
	QList<raw_address> addresses;

	for(int i=0; i<links.size(); i++){
		NetLink* link = links[i];

		NetNode* n = link->getOther(this);

		if(n != NULL){
			raw_address addr = n->getAddress();

			if(addr.chunks.size() > 0){
				addresses.push_back(addr);
			}
		}
	}

	return addresses;
}

void NetNode::updateAddress(){
	if(autoAssign){
		NetNode* p = NULL;
		uint8_t lowestBC = 0xff;

		secondaryAddresses.clear();

		if(parentNode != NULL){
			if(raw_address::getRelationship(parentNode->getAddress().buffer, localAddress.buffer) == UPLINK){
				p=parentNode;
				lowestBC = parentNode->getAddress().tierLevel;
			}
		}

		for(int i=0; i<links.size(); i++){
			NetLink* link = links[i];

			NetNode* n = link->getOther(this);

			if(n != NULL){
				raw_address addr = n->getAddress();

				if(addr.tierLevel < lowestBC && addr.tierLevel != 0){
					lowestBC = addr.tierLevel;
					p = n;
				}
			}
		}

		if(p != NULL){
			raw_address addrOffer = p->allocateChildAddress(this);

			//if(addrOffer.tierLevel <= localAddress.tierLevel || localAddress.tierLevel == 0){
			if(p->parentNode != this){
				//Generate address
				this->parentNode = p;

				setAddress(addrOffer);
			}
		}
		else{
			//si_address addrTemp("1.1");
			//localAddress = raw_address(addrTemp);

			//Get last address field
			address_chunk chunk;
			chunk.value = 0;
			uint16_t value = chunk.value;
			chunk.value = 1;
			chunk.size = 4;		//Number of bits used

			raw_address offerAddress;
			offerAddress.tierLevel = 1;
			offerAddress.chunks.push_back(chunk);
			offerAddress.chunks.push_back((address_chunk){0,0});

			qDebug() << "NetNode::updateAddress() - Trying address " << offerAddress.toString().c_str() << " at node="<<name;

			while(network->isAddressUsed(offerAddress) == true){
				//Generate next address
				++chunk;
				offerAddress.chunks[0] = chunk;


				if(chunk.value == value){
					qDebug() << "ERROR: Failed to select unique address for node "<<name;
					offerAddress = raw_address();
					break;
				}

				qDebug() << "NetNode::updateAddress() - Trying address " << offerAddress.toString().c_str() << " at node="<<name;
			}

			offerAddress.updateBuffer();
			this->parentNode = NULL;
			setAddress(offerAddress);
		}
	}
}

void NetNode::updateSecondaryAddresses(){
	secondaryAddresses.clear();

	for(int i=0; i<links.size(); i++){
		NetLink* link = links[i];

		NetNode* n = link->getOther(this);
		raw_address otherAddr = n->getAddress();

		uint8_t type = raw_address::getRelationship(otherAddr.buffer,localAddress.buffer);

		if(type == (IMPROPER|UPLINK) || type== (IMPROPER|TRUNK)){
			raw_address offered = n->allocateChildAddress(this);

			if(!secondaryAddresses.contains(offered)){
				secondaryAddresses.push_back(offered);
			}
		}
	}
}

raw_address NetNode::allocateChildAddress(NetNode* node){
	QList<raw_address> addresses = getNeighborAddresses();


	//Get last address field
	address_chunk chunk;
	chunk.value = 0;
	uint16_t value = chunk.value;


	raw_address offerAddress(localAddress);
	offerAddress.tierLevel++;
	offerAddress.chunks.insert(offerAddress.chunks.end()-1, chunk);

	bool checkVal = true;

	while(checkVal){
		checkVal = false;

		//Generate next address
		++chunk;
		offerAddress.chunks[offerAddress.chunks.size()-2] = chunk;

		if(network->isAddressUsed(offerAddress) == true && network->getNode(offerAddress) != node){
			checkVal = true;
		}
		else{
			checkVal = false;
		}

		/*for(int i=0; i<addresses.size(); i++){
			raw_address addr = addresses[i];

			if(raw_address::getRelationship(addr.buffer, localAddress.buffer) == DOWNLINK){

				if(offerAddress == addr){
					checkVal = true;
					break;
				}
			}
		}*/

		if(value == chunk.value){
			qDebug() << "ERROR: Failed to allocate child address from node "<<name;
			return raw_address();
		}
	}

	offerAddress.updateBuffer();
	return offerAddress;
}
