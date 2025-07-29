#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

int test_port(const char *target_ip, int port, int timeout_ms) {
    int sock;
    struct sockaddr_in addr;
    int result;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Setup address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_aton(target_ip, &addr.sin_addr) == 0) {
        printf("Invalid IP address: %s\n", target_ip);
        close(sock);
        return -1;
    }

    // Attempt connection
    result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (result == 0) {
        // Connected immediately
        printf("Port %d is OPEN (immediate connection)\n", port);
        close(sock);
        return 1;
    } else if (errno == EINPROGRESS) {
        // Connection in progress, use select to wait
        fd_set write_fds;
        struct timeval timeout;

        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);

        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        result = select(sock + 1, NULL, &write_fds, NULL, &timeout);

        if (result > 0) {
            // Check if connection succeeded
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
                if (error == 0) {
                    printf("Port %d is OPEN (connected)\n", port);
                    close(sock);
                    return 1;
                } else {
                    printf("Port %d is CLOSED (connection refused: %s)\n", port, strerror(error));
                    close(sock);
                    return 0;
                }
            }
        } else if (result == 0) {
            printf("Port %d is FILTERED (timeout)\n", port);
            close(sock);
            return 0;
        }
    } else {
        printf("Port %d is CLOSED (connection failed: %s)\n", port, strerror(errno));
        close(sock);
        return 0;
    }

    close(sock);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <target_ip> <port>\n", argv[0]);
        printf("Example: %s 8.8.8.8 80\n", argv[0]);
        printf("Example: %s google.com 443\n", argv[0]);
        return 1;
    }

    const char *target_ip = argv[1];
    int port = atoi(argv[2]);

    if (port <= 0 || port > 65535) {
        printf("Invalid port number: %d. Must be between 1 and 65535.\n", port);
        return 1;
    }

    printf("Testing connection to %s:%d...\n", target_ip, port);
    test_port(target_ip, port, 3000);  // 3 second timeout

    return 0;
}