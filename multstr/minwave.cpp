/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    minwave.cpp

Abstract:

    Implementation of wavecyclic miniport.

--*/

#pragma warning (disable : 4127)

#include <msvad.h>
#include <common.h>
#include "multi.h"
#include "minwave.h"
#include "wavtable.h"

#pragma code_seg("PAGE")

//=============================================================================
// CMiniportWaveCyclic
//=============================================================================

//=============================================================================
NTSTATUS
CreateMiniportWaveCyclicMSVAD
(
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE               PoolType
)
/*++

Routine Description:

  Create the wavecyclic miniport.

Arguments:

  Unknown -

  RefClsId -

  UnknownOuter -

  PoolType -

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();
    ASSERT(Unknown);
    STD_CREATE_BODY_(CMiniportWaveCyclic, Unknown, UnknownOuter, PoolType,PMINIPORTWAVECYCLIC);
}

//=============================================================================
CMiniportWaveCyclic::~CMiniportWaveCyclic()
/*
Routine Description:
  Destructor for wavecyclic miniport
*/
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclic::~CMiniportWaveCyclic]"));
}


//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclic::DataRangeIntersection
(
    _In_        ULONG                       PinId,
    _In_        PKSDATARANGE                ClientDataRange,
    _In_        PKSDATARANGE                MyDataRange,
    _In_        ULONG                       OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID                       ResultantFormat,
    _Out_       PULONG                      ResultantFormatLength
)
/*++

Routine Description:

  The DataRangeIntersection function determines the highest quality
  intersection of two data ranges.

Arguments:

  PinId -           Pin for which data intersection is being determined.

  ClientDataRange - Pointer to KSDATARANGE structure which contains the data
                    range submitted by client in the data range intersection
                    property request.

  MyDataRange -         Pin's data range to be compared with client's data
                        range. In this case we actually ignore our own data
                        range, because we know that we only support one range.

  OutputBufferLength -  Size of the buffer pointed to by the resultant format
                        parameter.

  ResultantFormat -     Pointer to value where the resultant format should be
                        returned.

  ResultantFormatLength -   Actual length of the resultant format placed in
                            ResultantFormat. This should be less than or equal
                            to OutputBufferLength.
--*/
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ClientDataRange);
    UNREFERENCED_PARAMETER(MyDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    UNREFERENCED_PARAMETER(ResultantFormatLength);

    PAGED_CODE();

    // This driver only supports PCM formats.
    // Portcls will handle the request for us.
    //

    return STATUS_NOT_IMPLEMENTED;
}

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclic::GetDescription
(
    _Out_ PPCFILTER_DESCRIPTOR * OutFilterDescriptor
)
/*++

Routine Description:
  The GetDescription function gets a pointer to a filter description.
  It provides a location to deposit a pointer in miniport's description
  structure. This is the placeholder for the FromNode or ToNode fields in
  connections which describe connections to the filter's pins.

Arguments:
  OutFilterDescriptor - Pointer to the filter description.
*/
{
    PAGED_CODE();
    ASSERT(OutFilterDescriptor);
    return CMiniportWaveCyclicMSVAD::GetDescription(OutFilterDescriptor);
}

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclic::Init
(
    _In_  PUNKNOWN                UnknownAdapter_,
    _In_  PRESOURCELIST           ResourceList_,
    _In_  PPORTWAVECYCLIC         Port_
)
/*++

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

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(UnknownAdapter_);
    ASSERT(Port_);

    NTSTATUS                    ntStatus;

    DPF_ENTER(("[CMiniportWaveCyclic::Init]"));

    m_MaxOutputStreams      = MAX_OUTPUT_STREAMS;
    m_MaxInputStreams       = MAX_INPUT_STREAMS;
    m_MaxTotalStreams       = MAX_TOTAL_STREAMS;

    m_MinChannels           = MIN_CHANNELS;
    m_MaxChannelsPcm        = MAX_CHANNELS_PCM;

    m_MinBitsPerSamplePcm   = MIN_BITS_PER_SAMPLE_PCM;
    m_MaxBitsPerSamplePcm   = MAX_BITS_PER_SAMPLE_PCM;
    m_MinSampleRatePcm      = MIN_SAMPLE_RATE;
    m_MaxSampleRatePcm      = MAX_SAMPLE_RATE;

    ntStatus = CMiniportWaveCyclicMSVAD::Init(UnknownAdapter_, ResourceList_, Port_);
    if (NT_SUCCESS(ntStatus))
    {
        // Set filter descriptor.
        m_FilterDescriptor = &MiniportFilterDescriptor;

        m_fCaptureAllocated = FALSE;
        RtlZeroMemory(m_pStream, MAX_INPUT_STREAMS * sizeof(PCMiniportWaveCyclicStream));
    }

    return ntStatus;
}

//=============================================================================
_Use_decl_annotations_
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclic::NewStream
(
    PMINIPORTWAVECYCLICSTREAM * OutStream,
    PUNKNOWN                OuterUnknown,
    POOL_TYPE               PoolType,
    ULONG                   Pin,
    BOOLEAN                 Capture,
    PKSDATAFORMAT           DataFormat,
    PDMACHANNEL *           OutDmaChannel,
    PSERVICEGROUP *         OutServiceGroup
)
/*++

Routine Description:

  The NewStream function creates a new instance of a logical stream
  associated with a specified physical channel. Callers of NewStream should
  run at IRQL PASSIVE_LEVEL.

Arguments:

  OutStream -

  OuterUnknown -

  PoolType -

  Pin -

  Capture -

  DataFormat -

  OutDmaChannel -

  OutServiceGroup -

Return Value:

  NT status code.

--*/
{
    UNREFERENCED_PARAMETER(PoolType);

    PAGED_CODE();

    ASSERT(OutStream);
    ASSERT(DataFormat);
    ASSERT(OutDmaChannel);
    ASSERT(OutServiceGroup);

    DPF_ENTER(("[CMiniportWaveCyclic::NewStream]"));

    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PCMiniportWaveCyclicStream  stream = nullptr;
    ULONG                       streamIndex = 0;

    // MSVAD supports one capture stream.
    //
    if (Capture)
    {
        if (m_fCaptureAllocated)
        {
            DPF(D_TERSE, ("[Only one capture stream supported]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    // This version supports multiple playback streams.
    //
    else
    {
        for (streamIndex = 0; streamIndex < m_MaxInputStreams; streamIndex++)
        {
            if (!m_pStream[streamIndex])
            {
                break;
            }
        }
        if (streamIndex == m_MaxInputStreams)
        {
            DPF(D_TERSE, ("[All render streams are in use]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Determine if the format is valid.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ValidateFormat(DataFormat);
    }

    // Instantiate a stream. Stream must be in
    // NonPagedPool because of file saving.
    //
    if (NT_SUCCESS(ntStatus))
    {
        stream = new (NonPagedPool, MSVAD_POOLTAG) CMiniportWaveCyclicStream(OuterUnknown);

        if (stream)
        {
            stream->AddRef();

            ntStatus = stream->Init(this, Pin, Capture, DataFormat);
        }
        else
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        if (Capture)
        {
            m_fCaptureAllocated = TRUE;
        }
        else
        {
            m_pStream[streamIndex] = stream;
        }

        *OutStream = PMINIPORTWAVECYCLICSTREAM(stream);
        (*OutStream)->AddRef();

        *OutDmaChannel = PDMACHANNEL(stream);
        (*OutDmaChannel)->AddRef();

        *OutServiceGroup = m_ServiceGroup;
        (*OutServiceGroup)->AddRef();

        // The stream, the DMA channel, and the service group have
        // references now for the caller.  The caller expects these
        // references to be there.
    }

    // This is our private reference to the stream.  The caller has
    // its own, so we can release in any case.
    //
    if (stream)
    {
        stream->Release();
    }

    return ntStatus;
}

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclic::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * Object
)
/*++

Routine Description:

  QueryInterface

Arguments:

  Interface - GUID

  Object - interface pointer to be returned.

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLIC(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclic))
    {
        *Object = PVOID(PMINIPORTWAVECYCLIC(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IPinCount))
    {
        *Object = PVOID(PPINCOUNT(this));
    }
    else
    {
        *Object = nullptr;
    }

    if (*Object)
    {
        // We reference the interface for the caller.

        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

//=============================================================================
STDMETHODIMP_(void)
CMiniportWaveCyclic::PinCount
(
    _In_    ULONG   PinId,
    _Inout_ PULONG  FilterNecessary,
    _Inout_ PULONG  FilterCurrent,
    _Inout_ PULONG  FilterPossible,
    _Inout_ PULONG  GlobalCurrent,
    _Inout_ PULONG  GlobalPossible
)
/*++

Routine Description:

  Provide a way for the miniport to modify the pin counts for this miniport.

Arguments:

  PinId - KS pin number being referenced

  FilterNecessary - number of pins required on this pin factory
  FilterCurrent   - number of pins opened on this pin factory
  FilterPossible  - number of pins possible on this pin factory
  GlobalCurrent   - total number of pins opened, across all pin instances on this filter
  GlobalPossible  - total number of pins possible, across all pin factories on this filter

Return Value:

  OUT parameters for the five pin counts.

--*/
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(FilterNecessary);
    UNREFERENCED_PARAMETER(FilterCurrent);
    UNREFERENCED_PARAMETER(FilterPossible);
    UNREFERENCED_PARAMETER(GlobalCurrent);
    UNREFERENCED_PARAMETER(GlobalPossible);

    PAGED_CODE();

    _DbgPrintF( DEBUGLVL_VERBOSE,
                ("PinCount PID:0x%08x FN(0x%08p):%d FC(0x%08p):%d FP(0x%08p):%d GC(0x%08p):%d GP(0x%08p):%d",
                  PinId,
                  FilterNecessary,*FilterNecessary,
                  FilterCurrent,  *FilterCurrent,
                  FilterPossible, *FilterPossible,
                  GlobalCurrent,  *GlobalCurrent,
                  GlobalPossible, *GlobalPossible ) );

    //
    // Something like the following:
    //
//    if (0 == PinId)
//    {
//        *FilterPossible += 1;
//    }

}

//=============================================================================
// CMiniportWaveStreamCyclicSimple
//=============================================================================

//=============================================================================
CMiniportWaveCyclicStream::~CMiniportWaveCyclicStream
(

)
/*++

Routine Description:

  Destructor for wavecyclicstream

Arguments:

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStream::~CMiniportWaveCyclicStream]"));

    if (nullptr != m_pMiniportLocal)
    {
        // Tell the Miniport that the slot is freed now.
        //
        if (m_fCapture)
        {
            m_pMiniportLocal->m_fCaptureAllocated = FALSE;
        }
        else
        {
            for (ULONG i = 0; i < m_pMiniportLocal->m_MaxInputStreams; i++)
            {
                if (this == m_pMiniportLocal->m_pStream[i])
                {
                    m_pMiniportLocal->m_pStream[i] = nullptr;
                    break;
                }
            }
        }
    }
}

//=============================================================================
NTSTATUS
CMiniportWaveCyclicStream::Init
(
    IN PCMiniportWaveCyclic Miniport_,
    IN ULONG                Pin_,
    IN BOOLEAN              Capture_,
    IN PKSDATAFORMAT        DataFormat_
)
/*++
Routine Description:
  Initializes the stream object. Allocate a DMA buffer, timer and DPC
*/
{
    PAGED_CODE();

    m_pMiniportLocal = Miniport_;

    return CMiniportWaveCyclicStreamMSVAD::Init(Miniport_, Pin_, Capture_, DataFormat_);
}

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicStream::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * Object
)
/*++

Routine Description:

  QueryInterface

Arguments:

  Interface - GUID

  Object - interface pointer to be returned

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLICSTREAM(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclicStream))
    {
        *Object = PVOID(PMINIPORTWAVECYCLICSTREAM(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IDmaChannel))
    {
        *Object = PVOID(PDMACHANNEL(this));
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
#pragma code_seg()

