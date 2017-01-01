#ifndef WINCON_H
#define WINCON_H

#include <QObject>

class CWinCon:public QObject
{
    Q_OBJECT
public:
    explicit CWinCon(QObject*parent=0);
    ~CWinCon();
signals:
    void msg(const QString&msg);
public slots:
    void readOutput();
private:
    FILE*con;
};

#endif // WINCON_H
