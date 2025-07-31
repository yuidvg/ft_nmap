/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_str_is_alnum.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sseneca <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2018/12/02 13:25:54 by sseneca           #+#    #+#             */
/*   Updated: 2018/12/02 13:28:39 by sseneca          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

int	ft_str_is_alnum(char *str)
{
	if (ft_str_is_numeric(str) || ft_str_is_alpha(str))
		return (1);
	else
		return (0);
}
