//------------------------------------------------------------------------------------------------
//! Rank-unlocked loadouts replace dressing up: arsenals hand out rifles, ammunition, grenades, medical
//! and support station / pylon stock, but no clothing, gear, launchers or specialist weapons, and custom
//! loadout saving is off. Applies only when the AAS game mode component is present.
modded class SCR_ArsenalComponent
{
	protected static const int COA_AAS_ALLOWED_MODES = SCR_EArsenalItemMode.AMMUNITION | SCR_EArsenalItemMode.CONSUMABLE | SCR_EArsenalItemMode.SUPPORT_STATION | SCR_EArsenalItemMode.PYLON;
	protected static const int COA_AAS_ALLOWED_TYPES = SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.HEAL;

	//------------------------------------------------------------------------------------------------
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		bool aas = COA_AAS_ConflictComponent.GetInstance() != null;

		// The armory's weapons box doesn't stock medical (only the field hospital's box does) - add it, so
		// every armory hands out the faction's medical items (vanilla + ACE). Same result on every machine.
		if (aas && (m_eSupportedArsenalItemTypes & SCR_EArsenalItemType.RIFLE) && !(m_eSupportedArsenalItemTypes & SCR_EArsenalItemType.HEAL))
			SetSupportedArsenalItemTypes(m_eSupportedArsenalItemTypes | SCR_EArsenalItemType.HEAL);

		bool found = super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);
		if (!found || !aas)
			return found;

		for (int i = filteredArsenalItems.Count() - 1; i >= 0; i--)
		{
			SCR_ArsenalItem item = filteredArsenalItems[i];
			if (!item || ((item.GetItemMode() & COA_AAS_ALLOWED_MODES) == 0 && (item.GetItemType() & COA_AAS_ALLOWED_TYPES) == 0))
				filteredArsenalItems.Remove(i);
		}

		return !filteredArsenalItems.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override SCR_EArsenalSaveType GetArsenalSaveType()
	{
		if (COA_AAS_ConflictComponent.GetInstance())
			return SCR_EArsenalSaveType.SAVING_DISABLED;

		return super.GetArsenalSaveType();
	}
}
