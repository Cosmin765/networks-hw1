#include "utils.h"

int main() {
    char buf[4096];

    if(access(FIFO_1, F_OK) == -1) {
        EXPECT(mkfifo(FIFO_1, 0666) != -1, "mkfifo1");
    }

    if(access(FIFO_2, F_OK) == -1) {
        EXPECT(mkfifo(FIFO_2, 0666) != -1, "mkfifo2");
    }

    int fifo_d1, fifo_d2;

    int pid = getpid();


    EXPECT((fifo_d1 = open(FIFO_1, O_RDWR)) != -1, "open1");
    EXPECT((fifo_d2 = open(FIFO_2, O_RDWR)) != -1, "open2");

    while(1) {
        printf(YELLOW);
        fflush(stdout);
        int r = read_by_byte(0, buf);
        printf(NO_COLOR);
        fflush(stdout);

        if(!strcmp(buf, "quit")) {
            write(fifo_d1, "logout\n", 7);
            break;
        }

        sprintf(buf + r, "\n");
        write(fifo_d1, &pid, sizeof(int));
        write(fifo_d1, buf, r + 1);

        int size;
        read(fifo_d2, &size, sizeof(int));
        read(fifo_d2, buf, size);
        buf[size] = 0;
        printf("%s\n", buf);
    }
    

    close(fifo_d1);
    close(fifo_d2);

    return 0;
}
