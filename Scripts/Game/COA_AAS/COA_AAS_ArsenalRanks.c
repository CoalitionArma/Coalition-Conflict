//------------------------------------------------------------------------------------------------
//! A sight's rank by how far it zooms (COA_AAS_ArsenalRanks)
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class COA_AAS_ArsenalZoomRank
{
	[Attribute(desc: "Name - for the list only")]
	string m_sName;

	[Attribute("1", desc: "Sights that zoom at least this much (their highest magnification) need the rank", params: "0 100 0.1")]
	float m_fMinZoom;

	[Attribute(SCR_ECharacterRank.PRIVATE.ToString(), UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	SCR_ECharacterRank m_eRank;
}

//------------------------------------------------------------------------------------------------
//! The base rank of a kind of weapon
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class COA_AAS_ArsenalTypeRank
{
	[Attribute(desc: "Name - for the list only")]
	string m_sName;

	[Attribute(SCR_EArsenalItemType.RIFLE.ToString(), UIWidgets.ComboBox, desc: "Arsenal item type", enums: ParamEnumArray.FromEnum(SCR_EArsenalItemType))]
	SCR_EArsenalItemType m_eType;

	[Attribute(SCR_ECharacterRank.PRIVATE.ToString(), UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	SCR_ECharacterRank m_eRank;
}

//------------------------------------------------------------------------------------------------
//! A rank for weapons or attachments whose prefab path contains any of the texts (case-insensitive)
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class COA_AAS_ArsenalPatternRank
{
	[Attribute(desc: "Name - for the list only")]
	string m_sName;

	[Attribute(desc: "Prefab path contains any of these, e.g. \"PKM\"")]
	ref array<string> m_aContains;

	[Attribute(SCR_ECharacterRank.PRIVATE.ToString(), UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	SCR_ECharacterRank m_eRank;

	//------------------------------------------------------------------------------------------------
	bool Matches(string lowerPath)
	{
		if (!m_aContains)
			return false;

		foreach (string text : m_aContains)
		{
			string lower = text;
			lower.ToLower();
			if (!lower.IsEmpty() && lowerPath.Contains(lower))
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! One item's rank, set by hand
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sPrefab")]
class COA_AAS_ArsenalItemRank
{
	[Attribute(desc: "The weapon or attachment - matched by GUID", params: "et")]
	ResourceName m_sPrefab;

	[Attribute(SCR_ECharacterRank.PRIVATE.ToString(), UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	SCR_ECharacterRank m_eRank;
}

//------------------------------------------------------------------------------------------------
//! Ranks needed to take weapons and attachments from the arsenal (Conflict rank-locks arsenal items
//! and greys out what you're not ranked for). Worked out from each item's prefab, so modded gear
//! is covered without listing it:
//!  - a sight by its highest zoom (m_aZoomRanks); a suppressor or underbarrel grenade launcher by kind
//!  - a weapon by its arsenal type (m_aTypeRanks), raised by the first name pattern it matches
//!    (m_aPatternRanks: machine guns, heavy snipers, launchers...), and by whatever scope, suppressor
//!    or launcher it comes fitted with (a scope built into the weapon counts too)
//!  - m_aItems sets an item's rank by hand, over all of that
//! Other arsenal items (clothing, medical, ammo...) keep their own rank.
[BaseContainerProps(configRoot: true)]
class COA_AAS_ArsenalRanks
{
	static const ResourceName CONFIG = "{6C5F3B2A1D4E7C80}Configs/COA_AAS/COA_AAS_ArsenalRanks.conf";

	protected static const string SIGHT_BASE = "Prefabs/Weapons/Core/WeaponSight_Base.et";
	protected static const string SUPPRESSOR_BASE = "Prefabs/Weapons/Core/Suppressor_base.et";
	protected static const string UGL_BASE = "Prefabs/Weapons/Core/UGL_Base.et";
	protected static const float INTEGRAL_SCOPE_MIN_ZOOM = 2;	// a weapon's own sights count as a scope from this zoom

	[Attribute(desc: "Sights by highest zoom - the highest tier a sight reaches applies")]
	protected ref array<ref COA_AAS_ArsenalZoomRank> m_aZoomRanks;

	[Attribute(SCR_ECharacterRank.LIEUTENANT.ToString(), UIWidgets.ComboBox, desc: "Suppressors", enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	protected SCR_ECharacterRank m_eSuppressorRank;

	[Attribute(SCR_ECharacterRank.CORPORAL.ToString(), UIWidgets.ComboBox, desc: "Underbarrel grenade launchers", enums: ParamEnumArray.FromEnum(SCR_ECharacterRank))]
	protected SCR_ECharacterRank m_eGrenadeLauncherRank;

	[Attribute(desc: "Weapons by arsenal type")]
	protected ref array<ref COA_AAS_ArsenalTypeRank> m_aTypeRanks;

	[Attribute(desc: "Weapons and attachments by name - the first match replaces the type's rank")]
	protected ref array<ref COA_AAS_ArsenalPatternRank> m_aPatternRanks;

	[Attribute(desc: "Items set by hand - over everything above")]
	protected ref array<ref COA_AAS_ArsenalItemRank> m_aItems;

	protected ref map<string, int> m_mCache = new map<string, int>();
	protected ref map<string, int> m_mItems;

	protected static ref COA_AAS_ArsenalRanks s_Instance;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	static COA_AAS_ArsenalRanks Get()
	{
		if (s_bLoaded)
			return s_Instance;

		s_bLoaded = true;
		Resource resource = BaseContainerTools.LoadContainer(CONFIG);
		if (!resource || !resource.IsValid())
		{
			Print("[COA_AAS] Arsenal ranks: can't load " + CONFIG, LogLevel.ERROR);
			return null;
		}

		s_Instance = COA_AAS_ArsenalRanks.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the rank an arsenal item needs, INVALID to leave it as it is
	SCR_ECharacterRank GetRank(ResourceName prefab, SCR_EArsenalItemType type, SCR_EArsenalItemMode mode)
	{
		bool weapon = mode == SCR_EArsenalItemMode.WEAPON || mode == SCR_EArsenalItemMode.WEAPON_VARIANTS;
		bool attachment = mode == SCR_EArsenalItemMode.ATTACHMENT || type == SCR_EArsenalItemType.WEAPON_ATTACHMENT;
		if (!weapon && !attachment)
			return SCR_ECharacterRank.INVALID;

		string guid = Guid(prefab);
		int cached;
		if (m_mCache.Find(guid, cached))
			return cached;

		SCR_ECharacterRank rank = SCR_ECharacterRank.INVALID;
		if (!m_mItems)
			BuildItems();

		if (m_mItems.Contains(guid))
			rank = m_mItems.Get(guid);
		else
			rank = Work(prefab, type, weapon);

		m_mCache.Set(guid, rank);
		return rank;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ECharacterRank Work(ResourceName prefab, SCR_EArsenalItemType type, bool weapon)
	{
		// The resource owns the prefab's containers - keep it alive while they're read
		Resource resource = Resource.Load(prefab);
		IEntitySource source = SCR_BaseContainerTools.FindEntitySource(resource);
		if (!source)
			return SCR_ECharacterRank.INVALID;

		string lowerPath = prefab;
		lowerPath.ToLower();

		// Weapons: the type's rank, or the first matching pattern's instead. Attachments: their kind,
		// raised by a matching pattern.
		SCR_ECharacterRank rank;
		if (weapon)
			rank = TypeRank(type);
		else
			rank = AttachmentRank(source);

		if (!m_aPatternRanks)
			m_aPatternRanks = {};

		foreach (COA_AAS_ArsenalPatternRank pattern : m_aPatternRanks)
		{
			if (!pattern || !pattern.Matches(lowerPath))
				continue;

			if (weapon)
				rank = pattern.m_eRank;
			else
				rank = Math.Max(rank, pattern.m_eRank);

			break;
		}

		if (!weapon)
			return rank;

		// What the weapon comes fitted with, and a scope of its own
		rank = Math.Max(rank, AttachmentsRank(source));
		float ownZoom = HighestZoom(source);
		if (ownZoom >= INTEGRAL_SCOPE_MIN_ZOOM)
			rank = Math.Max(rank, ZoomRank(ownZoom));

		return rank;
	}

	//------------------------------------------------------------------------------------------------
	//! A single attachment: a sight by zoom, a suppressor, a grenade launcher, else private
	protected SCR_ECharacterRank AttachmentRank(notnull IEntitySource source)
	{
		if (Inherits(source, SIGHT_BASE))
			return ZoomRank(Math.Max(1, HighestZoom(source)));

		if (Inherits(source, SUPPRESSOR_BASE))
			return m_eSuppressorRank;

		if (Inherits(source, UGL_BASE))
			return m_eGrenadeLauncherRank;

		return SCR_ECharacterRank.PRIVATE;
	}

	//------------------------------------------------------------------------------------------------
	//! The highest rank of the attachments a weapon comes fitted with
	protected SCR_ECharacterRank AttachmentsRank(notnull IEntitySource source)
	{
		SCR_ECharacterRank rank = SCR_ECharacterRank.PRIVATE;
		array<IEntityComponentSource> slots = {};
		SCR_BaseContainerTools.FindComponentSourcesOfClass(source, AttachmentSlotComponent, true, slots);
		foreach (IEntityComponentSource slot : slots)
		{
			BaseContainer attachSlot = slot.GetObject("AttachmentSlot");
			if (!attachSlot)
				continue;

			bool enabled = true;
			attachSlot.Get("Enabled", enabled);
			ResourceName attached;
			attachSlot.Get("Prefab", attached);
			if (!enabled || attached.IsEmpty())
				continue;

			Resource attachedResource = Resource.Load(attached);
			IEntitySource attachedSource = SCR_BaseContainerTools.FindEntitySource(attachedResource);
			if (attachedSource)
				rank = Math.Max(rank, AttachmentRank(attachedSource));
		}

		return rank;
	}

	//------------------------------------------------------------------------------------------------
	//! The highest magnification of the prefab's own sights (not its attachments'), 0 if none zoom
	protected float HighestZoom(notnull IEntitySource source)
	{
		float zoom;
		array<IEntityComponentSource> sights = {};
		SCR_BaseContainerTools.FindComponentSourcesOfClass(source, BaseSightsComponent, true, sights);
		foreach (IEntityComponentSource component : sights)
		{
			BaseContainer fov = component.GetObject("SightsFOVInfo");
			if (!fov)
				continue;

			typename fovType = fov.GetClassName().ToType();
			if (!fovType || !fovType.IsInherited(SCR_SightsZoomFOVInfo))
				continue;

			float baseZoom, maxZoom;
			fov.Get("m_fBaseZoom", baseZoom);
			fov.Get("m_fZoomMax", maxZoom);
			zoom = Math.Max(zoom, Math.Max(baseZoom, maxZoom));
		}

		return zoom;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ECharacterRank ZoomRank(float zoom)
	{
		SCR_ECharacterRank rank = SCR_ECharacterRank.PRIVATE;
		float reached = -1;
		if (!m_aZoomRanks)
			return rank;

		foreach (COA_AAS_ArsenalZoomRank tier : m_aZoomRanks)
		{
			if (tier && zoom >= tier.m_fMinZoom && tier.m_fMinZoom > reached)
			{
				reached = tier.m_fMinZoom;
				rank = tier.m_eRank;
			}
		}

		return rank;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ECharacterRank TypeRank(SCR_EArsenalItemType type)
	{
		if (!m_aTypeRanks)
			return SCR_ECharacterRank.PRIVATE;

		foreach (COA_AAS_ArsenalTypeRank typeRank : m_aTypeRanks)
		{
			if (typeRank && (type & typeRank.m_eType))
				return typeRank.m_eRank;
		}

		return SCR_ECharacterRank.PRIVATE;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Inherits(notnull BaseContainer source, string basePath)
	{
		for (BaseContainer ancestor = source; ancestor; ancestor = ancestor.GetAncestor())
		{
			string name = ancestor.GetResourceName();
			if (name.EndsWith(basePath))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildItems()
	{
		m_mItems = new map<string, int>();
		if (!m_aItems)
			return;

		foreach (COA_AAS_ArsenalItemRank item : m_aItems)
		{
			if (item && !item.m_sPrefab.IsEmpty())
				m_mItems.Set(Guid(item.m_sPrefab), item.m_eRank);
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
//! Applies COA_AAS_ArsenalRanks to weapons and attachments once the catalog is set up.
//! A config-built class: the modded declaration repeats vanilla's attributes and parent, or configs can
//! no longer create it ("Unknown class 'SCR_ArsenalItem'") and every arsenal comes up empty.
[BaseContainerProps(configRoot: true), BaseContainerCustomCheckIntTitleField("m_bEnabled", "Arsenal Data", "DISABLED - Arsenal Data", 1)]
modded class SCR_ArsenalItem : SCR_BaseEntityCatalogData
{
	//------------------------------------------------------------------------------------------------
	override void PostInitData(notnull SCR_EntityCatalogEntry entry)
	{
		super.PostInitData(entry);

		COA_AAS_ArsenalRanks ranks = COA_AAS_ArsenalRanks.Get();
		if (!ranks)
			return;

		SCR_ECharacterRank rank = ranks.GetRank(entry.GetPrefab(), m_eItemType, m_eItemMode);
		if (rank != SCR_ECharacterRank.INVALID)
			m_eRequiredRank = rank;
	}
}
