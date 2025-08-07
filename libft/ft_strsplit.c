/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   fft_strsplit.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/30 12:01:56 by sseneca           #+#    #+#             */
/*   Updated: 2019/07/01 01:53:15 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static size_t	ft_word(char const *s, char c)
{
	size_t	word;
	size_t	in;

	word = 0;
	in = 0;
	while (s[in])
	{
		while (s[in] == c && s[in])
			in++;
		if (!(s[in]))
			return (word);
		while (s[in] != c && s[in])
		{
			in++;
		}
		word++;
	}
	return (word);
}

static char		**ft_split(size_t i, char const *s, char c, char **tab)
{
	size_t	len;
	size_t	j;

	j = 0;
	len = 0;
	while (s[i])
	{
		if (s[i] == c)
			i++;
		else
		{
			len = 0;
			while (s[i + len] && (s[i + len++] != c))
			{
				if (len > 1)
					free(tab[j]);
				tab[j] = ft_strsub(s, i, len);
			}
			j++;
			i += len;
		}
	}
	return (tab);
}

char			**ft_strsplit(char const *s, char c)
{
	char	**tab;
	size_t	i;
	size_t	j;
	size_t	wordn;

	if (!s || !c)
		return (NULL);
	i = 0;
	j = 0;
	wordn = ft_word(s, c);
	if (!(tab = (char **)malloc(sizeof(char *) * (wordn + 1))))
		return (NULL);
	tab[wordn] = NULL;
	return (ft_split(i, s, c, tab));
}
