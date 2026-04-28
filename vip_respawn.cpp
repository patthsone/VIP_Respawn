#include <stdio.h>
#include "vip_respawn.h"

vip_respawn g_vip_respawn;

IVIPApi* g_pVIPCore;
IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayers;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

int g_iRespawns[64];
bool g_isActive[64];
bool g_AisActive[64];
CTimer* g_pRespawnTimers[64] = { nullptr };
CTimer* g_pAutoRespawnTimers[64] = { nullptr };
CTimer* g_pRespawnDelayTimers[64] = { nullptr };
CTimer* g_pAutoRespawnDelayTimers[64] = { nullptr };
CTimer* g_pImmunityTimers[64] = { nullptr };

PLUGIN_EXPOSE(vip_respawn, g_vip_respawn);

int GetTeamTotalPlayers(int iTeam)
{
    int count = 0;
    for (int i = 0; i < 64; i++)
    {
        if (!g_pPlayers->IsAuthenticated(i) || !g_pPlayers->IsInGame(i) || !g_pPlayers->IsConnected(i))
            continue;
        if (g_pPlayers->IsFakeClient(i))
            continue;
        CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
        if (!pController) continue;
        if (pController->m_iTeamNum() == iTeam)
            count++;
    }
    return count;
}

int GetTeamAlivePlayers(int iTeam)
{
    int count = 0;
    for (int i = 0; i < 64; i++)
    {
        if (!g_pPlayers->IsAuthenticated(i) || !g_pPlayers->IsInGame(i) || !g_pPlayers->IsConnected(i))
            continue;
        if (g_pPlayers->IsFakeClient(i))
            continue;
        CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
        if (!pController) continue;
        if (pController->m_iTeamNum() != iTeam) continue;
        CCSPlayerPawn* pPawn = pController->m_hPlayerPawn();
        if (!pPawn) continue;
        if (pPawn->IsAlive())
            count++;
    }
    return count;
}

void GiveImmunity(int iSlot, float duration)
{
    if (g_pImmunityTimers[iSlot])
    {
        g_pUtils->RemoveTimer(g_pImmunityTimers[iSlot]);
        g_pImmunityTimers[iSlot] = nullptr;
    }

    CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
    if (!pController) return;
    CCSPlayerPawn* pPawn = pController->m_hPlayerPawn();
    if (!pPawn) return;

    pPawn->m_bTakesDamage = false;

    g_pImmunityTimers[iSlot] = g_pUtils->CreateTimer(duration, [iSlot]() -> float {
        CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
        if (pController && g_pPlayers->IsConnected(iSlot))
        {
            CCSPlayerPawn* pPawn = pController->m_hPlayerPawn();
            if (pPawn)
                pPawn->m_bTakesDamage = true;
        }
        g_pImmunityTimers[iSlot] = nullptr;
        return -1.0f;
    });
}

int GetOnlinePlayers()
{
    int iCount = 0;
    for (int i = 0; i < 64; i++)
    {
        if (g_pPlayers->IsFakeClient(i))
            continue;
        if (!g_pPlayers->IsAuthenticated(i))
            continue;
        if (!g_pPlayers->IsInGame(i))
            continue;
        if (!g_pPlayers->IsConnected(i))
            continue;
        iCount++;
    }
    return iCount;
}

void SetupPlayerRespawn(int iSlot)
{
    if (!g_pVIPCore->VIP_IsClientVIP(iSlot))
        return;

    if (g_pRespawnTimers[iSlot])
    {
        g_pUtils->RemoveTimer(g_pRespawnTimers[iSlot]);
        g_pRespawnTimers[iSlot] = nullptr;
    }
    if (g_pAutoRespawnTimers[iSlot])
    {
        g_pUtils->RemoveTimer(g_pAutoRespawnTimers[iSlot]);
        g_pAutoRespawnTimers[iSlot] = nullptr;
    }

    g_isActive[iSlot] = false;
    g_AisActive[iSlot] = g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "AutoRespawn");

    float fNoRespawnDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "NoRespawnDelay");
    if (fNoRespawnDelay > 0)
    {
        g_pRespawnTimers[iSlot] = g_pUtils->CreateTimer(fNoRespawnDelay, [iSlot]() -> float {
            g_isActive[iSlot] = false;
            g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("RespawnIsNoLongerAvailable"));
            g_pRespawnTimers[iSlot] = nullptr;
            return -1.0f;
        });
    }
    else
    {
        g_isActive[iSlot] = true;
    }

    float fNoAutoRespawnDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "NoAutoRespawnDelay");
    if (fNoAutoRespawnDelay > 0 && g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "AutoRespawn"))
    {
        g_pAutoRespawnTimers[iSlot] = g_pUtils->CreateTimer(fNoAutoRespawnDelay, [iSlot]() -> float {
            g_AisActive[iSlot] = false;
            g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("AutoRespawnIsNoLongerAvailable"));
            g_pAutoRespawnTimers[iSlot] = nullptr;
            return -1.0f;
        });
    }
}

bool OnRespawnCommand(int iSlot, const char* szContent)
{
    if (!g_pVIPCore->VIP_IsClientVIP(iSlot))
        return false;

    int iCount = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "Respawn");
    if (iCount <= 0)
        return false;

    if (!g_isActive[iSlot])
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("RespawnIsNotAvailable"));
        return false;
    }

    if (iCount > 0 && g_iRespawns[iSlot] >= iCount)
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("LimitRespawn"));
        return false;
    }

    CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
    if (!pPlayerController) return false;

    CCSPlayerPawn* pPlayerPawn = pPlayerController->m_hPlayerPawn();
    if (!pPlayerPawn || pPlayerPawn->m_lifeState() == LIFE_ALIVE)
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("YourAlive"));
        return false;
    }

    int iTeam = pPlayerController->m_iTeamNum();
    if (iTeam < 2)
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("SelectTeam"));
        return false;
    }

    if (GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "RespawnAccessMinPlayers"))
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForRespawn"));
        return false;
    }

    int iOpponentTeam = (iTeam == 2) ? 3 : 2;
    if (GetTeamTotalPlayers(iTeam) >= GetTeamTotalPlayers(iOpponentTeam))
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("TeamBalanceError"));
        return false;
    }

    if (GetTeamAlivePlayers(iTeam) <= 1)
    {
        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("ClutchModeNoRespawn"));
        return false;
    }

    g_iRespawns[iSlot]++;
    g_isActive[iSlot] = false;

    g_pPlayers->Respawn(iSlot);

    CCSPlayerPawn* pNewPawn = pPlayerController->m_hPlayerPawn();
    if (pNewPawn)
    {
        int iHealth = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "HPAfterRespawn");
        pNewPawn->m_iHealth() = iHealth;
    }

    GiveImmunity(iSlot, 5.0f);

    return false;
}

bool OnSelect(int iSlot, const char* szFeature)
{
    return OnRespawnCommand(iSlot, "");
}

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    for (int iSlot = 0; iSlot < 64; iSlot++)
    {
        if (g_pRespawnTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pRespawnTimers[iSlot]);
            g_pRespawnTimers[iSlot] = nullptr;
        }
        if (g_pAutoRespawnTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pAutoRespawnTimers[iSlot]);
            g_pAutoRespawnTimers[iSlot] = nullptr;
        }
        if (g_pRespawnDelayTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pRespawnDelayTimers[iSlot]);
            g_pRespawnDelayTimers[iSlot] = nullptr;
        }
        if (g_pAutoRespawnDelayTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pAutoRespawnDelayTimers[iSlot]);
            g_pAutoRespawnDelayTimers[iSlot] = nullptr;
        }
        if (g_pImmunityTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pImmunityTimers[iSlot]);
            g_pImmunityTimers[iSlot] = nullptr;
        }

        g_iRespawns[iSlot] = 0;
        SetupPlayerRespawn(iSlot);
    }
}

void OnRoundEnd(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    for (int iSlot = 0; iSlot < 64; iSlot++)
    {
        g_isActive[iSlot] = false;
        g_AisActive[iSlot] = false;

        if (g_pRespawnTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pRespawnTimers[iSlot]);
            g_pRespawnTimers[iSlot] = nullptr;
        }
        if (g_pAutoRespawnTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pAutoRespawnTimers[iSlot]);
            g_pAutoRespawnTimers[iSlot] = nullptr;
        }
        if (g_pRespawnDelayTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pRespawnDelayTimers[iSlot]);
            g_pRespawnDelayTimers[iSlot] = nullptr;
        }
        if (g_pAutoRespawnDelayTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pAutoRespawnDelayTimers[iSlot]);
            g_pAutoRespawnDelayTimers[iSlot] = nullptr;
        }
        if (g_pImmunityTimers[iSlot]) {
            g_pUtils->RemoveTimer(g_pImmunityTimers[iSlot]);
            g_pImmunityTimers[iSlot] = nullptr;
        }
    }
}

void OnPlayerDeath(const char* sName, IGameEvent* event, bool bDontBroadcast)
{
    CCSPlayerController* pController = (CCSPlayerController*)event->GetPlayerController("userid");
    if (!pController) return;

    int iSlot = pController->GetPlayerSlot();
    if (iSlot < 0 || iSlot >= 64) return;

    if (!g_pVIPCore->VIP_IsClientVIP(iSlot))
        return;

    int iCount = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "Respawn");
    if (iCount <= 0)
        return;

    bool bRespawnTimeExpired = (g_pRespawnTimers[iSlot] == nullptr && !g_isActive[iSlot]);

    if (g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "AutoRespawn") && g_iRespawns[iSlot] < iCount && g_AisActive[iSlot])
    {
        if (GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "AutoRespawnMinPlayers"))
        {
            g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForAutoRespawn"));
        }
        else
        {
            int iTeam = pController->m_iTeamNum();
            int iOpponentTeam = (iTeam == 2) ? 3 : 2;

            if (GetTeamTotalPlayers(iTeam) >= GetTeamTotalPlayers(iOpponentTeam))
            {
                g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("TeamBalanceError"));
                return;
            }

            if (GetTeamAlivePlayers(iTeam) <= 1)
            {
                g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("ClutchModeNoRespawn"));
                return;
            }

            float fRespawnDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "AutoRespawnDelay");
            if (fRespawnDelay > 0)
            {
                g_pAutoRespawnDelayTimers[iSlot] = g_pUtils->CreateTimer(fRespawnDelay, [iSlot, iCount]() -> float {
                    if (!g_pPlayers->IsConnected(iSlot)) return -1.0f;

                    CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
                    if (!pController) return -1.0f;

                    CCSPlayerPawn* pPlayerPawn = pController->m_hPlayerPawn();
                    if (!pPlayerPawn || pPlayerPawn->IsAlive()) return -1.0f;

                    int iTeam = pController->m_iTeamNum();
                    int iOpponentTeam = (iTeam == 2) ? 3 : 2;

                    if (GetTeamTotalPlayers(iTeam) >= GetTeamTotalPlayers(iOpponentTeam))
                    {
                        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("TeamBalanceError"));
                        return -1.0f;
                    }

                    if (GetTeamAlivePlayers(iTeam) <= 1)
                    {
                        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("ClutchModeNoRespawn"));
                        return -1.0f;
                    }

                    if (GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "AutoRespawnMinPlayers"))
                    {
                        g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForAutoRespawn"));
                        return -1.0f;
                    }

                    g_iRespawns[iSlot]++;
                    g_pPlayers->Respawn(iSlot);

                    CCSPlayerPawn* pNewPawn = pController->m_hPlayerPawn();
                    if (pNewPawn)
                    {
                        int iHealth = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "HPAfterAutoRespawn");
                        pNewPawn->m_iHealth() = iHealth;
                    }

                    GiveImmunity(iSlot, 5.0f);

                    const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
                    const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnRemaining");
                    char szResp[128];
                    int remaining = (iCount > 0) ? iCount - g_iRespawns[iSlot] : 0;
                    snprintf(szResp, sizeof(szResp), fmt, remaining);
                    g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
                    return -1.0f;
                });
            }
            else
            {
                g_iRespawns[iSlot]++;
                g_pPlayers->Respawn(iSlot);

                CCSPlayerPawn* pNewPawn = pController->m_hPlayerPawn();
                if (pNewPawn)
                {
                    int iHealth = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "HPAfterAutoRespawn");
                    pNewPawn->m_iHealth() = iHealth;
                }

                GiveImmunity(iSlot, 5.0f);

                const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
                const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnRemaining");
                char szResp[128];
                int remaining = (iCount > 0) ? iCount - g_iRespawns[iSlot] : 0;
                snprintf(szResp, sizeof(szResp), fmt, remaining);
                g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
            }
            return;
        }
    }

    if (g_iRespawns[iSlot] < iCount && !bRespawnTimeExpired)
    {
        if (GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "RespawnMinPlayers"))
        {
            g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForRespawn"));
        }
        else
        {
            float fRespawnDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "RespawnDelay");
            if (fRespawnDelay > 0)
            {
                g_pRespawnDelayTimers[iSlot] = g_pUtils->CreateTimer(fRespawnDelay, [iSlot, iCount]() -> float {
                    if (!g_pPlayers->IsConnected(iSlot)) return -1.0f;

                    if (g_pRespawnTimers[iSlot] != nullptr || !g_isActive[iSlot]) {
                        return -1.0f;
                    }

                    g_isActive[iSlot] = true;
                    const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
                    const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnAvailable");
                    char szResp[128];
                    int remaining = (iCount > 0) ? iCount - g_iRespawns[iSlot] : 0;
                    snprintf(szResp, sizeof(szResp), fmt, remaining);
                    g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
                    return -1.0f;
                });
            }
            else
            {
                if (g_isActive[iSlot])
                {
                    const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
                    const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnAvailable");
                    char szResp[128];
                    int remaining = (iCount > 0) ? iCount - g_iRespawns[iSlot] : 0;
                    snprintf(szResp, sizeof(szResp), fmt, remaining);
                    g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
                }
            }
        }
    }
}

void OnPlayerSpawn(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
    if (!pController) return;
    int iSlot = pController->GetPlayerSlot();
    if (iSlot < 0 || iSlot >= 64) return;

    if (!g_pPlayers->IsConnected(iSlot)) return;

    if (!g_pRespawnTimers[iSlot] && !g_pAutoRespawnTimers[iSlot] && !g_pRespawnDelayTimers[iSlot] && !g_pAutoRespawnDelayTimers[iSlot])
    {
        SetupPlayerRespawn(iSlot);
    }
}

void OnPlayerDisconnect(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
    if (!pController) return;
    int iSlot = pController->GetPlayerSlot();
    if (iSlot < 0 || iSlot >= 64) return;

    if (g_pRespawnTimers[iSlot]) {
        g_pUtils->RemoveTimer(g_pRespawnTimers[iSlot]);
        g_pRespawnTimers[iSlot] = nullptr;
    }
    if (g_pAutoRespawnTimers[iSlot]) {
        g_pUtils->RemoveTimer(g_pAutoRespawnTimers[iSlot]);
        g_pAutoRespawnTimers[iSlot] = nullptr;
    }
    if (g_pRespawnDelayTimers[iSlot]) {
        g_pUtils->RemoveTimer(g_pRespawnDelayTimers[iSlot]);
        g_pRespawnDelayTimers[iSlot] = nullptr;
    }
    if (g_pAutoRespawnDelayTimers[iSlot]) {
        g_pUtils->RemoveTimer(g_pAutoRespawnDelayTimers[iSlot]);
        g_pAutoRespawnDelayTimers[iSlot] = nullptr;
    }
    if (g_pImmunityTimers[iSlot]) {
        g_pUtils->RemoveTimer(g_pImmunityTimers[iSlot]);
        g_pImmunityTimers[iSlot] = nullptr;
    }

    g_iRespawns[iSlot] = 0;
    g_isActive[iSlot] = false;
    g_AisActive[iSlot] = false;
}

void OnPlayerTeam(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
    if (!pController) return;
    int iSlot = pController->GetPlayerSlot();
    if (iSlot < 0 || iSlot >= 64) return;

    if (g_iRespawns[iSlot] != 0)
    {
        g_iRespawns[iSlot] = 0;
        SetupPlayerRespawn(iSlot);
    }
}

CGameEntitySystem* GameEntitySystem()
{
    return g_pVIPCore->VIP_GetEntitySystem();
};

void VIP_OnVIPLoaded()
{
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = g_pGameEntitySystem;
    g_pUtils->HookEvent(g_PLID, "round_start", OnRoundStart);
    g_pUtils->HookEvent(g_PLID, "round_end", OnRoundEnd);
    g_pUtils->HookEvent(g_PLID, "player_death", OnPlayerDeath);
    g_pUtils->HookEvent(g_PLID, "player_spawn", OnPlayerSpawn);
    g_pUtils->HookEvent(g_PLID, "player_disconnect", OnPlayerDisconnect);
    g_pUtils->HookEvent(g_PLID, "player_team", OnPlayerTeam);
    g_pUtils->RegCommand(g_PLID, {"mm_respawn", "sm_respawn", "respawn"}, {"!respawn", "respawn"}, OnRespawnCommand);
}

std::string OnDisplay(int iSlot, const char* szFeature)
{
    int iCount = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "Respawn");
    int remaining = (iCount > 0 && iCount - g_iRespawns[iSlot] >= 0) ? iCount - g_iRespawns[iSlot] : 0;
    char szDisplay[128];
    g_SMAPI->Format(szDisplay, sizeof(szDisplay), "%s [%i]", g_pVIPCore->VIP_GetTranslate(szFeature), remaining);
    return std::string(szDisplay);
}

bool vip_respawn::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    g_SMAPI->AddListener( this, this );
    return true;
}

bool vip_respawn::Unload(char *error, size_t maxlen)
{
    delete g_pVIPCore;
    delete g_pUtils;
    return true;
}

void vip_respawn::AllPluginsLoaded()
{
    int ret;
    g_pVIPCore = (IVIPApi*)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);

    if (ret == META_IFACE_FAILED)
    {
        char error[64];
        V_strncpy(error, "Failed to lookup vip core. Aborting", 64);
        ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
        std::string sBuffer = "meta unload "+std::to_string(g_PLID);
        engine->ServerCommand(sBuffer.c_str());
        return;
    }
    g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);

    if (ret == META_IFACE_FAILED)
    {
        char error[64];
        V_strncpy(error, "Failed to lookup utils api. Aborting", 64);
        ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
        std::string sBuffer = "meta unload "+std::to_string(g_PLID);
        engine->ServerCommand(sBuffer.c_str());
        return;
    }
    g_pPlayers = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);

    if (ret == META_IFACE_FAILED)
    {
        char error[64];
        V_strncpy(error, "Failed to lookup players api. Aborting", 64);
        ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
        std::string sBuffer = "meta unload "+std::to_string(g_PLID);
        engine->ServerCommand(sBuffer.c_str());
        return;
    }
    g_pVIPCore->VIP_OnVIPLoaded(VIP_OnVIPLoaded);
    g_pVIPCore->VIP_RegisterFeature("Respawn", VIP_INT, SELECTABLE, OnSelect, nullptr, OnDisplay);
    g_pVIPCore->VIP_RegisterFeature("AutoRespawn", VIP_BOOL, TOGGLABLE);
    g_pVIPCore->VIP_RegisterFeature("NoRespawnDelay", VIP_FLOAT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("NoAutoRespawnDelay", VIP_FLOAT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("RespawnDelay", VIP_FLOAT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("AutoRespawnDelay", VIP_FLOAT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("RespawnMinPlayers", VIP_INT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("RespawnAccessMinPlayers", VIP_INT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("AutoRespawnMinPlayers", VIP_INT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("HPAfterRespawn", VIP_INT, HIDE);
    g_pVIPCore->VIP_RegisterFeature("HPAfterAutoRespawn", VIP_INT, HIDE);
}

const char *vip_respawn::GetLicense() { return "Public"; }
const char *vip_respawn::GetVersion() { return "1.1"; }
const char *vip_respawn::GetDate() { return __DATE__; }
const char *vip_respawn::GetLogTag() { return "[VIP-RESPAWN-Addition]"; }
const char *vip_respawn::GetAuthor() { return "PattHs and Pisex"; }
const char *vip_respawn::GetDescription() { return "VIP-RESPAWN"; }
const char *vip_respawn::GetName() { return "[VIP] Respawn"; }
const char *vip_respawn::GetURL() { return "https://nova-hosting.ru?ref=TNC36I97"; }
