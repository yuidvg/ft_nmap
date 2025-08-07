/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_itoa.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/28 16:31:20 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/28 17:20:27 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static size_t	ft_itoalen(int n)
{
	size_t		len;

	len = 1;
	while (n /= 10)
		len++;
	return (len);
}

char			*ft_itoa(int n)
{
	char			*str;
	int				len;
	unsigned int	nn;

	len = ft_itoalen(n);
	nn = n;
	if (n < 0)
	{
		nn = -n;
		len++;
	}
	if (!(str = ft_strnew(len)))
		return (NULL);
	str[--len] = nn % 10 + '0';
	while (nn /= 10)
		str[--len] = nn % 10 + '0';
	if (n < 0)
		*(str + 0) = '-';
	return (str);
}
