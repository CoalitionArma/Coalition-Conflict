//------------------------------------------------------------------------------------------------
//! Server: the territory each side holds, as 100 m squares, and how it moves.
//!  - It covers the whole terrain (or just the bases and a margin round them). Sea squares belong to nobody,
//!    for good.
//!  - It starts split between the bases: each land square goes to whoever owns the nearest base (within the
//!    seed radius, if there is one) - a clean front halfway between enemy bases, right across the map.
//!  - A square only one side's living players stand in, next to that side's territory, is taken once
//!    they've held it for the capture time; defenders in it as well stop the clock, leaving resets it. The
//!    "next to" rule keeps the front moving as a line - a lone infiltrator doesn't paint islands.
//!  - A captured base takes the squares around it with it, and an objective or HQ its whole zone - every
//!    square nearer to it than to any other of them that the previous owner held - so the front jumps
//!    halfway to the next base instead of leaving the loser's ground stranded round it.
//! Owned by COA_AAS_ConflictComponent, which replicates the result (m_sTerritory) to everyone.
class COA_AAS_Frontline
{
	protected static const int MAX_CELLS = 40000;

	protected COA_AAS_ConflictComponent m_AAS;
	protected SCR_GameModeCampaign m_Campaign;
	protected ref COA_AAS_Territory m_Territory = new COA_AAS_Territory();
	protected ref array<bool> m_aLand = {};		// per cell: false = sea, never anyone's
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aSeedBases = {};	// the objectives and HQs
	protected ref array<int> m_aZone = {};		// per cell: index into m_aSeedBases of the nearest, -1 = none

	protected bool m_bWholeMap;
	protected float m_fMargin;
	protected float m_fSeedRadius;
	protected float m_fCaptureTime;
	protected float m_fBaseRadius;
	protected float m_fTick;

	// Squares being taken: cell -> taking side, and for how long [s]
	protected ref map<int, int> m_mTakingSide = new map<int, int>();
	protected ref map<int, float> m_mTakingTime = new map<int, float>();

	//------------------------------------------------------------------------------------------------
	void COA_AAS_Frontline(COA_AAS_ConflictComponent aas, SCR_GameModeCampaign campaign, float cellSize, bool wholeMap, float margin, float seedRadius, float captureTime, float baseRadius, float tick)
	{
		m_AAS = aas;
		m_Campaign = campaign;
		m_Territory.m_fCell = Math.Max(25, cellSize);
		m_bWholeMap = wholeMap;
		m_fMargin = margin;
		m_fSeedRadius = seedRadius;
		m_fCaptureTime = captureTime;
		m_fBaseRadius = baseRadius;
		m_fTick = Math.Max(1, tick);
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_Frontline()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Remove(OnBaseFactionChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] bases the objective chain and the HQs - they seed the grid (and bound it, if it isn't the whole map)
	void Start(notnull array<SCR_CampaignMilitaryBaseComponent> bases)
	{
		if (bases.IsEmpty())
			return;

		// The area: the whole terrain, or around every base by the margin
		vector minimum, maximum;
		if (m_bWholeMap)
		{
			GetGame().GetWorld().GetBoundBox(minimum, maximum);
		}
		else
		{
			minimum = bases[0].GetOwner().GetOrigin();
			maximum = minimum;
			foreach (SCR_CampaignMilitaryBaseComponent base : bases)
			{
				vector position = base.GetOwner().GetOrigin();
				minimum[0] = Math.Min(minimum[0], position[0]);
				minimum[2] = Math.Min(minimum[2], position[2]);
				maximum[0] = Math.Max(maximum[0], position[0]);
				maximum[2] = Math.Max(maximum[2], position[2]);
			}

			minimum[0] = minimum[0] - m_fMargin;
			minimum[2] = minimum[2] - m_fMargin;
			maximum[0] = maximum[0] + m_fMargin;
			maximum[2] = maximum[2] + m_fMargin;
		}

		// Too many squares for the area: make them bigger (in 25 m steps) rather than fail
		float cell = m_Territory.m_fCell;
		float area = (maximum[0] - minimum[0]) * (maximum[2] - minimum[2]);
		if (area / (cell * cell) > MAX_CELLS)
		{
			cell = Math.Ceil(Math.Sqrt(area / MAX_CELLS) / 25) * 25;
			while (Math.Ceil((maximum[0] - minimum[0]) / cell + 1) * Math.Ceil((maximum[2] - minimum[2]) / cell + 1) > MAX_CELLS)
			{
				cell += 25;
			}

			Print(string.Format("[COA_AAS] Frontline: %1 m squares are too many for this area - using %2 m", m_Territory.m_fCell, cell), LogLevel.WARNING);
			m_Territory.m_fCell = cell;
		}

		m_Territory.m_fOriginX = Math.Floor(minimum[0] / cell) * cell;
		m_Territory.m_fOriginZ = Math.Floor(minimum[2] / cell) * cell;
		m_Territory.m_iWidth = Math.Ceil((maximum[0] - m_Territory.m_fOriginX) / cell);
		m_Territory.m_iHeight = Math.Ceil((maximum[2] - m_Territory.m_fOriginZ) / cell);

		// Seed: each land square to the owner of the nearest base (in reach, if there's a seed radius)
		float seedSq = float.MAX;
		if (m_fSeedRadius > 0)
			seedSq = m_fSeedRadius * m_fSeedRadius;

		m_aSeedBases.Clear();
		m_aSeedBases.InsertAll(bases);
		m_Territory.m_aOwners.Clear();
		m_aLand.Clear();
		m_aZone.Clear();
		for (int z = 0; z < m_Territory.m_iHeight; z++)
		{
			for (int x = 0; x < m_Territory.m_iWidth; x++)
			{
				vector centre = m_Territory.CellCentre(x, z);
				int owner = COA_AAS_ETerritoryOwner.NONE;
				bool land = IsLand(x, z);
				m_aLand.Insert(land);
				if (!land)
				{
					m_Territory.m_aOwners.Insert(owner);
					m_aZone.Insert(-1);
					continue;
				}

				float nearest = seedSq;
				int zone = -1;
				foreach (int i, SCR_CampaignMilitaryBaseComponent seedBase : bases)
				{
					vector basePosition = seedBase.GetOwner().GetOrigin();
					float dx = basePosition[0] - centre[0];
					float dz = basePosition[2] - centre[2];
					float distance = dx * dx + dz * dz;
					if (distance < nearest)
					{
						nearest = distance;
						owner = OwnerCode(seedBase.GetFaction());
						zone = i;
					}
				}

				m_Territory.m_aOwners.Insert(owner);
				m_aZone.Insert(zone);
			}
		}

		Print(string.Format("[COA_AAS] Frontline: %1 x %2 cells of %3 m", m_Territory.m_iWidth, m_Territory.m_iHeight, cell), LogLevel.NORMAL);
		Publish();

		SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Insert(OnBaseFactionChanged);
		GetGame().GetCallqueue().CallLater(Tick, m_fTick * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the living players stand, and what that takes
	protected void Tick()
	{
		if (m_Territory.m_aOwners.IsEmpty())
			return;

		// Cell -> bit per side standing in it, and who (for the XP when it's taken)
		map<int, int> presence = new map<int, int>();
		map<int, ref array<int>> occupants = new map<int, ref array<int>>();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
			if (!character)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.IsDead() || controller.IsUnconscious())
				continue;

			int side = OwnerCode(character.GetFaction());
			int x, z;
			if (side == COA_AAS_ETerritoryOwner.NONE || !m_Territory.CellAt(character.GetOrigin(), x, z))
				continue;

			int cell = m_Territory.Index(x, z);
			presence.Set(cell, presence.Get(cell) | (1 << side));

			array<int> inCell = occupants.Get(cell);
			if (!inCell)
			{
				inCell = {};
				occupants.Set(cell, inCell);
			}

			inCell.Insert(playerId);
		}

		map<int, int> takingSide = new map<int, int>();
		map<int, float> takingTime = new map<int, float>();
		bool changed;
		foreach (int cell, int sides : presence)
		{
			int side = SoleSide(sides);
			if (side == COA_AAS_ETerritoryOwner.NONE || m_Territory.m_aOwners[cell] == side || !m_aLand[cell])
				continue;

			int x = cell % m_Territory.m_iWidth;
			int z = cell / m_Territory.m_iWidth;
			if (!Borders(x, z, side))
				continue;

			float held = m_fTick;
			if (m_mTakingSide.Get(cell) == side)
				held += m_mTakingTime.Get(cell);

			if (held >= m_fCaptureTime)
			{
				m_Territory.m_aOwners[cell] = side;
				changed = true;

				// Only that side stands there, so everyone in it took it
				m_AAS.OnSquareTaken(occupants.Get(cell));
				continue;
			}

			takingSide.Set(cell, side);
			takingTime.Set(cell, held);
		}

		// Squares nobody is still taking start over
		m_mTakingSide = takingSide;
		m_mTakingTime = takingTime;

		if (changed)
			Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! The one side in a presence mask, NONE when it's nobody or contested
	protected int SoleSide(int sides)
	{
		for (int side = COA_AAS_ETerritoryOwner.BLUFOR; side <= COA_AAS_ETerritoryOwner.INDFOR; side++)
		{
			if (sides == (1 << side))
				return side;
		}

		return COA_AAS_ETerritoryOwner.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Is any of a square above the sea: its centre or a point near each corner (so the beach counts)
	protected bool IsLand(int x, int z)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world.IsOcean())
			return true;

		float sea = world.GetOceanBaseHeight();
		float cell = m_Territory.m_fCell;
		float x0 = m_Territory.m_fOriginX + x * cell;
		float z0 = m_Territory.m_fOriginZ + z * cell;
		// Centre, then the four corners (as fractions of the square)
		array<float> u = {0.5, 0.15, 0.85, 0.15, 0.85};
		array<float> v = {0.5, 0.15, 0.15, 0.85, 0.85};
		foreach (int sample, float fraction : u)
		{
			if (world.GetSurfaceY(x0 + fraction * cell, z0 + v[sample] * cell) > sea)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Does a square touch the side's territory (edge to edge)
	protected bool Borders(int x, int z, int side)
	{
		return m_Territory.OwnerAt(x - 1, z) == side || m_Territory.OwnerAt(x + 1, z) == side
			|| m_Territory.OwnerAt(x, z - 1) == side || m_Territory.OwnerAt(x, z + 1) == side;
	}

	//------------------------------------------------------------------------------------------------
	//! A base changed hands: the squares around it go with it, and for an objective or HQ, its zone
	protected void OnBaseFactionChanged(SCR_CampaignMilitaryBaseComponent base, Faction previousFaction, Faction faction)
	{
		int side = OwnerCode(faction);
		if (side == COA_AAS_ETerritoryOwner.NONE || m_Territory.m_aOwners.IsEmpty())
			return;

		bool changed = TakeZone(base, OwnerCode(previousFaction), side);

		vector position = base.GetOwner().GetOrigin();
		int reach = Math.Ceil(m_fBaseRadius / m_Territory.m_fCell);
		int cx, cz;
		m_Territory.CellAt(position, cx, cz);
		float radiusSq = m_fBaseRadius * m_fBaseRadius;
		for (int z = cz - reach; z <= cz + reach; z++)
		{
			for (int x = cx - reach; x <= cx + reach; x++)
			{
				if (x < 0 || z < 0 || x >= m_Territory.m_iWidth || z >= m_Territory.m_iHeight)
					continue;

				vector centre = m_Territory.CellCentre(x, z);
				float dx = centre[0] - position[0];
				float dz = centre[2] - position[2];
				if (dx * dx + dz * dz > radiusSq)
					continue;

				int cell = m_Territory.Index(x, z);
				if (m_Territory.m_aOwners[cell] == side || !m_aLand[cell])
					continue;

				m_Territory.m_aOwners[cell] = side;
				changed = true;
			}
		}

		if (changed)
			Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! The squares of an objective's or HQ's zone that the previous owner held go to the captor. Ground
	//! another side had pushed into the zone stays theirs.
	//! \return whether any square changed
	protected bool TakeZone(notnull SCR_CampaignMilitaryBaseComponent base, int previousSide, int side)
	{
		int zone = m_aSeedBases.Find(base);
		if (zone < 0 || previousSide == COA_AAS_ETerritoryOwner.NONE || previousSide == side)
			return false;

		bool changed;
		foreach (int cell, int cellZone : m_aZone)
		{
			if (cellZone != zone || m_Territory.m_aOwners[cell] != previousSide)
				continue;

			m_Territory.m_aOwners[cell] = side;
			changed = true;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish()
	{
		m_AAS.SetTerritory(m_Territory.Encode());
	}

	//------------------------------------------------------------------------------------------------
	protected int OwnerCode(Faction faction)
	{
		return COA_AAS_Territory.OwnerOf(m_Campaign, faction);
	}
}
