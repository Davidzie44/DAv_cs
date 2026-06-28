#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "Offsets.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>

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
    uintptr_t entityListEntry;
    static constexpr uintptr_t ENTITY_LIST_ENTRY_SIZE = 0x70;

    uintptr_t ResolveController(int index) {
        if (entityListEntry == 0) return 0;
        try {
            return process.ReadMemory<uintptr_t>(
                entityListEntry + ((uintptr_t)(index + 1)) * ENTITY_LIST_ENTRY_SIZE
            );
        } catch (...) { return 0; }
    }

    uintptr_t ResolvePawnFromHandle(uint32_t handle) {
        if (handle == 0 || handle == 0xFFFFFFFF || entityListBase == 0) return 0;
        try {
            uint32_t index = handle & 0x7FFF;
            uintptr_t listPage = process.ReadMemory<uintptr_t>(
                entityListBase + 0x10 + 8 * ((uintptr_t)(index >> 9))
            );
            if (listPage == 0) return 0;
            return process.ReadMemory<uintptr_t>(
                listPage + ENTITY_LIST_ENTRY_SIZE * (uintptr_t)(index & 0x1FF)
            );
        } catch (...) { return 0; }
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
    EntityManager(ProcessMemory& pm) : process(pm), entityListBase(0), entityListEntry(0) {
        localPlayer = PlayerData{};
    }

    void Update() {
        players.clear();

        entityListBase = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwEntityList
        );
        if (entityListBase == 0) return;

        entityListEntry = process.ReadMemory<uintptr_t>(entityListBase + 0x10);
        if (entityListEntry == 0) return;

        uintptr_t localPawnAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerPawn
        );
        uintptr_t localControllerAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerController
        );

        if (localPawnAddr != 0) {
            Entity localPawn(process, localPawnAddr);
            localPlayer.index = -1;
            localPlayer.health = localPawn.GetHealth();
            localPlayer.team = localPawn.GetTeam();
            localPlayer.armor = localPawn.GetArmor();
            localPlayer.position = ReadPositionFromPawn(localPawnAddr);
            localPlayer.headPosition = localPawn.GetHeadPosition();
            localPlayer.isAlive = localPawn.GetHealth() > 0;
            localPlayer.isDormant = IsDormant(localPawnAddr);
            localPlayer.pawnAddr = localPawnAddr;
            localPlayer.controllerAddr = localControllerAddr;
            localPlayer.entityIndex = -1;
            localPlayer.flashAlpha = localPawn.GetFlashAlpha();
            localPlayer.name = "Local Player";
            if (localControllerAddr != 0) {
                localPlayer.name = GetPlayerName(localControllerAddr);
            }
        }

        for (int i = 0; i < 64; i++) {
            uintptr_t controller = ResolveController(i);
            if (controller == 0) continue;

            uint32_t pawnHandle = process.ReadMemory<uint32_t>(controller + Offsets::schema::m_hPlayerPawn);
            if (pawnHandle == 0 || pawnHandle == 0xFFFFFFFF) continue;

            uintptr_t pawn = ResolvePawnFromHandle(pawnHandle);
            if (pawn == 0) continue;

            if (pawn == localPawnAddr) continue;

            int health = 0;
            int team = 0;
            try {
                health = process.ReadMemory<int>(pawn + Offsets::schema::m_iHealth);
                team = process.ReadMemory<int>(pawn + Offsets::schema::m_iTeamNum);
            } catch (...) { continue; }

            if (health <= 0 || health > 200) continue;
            if (team != 2 && team != 3) continue;

            Vector3 position = ReadPositionFromPawn(pawn);
            if (position.Length() < 1.0f) continue;

            Entity pawnEntity(process, pawn);
            Vector3 headPosition = pawnEntity.GetHeadPosition();
            if (headPosition.Length() < 1.0f) headPosition = position;

            float distance = 0;
            if (localPlayer.pawnAddr != 0) {
                distance = localPlayer.position.Distance(position);
            }

            PlayerData data;
            data.index = i;
            data.health = health;
            data.team = team;
            data.armor = pawnEntity.GetArmor();
            data.position = position;
            data.headPosition = headPosition;
            data.name = GetPlayerName(controller);
            data.isAlive = true;
            data.isDormant = false;
            data.distance = distance;
            data.pawnAddr = pawn;
            data.controllerAddr = controller;
            data.entityIndex = i;
            data.flashAlpha = pawnEntity.GetFlashAlpha();
            try {
                data.isDefusing = process.ReadMemory<bool>(pawn + Offsets::schema::m_bIsDefusing);
            } catch (...) {
                data.isDefusing = false;
            }

            players.push_back(data);
        }
    }

    Entity GetLocalPawn() {
        uintptr_t localPawnAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerPawn
        );
        return Entity(process, localPawnAddr);
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

    std::vector<PlayerData> GetPlayersByTeam(int teamNum) {
        std::vector<PlayerData> teamPlayers;
        for (const auto& player : players) {
            if (player.team == teamNum) teamPlayers.push_back(player);
        }
        return teamPlayers;
    }

    PlayerData* GetPlayerByIndex(int index) {
        for (auto& player : players) {
            if (player.index == index) return &player;
        }
        return nullptr;
    }

    void SortByDistance() {
        std::sort(players.begin(), players.end(),
            [](const PlayerData& a, const PlayerData& b) { return a.distance < b.distance; });
    }
};
