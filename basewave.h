/*
Abstract:
    Definition of base wavecyclic and wavecyclic stream class.
*/

#ifndef _MSVAD_BASEWAVE_H_
#define _MSVAD_BASEWAVE_H_

#include "savedata.h"

//=============================================================================
// Referenced Forward
//=============================================================================
KDEFERRED_ROUTINE timerNotify;

class   MiniportWaveCyclicStreamMSVAD;
typedef MiniportWaveCyclicStreamMSVAD* PCMiniportWaveCyclicStreamMSVAD;

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveCyclicMSVAD
//   This is the common base class for all MSVAD samples. It implements basic
//   functionality.

class MiniportWaveCyclicMSVAD
{
protected:
    NTSTATUS validateFormat(IN PKSDATAFORMAT pDataFormat);
    NTSTATUS validatePcm(   IN PWAVEFORMATEX pWfx);

public:
     MiniportWaveCyclicMSVAD();
    ~MiniportWaveCyclicMSVAD();

    STDMETHODIMP GetDescription(_Out_ PPCFILTER_DESCRIPTOR* Description);

    STDMETHODIMP                Init
    (
        _In_ PUNKNOWN        UnknownAdapter,
        _In_ PRESOURCELIST   ResourceList,
        _In_ PPORTWAVECYCLIC Port
    );

    NTSTATUS propertyHandlerCpuResources(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerGeneric(     IN PPCPROPERTY_REQUEST PropertyRequest);

    // Friends
    friend class MiniportWaveCyclicStreamMSVAD;
    friend class MiniportTopologyMSVAD;
    friend void  timerNotify(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SA1, IN PVOID SA2);

protected:
    PADAPTERCOMMON       adapterCommon_;        // Adapter common object
    PPORTWAVECYCLIC      port_;                 // Callback interface
    PPCFILTER_DESCRIPTOR filterDescriptor_;     // Filter descriptor

    ULONG                notificationInterval_; // milliseconds.
    ULONG                samplingFrequency_;    // Frames per second.

    PSERVICEGROUP        serviceGroup_;         // For notification.
    KMUTEX               sampleRateSync_;       // Sync for sample rate 
                                                 
    ULONG                maxDmaBufferSize_;     // Dma buffer size.

    // All the below members should be updated by the child classes
    //
    ULONG maxOutputStreams_; // Max stream caps
    ULONG maxInputStreams_;
    ULONG maxTotalStreams_;
          
    ULONG minChannels_;      // Format caps
    ULONG maxChannelsPcm_;
    ULONG minBitsPerSamplePcm_;
    ULONG maxBitsPerSamplePcm_;
    ULONG minSampleRatePcm_;
    ULONG maxSampleRatePcm_;
};
typedef MiniportWaveCyclicMSVAD *PCMiniportWaveCyclicMSVAD;

///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveCyclicStreamMSVAD
//   This is the common base class for all MSVAD samples. It implements basic
//   functionality for wavecyclic streams.

class MiniportWaveCyclicStreamMSVAD : public IMiniportWaveCyclicStream,
                                      public IDmaChannel
{
protected:
    PCMiniportWaveCyclicMSVAD miniport_;                     // Miniport that created us
    BOOLEAN                   isCapture_;                    // Capture or render.
    BOOLEAN                   is16BitSample;                 // 16- or 8-bit samples.
    USHORT                    blockAlign_;                   // Block alignment of current format.
    KSSTATE                   ksState_;                      // Stop, pause, run.
    ULONG                     pinId_;                        // Pin Id.

    PRKDPC                    dpc_;                          // Deferred procedure call object
    PKTIMER                   timer_;                        // Timer object
                                                             
    BOOLEAN                   dmaActive_;                    // Dma currently active? 
    ULONG                     dmaPosition_;                  // Position in Dma
    PVOID                     dmaBuffer_;                    // Dma buffer pointer
    ULONG                     dmaBufferSize_;                // Size of dma buffer
    ULONG                     dmaMovementRate_;              // Rate of transfer specific to system
    ULONGLONG                 dmaTimeStamp_;                 // Dma time elapsed 
    ULONGLONG                 elapsedTimeCarryForward_;      // Time to carry forward in position calc.
    ULONG                     byteDisplacementCarryForward_; // Bytes to carry forward to next calc.

    CSaveData                 saveData_;                     // Object to save settings.
  
public:
     MiniportWaveCyclicStreamMSVAD();
    ~MiniportWaveCyclicStreamMSVAD();

    IMP_IMiniportWaveCyclicStream;
    IMP_IDmaChannel;

    NTSTATUS Init
    ( 
        IN  PCMiniportWaveCyclicMSVAD Miniport,
        IN  ULONG                     Pin,
        IN  BOOLEAN                   Capture,
        IN  PKSDATAFORMAT             DataFormat
    );

    // Friends
    friend class MiniportWaveCyclicMSVAD;
};
typedef MiniportWaveCyclicStreamMSVAD *PCMiniportWaveCyclicStreamMSVAD;

#endif