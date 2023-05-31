#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    char* sf_path = argv[1]; // reading the message-slot file path
    int cid = atoi(argv[2]); // reading the channel ID
    int fd;
    char msg[BUF_LEN];
    size_t bytes_read;

    if (argc != 3) { 
        fprintf(stderr, "Invalid Arguments in Message-Sender");
        exit(-1);
    }

    fd = open(sf_path, O_RDONLY);
    if( fd < 0 ) {  
        fprintf(stderr, "Can't open device file: %s\n", sf_path);
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, cid) < 0) {
        fprintf(stderr, "ioctl failed");
        exit(EXIT_FAILURE);
    }

    bytes_read = read(fd, msg, BUF_LEN);
    write(STDOUT_FILENO, msg, bytes_read);
    
    close(fd); 
    return 0;
}
