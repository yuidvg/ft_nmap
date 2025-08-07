/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_next_line.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/02/10 14:23:40 by sseneca           #+#    #+#             */
/*   Updated: 2019/04/03 14:23:56 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef GET_NEXT_LINE_H
# define GET_NEXT_LINE_H

# include <limits.h>
//# include <unistd.h>
# include <stdlib.h>
# include <fcntl.h>

# define BUFF_SIZE 10000
# define SUCCESS 1
# define FINISH 0
# define ERROR -1
# define OPEN_MAX 512
# define MAX OPEN_MAX
# define EOL '\n'

int					get_next_line(const int fd, char **line);

#endif
