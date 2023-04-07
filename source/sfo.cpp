#include <cstring>
#include "sfo.h"

static constexpr uint32_t SFO_MAGIC = 0x46535000;

namespace SFO {
    const char* GetString(const char* buffer, size_t size, const char *name)
    {
        if (size < sizeof(SfoHeader))
            return nullptr;

        const SfoHeader* header = reinterpret_cast<const SfoHeader*>(buffer);
        const SfoEntry* entries =
                reinterpret_cast<const SfoEntry*>(buffer + sizeof(SfoHeader));

        if (header->magic != SFO_MAGIC)
            return nullptr;

        if (size < sizeof(SfoHeader) + header->count * sizeof(SfoEntry))
            return nullptr;

        for (uint32_t i = 0; i < header->count; i++) {
            const char* key = reinterpret_cast<const char*>(buffer + header->keyofs + entries[i].nameofs);
            if (strcmp(key, name) == 0)
                return reinterpret_cast<const char*>(buffer + header->valofs + entries[i].dataofs);
        }
        
        return {};
    }

    std::map<std::string, std::string> GetParams(const char* buffer, size_t size)
    {
        std::map<std::string, std::string> out;

        if (size < sizeof(SfoHeader))
            return out;

        const SfoHeader* header = reinterpret_cast<const SfoHeader*>(buffer);
        const SfoEntry* entries =
                reinterpret_cast<const SfoEntry*>(buffer + sizeof(SfoHeader));

        if (header->magic != SFO_MAGIC)
            return out;

        if (size < sizeof(SfoHeader) + header->count * sizeof(SfoEntry))
            return out;

        for (uint32_t i = 0; i < header->count; i++) {
            const char* key = reinterpret_cast<const char*>(buffer + header->keyofs + entries[i].nameofs);
            if (entries[i].type == 2)
            {
                const char* value = reinterpret_cast<const char*>(buffer + header->valofs + entries[i].dataofs);
                out.insert(std::make_pair(key, value));
            }
            else
            {
                uint32_t *value = (uint32_t *)(buffer + header->valofs + entries[i].dataofs);
                out.insert(std::make_pair(key, std::to_string(*value)));
            }
        }

        return out;
    }
}