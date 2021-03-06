/*
    Implementation of wavecyclic miniport.
*/

#pragma warning (disable : 4127)

#include <msvad.h>
#include <common.h>
#include "simple.h"
#include "minwave.h"
#include "wavtable.h"

#pragma code_seg("PAGE")

//=============================================================================
// CMiniportWaveCyclic
//=============================================================================

NTSTATUS CreateMiniportWaveCyclicMSVAD
( 
    OUT PUNKNOWN* unknown,
    IN  REFCLSID,
    IN  PUNKNOWN  unknownOuter OPTIONAL,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE PoolType 
)
{
    PAGED_CODE();
    ASSERT(unknown);
    STD_CREATE_BODY(MiniportWaveCyclic, unknown, unknownOuter, PoolType);
}

//=============================================================================
MiniportWaveCyclic::~MiniportWaveCyclic()
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclic::~CMiniportWaveCyclic]"));
}


//=============================================================================
/*
Routine Description:

  The DataRangeIntersection function determines the highest quality 
  intersection of two data ranges.

Arguments:

  PinId                 - Pin for which data intersection is being determined. 

  ClientDataRange       - Pointer to KSDATARANGE structure which contains the data 
                          range submitted by client in the data range intersection 
                          property request. 

  MyDataRange           - Pin's data range to be compared with client's data 
                          range. In this case we actually ignore our own data 
                          range, because we know that we only support one range.

  OutputBufferLength    - Size of the buffer pointed to by the resultant format parameter. 

  ResultantFormat       - Pointer to value where the resultant format should be returned. 

  ResultantFormatLength - Actual length of the resultant format placed in 
                          ResultantFormat. This should be less than or equal 
                          to OutputBufferLength. 
*/
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclic::DataRangeIntersection
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
    UNREFERENCED_PARAMETER(pinId);
    UNREFERENCED_PARAMETER(clientDataRange);
    UNREFERENCED_PARAMETER(myDataRange);
    UNREFERENCED_PARAMETER(outputBufferLength);
    UNREFERENCED_PARAMETER(resultantFormat);
    UNREFERENCED_PARAMETER(resultantFormatLength);

    PAGED_CODE();

    // This driver only supports PCM formats.
    // Portcls will handle the request for us.
    //

    return STATUS_NOT_IMPLEMENTED;
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
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclic::GetDescription(_Out_ PPCFILTER_DESCRIPTOR* outFilterDescriptor)
{
    PAGED_CODE();

    ASSERT(outFilterDescriptor);

    return MiniportWaveCyclicMSVAD::GetDescription(outFilterDescriptor);
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
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclic::Init
(
    _In_  PUNKNOWN        unknownAdapter,
    _In_  PRESOURCELIST   resourceList,
    _In_  PPORTWAVECYCLIC port
)
{
    PAGED_CODE();

    ASSERT(unknownAdapter);
    ASSERT(port);

    DPF_ENTER(("[CMiniportWaveCyclic::Init]"));

    maxOutputStreams_      = MAX_OUTPUT_STREAMS;
    maxInputStreams_       = MAX_INPUT_STREAMS;
    maxTotalStreams_       = MAX_TOTAL_STREAMS;

    minChannels_           = MIN_CHANNELS;
    maxChannelsPcm_        = MAX_CHANNELS_PCM;

    minBitsPerSamplePcm_   = MIN_BITS_PER_SAMPLE_PCM;
    maxBitsPerSamplePcm_   = MAX_BITS_PER_SAMPLE_PCM;
    minSampleRatePcm_      = MIN_SAMPLE_RATE;
    maxSampleRatePcm_      = MAX_SAMPLE_RATE;

    NTSTATUS ntStatus = MiniportWaveCyclicMSVAD::Init(unknownAdapter, resourceList, port);
    if (NT_SUCCESS(ntStatus))
    {
        // Set filter descriptor.
        filterDescriptor_ = &MiniportFilterDescriptor;

        isCaptureAllocated_ = FALSE;
        isRenderAllocated_ = FALSE;
    }

    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  The NewStream function creates a new instance of a logical stream 
  associated with a specified physical channel. Callers of NewStream should 
  run at IRQL PASSIVE_LEVEL.
*/
_Use_decl_annotations_
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclic::NewStream
(
    PMINIPORTWAVECYCLICSTREAM* outStream,
    PUNKNOWN                   outerUnknown,
    POOL_TYPE                  poolType,
    ULONG                      pin,
    BOOLEAN                    capture,
    PKSDATAFORMAT              dataFormat,
    PDMACHANNEL *              outDmaChannel,
    PSERVICEGROUP *            outServiceGroup
)
{
    UNREFERENCED_PARAMETER(poolType);

    PAGED_CODE();

    ASSERT(outStream);
    ASSERT(dataFormat);
    ASSERT(outDmaChannel);
    ASSERT(outServiceGroup);

    DPF_ENTER(("[CMiniportWaveCyclic::NewStream]"));

    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PCMiniportWaveCyclicStream  stream = nullptr;

    // Check if we have enough streams.
    if (capture)
    {
        if (isCaptureAllocated_)
        {
            DPF(D_TERSE, ("[Only one capture stream supported]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
    {
        if (isRenderAllocated_)
        {
            DPF(D_TERSE, ("[Only one render stream supported]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Determine if the format is valid.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = validateFormat(dataFormat);
    }

    // Instantiate a stream. Stream must be in
    // NonPagedPool because of file saving.
    //
    if (NT_SUCCESS(ntStatus))
    {
        stream = new (NonPagedPool, MSVAD_POOLTAG) 
            MiniportWaveCyclicStream(outerUnknown);

        if (stream)
        {
            stream->AddRef();

            ntStatus = stream->Init(this, pin, capture, dataFormat);
        }
        else
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        if (capture)
        {
            isCaptureAllocated_ = TRUE;
        }
        else
        {
            isRenderAllocated_ = TRUE;
        }

         *outStream = PMINIPORTWAVECYCLICSTREAM(stream);
        (*outStream)->AddRef();
        
         *outDmaChannel = PDMACHANNEL(stream);
        (*outDmaChannel)->AddRef();

         *outServiceGroup = serviceGroup_;
        (*outServiceGroup)->AddRef();

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
/*
Routine Description:
  QueryInterface

Arguments:
  Interface - GUID
  Object - interface pointer to be returned.
*/
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclic::NonDelegatingQueryInterface
(
    _In_         REFIID Interface,
    _COM_Outptr_ PVOID* object
)
{
    PAGED_CODE();

    ASSERT(object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLIC(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclic))
    {
        *object = PVOID(PMINIPORTWAVECYCLIC(this));
    }
    else
    {
        *object = nullptr;
    }

    if (*object)
    {
        // We reference the interface for the caller.

        PUNKNOWN(*object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

//=============================================================================
/*
Routine Description:
  Handles KSPROPERTY_GENERAL_COMPONENTID
*/
NTSTATUS MiniportWaveCyclic::propertyHandlerComponentId(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    DPF_ENTER(("[PropertyHandlerComponentId]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET, VT_ILLEGAL);
    }
    else
    {
        ntStatus = ValidatePropertyParams(propertyRequest, sizeof(KSCOMPONENTID), 0);
        if (NT_SUCCESS(ntStatus))
        {
            if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                PKSCOMPONENTID pComponentId = (PKSCOMPONENTID)propertyRequest->Value;

                INIT_MMREG_MID(&pComponentId->Manufacturer, MM_MICROSOFT);
                pComponentId->Product   = PID_MSVAD;
                pComponentId->Name      = NAME_MSVAD_SIMPLE;
                pComponentId->Component = GUID_NULL; // Not used for extended caps.
                pComponentId->Version   = MSVAD_VERSION;
                pComponentId->Revision  = MSVAD_REVISION;

                propertyRequest->ValueSize = sizeof(KSCOMPONENTID);
                ntStatus = STATUS_SUCCESS;
            }
        }
        else
        {
            DPF(D_TERSE, ("[PropertyHandlerComponentId - Invalid parameter]"));
            ntStatus = STATUS_INVALID_PARAMETER;
        }
    }

    return ntStatus;
}

#define CB_EXTENSIBLE (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))

//=============================================================================
/*
Routine Description:
  Handles KSPROPERTY_GENERAL_COMPONENTID
*/
NTSTATUS MiniportWaveCyclic::propertyHandlerProposedFormat(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    DPF_ENTER(("[PropertyHandlerProposedFormat]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_SET, VT_ILLEGAL);
    }
    else
    {
        ULONG cbMinSize = sizeof(KSDATAFORMAT_WAVEFORMATEX);

        if (propertyRequest->ValueSize == 0)
        {
            propertyRequest->ValueSize = cbMinSize;
            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
        else if (propertyRequest->ValueSize < cbMinSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                KSDATAFORMAT_WAVEFORMATEX* pKsFormat = (KSDATAFORMAT_WAVEFORMATEX*)propertyRequest->Value;

                ntStatus = STATUS_NO_MATCH;

                if ((pKsFormat->DataFormat.MajorFormat == KSDATAFORMAT_TYPE_AUDIO)  &&
                    (pKsFormat->DataFormat.SubFormat   == KSDATAFORMAT_SUBTYPE_PCM) &&
                    (pKsFormat->DataFormat.Specifier   == KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
                {
                    WAVEFORMATEX* wfx = (WAVEFORMATEX*)&pKsFormat->WaveFormatEx;

                    // make sure the WAVEFORMATEX part of the format makes sense
                    if ( (wfx->wBitsPerSample  == 16)                                       &&
                        ((wfx->nSamplesPerSec  == 44100) || (wfx->nSamplesPerSec == 48000)) &&
                         (wfx->nBlockAlign     == (wfx->nChannels * 2))                     &&
                         (wfx->nAvgBytesPerSec == (wfx->nSamplesPerSec * wfx->nBlockAlign)))
                    {
                        if ((wfx->wFormatTag == WAVE_FORMAT_PCM) && (wfx->cbSize == 0))
                        {
                            if (wfx->nChannels == 2)
                            {
                                ntStatus = STATUS_SUCCESS;
                            }
                        }
                        else 
                        if ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (wfx->cbSize == CB_EXTENSIBLE))
                        {
                            WAVEFORMATEXTENSIBLE* pWfxT = (WAVEFORMATEXTENSIBLE*)wfx;

                            if (((wfx->nChannels == 2) && (pWfxT->dwChannelMask == KSAUDIO_SPEAKER_STEREO)) ||
                                ((wfx->nChannels == 6) && (pWfxT->dwChannelMask == KSAUDIO_SPEAKER_5POINT1_SURROUND)) ||
                                ((wfx->nChannels == 8) && (pWfxT->dwChannelMask == KSAUDIO_SPEAKER_7POINT1_SURROUND)))
                            {
                                ntStatus = STATUS_SUCCESS;
                            }
                        }
                    }
                }
            }
            else
            {
                ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Redirects general property request to miniport object
*/
NTSTATUS propertyHandler_WaveFilter(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    NTSTATUS          ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportWaveCyclic pWave = (PCMiniportWaveCyclic) propertyRequest->MajorTarget;

    switch (propertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_GENERAL_COMPONENTID:
            ntStatus = pWave->propertyHandlerComponentId(propertyRequest);
            break;

        case KSPROPERTY_PIN_PROPOSEDATAFORMAT:
            ntStatus = pWave->propertyHandlerProposedFormat(propertyRequest);
            break;
        
        default:
            DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
    }

    return ntStatus;
}

//=============================================================================
// CMiniportWaveStreamCyclicSimple
//=============================================================================

//=============================================================================
MiniportWaveCyclicStream::~MiniportWaveCyclicStream()
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStream::~CMiniportWaveCyclicStream]"));

    if (miniportLocal_)
    {
        if (isCapture_)
        {
            miniportLocal_->isCaptureAllocated_ = FALSE;
        }
        else // render
        {
            miniportLocal_->isRenderAllocated_ = FALSE;
        }
    }
}

//=============================================================================
/*
Routine Description:
  Initializes the stream object. Allocate a DMA buffer, timer and DPC
*/
NTSTATUS MiniportWaveCyclicStream::Init
( 
    IN PCMiniportWaveCyclic miniport,
    IN ULONG                pin,
    IN BOOLEAN              capture,
    IN PKSDATAFORMAT        dataFormat
)
{
    PAGED_CODE();

    miniportLocal_ = miniport;

    return MiniportWaveCyclicStreamMSVAD::Init(miniport, pin, capture, dataFormat);
}

//=============================================================================
/*
Routine Description:
  QueryInterface

Arguments:
  Interface - GUID
  Object    - interface pointer to be returned
*/
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclicStream::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * object
)
{
    PAGED_CODE();

    ASSERT(object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLICSTREAM(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclicStream))
    {
        *object = PVOID(PMINIPORTWAVECYCLICSTREAM(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IDmaChannel))
    {
        *object = PVOID(PDMACHANNEL(this));
    }
    else
    {
        *object = nullptr;
    }

    if (*object)
    {
        PUNKNOWN(*object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}
#pragma code_seg()