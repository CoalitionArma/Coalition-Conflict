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
		if (!m_mCostByGuid)
		{
			m_mCostByGuid = new map<string, int>();
			if (m_aVehicles)
			{
				foreach (COA_AAS_VehicleSupplyCost vehicle : m_aVehicles)
				{
					if (vehicle && !vehicle.m_sPrefab.IsEmpty())
						m_mCostByGuid.Set(Guid(vehicle.m_sPrefab), vehicle.m_iCost);
				}
			}
		}

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
//! Applies COA_AAS_VehicleSupplyCosts once Conflict has read the prefab's own cost
modded class SCR_EntityCatalogSpawnerData
{
	//------------------------------------------------------------------------------------------------
	override void InitData(notnull SCR_EntityCatalogEntry entry)
	{
		super.InitData(entry);

		COA_AAS_VehicleSupplyCosts costs = COA_AAS_VehicleSupplyCosts.Get();
		if (!costs)
			return;

		int cost = costs.GetCost(entry, m_iSupplyCost);
		if (cost >= 0)
			m_iSupplyCost = cost;
	}
}
