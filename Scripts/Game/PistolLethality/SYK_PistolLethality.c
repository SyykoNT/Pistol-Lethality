//------------------------------------------------------------------------------------------------
//! Loads the config once and answers "which group does this projectile belong to?", with a cache.
//! Prefabs are matched by GUID when the config entry has one, and by lower-case path otherwise, so
//! rounds from other mods can be listed without making those mods a dependency.
class SYK_PistolLethality
{
	static const ResourceName CONFIG = "{7CAA2A7328981CE2}Configs/PistolLethality/SYK_PistolLethality.conf";

	protected static ref SYK_PistolLethalityConfig s_Config;
	protected static bool s_bLoaded;
	protected static ref map<string, int> s_mGroupByKey;
	protected static ref map<ResourceName, int> s_mCache;

	//------------------------------------------------------------------------------------------------
	//! Lookup keys for a resource name: "guid:<GUID>" when it carries one, and "path:<lower-case path>"
	protected static void GetKeys(string resourceName, out string guidKey, out string pathKey)
	{
		guidKey = "";
		string path = resourceName;
		int close = resourceName.IndexOf("}");
		if (resourceName.StartsWith("{") && close > 1)
		{
			guidKey = "guid:" + resourceName.Substring(1, close - 1);
			path = resourceName.Substring(close + 1, resourceName.Length() - close - 1);
		}

		path.ToLower();
		pathKey = "path:" + path;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FindGroupIndex(string resourceName, out int groupIndex)
	{
		string guidKey, pathKey;
		GetKeys(resourceName, guidKey, pathKey);
		if (!guidKey.IsEmpty() && s_mGroupByKey.Find(guidKey, groupIndex))
			return true;

		return s_mGroupByKey.Find(pathKey, groupIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		s_bLoaded = true;
		s_mGroupByKey = new map<string, int>();
		s_mCache = new map<ResourceName, int>();
		s_Config = SCR_ConfigHelperT<SYK_PistolLethalityConfig>.GetConfigObject(CONFIG);
		if (!s_Config)
		{
			Print("[PistolLethality] Config not found: " + CONFIG, LogLevel.ERROR);
			return;
		}

		if (!s_Config.m_aGroups)
			return;

		int enabledGroups;
		int prefabs;
		foreach (int index, SYK_PistolLethalityGroup group : s_Config.m_aGroups)
		{
			if (!group || !group.m_aProjectilePrefabs)
				continue;

			if (group.m_bEnabled)
				enabledGroups++;

			foreach (ResourceName prefab : group.m_aProjectilePrefabs)
			{
				string guidKey, pathKey;
				GetKeys(prefab, guidKey, pathKey);
				if (!guidKey.IsEmpty())
					s_mGroupByKey.Set(guidKey, index);

				s_mGroupByKey.Set(pathKey, index);
				prefabs++;
			}
		}

		PrintFormat("[PistolLethality] Loaded %1 groups (%2 enabled), %3 projectile prefabs. Master switch: %4", s_Config.m_aGroups.Count(), enabledGroups, prefabs, s_Config.m_bEnabled);
	}

	//------------------------------------------------------------------------------------------------
	//! \return index of the group (enabled or not) whose list contains this prefab or one of its parents, or -1
	static int GetGroupIndexForContainer(BaseContainer container)
	{
		if (!s_bLoaded)
			Load();

		if (!s_mGroupByKey)
			return -1;

		while (container)
		{
			int found;
			if (FindGroupIndex(container.GetResourceName(), found))
				return found;

			container = container.GetAncestor();
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the enabled group the projectile belongs to, or null
	static SYK_PistolLethalityGroup GetGroup(IEntity projectile)
	{
		if (!projectile)
			return null;

		if (!s_bLoaded)
			Load();

		if (!s_Config || !s_Config.m_bEnabled || s_mGroupByKey.IsEmpty())
			return null;

		EntityPrefabData prefabData = projectile.GetPrefabData();
		if (!prefabData)
			return null;

		ResourceName prefabName = prefabData.GetPrefabName();
		int groupIndex;
		if (!s_mCache.Find(prefabName, groupIndex))
		{
			groupIndex = GetGroupIndexForContainer(prefabData.GetPrefab());
			s_mCache.Set(prefabName, groupIndex);
		}

		if (groupIndex < 0)
			return null;

		SYK_PistolLethalityGroup group = s_Config.m_aGroups[groupIndex];
		if (!group.m_bEnabled)
			return null;

		return group;
	}
}
