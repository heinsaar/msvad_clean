/*
    Declaration of topology miniport.
*/

#ifndef _MSVAD_MINTOPO_H_
#define _MSVAD_MINTOPO_H_

#include "basetopo.h"

class MiniportTopology : public MiniportTopologyMSVAD,
                         public IMiniportTopology,
                         public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(MiniportTopology);
    ~MiniportTopology();

    IMP_IMiniportTopology;

    NTSTATUS PropertyHandlerJackDescription(IN PPCPROPERTY_REQUEST  PropertyRequest);
};
typedef MiniportTopology *PCMiniportTopology;

extern NTSTATUS PropertyHandler_TopoFilter(IN PPCPROPERTY_REQUEST PropertyRequest);

#endif