//------------------------------------------------------------------------------------------------
//! Rank-unlocked loadouts replace dressing up: arsenals hand out weapons and their attachments (each
//! rank-locked - COA_AAS_ArsenalRanks), ammunition, grenades, explosives, medical and support station /
//! pylon stock, backpacks (parachutes among them, for the Marines' air insertions), and field gear - maps,
//! compasses, watches, flashlights, e-tools, wirecutters, kits, flares, handheld radios, binoculars, mortar
//! range cards - but no other clothing or gear, and custom loadout saving is off. Each item is listed
//! once, though the faction's catalogs (vanilla, RHS, Tactical Flava...) may each carry it. Applies only when the AAS game mode component is
//! present.
modded class SCR_ArsenalComponent
{
	protected static const int COA_AAS_ALLOWED_MODES = SCR_EArsenalItemMode.AMMUNITION | SCR_EArsenalItemMode.CONSUMABLE | SCR_EArsenalItemMode.SUPPORT_STATION | SCR_EArsenalItemMode.PYLON | SCR_EArsenalItemMode.ATTACHMENT;
	protected static const int COA_AAS_WEAPON_TYPES = SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.PISTOL | SCR_EArsenalItemType.MACHINE_GUN | SCR_EArsenalItemType.SNIPER_RIFLE | SCR_EArsenalItemType.ROCKET_LAUNCHER | SCR_EArsenalItemType.WEAPON_ATTACHMENT | SCR_EArsenalItemType.EXPLOSIVES;
	protected static const int COA_AAS_ALLOWED_TYPES = COA_AAS_WEAPON_TYPES | SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.HEAL;
	// Field gear is mostly equipment, range cards are mortar items: armories stock those types for it alone
	protected static const int COA_AAS_FIELD_GEAR_TYPES = SCR_EArsenalItemType.EQUIPMENT | SCR_EArsenalItemType.MORTARS;

	protected int m_iCOA_AAS_OwnTypes = -1;	// the arsenal's types before the armory's were added

	//------------------------------------------------------------------------------------------------
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		bool aas = COA_AAS_ConflictComponent.GetInstance() != null;

		// The armory's weapons box stocks every weapon type and attachment, medical (only the field
		// hospital's box does by default) and backpacks - so every armory hands out the faction's whole
		// RHS / vanilla / ACE range, rank permitting. Same result on every machine.
		int armoryTypes = COA_AAS_WEAPON_TYPES | SCR_EArsenalItemType.HEAL | SCR_EArsenalItemType.BACKPACK | COA_AAS_FIELD_GEAR_TYPES;
		if (aas && (m_eSupportedArsenalItemTypes & SCR_EArsenalItemType.RIFLE) && (m_eSupportedArsenalItemTypes & armoryTypes) != armoryTypes)
		{
			if (m_iCOA_AAS_OwnTypes == -1)
				m_iCOA_AAS_OwnTypes = m_eSupportedArsenalItemTypes;

			SetSupportedArsenalItemTypes(m_eSupportedArsenalItemTypes | armoryTypes);
		}

		// Weapons, their variants and attachments as item modes too, and plain items (backpacks) - unless the
		// arsenal is switched off (Conflict clears an arsenal's modes to disable it). The filter below still
		// drops every plain item but backpacks.
		int armoryModes = SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS | SCR_EArsenalItemMode.ATTACHMENT | SCR_EArsenalItemMode.DEFAULT;
		if (aas && (m_eSupportedArsenalItemTypes & SCR_EArsenalItemType.RIFLE) && m_eSupportedArsenalItemModes != 0 && (m_eSupportedArsenalItemModes & armoryModes) != armoryModes)
			SetSupportedArsenalItemModes(m_eSupportedArsenalItemModes | armoryModes);

		bool found = super.GetFilteredArsenalItems(filteredArsenalItems, requiresDisplayType);
		if (!found || !aas)
			return found;

		int ownTypes = m_iCOA_AAS_OwnTypes;
		if (ownTypes == -1)
			ownTypes = m_eSupportedArsenalItemTypes;

		for (int i = filteredArsenalItems.Count() - 1; i >= 0; i--)
		{
			SCR_ArsenalItem item = filteredArsenalItems[i];
			if (!item || (item.GetItemType() & SCR_EArsenalItemType.BACKPACK) || COA_AAS_IsFieldGear(item))
				continue;

			// Other equipment and mortar items (mortar rounds) only where the arsenal stocked them itself
			if ((item.GetItemType() & COA_AAS_FIELD_GEAR_TYPES) && (item.GetItemType() & ownTypes) == 0)
			{
				filteredArsenalItems.Remove(i);
				continue;
			}

			if ((item.GetItemMode() & COA_AAS_ALLOWED_MODES) == 0 && (item.GetItemType() & COA_AAS_ALLOWED_TYPES) == 0)
				filteredArsenalItems.Remove(i);
		}

		COA_AAS_RemoveDuplicates(filteredArsenalItems);
		return !filteredArsenalItems.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! One entry per item: of several catalogs' entries for the same prefab, the lowest rank wins, then the
	//! cheapest - the first stays where it was listed
	protected void COA_AAS_RemoveDuplicates(notnull array<SCR_ArsenalItem> items)
	{
		map<ResourceName, int> kept = new map<ResourceName, int>();
		array<SCR_ArsenalItem> unique = {};
		foreach (SCR_ArsenalItem item : items)
		{
			if (!item)
				continue;

			ResourceName prefab = item.GetItemResourceName();
			int index;
			if (!kept.Find(prefab, index))
			{
				kept.Insert(prefab, unique.Count());
				unique.Insert(item);
				continue;
			}

			SCR_ArsenalItem current = unique[index];
			if (item.GetRequiredRank() < current.GetRequiredRank()
				|| (item.GetRequiredRank() == current.GetRequiredRank() && item.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT) < current.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT)))
				unique[index] = item;
		}

		items.Copy(unique);
	}

	//------------------------------------------------------------------------------------------------
	//! Field gear, by its prefab path: maps and basic supplies, binoculars (rangefinders too) and mortar range
	//! cards - not night vision, GPS, radio backpacks, pouches or weapon parts
	protected bool COA_AAS_IsFieldGear(notnull SCR_ArsenalItem item)
	{
		if (item.GetItemType() & SCR_EArsenalItemType.RADIO_BACKPACK)
			return false;

		string path = item.GetItemResourceName();
		path.ToLower();
		array<string> folders = {
			"/maps/", "/compass/", "/watches/", "/flashlights/", "/kits/", "etool", "wirecutters", "/flares/",
			"jerrycan", "/mineflags/", "/radios/", "/binoculars/", "/ballistictable/", "/balistictable/"
		};
		foreach (string folder : folders)
		{
			if (path.Contains(folder))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override SCR_EArsenalSaveType GetArsenalSaveType()
	{
		if (COA_AAS_ConflictComponent.GetInstance())
			return SCR_EArsenalSaveType.SAVING_DISABLED;

		return super.GetArsenalSaveType();
	}
}
