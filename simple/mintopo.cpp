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
    _In_        ULONG        pinId,
    _In_        PKSDATARANGE clientDataRange,
    _In_        PKSDATARANGE myDataRange,
    _In_        ULONG        outputBufferLength,
    _Out_writes_bytes_to_opt_(outputBufferLength, *resultantFormatLength)
    PVOID                    resultantFormat,
    _Out_       PULONG       resultantFormatLength
)
{
    PAGED_CODE();

    return MiniportTopologyMSVAD::DataRangeIntersection(pinId,
                                                        clientDataRange,
                                                        myDataRange,
                                                        outputBufferLength,
                                                        resultantFormat,
                                                        resultantFormatLength);
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
    _In_ PUNKNOWN      unknownAdapter,
    _In_ PRESOURCELIST resourceList,
    _In_ PPORTTOPOLOGY port
)
{
    UNREFERENCED_PARAMETER(resourceList);

    PAGED_CODE();

    ASSERT(unknownAdapter);
    ASSERT(port);

    DPF_ENTER(("[CMiniportTopology::Init]"));

    NTSTATUS ntStatus = MiniportTopologyMSVAD::Init(unknownAdapter, port);

    if (NT_SUCCESS(ntStatus))
    {
        filterDescriptor_ = &MiniportFilterDescriptor;
        adapterCommon_->mixerMuxWrite(KSPIN_TOPO_MIC_SOURCE);
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
    _COM_Outptr_ PVOID* object
)
{
    PAGED_CODE();
    ASSERT(object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
    {
        *object = PVOID(PMINIPORTTOPOLOGY(this));
    }
    else
    {
        *object = nullptr;
    }

    if (*object)
    {
        // We reference the interface for the caller.
        PUNKNOWN(*object)->AddRef();
        return(STATUS_SUCCESS);
    }

    return(STATUS_INVALID_PARAMETER);
}

//=============================================================================
/*
Routine Description:
  Handles ( KSPROPSETID_Jack, KSPROPERTY_JACK_DESCRIPTION )
*/
NTSTATUS MiniportTopology::propertyHandlerJackDescription(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    ASSERT(propertyRequest);
    DPF_ENTER(("[PropertyHandlerJackDescription]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    ULONG    nPinId = (ULONG)-1;

    if (propertyRequest->InstanceSize >= sizeof(ULONG))
    {
        nPinId = *(PULONG(propertyRequest->Instance));

        if ((nPinId < ARRAYSIZE(JackDescriptions)) && (JackDescriptions[nPinId] != nullptr))
        {
            if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
            {
                ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET, VT_ILLEGAL);
            }
            else
            {
                ULONG cbNeeded = sizeof(KSMULTIPLE_ITEM) + sizeof(KSJACK_DESCRIPTION);

                if (propertyRequest->ValueSize == 0)
                {
                    propertyRequest->ValueSize = cbNeeded;
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                }
                else if (propertyRequest->ValueSize < cbNeeded)
                {
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }
                else
                {
                    if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        PKSMULTIPLE_ITEM      pMI = (PKSMULTIPLE_ITEM)propertyRequest->Value;
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
NTSTATUS propertyHandler_TopoFilter(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    ASSERT(propertyRequest);
    DPF_ENTER(("[PropertyHandler_TopoFilter]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportTopology  miniport = (PCMiniportTopology)propertyRequest->MajorTarget;

    if (IsEqualGUIDAligned(*propertyRequest->PropertyItem->Set, KSPROPSETID_Jack) &&
                           (propertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION))
    {
        ntStatus = miniport->propertyHandlerJackDescription(propertyRequest);
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Redirects property request to miniport object
*/
NTSTATUS propertyHandler_Topology(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    ASSERT(propertyRequest);
    DPF_ENTER(("[PropertyHandler_Topology]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    return ((PCMiniportTopology)(propertyRequest->MajorTarget))->propertyHandlerGeneric(propertyRequest);
}

#pragma code_seg()