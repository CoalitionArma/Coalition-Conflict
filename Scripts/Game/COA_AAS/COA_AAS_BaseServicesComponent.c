//------------------------------------------------------------------------------------------------
//! One service composition placed around a base: built in the BLUFOR or OPFOR style depending on who
//! owns the base when the match starts. The services themselves are faction-neutral - every service
//! point registers with the base it stands in and follows its owner (SCR_MilitaryBaseLogicComponent),
//! so an armory sells from the owner's item catalog and a depot spawns from its vehicle catalog.
[BaseContainerProps(), SCR_BaseContainerCustomTitleResourceName("m_sBluforPrefab", true)]
class COA_AAS_ServiceSlot
{
	[Attribute(desc: "Composition used when BLUFOR owns the base at the start", params: "et")]
	ResourceName m_sBluforPrefab;

	[Attribute(desc: "Composition used otherwise (OPFOR or the independent defenders)", params: "et")]
	ResourceName m_sOpforPrefab;

	[Attribute("40", desc: "Preferred distance from the base centre [m]. Keep it inside the base radius (150 m) minus the search radius, or the service won't register with the base.", params: "0 150 1")]
	float m_fDistance;

	[Attribute("0", desc: "Preferred bearing from the base centre, relative to the base's own facing [deg]", params: "0 360 1")]
	float m_fBearing;

	[Attribute("8", desc: "Clear radius the composition needs [m] - roughly 7 for small, 11 for medium, 16 for large slots", params: "1 40 0.5")]
	float m_fClearance;
}

//------------------------------------------------------------------------------------------------
//! Pre-builds Conflict services (armory, vehicle depots, ...) at every base when the match starts, so
//! players can resupply and pull vehicles without building them first. Server only; the compositions
//! are replicated like the HQ tents Conflict spawns itself.
[ComponentEditorProps(category: "COALITION/Conflict", description: "Pre-built armories and vehicle depots at Conflict bases")]
class COA_AAS_BaseServicesComponentClass : SCR_BaseGameModeComponentClass
{
}

class COA_AAS_BaseServicesComponent : SCR_BaseGameModeComponent
{
	[Attribute(desc: "Services at bases that can be a main base (MOBs)", category: "Base Services")]
	protected ref array<ref COA_AAS_ServiceSlot> m_aMainBaseServices;

	[Attribute(desc: "Services at every other base", category: "Base Services")]
	protected ref array<ref COA_AAS_ServiceSlot> m_aBaseServices;

	[Attribute("35", desc: "How far from its preferred spot a service may move to find clear, level ground [m]", params: "5 100 1", category: "Base Services")]
	protected float m_fSearchRadius;

	[Attribute("0.3", desc: "Highest ground rise allowed across the composition, as a fraction of its clear radius", params: "0.05 1 0.05", category: "Base Services")]
	protected float m_fMaxSlope;

	protected SCR_GameModeCampaign m_Campaign;

	//! After Conflict's own start-up: its HQ tents (spawned 1 s after bases init) and the AAS owner hand-over (2 s)
	protected static const int SPAWN_DELAY_MS = 4000;
	protected static const int MAX_CANDIDATES = 80;
	protected static const float TRACE_HEIGHT = 4;
	protected static const int PILES_DELAY_MS = 2000;

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		m_Campaign = SCR_GameModeCampaign.Cast(GetGameMode());
		if (!m_Campaign || !m_Campaign.GetBaseManager())
			return;

		SCR_CampaignMilitaryBaseManager baseManager = m_Campaign.GetBaseManager();
		if (baseManager.IsBasesInitDone())
			OnBasesInitialized();
		else
			baseManager.GetOnAllBasesInitialized().Insert(OnBasesInitialized);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBasesInitialized()
	{
		GetGame().GetCallqueue().CallLater(SpawnServices, SPAWN_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnServices()
	{
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);

		int spawned;
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (!base.IsInitialized() || base.GetType() != SCR_ECampaignBaseType.BASE)
				continue;

			array<ref COA_AAS_ServiceSlot> slots = m_aBaseServices;
			if (base.CanBeHQ())
				slots = m_aMainBaseServices;

			if (!slots)
				continue;

			bool blufor = base.GetCampaignFaction() && base.GetCampaignFaction() == m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
			foreach (COA_AAS_ServiceSlot slot : slots)
			{
				ResourceName prefab = slot.m_sOpforPrefab;
				if (blufor)
					prefab = slot.m_sBluforPrefab;

				if (!prefab.IsEmpty() && SpawnService(base, slot, prefab))
					spawned++;
			}
		}

		Print(string.Format("[COA_AAS] Base services: %1 compositions placed", spawned), LogLevel.NORMAL);

		// Once the new storages have registered with their bases
		GetGame().GetCallqueue().CallLater(ShowSupplyPiles, PILES_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Empty objectives still show the supply pile trucks unload at (COA_AAS_SupplyStorage)
	protected void ShowSupplyPiles()
	{
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			if (base && base.IsInitialized())
				COA_AAS_SupplyStorage.KeepPilesVisible(base);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnService(notnull SCR_CampaignMilitaryBaseComponent base, notnull COA_AAS_ServiceSlot slot, ResourceName prefab)
	{
		IEntity baseEntity = base.GetOwner();
		BaseWorld world = baseEntity.GetWorld();
		vector center = baseEntity.GetOrigin();

		float bearing = (baseEntity.GetYawPitchRoll()[0] + slot.m_fBearing) * Math.DEG2RAD;
		vector preferred = center + Vector(Math.Sin(bearing), 0, Math.Cos(bearing)) * slot.m_fDistance;

		vector position;
		if (!FindSpot(preferred, slot.m_fClearance, world, position))
		{
			Print(string.Format("[COA_AAS] Base services: no clear ground for %1 at %2", prefab, base.GetBaseName()), LogLevel.WARNING);
			return false;
		}

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[COA_AAS] Base services: can't load %1", prefab), LogLevel.ERROR);
			return false;
		}

		// Face the base centre
		vector toCenter = center - position;
		toCenter[1] = 0;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(toCenter.ToYaw(), 0, 0), params.Transform);
		params.Transform[3] = position;

		IEntity composition = GetGame().SpawnEntityPrefab(resource, world, params);
		if (!composition)
			return false;

		// Same as Conflict's HQ tents: snap the composition and its children to the terrain
		vector transform[4];
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(composition.FindComponent(SCR_EditableEntityComponent));
		if (editable)
		{
			editable.GetTransform(transform);
			if (SCR_TerrainHelper.SnapToTerrain(transform, world))
				editable.SetTransformWithChildren(transform);
		}
		else
		{
			composition.GetTransform(transform);
			SCR_TerrainHelper.SnapToTerrain(transform, world);
			composition.SetTransform(transform);
		}

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
			aiWorld.RequestNavmeshRebuildEntity(composition);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest clear, roughly level spot to preferred within the search radius
	protected bool FindSpot(vector preferred, float clearance, BaseWorld world, out vector position)
	{
		array<vector> candidates = {};
		SCR_WorldTools.FindAllEmptyTerrainPositions(candidates, preferred, m_fSearchRadius, clearance, TRACE_HEIGHT, MAX_CANDIDATES, TraceFlags.ENTS | TraceFlags.OCEAN, world);

		float maxRise = clearance * m_fMaxSlope;
		float bestDistSq = float.MAX;
		bool found;
		foreach (vector candidate : candidates)
		{
			if (GetGroundRise(candidate, clearance, world) > maxRise)
				continue;

			float distSq = vector.DistanceSqXZ(candidate, preferred);
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				position = candidate;
				found = true;
			}
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Height difference of the terrain across the footprint (centre plus eight points on its edge)
	protected float GetGroundRise(vector center, float radius, BaseWorld world)
	{
		float low = world.GetSurfaceY(center[0], center[2]);
		float high = low;
		for (int i = 0; i < 8; i++)
		{
			float angle = i * Math.PI * 0.25;
			float y = world.GetSurfaceY(center[0] + Math.Sin(angle) * radius, center[2] + Math.Cos(angle) * radius);
			low = Math.Min(low, y);
			high = Math.Max(high, y);
		}

		return high - low;
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_BaseServicesComponent()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(SpawnServices);
			GetGame().GetCallqueue().Remove(ShowSupplyPiles);
		}
	}
}
