//------------------------------------------------------------------------------------------------
//! Supply runs to bases that start empty (COA_AAS_ConflictComponent: objectives start with no supplies).
//! Conflict's HQ tents keep their supplies in a pile that hides itself while empty, and a truck only
//! unloads within 12 m of a pile - so at an empty objective there was nothing to park next to and the
//! unload action said there was no storage nearby. Base piles now always show, and trucks unload from
//! up to 30 m away (Configs/Resources/Supplies/Generators/Generator_VehicleUnload.conf overrides vanilla's
//! 12 m - a script mod of SCR_ResourceGenerator breaks its replication codec).
class COA_AAS_SupplyStorage
{
	//------------------------------------------------------------------------------------------------
	//! Server: the base can hold at least capacity supplies - the shortfall goes onto one crate of one of
	//! its piles. Conflict only pays income into free space, and an HQ's piles hold little more than the
	//! HQ tent's 1000.
	//! \return whether the base now holds enough (false: no pile found yet, try again later)
	static bool RaiseCapacity(notnull SCR_CampaignMilitaryBaseComponent base, float capacity)
	{
		float shortfall = capacity - base.GetSuppliesMax();
		if (shortfall <= 0)
			return true;

		SCR_ResourceComponent resourceComponent = base.GetResourceComponent();
		if (!resourceComponent)
			return false;

		SCR_ResourceConsumer consumer = resourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer || !consumer.GetContainerQueue())
			return false;

		SCR_ResourceContainerQueueBase storages = consumer.GetContainerQueue();
		for (int i = 0, count = storages.GetContainerCount(); i < count; i++)
		{
			SCR_ResourceContainer storage = storages.GetContainerAt(i);
			if (!storage || !storage.GetComponent())
				continue;

			array<SCR_ResourceEncapsulator> encapsulators = storage.GetComponent().GetEncapsulators();
			if (!encapsulators)
				continue;

			foreach (SCR_ResourceEncapsulator encapsulator : encapsulators)
			{
				SCR_ResourceContainerQueueBase crates = encapsulator.GetContainerQueue();
				if (!crates || crates.GetContainerCount() == 0)
					continue;

				SCR_ResourceContainer crate = crates.GetContainerAt(0);
				if (!crate)
					continue;

				crate.SetMaxResourceValue(crate.GetMaxResourceValue() + shortfall);
				Print(string.Format("[COA_AAS] %1 can now hold %2 supplies", base.GetBaseName(), base.GetSuppliesMax()), LogLevel.NORMAL);
				return true;
			}
		}

		Print(string.Format("[COA_AAS] %1: no supply pile to enlarge", base.GetBaseName()), LogLevel.WARNING);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every supply pile the base draws on stays visible when empty
	static void KeepPilesVisible(notnull SCR_CampaignMilitaryBaseComponent base)
	{
		SCR_ResourceComponent resourceComponent = base.GetResourceComponent();
		if (!resourceComponent)
			return;

		SCR_ResourceConsumer consumer = resourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer)
			return;

		// Picks up the storages around the base first
		base.GetSuppliesMax();

		SCR_ResourceContainerQueueBase storages = consumer.GetContainerQueue();
		if (!storages)
			return;

		for (int i = storages.GetContainerCount() - 1; i >= 0; i--)
		{
			SCR_ResourceContainer storage = storages.GetContainerAt(i);
			if (!storage || !storage.GetComponent())
				continue;

			array<SCR_ResourceEncapsulator> encapsulators = storage.GetComponent().GetEncapsulators();
			if (!encapsulators)
				continue;

			// The pile's crates are the encapsulated containers
			foreach (SCR_ResourceEncapsulator encapsulator : encapsulators)
			{
				SCR_ResourceContainerQueueBase crates = encapsulator.GetContainerQueue();
				if (!crates)
					continue;

				for (int c = crates.GetContainerCount() - 1; c >= 0; c--)
				{
					SCR_ResourceContainer crate = crates.GetContainerAt(c);
					if (!crate || crate.GetOnEmptyBehavior() != EResourceContainerOnEmptyBehavior.HIDE)
						continue;

					crate.SetOnEmptyBehavior(EResourceContainerOnEmptyBehavior.NONE);
					if (crate.GetComponent() && !crate.GetComponent().IsVisible())
						crate.GetComponent().SetIsVisible(true);
				}
			}
		}
	}
}
