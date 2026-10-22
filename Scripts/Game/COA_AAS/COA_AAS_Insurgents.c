//------------------------------------------------------------------------------------------------
enum COA_AAS_EInsurgentRole
{
	PATROL,
	ATTACK,
	AMBUSH
}

//------------------------------------------------------------------------------------------------
//! One CHDKz group the director runs, and the waypoints it spawned for it
class COA_AAS_InsurgentGroup
{
	SCR_AIGroup m_Group;
	ref array<IEntity> m_aWaypoints = {};
	vector m_vAnchor;
	COA_AAS_EInsurgentRole m_eRole;
	float m_fSpawnedAt;		// world time [ms]
}

//------------------------------------------------------------------------------------------------
//! Server: keeps the CHDKz in the fight along the front line.
//!  - Patrols: groups anchored at random points near the front, walking a loop around their anchor. They
//!    stay dormant (no soldiers) until players come near, so a front-wide spread costs little; dormant
//!    patrols the front has moved away from are retired and new ones placed on the front as it is now.
//!  - Attacks: every few minutes a group spawns out of sight of players a few hundred metres from a base
//!    near the front - usually the live objective, else an objective or FOB players are around - and
//!    searches and destroys there. Both sides are told.
//!  - Ambushes: groups dug in beside roads in either side's rear, waiting for the supply runs. Dormant
//!    until players come near like patrols; one nobody has driven into for a while moves to another road.
//! Needs the frontline (COA_AAS_ConflictComponent's territory) to know where the front is.
class COA_AAS_Insurgents
{
	protected static const ResourceName WAYPOINT_PATROL = "{22A875E30470BD4F}Prefabs/AI/Waypoints/AIWaypoint_Patrol.et";
	protected static const ResourceName WAYPOINT_CYCLE = "{35BD6541CBB8AC08}Prefabs/AI/Waypoints/AIWaypoint_Cycle.et";
	protected static const ResourceName WAYPOINT_SEARCH_AND_DESTROY = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
	protected static const ResourceName WAYPOINT_DEFEND_SMALL = "{A70634B518C5C3B8}Prefabs/AI/Waypoints/AIWaypoint_Defend_Small.et";
	protected static const float AMBUSH_ROAD_SEARCH = 300;		// how far from a random spot a road may be [m]
	protected static const float AMBUSH_ROAD_OFFSET_MIN = 20;	// ambushers lie this far off the road... [m]
	protected static const float AMBUSH_ROAD_OFFSET_MAX = 40;	// ...up to this far
	protected static const float AMBUSH_HQ_DISTANCE = 1000;		// never this close to a side's HQ [m]
	protected static const float TICK = 20;					// [s]
	protected static const int PATROL_POINTS = 4;
	protected static const float PATROL_SPAWN_DISTANCE = 700;	// soldiers appear when a player is this close
	protected static const float PATROL_DESPAWN_DISTANCE = 900;
	protected static const float LIVE_OBJECTIVE_WEIGHT = 3;	// how much likelier the live objective is as a target
	protected static const float TARGET_PLAYER_RANGE = 1200;	// other targets need players this close
	protected static const float ATTACK_LIFETIME_MS = 1200000;	// a spent attack is cleared up after this long...
	protected static const float ATTACK_CLEANUP_PLAYER_RANGE = 500;	// ...once no player is this close

	protected COA_AAS_ConflictComponent m_AAS;
	protected SCR_GameModeCampaign m_Campaign;
	protected ref array<ResourceName> m_aPatrolGroups;
	protected ref array<ResourceName> m_aAttackGroups;
	protected int m_iMaxPatrols;
	protected int m_iMaxAttacks;
	protected float m_fFrontDistance;
	protected float m_fPatrolRadius;
	protected float m_fRetireDistance;
	protected float m_fAttackIntervalMin;
	protected float m_fAttackIntervalMax;
	protected float m_fAttackDistance;
	protected float m_fAttackFrontRange;
	protected float m_fMinPlayerDistance;

	protected ref array<ResourceName> m_aAmbushGroups;
	protected int m_iMaxAmbushes;
	protected float m_fAmbushMinBehind;
	protected float m_fAmbushMaxBehind;
	protected float m_fAmbushLifetime;		// [ms]
	protected float m_fAmbushMinRoadWidth;

	protected ref array<ref COA_AAS_InsurgentGroup> m_aGroups = {};
	protected ref array<vector> m_aFront = {};
	protected int m_iTerritoryVersion = -1;
	protected float m_fNextAttack;		// world time [ms]
	protected ref array<vector> m_aPlayers = {};

	//------------------------------------------------------------------------------------------------
	void COA_AAS_Insurgents(COA_AAS_ConflictComponent aas, SCR_GameModeCampaign campaign, array<ResourceName> patrolGroups, array<ResourceName> attackGroups,
		int maxPatrols, int maxAttacks, float frontDistance, float patrolRadius, float retireDistance,
		float attackIntervalMin, float attackIntervalMax, float attackDistance, float attackFrontRange, float minPlayerDistance)
	{
		m_AAS = aas;
		m_Campaign = campaign;
		m_aPatrolGroups = patrolGroups;
		m_aAttackGroups = attackGroups;
		m_iMaxPatrols = maxPatrols;
		m_iMaxAttacks = maxAttacks;
		m_fFrontDistance = frontDistance;
		m_fPatrolRadius = patrolRadius;
		m_fRetireDistance = retireDistance;
		m_fAttackIntervalMin = attackIntervalMin;
		m_fAttackIntervalMax = Math.Max(attackIntervalMin, attackIntervalMax);
		m_fAttackDistance = attackDistance;
		m_fAttackFrontRange = attackFrontRange;
		m_fMinPlayerDistance = minPlayerDistance;

		ScheduleAttack();
		GetGame().GetCallqueue().CallLater(Tick, TICK * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Road ambushes behind the front, between minBehind and maxBehind of it [m]; lifetime [s] is how long
	//! one waits untouched before moving
	void SetAmbushes(array<ResourceName> groups, int maxAmbushes, float minBehind, float maxBehind, float lifetime, float minRoadWidth)
	{
		m_aAmbushGroups = groups;
		m_iMaxAmbushes = maxAmbushes;
		m_fAmbushMinBehind = minBehind;
		m_fAmbushMaxBehind = Math.Max(minBehind, maxBehind);
		m_fAmbushLifetime = lifetime * 1000;
		m_fAmbushMinRoadWidth = minRoadWidth;
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_Insurgents()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	//! The match is over: stand everyone down
	void Stop()
	{
		GetGame().GetCallqueue().Remove(Tick);
		foreach (COA_AAS_InsurgentGroup insurgents : m_aGroups)
		{
			Remove(insurgents);
		}

		m_aGroups.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		COA_AAS_Territory territory = m_AAS.GetTerritory();
		if (!territory)
			return;

		if (m_iTerritoryVersion != m_AAS.GetTerritoryVersion())
		{
			m_iTerritoryVersion = m_AAS.GetTerritoryVersion();
			BuildFront(territory);
		}

		if (m_aFront.IsEmpty())
			return;

		GatherPlayers();

		// Forget the gone (wiped out groups delete themselves), retire patrols the front has left behind
		// and attacks that have run their course; move ambushes nobody came by (or that the front reached)
		float retireSq = m_fRetireDistance * m_fRetireDistance;
		float behindSq = m_fAmbushMinBehind * m_fAmbushMinBehind;
		float now = GetGame().GetWorld().GetWorldTime();
		int patrols;
		int attacks;
		int ambushes;
		for (int i = m_aGroups.Count() - 1; i >= 0; i--)
		{
			COA_AAS_InsurgentGroup insurgents = m_aGroups[i];
			if (!insurgents.m_Group)
			{
				Remove(insurgents);
				m_aGroups.Remove(i);
				continue;
			}

			if (insurgents.m_eRole == COA_AAS_EInsurgentRole.AMBUSH)
			{
				bool dormant = insurgents.m_Group.GetAgentsCount() == 0;
				if (dormant && (now - insurgents.m_fSpawnedAt > m_fAmbushLifetime || FrontDistanceSq(insurgents.m_vAnchor) < behindSq))
				{
					Remove(insurgents);
					m_aGroups.Remove(i);
					continue;
				}

				ambushes++;
				continue;
			}

			if (insurgents.m_eRole == COA_AAS_EInsurgentRole.ATTACK)
			{
				if (now - insurgents.m_fSpawnedAt > ATTACK_LIFETIME_MS && !PlayerWithin(insurgents.m_Group.GetCenterOfMass(), ATTACK_CLEANUP_PLAYER_RANGE))
				{
					Remove(insurgents);
					m_aGroups.Remove(i);
					continue;
				}

				attacks++;
				continue;
			}

			if (insurgents.m_Group.GetAgentsCount() == 0 && FrontDistanceSq(insurgents.m_vAnchor) > retireSq)
			{
				Remove(insurgents);
				m_aGroups.Remove(i);
				continue;
			}

			patrols++;
		}

		// One new patrol a tick, so they trickle in along the front
		if (patrols < m_iMaxPatrols)
			SpawnPatrol(territory);

		if (ambushes < m_iMaxAmbushes)
			SpawnAmbush(territory);

		if (attacks < m_iMaxAttacks && GetGame().GetWorld().GetWorldTime() >= m_fNextAttack)
		{
			if (LaunchAttack(territory))
				ScheduleAttack();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where two sides' territory meets: the centres of the squares on either side of it
	protected void BuildFront(notnull COA_AAS_Territory territory)
	{
		m_aFront.Clear();
		for (int z = 0; z < territory.m_iHeight; z++)
		{
			for (int x = 0; x < territory.m_iWidth; x++)
			{
				int owner = territory.OwnerAt(x, z);
				if (owner == COA_AAS_ETerritoryOwner.NONE)
					continue;

				if (Differs(territory, owner, x + 1, z) || Differs(territory, owner, x - 1, z) || Differs(territory, owner, x, z + 1) || Differs(territory, owner, x, z - 1))
					m_aFront.Insert(territory.CellCentre(x, z));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool Differs(notnull COA_AAS_Territory territory, int owner, int x, int z)
	{
		int other = territory.OwnerAt(x, z);
		return other != COA_AAS_ETerritoryOwner.NONE && other != owner;
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPatrol(notnull COA_AAS_Territory territory)
	{
		if (!m_aPatrolGroups || m_aPatrolGroups.IsEmpty())
			return;

		vector anchor;
		if (!FindSpot(territory, m_aFront.GetRandomElement(), 0, m_fFrontDistance, anchor))
			return;

		SCR_AIGroup.IgnoreSpawning(true);
		SCR_AIGroup group = SpawnGroup(m_aPatrolGroups.GetRandomElement(), anchor);
		if (!group)
			return;

		COA_AAS_InsurgentGroup insurgents = new COA_AAS_InsurgentGroup();
		insurgents.m_Group = group;
		insurgents.m_vAnchor = anchor;

		// A loop of patrol points around the anchor
		array<AIWaypoint> loop = {};
		float start = Math.RandomFloat(0, Math.PI2);
		for (int i = 0; i < PATROL_POINTS; i++)
		{
			float angle = start + Math.PI2 * i / PATROL_POINTS;
			vector point = anchor + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * m_fPatrolRadius;
			vector spot;
			if (!FindSpot(territory, point, 0, m_fPatrolRadius * 0.3, spot, false))
				continue;

			AIWaypoint waypoint = SpawnWaypoint(WAYPOINT_PATROL, spot);
			if (waypoint)
			{
				loop.Insert(waypoint);
				insurgents.m_aWaypoints.Insert(waypoint);
			}
		}

		if (loop.Count() > 1)
		{
			AIWaypointCycle cycle = AIWaypointCycle.Cast(SpawnWaypoint(WAYPOINT_CYCLE, anchor));
			if (cycle)
			{
				cycle.SetWaypoints(loop);
				insurgents.m_aWaypoints.Insert(cycle);
				group.AddWaypoint(cycle);
			}
		}
		else if (loop.Count() == 1)
		{
			group.AddWaypoint(loop[0]);
		}

		// Soldiers only while players are near - the group manages that itself
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven, PATROL_SPAWN_DISTANCE, PATROL_DESPAWN_DISTANCE);
		if (!group.GetSpawnImmediately())
			group.RequestSpawn(-1, group.GetSpawnDistance());

		m_aGroups.Insert(insurgents);
	}

	//------------------------------------------------------------------------------------------------
	//! A group lying in wait beside a road in a side's rear: a random spot behind the front, the nearest
	//! road to it, a little way off that road
	protected void SpawnAmbush(notnull COA_AAS_Territory territory)
	{
		if (!m_aAmbushGroups || m_aAmbushGroups.IsEmpty())
			return;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		RoadNetworkManager roads = aiWorld.GetRoadNetworkManager();
		if (!roads)
			return;

		// Somewhere behind the front, in either side's territory
		vector rear = m_aFront.GetRandomElement();
		float angle = Math.RandomFloat(0, Math.PI2);
		rear = rear + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * Math.RandomFloat(m_fAmbushMinBehind, m_fAmbushMaxBehind);
		int owner = territory.OwnerAtPosition(rear);
		if (owner != COA_AAS_ETerritoryOwner.BLUFOR && owner != COA_AAS_ETerritoryOwner.OPFOR)
			return;

		// The road nearest it, and the point along that road nearest it
		BaseRoad road;
		float roadDistance;
		roads.GetClosestRoad(rear, road, roadDistance, true);
		if (!road || roadDistance > AMBUSH_ROAD_SEARCH || road.GetWidth() < m_fAmbushMinRoadWidth)
			return;

		array<vector> points = {};
		if (road.GetPoints(points) < 2)
			return;

		int nearest;
		float nearestSq = float.MAX;
		foreach (int i, vector point : points)
		{
			float distanceSq = vector.DistanceSqXZ(point, rear);
			if (distanceSq < nearestSq)
			{
				nearestSq = distanceSq;
				nearest = i;
			}
		}

		vector onRoad = points[nearest];
		int next = nearest + 1;
		if (next >= points.Count())
			next = nearest - 1;

		// Off to one side of the road
		vector along = points[next] - onRoad;
		along[1] = 0;
		along.Normalize();
		vector across = Vector(-along[2], 0, along[0]);
		if (Math.RandomInt(0, 2) == 0)
			across = across * -1;

		vector position = onRoad + across * Math.RandomFloat(AMBUSH_ROAD_OFFSET_MIN, AMBUSH_ROAD_OFFSET_MAX);

		// On the side's own ground, still behind the front, away from players and HQs
		if (territory.OwnerAtPosition(position) != owner || FrontDistanceSq(position) < m_fAmbushMinBehind * m_fAmbushMinBehind)
			return;

		if (PlayerWithin(position, m_fMinPlayerDistance) || NearHQ(position))
			return;

		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);
		if (position[1] <= world.GetOceanBaseHeight())
			return;

		vector clear;
		if (SCR_WorldTools.FindEmptyTerrainPosition(clear, position, 10, 1, 2))
			position = clear;

		SCR_AIGroup.IgnoreSpawning(true);
		SCR_AIGroup group = SpawnGroup(m_aAmbushGroups.GetRandomElement(), position);
		if (!group)
			return;

		COA_AAS_InsurgentGroup insurgents = new COA_AAS_InsurgentGroup();
		insurgents.m_Group = group;
		insurgents.m_vAnchor = position;
		insurgents.m_eRole = COA_AAS_EInsurgentRole.AMBUSH;
		insurgents.m_fSpawnedAt = GetGame().GetWorld().GetWorldTime();

		// Hold the roadside and fire on whatever comes along
		AIWaypoint hold = SpawnWaypoint(WAYPOINT_DEFEND_SMALL, position);
		if (hold)
		{
			insurgents.m_aWaypoints.Insert(hold);
			group.AddWaypoint(hold);
		}

		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven, PATROL_SPAWN_DISTANCE, PATROL_DESPAWN_DISTANCE);
		if (!group.GetSpawnImmediately())
			group.RequestSpawn(-1, group.GetSpawnDistance());

		m_aGroups.Insert(insurgents);
	}

	//------------------------------------------------------------------------------------------------
	protected bool NearHQ(vector position)
	{
		float rangeSq = AMBUSH_HQ_DISTANCE * AMBUSH_HQ_DISTANCE;
		array<SCR_ECampaignFaction> sides = {SCR_ECampaignFaction.BLUFOR, SCR_ECampaignFaction.OPFOR};
		foreach (SCR_ECampaignFaction side : sides)
		{
			SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(side);
			if (faction && faction.GetMainBase() && vector.DistanceSqXZ(faction.GetMainBase().GetOwner().GetOrigin(), position) < rangeSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return whether a group went in
	protected bool LaunchAttack(notnull COA_AAS_Territory territory)
	{
		if (!m_aAttackGroups || m_aAttackGroups.IsEmpty())
			return false;

		SCR_CampaignMilitaryBaseComponent target = PickTarget();
		if (!target)
			return false;

		vector targetPosition = target.GetOwner().GetOrigin();
		vector spot;
		if (!FindSpot(territory, targetPosition, m_fAttackDistance * 0.75, m_fAttackDistance * 1.25, spot))
			return false;

		SCR_AIGroup.IgnoreSpawning(true);
		SCR_AIGroup group = SpawnGroup(m_aAttackGroups.GetRandomElement(), spot);
		if (!group)
			return false;

		COA_AAS_InsurgentGroup insurgents = new COA_AAS_InsurgentGroup();
		insurgents.m_Group = group;
		insurgents.m_vAnchor = spot;
		insurgents.m_eRole = COA_AAS_EInsurgentRole.ATTACK;
		insurgents.m_fSpawnedAt = GetGame().GetWorld().GetWorldTime();

		AIWaypoint attack = SpawnWaypoint(WAYPOINT_SEARCH_AND_DESTROY, targetPosition);
		if (attack)
		{
			insurgents.m_aWaypoints.Insert(attack);
			group.AddWaypoint(attack);
		}

		// The whole group, through the spawn queue (retries tiles whose navmesh isn't loaded yet)
		group.SetDeleteWhenEmpty(true);
		int slots;
		if (group.m_aUnitPrefabSlots)
			slots = group.m_aUnitPrefabSlots.Count();

		group.RequestSpawn(Math.Max(slots, 1));
		m_aGroups.Insert(insurgents);

		m_AAS.NotifyInsurgentAttack(target.GetBaseName());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The live objective most often; else an objective or FOB a side holds near the front with players around
	protected SCR_CampaignMilitaryBaseComponent PickTarget()
	{
		array<SCR_CampaignMilitaryBaseComponent> targets = {};
		array<float> weights = {};
		SCR_CampaignMilitaryBaseComponent live = m_AAS.GetLiveObjective();
		if (live)
		{
			targets.Insert(live);
			weights.Insert(LIVE_OBJECTIVE_WEIGHT);
		}

		SCR_CampaignMilitaryBaseManager baseManager = m_Campaign.GetBaseManager();
		if (baseManager)
		{
			array<SCR_CampaignMilitaryBaseComponent> bases = {};
			baseManager.GetBases(bases);
			float frontSq = m_fAttackFrontRange * m_fAttackFrontRange;
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				if (!base || base == live || base.IsHQ() || !base.IsInitialized())
					continue;

				if (!m_AAS.IsObjective(base) && !base.GetBuiltByPlayers())
					continue;

				int owner = COA_AAS_Territory.OwnerOf(m_Campaign, base.GetFaction());
				if (owner != COA_AAS_ETerritoryOwner.BLUFOR && owner != COA_AAS_ETerritoryOwner.OPFOR)
					continue;

				vector position = base.GetOwner().GetOrigin();
				if (FrontDistanceSq(position) > frontSq || !PlayerWithin(position, TARGET_PLAYER_RANGE))
					continue;

				targets.Insert(base);
				weights.Insert(1);
			}
		}

		if (targets.IsEmpty())
			return null;

		float total;
		foreach (float weight : weights)
		{
			total += weight;
		}

		float roll = Math.RandomFloat(0, total);
		foreach (int i, float w : weights)
		{
			roll -= w;
			if (roll <= 0)
				return targets[i];
		}

		return targets[targets.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! A clear spot on land between minRange and maxRange of the centre, away from players (when asked)
	protected bool FindSpot(notnull COA_AAS_Territory territory, vector centre, float minRange, float maxRange, out vector spot, bool awayFromPlayers = true)
	{
		BaseWorld world = GetGame().GetWorld();
		for (int attempt = 0; attempt < 12; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float range = Math.RandomFloat(minRange, Math.Max(minRange, maxRange));
			vector candidate = centre + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * range;

			if (territory.OwnerAtPosition(candidate) == COA_AAS_ETerritoryOwner.NONE)
				continue;

			if (awayFromPlayers && PlayerWithin(candidate, m_fMinPlayerDistance))
				continue;

			candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
			if (candidate[1] <= world.GetOceanBaseHeight())
				continue;

			vector clear;
			if (SCR_WorldTools.FindEmptyTerrainPosition(clear, candidate, 25, 1, 2))
				spot = clear;
			else
				spot = candidate;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_AIGroup SpawnGroup(ResourceName prefab, vector position)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;

		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
		if (!group)
			SCR_AIGroup.IgnoreSpawning(false);

		return group;
	}

	//------------------------------------------------------------------------------------------------
	protected AIWaypoint SpawnWaypoint(ResourceName prefab, vector position)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return AIWaypoint.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, params: params));
	}

	//------------------------------------------------------------------------------------------------
	//! The group (if still there) and its waypoints
	protected void Remove(notnull COA_AAS_InsurgentGroup insurgents)
	{
		if (insurgents.m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(insurgents.m_Group);

		foreach (IEntity waypoint : insurgents.m_aWaypoints)
		{
			if (waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void GatherPlayers()
	{
		m_aPlayers.Clear();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player)
				m_aPlayers.Insert(player.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlayerWithin(vector position, float range)
	{
		float rangeSq = range * range;
		foreach (vector player : m_aPlayers)
		{
			if (vector.DistanceSqXZ(player, position) < rangeSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected float FrontDistanceSq(vector position)
	{
		float nearest = float.MAX;
		foreach (vector front : m_aFront)
		{
			nearest = Math.Min(nearest, vector.DistanceSqXZ(front, position));
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleAttack()
	{
		m_fNextAttack = GetGame().GetWorld().GetWorldTime() + Math.RandomFloat(m_fAttackIntervalMin, m_fAttackIntervalMax) * 1000;
	}
}
