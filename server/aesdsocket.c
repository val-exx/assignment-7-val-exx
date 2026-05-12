#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"

static int server_fd = -1;
static volatile sig_atomic_t stop = 0;

static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t timer_thread;
static int timer_thread_started = 0;

static volatile sig_atomic_t first_packet_received = 0;

struct thread_data {
    pthread_t thread_id;
    int client_fd;
    int thread_complete;
    char ip[INET_ADDRSTRLEN];
    SLIST_ENTRY(thread_data) entries;
};

SLIST_HEAD(thread_list_head, thread_data);
static struct thread_list_head thread_list = SLIST_HEAD_INITIALIZER(thread_list);

static void signal_handler(int signo)
{
    (void)signo;

    stop = 1;

    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR);
    }
}

static int send_packet_on_buffer(int fd, const char *buf, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t rc = send(fd, buf + sent, len - sent, 0);

        if (rc <= 0) {
            return -1;
        }

        sent += rc;
    }

    return 0;
}

static int send_file_to_client(int fd)
{
    FILE *fp = fopen(FILE_PATH, "r");
    if (!fp) {
        return -1;
    }

    char buf[1024];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_packet_on_buffer(fd, buf, n) == -1) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

static int append_timestamp_to_file(void)
{
    time_t now;
    struct tm time_info;
    char timestamp[128];

    now = time(NULL);
    if (now == (time_t)-1) {
        return -1;
    }

    if (localtime_r(&now, &time_info) == NULL) {
        return -1;
    }

    if (strftime(timestamp,
                 sizeof(timestamp),
                 "timestamp:%a, %d %b %Y %H:%M:%S %z\n",
                 &time_info) == 0) {
        return -1;
    }

    pthread_mutex_lock(&file_mutex);

    FILE *fp = fopen(FILE_PATH, "a");
    if (!fp) {
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    if (fputs(timestamp, fp) == EOF) {
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    fclose(fp);

    pthread_mutex_unlock(&file_mutex);

    return 0;
}

static void *timer_thread_func(void *arg)
{
    (void)arg;

    while (!stop) {
        for (int i = 0; i < 10 && !stop; i++) {
            sleep(1);
        }

        if (!stop && first_packet_received) {
            if (append_timestamp_to_file() == -1) {
                syslog(LOG_ERR, "Failed to append timestamp");
            }
        }
    }

    return NULL;
}

static int write_packet_and_send_file(int fd, const char *packet, size_t line_len)
{
    int ret = 0;

    pthread_mutex_lock(&file_mutex);

    FILE *fp = fopen(FILE_PATH, "a");
    if (!fp) {
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    if (fwrite(packet, 1, line_len, fp) != line_len) {
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    fclose(fp);

    if (send_file_to_client(fd) == -1) {
        ret = -1;
    }

    pthread_mutex_unlock(&file_mutex);

    return ret;
}

static int handle_client(int fd)
{
    char recvbuf[1024];
    char *packet = NULL;
    size_t packet_len = 0;

    while (!stop) {
        ssize_t bytes = recv(fd, recvbuf, sizeof(recvbuf), 0);

        if (bytes <= 0) {
            break;
        }

        char *tmp = realloc(packet, packet_len + bytes + 1);
        if (!tmp) {
            free(packet);
            return -1;
        }

        packet = tmp;

        memcpy(packet + packet_len, recvbuf, bytes);
        packet_len += bytes;
        packet[packet_len] = '\0';

        char *newline;

        while ((newline = strchr(packet, '\n')) != NULL) {
            size_t line_len = newline - packet + 1;
            first_packet_received = 1;

            if (write_packet_and_send_file(fd, packet, line_len) == -1) {
                free(packet);
                return -1;
            }

            memmove(packet, packet + line_len, packet_len - line_len);
            packet_len -= line_len;
            packet[packet_len] = '\0';
        }
    }

    free(packet);
    return 0;
}

static void *client_thread_func(void *arg)
{
    struct thread_data *data = (struct thread_data *)arg;

    syslog(LOG_INFO, "Accepted connection from %s", data->ip);

    handle_client(data->client_fd);

    close(data->client_fd);
    data->client_fd = -1;

    syslog(LOG_INFO, "Closed connection from %s", data->ip);

    pthread_mutex_lock(&thread_list_mutex);
    data->thread_complete = 1;
    pthread_mutex_unlock(&thread_list_mutex);

    return NULL;
}

static void join_completed_threads(void)
{
    struct thread_data *data;
    struct thread_data *temp;

    pthread_mutex_lock(&thread_list_mutex);

    data = SLIST_FIRST(&thread_list);

    while (data != NULL) {
        temp = SLIST_NEXT(data, entries);

        if (data->thread_complete) {
            pthread_mutex_unlock(&thread_list_mutex);

            pthread_join(data->thread_id, NULL);

            pthread_mutex_lock(&thread_list_mutex);
            SLIST_REMOVE(&thread_list, data, thread_data, entries);
            free(data);
        }

        data = temp;
    }

    pthread_mutex_unlock(&thread_list_mutex);
}

static void request_threads_exit(void)
{
    struct thread_data *data;

    pthread_mutex_lock(&thread_list_mutex);

    SLIST_FOREACH(data, &thread_list, entries) {
        if (data->client_fd != -1) {
            shutdown(data->client_fd, SHUT_RDWR);
        }
    }

    pthread_mutex_unlock(&thread_list_mutex);
}

static void join_all_threads(void)
{
    struct thread_data *data;

    while (!SLIST_EMPTY(&thread_list)) {
        pthread_mutex_lock(&thread_list_mutex);

        data = SLIST_FIRST(&thread_list);
        SLIST_REMOVE_HEAD(&thread_list, entries);

        pthread_mutex_unlock(&thread_list_mutex);

        pthread_join(data->thread_id, NULL);
        free(data);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    int daemon_mode = 0;

    openlog("aesdsocket", 0, LOG_USER);

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    } else if (argc > 1) {
        closelog();
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        closelog();
        return -1;
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    if (daemon_mode) {
        pid_t pid = fork();

        if (pid < 0) {
            syslog(LOG_ERR, "fork failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }

        if (pid > 0) {
            close(server_fd);
            closelog();
            exit(0);
        }

        if (setsid() == -1) {
            syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }

        if (chdir("/") == -1) {
        syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    if (listen(server_fd, 10) == -1) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    /*
     * The timestamp thread is started after daemonization.
     * This way, if -d is used, only the child process creates the timer thread.
     */
    if (pthread_create(&timer_thread, NULL, timer_thread_func, NULL) != 0) {
        syslog(LOG_ERR, "pthread_create timer failed");
        close(server_fd);
        closelog();
        return -1;
    }

    timer_thread_started = 1;

    while (!stop) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int new_client_fd = accept(server_fd,
                                   (struct sockaddr *)&client_addr,
                                   &client_len);

        if (new_client_fd == -1) {
            if (stop) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        struct thread_data *data = calloc(1, sizeof(struct thread_data));
        if (!data) {
            close(new_client_fd);
            continue;
        }

        data->client_fd = new_client_fd;
        data->thread_complete = 0;

        inet_ntop(AF_INET,
                  &client_addr.sin_addr,
                  data->ip,
                  sizeof(data->ip));

        pthread_mutex_lock(&thread_list_mutex);
        SLIST_INSERT_HEAD(&thread_list, data, entries);
        pthread_mutex_unlock(&thread_list_mutex);

        if (pthread_create(&data->thread_id, NULL, client_thread_func, data) != 0) {
            syslog(LOG_ERR, "pthread_create client failed");

            pthread_mutex_lock(&thread_list_mutex);
            SLIST_REMOVE(&thread_list, data, thread_data, entries);
            pthread_mutex_unlock(&thread_list_mutex);

            close(new_client_fd);
            free(data);
            continue;
        }

        join_completed_threads();
    }

    syslog(LOG_INFO, "Caught signal, exiting");

    stop = 1;

    request_threads_exit();
    join_all_threads();

    if (timer_thread_started) {
        pthread_join(timer_thread, NULL);
    }

    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }

    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&thread_list_mutex);

    unlink(FILE_PATH);

    closelog();

    return 0;
}

