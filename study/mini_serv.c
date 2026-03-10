#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

int     sockfd, maxfd, g_id = 0;
int     ids[65536];
char    *msgs[65536];
char    buff[1025], info[128];
fd_set  fds, read_fds, write_fds;

void	fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	broadcast(int sender, char *str)
{
	for (int fd = 0; fd <= maxfd; fd++)
		if (FD_ISSET(fd, &write_fds) && fd != sender)
			send(fd, str, strlen(str), 0);
}

char	*str_join(char *buf, char *add)
{
	char	*new;
	int		len = buf ? strlen(buf) : 0;

	new = malloc(len + strlen(add) + 1);
	if (!new) fatal();
	new[0] = 0;
	if (buf) strcat(new, buf);
	free(buf);
	return strcat(new, add);
}

int		extract_message(char **buf, char **msg)
{
	char	*new;
	int		i = 0;

	if (!*buf) return (0);
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			new = calloc(1, strlen(*buf + i + 1) + 1);
			if (!new) fatal();
			strcpy(new, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = new;
			return (1);
		}
		i++;
	}
	return (0);
}

int		main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) fatal();

	struct sockaddr_in serv = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(2130706433),
		.sin_port = htons(atoi(av[1]))
	};

	if (bind(sockfd, (const struct sockaddr *)&serv, sizeof(serv)) < 0) fatal();
	if (listen(sockfd, 128) < 0) fatal();

	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	maxfd = sockfd;

	while (1)
	{
		read_fds = write_fds = fds;
		if (select(maxfd + 1, &read_fds, &write_fds, 0, 0) < 0) continue;

		for (int fd = 0; fd <= maxfd; fd++)
		{
			if (!FD_ISSET(fd, &read_fds)) continue;

			if (fd == sockfd)
			{
				struct sockaddr_in cli;
				socklen_t len = sizeof(cli);
				int conn = accept(sockfd, (struct sockaddr *)&cli, &len);
				if (conn < 0) continue;

				maxfd = (conn > maxfd) ? conn : maxfd;
				ids[conn] = g_id++;
				msgs[conn] = NULL;
				FD_SET(conn, &fds);

				sprintf(info, "server: client %d just arrived\n", ids[conn]);
				broadcast(conn, info);
			}
			else
			{
				int n = recv(fd, buff, 1024, 0);
				if (n <= 0)
				{
					sprintf(info, "server: client %d just left\n", ids[fd]);
					broadcast(fd, info);
					FD_CLR(fd, &fds);
					close(fd);
					free(msgs[fd]);
					msgs[fd] = NULL;
				}
				else
				{
					buff[n] = 0;
					msgs[fd] = str_join(msgs[fd], buff);
					char *m = NULL;
					while (extract_message(&msgs[fd], &m))
					{
						sprintf(info, "client %d: ", ids[fd]);
						broadcast(fd, info);
						broadcast(fd, m);
						free(m);
					}
				}
			}
		}
	}
}