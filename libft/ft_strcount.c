/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strcount.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/07/01 03:23:28 by sseneca           #+#    #+#             */
/*   Updated: 2019/07/01 03:23:41 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

int	ft_count(char *str, char c)
{
	int i;
	int len;
	int	count;

	i = -1;
	len = ft_strlen(str);
	count = 0;
	while (++i < len)
		if (str[i] == c)
			count++;
	return (count);
}
