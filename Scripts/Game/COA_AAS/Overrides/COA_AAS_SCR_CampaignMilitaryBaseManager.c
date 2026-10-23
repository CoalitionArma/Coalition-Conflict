//------------------------------------------------------------------------------------------------
//! Advance & Secure: Conflict's top bar scores "control points", which the objectives in the line aren't
//! (that flag also changes their map icons, seize XP and teardown rules). It shows each side's objectives
//! instead, out of the whole line (COA_AAS_SCR_GameModeCampaign.GetControlPointTreshold). Conflict's own
//! hold-to-win countdown stays off - the line has its own win (COA_AAS_ConflictComponent).
modded class SCR_CampaignMilitaryBaseManager
{
	//------------------------------------------------------------------------------------------------
	override void EvaluateControlPoints()
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || !aas.HasObjectiveChain())
		{
			super.EvaluateControlPoints();
			return;
		}

		// The server counts (the line is only known there) and replicates the scores
		if (m_Campaign.IsProxy())
			return;

		array<SCR_ECampaignFaction> sides = {SCR_ECampaignFaction.BLUFOR, SCR_ECampaignFaction.OPFOR};
		foreach (SCR_ECampaignFaction side : sides)
		{
			SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(side);
			if (faction)
				m_Campaign.SetControlPointsHeld(faction, aas.CountObjectivesHeld(faction));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Comms everywhere: a side's whole territory is in its radio signal, not just the bases' radio ranges -
	//! so the voice HUD's "no signal", the radio range notifications and what needs signal (radio spawn
	//! points, fast travel) follow the front. Behind enemy lines there's none.
	override bool IsEntityInFactionRadioSignal(notnull IEntity entity, notnull Faction faction)
	{
		if (super.IsEntityInFactionRadioSignal(entity, faction))
			return true;

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		return aas && aas.IsCommsEverywhere() && aas.IsInTerritory(entity.GetOrigin(), faction);
	}
}
