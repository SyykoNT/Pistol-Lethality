//------------------------------------------------------------------------------------------------
//! Pistol lethality for ACE Medical.
//! ACE Medical Hitzones removes the shared Health pool, so a character only dies when one vital body
//! part (head, chest, abdomen, heart) is destroyed on its own. Rifle rounds empty a 100 HP chest or
//! abdomen in one or two hits; pistol-caliber rounds need about five.
//!
//! For projectile prefabs listed in the config only:
//!  - Torso bonus: a listed round's hit on the chest or abdomen is multiplied inside the hit itself,
//!    after armor. It stays one damage event, so ACE (Second Chance, bleeding rate, kill flow) and any
//!    damage tracker see the full value. Knockout (Resilience) and pain still take the original hit.
//!  - Downed bonus: on a downed target whose ACE Second Chance window has expired, the multiplier is
//!    the downed one. The window itself is untouched.
//!  - Wound rule: a listed round whose hit (before the bonus) removes at least the wound minimum from
//!    the chest, abdomen or hips starts an ACE bleed on that body part.
//! Everything runs on the server. Rifles, frag, melee and every other damage source are unchanged.

//------------------------------------------------------------------------------------------------
enum SYK_EPistolLethalityZone
{
	NONE,
	CHEST,
	ABDOMEN,
	HIPS
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterHitZone : SCR_RegeneratingHitZone
{
	protected SYK_EPistolLethalityZone m_eSYK_PistolZone;
	protected SYK_PistolLethalityGroup m_SYK_PendingPistolGroup;
	protected float m_fSYK_PendingPistolBaseDamage;
	// Multiplier applied to the latest hit on this body part, and the projectile that made it
	protected float m_fSYK_HitMultiplier = 1;
	protected IEntity m_SYK_HitSource;

	//------------------------------------------------------------------------------------------------
	override void OnInit(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.OnInit(pOwnerEntity, pManagerComponent);

		string zoneName = GetName();
		if (zoneName == "Chest")
			m_eSYK_PistolZone = SYK_EPistolLethalityZone.CHEST;
		else if (zoneName == "Abdomen")
			m_eSYK_PistolZone = SYK_EPistolLethalityZone.ABDOMEN;
		else if (zoneName == "Hips")
			m_eSYK_PistolZone = SYK_EPistolLethalityZone.HIPS;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply the torso multiplier to a listed pistol round's hit, and remember it for the wound rule
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		float damage = super.ComputeEffectiveDamage(damageContext, isDOT);

		if (m_eSYK_PistolZone == SYK_EPistolLethalityZone.NONE || isDOT || damageContext.struckHitZone != this)
			return damage;

		// Clear on every direct hit on this body part, so a stale pistol hit can never carry over
		m_SYK_PendingPistolGroup = null;
		m_fSYK_HitMultiplier = 1;
		m_SYK_HitSource = null;
		if (damage <= 0 || damageContext.damageType != EDamageType.KINETIC || !damageContext.damageSource || !Replication.IsServer())
			return damage;

		SYK_PistolLethalityGroup group = SYK_PistolLethality.GetGroup(damageContext.damageSource);
		if (!group)
			return damage;

		m_SYK_PendingPistolGroup = group;
		m_fSYK_PendingPistolBaseDamage = damage;
		m_fSYK_HitMultiplier = SYK_GetPistolMultiplier(group);
		m_SYK_HitSource = damageContext.damageSource;
		return damage * m_fSYK_HitMultiplier;
	}

	//------------------------------------------------------------------------------------------------
	//! The torso multiplier this body part applied to the hit in damageContext (1 when none), so the
	//! knockout pool (Resilience) can take the original hit instead of the boosted one
	float SYK_GetPistolHitMultiplier(notnull BaseDamageContext damageContext)
	{
		if (m_fSYK_HitMultiplier <= 1 || !m_SYK_HitSource || damageContext.damageSource != m_SYK_HitSource)
			return 1;

		return m_fSYK_HitMultiplier;
	}

	//------------------------------------------------------------------------------------------------
	//! Start the bleed once the original hit has fully resolved
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		if (!m_SYK_PendingPistolGroup || damageContext.struckHitZone != this)
			return;

		SYK_PistolLethalityGroup group = m_SYK_PendingPistolGroup;
		m_SYK_PendingPistolGroup = null;
		if (group.m_fWoundMinimumDamage >= 0 && m_fSYK_PendingPistolBaseDamage >= group.m_fWoundMinimumDamage)
			GetGame().GetCallqueue().Call(SYK_StartPistolBleeding);
	}

	//------------------------------------------------------------------------------------------------
	protected float SYK_GetPistolMultiplier(notnull SYK_PistolLethalityGroup group)
	{
		float multiplier = 1;
		if (m_eSYK_PistolZone == SYK_EPistolLethalityZone.CHEST)
			multiplier = group.m_fChestMultiplier;
		else if (m_eSYK_PistolZone == SYK_EPistolLethalityZone.ABDOMEN)
			multiplier = group.m_fAbdomenMultiplier;
		else
			return 1;

		if (SYK_IsDownedAfterSecondChanceWindow())
			multiplier = Math.Max(multiplier, group.m_fDownedMultiplier);

		return Math.Max(multiplier, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected bool SYK_IsDownedAfterSecondChanceWindow()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetOwner());
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.INCAPACITATED)
			return false;

		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(GetHitZoneContainer());
		return damageManager && damageManager.ACE_Medical_ShouldDeactivateSecondChance();
	}

	//------------------------------------------------------------------------------------------------
	protected void SYK_StartPistolBleeding()
	{
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(GetHitZoneContainer());
		if (!damageManager || damageManager.IsDestroyed())
			return;

		damageManager.AddBleedingEffectOnHitZone(this);
	}
}

//------------------------------------------------------------------------------------------------
//! Damage a knockout or pain pool would take from this hit without the torso bonus
class SYK_PistolLethalityPassThrough
{
	static float RemoveBonus(notnull BaseDamageContext damageContext, bool isDOT, float damage)
	{
		if (isDOT || damage <= 0 || damageContext.damageType != EDamageType.KINETIC || !damageContext.damageSource)
			return damage;

		SCR_CharacterHitZone struckHitZone = SCR_CharacterHitZone.Cast(damageContext.struckHitZone);
		if (!struckHitZone)
			return damage;

		return damage / struckHitZone.SYK_GetPistolHitMultiplier(damageContext);
	}
}

//------------------------------------------------------------------------------------------------
//! Knockout stays as ACE intends: the Resilience pool takes the original pistol hit, not the torso bonus
modded class SCR_CharacterResilienceHitZone : SCR_RegeneratingHitZone
{
	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		return SYK_PistolLethalityPassThrough.RemoveBonus(damageContext, isDOT, super.ComputeEffectiveDamage(damageContext, isDOT));
	}
}

//------------------------------------------------------------------------------------------------
//! Pain also comes from the original pistol hit
modded class ACE_Medical_PainHitZone : SCR_RegeneratingHitZone
{
	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		return SYK_PistolLethalityPassThrough.RemoveBonus(damageContext, isDOT, super.ComputeEffectiveDamage(damageContext, isDOT));
	}
}
