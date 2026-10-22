//------------------------------------------------------------------------------------------------
//! Advance & Secure: at any objective in the line that isn't the live one, only its owners count -
//! attackers standing there can neither seize nor contest it (COA_AAS_ConflictComponent).
//! Capture time: the live objective seizes faster the more of the ground around it the seizing side
//! holds - push the front up before the assault - and slower on a full server, faster on a quiet one
//! (COA_AAS_ConflictComponent.GetCaptureTimeFactor).
//! CHDKz only count at bases they hold, unless the game mode lets them seize (InsurgentsCountAt).
modded class SCR_CampaignSeizingComponent
{
	//------------------------------------------------------------------------------------------------
	override void RefreshSeizingTimer()
	{
		super.RefreshSeizingTimer();

		if (!Replication.IsServer() || !m_Base || !m_fSeizingStartTimestamp || !m_fSeizingEndTimestamp)
			return;

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas)
			return;

		float factor = aas.GetCaptureTimeFactor(m_Base, m_PrevailingFaction);
		if (factor == 1)
			return;

		// Conflict's whole capture time (players, services, radio links), scaled
		float duration = m_fSeizingEndTimestamp.DiffMilliseconds(m_fSeizingStartTimestamp) * factor;
		m_fSeizingEndTimestamp = m_fSeizingStartTimestamp.PlusMilliseconds(duration);

		// Ground taken mid-capture can put the end in the past - finish it a moment from now instead
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		if (m_fSeizingEndTimestamp.LessEqual(now))
			m_fSeizingEndTimestamp = now.PlusMilliseconds(SCR_GameModeCampaign.DEFAULT_DELAY);

		Replication.BumpMe();
		OnSeizingTimestampChanged();
	}

	//------------------------------------------------------------------------------------------------
	override protected SCR_Faction EvaluateEntityFaction(IEntity ent)
	{
		SCR_Faction faction = super.EvaluateEntityFaction(ent);
		if (!faction || !m_Base)
			return faction;

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || !aas.IsAASActive())
			return faction;

		// CHDKz raiding a base someone else holds fight there but don't take it (COA_AAS_Insurgents)
		if (!aas.InsurgentsCountAt(m_Base, faction))
			return null;

		if (!aas.IsInChain(m_Base) || aas.IsLiveObjective(m_Base))
			return faction;

		if (faction != m_Base.GetFaction())
			return null;

		return faction;
	}
}
