//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Nail Projectile
//
//=============================================================================
#ifndef TF_PROJECTILE_NAIL_H
#define TF_PROJECTILE_NAIL_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "tf_projectile_base.h"
#include "tf_weaponbase_gun.h"

//-----------------------------------------------------------------------------
// Purpose: Identical to a nail except for model used
//-----------------------------------------------------------------------------
class CTFProjectile_Syringe : public CTFBaseProjectile
{
	DECLARE_CLASS( CTFProjectile_Syringe, CTFBaseProjectile );

public:
	// Creation.
	static CTFBaseProjectile *Create( const Vector &vecOrigin, const QAngle &vecAngles, CTFWeaponBaseGun *pLauncher = NULL, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL, bool bCritical = false );	

	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;
	virtual const char *GetProjectileModelName( void )	{ return "models/weapons/w_models/w_syringe_proj.mdl"; }
	virtual float GetGravity( void );
};

//-----------------------------------------------------------------------------
// Purpose: Nail projectile — fired by the Nail Grenade when it detonates.
//          PF2C port: unlike CTFProjectile_Syringe above, this does NOT use
//          CTFBaseProjectile::Create()'s dispatch-effect/temp-entity system.
//          That system always hides the real server entity (EF_NODRAW) and
//          fakes a visual via a clientside temp entity tied to the firing
//          player's muzzle — correct for guns, wrong for a grenade emitting
//          nails in mid-air. PF2C's real CTFProjectile_Nail is a normal
//          networked, visibly-drawn entity (own DT_TFProjectile_Nail table),
//          so we replicate that here instead of forcing it through the
//          syringe-style fake-projectile path.
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
#define CTFProjectile_Nail C_TFProjectile_Nail
#endif
class CTFProjectile_Nail : public CTFBaseProjectile
{
	DECLARE_CLASS( CTFProjectile_Nail, CTFBaseProjectile );
	DECLARE_NETWORKCLASS();

public:
	CTFProjectile_Nail();
	~CTFProjectile_Nail();

#ifdef GAME_DLL
	static CTFProjectile_Nail *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL, bool bCritical = false );
	virtual void Spawn( void );
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const { return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_REDTEAM | CONTENTS_BLUETEAM; }
	virtual void ProjectileTouch( CBaseEntity *pOther );	// PF2C port: traces owner chain for kill credit
#endif
	virtual const char *GetProjectileModelName( void ) { return "models/weapons/w_models/w_nail.mdl"; }
	virtual float GetGravity( void ) { return 0.001f; }
	static float GetInitialVelocity( void ) { return 2000.0f; }

#ifdef CLIENT_DLL
	virtual void CreateTrails( void );
#endif
};


#endif	//TF_PROJECTILE_NAIL_H
