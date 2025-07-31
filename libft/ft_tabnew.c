/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_tabnew.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2019/07/01 01:52:41 by sseneca           #+#    #+#             */
/*   Updated: 2019/07/01 01:52:43 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

int		**ft_tabnew(int column, int nb)
{
	int **tab;

	if (!(tab = (int **)malloc(sizeof(int *) * column)))
		return (NULL);
	while (column--)
		if (!(tab[column] = (int *)malloc(sizeof(int) * nb)))
			return (NULL);
	return (tab);
}
