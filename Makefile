# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: abenamar <abenamar@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/28 09:32:29 by abenamar          #+#    #+#              #
#    Updated: 2025/05/08 11:33:49 by abenamar         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME := mini_serv

SRCS := mini_serv.c

OBJS := $(SRCS:.c=.o)

CC := cc

CFLAGS := -Wall
CFLAGS += -Wextra
CFLAGS += -Werror
CFLAGS += -g3
CFLAGS += -DNDEBUG

MEM := valgrind

MEMFLAGS := --show-leak-kinds=all
MEMFLAGS += --leak-check=full
MEMFLAGS += --track-fds=yes

RM := rm -f

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJS)

all: $(NAME)

8081: all
	$(MEM) $(MEMFLAGS) ./$(NAME) 8081

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

debug: CFLAGS += -UNDEBUG
debug: re

.PHONY: debug re fclean clean 8081 all