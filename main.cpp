
#include "xshm.h"
#include <unistd.h>
#include <cstring>
#include <iostream>

int main(int argc, char* argv[])
{
    xShm xshm("xshmtest");
    if(!xshm.attach(xShm::AccessMode::ReadWrite))
    {
        printf("attached failed\n");
        if(!xshm.create(1024))
        {
            printf("created failed\n");
        }
    }
    else
    {
        printf("attached okay\n");
    }

    xshm.lock();
    char* buff = static_cast<char*>(xshm.data());
    if(buff[0] == 0)
    {
        snprintf(buff, 128, "%s", "shared memory test message!\n");
        printf("write: %s\n", buff);
    }
    else
    {
        printf("read: %s\n", buff);
    }
    xshm.unlock();
    getchar();
    return 0;
}

