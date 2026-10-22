//------------------------------------------------------------------------------------------------
//! Lets COA_AAS_ConflictComponent end the match the same way Conflict's own victory does
modded class SCR_GameModeCampaign
{
	//------------------------------------------------------------------------------------------------
	//! Supplies each HQ starts with - the mission header's override, else the game mode's
	int COA_AAS_GetHQStartingSupplies()
	{
		SCR_MissionHeaderCampaign header = SCR_MissionHeaderCampaign.Cast(GetGame().GetMissionHeader());
		if (header && header.m_iStartingHQSupplies != -1)
			return header.m_iStartingHQSupplies;

		return m_iHQStartingSupplies;
	}

	//------------------------------------------------------------------------------------------------
	//! Territory FOBs: command trucks can build bases without the commander role Conflict ties it to
	//! (COA_AAS_SCR_CampaignBuildingPlacingHQBaseEditorComponent swaps the commander's task for territory)
	override bool GetEstablishingBasesEnabled()
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (aas && aas.AreTerritoryFOBsEnabled())
			return true;

		return super.GetEstablishingBasesEnabled();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only
	void COA_AAS_EndMatch(SCR_CampaignFaction winner)
	{
		if (IsProxy() || m_bMatchOver || !winner)
			return;

		int winnerId = GetGame().GetFactionManager().GetFactionIndex(winner);
		GetGame().GetCallqueue().Remove(CheckForWinner);
		RPC_DoEndMatch(winnerId);
		Rpc(RPC_DoEndMatch, winnerId);
		OnMatchSituationChanged();
	}
}
