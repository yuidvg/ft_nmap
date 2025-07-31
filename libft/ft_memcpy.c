/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memcpy.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/13 11:10:51 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/29 14:29:29 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memcpy(void *dst, const void *src, size_t n)
{
	unsigned char		*str_dst;
	const unsigned char	*str_src;

	str_src = src;
	str_dst = dst;
	while (n > 0)
	{
		*str_dst++ = *str_src++;
		n--;
	}
	return (dst);
}
