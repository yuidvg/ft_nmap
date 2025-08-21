# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: sseneca <marvin@42.fr>                     +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2018/11/08 18:21:56 by sseneca           #+#    #+#              #
#    Updated: 2019/04/03 14:22:31 by sseneca          ###   ########.fr        #
#                                                                              #
# **************************************************************************** #
GREEN = \033[0;32m
YELLOW = \033[1;33m
WHITE = \033[1;37m
OK = $(GREEN)-OK-$(WHITE)

NAME = libft.a

SRCS =./memory.c ./get_next_line.c ./ft_isblank.c ./ft_isspace.c ./ft_str_is_printable.c ./ft_str_is_alnum.c ./ft_str_is_alpha.c ./ft_str_is_numeric.c ./ft_isxdigit.c ./ft_strlcpy.c ./ft_islower.c ./ft_isupper.c ./ft_strrev.c ./ft_lstmap.c ./ft_lstiter.c ./ft_lstadd.c ./ft_lstdel.c ./ft_lstdelone.c ./ft_lstnew.c ./ft_atoi.c ./ft_bzero.c ./ft_isalnum.c ./ft_isalpha.c ./ft_isascii.c ./ft_isdigit.c ./ft_isprint.c ./ft_itoa.c ./ft_memalloc.c ./ft_memccpy.c ./ft_memchr.c ./ft_memcmp.c ./ft_memcpy.c ./ft_memdel.c ./ft_memmove.c ./ft_memset.c ./ft_putchar.c ./ft_putchar_fd.c ./ft_putendl.c ./ft_putendl_fd.c ./ft_putnbr.c ./ft_putnbr_fd.c ./ft_putstr.c ./ft_putstr_fd.c ./ft_strcat.c ./ft_strchr.c ./ft_strclr.c ./ft_strcmp.c ./ft_strcpy.c ./ft_strdel.c ./ft_strdup.c ./ft_strequ.c ./ft_striter.c ./ft_striteri.c ./ft_strjoin.c ./ft_strlcat.c ./ft_strlen.c ./ft_strmap.c ./ft_strmapi.c ./ft_strncat.c ./ft_strncmp.c ./ft_strncpy.c ./ft_strnequ.c ./ft_strnew.c ./ft_strnstr.c ./ft_strrchr.c ./ft_strsplit.c ./ft_strstr.c ./ft_strsub.c ./ft_strtrim.c ./ft_tolower.c ./ft_toupper.c ./get_next_line.c ./ft_tabnew.c ./ft_remchar.c ./ft_strcount.c

HEADER = libft.h

OBJECT = $(SRCS:.c=.o)

FLAGS = -c# -Wall -Wextra -Werror

all : $(NAME)

$(NAME) :
	@gcc $(FLAGS) $(SRCS) $(HEADER)
	@ar -rc $(NAME) $(OBJECT)
	@ranlib $(NAME)
	@echo "$(YELLOW)-$(NAME)	$(OK)"

clean :
	@rm -f $(OBJECT)
	@echo "$(YELLOW)-LIB CLEAN	$(OK)"

fclean : clean
	@rm -f $(NAME)
	@echo "$(YELLOW)-LIB FCLEAN	$(OK)"

re : fclean all
