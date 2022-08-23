#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

#define BUFFER_SIZE 4096

typedef struct s_client {
	int id;
	int fd;
	struct s_client *next;
}	t_client;

char msg[42];
char * str = NULL;

int g_id = 0;
t_client * g_clients = NULL;
int sock_fd;
fd_set cpy_read, cpy_write, curr_sock;

void fatal() {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(1);
}

int get_id(int fd) {
	t_client * tmp = g_clients;
	while (tmp) {
		if (tmp->fd == fd)
			return tmp->id;
		tmp = tmp->next;
	}
	return -1;
}

int get_max_fd() {
	int max_sd = sock_fd;
	t_client * tmp = g_clients;
	while (tmp) {
		if (tmp->fd > max_sd)
			max_sd = tmp->fd;
		tmp = tmp->next;
	}
	return max_sd;
}

void send_all(int fd, char *str_req) {
	t_client * tmp = g_clients;
	while (tmp) {
		if (tmp->fd != fd && FD_ISSET(tmp->fd, &cpy_write)) {
			if (send(tmp->fd, str_req, strlen(str_req), 0) < 0)
				fatal();
		}
		tmp = tmp->next;
	}
}

int add_client_to_list(int fd) {
	t_client * new = calloc(1, sizeof(t_client));
	if (new == NULL)
		fatal();
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;
	t_client * tmpClt = g_clients;
	if (g_clients == NULL) {
		g_clients = new;
	}
	else {
		while (tmpClt->next) {
			tmpClt = tmpClt->next;
		}
		tmpClt->next = new;
	}
	return new->id;
}

void add_client() {
	struct sockaddr_in client_addr; 
	socklen_t len = sizeof(client_addr);
	int new_connection = accept(sock_fd, (struct sockaddr *)&client_addr, &len);
	if (new_connection < 0)
		fatal();
	sprintf(msg, "server: client %d just arrived\n", add_client_to_list(new_connection));
	send_all(new_connection, msg);
	FD_SET(new_connection, &curr_sock);
}

int rm_client(int fd) {
	t_client * tmp = g_clients;
	int id;
	t_client * del;
	if (g_clients != NULL && g_clients->fd == fd) {
		del = g_clients;
		g_clients = g_clients->next;
		id = del->id;
		free(del);
		return (id);
	}

	while (tmp && tmp->next && tmp->next->fd != fd)
		tmp = tmp->next;

	del = tmp->next;
	id = del->id;
	tmp->next = tmp->next->next;
	free(del);
	return id;
}

void ex_msg(int fd) {
	int i = 0;

	while (str[i]) {
		if (str[i] == '\n') {
			char old_char = str[i + 1];
			str[i + 1] = '\0';
			char *tmp_buf = calloc(strlen(str) + 4096, sizeof(char));
			if (tmp_buf == NULL)
				fatal();
			sprintf(tmp_buf, "client %d: %s", get_id(fd), str);
			send_all(fd, tmp_buf);
			free(tmp_buf);
			str[i + 1] = old_char;
		}
		i++;
	}
	free(str);
	str = calloc(42 * BUFFER_SIZE, sizeof(char));
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int main(int argc, char **argv) {

	if (argc != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}

	struct sockaddr_in servaddr; 

	// socket create and verification 
	sock_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sock_fd == -1)
		fatal();
	
	printf("Socket successfully created..\n");

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	int port = atoi(argv[1]);
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	printf("Socket successfully binded..\n");

	if (listen(sock_fd, 10) != 0)
		fatal();

	printf("Listening at port %d...\n", port);

	FD_ZERO(&curr_sock);
	FD_SET(sock_fd, &curr_sock);
	str = calloc(42 * BUFFER_SIZE, sizeof(char));

	while (1) {
		cpy_read = cpy_write = curr_sock;

		int select_ret = select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, NULL);
		if (select_ret < 0)
			continue ;

		for (int fd = 0;fd <= get_max_fd(); fd++) {

			if (FD_ISSET(fd, &cpy_read)) {
				if (fd == sock_fd) {
					bzero(&msg, sizeof(msg));
					add_client();
					break ;
				}
				else {
					int received = BUFFER_SIZE - 1;
					char buffer[BUFFER_SIZE];
					bzero(&buffer, sizeof(buffer));
					while (received == BUFFER_SIZE - 1 || buffer[strlen(buffer) - 1] != '\n') {
						received = recv(fd, buffer, BUFFER_SIZE - 1, 0);
						if (received <= 0)
							break;
						buffer[received] = '\0';
						str = str_join(str, buffer);
					}
					if (received <= 0) {
						bzero(&msg, strlen(msg));
						sprintf(msg, "server: client %d just left\n", rm_client(fd));
						send_all(fd, msg);
						FD_CLR(fd, &curr_sock);
						close(fd);
						break ;
					}
					else {
						ex_msg(fd);
					}
				}
			}
		}
	}
	return 0;
}
