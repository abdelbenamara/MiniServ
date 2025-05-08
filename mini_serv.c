/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abenamar <abenamar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 09:37:07 by abenamar          #+#    #+#             */
/*   Updated: 2025/05/08 11:32:52 by abenamar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct s_client
{
  int id;
  char *buf;
} t_client;

typedef struct s_server
{
  int sockfd, maxfd;
  t_client clients[FD_SETSIZE];
  char rbuf[BUFSIZ + 1];
  fd_set fds, rfds, wfds;
} t_server;

static void ft_error(char const *const msg)
{
  if (msg)
    write(STDERR_FILENO, msg, strlen(msg));
  else
    write(STDERR_FILENO, "Fatal error\n", 12);
  return (exit(EXIT_FAILURE));
}

static void ft_server_stop(t_server *srv)
{
  int err, fd;
  t_client *clt;

  err = 0;
  fd = -1;
  clt = srv->clients;
  while (++fd < srv->maxfd)
  {
    if (-1 < clt[fd].id && close(fd))
      err = -1;
    free(clt[fd].buf);
  }
  if (close(srv->sockfd))
    err = -1;
  if (err)
    return (ft_error(NULL));
  return;
}

static void ft_fatal(t_server *srv)
{
  return (ft_server_stop(srv), ft_error(NULL));
}

static int extract_message(char **buf, char **msg)
{
  int i;
  char *newbuf;

  *msg = NULL;
  if (!*buf)
    return (0);
  i = 0;
  while ((*buf)[i])
  {
    if ((*buf)[i] == '\n')
    {
      newbuf = calloc(1, sizeof(char) * (strlen(*buf + i + 1) + 1));
      if (!newbuf)
        return (-1);
      strcpy(newbuf, *buf + i + 1);
      *msg = *buf;
      (*msg)[i + 1] = '\0';
      *buf = newbuf;
      return (1);
    }
    ++i;
  }
  return (0);
}

static char *str_join(char *buf, char *add)
{
  int len;
  char *newbuf;

  if (!buf)
    len = 0;
  else
    len = strlen(buf);
  newbuf = malloc(sizeof(char) * (len + strlen(add) + 1));
  if (!newbuf)
    return (NULL);
  newbuf[0] = '\0';
  if (buf)
    strcat(newbuf, buf);
  free(buf);
  strcat(newbuf, add);
  return (newbuf);
}

static void ft_broadcast(t_server *srv, int const fd, char const *format)
{
  t_client *clt;
  int buflen, connfd, ready;
  char *msg;
  size_t msglen;

  clt = srv->clients;
  clt[fd].buf = str_join(clt[fd].buf, srv->rbuf);
  if (!clt[fd].buf)
    return (ft_fatal(srv));
  buflen = sprintf(srv->rbuf, format, clt[fd].id);
  if (0 > buflen)
    return (ft_fatal(srv));
  while (*clt[fd].buf)
  {
    ready = extract_message(&clt[fd].buf, &msg);
    if (-1 == ready)
      return (ft_fatal(srv));
    else if (!ready)
      break;
    msglen = strlen(msg);
    connfd = -1;
    while (++connfd < srv->maxfd)
    {
      if (fd != connfd && FD_ISSET(connfd, &srv->wfds))
      {
        if (-1 == write(connfd, srv->rbuf, buflen) ||
            -1 == write(connfd, msg, msglen))
          return (free(msg), ft_fatal(srv));
      }
    }
    free(msg);
  }
  return;
}

static void ft_notify(t_server *srv, int const fd, int const nbytes)
{
  if (0 > nbytes)
    return (ft_fatal(srv));
  return (ft_broadcast(srv, fd, "server: "));
}

static void ft_client_add(t_server *srv, int const fd)
{
  static int id = -1;
  t_client *clt;

  if (-1 == fd)
    return (ft_fatal(srv));
  else if (srv->maxfd >= FD_SETSIZE)
  {
    if (close(fd))
      return (ft_fatal(srv));
    ft_notify(srv, STDIN_FILENO, sprintf(srv->rbuf, "connection refused\n"));
    return;
  }
  clt = srv->clients;
  clt[fd].id = ++id;
  clt[fd].buf = NULL;
  if (srv->maxfd <= fd)
    srv->maxfd = fd + 1;
  FD_SET(fd, &srv->fds);
  ft_notify(srv, fd, sprintf(srv->rbuf, "client %d just arrived\n", id));
  return;
}

static void ft_server_start(t_server *srv, in_port_t const port)
{
  int fd;
  t_client *clt;
  struct sockaddr_in addr;

  srv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == srv->sockfd)
    return (ft_error(NULL));
  srv->maxfd = srv->sockfd + 1;
  fd = -1;
  clt = srv->clients;
  while (++fd < FD_SETSIZE)
  {
    clt[fd].id = -1;
    clt[fd].buf = NULL;
  }
  srv->rbuf[BUFSIZ] = '\0';
  FD_ZERO(&srv->fds);
  srv->rfds = srv->fds;
  srv->wfds = srv->fds;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (bind(srv->sockfd, (struct sockaddr const *)&addr, sizeof(addr)) ||
      listen(srv->sockfd, SOMAXCONN))
    return (ft_fatal(srv));
#ifndef NDEBUG
  FD_SET(STDOUT_FILENO, &srv->wfds);
#endif
  ft_notify(srv, STDIN_FILENO, sprintf(srv->rbuf, "listen on port %d\n", port));
  return;
}

static void ft_client_remove(t_server *srv, int const fd)
{
  t_client *clt;
  int topfd, connfd;

  clt = srv->clients;
  ft_notify(srv, fd, sprintf(srv->rbuf, "client %d just left\n", clt[fd].id));
  topfd = srv->maxfd;
  srv->maxfd = srv->sockfd + 1;
  FD_CLR(fd, &srv->fds);
  if (close(fd))
    return (ft_fatal(srv));
  free(clt[fd].buf);
  clt[fd].id = -1;
  clt[fd].buf = NULL;
  connfd = -1;
  while (++connfd < topfd)
    if (-1 < clt[connfd].id && srv->maxfd <= connfd)
      srv->maxfd = connfd + 1;
  return;
}

int main(int argc, char **argv)
{
  t_server srv;
  int nfds, fd, nbytes;

  if (argc < 2)
    return (ft_error("Wrong number of arguments\n"), EXIT_FAILURE);
  ft_server_start(&srv, atoi(argv[1]));
  while (1)
  {
    srv.rfds = srv.fds;
    FD_SET(srv.sockfd, &srv.rfds);
#ifndef NDEBUG
    FD_SET(STDIN_FILENO, &srv.rfds);
#endif
    srv.wfds = srv.fds;
#ifndef NDEBUG
    FD_SET(STDOUT_FILENO, &srv.wfds);
#endif
    nfds = select(srv.maxfd, &srv.rfds, &srv.wfds, NULL, NULL);
    if (-1 == nfds)
      return (ft_fatal(&srv), EXIT_FAILURE);
#ifndef NDEBUG
    else if (FD_ISSET(STDIN_FILENO, &srv.rfds))
      break;
#endif
    fd = STDIN_FILENO;
    while (nfds && ++fd < srv.maxfd)
    {
      if (!FD_ISSET(fd, &srv.rfds))
        continue;
      else if (fd == srv.sockfd)
        ft_client_add(&srv, accept(srv.sockfd, NULL, NULL));
      else
      {
        nbytes = BUFSIZ;
        while (BUFSIZ == nbytes)
        {
          nbytes = recv(fd, srv.rbuf, BUFSIZ, 0);
          if (-1 == nbytes)
            return (ft_fatal(&srv), EXIT_FAILURE);
          srv.rbuf[nbytes] = '\0';
          if (!nbytes)
            ft_client_remove(&srv, fd);
          else
            ft_broadcast(&srv, fd, "client %d: ");
        }
      }
      --nfds;
    }
  }
  ft_notify(&srv, STDIN_FILENO, sprintf(srv.rbuf, "shutting down...\n"));
  return (ft_server_stop(&srv), EXIT_SUCCESS);
}
