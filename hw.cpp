/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    hw.cpp

Abstract:

    Implementation of MSVAD HW class. 
    MSVAD HW has an array for storing mixer and volume settings
    for the topology.


--*/
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
BOOL
CMSVADHW::bGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Return Value:

  True or False (in this example).

--*/
{
    return m_bDevSpecific;
}

//=============================================================================
void
CMSVADHW::bSetDevSpecific
(
    IN  BOOL devSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  fDevSpecific - true or false for this example.
--*/
{
    m_bDevSpecific = devSpecific;
}

//=============================================================================
INT
CMSVADHW::iGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  int (in this example).

--*/
{
    return m_iDevSpecific;
}

//=============================================================================
void
CMSVADHW::iSetDevSpecific
(
    IN  INT                 devSpecific
)
/*++
Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  fDevSpecific - true or false for this example.
--*/
{
    m_iDevSpecific = devSpecific;
}

//=============================================================================
UINT
CMSVADHW::uiGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  UINT (in this example).

--*/
{
    return m_uiDevSpecific;
}

//=============================================================================
void
CMSVADHW::uiSetDevSpecific
(
    IN  UINT                devSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  uiDevSpecific - int for this example.
--*/
{
    m_uiDevSpecific = devSpecific;
}


//=============================================================================
BOOL
CMSVADHW::GetMixerMute
(
    IN  ULONG                   node
)
/*++

Routine Description:

  Gets the HW (!) mute levels for MSVAD

Arguments:

  ulNode - topology node id

Return Value:

  mute setting

--*/
{
    if (node < MAX_TOPOLOGY_NODES)
    {
        return m_MuteControls[node];
    }

    return 0;
}

//=============================================================================
ULONG                       
CMSVADHW::GetMixerMux()
/*++

Routine Description:

  Return the current mux selection

--*/
{
    return m_ulMux;
}

//=============================================================================
LONG
CMSVADHW::GetMixerVolume
(   
    IN  ULONG node,
    IN  LONG  channel
)
/*++

Routine Description:

  Gets the HW (!) volume for MSVAD.

Arguments:

  ulNode - topology node id

  lChannel - which channel are we setting?

Return Value:

  LONG - volume level

--*/
{
    UNREFERENCED_PARAMETER(channel);

    if (node < MAX_TOPOLOGY_NODES)
    {
        return m_VolumeControls[node];
    }

    return 0;
}

//=============================================================================
#pragma code_seg("PAGE")
void CMSVADHW::MixerReset()
/*
  Resets the mixer registers.
*/
{
    PAGED_CODE();
    
    RtlFillMemory(m_VolumeControls, sizeof(LONG) * MAX_TOPOLOGY_NODES, 0xFF);
    RtlFillMemory(m_MuteControls,   sizeof(BOOL) * MAX_TOPOLOGY_NODES, TRUE);
    
    // BUGBUG change this depending on the topology
    m_ulMux = 2;
}
#pragma code_seg()

//=============================================================================
void CMSVADHW::SetMixerMute(IN  ULONG node, IN  BOOL fMute)
/*++

Routine Description:

  Sets the HW (!) mute levels for MSVAD

Arguments:

  node - topology node id

  fMute - mute flag
--*/
{
    if (node < MAX_TOPOLOGY_NODES)
    {
        m_MuteControls[node] = fMute;
    }
}

//=============================================================================
void CMSVADHW::SetMixerMux(IN  ULONG node)
/*++

Routine Description:

  Sets the HW (!) mux selection

Arguments:

  ulNode - topology node id

Return Value:



--*/
{
    m_ulMux = node;
}

//=============================================================================
void CMSVADHW::SetMixerVolume(IN  ULONG node, IN  LONG channel, IN  LONG volume)
/*++

Routine Description:

  Sets the HW (!) volume for MSVAD.

Arguments:

  node - topology node id
  channel - which channel are we setting?
  volume - volume level

--*/
{
    UNREFERENCED_PARAMETER(channel);

    if (node < MAX_TOPOLOGY_NODES)
    {
        m_VolumeControls[node] = volume;
    }
}
