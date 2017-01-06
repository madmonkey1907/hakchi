#ifndef WINCON_H
#define WINCON_H

#include <QObject>
#include <QString>

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
    QString str;
};

#endif // WINCON_H
