//------------------------------------------------------------------------------------------------
//! Rank-unlocked loadouts replace dressing up: arsenals only resupply (ammunition, grenades, medical,
//! support station / pylon stock) and custom loadout saving is off, so the deploy menu loadouts are
//! the only way to change kit. Applies only when the AAS game mode component is present.
modded class SCR_ArsenalComponent
{
	protected static const int COA_AAS_RESUPPLY_MODES = SCR_EArsenalItemMode.AMMUNITION | SCR_EArsenalItemMode.CONSUMABLE | SCR_EArsenalItemMode.SUPPORT_STATION | SCR_EArsenalItemMode.PYLON;
	protected static const int COA_AAS_RESUPPLY_TYPES = SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.HEAL;

	//------------------------------------------------------------------------------------------------
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		bool found = super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);
		if (!found || !COA_AAS_ConflictComponent.GetInstance())
			return found;

		for (int i = filteredArsenalItems.Count() - 1; i >= 0; i--)
		{
			SCR_ArsenalItem item = filteredArsenalItems[i];
			if (!item || ((item.GetItemMode() & COA_AAS_RESUPPLY_MODES) == 0 && (item.GetItemType() & COA_AAS_RESUPPLY_TYPES) == 0))
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
