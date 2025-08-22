#include <fcntl.h>
#include <stdio.h>
#include "network.h"

int set_nonblocking(int fd){
    // fd must be opened
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags) < 0){
        perror("F_SETFL");
        return 1;
    }
    return 0;
}

