#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"

static int server_fd = -1;
static int client_fd = -1;
static volatile sig_atomic_t stop = 0;

static void signal_handler(int signo)
{
    (void)signo;
    stop = 1;
    syslog(LOG_INFO, "Caught signal, exiting");

    if (server_fd != -1)
        shutdown(server_fd, SHUT_RDWR);

    if (client_fd != -1)
        shutdown(client_fd, SHUT_RDWR);
}

static int send_packet_on_buffer(int fd, const char *buf, size_t len){
	size_t sent=0;
	while(sent<len){
		ssize_t rc=send(fd,buf+sent,len-sent,0);
		if(rc<=0) return -1;
		sent+=rc;
	}
	return 0;
}

static int send_file_to_client(int fd)
{
    FILE *fp = fopen(FILE_PATH, "r");
    if (!fp)
        return -1;

    char buf[1024];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
	if(send_packet_on_buffer(fd,buf,n)==-1){
		fclose(fp);
		return -1;
        }
    }

    fclose(fp);
    return 0;
}


static int handle_client(int fd)
{
    char recvbuf[1024];
    char *packet = NULL;
    size_t packet_len = 0;

    while (!stop) {
        ssize_t bytes = recv(fd, recvbuf, sizeof(recvbuf), 0);

        if (bytes <= 0)
            break;

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

            FILE *fp = fopen(FILE_PATH, "a");
            if (!fp) {
                free(packet);
                return -1;
            }

            if(fwrite(packet, 1, line_len, fp)!=line_len){
            	fclose(fp);
            	free(packet);
            	return -1;
            }
	    fclose(fp);
            send_file_to_client(fd);

            memmove(packet, packet + line_len, packet_len - line_len);
            packet_len -= line_len;
            packet[packet_len] = '\0';
        }
    }

    free(packet);
    return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    openlog("aesdsocket", 0, LOG_USER);
    
    int daemon_mode = 0;

	if (argc == 2 && strcmp(argv[1], "-d") == 0) {
	    daemon_mode = 1;
	} 
	else if (argc > 1) {
	    return -1;
	}

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        return -1;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        return -1;
        
    if (daemon_mode) {
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid > 0) {
        close(server_fd);
        closelog();
        exit(0);
    }

    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        return -1;
    }

    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    }

    if (listen(server_fd, 10) == -1)
        return -1;

    while (!stop) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char ip[INET_ADDRSTRLEN];

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (stop)
                break;
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        syslog(LOG_INFO, "Accepted connection from %s", ip);

        handle_client(client_fd);

        close(client_fd);
        client_fd = -1;

        syslog(LOG_INFO, "Closed connection from %s", ip);
    }

    if (server_fd != -1)
        close(server_fd);

    unlink(FILE_PATH);
    closelog();

    return 0;
}
