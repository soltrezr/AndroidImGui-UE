#pragma once

#include <link.h>
#include <cstring>
#include <cstddef>
#include <sys/uio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace Memory
{
    struct ModuleData {
        std::string Name;
        uintptr_t BaseAddr;
        uintptr_t EndAddr;
        size_t Size;
    };

    inline int FindModuleCallback(dl_phdr_info* info, size_t /*size*/, void* userdata) {
        auto* d = reinterpret_cast<ModuleData*>(userdata);
        if (!d || !info) return 0;
        if (!info->dlpi_name || !*info->dlpi_name) return 0;

        const char* name = info->dlpi_name;
        const char* base = strrchr(name, '/');
        const char* modname = base ? base + 1 : name;

        if (d->Name != modname) return 0;

        uintptr_t s = UINTPTR_MAX, e = 0;
        for (int i = 0; i < info->dlpi_phnum; ++i) {
            const auto& p = info->dlpi_phdr[i];
            if (p.p_type != PT_LOAD) continue;
            uintptr_t a = info->dlpi_addr + p.p_vaddr, b = a + p.p_memsz;
            if (a < s) s = a;
            if (b > e) e = b;
        }

        if (e > s) d->BaseAddr = s; d->EndAddr = e; d->Size = e - s;
        return 1;
    }

    inline uintptr_t FindModuleBase(const std::string& ModuleName)
    {
        ModuleData Data{ModuleName, 0, 0, 0};
        dl_iterate_phdr(FindModuleCallback, &Data);
        return Data.BaseAddr;
    }

    inline uintptr_t FindModuleEnd(const std::string& ModuleName)
    {
        ModuleData Data{ModuleName, 0, 0, 0};
        dl_iterate_phdr(FindModuleCallback, &Data);
        return Data.EndAddr;
    }

    inline uintptr_t FindModuleSize(const std::string& ModuleName)
    {
        ModuleData Data{ModuleName, 0, 0, 0};
        dl_iterate_phdr(FindModuleCallback, &Data);
        return Data.Size;
    } 
} // namespace Memory
