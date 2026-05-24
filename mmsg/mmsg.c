#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: mmsg <command> [args...]\n");
		fprintf(stderr, "  get <type> ...      one-shot request\n");
		fprintf(stderr, "  watch <type> ...    persistent stream\n");
		return EXIT_FAILURE;
	}

	const char *socket_path = getenv("MANGO_INSTANCE_SIGNATURE");
	if (!socket_path) {
		fprintf(stderr, "Error: MANGO_INSTANCE_SIGNATURE is not set. Did you "
						"run 'mmsg' in mango?\n");
		return EXIT_FAILURE;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	struct sockaddr_un addr = {.sun_family = AF_UNIX};
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		close(sock);
		return EXIT_FAILURE;
	}

	// 拼接命令，缓冲区大小 4096 以容纳较长参数
	char cmd[4096] = {0};
	int offset = 0;
	for (int i = 1; i < argc; i++) {
		int n = snprintf(cmd + offset, sizeof(cmd) - offset, "%s%s", argv[i],
						 (i == argc - 1) ? "" : " ");
		if (n < 0 || n >= (int)(sizeof(cmd) - offset)) {
			fprintf(stderr, "Error: command too long.\n");
			close(sock);
			return EXIT_FAILURE;
		}
		offset += n;
	}

	// 添加换行符
	int n = snprintf(cmd + offset, sizeof(cmd) - offset, "\n");
	if (n < 0 || n >= (int)(sizeof(cmd) - offset)) {
		fprintf(stderr, "Error: command too long to append newline.\n");
		close(sock);
		return EXIT_FAILURE;
	}

	// 发送命令，使用 MSG_NOSIGNAL 避免 SIGPIPE
	if (send(sock, cmd, strlen(cmd), MSG_NOSIGNAL) < 0) {
		perror("send");
		close(sock);
		return EXIT_FAILURE;
	}

	// 将 socket 封装为行缓冲文件流，自动处理 TCP 拆包，按完整行读取
	FILE *stream = fdopen(sock, "r");
	if (!stream) {
		perror("fdopen");
		close(sock);
		return EXIT_FAILURE;
	}

	// 按行读取并输出，直到连接关闭（get 模式服务端主动 close）或出错
	char *line = NULL;
	size_t len = 0;
	while (getline(&line, &len, stream) != -1) {
		printf("%s", line);
		fflush(stdout);
	}

	// 检查是否因读取错误退出（而非正常 EOF）
	if (ferror(stream)) {
		perror("recv");
		free(line);
		fclose(stream); // 关闭 stream 同时关闭 socket
		return EXIT_FAILURE;
	}

	free(line);
	fclose(stream);
	return EXIT_SUCCESS;
}