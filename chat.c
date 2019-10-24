// clang -no-pie -static chat.c -lpthread -ochat -Wl,-z,now -Wl,-z,relro
// strip -g chat

#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define MAXCHANNELS 5

static int cpipe[2];

static pthread_t channels[MAXCHANNELS];
static int active[MAXCHANNELS] = {0};
static int pipes[MAXCHANNELS][2];

static bool quit = false;

#define QUIT_CHANNEL(x) do { \
        active[x] = 2; \
        return NULL;        \
    } while (0);

static char command[32] = {0};


static int wait_loop(bool is_command_channel, int channel) {
    struct pollfd pfd;
    if (is_command_channel) {
        pfd.fd = cpipe[0];
    } else {
        pfd.fd = pipes[channel][0];
    }
    pfd.events = POLLIN;

    if (is_command_channel && channel != -1) {
        write(pipes[channel][1], "c", 1);
    } else if (!is_command_channel) {
        write(cpipe[1], "c", 1);
    }

    while (1) {
        int ret;
        if ((ret = poll(&pfd, 1, 1000)) == -1) {
            // error
            return 1;
        } else if (ret == 0) {
            // timeout
            if (quit == true) {
                return 1;
            }
            continue;
        }

        char c;
        if (is_command_channel) {
            read(cpipe[0], &c, 1);
        } else {
            read(pipes[channel][0], &c, 1);
        }
        break;
    }
    
    return 0;
}

static void chat_channel_help() {
    printf("Chat Commands:\n"
           "\t/e\t- Echo - The first line following this command specifies the number of characters to echo.\n"
           "\t/pc\t- Pause Chat Channel - Return to Command Channel. The Chat Channel stays open.\n"
           "\t/qc\t- Quit Chat Channel - Return to Command Channel. The Chat Channel is terminated.\n"
           "\t/h\t- Help - Print this help message.\n"
           "That's all for now :/\n");
}

static void *chat_channel_handler(void * arg) {
    int channel = (int) arg;

    if (pipe(pipes[channel]) == -1) {
        printf("Chat Channel Creation Failed: Pipes\n");
        QUIT_CHANNEL(channel);
    }

    printf("Chat Channel %d:\n", channel+1);

    while (1) {
        printf("> ");

        if(!fgets(command, 32, stdin)) {
            QUIT_CHANNEL(channel);
        }

        if (strlen(command) == 3 && command[0] == '/' && command[1] == 'e') {
            if(!fgets(command, 32, stdin)) {
                printf("Chat Command Error: Reading Echo Size\n");
            }

            int length = atoi(command) + 1;
            char *line = (char *)alloca(length);

            if (!fgets(line, length, stdin)) {
                QUIT_CHANNEL(channel);
            }

            write(1, line, strlen(line));
            write(1, "\n", 1);
        } else if (strlen(command) == 4 && command[0] == '/' && command[1] == 'p' && command[2] == 'c') {
            if (wait_loop(false, channel)) {
                QUIT_CHANNEL(channel);
            }

            printf("Chat Channel %d:\n", channel+1);
        } else if (strlen(command) == 4 && command[0] == '/' && command[1] == 'q' && command[2] == 'c') {
            write(cpipe[1], "c", 1);
            QUIT_CHANNEL(channel);
        } else if (strlen(command) == 3 && command[0] == '/' && command[1] == 'h') {
            chat_channel_help();
        }
    }
}

static void command_channel_help() {
    printf("Command Commands:\n"
           "\t/nc\t- New Chat Channel - Create and join a new Chat Channel.\n"
           "\t/jc x\t- Join Chat Channel - Join the Chat Channel number x.\n"
           "\t/lc\t- List Chat Channels - Lists the Chat Channels.\n"
           "\t/q\t- Quit - Quit this awesome chat program.\n"
           "\t/h\t- Help - Print this help message.\n");
}

int main() {
    if (pipe(cpipe) == -1) {
        return 1;
    }

    printf("Command Channel:\n");

    while (1) {
        printf("> ");

        if(!fgets(command, 32, stdin)) {
            return 1;
        }

        if (strlen(command) == 4 && command[0] == '/' && command[1] == 'n' && command[2] == 'c') {
            int channel = -1;
            for (int i = 0; i < MAXCHANNELS; i++) {
                if (active[i] == 0) {
                    channel = i;
                    break;
                }
            }

            if (channel == -1) {
                printf("Chat Channel Creation Failed: No channel slots left\n");
            } else {
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setstacksize(&attr, 0x3C000);

                pthread_create(&channels[channel], &attr, chat_channel_handler, (void *)channel);
                active[channel] = 1;

                wait_loop(true, -1);
                printf("Command Channel:\n");
            }
        } else if (command[0] == '/' && command[1] == 'j' && command[2] == 'c' && command[3] == ' ') {
            printf("Joining Chat Channel.\n");

            int channel = atoi(command + 4) - 1;

            if (0 <= channel && channel < MAXCHANNELS && active[channel] == 1) {
                wait_loop(true, channel);
                printf("Command Channel:\n");
            } else {
                printf("Invalid Chat Channel number.\n");
            }
        } else if (strlen(command) == 4 && command[0] == '/' && command[1] == 'l' && command[2] == 'c') {
            printf("Chat Channels:\n");

            for (int i = 0; i < MAXCHANNELS; i++) {
                if (active[i] == 1) {
                    printf("\tChat Channel %d\n", i+1);
                }
            }
        } else if (strlen(command) == 3 && command[0] == '/' && command[1] == 'q') {
            printf("Quitting.\n");
            quit = true;
            break;
        } else if (strlen(command) == 3 && command[0] == '/' && command[1] == 'h') {
            command_channel_help();
        } else {
            printf("Invalid command.\n");
        }

        for (int i = 0; i < MAXCHANNELS; i++) {
            if (active[i] == 2) {
                pthread_join(channels[i], NULL);
                close(pipes[i][0]);
                close(pipes[i][1]);
                active[i] = 0;
            }
        }
    }

    for (int i = 0; i < MAXCHANNELS; i++) {
        if (active[i] == 1 || active[i] == 2) {
            pthread_join(channels[i], NULL);
        }
    }

    close(cpipe[0]);
    close(cpipe[1]);

    return 0;
}
