#ifndef NETWORKGRAPHICS_H
#define NETWORKGRAPHICS_H

#include <QtCore>
#include <QtGui>

#include <rawutils.h>

#include "netlink.h"
#include "netnode.h"
#include "networkdesc.h"

#define CLOUD_X_MARGIN	10
#define CLOUD_Y_MARGIN	10


class GraphicNetNode : public QGraphicsObject
{
	Q_OBJECT
	public:
		GraphicNetNode(NetNode* node, QGraphicsItem* parent = 0);
		~GraphicNetNode();

		void hideAddress(bool enable);
		void hideHostname(bool enable);

		void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

		QRectF boundingRect() const;

		void updatePositions();

	public slots:
		void updateAddress();

	private:
		bool renderAddress;
		bool renderHostname;

		NetNode* node;
		//QString addressText;
		//QString* hostnameText;
		QGraphicsSimpleTextItem* addressText;
		QGraphicsSimpleTextItem* secondAddrsText;
		QGraphicsSimpleTextItem* hostnameText;
		//Pixmap* icon;
		QGraphicsPixmapItem* icon;
};

class GraphicNetCloud : public QGraphicsObject {
	Q_OBJECT
	public:
		GraphicNetCloud(raw_address address, QList<GraphicNetNode*> nodes, QGraphicsItem* parent = 0);
		~GraphicNetCloud();

		void removeNode(GraphicNetNode* node);
		void addNode(GraphicNetNode* node);

		bool hasNode(GraphicNetNode* node);

		int getNodeCount();

		raw_address getAddress();
		QList<GraphicNetNode*> getGraphicNodes();

		void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

		void updatePositions();
		QRectF boundingRect() const;

	private:
		QList<GraphicNetNode*> nodes;
		QGraphicsSimpleTextItem* addressText;
		QGraphicsPixmapItem* icon;
		raw_address address;
};



class NetworkGraphics : public QObject
{
	Q_OBJECT
	public:
		NetworkGraphics(NetworkDesc* network, QGraphicsScene* scene, QObject* parent = 0);
		~NetworkGraphics();

		void updateSelection(QList<NetNode*> nodes);

	protected:
		GraphicNetCloud* getCloud(NetNode* node);
		QList<GraphicNetCloud*> getClouds(QList<NetNode*> nodes);

		GraphicNetCloud* getCloudByAddress(raw_address cloud);
		QList<GraphicNetCloud*> getCloudsByAddress(QList<raw_address> addrList);

		void deleteCloud(GraphicNetCloud* cloud);

		QSizeF computeSize(raw_address branchAddr);
		QList<GraphicNetNode*> getNodesByAddress(raw_address addr);

		bool positionBranch(raw_address root, QPointF position);

	public slots:
		void rebuildNetwork();
		void updateNetwork();

	private:
		QGraphicsScene* scene;
		NetworkDesc* network;

		QMap<NetNode*, GraphicNetNode*> nodeMap;
		QMap<NetLink*, QGraphicsLineItem*> linkMap;
		QMap<GraphicNetNode*, GraphicNetCloud*> cloudMap;
		QMap<raw_address, GraphicNetCloud*> cloudAddrMap;
};

#endif // NETWORKGRAPHICS_H
