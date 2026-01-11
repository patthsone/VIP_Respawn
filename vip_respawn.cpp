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

PLUGIN_EXPOSE(vip_respawn, g_vip_respawn);

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

bool OnRespawnCommand(int iSlot, const char* szContent)
{
	if(g_pVIPCore->VIP_IsClientVIP(iSlot))
	{
		int iCount = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "Respawn");
		if(iCount > 0)
		{
            if(g_pRespawnTimers[iSlot] == nullptr && !g_isActive[iSlot])
            {
                g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("RespawnIsNotAvailable"));
                return false;
            }
			if(iCount > g_iRespawns[iSlot] || iCount == 0)
			{
				CCSPlayerController* pPlayerController =  CCSPlayerController::FromSlot(iSlot);
				if(!pPlayerController) return false;
				CCSPlayerPawnBase* pPlayerPawn = pPlayerController->m_hPlayerPawn();
				if (!pPlayerPawn || pPlayerPawn->m_lifeState() == LIFE_ALIVE)
				{
					g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("YourAlive"));
					return false;
				}
				int iTeam = pPlayerController->m_iTeamNum();
				if(iTeam < 2)
				{
					g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("SelectTeam"));
					return false;
				}
				if(!g_isActive[iSlot])
				{
					g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("RespawnIsNotAvailable"));
					return false;
				}

				if(GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "RespawnAccessMinPlayers"))
				{
					g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForRespawn"));
					return false;
				}

				g_iRespawns[iSlot]++;
				g_isActive[iSlot] = false;
				
				g_pPlayers->Respawn(iSlot);

				int iHealth = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "HPAfterRespawn");
				pPlayerPawn->m_iHealth() = iHealth;
			}
			else g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("LimitRespawn"));
		}
	}
	return false;
}

bool OnSelect(int iSlot, const char* szFeature)
{
	OnRespawnCommand(iSlot, "");
	return false;
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

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	for (size_t iSlot = 0; iSlot < 64; iSlot++)
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
		
		g_iRespawns[iSlot] = 0;
		
        if (g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "AutoRespawn"))
		{
            g_AisActive[iSlot] = true;
        }
	
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
    }
}

void OnPlayerDeath(const char* sName, IGameEvent* event, bool bDontBroadcast)
{
    int iSlot = event->GetInt("userid");
    if (iSlot >= 0 && iSlot < 64)
    {
		if (!g_pVIPCore->VIP_IsClientVIP(iSlot))
		{
			return;
		}
		
		int iCount = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "Respawn");
		
		if(iCount <= 0)
		{
			return;
		}
		
		bool bRespawnTimeExpired = g_pRespawnTimers[iSlot] == nullptr && !g_isActive[iSlot];
		
		if (g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "AutoRespawn") && g_iRespawns[iSlot] < iCount && g_AisActive[iSlot])
		{
			if(GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "AutoRespawnMinPlayers"))
			{
				g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForAutoRespawn"));
			}
			else
			{
				float fRespawnDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "AutoRespawnDelay");
				if (fRespawnDelay > 0)
				{
					g_pAutoRespawnDelayTimers[iSlot] = g_pUtils->CreateTimer(g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "AutoRespawnDelay"), [iSlot, iCount]() -> float {
						if (iSlot < 0 || iSlot >= 64) return -1.0f;
						CCSPlayerController* pPlayerController =  CCSPlayerController::FromSlot(iSlot);
						if(!pPlayerController) return -1.0f;
						CCSPlayerPawn* pPlayerPawn = pPlayerController->m_hPlayerPawn();
						if (!pPlayerPawn || pPlayerPawn->IsAlive()) return -1.0f;
						g_iRespawns[iSlot]++;
						g_pPlayers->Respawn(iSlot);
						int iHealth = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "HPAfterAutoRespawn");
						pPlayerPawn->m_iHealth() = iHealth;
						
						const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
						const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnRemaining");
						char szResp[128];
						snprintf(szResp, sizeof(szResp), fmt, iCount - g_iRespawns[iSlot]);
						g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
						return -1.0f;
					});
				}
				else
				{
					CCSPlayerController* pPlayerController =  CCSPlayerController::FromSlot(iSlot);
					if(!pPlayerController) return;
					CCSPlayerPawn* pPlayerPawn = pPlayerController->m_hPlayerPawn();
					if (!pPlayerPawn || pPlayerPawn->IsAlive()) return;
					g_iRespawns[iSlot]++;
					g_pPlayers->Respawn(iSlot);
					int iHealth = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "HPAfterAutoRespawn");
					pPlayerPawn->m_iHealth() = iHealth;
					
					const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
					const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnRemaining");
					char szResp[128];
					snprintf(szResp, sizeof(szResp), fmt, iCount - g_iRespawns[iSlot]);
					g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
				}
				return;
			}
		}

        if (g_iRespawns[iSlot] < iCount && !bRespawnTimeExpired)
        {
            if(GetOnlinePlayers() < g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "RespawnMinPlayers"))
            {
                g_pUtils->PrintToChat(iSlot, "%s %s", g_pVIPCore->VIP_GetTranslate("Prefix"), g_pVIPCore->VIP_GetTranslate("NotEnoughOnlinePlayersForRespawn"));
            }
            else
            {
                float fRespawnDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "RespawnDelay");
                if (fRespawnDelay > 0)
                {
                    g_pRespawnDelayTimers[iSlot] = g_pUtils->CreateTimer(g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, "RespawnDelay"), [iSlot, iCount]() -> float {
                        if (iSlot < 0 || iSlot >= 64) return -1.0f;
                        
                        if (g_pRespawnTimers[iSlot] == nullptr && !g_isActive[iSlot]) {
                            return -1.0f;
                        }
                        
                        g_isActive[iSlot] = true;
                        const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
                        const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnAvailable");
                        char szResp[128];
                        snprintf(szResp, sizeof(szResp), fmt, iCount - g_iRespawns[iSlot]);
                        g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
                        return -1.0f;
                    });
                }
                else
                {
                    if (g_pRespawnTimers[iSlot] != nullptr || g_isActive[iSlot]) {
                        g_isActive[iSlot] = true;
                        const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
                        const char* fmt   = g_pVIPCore->VIP_GetTranslate("RespawnAvailable");
                        char szResp[128];
                        snprintf(szResp, sizeof(szResp), fmt, iCount - g_iRespawns[iSlot]);
                        g_pUtils->PrintToChat(iSlot, "%s %s", prefix, szResp);
                    }
                }
            }
        }
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
	g_pUtils->RegCommand(g_PLID, {"mm_respawn", "sm_respawn", "respawn"}, {"!respawn", "respawn"}, OnRespawnCommand);
}

std::string OnDisplay(int iSlot, const char* szFeature)
{
	int iCount = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "Respawn");
	char szDisplay[128];
	g_SMAPI->Format(szDisplay, sizeof(szDisplay), "%s [%i]", g_pVIPCore->VIP_GetTranslate(szFeature), iCount - g_iRespawns[iSlot]);
	return std::string(szDisplay);
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

const char *vip_respawn::GetLicense()
{
	return "Public";
}

const char *vip_respawn::GetVersion()
{
	return "1.0";
}

const char *vip_respawn::GetDate()
{
	return __DATE__;
}

const char *vip_respawn::GetLogTag()
{
	return "[VIP-RESPAWN-Addition]";
}

const char *vip_respawn::GetAuthor()
{
	return "PattHs and Pisex";
}

const char *vip_respawn::GetDescription()
{
	return "";
}

const char *vip_respawn::GetName()
{
	return "[VIP] Respawn";
}

const char *vip_respawn::GetURL()
{
	return "https://nova-hosting.ru?ref=TNC36I97";
}
