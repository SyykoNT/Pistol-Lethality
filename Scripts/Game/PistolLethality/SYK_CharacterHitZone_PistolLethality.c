//------------------------------------------------------------------------------------------------
//! Pistol lethality for ACE Medical.
//! ACE Medical Hitzones removes the shared Health pool, so a character only dies when one vital body
//! part (head, chest, abdomen, heart) is destroyed on its own. Rifle rounds empty a 100 HP chest or
//! abdomen in one or two hits; pistol-caliber rounds need about five, and their hits rarely pass the
//! vanilla wound threshold, so they mostly cause pain and knockout.
//!
//! For projectile prefabs listed in the config only:
//!  - Torso bonus: after a listed round damages the chest or abdomen, that body part loses an extra
//!    (multiplier - 1) x the HP the hit actually removed (so armor still counts). Pain, consciousness,
//!    ACE's pneumothorax roll, organs and the vanilla wound check all see the original hit.
//!  - Downed bonus: on a downed target whose ACE Second Chance window has expired, the multiplier is
//!    the downed one, so confirming a kill matches rifles. The window itself is untouched.
//!  - Wound rule: a listed round that removed at least the wound minimum from the chest, abdomen or
//!    hips starts an ACE bleed on that body part.
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
	protected float m_fSYK_PendingPistolHealth;

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
	//! Remember the body part's HP before a listed pistol round damages it
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		float damage = super.ComputeEffectiveDamage(damageContext, isDOT);

		if (m_eSYK_PistolZone == SYK_EPistolLethalityZone.NONE || isDOT || damageContext.struckHitZone != this)
			return damage;

		// Clear on every direct torso hit, so a stale pistol hit can never carry over to the next one
		m_SYK_PendingPistolGroup = null;
		if (damageContext.damageType != EDamageType.KINETIC || !damageContext.damageSource || !Replication.IsServer())
			return damage;

		m_SYK_PendingPistolGroup = SYK_PistolLethality.GetGroup(damageContext.damageSource);
		m_fSYK_PendingPistolHealth = GetHealth();
		return damage;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply the bleed and the extra HP loss once the original hit has fully resolved
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		if (!m_SYK_PendingPistolGroup || damageContext.struckHitZone != this)
			return;

		SYK_PistolLethalityGroup group = m_SYK_PendingPistolGroup;
		m_SYK_PendingPistolGroup = null;
		float dealt = m_fSYK_PendingPistolHealth - GetHealth();
		if (dealt <= 0)
			return;

		if (group.m_fWoundMinimumDamage >= 0 && dealt >= group.m_fWoundMinimumDamage)
			GetGame().GetCallqueue().Call(SYK_StartPistolBleeding);

		float multiplier = 1;
		if (m_eSYK_PistolZone == SYK_EPistolLethalityZone.CHEST)
			multiplier = group.m_fChestMultiplier;
		else if (m_eSYK_PistolZone == SYK_EPistolLethalityZone.ABDOMEN)
			multiplier = group.m_fAbdomenMultiplier;
		else
			return;

		if (SYK_IsDownedAfterSecondChanceWindow())
			multiplier = Math.Max(multiplier, group.m_fDownedMultiplier);

		if (multiplier <= 1)
			return;

		GetGame().GetCallqueue().Call(SYK_ApplyPistolBonusDamage, (multiplier - 1) * dealt);
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
	protected void SYK_ApplyPistolBonusDamage(float extra)
	{
		if (extra <= 0 || GetDamageState() == EDamageState.DESTROYED)
			return;

		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(GetHitZoneContainer());
		if (!damageManager || damageManager.IsDestroyed())
			return;

		SetHealth(Math.Max(GetHealth() - extra, 0));
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
