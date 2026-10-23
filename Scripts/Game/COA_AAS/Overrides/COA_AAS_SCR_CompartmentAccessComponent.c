//------------------------------------------------------------------------------------------------
//! COA_AAS_VehicleRoles' backstop: a player who still gets into a seat their loadout can't take (seat-switch
//! keys, the editor) is moved to a passenger seat, or out. Runs on the player's own machine, which moves them.
modded class SCR_CompartmentAccessComponent
{
	protected static const int COA_AAS_MOVE_OUT_DELAY_MS = 300;	// let getting in finish first

	//------------------------------------------------------------------------------------------------
	override protected void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		super.OnCompartmentEntered(targetEntity, manager, mgrID, slotID, move);

		if (GetOwner() != SCR_PlayerController.GetLocalControlledEntity())
			return;

		if (!COA_AAS_VehicleRoles.Denied(GetCompartment(), GetOwner()).IsEmpty())
			GetGame().GetCallqueue().CallLater(COA_AAS_MoveOut, COA_AAS_MOVE_OUT_DELAY_MS, false, targetEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void COA_AAS_MoveOut(IEntity vehicle)
	{
		BaseCompartmentSlot slot = GetCompartment();
		if (COA_AAS_VehicleRoles.Denied(slot, GetOwner()).IsEmpty())
			return;

		COA_AAS_VehicleRoles.ShowMovedOutHint(COA_AAS_VehicleRoles.Required(slot));

		// A passenger seat keeps them aboard (an aircraft in flight), else they get out
		BaseCompartmentSlot seat;
		if (vehicle)
			seat = FindFreeAndAccessibleCompartment(SCR_EntityHelper.GetMainParent(vehicle, true), ECompartmentType.CARGO);

		if (seat && GetInVehicle(seat.GetOwner(), seat, true, -1, ECloseDoorAfterActions.INVALID, false))
			return;

		GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.INVALID, false);
	}
}
