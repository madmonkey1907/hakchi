#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#endif
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <QURL>
#endif

static QFileInfo appFileInfo_init()
{
#ifdef WIN32
    TCHAR buffer[MAX_PATH];
    buffer[0]=0;
    GetModuleFileName(GetModuleHandle(0),buffer,MAX_PATH);
#ifdef UNICODE
    const QString mfn(QString::fromUtf16((ushort*)buffer));
#else
    const QString mfn(QString::fromLocal8Bit(buffer));
#endif
    QFileInfo pfi(mfn);
#elif __APPLE__
    CFURLRef url = (CFURLRef)CFAutorelease((CFURLRef)CFBundleCopyBundleURL(CFBundleGetMainBundle()));
    QString mfn = (QUrl::fromCFURL(url).path());
    mfn += "Contents/MacOS/";
    QFileInfo pfi(mfn);
#else
    QFileInfo pfi(QFileInfo(QString::fromLatin1("/proc/%1/exe").arg(getpid())).canonicalFilePath());
#endif
    return pfi;
}

static const QFileInfo&appFileInfo()
{
    static QFileInfo pfi(appFileInfo_init());
    return pfi;
}

QString appPath()
{
    return appFileInfo().absolutePath();
}

int main(int argc,char*argv[])
{
    QDir dir(".");
    dir.setCurrent(appPath());
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
