/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strjoin.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/25 15:20:24 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/30 14:41:43 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char	*ft_strjoin(char const *s1, char const *s2)
{
	char	*str;
	int		i;

	if (!s1 || !s2)
		return (0);
	i = -1;
	str = (char *)safe_malloc_or_exit(sizeof(char) * (ft_strlen(s1) + ft_strlen(s2) + 1));
	if (str == NULL)
		return (NULL);
	while (*s1)
		str[++i] = *s1++;
	while (*s2)
		str[++i] = *s2++;
	str[i + 1] = '\0';
	return (str);
}
