//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "engine/IEngineSound.h"
#include "c_world.h"
#include "ivmodemanager.h"
#include "activitylist.h"
#include "decals.h"
#include "engine/ivmodelinfo.h"
#include "iviewrender.h"
#include "ivieweffects.h"
#include "shake.h"
#include "eventlist.h"
// NVNT haptic include for notification of world precache
#include "haptics/haptic_utils.h"
#include "ammodef.h"
#include "iachievementmgr.h"
#include "usermessages.h"
#include "engine/ivdebugoverlay.h"
#include "hud_basechat.h"
#include "hud_macros.h"
#include "hud_vote.h"
#include "hltvcamera.h"
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>
#include <vgui/IPanel.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/AnimationController.h>
#include "vgui_int.h"
#include "cam_thirdperson.h"
#include "c_vguiscreen.h"
#include "c_rumble.h"
#include "c_team.h"
#include "sourcevr/isourcevirtualreality.h"
#include "iinput.h"
#include "ienginevgui.h"
#include "weapon_selection.h"
#include "achievementmgr.h"
#include "c_playerresource.h"

#if defined( REPLAY_ENABLED )
#include "replay/replaycamera.h"
#include "replay/ireplaysystem.h"
#include "replay/iclientreplaycontext.h"
#include "replay/ireplaymanager.h"
#include "replay/replay.h"
#include "replay/ienginereplay.h"
#include "replay/vgui/replayreminderpanel.h"
#include "replay/vgui/replaymessagepanel.h"
#include "econ/econ_controls.h"
#include "econ/confirm_dialog.h"

extern IClientReplayContext* g_pClientReplayContext;
extern ConVar replay_rendersetting_renderglow;
#endif

#if defined USES_ECON_ITEMS
#include "econ_item_view.h"
#endif

#if defined( TF_CLIENT_DLL )
#include "c_tf_player.h"
#include "econ_item_description.h"
#endif

#ifdef CSTRIKE_DLL
#include "cs_gamerules.h"
#endif // CSTRIKE_DLL
#ifdef DOD_DLL
#include "dod_gamerules.h"
#endif // DOD_DLL
#if defined(HL1_CLIENT_DLL) && !defined(HL1MP_CLIENT_DLL)
#include "hl1_gamerules.h"
#endif // HL1_DLL
#ifdef HL1MP_CLIENT_DLL
#include "hl1mp_gamerules.h"
#endif // HL1MP_DLL
#if defined(HL2_CLIENT_DLL) && !defined(HL2MP) && !defined(PORTAL)
#include "hl2_gamerules.h"
#endif
#ifdef HL2MP
#include "hl2mp_gamerules.h"
#endif
#ifdef PORTAL
#include "portal_gamerules.h"
#endif // PORTAL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CWorld
#undef CWorld
#endif

class CHudWeaponSelection;
class CHudChat;
class CHudVote;

ConVar g_Language("g_Language", "0", FCVAR_REPLICATED);
ConVar sk_autoaim_mode("sk_autoaim_mode", "1", FCVAR_ARCHIVE | FCVAR_REPLICATED);
ConVar	old_radius_damage("old_radiusdamage", "0.0", FCVAR_REPLICATED);
ConVar cl_drawhud("cl_drawhud", "1", FCVAR_CHEAT, "Enable the rendering of the hud");
ConVar hud_takesshots("hud_takesshots", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Auto-save a scoreboard screenshot at the end of a map.");
ConVar hud_freezecamhide("hud_freezecamhide", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Hide the HUD during freeze-cam");
ConVar cl_show_num_particle_systems("cl_show_num_particle_systems", "0", FCVAR_CLIENTDLL, "Display the number of active particle systems.");
extern ConVar v_viewmodel_fov;
extern ConVar voice_modenable;

static CViewVectors g_DefaultViewVectors(
	Vector(0, 0, 64),			//VEC_VIEW (m_vView)

	Vector(-16, -16, 0),		//VEC_HULL_MIN (m_vHullMin)
	Vector(16, 16, 72),		//VEC_HULL_MAX (m_vHullMax)

	Vector(-16, -16, 0),		//VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector(16, 16, 36),		//VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector(0, 0, 28),			//VEC_DUCK_VIEW		(m_vDuckView)

	Vector(-10, -10, -10),		//VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector(10, 10, 10),		//VEC_OBS_HULL_MAX	(m_vObsHullMax)

	Vector(0, 0, 14)			//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)
);
static vgui::HContext s_hVGuiContext = DEFAULT_VGUI_CONTEXT;


IClientWorld* g_pGameRules = NULL;
extern bool IsInCommentaryMode(void);


//static C_World *g_pClientWorld;


//void ClientWorldFactoryInit()
//{
//	g_pClientWorld = new C_World;
//}

//void ClientWorldFactoryShutdown()
//{
//	delete g_pClientWorld;
//	g_pClientWorld = NULL;
//}

//static IClientNetworkable* ClientWorldFactory( int entnum, int serialNum )
//{
//	Assert( g_pClientWorld != NULL );
//
//	g_pClientWorld->Init( entnum, serialNum );
//	return g_pClientWorld;
//}


IMPLEMENT_CLIENTCLASS_NO_FACTORY( C_World, DT_WORLD, CWorld );//, ClientWorldFactory

BEGIN_RECV_TABLE( C_World, DT_WORLD)
	RecvPropFloat(RECVINFO(m_flWaveHeight)),
	RecvPropVector(RECVINFO(m_WorldMins)),
	RecvPropVector(RECVINFO(m_WorldMaxs)),
	RecvPropInt(RECVINFO(m_bStartDark)),
	RecvPropFloat(RECVINFO(m_flMaxOccludeeArea)),
	RecvPropFloat(RECVINFO(m_flMinOccluderArea)),
	RecvPropFloat(RECVINFO(m_flMaxPropScreenSpaceWidth)),
	RecvPropFloat(RECVINFO(m_flMinPropScreenSpaceWidth)),
	RecvPropString(RECVINFO(m_iszDetailSpriteMaterial)),
	RecvPropInt(RECVINFO(m_bColdWorld)),
END_RECV_TABLE()

#ifdef CSTRIKE_DLL
LINK_ENTITY_TO_CLASS(worldspawn, C_CSGameWorld);
#endif
#ifdef DOD_DLL
LINK_ENTITY_TO_CLASS(worldspawn, C_DODGameWorld);
#endif // DOD_DLL
#if defined(HL1_CLIENT_DLL) && !defined(HL1MP_CLIENT_DLL)
LINK_ENTITY_TO_CLASS(worldspawn, C_HalfLife1World);
#endif // HL1_DLL
#ifdef HL1MP_CLIENT_DLL
LINK_ENTITY_TO_CLASS(worldspawn, C_HL1MPWorld);
#endif // HL1MP_DLL
#if defined(HL2_CLIENT_DLL) && !defined(HL2MP) && !defined(PORTAL)
LINK_ENTITY_TO_CLASS(worldspawn, C_HalfLife2World);
#endif
#ifdef HL2MP
LINK_ENTITY_TO_CLASS(worldspawn, C_HL2MPWorld);
#endif
#ifdef PORTAL
LINK_ENTITY_TO_CLASS(worldspawn, C_PortalGameWorld);
#endif // PORTAL

C_World::C_World( void )
{
	m_pViewport = NULL;
	m_pChatElement = NULL;
	m_pWeaponSelection = NULL;
	m_nRootSize[0] = m_nRootSize[1] = -1;

#if defined( REPLAY_ENABLED )
	m_pReplayReminderPanel = NULL;
	m_flReplayStartRecordTime = 0.0f;
	m_flReplayStopRecordTime = 0.0f;
#endif
	g_pGameRules = this;
}

C_World::~C_World( void )
{
	delete m_pViewport;
	if (!g_pGameRules) {
		Error("m_pWorld not inited!\n");
	}
	//g_pGameRules->LevelShutdownPostEntity();
	g_pGameRules = NULL;
}

bool C_World::Init( int entnum, int iSerialNum )
{
	m_flWaveHeight = 0.0f;
	return BaseClass::Init( entnum, iSerialNum );
}

void C_World::UpdateOnRemove()
{

	//Term();
	BaseClass::UpdateOnRemove();
}

void C_World::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}

void C_World::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Always force reset to normal mode upon receipt of world in new map
	if ( updateType == DATA_UPDATE_CREATED )
	{
		//this->SwitchMode( false, true );

		if ( m_bStartDark )
		{
			ScreenFade_t sf;
			memset( &sf, 0, sizeof( sf ) );
			sf.a = 255;
			sf.r = 0;
			sf.g = 0;
			sf.b = 0;
			sf.duration = (float)(1<<SCREENFADE_FRACBITS) * 5.0f;
			sf.holdTime = (float)(1<<SCREENFADE_FRACBITS) * 1.0f;
			sf.fadeFlags = FFADE_IN | FFADE_PURGE;
			vieweffects->Fade( sf );
		}

		OcclusionParams_t params;
		params.m_flMaxOccludeeArea = m_flMaxOccludeeArea;
		params.m_flMinOccluderArea = m_flMinOccluderArea;
		engine->SetOcclusionParameters( params );

		modelinfo->SetLevelScreenFadeRange( m_flMinPropScreenSpaceWidth, m_flMaxPropScreenSpaceWidth );
	}
}

void C_World::PostDataUpdate(DataUpdateType_t updateType)
{
	ConVarRef cl_detail_sprite_material("cl_detail_sprite_material");
	cl_detail_sprite_material.SetValue(m_iszDetailSpriteMaterial);
}

void C_World::RegisterSharedActivities( void )
{
	ActivityList_RegisterSharedActivities();
	EventList_RegisterSharedEvents();
}

// -----------------------------------------
//	Sprite Index info
// -----------------------------------------
short		g_sModelIndexLaser;			// holds the index for the laser beam
const char	*g_pModelNameLaser = "sprites/laserbeam.vmt";
short		g_sModelIndexLaserDot;		// holds the index for the laser beam dot
short		g_sModelIndexFireball;		// holds the index for the fireball
short		g_sModelIndexSmoke;			// holds the index for the smoke cloud
short		g_sModelIndexWExplosion;	// holds the index for the underwater explosion
short		g_sModelIndexBubbles;		// holds the index for the bubbles model
short		g_sModelIndexBloodDrop;		// holds the sprite index for the initial blood
short		g_sModelIndexBloodSpray;	// holds the sprite index for splattered blood

//-----------------------------------------------------------------------------
// Purpose: Precache global weapon sounds
//-----------------------------------------------------------------------------
void W_Precache(void)
{
	PrecacheFileWeaponInfoDatabase( filesystem, g_pGameRules->GetEncryptionKey() );

	g_sModelIndexFireball = modelinfo->GetModelIndex ("sprites/zerogxplode.vmt");// fireball
	g_sModelIndexWExplosion = modelinfo->GetModelIndex ("sprites/WXplo1.vmt");// underwater fireball
	g_sModelIndexSmoke = modelinfo->GetModelIndex ("sprites/steam1.vmt");// smoke
	g_sModelIndexBubbles = modelinfo->GetModelIndex ("sprites/bubble.vmt");//bubbles
	g_sModelIndexBloodSpray = modelinfo->GetModelIndex ("sprites/bloodspray.vmt"); // initial blood
	g_sModelIndexBloodDrop = modelinfo->GetModelIndex ("sprites/blood.vmt"); // splattered blood 
	g_sModelIndexLaser = modelinfo->GetModelIndex( (char *)g_pModelNameLaser );
	g_sModelIndexLaserDot = modelinfo->GetModelIndex("sprites/laserdot.vmt");
}

void C_World::Precache( void )
{


	// Get weapon precaches
	W_Precache();	

	// Call all registered precachers.
	CPrecacheRegister::Precache();
	// NVNT notify system of precache
	if (haptics)
		haptics->WorldPrecache();
}

void C_World::Spawn( void )
{
	
}

C_World *GetClientWorldEntity()
{
	//Assert( g_pClientWorld != NULL );
	return (C_World*)EntityList()->GetBaseEntity(0);
}

static void __MsgFunc_Rumble(bf_read& msg)
{
	unsigned char waveformIndex;
	unsigned char rumbleData;
	unsigned char rumbleFlags;

	waveformIndex = msg.ReadByte();
	rumbleData = msg.ReadByte();
	rumbleFlags = msg.ReadByte();

	RumbleEffect(waveformIndex, rumbleData, rumbleFlags);
}

static void __MsgFunc_VGUIMenu(bf_read& msg)
{
	char panelname[2048];

	msg.ReadString(panelname, sizeof(panelname));

	bool  bShow = msg.ReadByte() != 0;

	IViewPortPanel* viewport = gViewPortInterface->FindPanelByName(panelname);

	if (!viewport)
	{
		// DevMsg("VGUIMenu: couldn't find panel '%s'.\n", panelname );
		return;
	}

	int count = msg.ReadByte();

	if (count > 0)
	{
		KeyValues* keys = new KeyValues("data");
		//Msg( "MsgFunc_VGUIMenu:\n" );

		for (int i = 0; i < count; i++)
		{
			char name[255];
			char data[255];

			msg.ReadString(name, sizeof(name));
			msg.ReadString(data, sizeof(data));
			//Msg( "  %s <- '%s'\n", name, data );

			keys->SetString(name, data);
		}

		// !KLUDGE! Whitelist of URL protocols formats for MOTD
		if (
			!V_stricmp(panelname, PANEL_INFO) // MOTD
			&& keys->GetInt("type", 0) == 2 // URL message type
			) {
			const char* pszURL = keys->GetString("msg", "");
			if (Q_strncmp(pszURL, "http://", 7) != 0 && Q_strncmp(pszURL, "https://", 8) != 0 && Q_stricmp(pszURL, "about:blank") != 0)
			{
				Warning("Blocking MOTD URL '%s'; must begin with 'http://' or 'https://' or be about:blank\n", pszURL);
				keys->deleteThis();
				return;
			}
		}

		viewport->SetData(keys);

		keys->deleteThis();
	}

	// is the server telling us to show the scoreboard (at the end of a map)?
	if (Q_stricmp(panelname, "scores") == 0)
	{
		if (hud_takesshots.GetBool() == true)
		{
			gHUD.SetScreenShotTime(gpGlobals->curtime + 1.0); // take a screenshot in 1 second
		}
	}

	// is the server trying to show an MOTD panel? Check that it's allowed right now.
	C_World* pWorld = (C_World*)GetClientWorldEntity();
	if (Q_stricmp(panelname, PANEL_INFO) == 0 && pWorld)
	{
		if (!pWorld->IsInfoPanelAllowed())
		{
			return;
		}
		else
		{
			pWorld->InfoPanelDisplayed();
		}
	}

	gViewPortInterface->ShowPanel(viewport, bShow);
}

void C_World::Init()
{
	if (!mdlcache->ActivityList_Inited()) {
		mdlcache->ActivityList_Init();
		mdlcache->EventList_Init();
		RegisterSharedActivities();
		m_bActivityInitedByMe = true;
	}

	gHUD.Init();

	m_pChatElement = (CBaseHudChat*)GET_HUDELEMENT(CHudChat);
	Assert(m_pChatElement);

	m_pWeaponSelection = (CBaseHudWeaponSelection*)GET_HUDELEMENT(CHudWeaponSelection);
	Assert(m_pWeaponSelection);

	KeyValuesAD pConditions("conditions");
	ComputeVguiResConditions(pConditions);

	// Derived ClientMode class must make sure m_Viewport is instantiated
	Assert(m_pViewport);
	m_pViewport->LoadControlSettings("scripts/HudLayout.res", NULL, NULL, pConditions);

#if defined( REPLAY_ENABLED )
	m_pReplayReminderPanel = GET_HUDELEMENT(CReplayReminderPanel);
	Assert(m_pReplayReminderPanel);
#endif

	ListenForGameEvent("player_connect");
	ListenForGameEvent("player_disconnect");
	ListenForGameEvent("player_team");
	ListenForGameEvent("server_cvar");
	ListenForGameEvent("player_changename");
	ListenForGameEvent("teamplay_broadcast_audio");
	ListenForGameEvent("achievement_earned");

#if defined( TF_CLIENT_DLL )
	ListenForGameEvent("item_found");
#endif 

#if defined( REPLAY_ENABLED )
	ListenForGameEvent("replay_startrecord");
	ListenForGameEvent("replay_endrecord");
	ListenForGameEvent("replay_replaysavailable");
	ListenForGameEvent("replay_servererror");
	ListenForGameEvent("game_newmap");
#endif

#ifndef _XBOX
	HLTVCamera()->Init();
#if defined( REPLAY_ENABLED )
	ReplayCamera()->Init();
#endif
#endif

	m_CursorNone = vgui::dc_none;

	HOOK_MESSAGE(VGUIMenu);
	HOOK_MESSAGE(Rumble);
}

void C_World::VGui_Shutdown()
{
	delete m_pViewport;
	m_pViewport = NULL;
}

void C_World::Shutdown()
{
	gHUD.Shutdown();
	if (m_bActivityInitedByMe) {
		mdlcache->ActivityList_Free();
		mdlcache->EventList_Free();
		m_bActivityInitedByMe = false;
	}
}

void C_World::LevelInit()
{
	m_flWaveHeight = 0.0f;
	// UNDONE: Make most of these things server systems or precache_registers
// =================================================
//	Activities
// =================================================

	Precache();

	m_pViewport->GetAnimationController()->StartAnimationSequence("LevelInit");

	// Tell the Chat Interface
	if (m_pChatElement)
	{
		m_pChatElement->LevelInit(gpGlobals->mapname.ToCStr());
	}

	// we have to fake this event clientside, because clients connect after that
	IGameEvent* event = gameeventmanager->CreateEvent("game_newmap");
	if (event)
	{
		event->SetString("mapname", gpGlobals->mapname.ToCStr());
		gameeventmanager->FireEventClientSide(event);
	}

	// Create a vgui context for all of the in-game vgui panels...
	if (s_hVGuiContext == DEFAULT_VGUI_CONTEXT)
	{
		s_hVGuiContext = vgui::ivgui()->CreateContext();
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP(filter, 0, true);
}

// Level init, shutdown
void C_World::LevelInitPreEntity()
{

}

void C_World::LevelInitPostEntity()
{

}

// The level is shutdown in two parts
void C_World::LevelShutdownPreEntity()
{

}

void C_World::LevelShutdownPostEntity()
{

}

void C_World::LevelShutdown()
{
	// Reset the third person camera so we don't crash
	g_ThirdPersonManager.Init();

	if (m_pChatElement)
	{
		m_pChatElement->LevelShutdown();
	}
	if (s_hVGuiContext != DEFAULT_VGUI_CONTEXT)
	{
		vgui::ivgui()->DestroyContext(s_hVGuiContext);
		s_hVGuiContext = DEFAULT_VGUI_CONTEXT;
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP(filter, 0, true);
}

void C_World::Enable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();;

	// Add our viewport to the root panel.
	if (pRoot != 0)
	{
		m_pViewport->SetParent(pRoot);
	}

	// All hud elements should be proportional
	// This sets that flag on the viewport and all child panels
	m_pViewport->SetProportional(true);

	m_pViewport->SetCursor(m_CursorNone);
	vgui::surface()->SetCursor(m_CursorNone);

	m_pViewport->SetVisible(true);
	if (m_pViewport->IsKeyBoardInputEnabled())
	{
		m_pViewport->RequestFocus();
	}

	Layout();
}


void C_World::Disable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();;

	// Remove our viewport from the root panel.
	if (pRoot != 0)
	{
		m_pViewport->SetParent((vgui::VPANEL)NULL);
	}

	m_pViewport->SetVisible(false);
}

void C_World::ReloadScheme(void)
{
	m_pViewport->ReloadScheme("resource/ClientScheme.res");
	ClearKeyValuesCache();
}

void C_World::Layout()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();
	int wide, tall;

	// Make the viewport fill the root panel.
	if (pRoot != 0)
	{
		vgui::ipanel()->GetSize(pRoot, wide, tall);

		bool changed = wide != m_nRootSize[0] || tall != m_nRootSize[1];
		m_nRootSize[0] = wide;
		m_nRootSize[1] = tall;

		m_pViewport->SetBounds(0, 0, wide, tall);
		if (changed)
		{
			ReloadScheme();
		}
	}
}

bool C_World::IsBonusChallengeTimeBased(void)
{
	return true;
}

bool C_World::IsLocalPlayer(int nEntIndex)
{
	C_BasePlayer* pLocalPlayer = (C_BasePlayer*)EntityList()->GetLocalPlayer();
	return (pLocalPlayer && pLocalPlayer == EntityList()->GetEnt(nEntIndex));
}

bool C_World::SwitchToNextBestWeapon(CBaseCombatCharacter* pPlayer, CBaseCombatWeapon* pCurrentWeapon)
{
	return false;
}

CBaseCombatWeapon* C_World::GetNextBestWeapon(CBaseCombatCharacter* pPlayer, CBaseCombatWeapon* pCurrentWeapon)
{
	return NULL;
}

bool C_World::ShouldCollide(int collisionGroup0, int collisionGroup1)
{
	if (collisionGroup0 > collisionGroup1)
	{
		// swap so that lowest is always first
		::V_swap(collisionGroup0, collisionGroup1);
	}

#ifndef HL2MP
	if ((collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_PUSHAWAY)
	{
		return false;
	}
#endif

	if (collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY)
	{
		// let debris and multiplayer objects collide
		return true;
	}

	// --------------------------------------------------------------------------
	// NOTE: All of this code assumes the collision groups have been sorted!!!!
	// NOTE: Don't change their order without rewriting this code !!!
	// --------------------------------------------------------------------------

	// Don't bother if either is in a vehicle...
	if ((collisionGroup0 == COLLISION_GROUP_IN_VEHICLE) || (collisionGroup1 == COLLISION_GROUP_IN_VEHICLE))
		return false;

	if ((collisionGroup1 == COLLISION_GROUP_DOOR_BLOCKER) && (collisionGroup0 != COLLISION_GROUP_NPC))
		return false;

	if ((collisionGroup0 == COLLISION_GROUP_PLAYER) && (collisionGroup1 == COLLISION_GROUP_PASSABLE_DOOR))
		return false;

	if (collisionGroup0 == COLLISION_GROUP_DEBRIS || collisionGroup0 == COLLISION_GROUP_DEBRIS_TRIGGER)
	{
		// put exceptions here, right now this will only collide with COLLISION_GROUP_NONE
		return false;
	}

	// Dissolving guys only collide with COLLISION_GROUP_NONE
	if ((collisionGroup0 == COLLISION_GROUP_DISSOLVING) || (collisionGroup1 == COLLISION_GROUP_DISSOLVING))
	{
		if (collisionGroup0 != COLLISION_GROUP_NONE)
			return false;
	}

	// doesn't collide with other members of this group
	// or debris, but that's handled above
	if (collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS && collisionGroup1 == COLLISION_GROUP_INTERACTIVE_DEBRIS)
		return false;

#ifndef HL2MP
	// This change was breaking HL2DM
	// Adrian: TEST! Interactive Debris doesn't collide with the player.
	if (collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS && (collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT || collisionGroup1 == COLLISION_GROUP_PLAYER))
		return false;
#endif

	if (collisionGroup0 == COLLISION_GROUP_BREAKABLE_GLASS && collisionGroup1 == COLLISION_GROUP_BREAKABLE_GLASS)
		return false;

	// interactive objects collide with everything except debris & interactive debris
	if (collisionGroup1 == COLLISION_GROUP_INTERACTIVE && collisionGroup0 != COLLISION_GROUP_NONE)
		return false;

	// Projectiles hit everything but debris, weapons, + other projectiles
	if (collisionGroup1 == COLLISION_GROUP_PROJECTILE)
	{
		if (collisionGroup0 == COLLISION_GROUP_DEBRIS ||
			collisionGroup0 == COLLISION_GROUP_WEAPON ||
			collisionGroup0 == COLLISION_GROUP_PROJECTILE)
		{
			return false;
		}
	}

	// Don't let vehicles collide with weapons
	// Don't let players collide with weapons...
	// Don't let NPCs collide with weapons
	// Weapons are triggers, too, so they should still touch because of that
	if (collisionGroup1 == COLLISION_GROUP_WEAPON)
	{
		if (collisionGroup0 == COLLISION_GROUP_VEHICLE ||
			collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == COLLISION_GROUP_NPC)
		{
			return false;
		}
	}

	// collision with vehicle clip entity??
	if (collisionGroup0 == COLLISION_GROUP_VEHICLE_CLIP || collisionGroup1 == COLLISION_GROUP_VEHICLE_CLIP)
	{
		// yes then if it's a vehicle, collide, otherwise no collision
		// vehicle sorts lower than vehicle clip, so must be in 0
		if (collisionGroup0 == COLLISION_GROUP_VEHICLE)
			return true;
		// vehicle clip against non-vehicle, no collision
		return false;
	}

	return true;
}


const CViewVectors* C_World::GetViewVectors() const
{
	return &g_DefaultViewVectors;
}


//-----------------------------------------------------------------------------
// Purpose: Returns how much damage the given ammo type should do to the victim
//			when fired by the attacker.
// Input  : pAttacker - Dude what shot the gun.
//			pVictim - Dude what done got shot.
//			nAmmoType - What been shot out.
// Output : How much hurt to put on dude what done got shot (pVictim).
//-----------------------------------------------------------------------------
float C_World::GetAmmoDamage(IHandleEntity* pAttacker, IHandleEntity* pVictim, int nAmmoType)
{
	float flDamage = 0;
	CAmmoDef* pAmmoDef = GetAmmoDef();

	if (pAttacker->IsPlayer())
	{
		flDamage = pAmmoDef->PlrDamage(nAmmoType);
	}
	else
	{
		flDamage = pAmmoDef->NPCDamage(nAmmoType);
	}

	return flDamage;
}

void C_World::DebugDrawLine(const Vector& vecAbsStart, const Vector& vecAbsEnd, int r, int g, int b, bool test, float duration)
{
	debugoverlay->AddLineOverlay(vecAbsStart + Vector(0, 0, 0.1), vecAbsEnd + Vector(0, 0, 0.1), r, g, b, test, duration);
}

//----------------------------------------------------------------------------
// Purpose: Let the client mode set some vgui conditions
//-----------------------------------------------------------------------------
void	C_World::ComputeVguiResConditions(KeyValues* pkvConditions)
{
	if (UseVR())
	{
		pkvConditions->FindKey("if_vr", true);
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
bool C_World::CreateMove(float flInputSampleTime, CUserCmd* cmd)
{
	// Let the player override the view.
	C_BasePlayer* pPlayer = (C_BasePlayer*)EntityList()->GetLocalPlayer();
	if (!pPlayer)
		return true;

	// Let the player at it
	return pPlayer->CreateMove(flInputSampleTime, cmd);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
void C_World::OverrideView(CViewSetup* pSetup)
{
	QAngle camAngles;

	// Let the player override the view.
	C_BasePlayer* pPlayer = (C_BasePlayer*)EntityList()->GetLocalPlayer();
	if (!pPlayer)
		return;

	pPlayer->OverrideView(pSetup);

	if (g_pUserInput->CAM_IsThirdPerson())
	{
		Vector cam_ofs = g_ThirdPersonManager.GetCameraOffsetAngles();
		Vector cam_ofs_distance = g_ThirdPersonManager.GetFinalCameraOffset();

		cam_ofs_distance *= g_ThirdPersonManager.GetDistanceFraction();

		camAngles[PITCH] = cam_ofs[PITCH];
		camAngles[YAW] = cam_ofs[YAW];
		camAngles[ROLL] = 0;

		Vector camForward, camRight, camUp;


		if (g_ThirdPersonManager.IsOverridingThirdPerson() == false)
		{
			engine->GetViewAngles(camAngles);
		}

		// get the forward vector
		AngleVectors(camAngles, &camForward, &camRight, &camUp);

		VectorMA(pSetup->origin, -cam_ofs_distance[0], camForward, pSetup->origin);
		VectorMA(pSetup->origin, cam_ofs_distance[1], camRight, pSetup->origin);
		VectorMA(pSetup->origin, cam_ofs_distance[2], camUp, pSetup->origin);

		// Override angles from third person camera
		VectorCopy(camAngles, pSetup->angles);
	}
	else if (g_pUserInput->CAM_IsOrthographic())
	{
		pSetup->m_bOrtho = true;
		float w, h;
		g_pUserInput->CAM_OrthographicSize(w, h);
		w *= 0.5f;
		h *= 0.5f;
		pSetup->m_OrthoLeft = -w;
		pSetup->m_OrthoTop = -h;
		pSetup->m_OrthoRight = w;
		pSetup->m_OrthoBottom = h;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_World::ShouldDrawEntity(C_BaseEntity* pEnt)
{
	return true;
}

bool C_World::ShouldDrawParticles()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Allow weapons to override mouse input (for binoculars)
//-----------------------------------------------------------------------------
void C_World::OverrideMouseInput(float* x, float* y)
{
	C_BaseCombatWeapon* pWeapon = GetActiveWeapon();
	if (pWeapon)
	{
		pWeapon->OverrideMouseInput(x, y);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_World::ShouldDrawViewModel()
{
	return true;
}

bool C_World::ShouldDrawDetailObjects()
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if VR mode should black out everything outside the HUD.
//			This is used for things like sniper scopes and full screen UI
//-----------------------------------------------------------------------------
bool C_World::ShouldBlackoutAroundHUD()
{
	return enginevgui->IsGameUIVisible();
}


//-----------------------------------------------------------------------------
// Purpose: Allows the client mode to override mouse control stuff in sourcevr
//-----------------------------------------------------------------------------
HeadtrackMovementMode_t C_World::ShouldOverrideHeadtrackControl()
{
	return HMM_NOOVERRIDE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_World::ShouldDrawCrosshair(void)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Don't draw the current view entity if we are using the fake viewmodel instead
//-----------------------------------------------------------------------------
bool C_World::ShouldDrawLocalPlayer(C_BasePlayer* pPlayer)
{
	if ((pPlayer->entindex() == render->GetViewEntity()) && !EntityList()->GetLocalPlayer()->AsHandlePlayer()->ShouldDrawLocalPlayer())
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: The mode can choose to not draw fog
//-----------------------------------------------------------------------------
bool C_World::ShouldDrawFog(void)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_World::AdjustEngineViewport(int& x, int& y, int& width, int& height)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_World::PreRender(CViewSetup* pSetup)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_World::PostRender()
{
	// Let the particle manager simulate things that haven't been simulated.
	ParticleMgr()->PostRender();
}

void C_World::PostRenderVGui()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_World::Update()
{
#if defined( REPLAY_ENABLED )
	UpdateReplayMessages();
#endif

	if (m_pViewport->IsVisible() != cl_drawhud.GetBool())
	{
		m_pViewport->SetVisible(cl_drawhud.GetBool());
	}

	UpdateRumbleEffects();

	if (cl_show_num_particle_systems.GetBool())
	{
		int nCount = 0;

		for (int i = 0; i < g_pParticleSystemMgr->GetParticleSystemCount(); i++)
		{
			const char* pParticleSystemName = g_pParticleSystemMgr->GetParticleSystemNameFromIndex(i);
			CParticleSystemDefinition* pParticleSystem = g_pParticleSystemMgr->FindParticleSystem(pParticleSystemName);
			if (!pParticleSystem)
				continue;

			for (CParticleCollection* pCurCollection = pParticleSystem->FirstCollection();
				pCurCollection != NULL;
				pCurCollection = pCurCollection->GetNextCollectionUsingSameDef())
			{
				++nCount;
			}
		}

		engine->Con_NPrintf(0, "# Active particle systems: %i", nCount);
	}
}

//-----------------------------------------------------------------------------
// This processes all input before SV Move messages are sent
//-----------------------------------------------------------------------------

void C_World::ProcessInput(bool bActive)
{
	gHUD.ProcessInput(bActive);
}

//-----------------------------------------------------------------------------
// Purpose: We've received a keypress from the engine. Return 1 if the engine is allowed to handle it.
//-----------------------------------------------------------------------------
int	C_World::KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
	if (engine->Con_IsVisible())
		return 1;

	// If we're voting...
#ifdef VOTING_ENABLED
	CHudVote* pHudVote = GET_HUDELEMENT(CHudVote);
	if (pHudVote && pHudVote->IsVisible())
	{
		if (!pHudVote->KeyInput(down, keynum, pszCurrentBinding))
		{
			return 0;
		}
	}
#endif

	C_BasePlayer* pPlayer = (C_BasePlayer*)EntityList()->GetLocalPlayer();

	// if ingame spectator mode, let spectator input intercept key event here
	if (pPlayer &&
		(pPlayer->GetObserverMode() > OBS_MODE_DEATHCAM) &&
		!HandleSpectatorKeyInput(down, keynum, pszCurrentBinding))
	{
		return 0;
	}

	// Let game-specific hud elements get a crack at the key input
	if (!HudElementKeyInput(down, keynum, pszCurrentBinding))
	{
		return 0;
	}

	C_BaseCombatWeapon* pWeapon = GetActiveWeapon();
	if (pWeapon)
	{
		return pWeapon->KeyInput(down, keynum, pszCurrentBinding);
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: See if spectator input occurred. Return 0 if the key is swallowed.
//-----------------------------------------------------------------------------
int C_World::HandleSpectatorKeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
	// we are in spectator mode, open spectator menu
	if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+duck") == 0)
	{
		m_pViewport->ShowPanel(PANEL_SPECMENU, true);
		return 0; // we handled it, don't handle twice or send to server
	}
	else if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+attack") == 0)
	{
		engine->ClientCmd("spec_next");
		return 0;
	}
	else if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+attack2") == 0)
	{
		engine->ClientCmd("spec_prev");
		return 0;
	}
	else if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+jump") == 0)
	{
		engine->ClientCmd("spec_mode");
		return 0;
	}
	else if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+strafe") == 0)
	{
		HLTVCamera()->SetAutoDirector(true);
#if defined( REPLAY_ENABLED )
		ReplayCamera()->SetAutoDirector(true);
#endif
		return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: See if hud elements want key input. Return 0 if the key is swallowed
//-----------------------------------------------------------------------------
int C_World::HudElementKeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
	if (m_pWeaponSelection)
	{
		if (!m_pWeaponSelection->KeyInput(down, keynum, pszCurrentBinding))
		{
			return 0;
		}
	}

#if defined( REPLAY_ENABLED )
	if (m_pReplayReminderPanel)
	{
		if (m_pReplayReminderPanel->HudElementKeyInput(down, keynum, pszCurrentBinding))
		{
			return 0;
		}
	}
#endif

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_World::DoPostScreenSpaceEffects(const CViewSetup* pSetup)
{
#if defined( REPLAY_ENABLED )
	if (engine->IsPlayingDemo())
	{
		if (!replay_rendersetting_renderglow.GetBool())
			return false;
	}
#endif 
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel* C_World::GetMessagePanel()
{
	if (m_pChatElement && m_pChatElement->GetInputPanel() && m_pChatElement->GetInputPanel()->IsVisible())
		return m_pChatElement->GetInputPanel();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The player has started to type a message
//-----------------------------------------------------------------------------
void C_World::StartMessageMode(int iMessageModeType)
{
	// Can only show chat UI in multiplayer!!!
	if (gpGlobals->maxClients == 1)
	{
		return;
	}
	if (m_pChatElement)
	{
		m_pChatElement->StartMessageMode(iMessageModeType);
	}
}

float C_World::GetViewModelFOV(void)
{
	return v_viewmodel_fov.GetFloat();
}

vgui::Panel* C_World::GetViewport() 
{ 
	if (!m_pViewport) {
		Error("m_pViewport not inited!");
	}
	return m_pViewport; 
}

class CHudChat;

bool PlayerNameNotSetYet(const char* pszName)
{
	if (pszName && pszName[0])
	{
		// Don't show "unconnected" if we haven't got the players name yet
		if (Q_strnicmp(pszName, "unconnected", 11) == 0)
			return true;
		if (Q_strnicmp(pszName, "NULLNAME", 11) == 0)
			return true;
	}

	return false;
}

void C_World::FireGameEvent(IGameEvent* event)
{
	CBaseHudChat* hudChat = (CBaseHudChat*)GET_HUDELEMENT(CHudChat);

	const char* eventname = event->GetName();

	if (Q_strcmp("player_connect", eventname) == 0)
	{
		if (!hudChat)
			return;
		if (PlayerNameNotSetYet(event->GetString("name")))
			return;

		if (!IsInCommentaryMode())
		{
			wchar_t wszLocalized[100];
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("name"), wszPlayerName, sizeof(wszPlayerName));
			g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_joined_game"), 1, wszPlayerName);

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

			hudChat->Printf(CHAT_FILTER_JOINLEAVE, "%s", szLocalized);
		}
	}
	else if (Q_strcmp("player_disconnect", eventname) == 0)
	{
		C_BasePlayer* pPlayer = USERID2PLAYER(event->GetInt("userid"));

		if (!hudChat || !pPlayer)
			return;
		if (PlayerNameNotSetYet(event->GetString("name")))
			return;

		if (!IsInCommentaryMode())
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName));

			wchar_t wszReason[64];
			const char* pszReason = event->GetString("reason");
			if (pszReason && (pszReason[0] == '#') && g_pVGuiLocalize->Find(pszReason))
			{
				V_wcsncpy(wszReason, g_pVGuiLocalize->Find(pszReason), sizeof(wszReason));
			}
			else
			{
				g_pVGuiLocalize->ConvertANSIToUnicode(pszReason, wszReason, sizeof(wszReason));
			}

			wchar_t wszLocalized[100];
			if (IsPC())
			{
				g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_left_game"), 2, wszPlayerName, wszReason);
			}
			else
			{
				g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_left_game"), 1, wszPlayerName);
			}

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

			hudChat->Printf(CHAT_FILTER_JOINLEAVE, "%s", szLocalized);
		}
	}
	else if (Q_strcmp("player_team", eventname) == 0)
	{
		C_BasePlayer* pPlayer = USERID2PLAYER(event->GetInt("userid"));
		if (!hudChat)
			return;

		bool bDisconnected = event->GetBool("disconnect");

		if (bDisconnected)
			return;

		int team = event->GetInt("team");
		bool bAutoTeamed = event->GetInt("autoteam", false);
		bool bSilent = event->GetInt("silent", false);

		const char* pszName = event->GetString("name");
		if (PlayerNameNotSetYet(pszName))
			return;

		if (!bSilent)
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(pszName, wszPlayerName, sizeof(wszPlayerName));

			wchar_t wszTeam[64];
			C_Team* pTeam = GetGlobalTeam(team);
			if (pTeam)
			{
				g_pVGuiLocalize->ConvertANSIToUnicode(pTeam->Get_Name(), wszTeam, sizeof(wszTeam));
			}
			else
			{
				_snwprintf(wszTeam, sizeof(wszTeam) / sizeof(wchar_t), L"%d", team);
			}

			if (!IsInCommentaryMode())
			{
				wchar_t wszLocalized[100];
				if (bAutoTeamed)
				{
					g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_joined_autoteam"), 2, wszPlayerName, wszTeam);
				}
				else
				{
					g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_joined_team"), 2, wszPlayerName, wszTeam);
				}

				char szLocalized[100];
				g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

				hudChat->Printf(CHAT_FILTER_TEAMCHANGE, "%s", szLocalized);
			}
		}

		if (pPlayer && pPlayer->IsLocalPlayer())
		{
			// that's me
			pPlayer->TeamChange(team);
		}
	}
	else if (Q_strcmp("player_changename", eventname) == 0)
	{
		if (!hudChat)
			return;

		const char* pszOldName = event->GetString("oldname");
		if (PlayerNameNotSetYet(pszOldName))
			return;

		wchar_t wszOldName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode(pszOldName, wszOldName, sizeof(wszOldName));

		wchar_t wszNewName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("newname"), wszNewName, sizeof(wszNewName));

		wchar_t wszLocalized[100];
		g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_changed_name"), 2, wszOldName, wszNewName);

		char szLocalized[100];
		g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

		hudChat->Printf(CHAT_FILTER_NAMECHANGE, "%s", szLocalized);
	}
	else if (Q_strcmp("teamplay_broadcast_audio", eventname) == 0)
	{
		int team = event->GetInt("team");

		bool bValidTeam = false;

		if ((GetLocalTeam() && GetLocalTeam()->GetTeamNumber() == team))
		{
			bValidTeam = true;
		}

		//If we're in the spectator team then we should be getting whatever messages the person I'm spectating gets.
		if (bValidTeam == false)
		{
			CBasePlayer* pSpectatorTarget = ToBasePlayer(EntityList()->GetPlayerByIndex(GetSpectatorTarget()));

			if (pSpectatorTarget && (GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE))
			{
				if (pSpectatorTarget->GetTeamNumber() == team)
				{
					bValidTeam = true;
				}
			}
		}

		if (team == 0 && GetLocalTeam())
			bValidTeam = false;

		if (team == 255)
			bValidTeam = true;

		if (bValidTeam == true)
		{
			EmitSound_t et;
			et.m_pSoundName = event->GetString("sound");
			et.m_nFlags = event->GetInt("additional_flags");

			CLocalPlayerFilter filter;
			g_pSoundEmitterSystem->EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, et);//C_BaseEntity::
		}
	}
	else if (Q_strcmp("server_cvar", eventname) == 0)
	{
		if (!IsInCommentaryMode())
		{
			wchar_t wszCvarName[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("cvarname"), wszCvarName, sizeof(wszCvarName));

			wchar_t wszCvarValue[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("cvarvalue"), wszCvarValue, sizeof(wszCvarValue));

			wchar_t wszLocalized[256];
			g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_server_cvar_changed"), 2, wszCvarName, wszCvarValue);

			char szLocalized[256];
			g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

			hudChat->Printf(CHAT_FILTER_SERVERMSG, "%s", szLocalized);
		}
	}
	else if (Q_strcmp("achievement_earned", eventname) == 0)
	{
		int iPlayerIndex = event->GetInt("player");
		C_BasePlayer* pPlayer = ToBasePlayer(EntityList()->GetPlayerByIndex(iPlayerIndex));
		int iAchievement = event->GetInt("achievement");

		if (!hudChat || !pPlayer)
			return;

		if (!IsInCommentaryMode())
		{
			CAchievementMgr* pAchievementMgr = dynamic_cast<CAchievementMgr*>(engine->GetAchievementMgr());
			if (!pAchievementMgr)
				return;

			IAchievement* pAchievement = pAchievementMgr->GetAchievementByID(iAchievement);
			if (pAchievement)
			{
				if (!pPlayer->IsDormant() && pPlayer->ShouldAnnounceAchievement())
				{
					pPlayer->SetNextAchievementAnnounceTime(gpGlobals->curtime + ACHIEVEMENT_ANNOUNCEMENT_MIN_TIME);

					// no particle effect if the local player is the one with the achievement or the player is dead
					if (!pPlayer->IsLocalPlayer() && pPlayer->IsAlive())
					{
						//tagES using the "head" attachment won't work for CS and DoD
						pPlayer->ParticleProp()->Create("achieved", PATTACH_POINT_FOLLOW, "head");
					}

					pPlayer->OnAchievementAchieved(iAchievement);
				}

				if (g_PR)
				{
					wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConvertANSIToUnicode(g_PR->GetPlayerName(iPlayerIndex), wszPlayerName, sizeof(wszPlayerName));

					const wchar_t* pchLocalizedAchievement = ACHIEVEMENT_LOCALIZED_NAME_FROM_STR(pAchievement->GetName());
					if (pchLocalizedAchievement)
					{
						wchar_t wszLocalizedString[128];
						g_pVGuiLocalize->ConstructString(wszLocalizedString, sizeof(wszLocalizedString), g_pVGuiLocalize->Find("#Achievement_Earned"), 2, wszPlayerName, pchLocalizedAchievement);

						char szLocalized[128];
						g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalizedString, szLocalized, sizeof(szLocalized));

						hudChat->ChatPrintf(iPlayerIndex, CHAT_FILTER_SERVERMSG, "%s", szLocalized);
					}
				}
			}
		}
	}
#if defined( TF_CLIENT_DLL )
	else if (Q_strcmp("item_found", eventname) == 0)
	{
		int iPlayerIndex = event->GetInt("player");
		entityquality_t iItemQuality = event->GetInt("quality");
		int iMethod = event->GetInt("method");
		int iItemDef = event->GetInt("itemdef");
		C_BasePlayer* pPlayer = EntityList()->GetPlayerByIndex(iPlayerIndex);
		const GameItemDefinition_t* pItemDefinition = dynamic_cast<GameItemDefinition_t*>(GetItemSchema()->GetItemDefinition(iItemDef));

		if (!pPlayer || !pItemDefinition)
			return;

		if (g_PR)
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(g_PR->GetPlayerName(iPlayerIndex), wszPlayerName, sizeof(wszPlayerName));

			if (iMethod < 0 || iMethod >= ARRAYSIZE(g_pszItemFoundMethodStrings))
			{
				iMethod = 0;
			}

			const char* pszLocString = g_pszItemFoundMethodStrings[iMethod];
			if (pszLocString)
			{
				wchar_t wszItemFound[256];
				_snwprintf(wszItemFound, ARRAYSIZE(wszItemFound), L"%ls", g_pVGuiLocalize->Find(pszLocString));

				wchar_t* colorMarker = wcsstr(wszItemFound, L"::");
				if (colorMarker)
				{
					const char* pszQualityColorString = EconQuality_GetColorString((EEconItemQuality)iItemQuality);
					if (pszQualityColorString)
					{
						hudChat->SetCustomColor(pszQualityColorString);
						*(colorMarker + 1) = COLOR_CUSTOM;
					}
				}

				// TODO: Update the localization strings to only have two format parameters since that's all we need.
				wchar_t wszLocalizedString[256];
				g_pVGuiLocalize->ConstructString(wszLocalizedString, sizeof(wszLocalizedString), wszItemFound, 3, wszPlayerName, CEconItemLocalizedFullNameGenerator(GLocalizationProvider(), pItemDefinition, iItemQuality).GetFullName(), L"");

				char szLocalized[256];
				g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalizedString, szLocalized, sizeof(szLocalized));

				hudChat->ChatPrintf(iPlayerIndex, CHAT_FILTER_SERVERMSG, "%s", szLocalized);
			}
		}
	}
#endif
#if defined( REPLAY_ENABLED )
	else if (!V_strcmp("replay_servererror", eventname))
	{
		DisplayReplayMessage(event->GetString("error", "#Replay_DefaultServerError"), replay_msgduration_error.GetFloat(), true, NULL, false);
	}
	else if (!V_strcmp("replay_startrecord", eventname))
	{
		m_flReplayStartRecordTime = gpGlobals->curtime;
	}
	else if (!V_strcmp("replay_endrecord", eventname))
	{
		m_flReplayStopRecordTime = gpGlobals->curtime;
	}
	else if (!V_strcmp("replay_replaysavailable", eventname))
	{
		DisplayReplayMessage("#Replay_ReplaysAvailable", replay_msgduration_replaysavailable.GetFloat(), false, NULL, false);
	}

	else if (!V_strcmp("game_newmap", eventname))
	{
		// Make sure the instance count is reset to 0.  Sometimes the count stay in sync and we get replay messages displaying lower than they should.
		CReplayMessagePanel::RemoveAll();
	}
#endif

	else
	{
		DevMsg(2, "Unhandled GameEvent in ClientModeShared::FireGameEvent - %s\n", event->GetName());
	}
}

void C_World::UpdateReplayMessages()
{
#if defined( REPLAY_ENABLED )
	// Received a replay_startrecord event?
	if (m_flReplayStartRecordTime != 0.0f)
	{
		DisplayReplayMessage("#Replay_StartRecord", replay_msgduration_startrecord.GetFloat(), true, "replay\\startrecord.mp3", false);

		m_flReplayStartRecordTime = 0.0f;
		m_flReplayStopRecordTime = 0.0f;
	}

	// Received a replay_endrecord event?
	if (m_flReplayStopRecordTime != 0.0f)
	{
		DisplayReplayMessage("#Replay_EndRecord", replay_msgduration_stoprecord.GetFloat(), true, "replay\\stoprecord.wav", false);

		// Hide the replay reminder
		if (m_pReplayReminderPanel)
		{
			m_pReplayReminderPanel->Hide();
		}

		m_flReplayStopRecordTime = 0.0f;
	}

	if (!engine->IsConnected())
	{
		ClearReplayMessageList();
	}
#endif
}

void C_World::ClearReplayMessageList()
{
#if defined( REPLAY_ENABLED )
	CReplayMessagePanel::RemoveAll();
#endif
}

void C_World::DisplayReplayMessage(const char* pLocalizeName, float flDuration, bool bUrgent,
	const char* pSound, bool bDlg)
{
#if defined( REPLAY_ENABLED )
	// Don't display during replay playback, and don't allow more than 4 at a time
	const bool bInReplay = g_pEngineClientReplay->IsPlayingReplayDemo();
	if (bInReplay || (!bDlg && CReplayMessagePanel::InstanceCount() >= 4))
		return;

	// Use default duration?
	if (flDuration == -1.0f)
	{
		flDuration = replay_msgduration_misc.GetFloat();
	}

	// Display a replay message
	if (bDlg)
	{
		if (engine->IsInGame())
		{
			Panel* pPanel = new CReplayMessageDlg(pLocalizeName);
			pPanel->SetVisible(true);
			pPanel->MakePopup();
			pPanel->MoveToFront();
			pPanel->SetKeyBoardInputEnabled(true);
			pPanel->SetMouseInputEnabled(true);
#if defined( TF_CLIENT_DLL )
			TFModalStack()->PushModal(pPanel);
#endif
		}
		else
		{
			ShowMessageBox("#Replay_GenericMsgTitle", pLocalizeName, "#GameUI_OK");
		}
	}
	else
	{
		CReplayMessagePanel* pMsgPanel = new CReplayMessagePanel(pLocalizeName, flDuration, bUrgent);
		pMsgPanel->Show();
	}

	// Play a sound if appropriate
	if (pSound)
	{
		surface()->PlaySound(pSound);
	}
#endif
}

void C_World::DisplayReplayReminder()
{
#if defined( REPLAY_ENABLED )
	if (m_pReplayReminderPanel && g_pReplay->IsRecording())
	{
		// Only display the panel if we haven't already requested a replay for the given life
		CReplay* pCurLifeReplay = static_cast<CReplay*>(g_pClientReplayContext->GetReplayManager()->GetReplayForCurrentLife());
		if (pCurLifeReplay && !pCurLifeReplay->m_bRequestedByUser && !pCurLifeReplay->m_bSaved)
		{
			m_pReplayReminderPanel->Show();
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// In-game VGUI context 
//-----------------------------------------------------------------------------
void C_World::ActivateInGameVGuiContext(vgui::Panel* pPanel)
{
	vgui::ivgui()->AssociatePanelWithContext(s_hVGuiContext, pPanel->GetVPanel());
	vgui::ivgui()->ActivateContext(s_hVGuiContext);
}

void C_World::DeactivateInGameVGuiContext()
{
	vgui::ivgui()->ActivateContext(DEFAULT_VGUI_CONTEXT);
}

IRecipientFilter* C_World::CreatePASAttenuationFilter(IClientEntity* entity, float attenuation)
{
	return new CPASAttenuationFilter(entity, attenuation);
}

IRecipientFilter* C_World::CreatePASAttenuationFilter(IClientEntity* entity, const char* lookupSound)
{
	return new CPASAttenuationFilter(entity, lookupSound);
}

IRecipientFilter* C_World::CreatePASAttenuationFilter(const Vector& origin, float attenuation)
{
	return new CPASAttenuationFilter(origin, attenuation);
}

#ifdef VOICE_VOX_ENABLE
void VoxCallback(IConVar* var, const char* oldString, float oldFloat)
{
	if (engine && engine->IsConnected())
	{
		ConVarRef voice_vox(var->GetName());
		if (voice_vox.GetBool() && voice_modenable.GetBool())
		{
			engine->ClientCmd_Unrestricted("voicerecord_toggle on\n");
		}
		else
		{
			engine->ClientCmd_Unrestricted("voicerecord_toggle off\n");
		}
	}
}
ConVar voice_vox("voice_vox", "0", FCVAR_ARCHIVE, "Voice chat uses a vox-style always on", true, 0, true, 1, VoxCallback);

// --------------------------------------------------------------------------------- //
// CVoxManager.
// --------------------------------------------------------------------------------- //
class CVoxManager : public CAutoGameSystem
{
public:
	CVoxManager() : CAutoGameSystem("VoxManager")
	{
	}

	virtual void LevelInitPostEntity(void)
	{
		if (voice_vox.GetBool() && voice_modenable.GetBool())
		{
			engine->ClientCmd_Unrestricted("voicerecord_toggle on\n");
		}
	}

	virtual void LevelShutdownPreEntity(void)
	{
		if (voice_vox.GetBool())
		{
			engine->ClientCmd_Unrestricted("voicerecord_toggle off\n");
		}
	}
};

static CVoxManager s_VoxManager;
// --------------------------------------------------------------------------------- //
#endif // VOICE_VOX_ENABLE

CON_COMMAND(hud_reloadscheme, "Reloads hud layout and animation scripts.")
{
	C_World* mode = (C_World*)GetClientWorldEntity();
	if (!mode)
		return;

	mode->ReloadScheme();
}

CON_COMMAND(messagemode, "Opens chat dialog")
{
	C_World* mode = (C_World*)GetClientWorldEntity();
	mode->StartMessageMode(MM_SAY);
}

CON_COMMAND(messagemode2, "Opens chat dialog")
{
	C_World* mode = (C_World*)GetClientWorldEntity();
	mode->StartMessageMode(MM_SAY_TEAM);
}

#ifdef _DEBUG
CON_COMMAND_F(crash, "Crash the client. Optional parameter -- type of crash:\n 0: read from NULL\n 1: write to NULL\n 2: DmCrashDump() (xbox360 only)", FCVAR_CHEAT)
{
	int crashtype = 0;
	int dummy;
	if (args.ArgC() > 1)
	{
		crashtype = Q_atoi(args[1]);
	}
	switch (crashtype)
	{
	case 0:
		dummy = *((int*)NULL);
		Msg("Crashed! %d\n", dummy); // keeps dummy from optimizing out
		break;
	case 1:
		*((int*)NULL) = 42;
		break;
#if defined( _X360 )
	case 2:
		XBX_CrashDump(false);
		break;
#endif
	default:
		Msg("Unknown variety of crash. You have now failed to crash. I hope you're happy.\n");
		break;
	}
}
#endif // _DEBUG