//------------------------------------------------------------------------------------------------
//! One vehicle's supply cost in COA_AAS_VehicleSupplyCosts.conf
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sCategory")]
class COA_AAS_VehicleSupplyCost
{
	[Attribute(desc: "What kind of vehicle this is - for reading the list only")]
	string m_sCategory;

	[Attribute(desc: "The vehicle - matched by its GUID, so a moved file still matches", params: "et")]
	ResourceName m_sPrefab;

	[Attribute("100", desc: "Supplies it costs from a depot or helipad", params: "0 inf 1")]
	int m_iCost;

	[Attribute(SCR_ECharacterRank.INVALID.ToString(), UIWidgets.ComboBox, desc: "Rank needed to request it - INVALID keeps the vehicle's own", enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	SCR_ECharacterRank m_eRequiredRank;
}

//------------------------------------------------------------------------------------------------
//! A cost for vehicles not in the list, by their editor labels (first matching rule wins)
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sCategory")]
class COA_AAS_VehicleSupplyCostRule
{
	[Attribute(desc: "What kind of vehicle this is - for reading the list only")]
	string m_sCategory;

	[Attribute(desc: "Vehicles with all of these labels match", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(EEditableEntityLabel))]
	ref array<EEditableEntityLabel> m_aLabels;

	[Attribute("100", desc: "Supplies it costs", params: "0 inf 1")]
	int m_iCost;

	[Attribute(SCR_ECharacterRank.INVALID.ToString(), UIWidgets.ComboBox, desc: "Rank needed to request it - INVALID keeps the vehicle's own. Applies to any unlisted vehicle that matches, whatever its cost.", enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	SCR_ECharacterRank m_eRequiredRank;
}

//------------------------------------------------------------------------------------------------
//! Supply costs of every vehicle the factions can pull from depots and helipads. Conflict takes a
//! vehicle's cost from its prefab's editor budget, which many modded vehicles don't have (so they're
//! free); this overrides it from one config, COA_AAS_VehicleSupplyCosts.conf:
//!  - a vehicle in the list costs what the list says
//!  - any other vehicle keeps its own cost, or - if it has none - gets the first fallback rule its
//!    labels match, else the default
//! Read when the catalogs are set up, the same on the server and every client.
[BaseContainerProps(configRoot: true)]
class COA_AAS_VehicleSupplyCosts
{
	static const ResourceName CONFIG = "{6C4D2E8F1A3B5C70}Configs/COA_AAS/COA_AAS_VehicleSupplyCosts.conf";

	[Attribute(desc: "Every vehicle and what it costs")]
	protected ref array<ref COA_AAS_VehicleSupplyCost> m_aVehicles;

	[Attribute(desc: "For vehicles not in the list that have no cost of their own")]
	protected ref array<ref COA_AAS_VehicleSupplyCostRule> m_aFallbackRules;

	[Attribute("100", desc: "For vehicles not in the list, with no cost of their own and no matching rule", params: "0 inf 1")]
	protected int m_iDefaultCost;

	protected ref map<string, int> m_mCostByGuid;
	protected ref map<string, SCR_ECharacterRank> m_mRankByGuid;

	protected static ref COA_AAS_VehicleSupplyCosts s_Instance;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	static COA_AAS_VehicleSupplyCosts Get()
	{
		if (s_bLoaded)
			return s_Instance;

		s_bLoaded = true;
		Resource resource = BaseContainerTools.LoadContainer(CONFIG);
		if (!resource || !resource.IsValid())
		{
			Print("[COA_AAS] Vehicle supply costs: can't load " + CONFIG, LogLevel.ERROR);
			return null;
		}

		s_Instance = COA_AAS_VehicleSupplyCosts.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the entry's supply cost, -1 to leave it as it is
	int GetCost(notnull SCR_EntityCatalogEntry entry, int current)
	{
		BuildLookup();
		string guid = Guid(entry.GetPrefab());
		if (m_mCostByGuid.Contains(guid))
			return m_mCostByGuid.Get(guid);

		// Only vehicles - the same catalogs hold characters and groups
		SCR_EntityCatalog catalog = entry.GetCatalogParent();
		if (!catalog || catalog.GetCatalogType() != EEntityCatalogType.VEHICLE || current > 0)
			return -1;

		if (m_aFallbackRules)
		{
			foreach (COA_AAS_VehicleSupplyCostRule rule : m_aFallbackRules)
			{
				if (rule && rule.m_aLabels && !rule.m_aLabels.IsEmpty() && entry.HasAllEditableEntityLabels(rule.m_aLabels))
					return rule.m_iCost;
			}
		}

		return m_iDefaultCost;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the rank needed to request the entry, INVALID to leave the vehicle's own
	SCR_ECharacterRank GetRequiredRank(notnull SCR_EntityCatalogEntry entry)
	{
		BuildLookup();
		string guid = Guid(entry.GetPrefab());
		if (m_mRankByGuid.Contains(guid))
			return m_mRankByGuid.Get(guid);

		SCR_EntityCatalog catalog = entry.GetCatalogParent();
		if (!catalog || catalog.GetCatalogType() != EEntityCatalogType.VEHICLE || !m_aFallbackRules)
			return SCR_ECharacterRank.INVALID;

		// The first matching rule that sets a rank
		foreach (COA_AAS_VehicleSupplyCostRule rule : m_aFallbackRules)
		{
			if (rule && rule.m_eRequiredRank != SCR_ECharacterRank.INVALID && rule.m_aLabels && !rule.m_aLabels.IsEmpty() && entry.HasAllEditableEntityLabels(rule.m_aLabels))
				return rule.m_eRequiredRank;
		}

		return SCR_ECharacterRank.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildLookup()
	{
		if (m_mCostByGuid)
			return;

		m_mCostByGuid = new map<string, int>();
		m_mRankByGuid = new map<string, SCR_ECharacterRank>();
		if (!m_aVehicles)
			return;

		foreach (COA_AAS_VehicleSupplyCost vehicle : m_aVehicles)
		{
			if (!vehicle || vehicle.m_sPrefab.IsEmpty())
				continue;

			string guid = Guid(vehicle.m_sPrefab);
			m_mCostByGuid.Set(guid, vehicle.m_iCost);
			if (vehicle.m_eRequiredRank != SCR_ECharacterRank.INVALID)
				m_mRankByGuid.Set(guid, vehicle.m_eRequiredRank);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}path" -> "GUID"
	protected static string Guid(ResourceName prefab)
	{
		string text = prefab;
		int close = text.IndexOf("}");
		if (text.StartsWith("{") && close > 1)
			return text.Substring(1, close - 1);

		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! Applies COA_AAS_VehicleSupplyCosts once Conflict has read the prefab's own cost.
//! A config-built class: the modded declaration repeats vanilla's attributes and parent, or configs can
//! no longer create it ("Unknown class 'SCR_EntityCatalogSpawnerData'") and depots lose their vehicles.
[BaseContainerProps(configRoot: true), BaseContainerCustomCheckIntTitleField("m_bEnabled", "SpawnerData", "DISABLED - SpawnerData", 1)]
modded class SCR_EntityCatalogSpawnerData : SCR_BaseEntityCatalogData
{
	// What the config says, kept apart so nothing initialising after us (other mods) can undo it
	protected int m_iCOA_AAS_SupplyCost = -1;
	protected SCR_ECharacterRank m_eCOA_AAS_RequiredRank = SCR_ECharacterRank.INVALID;

	//------------------------------------------------------------------------------------------------
	override void InitData(notnull SCR_EntityCatalogEntry entry)
	{
		super.InitData(entry);

		COA_AAS_VehicleSupplyCosts costs = COA_AAS_VehicleSupplyCosts.Get();
		if (!costs)
			return;

		int cost = costs.GetCost(entry, m_iSupplyCost);
		if (cost >= 0)
		{
			m_iSupplyCost = cost;
			m_iCOA_AAS_SupplyCost = cost;
		}

		SCR_ECharacterRank rank = costs.GetRequiredRank(entry);
		if (rank != SCR_ECharacterRank.INVALID)
		{
			m_eMinimumRequiredRank = rank;
			m_eCOA_AAS_RequiredRank = rank;
		}
	}

	//------------------------------------------------------------------------------------------------
	override int GetSupplyCost()
	{
		if (m_iCOA_AAS_SupplyCost >= 0)
			return m_iCOA_AAS_SupplyCost;

		return super.GetSupplyCost();
	}

	//------------------------------------------------------------------------------------------------
	override SCR_ECharacterRank GetMinimumRequiredRank()
	{
		if (m_eCOA_AAS_RequiredRank != SCR_ECharacterRank.INVALID)
			return m_eCOA_AAS_RequiredRank;

		return super.GetMinimumRequiredRank();
	}
}
