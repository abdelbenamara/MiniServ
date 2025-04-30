/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abenamar <abenamar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 09:37:07 by abenamar          #+#    #+#             */
/*   Updated: 2025/04/30 14:20:10 by abenamar         ###   ########.fr       */
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

typedef struct s_srver
{
  int sockfd, maxfd;
  t_client *clients;
  char rbuf[BUFSIZ];
  fd_set fds, rfds, wfds;
} t_server;

static void ft_fatal(char const *const str)
{
  if (str)
    write(STDERR_FILENO, str, strlen(str));
  else
    write(STDERR_FILENO, "Fatal error\n", 12);
  return (exit(EXIT_FAILURE));
}

static void ft_init_server(t_server *srv, in_port_t const port)
{
  struct sockaddr_in addr;

  srv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == srv->sockfd)
    return (ft_fatal(NULL));
  srv->maxfd = srv->sockfd + 1;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (bind(srv->sockfd, (struct sockaddr const *)&addr, sizeof(addr)) ||
      listen(srv->sockfd, SOMAXCONN) ||
      0 > sprintf(srv->rbuf,
                  "server: accepting connections on port %d\n", port) ||
      -1 == write(STDOUT_FILENO, srv->rbuf, strlen(srv->rbuf)))
    return (close(srv->sockfd), ft_fatal(NULL));
  srv->clients = NULL;
  FD_ZERO(&srv->fds);
  srv->rfds = srv->fds;
  srv->wfds = srv->fds;
  return;
}

static void ft_free_server(t_server *srv)
{
  t_client *prev, *curr;
  int err;

  err = 0;
  if (close(srv->sockfd))
    err = -1;
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
  if (err)
    return (ft_fatal(NULL));
  return;
}

static int extract_message(char **buf, char **msg)
{
  char *newbuf;
  int i;

  *msg = 0;
  if (!*buf)
    return (0);
  i = 0;
  while ((*buf)[i])
  {
    if ((*buf)[i] == '\n')
    {
      newbuf = calloc(1, sizeof(char) * (strlen(*buf + i + 1) + 1));
      if (newbuf == 0)
        return (-1);
      strcpy(newbuf, *buf + i + 1);
      *msg = *buf;
      (*msg)[i + 1] = 0;
      *buf = newbuf;
      return (1);
    }
    i++;
  }
  return (0);
}

static char *str_join(char *buf, char *add)
{
  char *newbuf;
  int len;

  if (!buf)
    len = 0;
  else
    len = strlen(buf);
  newbuf = malloc(sizeof(char) * (len + strlen(add) + 1));
  if (!newbuf)
    return (0);
  newbuf[0] = 0;
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
  int ready, buflen;
  size_t msglen;

  prev = srv->clients;
  while (prev)
  {
    if (prev->connfd == fd)
      break;
    prev = prev->next;
  }
  prev->buf = str_join(prev->buf, srv->rbuf);
  if (!prev->buf)
    return (ft_free_server(srv), ft_fatal(NULL));
  buflen = sprintf(srv->rbuf, format, prev->id);
  if (0 > buflen)
    return (ft_free_server(srv), ft_fatal(NULL));
  while (*prev->buf)
  {
    ready = extract_message(&prev->buf, &msg);
    if (-1 == ready)
      return (ft_free_server(srv), ft_fatal(NULL));
    else if (!ready)
      break;
    msglen = strlen(msg);
    curr = srv->clients;
    while (curr)
    {
      if (curr->connfd != fd && FD_ISSET(curr->connfd, &srv->wfds))
      {
        if (-1 == write(curr->connfd, srv->rbuf, buflen) ||
            -1 == write(curr->connfd, msg, msglen))
          return (free(msg), ft_free_server(srv), ft_fatal(NULL));
      }
      curr = curr->next;
    }
    free(msg);
  }
  return;
}

static void ft_add_client(t_server *srv, int const fd)
{
  static int id = 0;
  t_client *new;

  new = malloc(sizeof(t_client));
  if (!new)
    return (ft_free_server(srv), ft_fatal(NULL));
  if (-1 == fd)
    new->connfd = accept(srv->sockfd, NULL, NULL);
  else
    new->connfd = fd;
  if (-1 == new->connfd)
    return (free(new), ft_free_server(srv), ft_fatal(NULL));
  if (srv->maxfd >= FD_SETSIZE)
  {
    if (close(new->connfd))
      return (free(new), ft_free_server(srv), ft_fatal(NULL));
    return (free(new));
  }
  if (srv->maxfd <= new->connfd)
    srv->maxfd = new->connfd + 1;
  if (-1 == fd)
    new->id = id++;
  else
    new->id = -1;
  new->buf = NULL;
  new->next = srv->clients;
  srv->clients = new;
  FD_SET(new->connfd, &srv->fds);
  if (0 > sprintf(srv->rbuf, "client %d just arrived\n", new->id))
    return (ft_free_server(srv), ft_fatal(NULL));
  ft_broadcast(srv, new->connfd, "server: ");
  return;
}

static void ft_del_client(t_server *srv, int const fd)
{
  t_client *prev, *curr;

  srv->maxfd = srv->sockfd + 1;
  prev = NULL;
  curr = srv->clients;
  while (curr)
  {
    if (curr->connfd == fd)
    {
      if (0 > sprintf(srv->rbuf, "client %d just left\n", curr->id))
        return (ft_free_server(srv), ft_fatal(NULL));
      ft_broadcast(srv, fd, "server: ");
      if (!prev)
      {
        prev = curr->next;
        srv->clients = prev;
      }
      else
        prev->next = curr->next;
      FD_CLR(curr->connfd, &srv->fds);
      if (close(curr->connfd))
        return (ft_free_server(srv), ft_fatal(NULL));
      free(curr->buf);
      free(curr);
      curr = prev;
    }
    else if (srv->maxfd <= curr->connfd)
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
    ft_fatal("Wrong number of arguments\n");
  ft_init_server(&srv, atoi(argv[1]));
  ft_add_client(&srv, STDIN_FILENO);
  ft_add_client(&srv, STDOUT_FILENO);
  while (1)
  {
    srv.rfds = srv.fds;
    FD_SET(srv.sockfd, &srv.rfds);
    FD_CLR(STDOUT_FILENO, &srv.rfds);
    srv.wfds = srv.fds;
    FD_CLR(STDIN_FILENO, &srv.wfds);
    nfds = select(srv.maxfd, &srv.rfds, &srv.wfds, NULL, NULL);
    if (-1 == nfds)
      return (ft_free_server(&srv), ft_fatal(NULL), EXIT_FAILURE);
    if (FD_ISSET(STDIN_FILENO, &srv.rfds))
      break;
    fd = STDOUT_FILENO;
    while (nfds && fd < srv.maxfd)
    {
      if (FD_ISSET(fd, &srv.rfds))
      {
        if (fd == srv.sockfd)
          ft_add_client(&srv, -1);
        else
        {
          nbytes = BUFSIZ;
          while (nbytes == BUFSIZ)
          {
            nbytes = recv(fd, srv.rbuf, BUFSIZ, 0);
            if (-1 == nbytes)
              return (ft_free_server(&srv), ft_fatal(NULL), EXIT_FAILURE);
            srv.rbuf[nbytes] = '\0';
            if (!nbytes)
              ft_del_client(&srv, fd);
            else
              ft_broadcast(&srv, fd, "client %d: ");
          }
        }
        --nfds;
      }
      ++fd;
    }
  }
  if (0 > sprintf(srv.rbuf, "shutting down...\n"))
    return (ft_free_server(&srv), ft_fatal(NULL), EXIT_FAILURE);
  return (ft_broadcast(&srv, STDIN_FILENO, "server: "),
          ft_free_server(&srv), EXIT_SUCCESS);
}
