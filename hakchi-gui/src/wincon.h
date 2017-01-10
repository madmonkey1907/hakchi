#ifndef WINCON_H
#define WINCON_H

#include <QObject>
#include <QString>
#include <QTextCodec>

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
    QTextCodec*codec;
};

#endif // WINCON_H
