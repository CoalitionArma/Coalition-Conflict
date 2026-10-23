//------------------------------------------------------------------------------------------------
//! COA_AAS_VehicleRoles: the seat actions refuse crewman-only and pilot-only seats, with the reason in the menu
modded class SCR_GetInUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		return COA_AAS_AllowSeat(GetCompartmentSlot(), user);
	}

	//------------------------------------------------------------------------------------------------
	protected bool COA_AAS_AllowSeat(BaseCompartmentSlot slot, IEntity user)
	{
		string reason = COA_AAS_VehicleRoles.Denied(slot, user);
		if (reason.IsEmpty())
			return true;

		SetCannotPerformReason(reason);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Its own check replaces SCR_GetInUserAction's rather than extending it
modded class SCR_SwitchSeatAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		return COA_AAS_AllowSeat(GetCompartmentSlot(), user);
	}
}

//------------------------------------------------------------------------------------------------
//! "Move to pilot / rear seats" from inside the vehicle
modded class SCR_ChangeComparmentAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		string reason = COA_AAS_VehicleRoles.Denied(m_NonOccupiedSlot, user);
		if (reason.IsEmpty())
			return true;

		SetCannotPerformReason(reason);
		return false;
	}
}
