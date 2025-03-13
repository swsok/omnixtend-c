/*
 * =====================================================================================
 *
 *       Filename:  tloe_ns.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  02/28/2025 01:41:18 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Kwangwon Koh
 *   Organization:  ETRI
 *
 * =====================================================================================
 */
#include "tloe_mq.h"
#include "tloe_fabric.h"
#include "tloe_ns_thread.h"

#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>

#define MAX_CMD_LEN 32
#define MAX_QUEUE_NAME 64

static void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s -p <queue_name>\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -p <queue_name>    Set the base name for message queues\n");
}

static int parse_cli_argument(int argc, char **argv, char *queue_name,
                              size_t queue_name_size)
{
    int opt;

    // Initialize queue_name
    memset(queue_name, 0, queue_name_size);

    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
        case 'p':
            strncpy(queue_name, optarg, queue_name_size - 1);
            queue_name[queue_name_size - 1] = '\0';
            break;
        case 'h':
            print_usage(argv[0]);
            return -1;
        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    // Check if queue name was provided
    if (queue_name[0] == '\0') {
        printf("Error: Queue name must be specified with -p option\n");
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

void print_command_help(void)
{
    printf("Available commands:\n");
    printf("  s               - Toggle tloe_ns thread (start/stop)\n");
    printf("  start, run, r   - Start tloe_ns thread\n");
    printf("  stop, halt, h   - Stop tloe_ns thread\n");
    printf("  flush, f        - Flush message queues\n");
    printf("  a <count>       - Drop next <count> requests from Port A to B\n");
    printf("  b <count>       - Drop next <count> requests from Port B to A\n");
    printf("  w <count>       - Drop next <count> requests bi-directionally\n");
    printf("  quit, q         - Quit program\n");
}

static int is_empty_command(const char *cmd) {
    while (*cmd) {
        if (!isspace(*cmd)) {
            return 0;
        }
        cmd++;
    }
    return 1;
}

int main(int argc, char **argv)
{
    char cmd[MAX_CMD_LEN];
    char queue_name[MAX_QUEUE_NAME];

    if (parse_cli_argument(argc, argv, queue_name, sizeof(queue_name)) < 0) {
        return 1;
    }

    printf("TLOE tloe_ns CLI (PortA: %s-a, PortB: %s-b)\n", queue_name,
           queue_name);
    print_command_help();

    while (1) {
        printf("> ");
        if (fgets(cmd, MAX_CMD_LEN, stdin) == NULL)
            break;

        cmd[strcspn(cmd, "\n")] = 0;
        if (is_empty_command(cmd)) {
            continue;
        } else if (strcmp(cmd, "s") == 0) {
            if (tloe_ns_thread_is_done()) {
                tloe_ns_thread_start(queue_name);
            } else {
                tloe_ns_thread_stop();
            }
        } else if (strcmp(cmd, "start") == 0 || strcmp(cmd, "run") == 0 ||
                   strcmp(cmd, "r") == 0) {
            tloe_ns_thread_start(queue_name);
        } else if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "halt") == 0 ||
                   strcmp(cmd, "h") == 0) {
            tloe_ns_thread_stop();
        } else if (strcmp(cmd, "flush") == 0 || strcmp(cmd, "f") == 0) {
            tloe_ns_flush_ports();
        } else if (strncmp(cmd, "a ", 2) == 0) {
            int count = atoi(cmd + 2);
            if (count > 0) {
                tloe_ns_set_drop_count_a_to_b(count);
            } else {
                printf("Invalid count value. Usage: a <positive_number>\n");
            }
        } else if (strncmp(cmd, "b ", 2) == 0) {
            int count = atoi(cmd + 2);
            if (count > 0) {
                tloe_ns_set_drop_count_b_to_a(count);
            } else {
                printf("Invalid count value. Usage: b <positive_number>\n");
            }
        } else if (strncmp(cmd, "w ", 2) == 0) {
            int count = atoi(cmd + 2);
            if (count > 0) {
                tloe_ns_set_drop_count_bidirectional(count);
            } else {
                printf("Invalid count value. Usage: b <positive_number>\n");
            }
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            if (!tloe_ns_thread_is_done()) {
                tloe_ns_thread_stop();
            }
            break;
        } else {
            printf("Unknown command. ");
            print_command_help();
        }
    }

    return 0;
}
