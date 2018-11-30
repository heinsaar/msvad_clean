// Declaration of topology miniport.

#ifndef _MSVAD_MINTOPO_H_
#define _MSVAD_MINTOPO_H_

#include "basetopo.h"

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportTopology 
//   

class CMiniportTopology : public CMiniportTopologyMSVAD,
                          public IMiniportTopology,
                          public CUnknown
{
  public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportTopology);
    ~CMiniportTopology();

    IMP_IMiniportTopology;
};
typedef CMiniportTopology *PCMiniportTopology;

#endif

