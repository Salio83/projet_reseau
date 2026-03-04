#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		return 1;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
		perror("inet_pton");
		close(sock);
		return 1;
	}
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect");
		close(sock);
		return 1;
	}
	printf("Connected to server\n");
	close(sock);
	return 0;
}