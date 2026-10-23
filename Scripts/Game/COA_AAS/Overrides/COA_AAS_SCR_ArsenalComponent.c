//------------------------------------------------------------------------------------------------
//! Rank-unlocked loadouts replace dressing up: arsenals hand out weapons and their attachments (each
//! rank-locked - COA_AAS_ArsenalRanks), ammunition, grenades, explosives, medical and support station /
//! pylon stock, and backpacks (parachutes among them, for the Marines' air insertions), but no other
//! clothing or gear, and custom loadout saving is off. Applies only when the AAS game mode component is
//! present.
modded class SCR_ArsenalComponent
{
	protected static const int COA_AAS_ALLOWED_MODES = SCR_EArsenalItemMode.AMMUNITION | SCR_EArsenalItemMode.CONSUMABLE | SCR_EArsenalItemMode.SUPPORT_STATION | SCR_EArsenalItemMode.PYLON | SCR_EArsenalItemMode.ATTACHMENT;
	protected static const int COA_AAS_WEAPON_TYPES = SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.PISTOL | SCR_EArsenalItemType.MACHINE_GUN | SCR_EArsenalItemType.SNIPER_RIFLE | SCR_EArsenalItemType.ROCKET_LAUNCHER | SCR_EArsenalItemType.WEAPON_ATTACHMENT | SCR_EArsenalItemType.EXPLOSIVES;
	protected static const int COA_AAS_ALLOWED_TYPES = COA_AAS_WEAPON_TYPES | SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.HEAL;

	//------------------------------------------------------------------------------------------------
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		bool aas = COA_AAS_ConflictComponent.GetInstance() != null;

		// The armory's weapons box stocks every weapon type and attachment, medical (only the field
		// hospital's box does by default) and backpacks - so every armory hands out the faction's whole
		// RHS / vanilla / ACE range, rank permitting. Same result on every machine.
		int armoryTypes = COA_AAS_WEAPON_TYPES | SCR_EArsenalItemType.HEAL | SCR_EArsenalItemType.BACKPACK;
		if (aas && (m_eSupportedArsenalItemTypes & SCR_EArsenalItemType.RIFLE) && (m_eSupportedArsenalItemTypes & armoryTypes) != armoryTypes)
			SetSupportedArsenalItemTypes(m_eSupportedArsenalItemTypes | armoryTypes);

		// Weapons, their variants and attachments as item modes too, and plain items (backpacks) - unless the
		// arsenal is switched off (Conflict clears an arsenal's modes to disable it). The filter below still
		// drops every plain item but backpacks.
		int armoryModes = SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS | SCR_EArsenalItemMode.ATTACHMENT | SCR_EArsenalItemMode.DEFAULT;
		if (aas && (m_eSupportedArsenalItemTypes & SCR_EArsenalItemType.RIFLE) && m_eSupportedArsenalItemModes != 0 && (m_eSupportedArsenalItemModes & armoryModes) != armoryModes)
			SetSupportedArsenalItemModes(m_eSupportedArsenalItemModes | armoryModes);

		bool found = super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);
		if (!found || !aas)
			return found;

		for (int i = filteredArsenalItems.Count() - 1; i >= 0; i--)
		{
			SCR_ArsenalItem item = filteredArsenalItems[i];
			if (!item || (item.GetItemType() & SCR_EArsenalItemType.BACKPACK))
				continue;

			if ((item.GetItemMode() & COA_AAS_ALLOWED_MODES) == 0 && (item.GetItemType() & COA_AAS_ALLOWED_TYPES) == 0)
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
