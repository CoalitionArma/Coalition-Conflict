//------------------------------------------------------------------------------------------------
//! Lets COA_AAS_ConflictComponent end the match the same way Conflict's own victory does
modded class SCR_GameModeCampaign
{
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
