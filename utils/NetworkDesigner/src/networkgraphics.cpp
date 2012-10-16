#include <math.h>
#include "networkgraphics.h"

GraphicNetNode::GraphicNetNode(NetNode* node, QGraphicsItem* parent) :
	QGraphicsObject(parent)
{
	this->node = node;
	//addressText = node->getAddress().toString().c_str();
	//hostnameText = node->getName();
	addressText = new QGraphicsSimpleTextItem(node->getAddress().toString().c_str(), this);
	hostnameText = new QGraphicsSimpleTextItem(node->getName(), this);
	secondAddrsText = new QGraphicsSimpleTextItem(" ", this);

	QBrush blueBrush(Qt::blue);
	secondAddrsText->setBrush(blueBrush);

	QBrush brush(Qt::black);
	addressText->setBrush(brush);
	hostnameText->setBrush(brush);

	QPixmap pic(":/media/serverIcon");
	icon = new QGraphicsPixmapItem(pic, this);
	//icon->scale(0.75, 0.75);

	addressText->setVisible(true);
	hostnameText->setVisible(true);
	icon->setVisible(true);

	QFont textFont;
	textFont.setPixelSize(20);

	addressText->setFont(textFont);
	hostnameText->setFont(textFont);
	secondAddrsText->setFont(textFont);

	updateAddress();
}

GraphicNetNode::~GraphicNetNode(){
	//qDebug() << "GraphicNetNode::~GraphicNetNode - " << node->getAddress().toString().c_str();

	delete addressText;
	delete hostnameText;
	delete secondAddrsText;
	delete icon;
}

void GraphicNetNode::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget){
	/*painter->setPen(Qt::blue);
	QRectF bounds = boundingRect();
	painter->drawRect(bounds);*/
}

QRectF GraphicNetNode::boundingRect() const {
	QRectF bounds;

	qreal height = icon->boundingRect().height() + (2.0 * CLOUD_Y_MARGIN);
	qreal width = icon->boundingRect().width() + (2.0 * CLOUD_X_MARGIN);

	if(addressText->isVisible()){
		width = fmax(width, addressText->boundingRect().width());
		height += addressText->boundingRect().height();
	}

	if(hostnameText->isVisible()){
		width = fmax(width, hostnameText->boundingRect().width());
		height += hostnameText->boundingRect().height();
	}

	if(secondAddrsText->isVisible()){
		width = fmax(width, secondAddrsText->boundingRect().width());
		height += secondAddrsText->boundingRect().height();
	}

	bounds.setX(0);
	bounds.setY(0);

	bounds.setWidth(width);
	bounds.setHeight(height);

	return bounds;
}

void GraphicNetNode::updatePositions(){
	prepareGeometryChange();

	QRectF bounds = boundingRect();

	qreal currentY = bounds.topLeft().y() + CLOUD_Y_MARGIN;
	qreal midX = bounds.center().x();

	if(hostnameText->isVisible()){
		hostnameText->setX(midX - (hostnameText->boundingRect().width()/2.0));
		hostnameText->setY(currentY);

		currentY += addressText->boundingRect().height();
	}

	if(addressText->isVisible()){
		addressText->setX(midX - (addressText->boundingRect().width()/2.0));
		addressText->setY(currentY);

		currentY += addressText->boundingRect().height();
	}

	if(secondAddrsText->isVisible()){
		secondAddrsText->setX(midX - (secondAddrsText->boundingRect().width()/2.0));
		secondAddrsText->setY(currentY);

		currentY += secondAddrsText->boundingRect().height();
	}

	icon->setX(midX - (icon->boundingRect().width()/2.0));
	icon->setY(currentY);

	update();
}

void GraphicNetNode::updateAddress(){
	addressText->setText(node->getAddress().toString().c_str());

	QList<raw_address> secondary = 	node->getSecondaryAddresses();
	if(secondary.size() > 0){
		QString text = "{";
		for(int i=0; i<secondary.size(); i++){
			text.append(secondary[i].toString().c_str());

			if(i < (secondary.size() - 1)){
				text.append(" ");
			}
		}
		text.append("}");

		secondAddrsText->setText(text);
		secondAddrsText->setVisible(true);
	}
	else{
		secondAddrsText->setVisible(false);
	}

	updatePositions();
}



GraphicNetCloud::GraphicNetCloud(raw_address address, QList<GraphicNetNode*> nodes, QGraphicsItem* parent) :
	QGraphicsObject(parent)
{
	this->address = address;
	this->nodes = nodes;
	addressText = new QGraphicsSimpleTextItem(address.toString().c_str(), this);

	QFont textFont;
	textFont.setPixelSize(20);
	addressText->setFont(textFont);

	QBrush brush(Qt::black);

	addressText->setBrush(brush);

	QPixmap pic(":/media/cloudIcon");
	icon = new QGraphicsPixmapItem(pic, this);
	//icon->scale(0.75, 0.75);

	//QRectF bounds = boundingRect();

	for(int i=0; i<nodes.size(); i++){
		nodes[i]->setParentItem(this);
	}

	updatePositions();
}

GraphicNetCloud::~GraphicNetCloud(){
	//qDebug() << "GraphicNetCloud::~GraphicNetCloud - " << address.toString().c_str();

	delete addressText;
	delete icon;
}


void GraphicNetCloud::removeNode(GraphicNetNode* node){
	nodes.removeAt(nodes.indexOf(node));
	if(node->parentItem() == this){
		qDebug() << "GraphicNetCloud::removeNode - Cloud " << address.toString().c_str() <<" parent of node";
		node->setParentItem(NULL);
	}

	updatePositions();
}

void GraphicNetCloud::addNode(GraphicNetNode* node){
	if(!nodes.contains(node)){
		nodes.push_back(node);
		node->setParentItem(this);
	}

	updatePositions();
}

bool GraphicNetCloud::hasNode(GraphicNetNode* node){
	return nodes.contains(node);
}

int GraphicNetCloud::getNodeCount(){
	return nodes.count();
}

raw_address GraphicNetCloud::getAddress(){
	return address;
}

QList<GraphicNetNode*> GraphicNetCloud::getGraphicNodes(){
	return nodes;
}

void GraphicNetCloud::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget){
	/*painter->setPen(Qt::yellow);
	QRectF bounds = boundingRect();
	painter->drawRect(bounds);*/
}

void GraphicNetCloud::updatePositions(){
	prepareGeometryChange();

	if(nodes.count() == 1){
		addressText->setVisible(false);
		icon->setVisible(false);
		nodes[0]->show();

		nodes[0]->setPos(0.0, 0.0);
		return;
	}
	else{
		addressText->setVisible(true);
		icon->setVisible(true);

		for(int i=0; i<nodes.size(); i++){
			nodes[i]->hide();
		}
	}

	QRectF bounds = boundingRect();

	qreal currentY = bounds.topLeft().y() + CLOUD_Y_MARGIN;
	qreal midX = bounds.center().x();

	if(addressText->isVisible()){
		addressText->setX(midX - (addressText->boundingRect().width()/2.0));
		addressText->setY(currentY);

		currentY += addressText->boundingRect().height();
	}

	icon->setX(midX - (icon->boundingRect().width()/2.0));
	icon->setY(currentY);

	update();
}

QRectF GraphicNetCloud::boundingRect() const {
	if(nodes.count() == 1){
		return nodes[0]->boundingRect();
	}

	QRectF bounds = icon->boundingRect();

	//bounds.
	bounds.setHeight(bounds.height()+addressText->boundingRect().height() + (2.0 * CLOUD_Y_MARGIN));
	bounds.setWidth(fmax(bounds.width(), addressText->boundingRect().width()) + (2.0 * CLOUD_X_MARGIN));

	//bounds.setX(-bounds.width()/2.0);
	//bounds.setY(-bounds.height()/2.0);

	bounds.setX(0);
	bounds.setY(0);

	return bounds;
}



NetworkGraphics::NetworkGraphics(NetworkDesc* network, QGraphicsScene* scene, QObject* parent) :
	QObject(parent)
{
	this->scene = scene;
	this->network = network;

	//connect(network, SIGNAL(networkChanged()), this, SLOT(updateNetwork()));

}

NetworkGraphics::~NetworkGraphics(){
	qDebug() << "NetworkGraphics::~NetworkGraphics()";

	linkMap.clear();

	QList<GraphicNetCloud*> clouds = cloudAddrMap.values();
	for(int i=0; i<clouds.count(); i++){
		deleteCloud(clouds[i]);
	}

	QList<GraphicNetNode*> nodes = nodeMap.values();
	for(int i=0; i<nodes.count(); i++){
		if(scene->items().contains(nodes[i])){
			scene->removeItem(nodes[i]);
		}
		delete nodes[i];
	}

	/*QList<GraphicNetLink*> links = linkMap.values();
	for(int i=0; i<links.count(); i++){
		delete links[i];
	}*/
}

void NetworkGraphics::updateSelection(QList<NetNode*> nodes){
	QList<GraphicNetCloud*> selectedClouds = getClouds(nodes);
	QList<GraphicNetCloud*> allClouds = cloudMap.values();

	for(int i=0; i<allClouds.size(); i++){
		GraphicNetCloud* cloud = allClouds[i];

		if(selectedClouds.size() == 0){
			cloud->setOpacity(1.0);
		}
		else{
			if(selectedClouds.contains(cloud)){
				cloud->setOpacity(1.0);

			}
			else{
				cloud->setOpacity(0.3);
			}
		}
	}


	QPen linkPen = QPen();
	linkPen.setColor(Qt::gray);
	linkPen.setWidth(2);

	QList<QGraphicsLineItem*> links = linkMap.values();
	for(int i=0; i<links.size(); i++){
		if(links[i] != NULL){
			links[i]->setPen(linkPen);
		}
	}

	if(selectedClouds.size() > 0){
		QPen linkPen = QPen();
		linkPen.setColor(Qt::yellow);
		linkPen.setWidth(4);

		for(int i=0; i<nodes.size(); i++){
			QList<NetLink*> links = nodes[i]->getLinks();

			for(int j=0; j<links.size(); j++){
				QGraphicsLineItem* line = linkMap[links[j]];

				if(line != NULL){
					line->setPen(linkPen);
				}
			}
		}
	}
}


GraphicNetCloud* NetworkGraphics::getCloud(NetNode *node){
	GraphicNetCloud* cloud = NULL;
	GraphicNetNode* gnode = nodeMap[node];

	if(gnode != NULL){
		cloud = cloudMap[gnode];
	}

	return cloud;
}

QList<GraphicNetCloud*> NetworkGraphics::getClouds(QList<NetNode*> nodes){
	QList<GraphicNetCloud*> clouds;

	for(int i=0; i<nodes.size(); i++){
		clouds.push_back(getCloud(nodes[i]));
	}

	return clouds;
}

GraphicNetCloud* NetworkGraphics::getCloudByAddress(raw_address cloud){
	if(cloudAddrMap.contains(cloud)){
		return cloudAddrMap[cloud];
	}

	return NULL;
}

QList<GraphicNetCloud*> NetworkGraphics::getCloudsByAddress(QList<raw_address> addrList){
	QList<GraphicNetCloud*> clouds;

	for(int i=0; i<addrList.size(); i++){
		GraphicNetCloud* cloud = getCloudByAddress(addrList[i]);
		if(cloud != NULL){
			clouds.push_back(cloud);
		}
	}

	return clouds;
}

void NetworkGraphics::deleteCloud(GraphicNetCloud* cloud){
	scene->removeItem(cloud);
	QList<GraphicNetNode*> nodes = cloud->getGraphicNodes();

	qDebug() << "NetworkGraphics::deleteCloud - Deleting cloud = " << cloud->getAddress().toString().c_str();

	for(int i=0; i<nodes.count(); i++){
		cloudMap.remove(nodes[i]);
		cloud->removeNode(nodes[i]);
	}

	cloudAddrMap.remove(cloud->getAddress());
	delete cloud;
}

QList<GraphicNetNode*> NetworkGraphics::getNodesByAddress(raw_address addr){
	QList<GraphicNetNode*> nodeGraphics;

	QList<NetNode*> nodes = network->getNodesByAddress(addr);

	for(int i=0; i<nodes.count(); i++){
		NetNode* node = nodes[i];

		if(nodeMap.contains(node)){
			nodeGraphics.push_back(nodeMap[node]);
		}
	}

	return nodeGraphics;
}

QSizeF NetworkGraphics::computeSize(raw_address branchAddr){
	GraphicNetCloud* parentCloud = cloudAddrMap[branchAddr];

	QSizeF size = QSizeF(0, 0);

	if(parentCloud != NULL){
		QList<raw_address> children = network->getChildAddressList(branchAddr);
		double parentHeight = parentCloud->boundingRect().height();

		if(children.count() > 0){
			for(int i=0; i<children.count(); i++){
				QSizeF childSize = computeSize(children[i]);

				size.setWidth( size.width() + childSize.width() );
				size.setHeight( fmax(size.height(), (parentHeight + childSize.height())) );
			}
		}
		else{
			size.setHeight(parentHeight);
			size.setWidth(parentCloud->boundingRect().width());
		}
	}

	qDebug() << "NetworkGraphics::computeSize("<<branchAddr.toString().c_str()<<") - Done "
			 << "size=" << size.width() << ", " << size.height();

	return size;

	/*QMultiMap<uint8_t, raw_address> branch = network->getBranchAddresses(branchAddr);

	qDebug() << "NetworkGraphics::computeSize("<<branchAddr.toString().c_str()<<") - Start";

	QSizeF size = QSizeF(0, 0);

	uint8_t lastKey=0;
	QMap<uint8_t, raw_address>::iterator addrIter = branch.begin();

	qreal maxHeight = 0.0;
	qreal widthSum = 0.0;

	for(; addrIter != branch.end(); addrIter++){
		uint8_t key = addrIter.key();

		if(key != lastKey){
			//Update size
			size.setHeight( maxHeight + size.height() );
			size.setWidth( fmax(widthSum, size.width()) );

			maxHeight = 0.0;
			widthSum = 0.0;
		}

		raw_address address = addrIter.value();
		GraphicNetCloud* cloud = cloudAddrMap[address];

		if(cloud != NULL){
			QRectF rect = cloud->boundingRect();
			//QSizeF =

			widthSum += rect.width();
			maxHeight = fmax(maxHeight, rect.height());

			qDebug() << "\t" << address.toString().c_str() << "\tsum:"<<widthSum <<"\twidth:"<<rect.width();

		}

		//Update key
		lastKey = key;
	}

	//Update size
	size.setHeight( maxHeight + size.height() );
	size.setWidth( fmax(widthSum, size.width()) );

	qDebug() << "NetworkGraphics::computeSize("<<branchAddr.toString().c_str()<<") - Done "
			 << "size=" << size.width() << ", " << size.height();

	return size;*/
}


void NetworkGraphics::rebuildNetwork(){
	//Update link data structures
	/*QList<NetLink*> links = network->getLinks();

	for(int i=0; i<links.count(); i++){
		NetLink* link = links[i];

		if(!linkMap.contains(link)){
			GraphicNetLink* graphicLink = new GraphicNetLink(link);
			linkMap.insert(link, graphicLink);
			//scene->addItem(graphLink);
		}
	}
	*/

	//Update node data structures
	QList<NetNode*> nodes = network->getNodes();

	for(int i=0; i<nodes.count(); i++){
		NetNode* node = nodes[i];

		GraphicNetNode* graphicNode = NULL;

		if(!nodeMap.contains(node)){
			graphicNode = new GraphicNetNode(node);
			graphicNode->setZValue(10);
			nodeMap.insert(node, graphicNode);
		}
		else{
			graphicNode = nodeMap[node];
			graphicNode->updateAddress();
		}

		raw_address address = node->getAddress();

		//Check if a cloud needs to be constructed
		GraphicNetCloud* cloud = NULL;
		if(cloudAddrMap.contains(address)){
			//Retrive cloud
			cloud = cloudAddrMap[address];
			if(!cloud->hasNode(graphicNode)){
				cloud->addNode(graphicNode);
			}
		}
		else{
			//Build new cloud
			QList<GraphicNetNode*> cloudNodes = getNodesByAddress(address);
			cloud = new GraphicNetCloud(address, cloudNodes);
			cloud->setZValue(5);
			cloudAddrMap.insert(address, cloud);
			scene->addItem(cloud);
			//cloud->setPos(-20.0, -20.0);
		}

		//Update cloud association
		if(cloudMap.contains(graphicNode)){
			GraphicNetCloud* oldCloud = cloudMap[graphicNode];
			if(oldCloud != cloud){
				oldCloud->removeNode(graphicNode);

				if(oldCloud->getNodeCount() < 1){
					cloudMap.remove(graphicNode);
					deleteCloud(oldCloud);
				}
			}
		}

		cloudMap.insert(graphicNode, cloud);
	}
}

bool NetworkGraphics::positionBranch(raw_address root, QPointF topLeft){
	if(!cloudAddrMap.contains(root)){
		return false;
	}

	GraphicNetCloud* cloud = getCloudByAddress(root);

	qDebug() << "NetworkGraphics::positionBranch - Moving cloud:" << cloud->getAddress().toString().c_str();

	QSizeF size = computeSize(root);
	qreal centerX = topLeft.x() + (size.width()/2.0);

	cloud->setY(topLeft.y());
	cloud->setX(centerX - (cloud->boundingRect().width() / 2.0));

	//Update to start in next row
	topLeft.setY(topLeft.y() + cloud->boundingRect().height());

	QMultiMap<uint8_t, raw_address> branch = network->getBranchAddresses(root);

	QList<uint8_t> keys = branch.uniqueKeys();

	if(keys.size() > 1){
	//for(int i=1; i<keys.size(); i++){
		QPointF cloudPos = topLeft;
		QList<GraphicNetCloud*> clouds = getCloudsByAddress(branch.values(keys[1]));
		qreal maxHeight = 0.0;

		for(int j=0; j<clouds.size(); j++){
			GraphicNetCloud* cloud = clouds[j];

			//cloud->setPos(cloudPos);

			QSizeF size = computeSize(cloud->getAddress());

			positionBranch(cloud->getAddress(), cloudPos);

			cloudPos.setX(cloudPos.x() + size.width());

			maxHeight = fmax(cloud->boundingRect().height(), maxHeight);
		}

		//topLeft.setY(topLeft.y() + maxHeight);
	}

	return true;
}

void NetworkGraphics::updateNetwork(){
	qDebug() << "NetworkGraphics::updateNetwork()";

	scene->clear();
	linkMap.clear();
	rebuildNetwork();

	QPointF position = QPointF(0.0, 0.0);
	QList<raw_address> rootAddrs = network->getTopLevelAddresses();

	for(int i=0; i<rootAddrs.size(); i++){
		if(positionBranch(rootAddrs[i], position)){
			QSizeF size = computeSize(rootAddrs[i]);

			QRectF rect = QRectF(position, size);
			//scene->addRect(rect);*/

			position.setX( position.x() + size.width() + (3.0*CLOUD_X_MARGIN) );

			//scene->update(rect);
		}
	}

	//Draw links
	QList<NetLink*> links = network->getLinks();

	QPen blackPen = QPen();
	blackPen.setColor(Qt::gray);
	blackPen.setWidth(2);


	for(int i=0; i<links.size(); i++){
		NetLink* link = links[i];

		GraphicNetCloud* firstCloud = cloudMap[nodeMap[link->getFirst()]];
		GraphicNetCloud* secondCloud = cloudMap[nodeMap[link->getSecond()]];

		if(firstCloud != secondCloud){

			QPointF startPoint = firstCloud->boundingRect().center();
			QPointF endPoint = secondCloud->boundingRect().center();

			startPoint.setX(firstCloud->pos().x() + startPoint.x());
			startPoint.setY(firstCloud->pos().y() + startPoint.y());

			endPoint.setX(secondCloud->pos().x() + endPoint.x());
			endPoint.setY(secondCloud->pos().y() + endPoint.y());

			QLineF line(startPoint, endPoint);

			QGraphicsLineItem* lineItem = new QGraphicsLineItem(line);
			lineItem->setPen(blackPen);
			lineItem->setZValue(1);
			lineItem->setOpacity(.8);

			scene->addItem(lineItem);

			//qDebug() << "Line: " << line ;

			linkMap.insert(link, lineItem);
		}
	}
}



