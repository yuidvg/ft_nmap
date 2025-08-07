/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_lstmap.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/12/01 20:16:09 by sseneca           #+#    #+#             */
/*   Updated: 2018/12/01 21:41:10 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

t_list	*ft_lstmap(t_list *lst, t_list *(*f)(t_list *elem))
{
	t_list		*lst_f;
	t_list		*tmp;

	if (!lst)
		return (NULL);
	lst_f = f(lst);
	tmp = lst_f;
	while (lst->next)
	{
		lst = lst->next;
		if (!(lst_f->next = f(lst)))
			return (NULL);
		lst_f = lst_f->next;
	}
	return (tmp);
}
