/*
Abstract:
    Declaration of MSVAD HW class. 
    MSVAD HW has an array for storing mixer and volume settings for the topology.
*/

#ifndef _MSVAD_HW_H_
#define _MSVAD_HW_H_

//=============================================================================
// Defines
//=============================================================================
// BUGBUG we should dynamically allocate this...
#define MAX_TOPOLOGY_NODES      20

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CMSVADHW
// This class represents virtual MSVAD HW.
// An array representing volume registers and mute registers.

class CMSVADHW
{
public:
    CMSVADHW();
    
    void MixerReset();
    BOOL bGetDevSpecific();
    void bSetDevSpecific(IN  BOOL  bDevSpecific);

    INT  iGetDevSpecific();
    void iSetDevSpecific(  IN  INT iDevSpecific    );

    UINT  uiGetDevSpecific();
    void  uiSetDevSpecific(IN  UINT  uiDevSpecific);

    BOOL  GetMixerMute(  IN  ULONG ulNode);
    void  SetMixerMute(  IN  ULONG ulNode, IN  BOOL fMute);
    ULONG GetMixerMux(); 
    void  SetMixerMux(   IN  ULONG ulNode);
    LONG  GetMixerVolume(IN  ULONG ulNode, IN  LONG lChannel);
    void  SetMixerVolume(IN  ULONG ulNode, IN  LONG lChannel, IN  LONG lVolume);

protected:
    BOOL  muteControls_[MAX_TOPOLOGY_NODES];
    LONG  volumeControls_[MAX_TOPOLOGY_NODES];
    ULONG mux_;            // Mux selection
    BOOL  bDevSpecific_;
    INT   iDevSpecific_;
    UINT  uiDevSpecific_;
};
typedef CMSVADHW                *PCMSVADHW;

#endif
