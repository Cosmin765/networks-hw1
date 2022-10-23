#include "utils.h"

struct Session {
    int logged_pids[100];
    int logged_count;
} session;

int valid_user(const char* user) {
    char* users[20];
    int users_count = parse_users(users);
    
    for(int i = 0; i < users_count; ++i) {
        if(!strcmp(user, users[i])) {
            return 1;
        }
    }
    return 0;
}

int is_logged_in(int pid) {
    for(int i = 0; i < session.logged_count; ++i) {
        if(session.logged_pids[i] == pid) {
            return 1;
        }
    }

    return 0;
}

void add_logged(int pid) {
    session.logged_pids[session.logged_count++] = pid;
}

void remove_logged(int pid) {
    int i;
    for(i = 0; i < session.logged_count; ++i) {
        if(session.logged_pids[i] == pid) {
            i++;
            break;
        }
    }

    while(i < session.logged_count) {
        session.logged_pids[i - 1] = session.logged_pids[i];
        i++;
    }
    session.logged_count--;
}

void login_handler(char* command, int fifo_d, int client_pid) {
    int logged_in = is_logged_in(client_pid);
    if(logged_in) {
        send_with_header(fifo_d, color_text("You are already logged in", BLUE));
        return;
    }

    int p[2];
    EXPECT(pipe(p) != -1, "pipe");
    int pid = fork();
    EXPECT(pid != -1, "login fork");

    if(!pid) {
        close(p[0]);
        char* user = get_delimited_value(command);
        int logged = valid_user(user);

        EXPECT(write(p[1], &logged, sizeof(int)) != -1, "write login");
        close(p[1]);
        exit(0);
    } else {
        close(p[1]);
        EXPECT(read(p[0], &logged_in, sizeof(int)) != -1, "read login");
        close(p[0]);

        wait(NULL);

        if(logged_in) {
            add_logged(client_pid);
        }

        const char* message = logged_in ? "Logged in" : "Invalid username";
        message = color_text(message, logged_in ? GREEN : RED);
        send_with_header(fifo_d, message);
        printf("%s\n", logged_in ? "logged" : "not logged");
    }
}

void get_logged_handler(char* command, int fifo_d, int client_pid) {
    if(!is_logged_in(client_pid)) {
        send_with_header(fifo_d, color_text("You are not logged in", RED));
        return;
    }

    int sockets[2];
    EXPECT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != -1, "socket users");

    int pid = fork();
    EXPECT(pid != -1, "fork users");

    if(!pid) {
        close(sockets[0]);
        char local_buffer[4096];

        int offset = 0;

        struct utmp* info;
        while((info = getutent())) {
            if(info->ut_type != USER_PROCESS) {
                continue;
            }

            char time_buf[100];
            struct tm ts = *localtime((const time_t*)&info->ut_tv.tv_sec);
            strftime(time_buf, sizeof(time_buf), "%a %Y-%m-%d %H:%M:%S", &ts);

            char* parts[] = {
                "Username: ",
                info->ut_user,
                " | Hostname: ",
                info->ut_host,
                " | Entry time: ",
                time_buf,
                " \n"
            };

            int count = sizeof(parts) / sizeof(char*);
            for(int i = 0; i < count; ++i) {
                const char* part = parts[i];

                int n = strlen(part);
                memcpy(local_buffer + offset, part, n);
                offset += n;
            }
        }


        send_with_header(sockets[1], local_buffer);

        close(sockets[1]);
        exit(0);
    } else {
        close(sockets[1]);

        char local_buffer[4096];
        receive_with_header(sockets[0], local_buffer);

        close(sockets[0]);
        wait(NULL);

        send_with_header(fifo_d, color_text(local_buffer, BLUE));
        memset(local_buffer, 0, 4096);
    }
}

void get_proc_handler(char* command, int fifo_d, int client_pid) {
    if(!is_logged_in(client_pid)) {
        send_with_header(fifo_d, color_text("You are not logged in", RED));
        return;
    }

    int sockets[2];
    EXPECT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != -1, "socket users");

    char* value = get_delimited_value(command);
    if(!value) {
        send_with_header(fifo_d, color_text("Wrong format", RED));
        return;
    }

    int proc_pid;
    sscanf(value, "%d", &proc_pid);

    int pid = fork();
    EXPECT(pid != -1, "fork proc");

    if(!pid) {
        close(sockets[0]);

        char local_buffer[4096];

        char filepath[256];
        sprintf(filepath, "/proc/%d/status", proc_pid);

        int fd = open(filepath, O_RDONLY);
        EXPECT(fd != -1, "open proc");

        const char* props[] = {
            "Name",
            "State",
            "Uid",
            "VmSize"
        };

        int prop_index = 0;
        const int props_count = sizeof(props) / sizeof(props[0]);
        char* read_pointer = local_buffer;
        while(prop_index < props_count && read_by_byte(fd, read_pointer)) {
            const char* property = props[prop_index];
            if(strncmp(read_pointer, property, strlen(property))) {
                continue;
            }
            prop_index++;
            read_pointer += strlen(read_pointer);

            sprintf(read_pointer, "\n");
            read_pointer += 1;
        }

        send_with_header(sockets[1], local_buffer);

        close(sockets[1]);
        exit(0);
    } else {
        close(sockets[1]);

        char local_buffer[4096];
        receive_with_header(sockets[0], local_buffer);

        close(sockets[0]);
        wait(NULL);

        printf("%s\n", local_buffer);

        send_with_header(fifo_d, color_text(local_buffer, BLUE));
        memset(local_buffer, 0, 4096);
    }

}

void logout_handler(char* command, int fifo_d, int client_pid) {
    int logged_in = is_logged_in(client_pid);
    if(logged_in) {
        remove_logged(client_pid);
    }
    const char* message = logged_in ? "Logged out" : "Already logged out";
    message = color_text(message, logged_in ? GREEN : RED);
    send_with_header(fifo_d, message);

    logged_in = 0;
    printf("%s\n", message);
}

const char* commands[] = {
    "login",
    "get-logged-users",
    "get-proc-info",
    "logout"
};

void(*handlers[])(char*, int, int)  = {
    login_handler,
    get_logged_handler,
    get_proc_handler,
    logout_handler
};

int main() {
    char buf[4096];

    if(access(FIFO_1, F_OK) == -1) {
        EXPECT(mkfifo(FIFO_1, 0666) != -1, "mkfifo1");
    }

    if(access(FIFO_2, F_OK) == -1) {
        EXPECT(mkfifo(FIFO_2, 0666) != -1, "mkfifo2");
    }

    int fifo_d1, fifo_d2;

    EXPECT((fifo_d1 = open(FIFO_1, O_RDWR)) != -1, "open1");
    EXPECT((fifo_d2 = open(FIFO_2, O_RDWR)) != -1, "open2");

    session.logged_count = 0;

    while(1) {
        int pid;
        read(fifo_d1, &pid, sizeof(int));
        read_by_byte(fifo_d1, buf);
        
        const int commands_count = sizeof(commands) / sizeof(commands[0]);

        int i;
        for(i = 0; i < commands_count; ++i) {
            if(!strncmp(buf, commands[i], strlen(commands[i]))) {
                handlers[i](buf, fifo_d2, pid);
                break;
            }
        }

        if(i >= commands_count) {
            // no match
            send_with_header(fifo_d2, color_text("Invalid command :(", RED));
        }
    }

    close(fifo_d1);
    close(fifo_d2);

    return 0;
}
