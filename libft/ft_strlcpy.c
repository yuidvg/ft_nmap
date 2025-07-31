/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strlcpy.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/12/02 11:37:59 by sseneca           #+#    #+#             */
/*   Updated: 2018/12/02 12:27:14 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

size_t	ft_strlcpy(char *dst, const char *src, size_t size)
{
	size_t	len;
	size_t	i;

	len = ft_strlen(src);
	i = 0;
	if (size < len)
		len = size - 1;
	if (src && dst && size)
	{
		while (i < len && src[i] != '\0')
		{
			dst[i] = src[i];
			i++;
		}
		if (i < size)
			dst[i++] = '\0';
	}
	return (ft_strlen(src));
}
