/*
Abstract:
    Definition of wavecyclic miniport class.
*/

#ifndef _MSVAD_MINWAVE_H_
#define _MSVAD_MINWAVE_H_

#include "basewave.h"

//=============================================================================
// Referenced Forward
//=============================================================================
class   MiniportWaveCyclicStream;
using PCMiniportWaveCyclicStream = MiniportWaveCyclicStream*;

class MiniportWaveCyclic : public MiniportWaveCyclicMSVAD,
                           public IMiniportWaveCyclic,
                           public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(MiniportWaveCyclic);
    ~MiniportWaveCyclic();

    IMP_IMiniportWaveCyclic;

    NTSTATUS propertyHandlerComponentId(   IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS propertyHandlerProposedFormat(IN PPCPROPERTY_REQUEST PropertyRequest);

    // Friends
    friend class MiniportWaveCyclicStream;
    friend class CMiniportTopologySimple;

private:
    BOOL isCaptureAllocated_;
    BOOL isRenderAllocated_;
};
using PCMiniportWaveCyclic = MiniportWaveCyclic*;

///////////////////////////////////////////////////////////////////////////////

class MiniportWaveCyclicStream : public MiniportWaveCyclicStreamMSVAD,
                                 public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(MiniportWaveCyclicStream);
    ~MiniportWaveCyclicStream();

    NTSTATUS Init
    ( 
        IN  PCMiniportWaveCyclic Miniport,
        IN  ULONG                Channel,
        IN  BOOLEAN              Capture,
        IN  PKSDATAFORMAT        DataFormat
    );

    friend class MiniportWaveCyclic;

protected:
    PCMiniportWaveCyclic miniportLocal_;  
};
using PCMiniportWaveCyclicStream = MiniportWaveCyclicStream*;

#endif