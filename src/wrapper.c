#include "internal.h"

static size_t count_split(char const *s, char c)
{
    size_t i;
    size_t count;

    i = 0;
    count = 0;
    while (s[i] != '\0')
    {
        if (s[i] != c)
        {
            count++;
            while (s[i] != c && s[i] != '\0')
                i++;
        }
        else
            i++;
    }
    return (count);
}

static size_t delim_len(char const *s, char c)
{
    size_t i;

    i = 0;
    while (s[i] != c && s[i] != '\0')
        i++;
    return (i);
}

static void insert_array(char *str, char const *s, size_t len, size_t *ctn)
{
    size_t i;

    i = 0;
    while (i < len)
    {
        str[i] = s[i];
        i++;
    }
    str[len] = '\0';
    *ctn += len;
}

char **ft_split_or_exit(char const *s, char c)
{
    if (s)
    {
        char **str_array = (char **)safe_malloc_or_exit(sizeof(char *) * (count_split(s, c) + 1));
        size_t i = 0;
        size_t j = 0;
        while (s[i] != '\0')
        {
            if (s[i] != c)
            {
                str_array[j] = (char *)safe_malloc_or_exit(delim_len(s + i, c) + 1);
                insert_array(str_array[j++], s + i, delim_len(s + i, c), &i);
            }
            else
                i++;
        }
        str_array[count_split(s, c)] = nullptr;
        return (str_array);
    }
    else
    {
        return (nullptr);
    }
}

char *strdup_or_exit(char const *s)
{
    void *const ptr = strdup(s);
    if (ptr != NULL)
    {
        register_memory_block_or_exit(ptr);
        return (ptr);
    }
    else
    {
        print_error_and_exit("strdup failed to allocate memory");
    }
    return (ptr);
}

