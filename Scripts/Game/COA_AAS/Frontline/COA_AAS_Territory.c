//------------------------------------------------------------------------------------------------
//! Owner codes of a territory cell
enum COA_AAS_ETerritoryOwner
{
	NONE,
	BLUFOR,
	OPFOR,
	INDFOR
}

//------------------------------------------------------------------------------------------------
//! The territory grid both sides see on the map: square cells over the battle area, each held by a side.
//! The server keeps it (COA_AAS_Frontline) and replicates it as a short run-length string
//! (COA_AAS_ConflictComponent.m_sTerritory); clients decode it to draw (COA_AAS_FrontlineMap).
//!
//!   "1;<originX>;<originZ>;<cell>;<width>;<height>;<owner>:<count>,<owner>:<count>,..."   cells row by row,
//!   from the south-west corner (origin) - x east, z north
class COA_AAS_Territory
{
	protected static const string VERSION = "1";

	float m_fOriginX;
	float m_fOriginZ;
	float m_fCell = 100;
	int m_iWidth;
	int m_iHeight;
	ref array<int> m_aOwners = {};

	//------------------------------------------------------------------------------------------------
	//! COA_AAS_ETerritoryOwner of a faction, NONE for anyone else
	static int OwnerOf(SCR_GameModeCampaign campaign, Faction faction)
	{
		if (!faction || !campaign)
			return COA_AAS_ETerritoryOwner.NONE;

		if (faction == campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR))
			return COA_AAS_ETerritoryOwner.BLUFOR;

		if (faction == campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR))
			return COA_AAS_ETerritoryOwner.OPFOR;

		if (faction == campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR))
			return COA_AAS_ETerritoryOwner.INDFOR;

		return COA_AAS_ETerritoryOwner.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! COA_AAS_ETerritoryOwner at a world position, NONE outside the grid
	int OwnerAtPosition(vector position)
	{
		int x, z;
		if (!CellAt(position, x, z))
			return COA_AAS_ETerritoryOwner.NONE;

		return m_aOwners[Index(x, z)];
	}

	//------------------------------------------------------------------------------------------------
	int Index(int x, int z)
	{
		return z * m_iWidth + x;
	}

	//------------------------------------------------------------------------------------------------
	//! COA_AAS_ETerritoryOwner of a cell, NONE outside the grid
	int OwnerAt(int x, int z)
	{
		if (x < 0 || z < 0 || x >= m_iWidth || z >= m_iHeight)
			return COA_AAS_ETerritoryOwner.NONE;

		return m_aOwners[Index(x, z)];
	}

	//------------------------------------------------------------------------------------------------
	//! The cell a world position falls in
	bool CellAt(vector position, out int x, out int z)
	{
		x = Math.Floor((position[0] - m_fOriginX) / m_fCell);
		z = Math.Floor((position[2] - m_fOriginZ) / m_fCell);
		return x >= 0 && z >= 0 && x < m_iWidth && z < m_iHeight;
	}

	//------------------------------------------------------------------------------------------------
	//! Per cell: how far it is from the nearest cell the side holds, in cells (0 inside it). Two-pass
	//! chamfer distance (1 along, 1.414 diagonally) - close to straight-line distance, and cheap.
	void DistanceFrom(int side, notnull array<float> field)
	{
		int total = m_iWidth * m_iHeight;
		float far = m_iWidth + m_iHeight;
		field.Clear();
		for (int i = 0; i < total; i++)
		{
			if (m_aOwners[i] == side)
				field.Insert(0);
			else
				field.Insert(far);
		}

		float diagonal = 1.414;
		for (int z = 0; z < m_iHeight; z++)
		{
			for (int x = 0; x < m_iWidth; x++)
			{
				int cell = Index(x, z);
				float best = field[cell];
				if (x > 0)
					best = Math.Min(best, field[cell - 1] + 1);
				if (z > 0)
				{
					best = Math.Min(best, field[cell - m_iWidth] + 1);
					if (x > 0)
						best = Math.Min(best, field[cell - m_iWidth - 1] + diagonal);
					if (x + 1 < m_iWidth)
						best = Math.Min(best, field[cell - m_iWidth + 1] + diagonal);
				}

				field[cell] = best;
			}
		}

		for (int z2 = m_iHeight - 1; z2 >= 0; z2--)
		{
			for (int x2 = m_iWidth - 1; x2 >= 0; x2--)
			{
				int cell2 = Index(x2, z2);
				float best2 = field[cell2];
				if (x2 + 1 < m_iWidth)
					best2 = Math.Min(best2, field[cell2 + 1] + 1);
				if (z2 + 1 < m_iHeight)
				{
					best2 = Math.Min(best2, field[cell2 + m_iWidth] + 1);
					if (x2 + 1 < m_iWidth)
						best2 = Math.Min(best2, field[cell2 + m_iWidth + 1] + diagonal);
					if (x2 > 0)
						best2 = Math.Min(best2, field[cell2 + m_iWidth - 1] + diagonal);
				}

				field[cell2] = best2;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	vector CellCentre(int x, int z)
	{
		return Vector(m_fOriginX + (x + 0.5) * m_fCell, 0, m_fOriginZ + (z + 0.5) * m_fCell);
	}

	//------------------------------------------------------------------------------------------------
	string Encode()
	{
		string data = string.Format("%1;%2;%3;%4;%5;%6;", VERSION, Math.Round(m_fOriginX), Math.Round(m_fOriginZ), Math.Round(m_fCell), m_iWidth, m_iHeight);
		int count = m_aOwners.Count();
		int i = 0;
		while (i < count)
		{
			int owner = m_aOwners[i];
			int run = 1;
			while (i + run < count && m_aOwners[i + run] == owner)
			{
				run++;
			}

			data += owner.ToString() + ":" + run.ToString() + ",";
			i += run;
		}

		return data;
	}

	//------------------------------------------------------------------------------------------------
	bool Decode(string data)
	{
		array<string> parts = {};
		data.Split(";", parts, false);
		if (parts.Count() < 7 || parts[0] != VERSION)
			return false;

		m_fOriginX = parts[1].ToFloat();
		m_fOriginZ = parts[2].ToFloat();
		m_fCell = parts[3].ToFloat();
		m_iWidth = parts[4].ToInt();
		m_iHeight = parts[5].ToInt();
		if (m_fCell <= 0 || m_iWidth <= 0 || m_iHeight <= 0)
			return false;

		int total = m_iWidth * m_iHeight;
		m_aOwners.Clear();

		array<string> runs = {};
		parts[6].Split(",", runs, true);
		foreach (string run : runs)
		{
			int colon = run.IndexOf(":");
			if (colon < 0)
				continue;

			int owner = run.Substring(0, colon).ToInt();
			int length = run.Substring(colon + 1, run.Length() - colon - 1).ToInt();
			for (int n = 0; n < length && m_aOwners.Count() < total; n++)
			{
				m_aOwners.Insert(owner);
			}
		}

		// Short data: the rest is nobody's
		while (m_aOwners.Count() < total)
		{
			m_aOwners.Insert(COA_AAS_ETerritoryOwner.NONE);
		}

		return true;
	}
}
