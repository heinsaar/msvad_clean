/*
Abstract:
    Declaration of topology miniport.
*/

#ifndef _MSVAD_BASETOPO_H_
#define _MSVAD_BASETOPO_H_

class MiniportTopologyMSVAD
{
  public:
    MiniportTopologyMSVAD();
    ~MiniportTopologyMSVAD();

    NTSTATUS GetDescription(_Out_ PPCFILTER_DESCRIPTOR*  Description);
    NTSTATUS DataRangeIntersection
    (   
        _In_        ULONG           PinId,
        _In_        PKSDATARANGE    ClientDataRange,
        _In_        PKSDATARANGE    MyDataRange,
        _In_        ULONG           OutputBufferLength,
        _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                    PVOID           ResultantFormat,
        _Out_       PULONG          ResultantFormatLength
    );

    NTSTATUS Init(IN  PUNKNOWN UnknownAdapter, IN  PPORTTOPOLOGY Port_);

    // PropertyHandlers.
    NTSTATUS PropertyHandlerBasicSupportVolume(IN  PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerCpuResources(      IN  PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerGeneric(           IN  PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerMute(              IN  PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerMuxSource(         IN  PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerVolume(            IN  PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerDevSpecific(       IN  PPCPROPERTY_REQUEST PropertyRequest);

protected:
    PADAPTERCOMMON       adapterCommon_;    // Adapter common object.
    PPCFILTER_DESCRIPTOR filterDescriptor_; // Filter descriptor.
};

#endif