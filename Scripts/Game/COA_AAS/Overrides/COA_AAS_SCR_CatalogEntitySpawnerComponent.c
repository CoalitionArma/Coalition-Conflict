//------------------------------------------------------------------------------------------------
//! Vanilla heavy vehicle depots only list trucks and APCs. With AAS the factions also field tanks
//! (M1A1, T-72), so heavy depots list VEHICLE_TANK entries of the owner's vehicle catalog as well.
//! Powerful assets cost XP as well as supplies (COA_AAS_AssetXPCost): not requestable without it, and
//! paid once the asset spawns.
modded class SCR_CatalogEntitySpawnerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Checked where the player looks at the depot and again on the server when they request it
	override SCR_EEntityRequestStatus GetRequestState(notnull SCR_EntityCatalogEntry entityEntry, IEntity user = null)
	{
		SCR_EEntityRequestStatus status = super.GetRequestState(entityEntry, user);
		if (status != SCR_EEntityRequestStatus.CAN_SPAWN || !user)
			return status;

		int cost = COA_AAS_AssetXPCost.GetCostOf(entityEntry);
		if (cost > 0 && COA_AAS_AssetXPCost.GetPlayerXP(user) < cost)
			return SCR_EEntityRequestStatus.COA_AAS_NOT_ENOUGH_XP;

		return status;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pay the XP once the asset is actually out (no slot, no charge)
	override protected void PerformSpawn(notnull SCR_EntityCatalogEntry entityEntry, IEntity user = null, SCR_EntitySpawnerSlotComponent preferredSlot = null)
	{
		super.PerformSpawn(entityEntry, user, preferredSlot);

		if (IsProxy() || !m_SpawnedEntity || !user)
			return;

		int cost = COA_AAS_AssetXPCost.GetCostOf(entityEntry);
		if (cost <= 0)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!controller)
			return;

		SCR_PlayerXPHandlerComponent xp = SCR_PlayerXPHandlerComponent.Cast(controller.FindComponent(SCR_PlayerXPHandlerComponent));
		if (xp)
			xp.AddPlayerXP(SCR_EXPRewards.CUSTOM_20, 1, false, -cost);
	}

	//------------------------------------------------------------------------------------------------
	protected override void AddAssetsFromCatalog(notnull SCR_EntityCatalog entityCatalog, bool overwriteOld = false)
	{
		if (COA_AAS_ConflictComponent.GetInstance() && GetType() == SCR_EServicePointType.HEAVY_VEHICLE_DEPOT
			&& !m_bNeedAllLabels && m_aAllowedLabels && !m_aAllowedLabels.IsEmpty() && !m_aAllowedLabels.Contains(EEditableEntityLabel.VEHICLE_TANK))
			m_aAllowedLabels.Insert(EEditableEntityLabel.VEHICLE_TANK);

		super.AddAssetsFromCatalog(entityCatalog, overwriteOld);
	}
}
