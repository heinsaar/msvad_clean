/*
Abstract:
    Implementation of wavecyclic miniport.
*/

#pragma warning (disable : 4127)

#include <msvad.h>
#include "common.h"
#include "basewave.h"

//=============================================================================
#pragma code_seg("PAGE")
MiniportWaveCyclicMSVAD::MiniportWaveCyclicMSVAD()
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicMSVAD::CMiniportWaveCyclicMSVAD]"));

    // Initialize members.
    //
    adapterCommon_ = nullptr;
    port_ = nullptr;
    filterDescriptor_ = nullptr;

    notificationInterval_ = 0;
    samplingFrequency_ = 0;

    serviceGroup_ = nullptr;
    maxDmaBufferSize_ = DMA_BUFFER_SIZE;

    maxOutputStreams_ = 0;
    maxInputStreams_ = 0;
    maxTotalStreams_ = 0;

    minChannels_ = 0;
    maxChannelsPcm_ = 0;
    minBitsPerSamplePcm_ = 0;
    maxBitsPerSamplePcm_ = 0;
    minSampleRatePcm_ = 0;
    maxSampleRatePcm_ = 0;
}

//=============================================================================
MiniportWaveCyclicMSVAD::~MiniportWaveCyclicMSVAD()
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicMSVAD::~CMiniportWaveCyclicMSVAD]"));

    if (port_)
    {
        port_->Release();
    }

    if (serviceGroup_)
    {
        serviceGroup_->Release();
    }

    if (adapterCommon_)
    {
        adapterCommon_->Release();
    }
}

//=============================================================================
/*
Routine Description:
    The GetDescription function gets a pointer to a filter description.
    The descriptor is defined in wavtable.h for each MSVAD sample.

Arguments:
  OutFilterDescriptor - Pointer to the filter description
*/
STDMETHODIMP
MiniportWaveCyclicMSVAD::GetDescription(_Out_ PPCFILTER_DESCRIPTOR* outFilterDescriptor)
{
    PAGED_CODE();
    ASSERT(outFilterDescriptor);
    DPF_ENTER(("[CMiniportWaveCyclicMSVAD::GetDescription]"));

    *outFilterDescriptor = filterDescriptor_;

    return STATUS_SUCCESS;
}

//=============================================================================
/*

Routine Description:

Arguments:
  UnknownAdapter_ - pointer to adapter common.
  ResourceList_   - resource list. MSVAD does not use resources.
  Port_           - pointer to the port

*/
STDMETHODIMP
MiniportWaveCyclicMSVAD::Init
(
    _In_ PUNKNOWN        unknownAdapter,
    _In_ PRESOURCELIST   resourceList,
    _In_ PPORTWAVECYCLIC port
)
{
    UNREFERENCED_PARAMETER(resourceList);

    PAGED_CODE();
    ASSERT(unknownAdapter);
    ASSERT(port);

    DPF_ENTER(("[CMiniportWaveCyclicMSVAD::Init]"));

    // AddRef() is required because we are keeping this pointer.
    //
    port_ = port;
    port_->AddRef();

    // We want the IAdapterCommon interface on the adapter common object,
    // which is given to us as a IUnknown.  The QueryInterface call gives us
    // an AddRefed pointer to the interface we want.
    //
    NTSTATUS ntStatus = unknownAdapter->QueryInterface(IID_IAdapterCommon, (PVOID *)&adapterCommon_);

    if (NT_SUCCESS(ntStatus))
    {
        KeInitializeMutex(&sampleRateSync_, 1);
        ntStatus = PcNewServiceGroup(&serviceGroup_, nullptr);

        if (NT_SUCCESS(ntStatus))
        {
            adapterCommon_->setWaveServiceGroup(serviceGroup_);
        }
    }

    if (!NT_SUCCESS(ntStatus))
    {
        // clean up AdapterCommon
        //
        if (adapterCommon_)
        {
            // clean up the service group
            //
            if (serviceGroup_)
            {
                adapterCommon_->setWaveServiceGroup(nullptr);
                serviceGroup_->Release();
                serviceGroup_ = nullptr;
            }

            adapterCommon_->Release();
            adapterCommon_ = nullptr;
        }

        // release the port
        //
        port_->Release();
        port_ = nullptr;
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Processes KSPROPERTY_AUDIO_CPURESOURCES

Arguments:
  PropertyRequest - property request structure
*/
NTSTATUS MiniportWaveCyclicMSVAD::propertyHandlerCpuResources(IN PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    ASSERT(propertyRequest);
    DPF_ENTER(("[CMiniportWaveCyclicMSVAD::PropertyHandlerCpuResources]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        ntStatus = ValidatePropertyParams(propertyRequest, sizeof(LONG), 0);
        if (NT_SUCCESS(ntStatus))
        {
            *(PLONG(propertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
            propertyRequest->ValueSize = sizeof(LONG);
            ntStatus = STATUS_SUCCESS;
        }
    }
    else if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT, VT_I4);
    }

    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  Handles all properties for this miniport.

Arguments:

  PropertyRequest - property request structure

*/
NTSTATUS MiniportWaveCyclicMSVAD::propertyHandlerGeneric(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    ASSERT(propertyRequest);
    ASSERT(propertyRequest->PropertyItem);

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (propertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = propertyHandlerCpuResources(propertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandlerGeneric: Invalid Device Request]"));
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }

    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  Validates that the given dataformat is valid.
  This version of the driver only supports PCM.

Arguments:

  pDataFormat - The dataformat for validation.

*/
NTSTATUS MiniportWaveCyclicMSVAD::validateFormat(IN  PKSDATAFORMAT dataFormat)
{
    PAGED_CODE();

    ASSERT(dataFormat);

    DPF_ENTER(("[CMiniportWaveCyclicMSVAD::ValidateFormat]"));

    NTSTATUS      ntStatus = STATUS_INVALID_PARAMETER;
    PWAVEFORMATEX pwfx = GetWaveFormatEx(dataFormat);
    if (pwfx)
    {
        if (IS_VALID_WAVEFORMATEX_GUID(&dataFormat->SubFormat))
        {
            const USHORT wfxID = EXTRACT_WAVEFORMATEX_ID(&dataFormat->SubFormat);

            switch (wfxID)
            {
                case WAVE_FORMAT_PCM:
                {
                    switch (pwfx->wFormatTag)
                    {
                        case WAVE_FORMAT_PCM:
                        {
                            ntStatus = validatePcm(pwfx);
                            break;
                        }
                    }
                    break;
                }

                default:
                    DPF(D_TERSE, ("Invalid format EXTRACT_WAVEFORMATEX_ID!"));
                    break;
            }
        }
        else
        {
            DPF(D_TERSE, ("Invalid pDataFormat->SubFormat!") );
        }
    }

    return ntStatus;
}

//-----------------------------------------------------------------------------
/*
Routine Description:

  Given a waveformatex and format size validates that the format is in device datarange.

Arguments:

  pWfx - wave format structure.
*/
NTSTATUS MiniportWaveCyclicMSVAD::validatePcm(IN  PWAVEFORMATEX wfx)
{
    PAGED_CODE();

    DPF_ENTER(("CMiniportWaveCyclicMSVAD::ValidatePcm"));

    if ( wfx                                          &&
        (wfx->cbSize == 0)                            &&
        (wfx->nChannels >= minChannels_)              &&
        (wfx->nChannels <= maxChannelsPcm_)           &&
        (wfx->nSamplesPerSec >= minSampleRatePcm_)    &&
        (wfx->nSamplesPerSec <= maxSampleRatePcm_)    &&
        (wfx->wBitsPerSample >= minBitsPerSamplePcm_) &&
        (wfx->wBitsPerSample <= maxBitsPerSamplePcm_))
    {
        return STATUS_SUCCESS;
    }

    DPF(D_TERSE, ("Invalid PCM format"));

    return STATUS_INVALID_PARAMETER;
}

//=============================================================================
// CMiniportWaveCyclicStreamMSVAD
//=============================================================================

MiniportWaveCyclicStreamMSVAD::MiniportWaveCyclicStreamMSVAD()
{
    PAGED_CODE();

    miniport_ = nullptr;
    isCapture_ = FALSE;
    is16BitSample = FALSE;
    blockAlign_ = 0;
    ksState_ = KSSTATE_STOP;
    pinId_ = (ULONG)-1;

    dpc_   = nullptr;
    timer_ = nullptr;

    dmaActive_ = FALSE;
    dmaPosition_ = 0;
    elapsedTimeCarryForward_ = 0;
    byteDisplacementCarryForward_ = 0;
    dmaBuffer_ = nullptr;
    dmaBufferSize_ = 0;
    dmaMovementRate_ = 0;
    dmaTimeStamp_ = 0;
}

//=============================================================================
MiniportWaveCyclicStreamMSVAD::~MiniportWaveCyclicStreamMSVAD()
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveCyclicStreamMS::~CMiniportWaveCyclicStreamMS]"));

    if (timer_)
    {
        KeCancelTimer(timer_);
        ExFreePoolWithTag(timer_, MSVAD_POOLTAG);
    }

    // Since we just canceled the timer, wait for all queued DPCs to complete before we free the DPC.
    KeFlushQueuedDpcs();

    if (dpc_)
    {
        ExFreePoolWithTag( dpc_, MSVAD_POOLTAG );
    }
    
    FreeBuffer(); // free the DMA buffer
}

//=============================================================================
/*

Routine Description:

  Initializes the stream object. Allocate a DMA buffer, timer and DPC

Arguments:

  Miniport_ - miniport object
  Pin_ - pin id
  Capture_ - TRUE if this is a capture stream
  DataFormat_ - new dataformat

*/
#pragma warning (push)
#pragma warning (disable : 26165)
NTSTATUS MiniportWaveCyclicStreamMSVAD::Init
(
    IN  PCMiniportWaveCyclicMSVAD miniport,
    IN  ULONG                     pin,
    IN  BOOLEAN                   capture,
    IN  PKSDATAFORMAT             dataFormat
)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::Init]"));

    ASSERT(miniport);
    ASSERT(dataFormat);

    NTSTATUS      ntStatus = STATUS_SUCCESS;
    PWAVEFORMATEX wfx = GetWaveFormatEx(dataFormat);
    if (!wfx)
    {
        DPF(D_TERSE, ("Invalid DataFormat param in NewStream"));
        ntStatus = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(ntStatus))
    {
        miniport_ = miniport;

        pinId_                        = pin;
        isCapture_                    = capture;
        blockAlign_                   = wfx->nBlockAlign;
        is16BitSample                 = (wfx->wBitsPerSample == 16);
        ksState_                      = KSSTATE_STOP;
        dmaPosition_                  = 0;
        elapsedTimeCarryForward_      = 0;
        byteDisplacementCarryForward_ = 0;
        dmaActive_                    = FALSE;
        dpc_                          = nullptr;
        timer_                        = nullptr;
        dmaBuffer_                    = nullptr;

        // If this is not the capture stream, create the output file.
        //
        if (!isCapture_)
        {
            DPF(D_TERSE, ("SaveData %p", &saveData_));
            ntStatus = saveData_.setDataFormat(dataFormat);
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = saveData_.initialize();
            }
        }
    }

    // Allocate DMA buffer for this stream.
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = AllocateBuffer(miniport_->maxDmaBufferSize_, nullptr);
    }

    // Set sample frequency. Note that m_SampleRateSync access should be synchronized.
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = KeWaitForSingleObject(&miniport_->sampleRateSync_, Executive, KernelMode, FALSE, nullptr);
        if (STATUS_SUCCESS == ntStatus)
        {
            miniport_->samplingFrequency_ = wfx->nSamplesPerSec;
            KeReleaseMutex(&miniport_->sampleRateSync_, FALSE);
        }
        else
        {
            DPF(D_TERSE, ("[SamplingFrequency Sync failed: %08X]", ntStatus));
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = SetFormat(dataFormat);
    }

    if (NT_SUCCESS(ntStatus))
    {
        dpc_ = (PRKDPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KDPC), MSVAD_POOLTAG);
        if (!dpc_)
        {
            DPF(D_TERSE, ("[Could not allocate memory for DPC]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        timer_ = (PKTIMER)ExAllocatePoolWithTag(NonPagedPool, sizeof(KTIMER), MSVAD_POOLTAG);
        if (!timer_)
        {
            DPF(D_TERSE, ("[Could not allocate memory for Timer]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        KeInitializeDpc(dpc_, timerNotify, miniport_);
        KeInitializeTimerEx(timer_, NotificationTimer);
    }

    return ntStatus;
}
#pragma warning (pop)

#pragma code_seg()

//=============================================================================
// CMiniportWaveCyclicStreamMSVAD IMiniportWaveCyclicStream
//=============================================================================

//=============================================================================
/*
Routine Description:
  The GetPosition function gets the current position of the DMA read or write
  pointer for the stream. Callers of GetPosition should run at
  IRQL <= DISPATCH_LEVEL.

Arguments:
  Position - Position of the DMA pointer
*/

STDMETHODIMP
MiniportWaveCyclicStreamMSVAD::GetPosition(_Out_ PULONG position)
{
    if (dmaActive_)
    {
        // Get the current time
        //
        ULONGLONG CurrentTime = KeQueryInterruptTime();

        // Calculate the time elapsed since the last call to GetPosition() or since the
        // DMA engine started.  Note that the division by 10000 to convert to milliseconds
        // may cause us to lose some of the time, so we will carry the remainder forward 
        // to the next GetPosition() call.
        //
        ULONG TimeElapsedInMS = ((ULONG)(CurrentTime - dmaTimeStamp_ + elapsedTimeCarryForward_)) / 10000;

        // Carry forward the remainder of this division so we don't fall behind with our position.
        //
        elapsedTimeCarryForward_ = (CurrentTime - dmaTimeStamp_ + elapsedTimeCarryForward_) % 10000;

        // Calculate how many bytes in the DMA buffer would have been processed in the elapsed
        // time.  Note that the division by 1000 to convert to milliseconds may cause us to 
        // lose some bytes, so we will carry the remainder forward to the next GetPosition() call.
        //
        ULONG ByteDisplacement = ((dmaMovementRate_ * TimeElapsedInMS) + byteDisplacementCarryForward_) / 1000;

        // Carry forward the remainder of this division so we don't fall behind with our position.
        //
        byteDisplacementCarryForward_ = ((dmaMovementRate_ * TimeElapsedInMS) + byteDisplacementCarryForward_) % 1000;

        // Increment the DMA position by the number of bytes displaced since the last
        // call to GetPosition() and ensure we properly wrap at buffer length.
        //
        dmaPosition_ = (dmaPosition_ + ByteDisplacement) % dmaBufferSize_;

        // Return the new DMA position
        //
        *position = dmaPosition_;

        // Update the DMA time stamp for the next call to GetPosition()
        //
        dmaTimeStamp_ = CurrentTime;
    }
    else
    {
        // DMA is inactive so just return the current DMA position.
        //
        *position = dmaPosition_;
    }

    return STATUS_SUCCESS;
}

//=============================================================================
/*
Routine Description:

  Given a physical position based on the actual number of bytes transferred,
  NormalizePhysicalPosition converts the position to a time-based value of
  100 nanosecond units. Callers of NormalizePhysicalPosition can run at any IRQL.

Arguments:

  PhysicalPosition - On entry this variable contains the value to convert.
                     On return it contains the converted value
*/
STDMETHODIMP
MiniportWaveCyclicStreamMSVAD::NormalizePhysicalPosition(_Inout_ PLONGLONG physicalPosition)
{
    ASSERT(physicalPosition);

    *physicalPosition = (_100NS_UNITS_PER_SECOND / blockAlign_ * *physicalPosition) / miniport_->samplingFrequency_;

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
//=============================================================================
/*

Routine Description:

  The SetFormat function changes the format associated with a stream.
  Callers of SetFormat should run at IRQL PASSIVE_LEVEL

Arguments:

  Format - Pointer to a KSDATAFORMAT structure which indicates the new format
           of the stream.
*/
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclicStreamMSVAD::SetFormat(_In_  PKSDATAFORMAT format)
{
    PAGED_CODE();
    ASSERT(format);
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::SetFormat]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (ksState_ != KSSTATE_RUN)
    {
        // MSVAD does not validate the format.
        //
        const PWAVEFORMATEX wfx = GetWaveFormatEx(format);
        if (wfx)
        {
            ntStatus = KeWaitForSingleObject(&miniport_->sampleRateSync_, Executive, KernelMode, FALSE, nullptr);
            if (STATUS_SUCCESS == ntStatus)
            {
                if (!isCapture_)
                {
                    ntStatus = saveData_.setDataFormat(format);
                }

                blockAlign_                   =  wfx->nBlockAlign;
                is16BitSample                 = (wfx->wBitsPerSample == 16);
                miniport_->samplingFrequency_ =  wfx->nSamplesPerSec;
                dmaMovementRate_              =  wfx->nAvgBytesPerSec;

                DPF(D_TERSE, ("New Format: %d", wfx->nSamplesPerSec));
            }

            KeReleaseMutex(&miniport_->sampleRateSync_, FALSE);
        }
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:

  The SetNotificationFrequency function sets the frequency at which
  notification interrupts are generated. Callers of SetNotificationFrequency
  should run at IRQL PASSIVE_LEVEL.

Arguments:

  Interval - Value indicating the interval between interrupts,
             expressed in milliseconds

  FramingSize - Pointer to a ULONG value where the number of bytes equivalent
                to Interval milliseconds is returned
*/
STDMETHODIMP_(ULONG)
MiniportWaveCyclicStreamMSVAD::SetNotificationFreq(_In_  ULONG  interval, _Out_ PULONG framingSize)
{
    PAGED_CODE();
    ASSERT(framingSize);
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::SetNotificationFreq]"));

    miniport_->notificationInterval_ = interval;
    *framingSize = blockAlign_ * miniport_->samplingFrequency_ * interval / 1000;

    return miniport_->notificationInterval_;
}

//=============================================================================
/*
Routine Description:

  The SetState function sets the new state of playback or recording for the
  stream. SetState should run at IRQL PASSIVE_LEVEL

Arguments:

  NewState - KSSTATE indicating the new state for the stream.
*/
STDMETHODIMP
MiniportWaveCyclicStreamMSVAD::SetState(_In_  KSSTATE newState)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::SetState]"));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    // The acquire state is not distinguishable from the stop state for our
    // purposes.
    //
    if (newState == KSSTATE_ACQUIRE)
    {
        newState = KSSTATE_STOP;
    }

    if (ksState_ != newState)
    {
        switch (newState)
        {
            case KSSTATE_PAUSE:
            {
                DPF(D_TERSE, ("KSSTATE_PAUSE"));
                dmaActive_ = FALSE;
            }
            break;

            case KSSTATE_RUN:
            {
                DPF(D_TERSE, ("KSSTATE_RUN"));

                 LARGE_INTEGER delay;

                // Set the timer for DPC.
                //
                dmaTimeStamp_             = KeQueryInterruptTime();
                elapsedTimeCarryForward_  = 0;
                dmaActive_                = TRUE;
                delay.HighPart            = 0;
                delay.LowPart             = miniport_->notificationInterval_;

                KeSetTimerEx(timer_, delay, miniport_->notificationInterval_, dpc_);
            }
            break;

        case KSSTATE_STOP:

            DPF(D_TERSE, ("KSSTATE_STOP"));

            dmaActive_                    = FALSE;
            dmaPosition_                  = 0;
            elapsedTimeCarryForward_      = 0;
            byteDisplacementCarryForward_ = 0;

            KeCancelTimer( timer_ );

            // Wait until all work items are completed.
            //
            if (!isCapture_)
            {
                saveData_.waitAllWorkItems();
            }

            break;
        }

        ksState_ = newState;
    }

    return ntStatus;
}

#pragma code_seg()

//=============================================================================
/*
Routine Description:

  The Silence function is used to copy silence samplings to a certain location.
  Callers of Silence can run at any IRQL

Arguments:

  Buffer - Pointer to the buffer where the silence samplings should
           be deposited.

  ByteCount - Size of buffer indicating number of bytes to be deposited.
*/
_Use_decl_annotations_
STDMETHODIMP_(void)
MiniportWaveCyclicStreamMSVAD::Silence(PVOID buffer, ULONG byteCount)
{
    RtlFillMemory(buffer, byteCount, is16BitSample ? 0 : 0x80);
}

//=============================================================================
/*

Routine Description:

  Dpc routine. This simulates an interrupt service routine. The Dpc will be
  called whenever CMiniportWaveCyclicStreamMSVAD::m_pTimer triggers.

Arguments:

  Dpc - the Dpc object

  DeferredContext - Pointer to a caller-supplied context to be passed to
                    the DeferredRoutine when it is called

  SA1 - System argument 1
  SA2 - System argument 2
*/
void timerNotify
(
    IN  PKDPC dpc,
    IN  PVOID deferredContext,
    IN  PVOID SA1,
    IN  PVOID SA2
)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(SA1);
    UNREFERENCED_PARAMETER(SA2);

    PCMiniportWaveCyclicMSVAD miniport = (PCMiniportWaveCyclicMSVAD) deferredContext;

    if (miniport && miniport->port_)
    {
        miniport->port_->Notify(miniport->serviceGroup_);
    }
}