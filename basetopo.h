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
    NTSTATUS propertyHandlerBasicSupportVolume(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerCpuResources(      IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerGeneric(           IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerMute(              IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerMuxSource(         IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerVolume(            IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerDevSpecific(       IN PPCPROPERTY_REQUEST PropertyRequest);

protected:
    PADAPTERCOMMON       adapterCommon_;    // Adapter common object.
    PPCFILTER_DESCRIPTOR filterDescriptor_; // Filter descriptor.
};

#endif