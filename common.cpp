/*
Abstract:
    Implementation of the AdapterCommon class. 
*/

#pragma warning (disable : 4127)

// Disable the 28204 warning due to annotation mismatches between DDK headers
// for QueryInterface().
//
#pragma warning(disable:28204)

#include <msvad.h>
#include "common.h"
#include "hw.h"
#include "savedata.h"

#define HNS_PER_MS              10000
#define INSTANTIATE_INTERVAL_MS 10000   // 10 seconds between instantiate and uninstantiate

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------

PSAVEWORKER_PARAM CSaveData::workItems_    = nullptr;
PDEVICE_OBJECT    CSaveData::deviceObject_ = nullptr;

typedef
NTSTATUS (*PMSVADMINIPORTCREATE)
(
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_TYPE
);

NTSTATUS CreateMiniportWaveCyclicMSVAD
( 
    OUT PUNKNOWN *,
    IN  REFCLSID,
    IN  PUNKNOWN,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE PoolType
);

NTSTATUS CreateMiniportTopologyMSVAD
( 
    OUT PUNKNOWN *,
    IN  REFCLSID,
    IN  PUNKNOWN,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE PoolType
);

//=============================================================================
// Helper Routines
//=============================================================================
/*

Routine Description:

    This function creates and registers a subdevice consisting of a port       
    driver, a minport driver and a set of resources bound together.  It will   
    also optionally place a pointer to an interface on the port driver in a    
    specified location before initializing the port driver.  This is done so   
    that a common ISR can have access to the port driver during 
    initialization, when the ISR might fire.                                   

Arguments:

    DeviceObject - pointer to the driver object

    Irp - pointer to the irp object.

    Name - name of the miniport. Passes to PcRegisterSubDevice
 
    PortClassId - port class id. Passed to PcNewPort.

    MiniportClassId - miniport class id. Passed to PcNewMiniport.

    MiniportCreate - pointer to a miniport creation function. If nullptr, 
                     PcNewMiniport is used.

    UnknownAdapter - pointer to the adapter object. 
                     Used for initializing the port.

    ResourceList - pointer to the resource list.

    PortInterfaceId - GUID that represents the port interface.
       
    OutPortInterface - pointer to store the port interface

    OutPortUnknown - pointer to store the unknown port interface.
*/
#pragma code_seg("PAGE")
NTSTATUS InstallSubdevice
(
    _In_        PDEVICE_OBJECT          DeviceObject,
    _In_opt_    PIRP                    Irp,
    _In_        PWSTR                   Name,
    _In_        REFGUID                 PortClassId,
    _In_        REFGUID                 MiniportClassId,
    _In_opt_    PMSVADMINIPORTCREATE    MiniportCreate,
    _In_opt_    PUNKNOWN                UnknownAdapter,
    _In_opt_    PRESOURCELIST           ResourceList,
    _Out_opt_   PUNKNOWN *              OutPortUnknown,
    _Out_opt_   PUNKNOWN *              OutMiniportUnknown
)
{
    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(Name);

    PPORT    port     = nullptr;
    PUNKNOWN miniport = nullptr;
     
    DPF_ENTER(("[InstallSubDevice %S]", Name));

    // Create the port driver object
    //
    NTSTATUS ntStatus = PcNewPort(&port, PortClassId);

    // Create the miniport object
    //
    if (NT_SUCCESS(ntStatus))
    {
        if (MiniportCreate)
        {
            ntStatus = MiniportCreate(&miniport, MiniportClassId, nullptr, NonPagedPool);
        }
        else
        {
            ntStatus = PcNewMiniport((PMINIPORT *)&miniport, MiniportClassId);
        }
    }

    // Init the port driver and miniport in one go.
    //
    if (NT_SUCCESS(ntStatus))
    {
#pragma warning(push)
        // IPort::Init's annotation on ResourceList requires it to be non-nullptr.  However,
        // for dynamic devices, we may no longer have the resource list and this should
        // still succeed.
        //
#pragma warning(disable:6387)
        ntStatus = port->Init(DeviceObject, Irp, miniport, UnknownAdapter, ResourceList);
#pragma warning(pop)

        if (NT_SUCCESS(ntStatus))
        {
            // Register the subdevice (port/miniport combination).
            //
            ntStatus = PcRegisterSubdevice(DeviceObject, Name, port);
        }
    }

    // Deposit the port interfaces if it's needed.
    //
    if (NT_SUCCESS(ntStatus))
    {
        if (OutPortUnknown)
        {
            ntStatus = port->QueryInterface(IID_IUnknown, (PVOID *)OutPortUnknown);
        }

        if (OutMiniportUnknown)
        {
            ntStatus = miniport->QueryInterface(IID_IUnknown, (PVOID *)OutMiniportUnknown);
        }
    }

    if (port)
    {
        port->Release();
    }

    if (miniport)
    {
        miniport->Release();
    }

    return ntStatus;
}

//=============================================================================
#pragma code_seg("PAGE")
IO_WORKITEM_ROUTINE InstantiateTimerWorkRoutine;
void InstantiateTimerWorkRoutine
(
    _In_ DEVICE_OBJECT *_DeviceObject, 
    _In_opt_ PVOID _Context
)
{
	PAGED_CODE();
    UNREFERENCED_PARAMETER(_DeviceObject);

    PADAPTERCOMMON  pCommon = (PADAPTERCOMMON) _Context;

    if (!pCommon)
    {
        // This is completely unexpected, assert here.
        //
        ASSERT(pCommon);
        return;
    }

    // Loop through various states of instantiated and
    // plugged in.
    //
    if (pCommon->isInstantiated() && pCommon->isPluggedIn())
    {
        pCommon->uninstantiateDevices();
    }
    else if (pCommon->isInstantiated() && !pCommon->isPluggedIn())
    {
        pCommon->plugin();
    }
    else if (!pCommon->isInstantiated())
    {
        pCommon->instantiateDevices();
        pCommon->unplug();
    }

    // Free the work item that was allocated in the DPC.
    //
	pCommon->freeInstantiateWorkItem();
}

//=============================================================================
/*

Routine Description:

  Dpc routine. This simulates an interrupt service routine to simulate
  a jack plug/unplug.

Arguments:

  Dpc - the Dpc object

  DeferredContext - Pointer to a caller-supplied context to be passed to
                    the DeferredRoutine when it is called

  SA1 - System argument 1
  SA2 - System argument 2
*/
#pragma code_seg()
KDEFERRED_ROUTINE InstantiateTimerNotify;
void InstantiateTimerNotify
(
    IN  PKDPC Dpc,
    IN  PVOID DeferredContext,
    IN  PVOID SA1,
    IN  PVOID SA2
)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SA1);
    UNREFERENCED_PARAMETER(SA2);

    PIO_WORKITEM   pWorkItem = nullptr;
    PADAPTERCOMMON pCommon = (PADAPTERCOMMON) DeferredContext;

    if (!pCommon)
    {
        // This is completely unexpected, assert here.
        //
        ASSERT(pCommon);
        return;
    }

    // Queue a work item to run at PASSIVE_LEVEL so we can call
    // PortCls in order to register or unregister subdevices.
    //
    pWorkItem = IoAllocateWorkItem(pCommon->getDeviceObject());
    if (nullptr != pWorkItem)
    {
        // Store the work item in the adapter common object and queue it.
        //
        if (NT_SUCCESS(pCommon->setInstantiateWorkItem(pWorkItem)))
        {
            IoQueueWorkItem(pWorkItem, InstantiateTimerWorkRoutine, DelayedWorkQueue, DeferredContext);
        }
        else
        {
            // If we failed to stash the work item in the common adapter object,
            // free it now or else we'll leak it.
            //
            IoFreeWorkItem(pWorkItem);
        }
    }
}


//=============================================================================
// Classes
//=============================================================================

class AdapterCommon : public IAdapterCommon,
                       public IAdapterPowerManagement,
                       public CUnknown    
{
    public:
        //=====================================================================
        // Default CUnknown
        DECLARE_STD_UNKNOWN();
        DEFINE_STD_CONSTRUCTOR(AdapterCommon);
        ~AdapterCommon();

        //=====================================================================
        // Default IAdapterPowerManagement
        IMP_IAdapterPowerManagement;

        //=====================================================================
        // IAdapterCommon methods                                               
        STDMETHODIMP_(NTSTATUS) Init(IN  PDEVICE_OBJECT  DeviceObject);

        STDMETHODIMP_(PDEVICE_OBJECT) getDeviceObject(void);
        STDMETHODIMP_(NTSTATUS)       instantiateDevices(void);
        STDMETHODIMP_(NTSTATUS)       uninstantiateDevices(void);
        STDMETHODIMP_(NTSTATUS)       plugin(void);
        STDMETHODIMP_(NTSTATUS)       unplug(void);
        STDMETHODIMP_(PUNKNOWN *)     wavePortDriverDest(void);

        STDMETHODIMP_(void)     setWaveServiceGroup(IN  PSERVICEGROUP ServiceGroup);

        STDMETHODIMP_(BOOL)     bDevSpecificRead();
        STDMETHODIMP_(void)     bDevSpecificWrite(IN  BOOL bDevSpecific);
        STDMETHODIMP_(INT)      iDevSpecificRead();
        STDMETHODIMP_(void)     iDevSpecificWrite(IN  INT iDevSpecific);
        STDMETHODIMP_(UINT)     uiDevSpecificRead();
        STDMETHODIMP_(void)     uiDevSpecificWrite(IN  UINT uiDevSpecific);

        STDMETHODIMP_(BOOL)     mixerMuteRead( IN  ULONG Index);
        STDMETHODIMP_(void)     mixerMuteWrite(IN  ULONG Index, IN  BOOL Value);

        STDMETHODIMP_(ULONG)    mixerMuxRead(void);
        STDMETHODIMP_(void)     mixerMuxWrite(IN  ULONG Index);

        STDMETHODIMP_(void)     mixerReset(void);

        STDMETHODIMP_(LONG)     mixerVolumeRead( IN  ULONG Index, IN  LONG Channel);
        STDMETHODIMP_(void)     mixerVolumeWrite(IN  ULONG Index, IN  LONG Channel, IN  LONG Value);

        STDMETHODIMP_(BOOL)     isInstantiated() { return isInstantiated_; };
        STDMETHODIMP_(BOOL)     isPluggedIn()    { return isPluggedIn_; }

        STDMETHODIMP_(NTSTATUS) setInstantiateWorkItem(_In_ __drv_aliasesMem PIO_WORKITEM WorkItem);

        STDMETHODIMP_(NTSTATUS) freeInstantiateWorkItem();

        //=====================================================================
        // friends

        friend NTSTATUS newAdapterCommon(OUT PADAPTERCOMMON * OutAdapterCommon, IN  PRESOURCELIST   ResourceList);

private:
    PUNKNOWN                portWave_;            // Port Wave Interface
    PUNKNOWN                miniportWave_;        // Miniport Wave Interface
    PUNKNOWN                portTopology_;        // Port Mixer Topology Interface
    PUNKNOWN                miniportTopology_;    // Miniport Mixer Topology Interface
    PSERVICEGROUP           serviceGroupWave_;
    PDEVICE_OBJECT          deviceObject_;
    DEVICE_POWER_STATE      powerState_;
    PCMSVADHW               msvadhw_;             // Virtual MSVAD HW object
    PKTIMER                 instantiateTimer_;    // Timer object
    PRKDPC                  instantiateDpc_;      // Deferred procedure call object
    BOOL                    isInstantiated_;      // Flag indicating whether or not subdevices are exposed
    BOOL                    isPluggedIn_;         // Flag indicating whether or not a jack is plugged in
    PIO_WORKITEM            instantiateWorkItem_; // Work Item for instantiate timer callback

    //=====================================================================
    // Helper routines for managing the states of topologies being exposed
    STDMETHODIMP_(NTSTATUS) exposeMixerTopology();
    STDMETHODIMP_(NTSTATUS) exposeWaveTopology();
    STDMETHODIMP_(NTSTATUS) unexposeMixerTopology();
    STDMETHODIMP_(NTSTATUS) unexposeWaveTopology();
    STDMETHODIMP_(NTSTATUS) connectTopologies();
    STDMETHODIMP_(NTSTATUS) disconnectTopologies();
};

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS newAdapterCommon
( 
    OUT PUNKNOWN* Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN UnknownOuter OPTIONAL,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE               PoolType 
)
{
    PAGED_CODE();

    ASSERT(Unknown);

    STD_CREATE_BODY_(AdapterCommon, Unknown, UnknownOuter, PoolType, PADAPTERCOMMON);
}

//=============================================================================
AdapterCommon::~AdapterCommon()
{
    PAGED_CODE();
    DPF_ENTER(("[CAdapterCommon::~CAdapterCommon]"));

    if (instantiateTimer_)
    {
        KeCancelTimer(instantiateTimer_);
        ExFreePoolWithTag(instantiateTimer_, MSVAD_POOLTAG);
    }

    // Since we just canceled the  the instantiate timer, wait for all 
    // queued DPCs to complete before we free the instantiate DPC.
    //
    KeFlushQueuedDpcs();

    if (instantiateDpc_)
    {
        ExFreePoolWithTag( instantiateDpc_, MSVAD_POOLTAG );
        // Should also ensure that this destructor is synchronized
        // with the instantiate timer DPC and work item.
        //
    }

    delete msvadhw_;

    CSaveData::destroyWorkItems();

    if (miniportWave_)
    {
        miniportWave_->Release();
        miniportWave_ = nullptr;
    }

    if (portWave_)
    {
        portWave_->Release();
        portWave_ = nullptr;
    }

    if (miniportTopology_)
    {
        miniportTopology_->Release();
        miniportTopology_ = nullptr;
    }

    if (portTopology_)
    {
        portTopology_->Release();
        portTopology_ = nullptr;
    }

    if (serviceGroupWave_)
    {
        serviceGroupWave_->Release();
    }
}

//=============================================================================
#pragma code_seg()
STDMETHODIMP_(PDEVICE_OBJECT)
AdapterCommon::getDeviceObject()
{
    return deviceObject_;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS AdapterCommon::Init(IN  PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    ASSERT(DeviceObject);

    NTSTATUS ntStatus = STATUS_SUCCESS;

    DPF_ENTER(("[CAdapterCommon::Init]"));

    deviceObject_     = DeviceObject;
    powerState_        = PowerDeviceD0;
    portWave_         = nullptr;
    miniportWave_     = nullptr;
    portTopology_     = nullptr;
    miniportTopology_ = nullptr;
    instantiateTimer_ = nullptr;
    instantiateDpc_   = nullptr;
    isInstantiated_     = FALSE;
    isPluggedIn_        = FALSE;
    instantiateWorkItem_ = nullptr;

    // Initialize HW.
    // 
    msvadhw_ = new (NonPagedPool, MSVAD_POOLTAG)  MSVADHW;
    if (!msvadhw_)
    {
        DPF(D_TERSE, ("Insufficient memory for MSVAD HW"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        msvadhw_->mixerReset();
    }

    CSaveData::setDeviceObject(DeviceObject);   //device object is needed by CSaveData

    // Allocate DPC for instantiation timer.
    //
    if (NT_SUCCESS(ntStatus))
    {
        instantiateDpc_ = (PRKDPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KDPC), MSVAD_POOLTAG);
        if (!instantiateDpc_)
        {
            DPF(D_TERSE, ("[Could not allocate memory for DPC]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Allocate timer for instantiation.
    //
    if (NT_SUCCESS(ntStatus))
    {
        instantiateTimer_ = (PKTIMER)ExAllocatePoolWithTag(NonPagedPool, sizeof(KTIMER), MSVAD_POOLTAG);
        if (!instantiateTimer_)
        {
            DPF(D_TERSE, ("[Could not allocate memory for Timer]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Initialize instantiation timer and DPC.
    //
    if (NT_SUCCESS(ntStatus))
    {
        KeInitializeDpc(instantiateDpc_, InstantiateTimerNotify, this);
        KeInitializeTimerEx(instantiateTimer_, NotificationTimer);

#ifdef _ENABLE_INSTANTIATION_INTERVAL_
        // Set the timer to expire every INSTANTIATE_INTERVAL_MS milliseconds.
        //
        LARGE_INTEGER   liInstantiateInterval = {0};
        liInstantiateInterval.QuadPart = -1 * INSTANTIATE_INTERVAL_MS * HNS_PER_MS;
        KeSetTimerEx(instantiateTimer_, liInstantiateInterval, INSTANTIATE_INTERVAL_MS, instantiateDpc_);
#endif
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Reset mixer registers from registry.
*/
STDMETHODIMP_(void)
AdapterCommon::mixerReset()
{
    PAGED_CODE();
    
    if (msvadhw_)
    {
        msvadhw_->mixerReset();
    }
}

//=============================================================================
/*

Routine Description:

  QueryInterface routine for AdapterCommon

*/
STDMETHODIMP
AdapterCommon::NonDelegatingQueryInterface
(
    _In_         REFIID Interface,
    _COM_Outptr_ PVOID* Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PADAPTERCOMMON(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IAdapterCommon))
    {
        *Object = PVOID(PADAPTERCOMMON(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IAdapterPowerManagement))
    {
        *Object = PVOID(PADAPTERPOWERMANAGEMENT(this));
    }
    else
    {
        *Object = nullptr;
    }

    if (*Object)
    {
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

//=============================================================================
STDMETHODIMP_(void)
AdapterCommon::setWaveServiceGroup(IN PSERVICEGROUP ServiceGroup)
{
    PAGED_CODE();
    
    DPF_ENTER(("[CAdapterCommon::SetWaveServiceGroup]"));
    
    if (serviceGroupWave_)
    {
        serviceGroupWave_->Release();
    }

    serviceGroupWave_ = ServiceGroup;

    if (serviceGroupWave_)
    {
        serviceGroupWave_->AddRef();
    }
}

//=============================================================================
/*
Routine Description:
  Instantiates the wave and topology ports and exposes them.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::instantiateDevices()
{
    PAGED_CODE();

    if (isInstantiated_)
    {
        return STATUS_SUCCESS;
    }

    // If the mixer topology port is not exposed, create and expose it.
    //
    NTSTATUS ntStatus = exposeMixerTopology();

    // Create and expose the wave topology.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = exposeWaveTopology();
    }

    // Register the physical connection between wave and mixer topologies.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = connectTopologies();
    }

    if (NT_SUCCESS(ntStatus))
    {
        isInstantiated_ = TRUE;
        isPluggedIn_ = TRUE;
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Uninstantiates the wave and topology ports.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::uninstantiateDevices()
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    // Check if we're already uninstantiated
    //
    if (!isInstantiated_)
    {
        return ntStatus;
    }

    // Unregister the physical connection between wave and mixer topologies
    // and unregister/unexpose the wave topology. This is the same as being 
    // unplugged.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = unplug();
    }

    // Unregister the topo port
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = unexposeMixerTopology();
    }

    if (NT_SUCCESS(ntStatus))
    {
        isInstantiated_ = FALSE;
        isPluggedIn_ = FALSE;
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Called in response to jacks being plugged in.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::plugin()
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (!isInstantiated_)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (isPluggedIn_)
    {
        return ntStatus;
    }

    // Create and expose the wave topology.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = exposeWaveTopology();
    }

    // Register the physical connection between wave and mixer topologies.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = connectTopologies();
    }

    if (NT_SUCCESS(ntStatus))
    {
        isPluggedIn_ = TRUE;
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Called in response to jacks being unplugged.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::unplug()
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (!isInstantiated_)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (!isPluggedIn_)
    {
        return ntStatus;
    }

    // Unregister the physical connection between wave and mixer topologies.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = disconnectTopologies();
    }

    // Unregister and destroy the wave port
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = unexposeWaveTopology();
    }

    if (NT_SUCCESS(ntStatus))
    {
        isPluggedIn_ = FALSE;
    }

    return ntStatus;
}

/*
  Creates and registers the mixer topology.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::exposeMixerTopology()
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    PAGED_CODE();

    if (portTopology_)
    {
        return ntStatus;
    }

    ntStatus = InstallSubdevice(deviceObject_,
                                nullptr,
                                L"Topology",
                                CLSID_PortTopology,
                                CLSID_PortTopology, 
                                CreateMiniportTopologyMSVAD,
                                PUNKNOWN(PADAPTERCOMMON(this)),
                                nullptr,
                                &portTopology_,
                                &miniportTopology_);
    return ntStatus;
}

/*
  Creates and registers wave topology.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::exposeWaveTopology()
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    PAGED_CODE();

    if (portWave_)
    {
        return ntStatus;
    }

    ntStatus = InstallSubdevice(deviceObject_,
                                nullptr,
                                L"Wave",
                                CLSID_PortWaveCyclic,
                                CLSID_PortWaveCyclic,   
                                CreateMiniportWaveCyclicMSVAD,
                                PUNKNOWN(PADAPTERCOMMON(this)),
                                nullptr,
                                &portWave_,
                                &miniportWave_);
    return ntStatus;
}

//  Unregisters and releases the mixer topology.
STDMETHODIMP_(NTSTATUS) 
AdapterCommon::unexposeMixerTopology()
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PUNREGISTERSUBDEVICE pUnregisterSubdevice = nullptr;

    PAGED_CODE();

    if (!portTopology_)
    {
        return ntStatus;
    }

    // Get the IUnregisterSubdevice interface.
    //
    ntStatus = portTopology_->QueryInterface(IID_IUnregisterSubdevice, (PVOID *)&pUnregisterSubdevice);

    // Unregister the topo port.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = pUnregisterSubdevice->UnregisterSubdevice(deviceObject_, portTopology_);

        // Release the IUnregisterSubdevice interface.
        //
        pUnregisterSubdevice->Release();

        // At this point, we're done with the mixer topology and 
        // the miniport.
        //
        if (NT_SUCCESS(ntStatus))
        {
            portTopology_->Release();
            portTopology_ = nullptr;

            miniportTopology_->Release();
            miniportTopology_ = nullptr;
        }
    }

    return ntStatus;
}

/*
Routine Description:
  Unregisters and releases the wave topology.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::unexposeWaveTopology()
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PUNREGISTERSUBDEVICE pUnregisterSubdevice = nullptr;

    PAGED_CODE();

    if (!portWave_)
    {
        return ntStatus;
    }

    // Get the IUnregisterSubdevice interface.
    //
    ntStatus = portWave_->QueryInterface(IID_IUnregisterSubdevice, (PVOID *)&pUnregisterSubdevice);

    // Unregister the wave port.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = pUnregisterSubdevice->UnregisterSubdevice(deviceObject_, portWave_);
    
        // Release the IUnregisterSubdevice interface.
        //
        pUnregisterSubdevice->Release();

        // At this point, we're done with the mixer topology and 
        // the miniport.
        //
        if (NT_SUCCESS(ntStatus))
        {
            portWave_->Release();
            portWave_ = nullptr;

            miniportWave_->Release();
            miniportWave_ = nullptr;
        }
    }
    return ntStatus;
}

/*
  Connects the bridge pins between the wave and mixer topologies.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::connectTopologies()
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    PAGED_CODE();

    // Connect the capture path.
    //
    if ((TopologyPhysicalConnections.topologyOut != (ULONG)-1) &&
        (TopologyPhysicalConnections.waveIn      != (ULONG)-1))
    {
        ntStatus = PcRegisterPhysicalConnection(deviceObject_,
                                                portTopology_, TopologyPhysicalConnections.topologyOut,
                                                portWave_,     TopologyPhysicalConnections.waveIn);
    }

    // Connect the render path.
    //
    if (NT_SUCCESS(ntStatus))
    {
        if ((TopologyPhysicalConnections.waveOut    != (ULONG)-1) &&
            (TopologyPhysicalConnections.topologyIn != (ULONG)-1))
        {
            ntStatus = PcRegisterPhysicalConnection(deviceObject_,
                                                    portWave_,     TopologyPhysicalConnections.waveOut,
                                                    portTopology_, TopologyPhysicalConnections.topologyIn);
        }
    }

    return ntStatus;
}

/*
  Disconnects the bridge pins between the wave and mixer topologies.
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::disconnectTopologies()
{
    NTSTATUS                        ntStatus    = STATUS_SUCCESS;
    NTSTATUS                        ntStatus2   = STATUS_SUCCESS;
    PUNREGISTERPHYSICALCONNECTION   pUnregisterPhysicalConnection = nullptr;

    PAGED_CODE();

    //
    // Get the IUnregisterPhysicalConnection interface
    //
    ntStatus = portTopology_->QueryInterface( IID_IUnregisterPhysicalConnection, (PVOID *)&pUnregisterPhysicalConnection);
    if (NT_SUCCESS(ntStatus))
    {
        // 
        // Remove the render physical connection
        //
        if ((TopologyPhysicalConnections.waveOut    != (ULONG)-1) &&
            (TopologyPhysicalConnections.topologyIn != (ULONG)-1))
        {
            ntStatus = pUnregisterPhysicalConnection->UnregisterPhysicalConnection(deviceObject_,
                                                                                   portWave_,     TopologyPhysicalConnections.waveOut,
                                                                                   portTopology_, TopologyPhysicalConnections.topologyIn);
            if(!NT_SUCCESS(ntStatus))
            {
                DPF(D_TERSE, ("DisconnectTopologies: UnregisterPhysicalConnection(render) failed, 0x%x", ntStatus));
            }
        }

        //
        // Remove the capture physical connection
        //
        if ((TopologyPhysicalConnections.topologyOut != (ULONG)-1) &&
            (TopologyPhysicalConnections.waveIn != (ULONG)-1))
        {
            ntStatus2 = pUnregisterPhysicalConnection->UnregisterPhysicalConnection(deviceObject_,
                                                                                    portTopology_, TopologyPhysicalConnections.topologyOut,
                                                                                    portWave_,     TopologyPhysicalConnections.waveIn);            
            if(!NT_SUCCESS(ntStatus2))
            {
                DPF(D_TERSE, ("DisconnectTopologies: UnregisterPhysicalConnection(capture) failed, 0x%x", ntStatus2));
                if (NT_SUCCESS(ntStatus))
                {
                    ntStatus = ntStatus2;
                }
            }
        }
    }

    SAFE_RELEASE(pUnregisterPhysicalConnection);
    
    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  Sets the work item that will be called to instantiate or uninstantiate topologies.

Arguments:

  PIO_WORKITEM - [in] work item that was previously allocated.

*/
#pragma code_seg()
STDMETHODIMP_(NTSTATUS)
AdapterCommon::setInstantiateWorkItem(_In_ __drv_aliasesMem PIO_WORKITEM WorkItem)
{
    // Make sure there isn't already a work item allocated.
    //
    if ( instantiateWorkItem_ != nullptr )
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    // Stash the work item to be free'd after the work routine is called.
    //
    instantiateWorkItem_ = WorkItem;

    return STATUS_SUCCESS;
}

//=============================================================================
/*

Routine Description:

  Frees a work item that was called to instantiate or 
  uninstantiate topologies.
  
*/
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
AdapterCommon::freeInstantiateWorkItem()
{
    PAGED_CODE();

    // Make sure we actually have a work item set.
    //
    if ( instantiateWorkItem_ == nullptr )
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    // Go ahead and free the work item.
    //
    IoFreeWorkItem( instantiateWorkItem_ );
    instantiateWorkItem_ = nullptr;

    return STATUS_SUCCESS;
}

//=============================================================================
STDMETHODIMP_(PUNKNOWN *)
AdapterCommon::wavePortDriverDest()
{
    PAGED_CODE();

    return &portWave_;
}

#pragma code_seg()

//=============================================================================
/*
Routine Description:
  Fetch Device Specific information.
  BOOL - Device Specific info
*/
STDMETHODIMP_(BOOL)
AdapterCommon::bDevSpecificRead()
{
    if (msvadhw_)
    {
        return msvadhw_->bGetDevSpecific();
    }

    return FALSE;
}

//=============================================================================
/*
Routine Description:
  Store the new value in the Device Specific location.
Arguments:
  bDevSpecific - Value to store
*/
STDMETHODIMP_(void)
AdapterCommon::bDevSpecificWrite(IN  BOOL devSpecific)
{
    if (msvadhw_)
    {
        msvadhw_->bSetDevSpecific(devSpecific);
    }
}

//=============================================================================
/*
Routine Description:
  Fetch Device Specific information.

    INT - Device Specific info
*/
STDMETHODIMP_(INT)
AdapterCommon::iDevSpecificRead()
{
    if (msvadhw_)
    {
        return msvadhw_->iGetDevSpecific();
    }

    return 0;
}

//=============================================================================
/*
Routine Description:
  Store the new value in the Device Specific location.
*/
STDMETHODIMP_(void)
AdapterCommon::iDevSpecificWrite(IN  INT devSpecific)
{
    if (msvadhw_)
    {
        msvadhw_->iSetDevSpecific(devSpecific);
    }
}

//=============================================================================
/*
Routine Description:
  Fetch Device Specific information.
*/
STDMETHODIMP_(UINT)
AdapterCommon::uiDevSpecificRead()
{
    if (msvadhw_)
    {
        return msvadhw_->uiGetDevSpecific();
    }

    return 0;
}

//=============================================================================
/*
Routine Description:
  Store the new value in the Device Specific location.
*/
STDMETHODIMP_(void)
AdapterCommon::uiDevSpecificWrite(IN  UINT devSpecific)
{
    if (msvadhw_)
    {
        msvadhw_->uiSetDevSpecific(devSpecific);
    }
}

//=============================================================================
/*
Routine Description:
  Store the new value in mixer register array.

Arguments:
  Index - node id
    BOOL - mixer mute setting for this node
*/
STDMETHODIMP_(BOOL)
AdapterCommon::mixerMuteRead(IN  ULONG index)
{
    if (msvadhw_)
    {
        return msvadhw_->getMixerMute(index);
    }

    return 0;
}

//=============================================================================
/*
Routine Description:
  Store the new value in mixer register array.

Arguments:
  Index - node id
  Value - new mute settings
*/
STDMETHODIMP_(void)
AdapterCommon::mixerMuteWrite(IN  ULONG Index, IN  BOOL Value)
{
    if (msvadhw_)
    {
        msvadhw_->setMixerMute(Index, Value);
    }
}

//=============================================================================
/*
Routine Description:
  Return the mux selection

Arguments:
  Index - node id
  Value - new mute settings
*/
STDMETHODIMP_(ULONG)
AdapterCommon::mixerMuxRead()
{
    if (msvadhw_)
    {
        return msvadhw_->getMixerMux();
    }

    return 0;
}

//=============================================================================
/*
Routine Description:
  Store the new mux selection

Arguments:
  Index - node id
  Value - new mute settings
*/
STDMETHODIMP_(void)
AdapterCommon::mixerMuxWrite(IN  ULONG Index)
{
    if (msvadhw_)
    {
        msvadhw_->setMixerMux(Index);
    }
}

//=============================================================================
/*
Routine Description:
  Return the value in mixer register array.

Arguments:
  Index - node id

  Channel = which channel
    Byte - mixer volume settings for this line
*/
STDMETHODIMP_(LONG)
AdapterCommon::mixerVolumeRead(IN  ULONG Index, IN  LONG Channel)
{
    if (msvadhw_)
    {
        return msvadhw_->getMixerVolume(Index, Channel);
    }

    return 0;
}

//=============================================================================
/*
Routine Description:
  Store the new value in mixer register array.

Arguments:

  Index   - node id
  Channel - which channel
  Value   - new volume level
*/
STDMETHODIMP_(void)
AdapterCommon::mixerVolumeWrite(IN  ULONG Index, IN  LONG Channel, IN  LONG Value)
{
    if (msvadhw_)
    {
        msvadhw_->setMixerVolume(Index, Channel, Value);
    }
}

//=============================================================================
/*
Arguments:
  NewState - The requested, new power state for the device.
*/
STDMETHODIMP_(void)
AdapterCommon::PowerChangeState(_In_  POWER_STATE NewState)
{
    DPF_ENTER(("[CAdapterCommon::PowerChangeState]"));

    // is this actually a state change??
    //
    if (NewState.DeviceState != powerState_)
    {
        // switch on new state
        //
        switch (NewState.DeviceState)
        {
            case PowerDeviceD0:
            case PowerDeviceD1:
            case PowerDeviceD2:
            case PowerDeviceD3:
                powerState_ = NewState.DeviceState;
                DPF(D_VERBOSE, ("Entering D%d", ULONG(powerState_) - ULONG(PowerDeviceD0)));
                break;
            default:
                DPF(D_VERBOSE, ("Unknown Device Power State"));
                break;
        }
    }
}

//=============================================================================
/*
Routine Description:

    Called at startup to get the caps for the device.  This structure provides 
    the system with the mappings between system power state and device power 
    state.  This typically will not need modification by the driver.         

Arguments:
  PowerDeviceCaps - The device's capabilities. 
*/
_Use_decl_annotations_
STDMETHODIMP_(NTSTATUS)
AdapterCommon::QueryDeviceCapabilities(PDEVICE_CAPABILITIES PowerDeviceCaps)
{
    UNREFERENCED_PARAMETER(PowerDeviceCaps);

    DPF_ENTER(("[CAdapterCommon::QueryDeviceCapabilities]"));

    return (STATUS_SUCCESS);
}

//=============================================================================
/*
Routine Description:
  Query to see if the device can change to this power state 

Arguments:
  NewStateQuery - The requested, new power state for the device
*/
STDMETHODIMP_(NTSTATUS)
AdapterCommon::QueryPowerChangeState(_In_  POWER_STATE NewStateQuery)
{
    UNREFERENCED_PARAMETER(NewStateQuery);

    DPF_ENTER(("[CAdapterCommon::QueryPowerChangeState]"));

    return STATUS_SUCCESS;
}
