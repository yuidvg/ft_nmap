/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_remchar.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/07/01 01:50:23 by sseneca           #+#    #+#             */
/*   Updated: 2019/07/01 01:52:19 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

int		ft_remchar(char *str, char c, size_t n)
{
	char	*src;
	int		i;

	i = 0;
	if (!str)
		return (0);
	src = str;
	while (n--)
	{
		*str = *src;
		if (*str != c)
		{
			str++;
			i++;
		}
		src++;
	}
	*str = '\0';
	return (i);
}
