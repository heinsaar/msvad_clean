/*
Abstract:
    Setup and miniport installation.  No resources are used by msvad.
*/

#pragma warning (disable : 4127)

//
// All the GUIDS for all the miniports end up in this object.
//
#define PUT_GUIDS_HERE

#include <msvad.h>
#include "common.h"

//-----------------------------------------------------------------------------
// Defines                                                                    
//-----------------------------------------------------------------------------

// BUGBUG set this to number of miniports
#define MAX_MINIPORTS 3 // Number of maximum miniports.

//-----------------------------------------------------------------------------
// Referenced forward.
//-----------------------------------------------------------------------------

DRIVER_ADD_DEVICE addDevice;

NTSTATUS startDevice(IN  PDEVICE_OBJECT, IN  PIRP, IN  PRESOURCELIST);

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

//=============================================================================
/*

Routine Description:

  Handles PnP IRPs                                                           

Arguments:

  _Fdo - Functional Device object pointer.
  _Irp - The Irp being passed

*/
#pragma code_seg("PAGE")
NTSTATUS pnpHandler(_In_ DEVICE_OBJECT* deviceObject, _In_ IRP* irp)
{
    PAGED_CODE(); 
    ASSERT(deviceObject);
    ASSERT(irp);

    // Check for the REMOVE_DEVICE irp.  If we're being unloaded, 
    // uninstantiate our devices and release the adapter common object.
    //
    IO_STACK_LOCATION* stack = IoGetCurrentIrpStackLocation(irp);
    
    if ((IRP_MN_REMOVE_DEVICE    == stack->MinorFunction) ||
        (IRP_MN_SURPRISE_REMOVAL == stack->MinorFunction) ||
        (IRP_MN_STOP_DEVICE      == stack->MinorFunction))
    {
        PortClassDeviceContext* ext = static_cast<PortClassDeviceContext*>(deviceObject->DeviceExtension);

        if (ext->common_ != nullptr)
        {
            ext->common_->uninstantiateDevices();
            ext->common_->Release();
            ext->common_ = nullptr;
        }
    }
    
    return PcDispatchIrp(deviceObject, irp);
}

//=============================================================================
/*
Routine Description:

  Installable driver initialization entry point.
  This entry point is called directly by the I/O system.
  All audio adapter drivers can use this code without change.

Arguments:

  DriverObject - pointer to the driver object
  RegistryPath - pointer to a Unicode string representing the path,
                 to driver-specific key in the registry.
*/
#pragma code_seg("INIT")
extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" NTSTATUS DriverEntry(IN  PDRIVER_OBJECT  driverObject, IN  PUNICODE_STRING registryPathName)
{
    DPF(D_TERSE, ("[DriverEntry]"));

    // Tell the class driver to initialize the driver.
    //
    NTSTATUS ntStatus = PcInitializeAdapterDriver(driverObject, registryPathName, (PDRIVER_ADD_DEVICE)addDevice);

    if (NT_SUCCESS(ntStatus))
    {
#pragma warning (push)
#pragma warning( disable:28169 ) 
#pragma warning( disable:28023 ) 
        driverObject->MajorFunction[IRP_MJ_PNP] = pnpHandler;
#pragma warning (pop)
    }

    return ntStatus;
}
#pragma code_seg()

// disable prefast warning 28152 because 
// DO_DEVICE_INITIALIZING is cleared in PcAddAdapterDevice
#pragma warning(disable:28152)
#pragma code_seg("PAGE")
//=============================================================================
/*
Routine Description:

  The Plug & Play subsystem is handing us a brand new PDO, for which we
  (by means of INF registration) have been asked to provide a driver.

  We need to determine if we need to be in the driver stack for the device.
  Create a function device object to attach to the stack
  Initialize that device object
  Return status success.

  All audio adapter drivers can use this code without change.
  Set MAX_MINIPORTS depending on the number of miniports that the driver
  uses.

Arguments:
  DriverObject - pointer to a driver object
  PhysicalDeviceObject -  pointer to a device object created by the underlying bus driver.
*/
NTSTATUS addDevice(IN  PDRIVER_OBJECT driverObject, IN  PDEVICE_OBJECT physicalDeviceObject)
{
    PAGED_CODE();
    DPF(D_TERSE, ("[AddDevice]"));

    // Tell the class driver to add the device.
    //
    return PcAddAdapterDevice(driverObject, physicalDeviceObject, PCPFNSTARTDEVICE(startDevice), MAX_MINIPORTS, 0);
}

//=============================================================================
/*
Routine Description:

  This function is called by the operating system when the device is started.
  It is responsible for starting the miniports.  This code is specific to the   
  adapter because it calls out miniports for functions that are specific to the adapter.                                                            

Arguments:

  DeviceObject - pointer to the driver object
  Irp          - pointer to the irp 
  ResourceList - pointer to the resource list assigned by PnP manager
*/
NTSTATUS startDevice
(
    IN  PDEVICE_OBJECT deviceObject,
    IN  PIRP           irp,
    IN  PRESOURCELIST  resourceList
)
{
    UNREFERENCED_PARAMETER(resourceList);
    UNREFERENCED_PARAMETER(irp);

    PAGED_CODE();

    ASSERT(deviceObject);
    ASSERT(irp);
    ASSERT(resourceList);

    PADAPTERCOMMON              pAdapterCommon  = nullptr;
    PUNKNOWN                    pUnknownCommon  = nullptr;
    PortClassDeviceContext*     pExtension      = static_cast<PortClassDeviceContext*>(deviceObject->DeviceExtension);

    DPF_ENTER(("[StartDevice]"));

    // create a new adapter common object
    //
    NTSTATUS ntStatus = newAdapterCommon(&pUnknownCommon, IID_IAdapterCommon, nullptr, NonPagedPool);
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = pUnknownCommon->QueryInterface(IID_IAdapterCommon, (PVOID *) &pAdapterCommon);

        if (NT_SUCCESS(ntStatus))
        {
            ntStatus = pAdapterCommon->Init(deviceObject);

            if (NT_SUCCESS(ntStatus))
            {
                // register with PortCls for power-management services
                //    
                ntStatus = PcRegisterAdapterPowerManagement(PUNKNOWN(pAdapterCommon), deviceObject);
            }
        }
    }

    // Tell the adapter common object to instantiate the subdevices.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = pAdapterCommon->instantiateDevices();
    }

    // Stash the adapter common object in the device extension so
    // we can access it for cleanup on stop/removal.
    //
    if (pAdapterCommon)
    {
        pExtension->common_ = pAdapterCommon;
    }

    if (pUnknownCommon)
    {
        pUnknownCommon->Release();
    }
    
    return ntStatus;
}
#pragma code_seg()