/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_atoi.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/13 17:56:48 by sseneca           #+#    #+#             */
/*   Updated: 2018/12/01 13:09:50 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int	ft_long(const char *str)
{
	char *max;
	char *min;

	max = "2147483647";
	min = "2147483648";
	if (*str == 43)
		str++;
	while (*str && *str > 47 && *str < 58 && *max && *max++ < *str++)
		str++;
	if (*max == '\0')
		return (-1);
	else if (*str == 45)
	{
		str++;
		while (*str && *str > 47 && *str < 58 && *min && *min++ < *str++)
			str++;
		if (*min == '\0')
			return (0);
	}
	return (1);
}

int			ft_atoi(const char *str)
{
	int i;
	int	s;
	int nega;
	int vlong;

	nega = 0;
	s = 0;
	i = 0;
	vlong = ft_long(str);
	while ((str[i] >= 9 && str[i] <= 13) || str[i] == ' ')
		i++;
	if (str[i] == '-')
		nega = 1;
	if (str[i] == '-' || str[i] == '+')
		i++;
	while (str[i] >= '0' && str[i] <= '9')
		s = s * 10 + str[i++] - 48;
	if (nega == 1)
		s = -s;
	if (vlong != 1)
		return (vlong);
	return (s);
}
