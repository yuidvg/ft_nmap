/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_strtrim.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/25 17:21:01 by sseneca           #+#    #+#             */
/*   Updated: 2018/11/30 16:46:32 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

char		*ft_strtrim(char const *s)
{
	int		i;
	int		start;
	int		len;
	char	*str;

	if (!s)
		return (NULL);
	i = 0;
	start = -1;
	len = ft_strlen(s);
	while (s[len - 1] == ' ' || s[len - 1] == '\n' || s[len - 1] == '\t')
		len--;
	if (len <= 0)
		return (ft_memalloc(1));
	while (s[++start] == ' ' || s[start] == '\n' || s[start] == '\t')
		len--;
	if (!(str = (char *)safe_malloc_or_exit(sizeof(char) * (len + 1))))
		return (NULL);
	while (start-- > 0)
		s++;
	while (i < len)
		str[i++] = (char)*s++;
	str[i] = '\0';
	return (str);
}
