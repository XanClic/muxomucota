#include <lock.h>
#include <stdlib.h>
#include <cdi/lists.h>

cdi_list_t cdi_list_create(void)
{
    return calloc(1, sizeof(struct cdi_list));
}

void cdi_list_destroy(cdi_list_t list)
{
    lock(&list->lock);

    while (list->first)
    {
        struct cdi_list_element *next = list->first->next;
        free(list->first);
        list->first = next;
    }

    free(list);
}

cdi_list_t cdi_list_push(cdi_list_t list, void *value)
{
    struct cdi_list_element *new = calloc(1, sizeof(*new));
    if (new == NULL)
        return NULL;

    new->data = value;

    lock(&list->lock);
    new->next = list->first;
    list->first = new;
    list->cached_index++;
    list->size++;
    unlock(&list->lock);

    return list;
}

void *cdi_list_pop(cdi_list_t list)
{
    lock(&list->lock);
    struct cdi_list_element *first = list->first;
    if (first != NULL)
        list->first = first->next;
    if (list->size)
        list->size--;
    if (list->cached_index)
        list->cached_index--;
    unlock(&list->lock);

    if (first == NULL)
        return NULL;

    void *data = first->data;
    free(first);

    return data;
}

size_t cdi_list_empty(cdi_list_t list)
{
    return !list->size;
}

void *cdi_list_get(cdi_list_t list, size_t index)
{
    struct cdi_list_element *ele;

    lock(&list->lock);

    if (list->cached_index && (index >= list->cached_index))
    {
        ele = list->cached;
        for (size_t i = list->cached_index; (i < index) && (ele != NULL); i++)
            ele = ele->next;
    }
    else
    {
        ele = list->first;
        for (size_t i = 0; (i < index) && (ele != NULL); i++)
            ele = ele->next;
    }

    if (ele == NULL)
    {
        unlock(&list->lock);
        return NULL;
    }

    list->cached_index = index;
    list->cached = ele;

    unlock(&list->lock);

    return ele->data;
}

cdi_list_t cdi_list_insert(cdi_list_t list, size_t index, void *value)
{
    struct cdi_list_element *new, *ele;

    if (!index)
        return cdi_list_push(list, value);

    new = calloc(1, sizeof(*new));
    if (new == NULL)
        return NULL;

    new->data = value;

    lock(&list->lock);

    if (list->cached_index && (index > list->cached_index))
    {
        ele = list->cached;
        for (size_t i = list->cached_index; (i < index - 1) && (ele != NULL); i++)
            ele = ele->next;
    }
    else
    {
        ele = list->first;
        for (size_t i = 0; (i < index - 1) && (ele != NULL); i++)
            ele = ele->next;
    }

    if (ele == NULL)
    {
        unlock(&list->lock);
        free(new);
        return NULL;
    }

    if (index > 1)
    {
        list->cached_index = index - 1;
        list->cached = ele;
    }

    new->next = ele->next;
    ele->next = new;

    if ((index == 1) && list->cached_index)
        list->cached_index++;

    list->size++;

    unlock(&list->lock);

    return list;
}

void *cdi_list_remove(cdi_list_t list, size_t index)
{
    struct cdi_list_element *ele, *old;

    if (!index)
        return cdi_list_pop(list);

    lock(&list->lock);

    if (list->cached_index && (index > list->cached_index))
    {
        ele = list->cached;
        for (size_t i = list->cached_index; (i < index - 1) && (ele != NULL); i++)
            ele = ele->next;
    }
    else
    {
        ele = list->first;
        for (size_t i = 0; (i < index - 1) && (ele != NULL); i++)
            ele = ele->next;
    }

    if ((ele == NULL) || (ele->next == NULL))
    {
        unlock(&list->lock);
        return NULL;
    }

    if (index == 1)
        list->cached_index--;
    else
    {
        list->cached_index = index - 1;
        list->cached = ele;
    }

    old = ele->next;
    ele->next = old->next;

    list->size--;

    unlock(&list->lock);

    void *data = old->data;
    free(old);

    return data;
}

size_t cdi_list_size(cdi_list_t list)
{
    return list->size;
}
