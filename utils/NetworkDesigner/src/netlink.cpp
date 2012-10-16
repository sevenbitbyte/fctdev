#include <QtCore>
#include "netlink.h"

NetLink::NetLink(QString name, NetNode* first, NetNode* second, QObject *parent) :
    QObject(parent)
{
	_name = name;
	_first = first;
	_second = second;

	_first->addLink(this);
	_second->addLink(this);
}

NetNode* NetLink::getFirst(){
	return _first;
}

NetNode* NetLink::getSecond(){
	return _second;
}

NetNode* NetLink::getOther(NetNode* node){
	if(_first == node){
		return _second;
	}

	return _first;
}

QString NetLink::getName() {
	return _name;
}
