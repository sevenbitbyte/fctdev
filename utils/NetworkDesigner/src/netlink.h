#ifndef NETLINK_H
#define NETLINK_H

#include <QtCore>
#include <QObject>
#include "netnode.h"

class NetNode;

class NetLink : public QObject
{
    Q_OBJECT
	public:
		explicit NetLink(QString name, NetNode* first, NetNode* second, QObject *parent = 0);

		NetNode* getFirst();
		NetNode* getSecond();
		NetNode* getOther(NetNode* node);
		QString getName();

	signals:

	public slots:

	private:
		QString _name;
		QMap<QString,QString> _properties;
		NetNode* _first;
		NetNode* _second;

};

#endif // NETLINK_H
