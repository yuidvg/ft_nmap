/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strlcat.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/24 13:19:57 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/29 19:31:16 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

size_t	ft_strlcat(char *dest, const char *src, size_t size)
{
	size_t	i;
	size_t	destl;
	size_t	srcl;
	size_t	len;

	i = 0;
	destl = ft_strlen(dest);
	srcl = ft_strlen(src);
	len = destl + srcl;
	if (size <= destl)
		len = srcl + size;
	while (src[i] != '\0' && destl + 1 < size)
	{
		dest[destl] = src[i];
		destl++;
		i++;
	}
	dest[destl] = '\0';
	return (len);
}
