#ifndef EZ_COMMON_H
#define EZ_COMMON_H

#include <string>
#include <cstdarg>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>

#define HTTP_SUCCESS(x) (x >= 200 && x < 300)
// Set from the saved setting before anything starts: skips the ImGui/SDL front
// end and serves only the web UI.
extern bool headless_mode;
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

enum DownloadState { STATE_PENDING, STATE_DOWNLOADING, STATE_RESUMED, STATE_FAILED, STATE_SUCCESS };

static const char* state_strings[] = {"Pending", "Downloading", "Resumed", "Failed", "Success"};

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

// Helper for code that previously wrote into fixed DirEntry char arrays.
// Keeps the formatting expressions intact while storing into std::string.
static inline void DirEntrySetString(std::string &out, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    out.assign(buffer);
}

struct DirEntry
{
    std::string directory;
    std::string name;
    char display_size[48];
    char display_date[48];
    std::string path;
    uint64_t file_size;
    bool isDir;
    bool isLink;
    DateTime modified;
    bool selectable;
    // "rwxr-xr-x" (the leading type character is not stored).
    char rights[11];

    friend bool operator<(DirEntry const &a, DirEntry const &b)
    {
        return a.name < b.name;
    }

    static bool CompareEntries(const DirEntry &a, const DirEntry &b)
    {
        if (a.name == ".." && b.name != "..")
            return true;
        if (b.name == ".." && a.name != "..")
            return false;

        if (a.isDir && !b.isDir)
            return true;
        if (!a.isDir && b.isDir)
            return false;

        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    }

    static void Sort(std::vector<DirEntry> &list)
    {
        std::sort(list.begin(), list.end(), CompareEntries);
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

struct DownloadProgress
{
    std::string path;
    std::string state;
    uint64_t bytes_transfered;
    uint64_t file_size;
    time_t timestamp;
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
