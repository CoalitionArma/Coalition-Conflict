//------------------------------------------------------------------------------------------------
//! A depot or helipad entry is out of reach: the player hasn't the XP it costs (COA_AAS_AssetXPCost)
modded enum SCR_EEntityRequestStatus
{
	COA_AAS_NOT_ENOUGH_XP
}

//------------------------------------------------------------------------------------------------
//! What a kind of asset costs in XP on top of its supplies, from vehicle depots and helipads - so
//! persistent ranks don't make tanks and gunships free. An entry matches when it has all the labels
//! (if any are set) or its prefab path contains any of the texts (case-insensitive). The first matching
//! rule in COA_AAS_ConflictComponent.m_aAssetXPCosts sets the cost.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class COA_AAS_AssetXPCost
{
	[Attribute(desc: "Name - for the list only")]
	protected string m_sName;

	[Attribute(desc: "Entries with all of these labels match", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(EEditableEntityLabel))]
	protected ref array<EEditableEntityLabel> m_aLabels;

	[Attribute(desc: "...or entries whose prefab path contains any of these (case-insensitive), e.g. \"gunship\"")]
	protected ref array<string> m_aPrefabContains;

	[Attribute("1000", desc: "XP it costs", params: "0 inf 1")]
	protected int m_iXPCost;

	//------------------------------------------------------------------------------------------------
	bool Matches(notnull SCR_EntityCatalogEntry entry)
	{
		if (m_aLabels && !m_aLabels.IsEmpty() && entry.HasAllEditableEntityLabels(m_aLabels))
			return true;

		if (!m_aPrefabContains || m_aPrefabContains.IsEmpty())
			return false;

		string prefab = entry.GetPrefab();
		prefab.ToLower();
		foreach (string text : m_aPrefabContains)
		{
			string lower = text;
			lower.ToLower();
			if (!lower.IsEmpty() && prefab.Contains(lower))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	int GetXPCost()
	{
		return m_iXPCost;
	}

	//------------------------------------------------------------------------------------------------
	//! XP of the player controlling user, -1 if there's no such player. Players only know their own.
	static int GetPlayerXP(IEntity user)
	{
		if (!user)
			return -1;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!controller)
			return -1;

		SCR_PlayerXPHandlerComponent xp = SCR_PlayerXPHandlerComponent.Cast(controller.FindComponent(SCR_PlayerXPHandlerComponent));
		if (!xp)
			return -1;

		return xp.GetPlayerXP();
	}

	//------------------------------------------------------------------------------------------------
	//! XP cost of a catalog entry, 0 when free (or there's no AAS)
	static int GetCostOf(SCR_EntityCatalogEntry entry)
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || !entry)
			return 0;

		return aas.GetAssetXPCost(entry);
	}
}
