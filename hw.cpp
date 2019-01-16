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
  : m_ulMux(0),
    m_bDevSpecific(FALSE),
    m_iDevSpecific(0),
    m_uiDevSpecific(0)
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
    return m_bDevSpecific;
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
    m_bDevSpecific = devSpecific;
}

//=============================================================================
/*
Routine Description:
  Gets the HW (!) Device Specific info
*/
INT CMSVADHW::iGetDevSpecific()
{
    return m_iDevSpecific;
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
    m_iDevSpecific = devSpecific;
}

//=============================================================================
/*
Routine Description:
  Gets the HW (!) Device Specific info
*/
UINT CMSVADHW::uiGetDevSpecific()
{
    return m_uiDevSpecific;
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
    m_uiDevSpecific = devSpecific;
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
        return m_MuteControls[node];
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
    return m_ulMux;
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
        return m_VolumeControls[node];
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
    
    RtlFillMemory(m_VolumeControls, sizeof(LONG) * MAX_TOPOLOGY_NODES, 0xFF);
    RtlFillMemory(m_MuteControls,   sizeof(BOOL) * MAX_TOPOLOGY_NODES, TRUE);
    
    // BUGBUG change this depending on the topology
    m_ulMux = 2;
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
        m_MuteControls[node] = fMute;
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
    m_ulMux = node;
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
        m_VolumeControls[node] = volume;
    }
}