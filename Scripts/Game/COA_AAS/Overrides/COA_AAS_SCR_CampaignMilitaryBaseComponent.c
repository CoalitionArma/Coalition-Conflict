//------------------------------------------------------------------------------------------------
//! Advance & Secure: objectives in the line other than the live one can't start a capture by any
//! route (seizing, radio relay capture) and get no seize tasks (COA_AAS_ConflictComponent).
//! The line, the HQs and FOBs are always in HQ radio range for both sides (COA_AAS_ConflictComponent.HasComms):
//! Conflict's coverage gates capturing, spawning, supplies and the map's "no signal" icon on it.
//! Logistics: no respawn at the objectives, they start (nearly) empty, and only the HQs make supplies -
//! the rest is trucked in.
modded class SCR_CampaignMilitaryBaseComponent
{
	//------------------------------------------------------------------------------------------------
	override SCR_ERadioCoverageStatus GetHQRadioCoverage(notnull SCR_CampaignFaction faction)
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (aas && aas.HasComms(this))
			return SCR_ERadioCoverageStatus.BOTH_WAYS;

		return super.GetHQRadioCoverage(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Objectives never open their spawn point (unless allowed) - re-run on every ownership, supply and
	//! capture change, so this holds whatever Conflict decides
	override protected void HandleSpawnPointFaction()
	{
		super.HandleSpawnPointFaction();

		if (!m_SpawnPoint || m_bIsHQ)
			return;

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || aas.IsObjectiveRespawnAllowed() || !aas.IsObjective(this))
			return;

		if (m_SpawnPoint.GetFactionKey() != FactionKey.Empty)
			m_SpawnPoint.SetFactionKey(FactionKey.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Only the HQs (and Conflict's supply sources) make supplies, unless bases are allowed to
	override protected int GetSuppliesIncomeAmount()
	{
		if (!m_bIsHQ && !m_bIsSupplyHub && m_eType != SCR_ECampaignBaseType.SOURCE_BASE)
		{
			COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
			if (aas && !aas.IsBaseSupplyIncomeAllowed())
				return 0;
		}

		return super.GetSuppliesIncomeAmount();
	}

	//------------------------------------------------------------------------------------------------
	//! The objectives start with the set amount (none by default) instead of Conflict's random one
	override void SetInitialSupplies(float suppliesCount)
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!m_bIsHQ && aas && aas.IsObjective(this) && aas.GetObjectiveStartingSupplies() >= 0)
		{
			SetSupplies(aas.GetObjectiveStartingSupplies());
			return;
		}

		super.SetInitialSupplies(suppliesCount);
	}

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
	//! With the AAS line, only the live objective ever gets a seize task - from the start (before the line
	//! is set up nothing is live) and for every other base too (FOBs, relays): one capture task at a time
	override bool IsValidForSeizeTask()
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (aas && aas.HasObjectiveChain() && !aas.IsLiveObjective(this))
			return false;

		return super.IsValidForSeizeTask();
	}
}
