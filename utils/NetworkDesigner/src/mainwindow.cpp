#include <QtGui>
#include <QtCore>
#include "netnode.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <switched.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	network = NULL;
	netgraphics = NULL;
    ui->setupUi(this);

	scene = new QGraphicsScene(this);

	ui->graphicsView->installEventFilter(this);
	ui->graphicsView->verticalScrollBar()->installEventFilter(this);
	ui->graphicsView->setScene(scene);



	connect(ui->tableWidget, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChanged(int,int)));

	connect(ui->tableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
}

MainWindow::~MainWindow()
{
	qDebug() << "~MainWindow() - Cleaning up";

	if(netgraphics != NULL){
		delete netgraphics;
	}

	if(network != NULL){
		delete network;
	}

    delete ui;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event){
	if(event->type() == QEvent::Wheel){
		if(obj == ui->graphicsView){
			QWheelEvent* wheel = (QWheelEvent*) event;

			//Ensure that [-240 <= delta <= +240]
			qreal delta = fmax(fmin(240, (qreal)wheel->delta()), -240);

			qreal scale = ((delta / 4.0) / 120.0) + 1.0;

			QTransform t = ui->graphicsView->transform();
			t.scale(scale, scale);

			//Prevent excessive zooming
			if(t.m11() > 0.1 && t.m11() < 4.0){
				ui->graphicsView->setTransform(t);
			}

			return true;
		}
		else if(obj == ui->graphicsView->verticalScrollBar()){
			return true;
		}
	}
	return false;
}

void MainWindow::updateTable(){
	ui->tableWidget->clearContents();
	ui->tableWidget->setRowCount(0);

	if(network != NULL){
		qDebug() << "UpdateTable() - Start";
		QMap<QString, NetNode*> nodes = network->getNodeMap();
		QMap<QString, NetNode*>::iterator nodeIter = nodes.begin();

		//ui->tableWidget->setRowCount(nodes.size());
		int row = 0;

		while(nodeIter != nodes.end()){
			QTableWidgetItem* name = new QTableWidgetItem(nodeIter.key());
			name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

			QString addrStr = nodeIter.value()->getAddress().toString().c_str();
			QTableWidgetItem* address = new QTableWidgetItem(addrStr);

			QTableWidgetItem* autoAssign = new QTableWidgetItem();
			if(nodeIter.value()->isAutoAddress()){
				autoAssign->setCheckState(Qt::Checked);
			}
			else{
				autoAssign->setCheckState(Qt::Unchecked);
			}

			QTableWidgetItem* mmtEnable = new QTableWidgetItem();
			if(nodeIter.value()->isMmtEnabled()){
				mmtEnable->setCheckState(Qt::Checked);
			}
			else{
				mmtEnable->setCheckState(Qt::Unchecked);
			}

			QString mmtUidStr;
			QTextStream strm(&mmtUidStr);
			strm << nodeIter.value()->getMmtUid();
			QTableWidgetItem* mmtUid = new QTableWidgetItem(mmtUidStr);

			ui->tableWidget->insertRow(row);

			qDebug() << "Insert at row "<<row;
			ui->tableWidget->setItem(row, 0, name);
			ui->tableWidget->setItem(row, 1, address);
			ui->tableWidget->setItem(row, 2, autoAssign);
			ui->tableWidget->setItem(row, 3, mmtEnable);
			ui->tableWidget->setItem(row, 4, mmtUid);

			row++;
			nodeIter++;
		}

		qDebug() << "UpdateTable() - End";
	}
}

void MainWindow::on_actionImport_Config_triggered()
{
	if(network != NULL){
		//Open and parse address config file
		QFileDialog importDialog(this, "Import Address Configuration");
		importDialog.setFileMode(QFileDialog::ExistingFile);
		importDialog.setNameFilter(tr("INI Files (*.ini)"));

		importDialog.exec();
		QStringList fileList = importDialog.selectedFiles();

		if(fileList.size() == 1){
			qDebug() << "Importing [" << fileList.at(0) << "]";

			statusBar()->showMessage(QString("Importing file ").append(fileList.at(0)));

			if(network->parseNetConf(fileList[0]) == false){
				QMessageBox errorMsg(QMessageBox::Warning, "Address Conf Import Error", "Failed to import address configuration file.");
				errorMsg.exec();
			}

			updateView();
		}
		else if(fileList.size() > 1){
			QMessageBox errorMsg(QMessageBox::Warning, "Address Conf Import Error", "Only one file can be imported at a time.");
			errorMsg.exec();
		}
		else{
			qDebug() << "No config file selected";
		}
	}
}

void MainWindow::on_actionExport_Config_triggered(){
	if(network != NULL){
		//Open and parse address config file
		QFileDialog exportDlg(this, "Export Address Configuration");

		exportDlg.setFileMode(QFileDialog::AnyFile);
		exportDlg.setAcceptMode(QFileDialog::AcceptSave);
		exportDlg.setNameFilter(tr("INI Files (*.ini)"));

		exportDlg.exec();
		QStringList fileList = exportDlg.selectedFiles();

		if(fileList.size() == 1){
			qDebug() << "Exporting [" << fileList.at(0) << "]";

			statusBar()->showMessage(QString("Exporting file ").append(fileList.at(0)));

			if(network->writeNetConf(fileList[0]) == true){
				statusBar()->showMessage(QString("Saved address config: ").append(fileList.at(0)));
			}
			else{

				statusBar()->showMessage(QString("Failed to export").append(fileList.at(0)));
			}
		}
	}
}

void MainWindow::on_actionImport_NS_triggered()
{
	//Open and parse an NS file
	QFileDialog importDialog(this, "Import NS File");
	importDialog.setFileMode(QFileDialog::ExistingFile);
	importDialog.setNameFilter(tr("NS Files (*.ns)"));

	importDialog.exec();
	QStringList fileList = importDialog.selectedFiles();

	if(fileList.size() == 1){

		qDebug() << "Importing [" << fileList.at(0) << "]";


		statusBar()->showMessage(QString("Importing file ").append(fileList.at(0)));

		if(netgraphics != NULL){
			delete netgraphics;
		}

		if(network != NULL){
			//disconnect(network, SIGNAL(networkChanged()), this, SLOT(update()));
			delete network;
		}


		network = new NetworkDesc(this);
		netgraphics = new NetworkGraphics(network, scene, this);

		connect(network, SIGNAL(networkChanged()), this, SLOT(update()));

		network->parseNSFile(fileList.at(0));

		updateView();
	}
	else if(fileList.size() > 1){
		qDebug() << "User selected" << fileList.size() << "files!";

		QMessageBox errorMsg(QMessageBox::Warning, "NS Import Error", "Importing multiple files not supported");
		errorMsg.exec();
	}
	else{
		qDebug() << "No NS file selected";
	}

}

void MainWindow::onCellChanged(int row, int col){
	//Determine if user caused this signal
	if(ui->tableWidget->selectedItems().contains(ui->tableWidget->item(row, col))){
		qDebug() << "onCellChanged row="<<row<<" col="<<col;

		//Determine which column was updated
		if(col == 1 && network != NULL){
			//Address field changed
			NetNode* node = network->getNode(ui->tableWidget->item(row, 0)->text());
			QString addrText = ui->tableWidget->item(row, col)->text();

			//Check for address formatting error
			si_address address;
			if(address.parseString(addrText.toAscii().data()) != 0){
				//Formating error

				qDebug() << "ERROR - Invalid address! [" << addrText<<"]";

				addrText = node->getAddress().toString().c_str();

				ui->tableWidget->item(row, col)->setText(addrText);
			}
			else{
				qDebug() << "Read new address "<<address.toString().c_str();
				node->setAddress(address);
				network->computeAddresses();
			}
		}
		else if(col == 2 && network != NULL){
			//Automatic address field changed
			NetNode* node = network->getNode(ui->tableWidget->item(row, 0)->text());

			node->setAutoAddress(ui->tableWidget->item(row, 2)->checkState() == Qt::Checked);
			network->computeAddresses();
		}
		else if(col == 3 && network != NULL){
			//MMT Enable field changed
			NetNode* node = network->getNode(ui->tableWidget->item(row, 0)->text());

			node->setMmtEnable(ui->tableWidget->item(row, 3)->checkState() == Qt::Checked);
		}
		else if(col == 4 && network != NULL){
			//MMT UID field changed
			NetNode* node = network->getNode(ui->tableWidget->item(row, 0)->text());

			int value = ui->tableWidget->item(row, 4)->text().toInt();
			if(value < 0 || network->isMmtUidUsed(value)){
				QString mmtUidStr;
				QTextStream strm(&mmtUidStr);
				strm << node->getMmtUid();

				ui->tableWidget->item(row, 4)->setText(mmtUidStr);
			}
			else{
				node->setMmtUid(value);
			}
		}
	}
}

void MainWindow::selectionChanged(){
	QList<NetNode*> selectedNodes;
	QList<QTableWidgetItem*> items = ui->tableWidget->selectedItems();

	for(int i=0; i<items.size(); i++){
		QTableWidgetItem* item = items[i];

		if(item->column() == 0){
			NetNode* node = network->getNode(item->text());
			selectedNodes.push_back(node);
		}
	}

	netgraphics->updateSelection(selectedNodes);
}

void MainWindow::resizeEvent(QResizeEvent* e){
	updateView();
}

void MainWindow::updateView(){
	//Initialize view to something reasonable

	scene->setSceneRect(scene->itemsBoundingRect());

	ui->graphicsView->resetMatrix();
	ui->graphicsView->scale(0.1, 0.1);
	ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
	ui->graphicsView->centerOn(scene->sceneRect().center());
}

void MainWindow::update(){
	qDebug() << "MainWindow::update() - Running";
	updateTable();

	if(network != NULL){

		if(netgraphics != NULL){
			delete netgraphics;
			scene->clear();
		}

		netgraphics = new NetworkGraphics(network, scene, this);
		netgraphics->updateNetwork();

		//ui->graphicsView->setMouseTracking(true);


		/*QList<raw_address> addresses = network->getAddressList();

		qDebug() << "Have" << addresses.count() << "addresses";

		for(int i=0; i < addresses.count(); i++){
			raw_address addr = addresses.at(i);

			qDebug() << "\t" << i << ":" << addr.toString().c_str();
		}

		QList<raw_address> parents = network->getTopLevelAddresses();

		qDebug() << "Have" << parents.count() << "top level parents";

		for(int i=0; i < parents.count(); i++){
			raw_address addr = parents.at(i);

			QList<NetNode*> children = network->getChildNodes(addr);
			QMultiMap<uint8_t, NetNode*> branch = network->getBranch(addr);

			qDebug() << "\t" << i << ":" << addr.toString().c_str()
					 << "\thas" << children.count()
					 << "children branch contains"
					 <<branch.size()<<"nodes";

			QList<uint8_t> layers = branch.uniqueKeys();
			qDebug() << "\tHas " << layers.count() <<" layers";

			for(int j=0; j<layers.count(); j++){
				QList<NetNode*> nodes = branch.values(layers[j]);

				QString outputStr;
				QTextStream stream(&outputStr);
				stream<<"\t\tLayer["<<j<<"] Size="<<nodes.count()<<" ";

				for(int k=0; k<nodes.count(); k++){
					NetNode* node = nodes[k];
					stream << node->getAddress().toString().c_str() << " ";
				}

				qDebug() << outputStr;
			}

		}*/
	}
}


