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
}