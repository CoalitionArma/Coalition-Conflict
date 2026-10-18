//------------------------------------------------------------------------------------------------
//! Vanilla heavy vehicle depots only list trucks and APCs. With AAS the factions also field tanks
//! (M1A1, T-72), so heavy depots list VEHICLE_TANK entries of the owner's vehicle catalog as well.
modded class SCR_CatalogEntitySpawnerComponent
{
	//------------------------------------------------------------------------------------------------
	protected override void AddAssetsFromCatalog(notnull SCR_EntityCatalog entityCatalog, bool overwriteOld = false)
	{
		if (COA_AAS_ConflictComponent.GetInstance() && GetType() == SCR_EServicePointType.HEAVY_VEHICLE_DEPOT
			&& !m_bNeedAllLabels && m_aAllowedLabels && !m_aAllowedLabels.IsEmpty() && !m_aAllowedLabels.Contains(EEditableEntityLabel.VEHICLE_TANK))
			m_aAllowedLabels.Insert(EEditableEntityLabel.VEHICLE_TANK);

		super.AddAssetsFromCatalog(entityCatalog, overwriteOld);
	}
}
