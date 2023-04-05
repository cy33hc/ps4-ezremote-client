#ifndef LAUNCHER_SFO_H
#define LAUNCHER_SFO_H

#pragma once

#include <cstdint>
#include <string>

struct SfoHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t keyofs;
    uint32_t valofs;
    uint32_t count;
} __attribute__((packed));

struct SfoEntry
{
    uint16_t nameofs;
    uint8_t alignment;
    uint8_t type;
    uint32_t valsize;
    uint32_t totalsize;
    uint32_t dataofs;
} __attribute__((packed));

namespace SFO {
    const char* GetString(const char* buffer, size_t size, const char *name);
}

#endif