/*
Abstract:
    Implementation of MSVAD HW class. 
    MSVAD HW has an array for storing mixer and volume settings for the topology.
*/
#include <msvad.h>
#include "hw.h"

//=============================================================================
// CMSVADHW
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
CMSVADHW::CMSVADHW()
  : mux_(0),
    bDevSpecific_(FALSE),
    iDevSpecific_(0),
    uiDevSpecific_(0)
{
    PAGED_CODE();    
    MixerReset();
}
#pragma code_seg()

//=============================================================================
/*
Routine Description:
  Gets the HW (!) Device Specific info
*/
BOOL CMSVADHW::bGetDevSpecific()
{
    return bDevSpecific_;
}

//=============================================================================
/*
Routine Description:
  Sets the HW (!) Device Specific info

Arguments:

  devSpecific - true or false for this example.
*/
void CMSVADHW::bSetDevSpecific(IN  BOOL devSpecific)
{
    bDevSpecific_ = devSpecific;
}

//=============================================================================
/*
Routine Description:
  Gets the HW (!) Device Specific info
*/
INT CMSVADHW::iGetDevSpecific()
{
    return iDevSpecific_;
}

//=============================================================================
/*
Routine Description:
  Sets the HW (!) Device Specific info

Arguments:
  devSpecific - true or false for this example.
*/
void CMSVADHW::iSetDevSpecific(IN  INT devSpecific)
{
    iDevSpecific_ = devSpecific;
}

//=============================================================================
/*
Routine Description:
  Gets the HW (!) Device Specific info
*/
UINT CMSVADHW::uiGetDevSpecific()
{
    return uiDevSpecific_;
}

//=============================================================================
/*

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  uiDevSpecific - int for this example.
*/
void CMSVADHW::uiSetDevSpecific(IN  UINT devSpecific)
{
    uiDevSpecific_ = devSpecific;
}

//=============================================================================
/*
Routine Description:
  Gets the HW (!) mute levels for MSVAD

Arguments:
  ulNode - topology node id
  mute setting

*/
BOOL CMSVADHW::GetMixerMute(IN  ULONG node)
{
    if (node < MAX_TOPOLOGY_NODES)
    {
        return muteControls_[node];
    }

    return 0;
}

//=============================================================================
/*
Routine Description:
  Return the current mux selection
*/
ULONG CMSVADHW::GetMixerMux()
{
    return mux_;
}

//=============================================================================
/*
Routine Description:
  Gets the HW (!) volume for MSVAD.

Arguments:
  ulNode   - topology node id
  lChannel - which channel are we setting?
  LONG     - volume level
*/
LONG CMSVADHW::GetMixerVolume(IN  ULONG node, IN  LONG  channel)
{
    UNREFERENCED_PARAMETER(channel);

    if (node < MAX_TOPOLOGY_NODES)
    {
        return volumeControls_[node];
    }

    return 0;
}

//=============================================================================
/*
  Resets the mixer registers.
*/
#pragma code_seg("PAGE")
void CMSVADHW::MixerReset()
{
    PAGED_CODE();
    
    RtlFillMemory(volumeControls_, sizeof(LONG) * MAX_TOPOLOGY_NODES, 0xFF);
    RtlFillMemory(muteControls_,   sizeof(BOOL) * MAX_TOPOLOGY_NODES, TRUE);
    
    // BUGBUG change this depending on the topology
    mux_ = 2;
}
#pragma code_seg()

//=============================================================================
/*
Routine Description:
  Sets the HW (!) mute levels for MSVAD

Arguments:
  node - topology node id
  fMute - mute flag
*/
void CMSVADHW::SetMixerMute(IN  ULONG node, IN  BOOL fMute)
{
    if (node < MAX_TOPOLOGY_NODES)
    {
        muteControls_[node] = fMute;
    }
}

//=============================================================================
/*
Routine Description:
  Sets the HW (!) mux selection

Arguments:
  ulNode - topology node id
*/
void CMSVADHW::SetMixerMux(IN  ULONG node)
{
    mux_ = node;
}

//=============================================================================
/*
Routine Description:
  Sets the HW (!) volume for MSVAD.

Arguments:
  node    - topology node id
  channel - which channel are we setting?
  volume  - volume level

*/
void CMSVADHW::SetMixerVolume(IN  ULONG node, IN  LONG channel, IN  LONG volume)
{
    UNREFERENCED_PARAMETER(channel);

    if (node < MAX_TOPOLOGY_NODES)
    {
        volumeControls_[node] = volume;
    }
}