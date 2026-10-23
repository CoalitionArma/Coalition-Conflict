//------------------------------------------------------------------------------------------------
//! Server: keeps the fighting at the front by giving away players deep in enemy territory.
//!  - Within the grace distance past their own side's territory nothing happens - flanking the front is fine.
//!  - Beyond it the player is told they're behind enemy lines, and from then on their rough position is
//!    pinged on the enemy's map (COA_AAS_FrontlineMap) - every max interval just past the grace distance,
//!    down to every min interval at the deep distance and beyond.
//!  - Never in an aircraft (inserting, resupplying), near the live objective, on the way from their side's
//!    ground to it or to the enemy's front-line objective (the ground between that base and the front has
//!    to be crossed to fight there - past it is behind their lines), or on CHDKz ground - only the other
//!    side's territory counts.
//! Players only, alive and conscious; needs the frontline's territory (COA_AAS_ConflictComponent).
class COA_AAS_BehindLines
{
	protected static const float PING_JITTER = 60;	// the ping is up to this far off the player [m]

	protected COA_AAS_ConflictComponent m_AAS;
	protected SCR_GameModeCampaign m_Campaign;
	protected float m_fGrace;
	protected float m_fDeep;
	protected float m_fMaxInterval;
	protected float m_fMinInterval;
	protected float m_fObjectiveRadius;
	protected float m_fApproachSlack;
	protected float m_fTick;

	// Distance of every cell from each side's territory [cells], rebuilt when the territory changes
	protected ref map<int, ref array<float>> m_mDistance = new map<int, ref array<float>>();
	protected int m_iTerritoryVersion = -1;

	// Per side: where attackers head through its ground - the live objective and its front-line objectives
	protected ref map<int, ref array<vector>> m_mApproaches = new map<int, ref array<vector>>();

	// Players behind the lines -> seconds to their next ping
	protected ref map<int, float> m_mNextPing = new map<int, float>();

	//------------------------------------------------------------------------------------------------
	void COA_AAS_BehindLines(COA_AAS_ConflictComponent aas, SCR_GameModeCampaign campaign, float grace, float deep, float maxInterval, float minInterval, float objectiveRadius, float approachSlack, float tick)
	{
		m_AAS = aas;
		m_Campaign = campaign;
		m_fGrace = grace;
		m_fDeep = Math.Max(deep, grace + 1);
		m_fMaxInterval = Math.Max(1, maxInterval);
		m_fMinInterval = Math.Clamp(minInterval, 1, m_fMaxInterval);
		m_fObjectiveRadius = objectiveRadius;
		m_fApproachSlack = Math.Max(0, approachSlack);
		m_fTick = Math.Max(1, tick);

		GetGame().GetCallqueue().CallLater(Tick, m_fTick * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_BehindLines()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
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
			m_mDistance.Clear();
			array<int> sides = {COA_AAS_ETerritoryOwner.BLUFOR, COA_AAS_ETerritoryOwner.OPFOR, COA_AAS_ETerritoryOwner.INDFOR};
			foreach (int side : sides)
			{
				array<float> field = {};
				territory.DistanceFrom(side, field);
				m_mDistance.Set(side, field);
			}
		}

		SCR_CampaignMilitaryBaseComponent live = m_AAS.GetLiveObjective();
		float objectiveRadiusSq = m_fObjectiveRadius * m_fObjectiveRadius;
		BuildApproaches(live);

		map<int, float> nextPing = new map<int, float>();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
			float depth = Depth(territory, character, live, objectiveRadiusSq);
			bool wasBehind = m_mNextPing.Contains(playerId);
			if (depth <= m_fGrace)
			{
				// Back on their side (not dead or gone - that just ends it quietly)
				if (wasBehind && depth >= 0)
					m_AAS.NotifyBehindLines(playerId, false);

				continue;
			}

			// Deeper pings sooner: a countdown already running is cut short to the new interval
			float interval = Interval(depth);
			float remaining = interval;
			if (wasBehind)
				remaining = Math.Min(m_mNextPing.Get(playerId), interval) - m_fTick;
			else
				m_AAS.NotifyBehindLines(playerId, true);

			if (remaining <= 0)
			{
				Ping(character);
				remaining = interval;
			}

			nextPing.Set(playerId, remaining);
		}

		// Anyone gone (left, died, back home) is forgotten
		m_mNextPing = nextPing;
	}

	//------------------------------------------------------------------------------------------------
	//! How far past their side's territory the player stands [m], 0 when it doesn't count there,
	//! -1 when there's no living, conscious character
	protected float Depth(notnull COA_AAS_Territory territory, SCR_ChimeraCharacter character, SCR_CampaignMilitaryBaseComponent live, float objectiveRadiusSq)
	{
		if (!character)
			return -1;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead() || controller.IsUnconscious())
			return -1;

		int side = COA_AAS_Territory.OwnerOf(m_Campaign, character.GetFaction());
		array<float> field = m_mDistance.Get(side);
		if (!field)
			return 0;

		// Only the other side's ground - not CHDKz ground (pushing into it is the fight), the sea, or
		// anywhere off the grid
		vector position = character.GetOrigin();
		int x, z;
		if (!territory.CellAt(position, x, z))
			return 0;

		int owner = territory.OwnerAt(x, z);
		bool enemySide = owner == COA_AAS_ETerritoryOwner.BLUFOR || owner == COA_AAS_ETerritoryOwner.OPFOR;
		if (!enemySide || owner == side)
			return 0;

		if (live && vector.DistanceSqXZ(position, live.GetOwner().GetOrigin()) <= objectiveRadiusSq)
			return 0;

		if (IsInAircraft(character))
			return 0;

		float depth = DepthAt(field, territory.Index(x, z)) * territory.m_fCell;

		// On the way to an objective: a straight push from their ground covers the objective's own depth, so
		// going in that way adds up to about it - detours add more, and going past the objective adds twice
		// the overshoot
		array<vector> approaches = m_mApproaches.Get(owner);
		if (approaches)
		{
			foreach (vector objective : approaches)
			{
				int objectiveX, objectiveZ;
				if (!territory.CellAt(objective, objectiveX, objectiveZ))
					continue;

				float objectiveDepth = DepthAt(field, territory.Index(objectiveX, objectiveZ)) * territory.m_fCell;
				if (depth + vector.DistanceXZ(position, objective) <= objectiveDepth + m_fApproachSlack)
					return 0;
			}
		}

		return depth;
	}

	//------------------------------------------------------------------------------------------------
	//! Per side, the objectives attackers cross its ground to reach: the live one, and each objective it holds
	//! next to one it doesn't along the line (its front-line base)
	protected void BuildApproaches(SCR_CampaignMilitaryBaseComponent live)
	{
		m_mApproaches.Clear();
		array<int> sides = {COA_AAS_ETerritoryOwner.BLUFOR, COA_AAS_ETerritoryOwner.OPFOR};
		int count = m_AAS.GetChainCount();
		foreach (int side : sides)
		{
			array<vector> objectives = {};
			if (live)
				objectives.Insert(live.GetOwner().GetOrigin());

			for (int i = 0; i < count; i++)
			{
				SCR_CampaignMilitaryBaseComponent base = m_AAS.GetChainBase(i);
				if (!base || base == live || COA_AAS_Territory.OwnerOf(m_Campaign, base.GetFaction()) != side)
					continue;

				SCR_CampaignMilitaryBaseComponent previous = m_AAS.GetChainBase(i - 1);
				SCR_CampaignMilitaryBaseComponent next = m_AAS.GetChainBase(i + 1);
				if ((previous && COA_AAS_Territory.OwnerOf(m_Campaign, previous.GetFaction()) != side)
					|| (next && COA_AAS_Territory.OwnerOf(m_Campaign, next.GetFaction()) != side))
					objectives.Insert(base.GetOwner().GetOrigin());
			}

			m_mApproaches.Set(side, objectives);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Distance of a cell from the nearest ground that isn't the enemy's - the side's own or CHDKz - so
	//! crossing CHDKz ground doesn't count toward the depth [cells]
	protected float DepthAt(notnull array<float> field, int cell)
	{
		float depth = field[cell];
		array<float> insurgentField = m_mDistance.Get(COA_AAS_ETerritoryOwner.INDFOR);
		if (insurgentField)
			depth = Math.Min(depth, insurgentField[cell]);

		return depth;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInAircraft(notnull SCR_ChimeraCharacter character)
	{
		if (!character.IsInVehicle())
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return false;

		IEntity vehicle = access.GetVehicleIn(character);
		if (!vehicle)
			return false;

		return vehicle.FindComponent(HelicopterControllerComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds between pings at a depth: max interval just past the grace distance, min from the deep distance
	protected float Interval(float depth)
	{
		float t = Math.Clamp((depth - m_fGrace) / (m_fDeep - m_fGrace), 0, 1);
		return Math.Lerp(m_fMaxInterval, m_fMinInterval, t);
	}

	//------------------------------------------------------------------------------------------------
	//! A rough position - somewhere within PING_JITTER of the player
	protected void Ping(notnull SCR_ChimeraCharacter character)
	{
		vector position = character.GetOrigin();
		float angle = Math.RandomFloat(0, Math.PI2);
		float offset = Math.RandomFloat(0, PING_JITTER);
		position[0] = position[0] + Math.Cos(angle) * offset;
		position[2] = position[2] + Math.Sin(angle) * offset;

		m_AAS.RevealBehindLines(COA_AAS_Territory.OwnerOf(m_Campaign, character.GetFaction()), position);
	}
}
