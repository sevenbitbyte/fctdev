#include <QtCore>
#include "networkdesc.h"

#include "iniparser.h"



NetworkDesc::NetworkDesc(QObject *parent) :
    QObject(parent)
{
	mmtEnabled = true;
}

NetworkDesc::~NetworkDesc(){
	clear();
}

void NetworkDesc::clear(){
	while(!links.empty()){
		QMap<QString, NetLink*>::iterator linkIter = links.begin();

		delete linkIter.value();
		links.erase(linkIter);
	}

	while(!nodes.empty()){
		QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();

		delete nodeIter.value();
		nodes.erase(nodeIter);
	}
}


QMap<QString, NetNode*> NetworkDesc::getNodeMap(){
	return nodes;
}

QMap<QString, NetLink*> NetworkDesc::getLinkMap(){
	return links;
}

QList<NetNode*> NetworkDesc::getNodes(){
	return nodes.values();
}

QList<NetLink*> NetworkDesc::getLinks(){
	return links.values();
}

void NetworkDesc::computeAddresses(){
	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		nodeIter.value()->updateAddress();
	}

	for(nodeIter=nodes.begin(); nodeIter != nodes.end(); nodeIter++){
		nodeIter.value()->updateSecondaryAddresses();
	}

	emit networkChanged();
}

bool NetworkDesc::parseNSFile(QString fileName){
	QString errorMsg;
	nsFileName = fileName;
	QFile file(fileName);
	file.open(QIODevice::ReadOnly);

	this->clear();

	if(file.isOpen() && file.isReadable()){
		qDebug() << "Parsing file[" << fileName << "]";

		int lineNum = 1;
		bool parseError = false;

		while(!file.atEnd() && !parseError){
			QByteArray line = file.readLine();

			QString lineStr(line.data());
			lineStr = lineStr.simplified();



			//qDebug() << "Line:" << lineNum <<"\tRead [" << lineStr <<"]";
			QStringList tokens = tokenize(lineStr);

			if(tokens.size() > 0){
				if(tokens.at(0) == "set"){

					if(tokens.size() < 4){
						QTextStream(&errorMsg) << "error:" << lineNum <<" - set command with too few arguments";
						parseError = true;
						break;
					}

					QString name = tokens.at(1);
					QString type = QString(tokens.at(3)).remove(']');

					if(isNameUsed(name)){
						//Variable name is invalid
						QTextStream(&errorMsg) << "error:"<<lineNum <<" - illegal redeclaration of variable ["<<name<<"]";
						parseError = true;
						break;
					}

					if(type == "node"){
						NetNode* node = new NetNode(name, this);
						nodes.insert(name, node);
						qDebug() << "Node = " << name;
					}
					else if(type == "duplex-link"){

						if(tokens.size() < 6){
							QTextStream(&errorMsg) << "error:" << lineNum <<" - too few arguments in duplex-link constructor";
							parseError = true;
							break;
						}

						NetNode* first = getNode(tokens.at(4));
						NetNode* second = getNode(tokens.at(5));

						if(first == NULL){
							//First node name is invalid
							QTextStream(&errorMsg) << "error:" << lineNum
												   <<" - duplex-link constructor first argument["
												   << tokens.at(4) << "] is not a valid node";
							parseError = true;
							break;
						}

						if(second == NULL){
							//Second node name is invalid
							QTextStream(&errorMsg) << "error:" << lineNum
												   <<" - duplex-link constructor second argument["
												   << tokens.at(5) << "] is not a valid node";
							parseError = true;
							break;
						}

						NetLink* link = new NetLink(name, first, second, this);
						links.insert(name, link);

						qDebug() << "link = " << name << "first: "<< tokens.at(4) <<" second: " << tokens.at(5);
					}
				}
			}

			lineNum++;
		}


		file.close();

		if(parseError){
			qWarning() << errorMsg;
		}

		computeAddresses();
		return !parseError;
	}

	QTextStream(&errorMsg) << "Faile to open file[" << fileName << "]";

	qWarning() << errorMsg;
	return false;
}


bool NetworkDesc::parseNetConf(QString fileName){
	qDebug() << "NetworkDesc::parseNetConf("<<fileName<<")";

	dictionary* dict = iniparser_load(fileName.toAscii().data());


	if(dict != NULL){
		QString errorMsg;
		int sections = iniparser_getnsec(dict);

		qDebug() << "Has " << sections << "sections";

		for(int i=0; i<sections; i++){
			QString nodeName = iniparser_getsecname(dict, i);
			qDebug() << "Section: " << nodeName;
			QString addrKey = QString(nodeName).append(":address");
			QString assignKey = QString(nodeName).append(":auto_assign");
			QString mmtEnabledKey = QString(nodeName).append(":mmt_enabled");
			QString mmtUidKey = QString(nodeName).append(":mmt_uid");

			QString addressStr = iniparser_getstring(dict, addrKey.toAscii().data(), NULL);
			bool autoAssign = iniparser_getboolean(dict, assignKey.toAscii().data(), NULL);
			bool mmtEA = (bool) iniparser_getboolean(dict, mmtEnabledKey.toAscii().data(), (int)mmtEnabled);
			int mmtUID = iniparser_getint(dict, mmtUidKey.toAscii().data(), -1);

			NetNode* node = getNode(nodeName);

			if(node == NULL){
				QTextStream(&errorMsg) << "error:" << " - no such node[" << nodeName << "]";
				break;
			}

			si_address addr(addressStr.toAscii().data());
			node->setAddress(raw_address(addr));
			node->setAutoAddress(autoAssign);
			node->setMmtEnable(mmtEA);
			node->setMmtUid(mmtUID);
		}

		iniparser_freedict(dict);

		if(errorMsg.size() > 0){
			qWarning() << errorMsg;
			return false;
		}

		computeAddresses();
		return true;
	}

	return false;
}


bool NetworkDesc::writeNetConf(QString fileName){
	QFile file(fileName);

	if(!file.open(QIODevice::WriteOnly)){
		qWarning() << "Export failed!";
		return false;
	}


	QTextStream stream(&file);

	QList<NetNode*> nodeList = getNodes();

	for(int i=0; i < nodeList.size(); i++){
		NetNode* node = nodeList[i];


		stream << "[" << node->getName() << "]\n";
		stream <<"address="<<node->getAddress().toString().c_str()<<"\n";
		stream <<"mmt_enabled="<<node->isMmtEnabled()<<"\n";
		stream <<"mmt_uid="<<node->getMmtUid()<<"\n";
		stream <<"auto_assign="<<node->isAutoAddress()<<"\n\n";
	}

	stream.flush();
	file.close();

	return true;
}


bool NetworkDesc::isAddressUsed(raw_address address){
	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		if(address == nodeIter.value()->getAddress()){
			return true;
		}
	}

	return false;
}

bool NetworkDesc::isNameUsed(QString varName){
	return links.contains(varName) || nodes.contains(varName);
}

bool NetworkDesc::isMmtUidUsed(int id){
	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		if(id == nodeIter.value()->getMmtUid()){
			return true;
		}
	}

	return false;
}

bool NetworkDesc::isMmtEnabled(){
	return mmtEnabled;
}

/**
  *	Returns a sorted list of all addresses that are in use.
  *
  *	@return	Ascending list of all used addresses
  */
QList<raw_address> NetworkDesc::getAddressList(){
	QMap<raw_address,raw_address> addresses;

	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		NetNode* node = nodeIter.value();

		//Use map insert to keep contents sorted
		addresses.insert(node->getAddress(), node->getAddress());
	}

	//Ensure we only retrieve unique keys
	return addresses.uniqueKeys();
}

QList<raw_address> NetworkDesc::getTopLevelAddresses(){
	uint16_t lowestTier = 0xffff;
	QList<raw_address> topAddresses;

	//Get sorted list of addresses
	QList<raw_address> allAddresses = getAddressList();

	//Search for top level addresses
	for(int i=0; i<allAddresses.count(); i++){
		uint16_t tierValue = allAddresses[i].tierLevel;

		if(tierValue < lowestTier){
			//Lowest tier seen thus far, clear old list contents
			lowestTier = tierValue;
			topAddresses.clear();
			topAddresses.push_back(allAddresses[i]);
		}
		else if(tierValue == lowestTier){
			topAddresses.push_back(allAddresses[i]);
		}
		else{
			//All remaining addresses should have great tier values
			break;
		}
	}

	return topAddresses;
}

/**
  *	Searches for the first node which has the specified address assigned to it
  *
  *	@param	address	Address of node to search for
  *	@return	Returns the first node found to match address
  *
  */
NetNode* NetworkDesc::getNode(raw_address address){
	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		if(address == nodeIter.value()->getAddress()){
			return nodeIter.value();
		}
	}

	return NULL;
}

/**
  *	Searches for nodes with the specified address
  *
  *	@param	address	Address to search for
  *	@return	Returns a QList of all of the matching NetNodes
  */
QList<NetNode*> NetworkDesc::getNodesByAddress(raw_address address){
	QList<NetNode*> matches;

	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		NetNode* node = nodeIter.value();

		if(node->getAddress() == address){
			matches.push_back(node);
		}
	}

	return matches;
}

/**
  *	Searches the set of nodes to find all nodes that are children of the
  *	specified address.
  *
  *	@param	parentAddr	Address of the parent cloud/node
  *
  *	@return	Returns a list of all logical downlinks of the specified parent address.
  */
QList<NetNode*> NetworkDesc::getChildNodes(raw_address parentAddr){
	//QList<NetNode*> children;

	QMultiMap<raw_address, NetNode*> children;

	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		NetNode* node = nodeIter.value();

		//raw_address::printAddressBytes(node->getAddress().buffer);
		raw_address address = node->getAddress();

		if(raw_address::getRelationship(address.buffer, parentAddr.buffer) == DOWNLINK){
			if(!children.values().contains(node)){
				children.insert(address, node);
			}
		}
	}

	return children.values();
}

QList<raw_address> NetworkDesc::getChildAddressList(raw_address parentAddr){
	QMap<raw_address, raw_address> children;

	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();
	for(; nodeIter != nodes.end(); nodeIter++){
		NetNode* node = nodeIter.value();

		raw_address address = node->getAddress();

		if(raw_address::getRelationship(address.buffer, parentAddr.buffer) == DOWNLINK){
			children.insert(address, address);
		}
	}

	return children.values();
}

QList<raw_address> NetworkDesc::getAddressesByTier(uint8_t tierLevel){
	QMap<raw_address,raw_address> addressMap;

	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();

	for(; nodeIter != nodes.end(); nodeIter++){
		raw_address address = nodeIter.value()->getAddress();
		if(address.tierLevel == tierLevel){
			addressMap.insert(address, address);
		}
	}

	return addressMap.keys();
}

QList<NetNode*> NetworkDesc::getNodesByTier(uint8_t tierLevel){
	QMap<raw_address,NetNode*> addressMap;

	QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();

	for(; nodeIter != nodes.end(); nodeIter++){
		NetNode* node = nodeIter.value();
		raw_address address = node->getAddress();
		if(address.tierLevel == tierLevel){
			addressMap.insert(address, node);
		}
	}

	return addressMap.values();
}

QMultiMap<uint8_t, NetNode*> NetworkDesc::getBranch(raw_address rootAddr){
	QMultiMap<uint8_t, NetNode*> branch;

	QList<NetNode*> roots = getNodesByAddress(rootAddr);

	for(int i=0; i<roots.count(); i++){
		branch.insert(rootAddr.tierLevel, roots[i]);
	}

	QList<NetNode*> children = getChildNodes(rootAddr);

	for(int i=0; i<children.count(); i++){
		NetNode* child = children[i];

		QMultiMap<uint8_t, NetNode*> childBranch = this->getBranch(child->getAddress());

		//Merge maps
		QMap<uint8_t, NetNode*>::iterator nodeIter = childBranch.begin();
		for(; nodeIter != childBranch.end(); nodeIter++){
			uint8_t key = nodeIter.key();
			NetNode* node = nodeIter.value();

			if(!branch.values(key).contains(node)){
				branch.insert(key, node);
			}
		}
	}

	return branch;
}

QMultiMap<uint8_t, raw_address> NetworkDesc::getBranchAddresses(raw_address rootAddr){
	QMultiMap<uint8_t, raw_address> branch;

	QList<NetNode*> roots = getNodesByAddress(rootAddr);

	for(int i=0; i<roots.count(); i++){
		if(branch.contains(rootAddr.tierLevel, rootAddr)){
			break;
		}

		branch.insert(rootAddr.tierLevel, rootAddr);
	}

	QList<NetNode*> children = getChildNodes(rootAddr);

	for(int i=0; i<children.count(); i++){
		NetNode* child = children[i];

		//if(!branch.values(key).contains(addr)){
			branch.insert(child->getAddress().tierLevel, child->getAddress());
		//}

		QMultiMap<uint8_t, raw_address> childBranch = this->getBranchAddresses(child->getAddress());

		//Merge maps
		QMap<uint8_t, raw_address>::iterator nodeIter = childBranch.begin();
		for(; nodeIter != childBranch.end(); nodeIter++){
			uint8_t key = nodeIter.key();
			raw_address addr = nodeIter.value();

			if(!branch.values(key).contains(addr)){
				branch.insert(key, addr);
			}
		}
	}

	return branch;
}


/*
QMultiMap<uint8_t, raw_address> NetworkDesc::getLogicalBranchAddresses(raw_address rootAddr){
	QMultiMap<uint8_t, raw_address> branch;

	QList<NetNode*> roots = getNodesByAddress(rootAddr);

	for(int i=0; i<roots.count(); i++){
		if(branch.contains(rootAddr.tierLevel, rootAddr)){
			break;
		}

		branch.insert(rootAddr.tierLevel, rootAddr);
	}

	QList<raw_address> children = getAddressesByTier(rootAddr.tierLevel + 1);

	for(int i=0; i<children.count(); i++){
		//NetNode* child = children[i];
		raw_address childAddr = children[i];

		if(raw_address::getRelationship(childAddr.buffer, rootAddr.buffer) != DOWNLINK){
			continue;
		}

		QMultiMap<uint8_t, raw_address> childBranch = this->getLogicalBranchAddresses(childAddr);

		//Merge maps
		QMap<uint8_t, raw_address>::iterator nodeIter = childBranch.begin();
		for(; nodeIter != childBranch.end(); nodeIter++){
			uint8_t key = nodeIter.key();
			raw_address addr = nodeIter.value();

			if(!branch.values(key).contains(addr)){
				branch.insert(key, addr);
			}
		}
	}

	return branch;
}
*/

QStringList NetworkDesc::tokenize(QString str){
	QStringList tokens = str.simplified().split(' ', QString::SkipEmptyParts);

	//QStringListIterator iter = tokens.begin();
	//int commentStart = -1;

	for(int i=0; i<tokens.size(); i++){

		//Check if this is the start of a comment
		if(tokens.at(i).at(0) == '#'){

			//Remove all comment text which follows
			while(tokens.size() > i){
				tokens.pop_back();
			}
			break;
		}

		//qDebug() << i << tokens.at(i);
	}



	return tokens;
}



NetNode* NetworkDesc::getNode(QString varName){
	NetNode* node = NULL;

	varName = varName.remove('$');

	if(varName.size() > 0){
		QMap<QString, NetNode*>::iterator iter = nodes.find(varName);

		if(iter != nodes.end()){
			node = iter.value();
		}
	}

	return node;
}
