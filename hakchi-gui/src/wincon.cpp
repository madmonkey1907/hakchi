#include "wincon.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#include <windows.h>
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
        con=_fdopen(filedes[0],"rb");
        *stdin=*fopen("nul","rb");
        *stdout=*_fdopen(STDOUT_FILENO,"wb");
        *stderr=*_fdopen(STDERR_FILENO,"wb");
#else
        while((dup2(filedes[1],STDOUT_FILENO)<0)&&(errno==EINTR)){}
        while((dup2(filedes[1],STDERR_FILENO)<0)&&(errno==EINTR)){}
        ::close(filedes[1]);
        con=fdopen(filedes[0],"rb");
#endif
        setvbuf(stdout,0,_IONBF,0);
        setvbuf(stderr,0,_IONBF,0);
    }
#ifdef WIN32
    CPINFOEX cpi;
    GetCPInfoEx(CP_OEMCP,0,&cpi);
    switch(cpi.CodePage)
    {
    case 866:
        codec=QTextCodec::codecForName("IBM866");
        break;
    default:
        codec=QTextCodec::codecForName("IBM437");
    }
#else
    codec=QTextCodec::codecForLocale();
#endif
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
    if(str.length())
    {
        emit msg(str.left(1));
        str=str.right(str.length()-1);
        return;
    }
    if(con)
    {
        //fucking windows
        char buffer[0x1000];
        if(fgets(buffer,sizeof(buffer),con))
        {
            str=codec->toUnicode(buffer);
            if(str.length())
            {
                readOutput();
                return;
            }
        }
    }
    emit msg("\n");
}
