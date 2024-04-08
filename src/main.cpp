/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "./Game/Env.h"
#include "./Game/O2Game.h"
#include "./MsgBox.h"

#include "../lib/src/Graphics/Backends/Vulkan/Misc/VkImage.h"

#if defined(_WIN32) && defined(_MSC_VER)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int           AmdPowerXpressRequestHighPerformance = 1;

const char *__asan_default_options()
{
    return "verbosity=1";
}
}
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

bool IsVM()
{
    int cpuInfo[4] = {};

    //
    // Upon execution, code should check bit 31 of register ECX
    // (the “hypervisor present bit”). If this bit is set, a hypervisor is present.
    // In a non-virtualized environment, the bit will be clear.
    //
    __cpuid(cpuInfo, 1);

    if (!(cpuInfo[2] & (1 << 31)))
        return false;

    //
    // A hypervisor is running on the machine. Query the vendor id.
    //
    const auto queryVendorIdMagic = 0x40000000;
    __cpuid(cpuInfo, queryVendorIdMagic);

    const int vendorIdLength = 13;
    using VendorIdStr = char[vendorIdLength];

    VendorIdStr hyperVendorId = {};

    memcpy(hyperVendorId + 0, &cpuInfo[1], 4);
    memcpy(hyperVendorId + 4, &cpuInfo[2], 4);
    memcpy(hyperVendorId + 8, &cpuInfo[3], 4);
    hyperVendorId[12] = '\0';

    static const VendorIdStr vendors[]{
        "KVMKVMKVM\0\0\0", // KVM
        "Microsoft Hv",    // Microsoft Hyper-V or Windows Virtual PC
        "VMwareVMware",    // VMware
        "XenVMMXenVMM",    // Xen
        "lrpepyh vr",      // Parallels
        "VBoxVBoxVBox"     // VirtualBox
    };

    for (const auto &vendor : vendors) {
        if (!memcmp(vendor, hyperVendorId, vendorIdLength)) {
#ifdef _WIN32
            if (!memcmp(vendor, "Microsoft Hv", vendorIdLength)) {
                HKEY  hKey = 0;
                DWORD dwType = REG_SZ;
                char  buf[255] = { 0 };
                DWORD dwBufSize = sizeof(buf);

                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\HardwareConfig\\Current\\", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
                    LSTATUS result = RegGetValueA(hKey, NULL, "BIOSVendor", RRF_RT_REG_SZ, NULL, buf, &dwBufSize);

                    if (result == ERROR_SUCCESS) {
                        if (strcmp(buf, "Microsoft Corporation") == 0) {
                            return true;
                        }
                    }
                }

                return false;
            }
#endif

            return true;
        }
    }

    return false;
}

int main(int argv, char **argc)
{
    if (IsVM()) {
        MsgBox::Show("VM Detected", "Please run this game on a real machine. :)", MsgBox::Type::Ok, MsgBox::Flags::Error);
        return 1;
    }

    O2Game game;
    game.Run(argv, argc);

    return 0;
}