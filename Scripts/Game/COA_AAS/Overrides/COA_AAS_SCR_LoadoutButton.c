//------------------------------------------------------------------------------------------------
//! Vanilla SetLoadout colours the badge with the faction from the loadout prefab's editable UI info and
//! crashes (NULL pointer) when that faction key isn't in the current faction manager - which is the case
//! for mod soldiers that carry their own faction key. Same behaviour, but the badge falls back to the
//! loadout's affiliated faction and is left alone when neither resolves.
modded class SCR_LoadoutButton
{
	//------------------------------------------------------------------------------------------------
	override void SetLoadout(SCR_BasePlayerLoadout loadout)
	{
		m_Loadout = loadout;
		if (!loadout)
			return;

		SCR_EditableEntityUIInfo entityUIInfo = GetUIInfo();
		if (!entityUIInfo)
			return;

		string iconPath = entityUIInfo.GetIconPath();
		if (iconPath.IsEmpty())
			SetImage(entityUIInfo.GetImageSetPath(), entityUIInfo.GetIconSetName());
		else
			SetImage(iconPath);

		if (!m_wBadge)
			return;

		Faction faction = entityUIInfo.GetFaction();
		SCR_FactionPlayerLoadout factionLoadout = SCR_FactionPlayerLoadout.Cast(loadout);
		if (!faction && factionLoadout && GetGame().GetFactionManager())
			faction = GetGame().GetFactionManager().GetFactionByKey(factionLoadout.m_sAffiliatedFaction);

		if (!faction)
			return;

		m_wBadge.SetOpacity(1.00);
		m_wBadge.SetColor(faction.GetFactionColor());
	}
}
