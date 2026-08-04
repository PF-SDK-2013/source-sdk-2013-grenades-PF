//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF Heal Grenade.
//
//=============================================================================//
#include "cbase.h"
#include "tf_weaponbase.h"
#include "tf_gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "tf_weapon_grenade_heal.h"

// Server specific.
#ifdef GAME_DLL
#include "tf_player.h"
#include "items.h"
#include "tf_weaponbase_grenadeproj.h"
#include "soundent.h"
#include "KeyValues.h"
#include "particle_parse.h"
#include "tf_gamestats.h"
#endif

#define GRENADE_HEAL_TIMER	3.0f //Seconds
#define	GRENADE_HEAL_LEADIN	2.0f 

//=============================================================================
//
// TF Heal Grenade tables.
//

IMPLEMENT_NETWORKCLASS_ALIASED( TFGrenadeHeal, DT_TFGrenadeHeal )

BEGIN_NETWORK_TABLE( CTFGrenadeHeal, DT_TFGrenadeHeal )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFGrenadeHeal )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_grenade_heal, CTFGrenadeHeal );
PRECACHE_WEAPON_REGISTER( tf_weapon_grenade_heal );

//=============================================================================
//
// TF Heal Grenade functions.
//

// Server specific.
#ifdef GAME_DLL

BEGIN_DATADESC( CTFGrenadeHeal )
END_DATADESC()

//ConVar tf_grenade_heal_amount( "tf_grenade_heal_amount", "100", FCVAR_CHEAT, "Amount healed by the medic heal grenade.\n" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFWeaponBaseGrenadeProj *CTFGrenadeHeal::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, 
													 AngularImpulse angImpulse, CBasePlayer *pPlayer, float flTime, int iflags )
{
	return CTFGrenadeHealProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, 
		pPlayer, GetTFWpnData(), flTime );
}

//=============================================================================
//
// TF Heal Grenade Projectile functions (Server specific).
//

#define GRENADE_MODEL "models/weapons/w_models/w_grenade_heal.mdl"

LINK_ENTITY_TO_CLASS( tf_weapon_grenade_heal_projectile, CTFGrenadeHealProjectile );
PRECACHE_WEAPON_REGISTER( tf_weapon_grenade_heal_projectile );

BEGIN_DATADESC( CTFGrenadeHealProjectile )
	DEFINE_THINKFUNC( DetonateThink ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGrenadeHealProjectile* CTFGrenadeHealProjectile::Create( const Vector &position, const QAngle &angles, 
														 const Vector &velocity, const AngularImpulse &angVelocity, 
														 CBaseCombatCharacter *pOwner, const CTFWeaponInfo &weaponInfo, float timer, int iFlags )
{
	CTFGrenadeHealProjectile *pGrenade = static_cast<CTFGrenadeHealProjectile*>( CTFWeaponBaseGrenadeProj::Create( "tf_weapon_grenade_heal_projectile", position, angles, velocity, angVelocity, pOwner, weaponInfo, timer, iFlags ) );
	if ( pGrenade )
	{
		pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );	
		pGrenade->m_iHealAmount = weaponInfo.GetWeaponData( TF_WEAPON_PRIMARY_MODE ).m_nHeal;	// PF2C port
	}

	return pGrenade;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::Spawn()
{
	SetModel( GRENADE_MODEL );

	BaseClass::Spawn();

	m_bPlayedLeadIn = false;
	m_iHealAmount = 0;		// overwritten by Create() right after construction; defensive default only

	SetThink( &CTFGrenadeHealProjectile::DetonateThink );

	// Since this code only runs on the server, make sure it shows the tempents it creates.
	CDisablePredictionFiltering disabler;

	if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		DispatchParticleEffect( "heal_ticker_blue", PATTACH_ABSORIGIN_FOLLOW, this );
	}
	else
	{
		DispatchParticleEffect( "heal_ticker_red", PATTACH_ABSORIGIN_FOLLOW, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	PrecacheScriptSound( "Weapon_Grenade_Heal.LeadIn" );
	PrecacheScriptSound( "Weapon_Grenade_Heal.Bounce" );
	PrecacheScriptSound( "Weapon_Grenade_Heal.Explode" );
	PrecacheParticleSystem( "heal_ticker_blue" );
	PrecacheParticleSystem( "heal_ticker_red" );
	PrecacheParticleSystem( "heal_grenade_blue" );
	PrecacheParticleSystem( "heal_grenade_red" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::BounceSound( void )
{
	EmitSound( "Weapon_Grenade_Heal.Bounce" );
}

extern ConVar tf_grenade_show_radius;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::Detonate()
{
	if ( ShouldNotDetonate() )
	{
		RemoveGrenade();
		return;
	}
	
	CTFPlayer* pThrower = ToTFPlayer(GetThrower());
	float flRadius = m_DmgRadius;
	float flHealAmount = m_iHealAmount;	// PF2C port -- weapon script "Heal" field, properly parsed

	CTakeDamageInfo info( this, GetThrower(), vec3_origin, GetAbsOrigin(), m_flDamage, DMG_GENERIC | DMG_PREVENT_PHYSICS_FORCE );

	if ( tf_grenade_show_radius.GetBool() )
	{
		DrawRadius( flRadius );
	}

	// Heal every friendly player in the radius for 100 health
	CBaseEntity * pEntity = NULL;
	for (CEntitySphereQuery sphere( GetAbsOrigin(), flRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
	{
		if (pEntity->m_takedamage == DAMAGE_NO)
			continue;

		// check for valid player
		if (!pEntity->IsPlayer())
			continue;

		CTFPlayer *pPlayer = ToTFPlayer( pEntity );

		if (!pPlayer)
			continue;

		if ( pPlayer->TeamID() == GetThrower()->TeamID() )
		{
			pPlayer->m_Shared.RemoveCond(TF_COND_INFECTED);
			pPlayer->m_Shared.RemoveCond(TF_COND_BURNING);
			pPlayer->m_Shared.RemoveCond(TF_COND_NAPALM_BURNING);
			pPlayer->m_Shared.RemoveCond(TF_COND_LEG_DAMAGED);
			pPlayer->m_Shared.RemoveCond(TF_COND_TRANQUILIZED);

			int iHealthNeeded = pPlayer->GetMaxHealth() - pPlayer->GetHealth();
			int iHealthIncreased = min( iHealthNeeded, flHealAmount );
			pPlayer->TakeHealth( iHealthIncreased, DMG_GENERIC );

			CTF_GameStats.Event_PlayerHealedOther( pThrower, iHealthIncreased, false );
			IGameEvent* event = gameeventmanager->CreateEvent( "player_healed" );
			if ( event )
			{
				event->SetInt( "priority", 1 );
				event->SetInt( "patient", pPlayer->GetUserID() );
				event->SetInt( "healer", pThrower->GetUserID() );
				event->SetInt( "amount", iHealthIncreased );

				gameeventmanager->FireEvent( event );
			}
		}
		else /* damage and infect enemies */
		{
			pPlayer->TakeDamage( info );
			pPlayer->m_Shared.Infect( pThrower );	// PF2C port: applies TF_COND_INFECTED
		}
	}

	EmitSound( "Weapon_Grenade_Heal.Explode" );

	// Since this code only runs on the server, make sure it shows the tempents it creates.
	CDisablePredictionFiltering disabler;

	if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		DispatchParticleEffect( "heal_grenade_blue", GetAbsOrigin(), vec3_angle );
	}
	else
	{
		DispatchParticleEffect( "heal_grenade_red", GetAbsOrigin(), vec3_angle );
	}

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGrenadeHealProjectile::DetonateThink( void )
{
	if ( !m_bPlayedLeadIn && gpGlobals->curtime > GetDetonateTime() - GRENADE_HEAL_LEADIN )
	{
		Vector soundPosition = GetAbsOrigin() + Vector( 0, 0, 5 );
		CPASAttenuationFilter filter( soundPosition );

		EmitSound( filter, entindex(), "Weapon_Grenade_Heal.LeadIn" );
		m_bPlayedLeadIn = true;
	}

	BaseClass::DetonateThink();
}

#endif
