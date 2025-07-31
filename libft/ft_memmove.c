/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memmove.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/25 15:55:40 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/29 17:32:00 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static void	*ft_memrcpy(void *dst, const void *src, size_t n)
{
	unsigned char		*str_dst;
	const unsigned char	*str_src;

	str_src = src;
	str_dst = dst;
	while (n > 0)
	{
		n--;
		str_dst[n] = str_src[n];
	}
	return (dst);
}

void		*ft_memmove(void *dst, const void *src, size_t n)
{
	unsigned char	*str;

	str = (unsigned char *)src;
	if (dst > src)
		ft_memrcpy(dst, str, n);
	else
		ft_memcpy(dst, str, n);
	return (dst);
}
