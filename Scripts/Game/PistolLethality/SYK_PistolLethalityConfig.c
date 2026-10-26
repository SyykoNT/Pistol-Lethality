//------------------------------------------------------------------------------------------------
//! One group of projectile prefabs and the rules applied to them.
[BaseContainerProps(), BaseContainerCustomTitleField("m_sName")]
class SYK_PistolLethalityGroup
{
	[Attribute(desc: "Group name, shown in the editor and the server log")]
	string m_sName;

	[Attribute(defvalue: "1", desc: "Whether this group's rounds get the pistol rules")]
	bool m_bEnabled;

	[Attribute(desc: "Projectile prefabs in this group. Child prefabs (tracers, variants) match through their parent.", uiwidget: UIWidgets.ResourceAssignArray, params: "et")]
	ref array<ResourceName> m_aProjectilePrefabs;

	[Attribute(defvalue: "1", desc: "Chest: total HP loss = this x the HP the hit removed (1 = unchanged)", params: "1 10")]
	float m_fChestMultiplier;

	[Attribute(defvalue: "1", desc: "Abdomen: total HP loss = this x the HP the hit removed (1 = unchanged)", params: "1 10")]
	float m_fAbdomenMultiplier;

	[Attribute(defvalue: "1", desc: "Chest and abdomen multiplier on a downed target after ACE's Second Chance window has expired", params: "1 20")]
	float m_fDownedMultiplier;

	[Attribute(defvalue: "10", desc: "A hit that removed at least this much HP from the chest or abdomen (and the hips, if switched on below) starts an ACE bleed. Negative = off.", params: "-1 100")]
	float m_fWoundMinimumDamage;

	[Attribute(defvalue: "0", desc: "Also apply the wound rule to hip hits. Off by default: rifle hip hits at range stay under the vanilla bleed threshold, so pistols would out-bleed rifles there.")]
	bool m_bWoundOnHips;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class SYK_PistolLethalityConfig
{
	[Attribute(defvalue: "1", desc: "Master switch")]
	bool m_bEnabled;

	[Attribute(desc: "Groups of projectile prefabs and their rules")]
	ref array<ref SYK_PistolLethalityGroup> m_aGroups;
}
