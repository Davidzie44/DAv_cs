#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdint>

int main() {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"cs2.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!pid) { printf("cs2 not found\n"); return 1; }
    printf("CS2 PID: %lu\n", pid);

    HANDLE hProc = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (!hProc) { printf("Cannot open process\n"); return 1; }

    // Find client.dll base
    HANDLE msnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    MODULEENTRY32W me = { sizeof(me) };
    uintptr_t clientBase = 0, engine2Base = 0;
    if (Module32FirstW(msnap, &me)) {
        do {
            if (_wcsicmp(me.szModule, L"client.dll") == 0) clientBase = (uintptr_t)me.modBaseAddr;
            if (_wcsicmp(me.szModule, L"engine2.dll") == 0) engine2Base = (uintptr_t)me.modBaseAddr;
        } while (Module32NextW(msnap, &me));
    }
    CloseHandle(msnap);

    printf("client.dll base: 0x%llX\n", clientBase);
    printf("engine2.dll base: 0x%llX\n", engine2Base);

    // Read CS2 build number
    uint32_t buildNum = 0;
    SIZE_T br;
    ReadProcessMemory(hProc, (LPCVOID)(engine2Base + 0x60CC74), &buildNum, 4, &br);
    printf("CS2 Build Number: %u\n", buildNum);

    // Read local player controller
    uintptr_t localCtrl = 0;
    ReadProcessMemory(hProc, (LPCVOID)(clientBase + 0x2320720), &localCtrl, 8, &br);
    printf("dwLocalPlayerController: 0x%llX\n", localCtrl);

    // Read entity list
    uintptr_t entityList = 0;
    ReadProcessMemory(hProc, (LPCVOID)(clientBase + 0x24E76A0), &entityList, 8, &br);
    printf("dwEntityList: 0x%llX\n", entityList);

    // Read local pawn from controller
    if (localCtrl) {
        uint32_t pawnHandle = 0;
        ReadProcessMemory(hProc, (LPCVOID)(localCtrl + 0x90C), &pawnHandle, 4, &br);
        printf("m_hPlayerPawn: 0x%08X\n", pawnHandle);

        // Try to resolve pawn
        if (pawnHandle && pawnHandle != 0xFFFFFFFF) {
            uint32_t idx = pawnHandle & 0x7FFF;
            uint32_t pgIdx = idx >> 9;
            uint32_t pgOff = idx & 0x1FF;
            
            uintptr_t pgAddr = entityList + 0x10 + (uintptr_t)pgIdx * 0x10;
            uintptr_t pgPtr = 0;
            ReadProcessMemory(hProc, (LPCVOID)pgAddr, &pgPtr, 8, &br);
            printf("  Page[%u] at 0x%llX = 0x%llX\n", pgIdx, pgAddr, pgPtr);
            
            if (pgPtr) {
                uintptr_t pawnAddr = 0;
                ReadProcessMemory(hProc, (LPCVOID)(pgPtr + (uintptr_t)pgOff * 0x70), &pawnAddr, 8, &br);
                printf("  Pawn entity: 0x%llX\n", pawnAddr);
                
                if (pawnAddr) {
                    uint32_t health = 0;
                    ReadProcessMemory(hProc, (LPCVOID)(pawnAddr + 0x34C), &health, 4, &br);
                    printf("  Health: %u\n", health);
                }
            }
            
            // Also try *8 stride
            uintptr_t pgAddr8 = entityList + 0x10 + (uintptr_t)pgIdx * 8;
            uintptr_t pgPtr8 = 0;
            ReadProcessMemory(hProc, (LPCVOID)pgAddr8, &pgPtr8, 8, &br);
            printf("  Page8[%u] at 0x%llX = 0x%llX\n", pgIdx, pgAddr8, pgPtr8);
            
            if (pgPtr8) {
                uintptr_t pawnAddr8 = 0;
                ReadProcessMemory(hProc, (LPCVOID)(pgPtr8 + (uintptr_t)pgOff * 0x70), &pawnAddr8, 8, &br);
                printf("  Pawn8 entity: 0x%llX\n", pawnAddr8);
            }
        }
    }

    CloseHandle(hProc);
    return 0;
}
