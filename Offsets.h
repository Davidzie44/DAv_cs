#pragma once

#include <cstdint>

namespace Offsets {
    namespace client_dll {
        constexpr uintptr_t dwCSGOInput = 0x2356240;
        constexpr uintptr_t dwEntityList = 0x24E76A0;
        constexpr uintptr_t dwGameEntitySystem = 0x24E76A0;
        constexpr uintptr_t dwGameEntitySystem_highestEntityIndex = 0x2090;
        constexpr uintptr_t dwGameRules = 0x2341158;
        constexpr uintptr_t dwGlobalVars = 0x20616D0;
        constexpr uintptr_t dwGlowManager = 0x233DF50;
        constexpr uintptr_t dwLocalPlayerController = 0x2320720;
        constexpr uintptr_t dwLocalPlayerPawn = 0x2341698;
        constexpr uintptr_t dwPlantedC4 = 0x234FF98;
        constexpr uintptr_t dwPrediction = 0x23415A0;
        constexpr uintptr_t dwSensitivity = 0x233EA68;
        constexpr uintptr_t dwSensitivity_sensitivity = 0x58;
        constexpr uintptr_t dwViewAngles = 0x23568C8;
        constexpr uintptr_t dwViewMatrix = 0x2346B30;
        constexpr uintptr_t dwViewRender = 0x2346EE0;
        constexpr uintptr_t dwWeaponC4 = 0x22BED20;
        constexpr uintptr_t dwCInputPtrGlobal = 0x23568C8;
    }

    namespace engine2_dll {
        constexpr uintptr_t dwNetworkGameClient = 0x90A1A0;
        constexpr uintptr_t dwNetworkGameClient_localPlayer = 0xF8;
        constexpr uintptr_t dwNetworkGameClient_clientTickCount = 0x378;
        constexpr uintptr_t dwNetworkGameClient_deltaTick = 0x24C;
        constexpr uintptr_t dwNetworkGameClient_signOnState = 0x230;
        constexpr uintptr_t dwBuildNumber = 0x60CC74;
        constexpr uintptr_t dwWindowHeight = 0x90E5C4;
        constexpr uintptr_t dwWindowWidth = 0x90E5C0;
    }

    namespace schema {
        constexpr uintptr_t m_iHealth = 0x34C;
        constexpr uintptr_t m_iTeamNum = 0x3EB;
        constexpr uintptr_t m_lifeState = 0x354;
        constexpr uintptr_t m_bDormant = 0x103;
        constexpr uintptr_t m_pGameSceneNode = 0x330;
        constexpr uintptr_t m_modelState = 0x150;
        constexpr uintptr_t m_boneArray = 0x80;
        constexpr uintptr_t m_vecOrigin = 0x1390;
        constexpr uintptr_t m_vecAbsOrigin = 0xC8;
        constexpr uintptr_t m_iIDEntIndex = 0x33FC;
        constexpr uintptr_t m_flFlashOverlayAlpha = 0x13F4;
        constexpr uintptr_t m_aimPunchAngle = 0x23F0;
        constexpr uintptr_t m_iShotsFired = 0x1C64;
        constexpr uintptr_t m_hPlayerPawn = 0x90C;
        constexpr uintptr_t m_bPawnIsAlive = 0x914;
        constexpr uintptr_t m_sSanitizedPlayerName = 0x6F4;
        constexpr uintptr_t m_iszPlayerName = 0x6F4;
        constexpr uintptr_t m_ArmorValue = 0x1C7C;
        constexpr uintptr_t m_bIsDefusing = 0x1C52;
        constexpr uintptr_t m_bHasDefuser = 0x23E9;
        constexpr uintptr_t m_vecLastClipCameraPos = 0x1604;
        constexpr uintptr_t m_angEyeAngles = 0x3320;
        constexpr uintptr_t m_fFlags = 0x3F8;
    }
}
