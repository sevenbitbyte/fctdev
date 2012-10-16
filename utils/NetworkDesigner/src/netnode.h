#ifndef NETNODE_H
#define NETNODE_H

#include <QtCore>
#include <QObject>

#include "netlink.h"
#include "networkdesc.h"
#include <rawutils.h>

class NetLink;
class NetworkDesc;

class NetNode : public QObject
{
    Q_OBJECT
	public:
		explicit NetNode(QString name, NetworkDesc *parentNode);

		void addLink(NetLink* link);
		void setAddress(raw_address addr, bool emitSignal = true);
		void setAutoAddress(bool state);

		void setMmtEnable(bool state);
		void setMmtUid(int id);

		QString getName() const;
		raw_address getAddress() const;
		QList<raw_address> getSecondaryAddresses() const;
		QList<NetLink*> getLinks() const;
		QList<NetNode*> getNeighbors();
		QList<raw_address> getNeighborAddresses();

		int getMmtUid() const;

		bool isAutoAddress() const;
		bool isMmtEnabled() const;

		raw_address allocateChildAddress(NetNode* node);

	signals:
		void addressChanged();

	public slots:
		void updateAddress();
		void updateSecondaryAddresses();

	private:
		QString name;
		raw_address localAddress;
		QList<raw_address> secondaryAddresses;
		QList<NetLink*> links;
		NetNode* parentNode;
		NetworkDesc* network;
		bool autoAssign;
		bool mmtEnabled;
		int mmtUID;
};

#endif // NETNODE_H
