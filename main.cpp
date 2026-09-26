#include "mainwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //初加载样式表
    QFile qss(":/style/stylesheet.qss");
    if(qss.open(QFile::ReadOnly)){
        //qDebug("stylesheet open success");
        QString style = QString::fromUtf8(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
        //qDebug("stylesheet open failed");
    }

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
