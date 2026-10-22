//------------------------------------------------------------------------------------------------
//! Capture order on the map: a line from your HQ through the objective chain, in the order the bases fall.
//! A stretch one side holds at both ends is solid in its colour; the contested stretches - the front, either
//! side of the live objective - are dashed white. The enemy HQ isn't linked, so the line never gives it away.
//!
//! The lines link the bases' own map items (links between items without an icon aren't drawn). Conflict
//! links those items into its radio network when a map opens and clears them when it closes, never in
//! between - so these are added a frame after it opens, and the radio links on the line are hidden
//! (HIDE_RADIO_LINKS) so the two don't draw over each other.
class COA_AAS_MapLines
{
	protected static const float LINE_WIDTH = 4;
	protected static const float OUTLINE_WIDTH = 1;
	protected static const int REFRESH_MS = 1000;
	protected static const bool HIDE_RADIO_LINKS = true;

	protected COA_AAS_ConflictComponent m_AAS;
	protected ref array<MapItem> m_aItems = {};
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aNodes = {};
	protected ref array<MapLink> m_aLinks = {};

	//------------------------------------------------------------------------------------------------
	void COA_AAS_MapLines(COA_AAS_ConflictComponent aas)
	{
		m_AAS = aas;
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_MapLines()
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Build);
			GetGame().GetCallqueue().Remove(Refresh);
		}

		Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A frame later: the bases' own map-open handlers add Conflict's radio links first
	protected void OnMapOpen(MapConfiguration config)
	{
		Clear();
		GetGame().GetCallqueue().Remove(Build);
		GetGame().GetCallqueue().CallLater(Build, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapClose(MapConfiguration config)
	{
		GetGame().GetCallqueue().Remove(Build);
		GetGame().GetCallqueue().Remove(Refresh);
		Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Your HQ, then the objectives from your end of the line to the enemy's
	protected void Build()
	{
		Clear();
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign || !m_AAS)
			return;

		int count = m_AAS.GetChainCount();
		if (count == 0)
			return;

		SCR_CampaignFaction playerFaction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
		SCR_CampaignFaction blufor = campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		SCR_CampaignFaction opfor = campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);

		array<SCR_CampaignMilitaryBaseComponent> nodes = {};
		for (int i = 0; i < count; i++)
		{
			SCR_CampaignMilitaryBaseComponent base = m_AAS.GetChainBase(i);
			if (base)
				nodes.Insert(base);
		}

		// The chain runs from the BLUFOR HQ side to the OPFOR one
		if (playerFaction && playerFaction == blufor && blufor.GetMainBase())
			nodes.InsertAt(blufor.GetMainBase(), 0);
		else if (playerFaction && playerFaction == opfor && opfor.GetMainBase())
			nodes.Insert(opfor.GetMainBase());

		foreach (SCR_CampaignMilitaryBaseComponent node : nodes)
		{
			SCR_CampaignMilitaryBaseMapDescriptorComponent descriptor = node.GetMapDescriptor();
			MapItem item;
			if (descriptor)
				item = descriptor.Item();

			if (!item)
				continue;

			m_aItems.Insert(item);
			m_aNodes.Insert(node);
		}

		HideRadioLinks();
		for (int i = 0; i + 1 < m_aItems.Count(); i++)
		{
			m_aLinks.Insert(m_aItems[i].LinkTo(m_aItems[i + 1]));
		}

		Refresh();
		GetGame().GetCallqueue().Remove(Refresh);
		GetGame().GetCallqueue().CallLater(Refresh, REFRESH_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Conflict's radio links from the bases on the line - the capture line replaces them there
	protected void HideRadioLinks()
	{
		if (!HIDE_RADIO_LINKS)
			return;

		array<MapLink> links = {};
		foreach (MapItem item : m_aItems)
		{
			if (!item)
				continue;

			links.Clear();
			item.GetLinks(links);
			foreach (MapLink link : links)
			{
				if (!link || m_aLinks.Contains(link))
					continue;

				MapLinkProps props = link.GetMapLinkProps();
				if (props)
					props.SetVisible(false);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Colours follow ownership: checked every REFRESH_MS while the map is open, so a capture shows at once
	protected void Refresh()
	{
		HideRadioLinks();

		foreach (int i, MapLink link : m_aLinks)
		{
			if (!link || !m_aNodes.IsIndexValid(i + 1))
				continue;

			MapLinkProps props = link.GetMapLinkProps();
			if (!props)
				continue;

			Faction from = m_aNodes[i].GetFaction();
			Faction to = m_aNodes[i + 1].GetFaction();
			bool held = from && from == to;

			Color color = Color.FromInt(Color.WHITE);
			if (held)
				color = Color.FromInt(from.GetFactionColor().PackToInt());

			if (held)
				props.SetLineType(EMapLineType.LT_FULL_LINE);
			else
				props.SetLineType(EMapLineType.LT_DASHED);

			props.SetLineColor(color);
			props.SetLineWidth(LINE_WIDTH);
			props.SetOutlineColor(Color.FromInt(Color.BLACK));
			props.SetOutlineWidth(OUTLINE_WIDTH);
			props.SetVisible(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Only our links - the items are the bases', and Conflict clears its own links when the map closes
	protected void Clear()
	{
		for (int i = 0; i + 1 < m_aItems.Count(); i++)
		{
			if (m_aItems[i] && m_aItems[i + 1])
				m_aItems[i].UnLink(m_aItems[i + 1]);
		}

		m_aItems.Clear();
		m_aNodes.Clear();
		m_aLinks.Clear();
	}
}
