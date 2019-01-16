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
class CMiniportWaveCyclicStream;
typedef CMiniportWaveCyclicStream* PCMiniportWaveCyclicStream;

class CMiniportWaveCyclic : public CMiniportWaveCyclicMSVAD,
                            public IMiniportWaveCyclic,
                            public CUnknown {
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclic);
    ~CMiniportWaveCyclic();

    IMP_IMiniportWaveCyclic;

    NTSTATUS PropertyHandlerComponentId(IN PPCPROPERTY_REQUEST  PropertyRequest);
    NTSTATUS PropertyHandlerProposedFormat(IN PPCPROPERTY_REQUEST  PropertyRequest);

    // Friends
    friend class CMiniportWaveCyclicStream;
    friend class CMiniportTopologySimple;

private:
    BOOL m_fCaptureAllocated;
    BOOL m_fRenderAllocated;
};
typedef CMiniportWaveCyclic* PCMiniportWaveCyclic;

///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveCyclicStream 
//   

class CMiniportWaveCyclicStream : public CMiniportWaveCyclicStreamMSVAD,
                                  public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclicStream);
    ~CMiniportWaveCyclicStream();

    NTSTATUS Init
    ( 
        IN  PCMiniportWaveCyclic Miniport,
        IN  ULONG               Channel,
        IN  BOOLEAN             Capture,
        IN  PKSDATAFORMAT       DataFormat
    );

    // Friends
    friend class CMiniportWaveCyclic;

protected:
    PCMiniportWaveCyclic m_pMiniportLocal;  
};
typedef CMiniportWaveCyclicStream *PCMiniportWaveCyclicStream;

#endif

