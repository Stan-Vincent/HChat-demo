#include "mainwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ///初加载样式表
    QFile qss(":/style/stylesheet.qss");
    if(qss.open(QFile::ReadOnly)){
        qDebug("Client's stylesheet open success");
        QString style = QString::fromUtf8(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
        qDebug("Client's stylesheet open failed");
    }

    ///从congif.ini获取GateServer的url前缀
    //获取congig.ini目录
    QString fileName = "config.ini";
    //获取执行目录
    QString app_path = QCoreApplication::applicationDirPath();
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + fileName);

    QSettings settings(config_path,QSettings::IniFormat);
    //读取ini文件关于GateServer的http地址
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://"+gate_host +":" +gate_port;

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
