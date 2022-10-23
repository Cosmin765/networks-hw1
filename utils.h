#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <utmp.h>
#include <time.h>
#include <sys/wait.h>

#define EXPECT(cond, msg) if(!(cond)) {fprintf(stderr, "%s - errno %s", msg, strerror(errno)); exit(-1);}
#define FIFO_1 "fifo1"
#define FIFO_2 "fifo2"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define BLUE "\033[0;34m"
#define YELLOW "\033[1;33m"
#define NO_COLOR "\033[0m"

int read_by_byte(int fd, char* buf) {
    char* before = buf;
    int r;
    while((r = read(fd, buf, 1))) {
        EXPECT(r != -1, "read by byte");
        if(*buf == '\n') {
            *buf = 0;
            break;
        }
        buf++;
    }
    return buf - before;
}

int parse_users(char** arr) {
    char buffer[4096];

    int fd = open("users.conf", O_RDONLY);
    EXPECT(fd != -1, "parse users");

    read(fd, buffer, 4096);
    int count = 0;
    char* p = strtok(buffer, "\n");
    while(p) {
        *arr = p;
        arr++;
        count++;
        p = strtok(NULL, "\n");
    }

    return count;
}

void send_with_header(int fd, const char* message) {
    int size = strlen(message);
    write(fd, &size, sizeof(int));
    write(fd, message, size);
}

int receive_with_header(int fd, char* buffer) {
    int size;
    read(fd, &size, sizeof(int));
    read(fd, buffer, size);
    return size;
}

char* get_delimited_value(char* pair) {
    char* pos = strtok(pair, ":");
    pos = strtok(NULL, ":");

    if(!pos) return NULL;

    int size = strlen(pos);

    while(*pos == ' ') {
        pos++;
        if(!--size) {
            return NULL;
        }
    }

    return pos;
}

char colored_buffer[4096];

char* color_text(const char* text, const char* color) {
    sprintf(colored_buffer, "%s%s%s", color, text, NO_COLOR);
    return colored_buffer;
}