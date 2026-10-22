//------------------------------------------------------------------------------------------------
//! A stretch of territory edge, in grid corners
class COA_AAS_TerritoryEdge
{
	int m_iX0;
	int m_iZ0;
	int m_iX1;
	int m_iZ1;
	bool m_bFront;		// between two sides (else the edge of the held area)
}

//------------------------------------------------------------------------------------------------
//! A row of squares one side holds, in grid cells
class COA_AAS_TerritoryRun
{
	int m_iOwner;
	int m_iX0;
	int m_iX1;		// inclusive
	int m_iZ;
}

//------------------------------------------------------------------------------------------------
//! An enemy reported behind our lines (COA_AAS_BehindLines), roughly here
class COA_AAS_MapPing
{
	vector m_vPosition;
	int m_iSide;
	float m_fStart;		// world time [ms]
	float m_fEnd;
}

//------------------------------------------------------------------------------------------------
//! Draws the territory (COA_AAS_Frontline) on every map: each side's squares tinted in its colour, a heavy
//! line where two sides meet (the front) and a thin one round the edge of the held area. A canvas in the
//! map's drawing container - under the icons, like map drawings - redrawn when the map pans or zooms or
//! the territory changes. Squares are merged into rows and edges into straight stretches, so a whole
//! map is a few hundred shapes.
class COA_AAS_FrontlineMap
{
	protected static const ResourceName CANVAS_LAYOUT = "{6C0F4A1D2E3B5C71}UI/layouts/COA_AAS/COA_AAS_TerritoryCanvas.layout";
	protected static const float FILL_ALPHA = 0.28;
	protected static const float FRONT_WIDTH = 4;
	protected static const float EDGE_WIDTH = 1.5;
	protected static const int FRONT_COLOR = 0xE0101010;
	protected static const int EDGE_COLOR = 0x90101010;
	protected static const float PING_RADIUS = 120;		// [m] - the ping is only rough
	protected static const float PING_FILL_ALPHA = 0.35;
	protected static const float PING_RING_WIDTH = 2;
	protected static const float PING_PULSE_MS = 1500;
	protected static const int PING_SEGMENTS = 24;

	protected ref array<ref COA_AAS_MapPing> m_aPings = {};

	protected ref COA_AAS_Territory m_Territory = new COA_AAS_Territory();
	protected bool m_bHasTerritory;
	protected ref array<ref COA_AAS_TerritoryRun> m_aRuns = {};
	protected ref array<ref COA_AAS_TerritoryEdge> m_aEdges = {};

	protected CanvasWidget m_wCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	protected ref array<ref CanvasWidgetCommand> m_aTerritoryCommands = {};
	protected bool m_bDirty;
	protected float m_fLastX;
	protected float m_fLastY;
	protected float m_fLastScaleX;
	protected float m_fLastScaleY;

	//------------------------------------------------------------------------------------------------
	void COA_AAS_FrontlineMap()
	{
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_FrontlineMap()
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Draw);

		RemoveCanvas();
	}

	//------------------------------------------------------------------------------------------------
	//! New territory from the server (COA_AAS_ConflictComponent.m_sTerritory)
	void SetTerritory(string data)
	{
		m_bHasTerritory = !data.IsEmpty() && m_Territory.Decode(data);
		BuildShapes();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! An enemy of the side is behind our lines, roughly here - shown for duration seconds, fading out
	void AddPing(vector position, int side, float duration)
	{
		COA_AAS_MapPing ping = new COA_AAS_MapPing();
		ping.m_vPosition = position;
		ping.m_iSide = side;
		ping.m_fStart = GetGame().GetWorld().GetWorldTime();
		ping.m_fEnd = ping.m_fStart + duration * 1000;
		m_aPings.Insert(ping);
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapOpen(MapConfiguration config)
	{
		RemoveCanvas();
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.GetMapMenuRoot())
			return;

		Widget parent = mapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.DRAWING_CONTAINER_WIDGET_NAME);
		if (!parent)
			parent = mapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!parent)
			return;

		m_wCanvas = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidgets(CANVAS_LAYOUT, parent));
		if (!m_wCanvas)
			return;

		// Under anything drawn on the map
		m_wCanvas.SetZOrder(-100);
		m_bDirty = true;
		GetGame().GetCallqueue().Remove(Draw);
		GetGame().GetCallqueue().CallLater(Draw, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapClose(MapConfiguration config)
	{
		GetGame().GetCallqueue().Remove(Draw);
		RemoveCanvas();
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveCanvas()
	{
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();

		m_wCanvas = null;
		m_aCommands.Clear();
	}

	// ── Shapes (grid units, rebuilt when the territory changes) ───────────────────────────────────
	//------------------------------------------------------------------------------------------------
	protected void BuildShapes()
	{
		m_aRuns.Clear();
		m_aEdges.Clear();
		if (!m_bHasTerritory)
			return;

		int width = m_Territory.m_iWidth;
		int height = m_Territory.m_iHeight;

		// Rows of one owner
		for (int z = 0; z < height; z++)
		{
			int x = 0;
			while (x < width)
			{
				int owner = m_Territory.OwnerAt(x, z);
				int end = x;
				while (end + 1 < width && m_Territory.OwnerAt(end + 1, z) == owner)
				{
					end++;
				}

				if (owner != COA_AAS_ETerritoryOwner.NONE)
				{
					COA_AAS_TerritoryRun run = new COA_AAS_TerritoryRun();
					run.m_iOwner = owner;
					run.m_iX0 = x;
					run.m_iX1 = end;
					run.m_iZ = z;
					m_aRuns.Insert(run);
				}

				x = end + 1;
			}
		}

		// Horizontal edges: between row z - 1 and row z, along the corner line z
		for (int lineZ = 0; lineZ <= height; lineZ++)
		{
			int startX = -1;
			bool startFront;
			for (int edgeX = 0; edgeX <= width; edgeX++)
			{
				int kind = 0;		// 0 no edge, 1 edge of the held area, 2 front
				if (edgeX < width)
					kind = EdgeKind(m_Territory.OwnerAt(edgeX, lineZ - 1), m_Territory.OwnerAt(edgeX, lineZ));

				bool front = kind == 2;
				if (startX >= 0 && (kind == 0 || front != startFront))
				{
					AddEdge(startX, lineZ, edgeX, lineZ, startFront);
					startX = -1;
				}

				if (kind != 0 && startX < 0)
				{
					startX = edgeX;
					startFront = front;
				}
			}
		}

		// Vertical edges: between column x - 1 and column x, along the corner line x
		for (int lineX = 0; lineX <= width; lineX++)
		{
			int startZ = -1;
			bool startFrontZ;
			for (int edgeZ = 0; edgeZ <= height; edgeZ++)
			{
				int kindZ = 0;
				if (edgeZ < height)
					kindZ = EdgeKind(m_Territory.OwnerAt(lineX - 1, edgeZ), m_Territory.OwnerAt(lineX, edgeZ));

				bool frontZ = kindZ == 2;
				if (startZ >= 0 && (kindZ == 0 || frontZ != startFrontZ))
				{
					AddEdge(lineX, startZ, lineX, edgeZ, startFrontZ);
					startZ = -1;
				}

				if (kindZ != 0 && startZ < 0)
				{
					startZ = edgeZ;
					startFrontZ = frontZ;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 0 no edge, 1 the edge of the held area, 2 the front between two sides
	protected int EdgeKind(int a, int b)
	{
		if (a == b)
			return 0;

		if (a != COA_AAS_ETerritoryOwner.NONE && b != COA_AAS_ETerritoryOwner.NONE)
			return 2;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEdge(int x0, int z0, int x1, int z1, bool front)
	{
		COA_AAS_TerritoryEdge edge = new COA_AAS_TerritoryEdge();
		edge.m_iX0 = x0;
		edge.m_iZ0 = z0;
		edge.m_iX1 = x1;
		edge.m_iZ1 = z1;
		edge.m_bFront = front;
		m_aEdges.Insert(edge);
	}

	// ── Drawing (screen units, when the view or the territory changes) ────────────────────────────
	//------------------------------------------------------------------------------------------------
	protected void Draw()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!m_wCanvas || !mapEntity || !mapEntity.IsOpen())
			return;

		if (!m_bHasTerritory)
		{
			if (m_bDirty)
			{
				m_aCommands.Clear();
				m_wCanvas.SetDrawCommands(m_aCommands);
				m_bDirty = false;
			}
			return;
		}

		// The map isn't rotated: a grid corner's screen position is origin + corner * scale
		float canvasX, canvasY;
		m_wCanvas.GetScreenPos(canvasX, canvasY);
		int originX, originY, farX, farY;
		float span = 1000;
		mapEntity.WorldToScreen(m_Territory.m_fOriginX, m_Territory.m_fOriginZ, originX, originY, true);
		mapEntity.WorldToScreen(m_Territory.m_fOriginX + span, m_Territory.m_fOriginZ + span, farX, farY, true);

		float baseX = originX - canvasX;
		float baseY = originY - canvasY;
		float scaleX = (farX - originX) / span * m_Territory.m_fCell;
		float scaleY = (farY - originY) / span * m_Territory.m_fCell;
		bool moved = baseX != m_fLastX || baseY != m_fLastY || scaleX != m_fLastScaleX || scaleY != m_fLastScaleY;
		if (!m_bDirty && !moved && m_aPings.IsEmpty())
			return;

		// The territory only when it or the view changed - pings animate every frame on top of it
		if (m_bDirty || moved)
			BuildTerritoryCommands(baseX, baseY, scaleX, scaleY);

		m_bDirty = false;
		m_fLastX = baseX;
		m_fLastY = baseY;
		m_fLastScaleX = scaleX;
		m_fLastScaleY = scaleY;

		m_aCommands.Clear();
		foreach (CanvasWidgetCommand command : m_aTerritoryCommands)
		{
			m_aCommands.Insert(command);
		}

		AddPingCommands(baseX, baseY, scaleX, scaleY);
		m_wCanvas.SetDrawCommands(m_aCommands);
	}

	//------------------------------------------------------------------------------------------------
	//! Every live ping: a disc in the side's colour with a ring pulsing out of it, fading out over its life
	protected void AddPingCommands(float baseX, float baseY, float scaleX, float scaleY)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = m_aPings.Count() - 1; i >= 0; i--)
		{
			if (now >= m_aPings[i].m_fEnd)
				m_aPings.Remove(i);
		}

		// Metres to screen units (grid scale is per cell)
		float perMetreX = scaleX / m_Territory.m_fCell;
		float perMetreY = scaleY / m_Territory.m_fCell;
		foreach (COA_AAS_MapPing ping : m_aPings)
		{
			float centreX = baseX + (ping.m_vPosition[0] - m_Territory.m_fOriginX) * perMetreX;
			float centreY = baseY + (ping.m_vPosition[2] - m_Territory.m_fOriginZ) * perMetreY;
			float life = 1 - (now - ping.m_fStart) / (ping.m_fEnd - ping.m_fStart);
			float pulse = Math.Mod(now - ping.m_fStart, PING_PULSE_MS) / PING_PULSE_MS;

			Color color = Color.FromInt(FillColor(ping.m_iSide));
			color.SetA(PING_FILL_ALPHA * life);
			PolygonDrawCommand disc = new PolygonDrawCommand();
			disc.m_iColor = color.PackToInt();
			disc.m_Vertices = Circle(centreX, centreY, PING_RADIUS * perMetreX, PING_RADIUS * perMetreY);
			m_aCommands.Insert(disc);

			color.SetA(life * (1 - pulse));
			LineDrawCommand ring = new LineDrawCommand();
			ring.m_iColor = color.PackToInt();
			ring.m_fWidth = PING_RING_WIDTH;
			ring.m_bShouldEnclose = true;
			float ringScale = 1 + pulse;
			ring.m_Vertices = Circle(centreX, centreY, PING_RADIUS * perMetreX * ringScale, PING_RADIUS * perMetreY * ringScale);
			m_aCommands.Insert(ring);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected array<float> Circle(float centreX, float centreY, float radiusX, float radiusY)
	{
		array<float> vertices = {};
		for (int i = 0; i < PING_SEGMENTS; i++)
		{
			float angle = Math.PI2 * i / PING_SEGMENTS;
			vertices.Insert(centreX + Math.Cos(angle) * radiusX);
			vertices.Insert(centreY + Math.Sin(angle) * radiusY);
		}

		return vertices;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildTerritoryCommands(float baseX, float baseY, float scaleX, float scaleY)
	{
		m_aTerritoryCommands.Clear();
		for (int owner = COA_AAS_ETerritoryOwner.BLUFOR; owner <= COA_AAS_ETerritoryOwner.INDFOR; owner++)
		{
			TriMeshDrawCommand mesh = FillFor(owner, baseX, baseY, scaleX, scaleY);
			if (mesh)
				m_aTerritoryCommands.Insert(mesh);
		}

		foreach (COA_AAS_TerritoryEdge edge : m_aEdges)
		{
			LineDrawCommand line = new LineDrawCommand();
			line.m_Vertices = {baseX + edge.m_iX0 * scaleX, baseY + edge.m_iZ0 * scaleY, baseX + edge.m_iX1 * scaleX, baseY + edge.m_iZ1 * scaleY};
			if (edge.m_bFront)
			{
				line.m_iColor = FRONT_COLOR;
				line.m_fWidth = FRONT_WIDTH;
			}
			else
			{
				line.m_iColor = EDGE_COLOR;
				line.m_fWidth = EDGE_WIDTH;
			}

			m_aTerritoryCommands.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One mesh of every row a side holds, two triangles per row
	protected TriMeshDrawCommand FillFor(int owner, float baseX, float baseY, float scaleX, float scaleY)
	{
		array<float> vertices = {};
		array<int> indices = {};
		foreach (COA_AAS_TerritoryRun run : m_aRuns)
		{
			if (run.m_iOwner != owner)
				continue;

			float left = baseX + run.m_iX0 * scaleX;
			float right = baseX + (run.m_iX1 + 1) * scaleX;
			float bottom = baseY + run.m_iZ * scaleY;
			float top = baseY + (run.m_iZ + 1) * scaleY;

			int first = vertices.Count() / 2;
			vertices.Insert(left);
			vertices.Insert(bottom);
			vertices.Insert(right);
			vertices.Insert(bottom);
			vertices.Insert(right);
			vertices.Insert(top);
			vertices.Insert(left);
			vertices.Insert(top);

			indices.Insert(first);
			indices.Insert(first + 1);
			indices.Insert(first + 2);
			indices.Insert(first);
			indices.Insert(first + 2);
			indices.Insert(first + 3);
		}

		if (indices.IsEmpty())
			return null;

		TriMeshDrawCommand mesh = new TriMeshDrawCommand();
		mesh.m_iColor = FillColor(owner);
		mesh.m_Vertices = vertices;
		mesh.m_Indices = indices;
		return mesh;
	}

	//------------------------------------------------------------------------------------------------
	protected int FillColor(int owner)
	{
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		Faction faction;
		if (campaign)
		{
			switch (owner)
			{
				case COA_AAS_ETerritoryOwner.BLUFOR: faction = campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR); break;
				case COA_AAS_ETerritoryOwner.OPFOR: faction = campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR); break;
				case COA_AAS_ETerritoryOwner.INDFOR: faction = campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR); break;
			}
		}

		Color color = Color.FromInt(Color.WHITE);
		if (faction)
			color = Color.FromInt(faction.GetFactionColor().PackToInt());

		color.SetA(FILL_ALPHA);
		return color.PackToInt();
	}
}
