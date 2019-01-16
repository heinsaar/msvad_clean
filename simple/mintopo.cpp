/*
Abstract:
    Implementation of topology miniport.
*/

#pragma warning (disable : 4127)

#include <msvad.h>
#include <common.h>
#include "simple.h"
#include "minwave.h"
#include "mintopo.h"
#include "toptable.h"


/*********************************************************************
* Topology/Wave bridge connection                                    *
*                                                                    *
*              +------+    +------+                                  *
*              | Wave |    | Topo |                                  *
*              |      |    |      |                                  *
*  Capture <---|0    1|<===|4    1|<--- Synth                        *
*              |      |    |      |                                  *
*   Render --->|2    3|===>|0     |                                  *
*              +------+    |      |                                  *
*                          |     2|<--- Mic                          *
*                          |      |                                  *
*                          |     3|---> Line Out                     *
*                          +------+                                  *
*********************************************************************/
PHYSICALCONNECTIONTABLE TopologyPhysicalConnections =
{
    KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
    KSPIN_TOPO_WAVEIN_DEST,     // TopologyOut
    KSPIN_WAVE_CAPTURE_SOURCE,  // WaveIn
    KSPIN_WAVE_RENDER_SOURCE    // WaveOut
};

#pragma code_seg("PAGE")

//=============================================================================
NTSTATUS CreateMiniportTopologyMSVAD
( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE               PoolType 
)
{
    PAGED_CODE();

    ASSERT(Unknown);

    STD_CREATE_BODY(MiniportTopology, Unknown, UnknownOuter, PoolType);
}

//=============================================================================
MiniportTopology::~MiniportTopology()
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportTopology::~CMiniportTopology]"));
}

//=============================================================================
/*
Routine Description:
  The DataRangeIntersection function determines the highest quality 
  intersection of two data ranges.

Arguments:

  PinId                 - Pin for which data intersection is being determined. 

  ClientDataRange       - Pointer to KSDATARANGE structure which contains the data range 
                          submitted by client in the data range intersection property request. 

  MyDataRange           - Pin's data range to be compared with client's data range. 

  OutputBufferLength    - Size of the buffer pointed to by the resultant format parameter. 

  ResultantFormat       - Pointer to value where the resultant format should be returned. 

  ResultantFormatLength - Actual length of the resultant format that is placed 
                          at ResultantFormat. This should be less than or equal 
                          to OutputBufferLength. 
*/
NTSTATUS MiniportTopology::DataRangeIntersection
(
    _In_        ULONG        PinId,
    _In_        PKSDATARANGE ClientDataRange,
    _In_        PKSDATARANGE MyDataRange,
    _In_        ULONG        OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
    PVOID                    ResultantFormat,
    _Out_       PULONG       ResultantFormatLength
)
{
    PAGED_CODE();

    return MiniportTopologyMSVAD::DataRangeIntersection(PinId,
                                                         ClientDataRange,
                                                         MyDataRange,
                                                         OutputBufferLength,
                                                         ResultantFormat,
                                                         ResultantFormatLength);
}

//=============================================================================
/*
Routine Description:

  The GetDescription function gets a pointer to a filter description. 
  It provides a location to deposit a pointer in miniport's description 
  structure. This is the placeholder for the FromNode or ToNode fields in 
  connections which describe connections to the filter's pins. 

Arguments:
  OutFilterDescriptor - Pointer to the filter description. 
*/
STDMETHODIMP MiniportTopology::GetDescription(_Out_ PPCFILTER_DESCRIPTOR* outFilterDescriptor)
{
    PAGED_CODE();

    return MiniportTopologyMSVAD::GetDescription(outFilterDescriptor);
}

//=============================================================================
/*
Routine Description:

  The Init function initializes the miniport. Callers of this function 
  should run at IRQL PASSIVE_LEVEL

Arguments:

  UnknownAdapter - A pointer to the Iuknown interface of the adapter object. 

  ResourceList - Pointer to the resource list to be supplied to the miniport 
                 during initialization. The port driver is free to examine the 
                 contents of the ResourceList. The port driver will not be 
                 modify the ResourceList contents. 

  Port - Pointer to the topology port object that is linked with this miniport. 
*/
STDMETHODIMP
MiniportTopology::Init
(
    _In_ PUNKNOWN                 UnknownAdapter,
    _In_ PRESOURCELIST            ResourceList,
    _In_ PPORTTOPOLOGY            Port_
)
{
    UNREFERENCED_PARAMETER(ResourceList);

    PAGED_CODE();

    ASSERT(UnknownAdapter);
    ASSERT(Port_);

    DPF_ENTER(("[CMiniportTopology::Init]"));

    NTSTATUS ntStatus = MiniportTopologyMSVAD::Init(UnknownAdapter, Port_);

    if (NT_SUCCESS(ntStatus))
    {
        filterDescriptor_ = &MiniportFilterDescriptor;
        adapterCommon_->MixerMuxWrite(KSPIN_TOPO_MIC_SOURCE);
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:

  QueryInterface for MiniportTopology

Arguments:

  Interface - GUID of the interface
  Object - interface object to be returned.
*/
STDMETHODIMP
MiniportTopology::NonDelegatingQueryInterface
(
    _In_         REFIID Interface,
    _COM_Outptr_ PVOID* Object
)
{
    PAGED_CODE();
    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
    {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
    }
    else
    {
        *Object = nullptr;
    }

    if (*Object)
    {
        // We reference the interface for the caller.
        PUNKNOWN(*Object)->AddRef();
        return(STATUS_SUCCESS);
    }

    return(STATUS_INVALID_PARAMETER);
}

//=============================================================================
/*
Routine Description:
  Handles ( KSPROPSETID_Jack, KSPROPERTY_JACK_DESCRIPTION )
*/
NTSTATUS MiniportTopology::PropertyHandlerJackDescription(IN PPCPROPERTY_REQUEST PropertyRequest)
{
    PAGED_CODE();
    ASSERT(PropertyRequest);
    DPF_ENTER(("[PropertyHandlerJackDescription]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    ULONG    nPinId = (ULONG)-1;

    if (PropertyRequest->InstanceSize >= sizeof(ULONG))
    {
        nPinId = *(PULONG(PropertyRequest->Instance));

        if ((nPinId < ARRAYSIZE(JackDescriptions)) && (JackDescriptions[nPinId] != nullptr))
        {
            if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
            {
                ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET, VT_ILLEGAL);
            }
            else
            {
                ULONG cbNeeded = sizeof(KSMULTIPLE_ITEM) + sizeof(KSJACK_DESCRIPTION);

                if (PropertyRequest->ValueSize == 0)
                {
                    PropertyRequest->ValueSize = cbNeeded;
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                }
                else if (PropertyRequest->ValueSize < cbNeeded)
                {
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }
                else
                {
                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        PKSMULTIPLE_ITEM pMI = (PKSMULTIPLE_ITEM)PropertyRequest->Value;
                        PKSJACK_DESCRIPTION pDesc = (PKSJACK_DESCRIPTION)(pMI+1);

                        pMI->Size = cbNeeded;
                        pMI->Count = 1;

                        RtlCopyMemory(pDesc, JackDescriptions[nPinId], sizeof(KSJACK_DESCRIPTION));
                        ntStatus = STATUS_SUCCESS;
                    }
                }
            }
        }
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Redirects property request to miniport object
*/
NTSTATUS PropertyHandler_TopoFilter(IN PPCPROPERTY_REQUEST PropertyRequest)
{
    PAGED_CODE();
    ASSERT(PropertyRequest);
    DPF_ENTER(("[PropertyHandler_TopoFilter]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportTopology  pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Jack) &&
                           (PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION))
    {
        ntStatus = pMiniport->PropertyHandlerJackDescription(PropertyRequest);
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Redirects property request to miniport object
*/
NTSTATUS PropertyHandler_Topology(IN PPCPROPERTY_REQUEST PropertyRequest)
{
    PAGED_CODE();
    ASSERT(PropertyRequest);
    DPF_ENTER(("[PropertyHandler_Topology]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    return ((PCMiniportTopology)(PropertyRequest->MajorTarget))->PropertyHandlerGeneric(PropertyRequest);
}

#pragma code_seg()