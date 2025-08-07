/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_lstnew.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/11/30 18:10:05 by sseneca           #+#    #+#             */
/*   Updated: 2018/12/01 14:14:16 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

t_list	*ft_lstnew(void const *content, size_t content_size)
{
	struct s_list	*m;

	m = (t_list *)malloc(sizeof(t_list));
	if (m == NULL)
		return (NULL);
	if (!content)
	{
		m->content = NULL;
		m->content_size = 0;
	}
	else
	{
		m->content = malloc(content_size);
		if (!m->content)
			return (NULL);
		ft_memcpy(m->content, content, content_size);
		m->content_size = content_size;
	}
	m->next = NULL;
	return (m);
}
