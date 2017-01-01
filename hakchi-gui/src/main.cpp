#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <stdlib.h>

int main(int argc,char*argv[])
{
    QDir dir(".");
    for(int i=0;i<4;i++)
    {
        if(dir.exists("bin"))
            break;
        else
            dir.cdUp();
    }
    if(dir.exists("bin"))
        dir.setCurrent(dir.absolutePath());
    QString path=QString::fromLocal8Bit(getenv("PATH"));
#ifdef WIN32
    path=QString("PATH=%1\\bin;%2").arg(dir.absolutePath()).arg(path);
    _putenv(path.toLocal8Bit());
#else
    path=QString("%1/bin:%2").arg(dir.absolutePath()).arg(path);
    setenv("PATH",path.toLocal8Bit(),1);
#endif

    QApplication a(argc,argv);
    MainWindow w;
    w.show();
    return a.exec();
}
