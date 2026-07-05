//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Nail
//
//=============================================================================
#include "cbase.h"
#include "tf_projectile_nail.h"
#include "tf_gamerules.h"

#ifdef CLIENT_DLL
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "c_te_effect_dispatch.h"
#include "input.h"
#include "c_tf_player.h"
#include "cliententitylist.h"
#endif

#ifdef GAME_DLL
#include "tf_player.h"
#endif


//=============================================================================
//
// TF Syringe Projectile functions (Server specific).
//
#define SYRINGE_MODEL				"models/weapons/w_models/w_syringe_proj.mdl"
#define SYRINGE_DISPATCH_EFFECT		"ClientProjectile_Syringe"

LINK_ENTITY_TO_CLASS( tf_projectile_syringe, CTFProjectile_Syringe );
PRECACHE_REGISTER( tf_projectile_syringe );


short g_sModelIndexSyringe;
void PrecacheSyringe(void *pUser)
{
	g_sModelIndexSyringe = modelinfo->GetModelIndex( SYRINGE_MODEL );
}

PRECACHE_REGISTER_FN(PrecacheSyringe);

//-----------------------------------------------------------------------------
// CTFProjectile_Syringe
//-----------------------------------------------------------------------------
#define SYRINGE_GRAVITY		0.3f
#define SYRINGE_VELOCITY	1000.0f
// Purpose:
//-----------------------------------------------------------------------------
CTFBaseProjectile *CTFProjectile_Syringe::Create( 
	const Vector &vecOrigin, 
	const QAngle &vecAngles, 
	CTFWeaponBaseGun *pLauncher /*= NULL*/,
	CBaseEntity *pOwner /*= NULL*/, 
	CBaseEntity *pScorer /*= NULL*/, 
	bool bCritical /*= false */
) {
	return CTFBaseProjectile::Create( "tf_projectile_syringe", vecOrigin, vecAngles, pOwner, SYRINGE_VELOCITY, g_sModelIndexSyringe, SYRINGE_DISPATCH_EFFECT, pScorer, bCritical );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int CTFProjectile_Syringe::PhysicsSolidMaskForEntity( void ) const
{ 
	return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_REDTEAM | CONTENTS_BLUETEAM;
}

//-----------------------------------------------------------------------------
float CTFProjectile_Syringe::GetGravity( void )
{
	return SYRINGE_GRAVITY;
}


#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
void GetSyringeTrailParticleName( CTFPlayer *pPlayer, CAttribute_String *attrParticleName, bool bCritical )
{
	int iTeamNumber = TF_TEAM_RED;	
	if ( pPlayer )
	{
		iTeamNumber = pPlayer->GetTeamNumber();
		CTFWeaponBase *pWeapon = pPlayer->GetActiveTFWeapon();
		if ( pWeapon )
		{
			static CSchemaAttributeDefHandle pAttrDef_ParticleName( "projectile particle name" );
			CEconItemView *pItem = pWeapon->GetAttributeContainer()->GetItem();
			if ( pAttrDef_ParticleName && pItem )
			{
				if ( pItem->FindAttribute( pAttrDef_ParticleName, attrParticleName ) )
				{
					const char * pParticleName = attrParticleName->value().c_str();
					if ( iTeamNumber == TF_TEAM_BLUE && V_stristr( pParticleName, "_teamcolor_red" ))
					{
						static char pBlue[256];
						V_StrSubst( attrParticleName->value().c_str(), "_teamcolor_red", "_teamcolor_blue", pBlue, 256 );
						attrParticleName->set_value( pBlue );
					}
					return;
				}
			}
		}
	}
	
	if ( iTeamNumber == TF_TEAM_BLUE )
	{
		attrParticleName->set_value( bCritical ? "nailtrails_medic_blue_crit" : "nailtrails_medic_blue" );
	}
	else
	{
		attrParticleName->set_value( bCritical ? "nailtrails_medic_red_crit" : "nailtrails_medic_red" );
	}
	return;
}

//-----------------------------------------------------------------------------
// Purpose: For Synrgine Projectiles, Add effects
//-----------------------------------------------------------------------------
void ClientsideProjectileSyringeCallback( const CEffectData &data )
{
	// Get the syringe and add it to the client entity list, so we can attach a particle system to it.
	C_TFPlayer *pPlayer = dynamic_cast<C_TFPlayer*>( ClientEntityList().GetBaseEntityFromHandle( data.m_hEntity ) );
	if ( pPlayer )
	{
		C_LocalTempEntity *pSyringe = ClientsideProjectileCallback( data, SYRINGE_GRAVITY );
		if ( pSyringe )
		{
			CAttribute_String attrParticleName;
			
			pSyringe->m_nSkin = ( pPlayer->GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;
			bool bCritical = ( ( data.m_nDamageType & DMG_CRITICAL ) != 0 );
			GetSyringeTrailParticleName( pPlayer, &attrParticleName, bCritical );

			pSyringe->AddParticleEffect( attrParticleName.value().c_str() );
			pSyringe->AddEffects( EF_NOSHADOW );
			pSyringe->flags |= FTENT_USEFASTCOLLISIONS;
		}
	}
}

DECLARE_CLIENT_EFFECT( SYRINGE_DISPATCH_EFFECT, ClientsideProjectileSyringeCallback );

#endif

//=============================================================================
//
// CTFProjectile_Nail — PF2C port
//
// PF2C's real implementation does NOT use this fork's CTFBaseProjectile::Create()
// dispatch-effect system. That system unconditionally hides the server entity
// (EF_NODRAW) and substitutes a clientside temp entity, which is only correct
// for projectiles fired directly from a gun (the temp-entity visual logic
// relocates to the firing player's weapon muzzle). Nails are emitted in
// mid-air by a grenade, so that relocation silently teleported every nail's
// visual entity onto the thrower's gun model, and the server entity's
// DAMAGE_NO/EF_NODRAW state combined with the dispatch path's expectations
// meant no real collision was happening either.
//
// Fix: CTFProjectile_Nail is a real, normally-drawn networked entity with its
// own datatable (matching PF2C's DT_TFProjectile_Nail), spawned directly via
// CBaseEntity::Create + manual Spawn() — exactly as PF2C does it.
//
//=============================================================================
#define NAIL_MODEL				"models/weapons/w_models/w_nail.mdl"

IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_Nail, DT_TFProjectile_Nail )

BEGIN_NETWORK_TABLE( CTFProjectile_Nail, DT_TFProjectile_Nail )
END_NETWORK_TABLE()

CTFProjectile_Nail::CTFProjectile_Nail()
{
}

CTFProjectile_Nail::~CTFProjectile_Nail()
{
#ifdef CLIENT_DLL
	ParticleProp()->StopEmission();
#endif
}

#ifdef GAME_DLL

LINK_ENTITY_TO_CLASS( tf_projectile_nail, CTFProjectile_Nail );
PRECACHE_REGISTER( tf_projectile_nail );

short g_sModelIndexNail;
void PrecacheNail( void *pUser )
{
	g_sModelIndexNail = modelinfo->GetModelIndex( NAIL_MODEL );
}
PRECACHE_REGISTER_FN( PrecacheNail );

//-----------------------------------------------------------------------------
// Purpose: Spawns the nail as a real, visible, solid networked entity —
//          mirrors CTFBaseProjectile::Spawn() but skips AddEffects(EF_NODRAW),
//          since this entity is meant to actually be seen.
//-----------------------------------------------------------------------------
void CTFProjectile_Nail::Spawn( void )
{
	Precache();

	SetModel( GetProjectileModelName() );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	AddEFlags( EFL_NO_WATER_VELOCITY_CHANGE );

	UTIL_SetSize( this, -Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ) );

	SetGravity( GetGravity() );
	m_takedamage = DAMAGE_NO;
	SetDamage( 25.0f );

	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	SetTouch( &CTFProjectile_Nail::ProjectileTouch );
	SetThink( &CTFProjectile_Nail::FlyThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Override ProjectileTouch to fix kill credit.
//          Our owner is the CTFGrenadeNailProjectile (needed so nails don't
//          immediately collide with and get blocked by the grenade body they
//          spawn inside). But CTFBaseProjectile::ProjectileTouch sets
//          info.SetAttacker(GetOwnerEntity()), which would credit the grenade
//          entity, not the player. At the moment of impact we swap the owner
//          to the player for the base call, then restore it.
//          PF2C port — kill credit fix.
//-----------------------------------------------------------------------------
void CTFProjectile_Nail::ProjectileTouch( CBaseEntity *pOther )
{
	CBaseEntity *pGrenade = GetOwnerEntity();
	if ( pGrenade && !pGrenade->IsPlayer() )
	{
		// Follow the chain: nail → nail grenade → player
		CBaseEntity *pThrower = pGrenade->GetOwnerEntity();
		if ( pThrower && pThrower->IsPlayer() )
		{
			SetOwnerEntity( pThrower );
			BaseClass::ProjectileTouch( pOther );
			// Entity is being removed after touch — no need to restore.
			return;
		}
	}
	BaseClass::ProjectileTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Creates the nail entity directly — does NOT go through
//          CTFBaseProjectile::Create(), which always hides the entity and
//          dispatches a clientside fake instead. This entity is real and
//          networked, so it just needs normal spawn + velocity setup.
//-----------------------------------------------------------------------------
CTFProjectile_Nail *CTFProjectile_Nail::Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner /*= NULL*/, CBaseEntity *pScorer /*= NULL*/, bool bCritical /*= false*/ )
{
	CTFProjectile_Nail *pNail = static_cast<CTFProjectile_Nail*>( CBaseEntity::Create( "tf_projectile_nail", vecOrigin, vecAngles, pOwner ) );
	if ( !pNail )
		return NULL;

	pNail->SetOwnerEntity( pOwner );
	pNail->SetScorer( pScorer );
	pNail->Spawn();

	Vector vecForward;
	AngleVectors( vecAngles, &vecForward );
	pNail->SetAbsVelocity( vecForward * CTFProjectile_Nail::GetInitialVelocity() );

	if ( pOwner )
	{
		pNail->ChangeTeam( pOwner->GetTeamNumber() );
	}

	if ( bCritical )
	{
		pNail->SetCritical( true );
	}

	return pNail;
}
}

#else // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Attaches a nail-trail particle to the real, visible nail entity.
//          Called every frame the entity is alive and not dormant — matches
//          PF2C's CreateTrails() pattern, reusing the medic nailtrail particles
//          already shipped (the grenade-specific nailgrenadejet.pcf adds the
//          jet effect on the grenade itself; this is the per-nail trail).
//-----------------------------------------------------------------------------
void CTFProjectile_Nail::CreateTrails( void )
{
	if ( IsDormant() )
		return;

	if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		ParticleProp()->Create( IsCritical() ? "nailtrails_medic_blue_crit" : "nailtrails_medic_blue", PATTACH_ABSORIGIN_FOLLOW );
	}
	else
	{
		ParticleProp()->Create( IsCritical() ? "nailtrails_medic_red_crit" : "nailtrails_medic_red", PATTACH_ABSORIGIN_FOLLOW );
	}
}

#endif
