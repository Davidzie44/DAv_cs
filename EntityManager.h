#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "Offsets.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <fstream>

extern std::ofstream g_log;
void Log(const char* msg);
void LogFmt(const char* fmt, ...);

struct PlayerData {
    int index;
    int health;
    int team;
    int armor;
    Vector3 position;
    Vector3 headPosition;
    std::string name;
    bool isAlive;
    bool isDormant;
    float distance;
    uintptr_t pawnAddr;
    uintptr_t controllerAddr;
    int entityIndex;
    float flashAlpha;
    bool isDefusing;
};

class EntityManager {
private:
    ProcessMemory& process;
    std::vector<PlayerData> players;
    PlayerData localPlayer;
    uintptr_t entityListBase;

    uintptr_t ReadEntityFromList(int entityIndex) {
        if (entityListBase == 0 || entityIndex < 0 || entityIndex > 2047) return 0;
        try {
            uint32_t pageIndex = entityIndex >> 9;
            uint32_t pageOffset = entityIndex & 0x1FF;
            uintptr_t pageAddr = entityListBase + 0x10 + (uintptr_t)pageIndex * 8;
            uintptr_t pagePtr = process.ReadMemory<uintptr_t>(pageAddr);
            if (pagePtr == 0) return 0;
            return process.ReadMemory<uintptr_t>(pagePtr + (uintptr_t)pageOffset * 0x70);
        } catch (...) { return 0; }
    }

    uintptr_t ResolveController(int index) {
        return ReadEntityFromList(index + 1);
    }

    uintptr_t ResolvePawnFromHandle(uint32_t handle) {
        if (handle == 0 || handle == 0xFFFFFFFF || entityListBase == 0) return 0;
        uint32_t index = handle & 0x7FFF;
        return ReadEntityFromList(index);
    }

    std::string GetPlayerName(uintptr_t controllerAddr) {
        if (controllerAddr == 0) return "Unknown";
        try {
            char nameBuffer[64];
            for (int i = 0; i < 63; i++) {
                nameBuffer[i] = process.ReadMemory<char>(controllerAddr + Offsets::schema::m_sSanitizedPlayerName + i);
                if (nameBuffer[i] == 0) break;
            }
            nameBuffer[63] = 0;
            return std::string(nameBuffer);
        } catch (...) {
            return "Unknown";
        }
    }

    Vector3 ReadPositionFromPawn(uint64_t pawnAddr) {
        if (pawnAddr == 0) return Vector3();
        try {
            uintptr_t sceneNode = process.ReadMemory<uintptr_t>(pawnAddr + Offsets::schema::m_pGameSceneNode);
            if (sceneNode == 0) return Vector3();
            float x = process.ReadMemory<float>(sceneNode + Offsets::schema::m_vecAbsOrigin);
            float y = process.ReadMemory<float>(sceneNode + Offsets::schema::m_vecAbsOrigin + 0x4);
            float z = process.ReadMemory<float>(sceneNode + Offsets::schema::m_vecAbsOrigin + 0x8);
            return Vector3(x, y, z);
        } catch (...) { return Vector3(); }
    }

    bool IsDormant(uint64_t pawnAddr) {
        if (pawnAddr == 0) return true;
        try {
            uintptr_t sceneNode = process.ReadMemory<uintptr_t>(pawnAddr + Offsets::schema::m_pGameSceneNode);
            if (sceneNode == 0) return true;
            return process.ReadMemory<bool>(sceneNode + Offsets::schema::m_bDormant);
        } catch (...) { return true; }
    }

public:
    EntityManager(ProcessMemory& pm) : process(pm), entityListBase(0) {
        localPlayer = PlayerData{};
    }

    void Update() {
        players.clear();

        uintptr_t clientBase = process.GetClientDllBase();
        uintptr_t engineBase = process.GetEngine2DllBase();

        static int logCounter = 0;
        logCounter++;

        // Read build number for offset verification
        static bool loggedOffsets = false;
        if (!loggedOffsets) {
            try {
                uint32_t buildNum = process.ReadMemory<uint32_t>(engineBase + Offsets::engine2_dll::dwBuildNumber);
                LogFmt("EntityManager: CS2 Build=%u", buildNum);

                uintptr_t elRaw = process.ReadMemory<uintptr_t>(clientBase + Offsets::client_dll::dwEntityList);
                LogFmt("EntityManager: dwEntityList ptr=0x%llX", (unsigned long long)elRaw);

                uintptr_t ctrlRaw = process.ReadMemory<uintptr_t>(clientBase + Offsets::client_dll::dwLocalPlayerController);
                LogFmt("EntityManager: dwLocalPlayerController ptr=0x%llX", (unsigned long long)ctrlRaw);

                uintptr_t pawnRaw = process.ReadMemory<uintptr_t>(clientBase + Offsets::client_dll::dwLocalPlayerPawn);
                LogFmt("EntityManager: dwLocalPlayerPawn ptr=0x%llX", (unsigned long long)pawnRaw);

                uintptr_t viewMatRaw = process.ReadMemory<uintptr_t>(clientBase + Offsets::client_dll::dwViewMatrix);
                LogFmt("EntityManager: dwViewMatrix ptr=0x%llX", (unsigned long long)viewMatRaw);

                // Dump first few entity list entries
                if (elRaw != 0) {
                    for (int i = 0; i < 5; i++) {
                        uintptr_t slotAddr = elRaw + 0x10 + (uintptr_t)i * 0x10;
                        uintptr_t slotVal = 0;
                        try { slotVal = process.ReadMemory<uintptr_t>(slotAddr); } catch (...) {}
                        LogFmt("  EL slot[%d] at 0x%llX = 0x%llX", i, (unsigned long long)slotAddr, (unsigned long long)slotVal);
                    }
                }

                loggedOffsets = true;
            } catch (...) {
                Log("EntityManager: Failed to read offset verification data");
                loggedOffsets = true;
            }
        }

        entityListBase = process.ReadMemory<uintptr_t>(clientBase + Offsets::client_dll::dwEntityList);
        if (entityListBase == 0) {
            Log("EntityManager: entityListBase is 0!");
            return;
        }

        // Read highest entity index for dynamic scan range
        int maxEntities = 128;
        try {
            maxEntities = process.ReadMemory<int>(entityListBase + Offsets::client_dll::dwGameEntitySystem_highestEntityIndex);
            if (maxEntities < 64) maxEntities = 64;
            if (maxEntities > 2048) maxEntities = 2048;
        } catch (...) {}

        if (logCounter <= 5 || logCounter % 300 == 0) {
            LogFmt("EntityManager: maxEntities=%d elBase=0x%llX", maxEntities, (unsigned long long)entityListBase);
        }

        uintptr_t localControllerAddr = process.ReadMemory<uintptr_t>(
            clientBase + Offsets::client_dll::dwLocalPlayerController
        );
        if (localControllerAddr == 0) {
            Log("EntityManager: localControllerAddr is 0!");
            return;
        }

        uint32_t localPawnHandle = process.ReadMemory<uint32_t>(
            localControllerAddr + Offsets::schema::m_hPlayerPawn
        );
        if (localPawnHandle == 0 || localPawnHandle == 0xFFFFFFFF) {
            LogFmt("EntityManager: localPawnHandle invalid: 0x%08X", localPawnHandle);
            return;
        }

        uintptr_t localPawnAddr = ResolvePawnFromHandle(localPawnHandle);
        if (localPawnAddr == 0) {
            LogFmt("EntityManager: localPawnAddr is 0! handle=0x%08X", localPawnHandle);
            return;
        }

        int health = process.ReadMemory<int>(localPawnAddr + Offsets::schema::m_iHealth);
        int team = process.ReadMemory<int>(localPawnAddr + Offsets::schema::m_iTeamNum);
        localPlayer.index = -1;
        localPlayer.health = health;
        localPlayer.team = team;
        localPlayer.armor = process.ReadMemory<int>(localPawnAddr + Offsets::schema::m_ArmorValue);
        localPlayer.position = ReadPositionFromPawn(localPawnAddr);
        localPlayer.headPosition = Vector3(localPlayer.position.x, localPlayer.position.y, localPlayer.position.z + 64.0f);
        localPlayer.isAlive = health > 0;
        localPlayer.isDormant = false;
        localPlayer.pawnAddr = localPawnAddr;
        localPlayer.controllerAddr = localControllerAddr;
        localPlayer.entityIndex = -1;
        localPlayer.flashAlpha = 0;
        localPlayer.distance = 0;
        localPlayer.name = "Local Player";
        if (localControllerAddr != 0) {
            localPlayer.name = GetPlayerName(localControllerAddr);
        }

        if (logCounter <= 5 || logCounter % 300 == 0) {
            LogFmt("  EntityManager: localPawn=0x%llX ctrl=0x%llX hp=%d team=%d pos=(%.1f,%.1f,%.1f) elBase=0x%llX",
                (unsigned long long)localPawnAddr, (unsigned long long)localControllerAddr,
                health, team, localPlayer.position.x, localPlayer.position.y, localPlayer.position.z,
                (unsigned long long)entityListBase);
        }

        int foundCount = 0;
        int validCount = 0;
        int failHandle = 0, failPawn = 0, failLocal = 0, failHealth = 0, failTeam = 0, failPos = 0, failRead = 0;
        for (int i = 0; i < maxEntities; i++) {
            uintptr_t controller = ResolveController(i);
            if (controller == 0) continue;
            foundCount++;

            uint32_t pawnHandle = 0;
            try {
                pawnHandle = process.ReadMemory<uint32_t>(controller + Offsets::schema::m_hPlayerPawn);
            } catch (...) { failRead++; continue; }
            if (pawnHandle == 0 || pawnHandle == 0xFFFFFFFF) { failHandle++; continue; }

            uintptr_t pawn = ResolvePawnFromHandle(pawnHandle);
            if (pawn == 0) {
                failPawn++;
                continue;
            }
            if (pawn == localPawnAddr) { failLocal++; continue; }

            int hp = 0;
            int tm = 0;
            try {
                hp = process.ReadMemory<int>(pawn + Offsets::schema::m_iHealth);
                tm = process.ReadMemory<int>(pawn + Offsets::schema::m_iTeamNum);
            } catch (...) { failRead++; continue; }

            if (hp <= 0 || hp > 300) { failHealth++; continue; }
            if (tm != 2 && tm != 3) { failTeam++; continue; }

            bool dormant = IsDormant(pawn);

            Vector3 position = ReadPositionFromPawn(pawn);
            if (position.Length() < 1.0f) { failPos++; continue; }

            float distance = 0;
            if (localPlayer.position.Length() > 1.0f) {
                distance = localPlayer.position.Distance(position);
            }

            PlayerData data{};
            data.index = i;
            data.health = hp;
            data.team = tm;
            data.armor = process.ReadMemory<int>(pawn + Offsets::schema::m_ArmorValue);
            data.position = position;
            data.headPosition = Vector3(position.x, position.y, position.z + 64.0f);
            data.name = GetPlayerName(controller);
            data.isAlive = hp > 0;
            data.isDormant = dormant;
            data.distance = distance;
            data.pawnAddr = pawn;
            data.controllerAddr = controller;
            data.entityIndex = i;
            data.flashAlpha = 0;
            data.isDefusing = false;

            players.push_back(data);
            validCount++;

            if (logCounter <= 3) {
                LogFmt("    Player[%d]: hp=%d team=%d pos=(%.0f,%.0f,%.0f) dist=%.0f dormant=%d name=%s",
                    i, hp, tm, position.x, position.y, position.z, distance, dormant, data.name.c_str());
            }
        }

        if (logCounter <= 5 || logCounter % 300 == 0) {
            LogFmt("  EntityManager: found=%d valid=%d handleFail=%d pawnFail=%d localFail=%d healthFail=%d teamFail=%d readFail=%d",
                foundCount, validCount, failHandle, failPawn, failLocal, failHealth, failTeam, failRead);
        }

        if (logCounter <= 5 || logCounter % 300 == 0) {
            LogFmt("  EntityManager: found=%d valid=%d totalPlayers=%d", foundCount, validCount, (int)players.size());
        }
    }

    Entity GetLocalPawn() {
        uintptr_t localControllerAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerController
        );
        uint32_t localPawnHandle = 0;
        if (localControllerAddr != 0) {
            localPawnHandle = process.ReadMemory<uint32_t>(
                localControllerAddr + Offsets::schema::m_hPlayerPawn
            );
        }
        return Entity(process, ResolvePawnFromHandle(localPawnHandle));
    }

    const PlayerData& GetLocalPlayer() const { return localPlayer; }

    std::vector<PlayerData> GetValidTargets() {
        std::vector<PlayerData> targets;
        int localTeam = localPlayer.team;
        for (const auto& player : players) {
            if (player.team == localTeam) continue;
            if (!player.isAlive || player.isDormant) continue;
            targets.push_back(player);
        }
        return targets;
    }

    std::vector<PlayerData> GetAllPlayers() { return players; }

    PlayerData* GetPlayerByIndex(int index) {
        for (auto& player : players) {
            if (player.index == index) return &player;
        }
        return nullptr;
    }
};
