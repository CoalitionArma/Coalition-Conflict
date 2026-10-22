//------------------------------------------------------------------------------------------------
//! Advance & Secure FOBs: a command truck's HQ composition can go anywhere in its side's territory
//! (COA_AAS_ConflictComponent.IsInTerritory) instead of only inside a commander's establish base task.
//! Conflict's own spacing from other bases still applies.
modded class SCR_CampaignBuildingPlacingHQBaseEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool IsPreviewOutOfTaskRange(out ENotification outNotification = -1)
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || !aas.AreTerritoryFOBsEnabled())
			return super.IsPreviewOutOfTaskRange(outNotification);

		if (m_PreviewEntity && aas.IsInTerritory(m_PreviewEntity.GetOrigin(), m_Faction))
			return false;

		outNotification = ENotification.GROUP_TASK_ESTABLISH_BASE_INVALID_AREA;
		return true;
	}
}
