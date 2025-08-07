/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memccpy.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/13 13:43:59 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/27 14:27:51 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memccpy(void *dst, const void *src, int c, size_t n)
{
	char	*str;
	size_t	l;

	l = -1;
	str = dst;
	while (++l < n)
	{
		*(str + l) = *((unsigned char *)src + l);
		if (*((unsigned char *)src + l) == (unsigned char)c)
			return (dst + l + 1);
	}
	return (NULL);
}
