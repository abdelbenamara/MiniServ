/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abenamar <abenamar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 09:37:07 by abenamar          #+#    #+#             */
/*   Updated: 2025/05/06 01:05:52 by abenamar         ###   ########.fr       */
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
  int connfd, id;
  char *buf;
  struct s_client *next;
} t_client;

typedef struct s_server
{
  int sockfd, maxfd;
  t_client *clients;
  char rbuf[BUFSIZ];
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
  int err;
  t_client *prev, *curr;

  err = 0;
  curr = srv->clients;
  while (curr)
  {
    if (close(curr->connfd))
      err = -1;
    free(curr->buf);
    prev = curr;
    curr = curr->next;
    free(prev);
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
  t_client *prev, *curr;
  char *msg;
  int buflen, ready;
  size_t msglen;

  prev = srv->clients;
  while (prev && fd != prev->connfd)
    prev = prev->next;
  prev->buf = str_join(prev->buf, srv->rbuf);
  if (!prev->buf)
    return (ft_fatal(srv));
  buflen = sprintf(srv->rbuf, format, prev->id);
  if (0 > buflen)
    return (ft_fatal(srv));
  while (*prev->buf)
  {
    ready = extract_message(&prev->buf, &msg);
    if (-1 == ready)
      return (ft_fatal(srv));
    else if (!ready)
      break;
    msglen = strlen(msg);
    curr = srv->clients;
    while (curr)
    {
      if (fd != curr->connfd && FD_ISSET(curr->connfd, &srv->wfds))
      {
        if (-1 == write(curr->connfd, srv->rbuf, buflen) ||
            -1 == write(curr->connfd, msg, msglen))
          return (free(msg), ft_fatal(srv));
      }
      curr = curr->next;
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
  static int id = -2;
  t_client *new;

  if (-1 == fd)
    return (ft_fatal(srv));
  else if (srv->maxfd >= FD_SETSIZE)
  {
    if (close(fd))
      return (ft_fatal(srv));
    ft_notify(srv, STDIN_FILENO, sprintf(srv->rbuf, "connection refused\n"));
    return;
  }
  new = malloc(sizeof(t_client));
  if (!new)
    return (close(fd), ft_fatal(srv));
  new->connfd = fd;
  new->id = id++;
  if (srv->maxfd <= fd)
    srv->maxfd = fd + 1;
  new->buf = NULL;
  new->next = srv->clients;
  srv->clients = new;
  FD_SET(fd, &srv->fds);
  ft_notify(srv, fd, sprintf(srv->rbuf, "client %d just arrived\n", new->id));
  return;
}

static void ft_server_start(t_server *srv, in_port_t const port)
{
  struct sockaddr_in addr;

  srv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == srv->sockfd)
    return (ft_error(NULL));
  srv->maxfd = srv->sockfd + 1;
  srv->clients = NULL;
  FD_ZERO(&srv->fds);
  srv->rfds = srv->fds;
  srv->wfds = srv->fds;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (bind(srv->sockfd, (struct sockaddr const *)&addr, sizeof(addr)) ||
      listen(srv->sockfd, SOMAXCONN))
    return (ft_fatal(srv));
  ft_client_add(srv, STDIN_FILENO);
  ft_client_add(srv, STDOUT_FILENO);
  FD_SET(STDOUT_FILENO, &srv->wfds);
  ft_notify(srv, STDIN_FILENO, sprintf(srv->rbuf, "listen on port %d\n", port));
  return;
}

static void ft_client_remove(t_server *srv, int const fd)
{
  t_client *prev, *curr;

  srv->maxfd = srv->sockfd + 1;
  prev = NULL;
  curr = srv->clients;
  while (curr)
  {
    if (fd == curr->connfd)
    {
      ft_notify(srv, fd, sprintf(srv->rbuf, "client %d just left\n", curr->id));
      FD_CLR(fd, &srv->fds);
      if (close(fd))
        return (ft_fatal(srv));
      else if (!prev)
      {
        prev = curr->next;
        srv->clients = prev;
      }
      else
        prev->next = curr->next;
      free(curr->buf);
      free(curr);
      curr = prev;
    }
    if (srv->maxfd <= curr->connfd)
      srv->maxfd = curr->connfd + 1;
    prev = curr;
    curr = curr->next;
  }
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
    FD_CLR(STDOUT_FILENO, &srv.rfds);
    srv.wfds = srv.fds;
    FD_CLR(STDIN_FILENO, &srv.wfds);
    nfds = select(srv.maxfd, &srv.rfds, &srv.wfds, NULL, NULL);
    if (-1 == nfds)
      return (ft_fatal(&srv), EXIT_FAILURE);
    else if (FD_ISSET(STDIN_FILENO, &srv.rfds))
      break;
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
