#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui>
#include <QMainWindow>

#include "networkdesc.h"
#include "networkgraphics.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

	public:
		explicit MainWindow(QWidget *parent = 0);
		~MainWindow();

		//bool event(QEvent e);
		bool eventFilter(QObject* obj, QEvent* event);

	public slots:
		void updateTable();
		void update();

		void updateView();

	protected:
		void resizeEvent(QResizeEvent* e);

	private:
		Ui::MainWindow *ui;
		NetworkDesc* network;
		QGraphicsScene* scene;
		NetworkGraphics* netgraphics;

	public slots:
		void on_actionImport_NS_triggered();
		void on_actionImport_Config_triggered();
		void on_actionExport_Config_triggered();
		void onCellChanged(int, int);
		void selectionChanged();
};

#endif // MAINWINDOW_H
