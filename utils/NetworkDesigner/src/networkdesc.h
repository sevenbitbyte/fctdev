#ifndef NETWORKDESC_H
#define NETWORKDESC_H

#include <QtCore>
#include <QObject>

#include "netnode.h"
#include "netlink.h"
#include <rawutils.h>

class NetNode;
class NetLink;

class NetworkDesc : public QObject
{
    Q_OBJECT
	public:
		explicit NetworkDesc(QObject *parent = 0);
		//NetworkDesc(NetworkDesc& other, QObject *parent=0);
		~NetworkDesc();

		void clear();
		bool parseNSFile(QString fileName);
		bool parseNetConf(QString fileName);

		bool writeNetConf(QString fileName);

		bool isAddressUsed(raw_address address);
		bool isMmtUidUsed(int id);
		bool isMmtEnabled();

		QList<raw_address> getAddressList();
		QList<raw_address> getTopLevelAddresses();

		QMap<QString, NetNode*> getNodeMap();
		QMap<QString, NetLink*> getLinkMap();

		QList<NetNode*> getNodes();
		QList<NetLink*> getLinks();

		void computeAddresses();

		NetNode* getNode(QString varName);
		NetNode* getNode(raw_address address);
		QList<NetNode*> getNodesByAddress(raw_address address);

		QList<NetNode*> getChildNodes(raw_address parentAddr);
		QList<raw_address> getChildAddressList(raw_address parentAddr);
		QList<NetNode*> getNodesByTier(uint8_t tierLevel);
		QList<raw_address> getAddressesByTier(uint8_t tierLevel);

		QMultiMap<uint8_t, NetNode*> getBranch(raw_address rootAddr);

		QMultiMap<uint8_t, raw_address> getBranchAddresses(raw_address rootAddr);

	protected:
		QStringList tokenize(QString str);
		bool isNameUsed(QString varName);

	signals:
		void networkChanged();

	public slots:

	private:
		QMap<QString, NetNode*> nodes;
		QMap<QString, NetLink*> links;

		QString nsFileName;
		QString netConfFileName;
		bool mmtEnabled;
};

#endif // NETWORKDESC_H
