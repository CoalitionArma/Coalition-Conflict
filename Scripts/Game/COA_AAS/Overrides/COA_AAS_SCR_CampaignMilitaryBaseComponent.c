//------------------------------------------------------------------------------------------------
//! Advance & Secure: objectives in the line other than the live one can't start a capture by any
//! route (seizing, radio relay capture) and get no seize tasks (COA_AAS_ConflictComponent)
modded class SCR_CampaignMilitaryBaseComponent
{
	//------------------------------------------------------------------------------------------------
	protected bool COA_AAS_IsLocked()
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		return aas && aas.IsAASActive() && aas.IsInChain(this) && !aas.IsLiveObjective(this);
	}

	//------------------------------------------------------------------------------------------------
	override bool BeginCapture(SCR_CampaignFaction faction, int playerID = INVALID_PLAYER_INDEX)
	{
		if (COA_AAS_IsLocked())
			return false;

		return super.BeginCapture(faction, playerID);
	}

	//------------------------------------------------------------------------------------------------
	override bool IsValidForSeizeTask()
	{
		if (COA_AAS_IsLocked())
			return false;

		return super.IsValidForSeizeTask();
	}
}
