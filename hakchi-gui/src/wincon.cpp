#include "wincon.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#endif

CWinCon::CWinCon(QObject*parent):QObject(parent)
{
    con=0;
    int filedes[2];
#ifdef WIN32
    if(_pipe(filedes,0x10000,O_BINARY|O_NOINHERIT)>=0)
#else
    if(pipe(filedes)>=0)
#endif
    {
        fflush(stdout);
        fflush(stderr);
#ifdef WIN32
        while((_dup2(filedes[1],STDOUT_FILENO)<0)&&(errno==EINTR)){}
        while((_dup2(filedes[1],STDERR_FILENO)<0)&&(errno==EINTR)){}
        _close(filedes[1]);
        *stdout=*_fdopen(STDOUT_FILENO,"wb");
        *stderr=*_fdopen(STDERR_FILENO,"wb");
        con=_fdopen(filedes[0],"rb");
#else
        while((dup2(filedes[1],STDOUT_FILENO)<0)&&(errno==EINTR)){}
        while((dup2(filedes[1],STDERR_FILENO)<0)&&(errno==EINTR)){}
        ::close(filedes[1]);
        con=fdopen(filedes[0],"rb");
#endif
        setvbuf(stdout,0,_IONBF,0);
        setvbuf(stderr,0,_IONBF,0);
    }
}

CWinCon::~CWinCon()
{
    if(con)
    {
        fclose(con);
        con=0;
    }
}

void CWinCon::readOutput()
{
    if(con)
    {
        //fucking windows
        char buffer[1];
        ssize_t size=fread(buffer,1,sizeof(buffer),con);
        if(size>0)
            emit msg(QString::fromLocal8Bit(buffer,size));
    }
}
