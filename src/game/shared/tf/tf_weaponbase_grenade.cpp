//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF Base Grenade.
//
//=============================================================================//
#include "cbase.h"
#include "tf_weaponbase.h"
#include "tf_weaponbase_grenade.h"
#include "tf_gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"	
#include "tf_weaponbase_grenadeproj.h"
#include "eventlist.h"
#include "pf_cvars.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "soundent.h"
#include "tf_player.h"
#include "items.h"
#endif

#define GRENADE_TIMER	1.5f			// seconds
#define GRENADE_THROW_SOUND		"Weapon_Grenade_Normal.Single"

//=============================================================================
//
// TF Grenade tables.
//

#if defined (CLIENT_DLL)
ConVar pf_grenade_press_throw( "pf_grenade_press_throw", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_USERINFO, "Causes grenades to require a second button press to throw." );
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponBaseGrenade, DT_TFWeaponBaseGrenade )

BEGIN_NETWORK_TABLE( CTFWeaponBaseGrenade, DT_TFWeaponBaseGrenade )
// Client specific.
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bPrimed ) ),
	RecvPropFloat( RECVINFO( m_flThrowTime ) ),
	RecvPropBool( RECVINFO( m_bThrow ) ),
	RecvPropFloat( RECVINFO( m_flPrimeStartTime ) ),
// Server specific.
#else
	SendPropBool( SENDINFO( m_bPrimed ) ),
	SendPropTime( SENDINFO( m_flThrowTime ) ),
	SendPropBool( SENDINFO( m_bThrow ) ),
	SendPropTime( SENDINFO( m_flPrimeStartTime ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponBaseGrenade )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bPrimed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flThrowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bThrow, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flPrimeStartTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weaponbase_grenade, CTFWeaponBaseGrenade );

//=============================================================================
//
// TF Grenade functions.
//

CTFWeaponBaseGrenade::CTFWeaponBaseGrenade()
{
}

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::Spawn( void )
{
	BaseClass::Spawn();

	SetViewModelIndex( 1 );
#ifdef GAME_DLL
	RegisterThinkContext("BeepThink");
	SetContextThink(&CTFWeaponBaseGrenade::BeepThink, gpGlobals->curtime, "BeepThink");
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::Precache()
{
	PrecacheScriptSound("Weapon_Grenade.Beep");
	PrecacheScriptSound("Weapon_Grenade.FinalBeep");
	PrecacheScriptSound( GRENADE_THROW_SOUND );
	PrecacheParticleSystem("stickybomb_pulse_red");
	PrecacheParticleSystem("stickybomb_pulse_blue");
	PrecacheParticleSystem("nadepulse_red");
	PrecacheParticleSystem("nadepulse_final_blue");

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseGrenade::IsPrimed( void )
{
	return m_bPrimed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseGrenade::Deploy(void)
{
	CTFPlayer* pPlayer = ToTFPlayer( GetPlayerOwner() );
	// kill me
	if ( pPlayer->m_Shared.InCond( TF_COND_STEALTHED ) ||
		pPlayer->m_Shared.InCond( TF_COND_STEALTHED_BLINK ) ||
		pPlayer->m_Shared.InCond( TF_COND_SMOKE_BOMB ) ||
		pPlayer->m_Shared.InCond( TF_COND_BUILDING_DETPACK ) ||
		pPlayer->m_Shared.GetPercentInvisible() > 0 )
		return false;

	bool bHolsteringEnabled = pf_grenade_holstering.GetBool();
	if (bHolsteringEnabled)
	{
		if (GetTFPlayerOwner() && GetTFPlayerOwner()->GetViewModel( 1 ))
			GetTFPlayerOwner()->GetViewModel( 1 )->RemoveEffects( EF_NODRAW );
	}

	if (!bHolsteringEnabled || BaseClass::Deploy())
	{
		Prime();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::WeaponReset()
{
	m_bPrimed = false;
	m_bThrow = false;
	BaseClass::WeaponReset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::Prime() 
{
	if (!m_bPrimed)
	{
		CTFWeaponInfo weaponInfo = GetTFWpnData();

		CTFPlayer* pPlayer = ToTFPlayer(GetPlayerOwner());
		if (pPlayer)
		{
			if ( pf_grenade_holstering.GetBool() )
			{
				if ( pPlayer->GetActiveTFWeapon() )
				{
					if ( ShouldLowerMainWeapon() && !pPlayer->GetActiveTFWeapon()->IsLowered() )
					{
						if ( !pPlayer->GetActiveTFWeapon()->Lower() )
							return;
					}
				}
			}
		}
		
		m_flThrowTime = gpGlobals->curtime + weaponInfo.m_flPrimerTime;
		m_flPrimeStartTime = gpGlobals->curtime;
		m_bPrimed = true;
#ifdef GAME_DLL
		SetNextThink(gpGlobals->curtime + 0.2, "BeepThink"); // Start thinking now
		if (GetWeaponID() != TF_WEAPON_GRENADE_SMOKE_BOMB)
		{
			// Get the player owning the weapon.
			if (!pPlayer)
				return;

			pPlayer->RemoveInvisibility();
		}
		m_nTeamNum = pPlayer->GetTeamNumber();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::Throw()
{
	if (!m_bPrimed)
		return;

	m_bPrimed = false;
	m_bThrow = false;

	// Get the owning player.
	CTFPlayer* pPlayer = ToTFPlayer(GetOwner());
	if (!pPlayer)
		return;

	pPlayer->EmitSound(GRENADE_THROW_SOUND);

#ifdef GAME_DLL
	// Calculate the time remaining.
	float flTime = m_flThrowTime - gpGlobals->curtime;
	bool bExplodingInHand = (flTime <= 0.0f);

	// Players who are dying may not have their death state set, so check that too
	bool bExplodingOnDeath = (!pPlayer->IsAlive() || pPlayer->StateGet() == TF_STATE_DYING);

	Vector vecSrc, vecThrow;

	// Don't throw if we've changed team
	if ( bExplodingOnDeath && m_nTeamNum != pPlayer->GetTeamNumber() )
		return;

	if (bExplodingInHand || bExplodingOnDeath)
	{
		vecThrow = vec3_origin;
		vecSrc = pPlayer->GetAbsOrigin() + (pPlayer->GetClassEyeHeight() / 3);
	}
	else
	{
		vecSrc = pPlayer->Weapon_ShootPosition();
		// Determine the throw angle and velocity.
		QAngle angThrow = pPlayer->LocalEyeAngles() + pPlayer->ConcAngles();
		if (angThrow.x < 90.0f)
		{
			angThrow.x = -10.0f + angThrow.x * ((90.0f + 10.0f) / 90.0f);
		}
		else
		{
			angThrow.x = 360.0f - angThrow.x;
			angThrow.x = -10.0f + angThrow.x * -((90.0f - 10.0f) / 90.0f);
		}

		// Adjust for the lowering of the spawn point
		angThrow.x -= 10;

		float flVelocity = (90.0f - angThrow.x) * 8.0f;
		if (flVelocity > 950.0f)
		{
			flVelocity = 950.0f;
		}

		Vector vForward, vRight, vUp;
		AngleVectors(angThrow, &vForward, &vRight, &vUp);

		Vector endPos = vecSrc + vForward * 400 + vRight * -100;

		trace_t tr;

		CTraceFilterIgnoreTeammates filter(pPlayer, COLLISION_GROUP_NONE, pPlayer->GetTeamNumber());
		UTIL_TraceLine(vecSrc, endPos, MASK_SOLID, &filter, &tr);

		// Find angles that will get us to our desired end point
		// Only use the trace end if it wasn't too close, which results
		// in visually bizarre forward angles
		if (tr.fraction > 0.1)
		{
			vecSrc += vForward * 16.0f + vRight * -8.0f + vUp * -6.0f;
		}
		else
		{
			vecSrc += vForward * -24.0f + vRight * -8.0f + vUp * -6.0f;
		}

		// Throw from the player's left hand position.


		vecThrow = vForward * flVelocity;
	}

#if 0
	// Debug!!!
	char str[256];
	Q_snprintf(str, sizeof(str), "GrenadeTime = %f\n", flTime);
	NDebugOverlay::ScreenText(0.5f, 0.38f, str, 255, 255, 255, 255, 2.0f);
#endif

	QAngle vecAngles = RandomAngle(0, 360);
	CTFWeaponBaseGrenadeProj* pGrenade = EmitGrenade(vecSrc, vecAngles, vecThrow, AngularImpulse(600, random->RandomInt(-1200, 1200), 0), pPlayer, bExplodingInHand ? 0.0 : flTime);
	// Create the projectile and send in the time remaining.
	if (pGrenade)
	{
		if (!bExplodingInHand)
		{
			pGrenade->SetContextThink(&CTFWeaponBaseGrenadeProj::BeepThink, GetNextThink("BeepThink"), "BeepThink");
		}
		else
		{
			if (pPlayer->GetViewModel( 1 ))
				pPlayer->GetViewModel( 1 )->AddEffects( EF_NODRAW );
			// We're holding onto an exploding grenade
			pGrenade->ExplodeInHand(GetTFPlayerOwner());
		}
	}

	if ( pf_grenade_holstering.GetBool() )
	{
		if (pPlayer->GetActiveTFWeapon())
		{
			if (pPlayer->GetActiveTFWeapon()->IsLowered())
				pPlayer->GetActiveTFWeapon()->Ready();
		}
	}

	if (!pf_grenades_infinite.GetBool())
		pPlayer->RemoveAmmo( 1, GetPrimaryAmmoType() );
	

	// The grenade is about to be destroyed, so it won't be able to holster.
	// Handle the viewmodel hiding for it.
	if ( bExplodingInHand || bExplodingOnDeath )
	{
		SendWeaponAnim( ACT_VM_IDLE );
		CBaseViewModel *vm = pPlayer->GetViewModel( 1 );
		if (vm)
		{
			vm->AddEffects( EF_NODRAW );
		}
	}
#endif
	// Reset the throw time
	m_flThrowTime = 0.0f;

	// Expected holster time
	if(pf_grenade_holstering.GetBool() )
		pPlayer->m_Shared.m_flNextThrowTime = gpGlobals->curtime + 0.67f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseGrenade::ShouldDetonate( void )
{
	return ( m_flThrowTime != 0.0f ) && ( m_flThrowTime < gpGlobals->curtime );
}

void CTFWeaponBaseGrenade::CheckThrow( CTFPlayer* pPlayer )
{
	if ( CanThrow() )
	{
		if ( !m_bThrow )
		{
			if ( pPlayer->GetGrenadePressThrow() )
			{
				if ( ( pPlayer->m_afButtonPressed & IN_GRENADE1 || pPlayer->m_afButtonPressed & IN_GRENADE2 ) )
					m_bThrow = true;
			}
			else
			{
				if ( !( pPlayer->m_nButtons & IN_GRENADE1 || pPlayer->m_nButtons & IN_GRENADE2 ) )
					m_bThrow = true;
			}
		}
		if ( m_bThrow )
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_GRENADE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::ItemPostFrame()
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( m_bPrimed )
	{
		// Is our timer up? If so, blow up immediately
		if ( ShouldDetonate() )
		{
			Throw();
			return;
		}

		CheckThrow( pPlayer );
	}

	if ( CanThrow() && m_bThrow )
	{
		if ( GetActivity() != ACT_VM_PRIMARYATTACK )
		{
			// Start the throw animation
			if ( !SendWeaponAnim( ACT_VM_PRIMARYATTACK ) )
			{
				Throw();
			}
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// The Throw call here exists solely to catch the lone case of thirdperson where the 
			// viewmodel isn't being drawn, and hence the anim event doesn't trigger and force a throw.
			// In all other cases, it'll do nothing because the grenade has already been thrown.
			Throw();
		}
		return;
	}

	if ( !m_bPrimed && !m_bThrow )
	{
		// We've been thrown. Go away.
		if (HasWeaponIdleTimeElapsed())
		{
			Holster();
			AddEffects(EF_NODRAW);
		}
		// Once we've finished being holstered, we'll be hidden. When that happens,
		// tell our player that we're all done with the grenade throw.
		if ( IsEffectActive(EF_NODRAW) )
		{
			pPlayer->FinishThrowGrenade();
			return;
		}

	}

	// Go straight to idle anim when deploy is done
	if ( m_flTimeWeaponIdle <= gpGlobals->curtime )
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::ItemBusyFrame()
{
	if (!pf_grenade_holstering.GetBool())
	{
		CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
		if (!pPlayer)
			return;

		if (m_bPrimed)
		{
			// Is our timer up? If so, blow up immediately
			if (ShouldDetonate())
			{
				Throw();
				return;
			}

			CheckThrow( pPlayer );
		}
	}
}

bool CTFWeaponBaseGrenade::ShouldLowerMainWeapon( void )
{
	return GetTFWpnData().m_bLowerWeapon;
}

#ifdef GAME_DLL
void CTFWeaponBaseGrenade::BeepThink(void)
{
	if (IsPrimed())
	{
		if ( m_flThrowTime - 0.8 > gpGlobals->curtime )
		{
			
			if( m_flThrowTime - 1.8f <= gpGlobals->curtime )
				SetNextThink( gpGlobals->curtime + 0.2f, "BeepThink" );
			else
			{
				if ( GetTFPlayerOwner() )
				{
					CPASAttenuationFilter filter( GetAbsOrigin() );
					EmitSound( filter, entindex(), "Weapon_Grenade.Beep" );
				}
				SetNextThink( gpGlobals->curtime + 0.8f, "BeepThink" );
			}
		}
		else
		{
			CPASAttenuationFilter filter( GetAbsOrigin() );
			EmitSound( filter, entindex(), "Weapon_Grenade.FinalBeep" );
		}
	}
}
#endif

//=============================================================================
//
// Client specific functions.
//
#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseGrenade::ShouldDraw( void )
{
	if ( !BaseClass::ShouldDraw() )
	{
		// Grenades need to be visible whenever they're being primed & thrown
		if ( !m_bPrimed )
			return false;

		// Don't draw primed grenades for local player in first person players
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer() )
			return false;
	}

	return true;
}

//=============================================================================
//
// Server specific functions.
//
#else

BEGIN_DATADESC( CTFWeaponBaseGrenade )
#ifdef CLIENT_DLL
	DEFINE_THINKFUNC(BeepThink),
#endif
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseGrenade::HandleAnimEvent( animevent_t *pEvent )
{
	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) )
	{
		if ( pEvent->event == AE_WPN_PRIMARYATTACK )
		{
			Throw();
			return;
		}
	}

	BaseClass::HandleAnimEvent( pEvent );	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFWeaponBaseGrenadeProj *CTFWeaponBaseGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flTime, int iFlags )
{
	Assert( 0 && "CBaseCSGrenade::EmitGrenade should not be called. Make sure to implement this in your subclass!\n" );
	return NULL;
}

#endif

