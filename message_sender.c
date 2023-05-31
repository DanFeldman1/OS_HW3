#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
    char* sf_path = argv[1]; // reading the message-slot file path
    int cid = atoi(argv[2]); // reading the channel ID
    int fd;

    if (argc != 4) { 
        fprintf(stderr, "Invalid Arguments in Message-Sender");
        exit(-1);
    }
    
    fd = open(sf_path, O_WRONLY);
    if( fd < 0 ) {  
        fprintf(stderr, "Can't open device file: %s\n", sf_path);
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, cid) < 0) {
        fprintf(stderr, "ioctl failed");
        exit(EXIT_FAILURE);
    }

    if (write(fd, argv[3], strlen(argv[3])) < strlen(argv[3])) {
        fprintf(stderr, "write failed");
        exit(EXIT_FAILURE);
    }
    
    close(fd); 
    return 0;
}
