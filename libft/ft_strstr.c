/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strstr.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/09 21:19:28 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/28 10:53:01 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strstr(const char *s1, const char *s2)
{
	size_t i;
	size_t j;

	j = 0;
	i = 0;
	while ((char *)&s1[i] == (char *)&s2[i])
	{
		if (!(char *)&s1[i + 1] == !(char *)&s2[i + 1])
			return ((char *)&s1[0]);
		i++;
	}
	i = 0;
	while (s1[i] != '\0')
	{
		j = 0;
		while (s1[i + j] && s1[i + j] == s2[j])
			j++;
		if (s2[j] == '\0')
			return ((char *)&s1[i]);
		i++;
	}
	return (NULL);
}
