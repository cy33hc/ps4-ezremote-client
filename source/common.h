#ifndef EZ_COMMON_H
#define EZ_COMMON_H

#include <string>
#include <vector>
#include <string.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>

#define HTTP_SUCCESS(x) (x >= 200 && x < 300)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t dayOfWeek;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint32_t microsecond;
} DateTime;

struct DirEntry
{
    char directory[512];
    char name[256];
    char display_size[48];
    char display_date[48];
    char path[768];
    uint64_t file_size;
    bool isDir;
    bool isLink;
    DateTime modified;
    bool selectable;

    friend bool operator<(DirEntry const &a, DirEntry const &b)
    {
        return strcmp(a.name, b.name) < 0;
    }

    static int DirEntryComparator(const void *v1, const void *v2)
    {
        const DirEntry *p1 = (DirEntry *)v1;
        const DirEntry *p2 = (DirEntry *)v2;
        if (strcasecmp(p1->name, "..") == 0)
            return -1;
        if (strcasecmp(p2->name, "..") == 0)
            return 1;

        if (p1->isDir && !p2->isDir)
        {
            return -1;
        }
        else if (!p1->isDir && p2->isDir)
        {
            return 1;
        }

        return strcasecmp(p1->name, p2->name);
    }

    static void Sort(std::vector<DirEntry> &list)
    {
        qsort(&list[0], list.size(), sizeof(DirEntry), DirEntryComparator);
    }

    static void SetDisplaySize(DirEntry *entry)
    {
        if (entry->file_size < 1024)
        {
            sprintf(entry->display_size, "%ldB", entry->file_size);
        }
        else if (entry->file_size < 1024 * 1024)
        {
            sprintf(entry->display_size, "%.2fKB", entry->file_size * 1.0f / 1024);
        }
        else if (entry->file_size < 1024 * 1024 * 1024)
        {
            sprintf(entry->display_size, "%.2fMB", entry->file_size * 1.0f / (1024 * 1024));
        }
        else
        {
            sprintf(entry->display_size, "%.2fGB", entry->file_size * 1.0f / (1024 * 1024 * 1024));
        }
    }
};

static lxb_dom_node_t *NextChildElement(lxb_dom_element_t *element)
{
    lxb_dom_node_t *node = element->node.first_child;
    while (node != nullptr && node->type != LXB_DOM_NODE_TYPE_ELEMENT)
    {
        node = node->next;
    }
    return node;
}

static lxb_dom_node_t *NextElement(lxb_dom_node_t *node)
{
    lxb_dom_node_t *next = node->next;
    while (next != nullptr && next->type != LXB_DOM_NODE_TYPE_ELEMENT)
    {
        next = next->next;
    }
    return next;
}

static lxb_dom_node_t *NextChildTextNode(lxb_dom_element_t *element)
{
    lxb_dom_node_t *node = element->node.first_child;
    while (node != nullptr && node->type != LXB_DOM_NODE_TYPE_TEXT)
    {
        node = node->next;
    }
    return node;
}

static lxb_dom_node_t *NextTextNode(lxb_dom_node_t *node)
{
    lxb_dom_node_t *next = node->next;
    while (next != nullptr && next->type != LXB_DOM_NODE_TYPE_TEXT)
    {
        next = next->next;
    }
    return next;
}

#endif