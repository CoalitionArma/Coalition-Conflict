//------------------------------------------------------------------------------------------------
enum COA_AAS_EVehicleRole
{
	NONE,
	CREW,	// drives and mans the guns of armour
	PILOT	// flies aircraft
}

//------------------------------------------------------------------------------------------------
class COA_AAS_VehicleRoleComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! On a loadout's character prefab: what it's trained to operate (COA_AAS_VehicleRoles)
[ComponentEditorProps(category: "COALITION/Conflict", description: "Vehicle role of a loadout - crewmen operate armour, pilots fly aircraft")]
class COA_AAS_VehicleRoleComponent : ScriptComponent
{
	[Attribute(COA_AAS_EVehicleRole.CREW.ToString(), UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(COA_AAS_EVehicleRole))]
	protected COA_AAS_EVehicleRole m_eRole;

	//------------------------------------------------------------------------------------------------
	COA_AAS_EVehicleRole GetRole()
	{
		return m_eRole;
	}
}

//------------------------------------------------------------------------------------------------
//! Who may take which seat (COA_AAS_ConflictComponent's vehicle roles):
//!  - armour - anything labelled APC or tank, so IFVs too - needs a crewman to drive or man its guns
//!  - aircraft need a pilot in the pilot seats
//!  - passenger seats, door guns and every other vehicle are open to anyone, and AI is never stopped
//! The seat actions refuse the seat up front (COA_AAS_SCR_VehicleRoleActions); anyone who still ends up in one
//! is moved out (COA_AAS_SCR_CompartmentAccessComponent).
class COA_AAS_VehicleRoles
{
	protected static const string CREW_ONLY = "Crewman only";
	protected static const string PILOT_ONLY = "Pilot only";

	//------------------------------------------------------------------------------------------------
	//! The role a seat needs, NONE when anyone may take it
	static COA_AAS_EVehicleRole Required(BaseCompartmentSlot slot)
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!slot || !aas || !aas.AreVehicleRolesEnabled())
			return COA_AAS_EVehicleRole.NONE;

		bool pilotSeat = PilotCompartmentSlot.Cast(slot) != null;
		if (!pilotSeat && !TurretCompartmentSlot.Cast(slot))
			return COA_AAS_EVehicleRole.NONE;

		IEntity vehicle = SCR_EntityHelper.GetMainParent(slot.GetOwner(), true);
		if (!vehicle)
			return COA_AAS_EVehicleRole.NONE;

		// Aircraft: only flying counts - door guns are for anyone
		if (vehicle.FindComponent(HelicopterControllerComponent))
		{
			if (pilotSeat)
				return COA_AAS_EVehicleRole.PILOT;

			return COA_AAS_EVehicleRole.NONE;
		}

		if (IsArmour(vehicle))
			return COA_AAS_EVehicleRole.CREW;

		return COA_AAS_EVehicleRole.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! APCs, IFVs and tanks, by their editor labels
	static bool IsArmour(notnull IEntity vehicle)
	{
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.GetEditableEntity(vehicle);
		if (!editable)
			return false;

		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());
		return info && (info.HasEntityLabel(EEditableEntityLabel.VEHICLE_APC) || info.HasEntityLabel(EEditableEntityLabel.VEHICLE_TANK));
	}

	//------------------------------------------------------------------------------------------------
	//! Why a character can't take a seat (the action menu shows it), empty when it can
	static string Denied(BaseCompartmentSlot slot, IEntity character)
	{
		if (!character)
			return string.Empty;

		COA_AAS_EVehicleRole required = Required(slot);
		if (required == COA_AAS_EVehicleRole.NONE)
			return string.Empty;

		// Players only
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) <= 0)
			return string.Empty;

		COA_AAS_VehicleRoleComponent role = COA_AAS_VehicleRoleComponent.Cast(character.FindComponent(COA_AAS_VehicleRoleComponent));
		if (role && role.GetRole() == required)
			return string.Empty;

		if (required == COA_AAS_EVehicleRole.PILOT)
			return PILOT_ONLY;

		return CREW_ONLY;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player: tells them why they were moved out of a seat
	static void ShowMovedOutHint(COA_AAS_EVehicleRole required)
	{
		if (required == COA_AAS_EVehicleRole.PILOT)
			SCR_HintManagerComponent.ShowCustomHint("Only pilots can fly aircraft. Respawn with the Pilot loadout to take the controls.", "Pilot required", 8);
		else
			SCR_HintManagerComponent.ShowCustomHint("Only crewmen can drive or man the guns of APCs, IFVs and tanks. Respawn with the Crewman loadout to operate them.", "Crewman required", 8);
	}
}
