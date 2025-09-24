#pragma once
#include "ns3/object.h"
#include "ns3/type-id.h"
#include "ns3/nr-mac-scheduler-tdma-rr.h"

namespace ns3 {

/**
 * \brief Minimal custom scheduler that inherits from TdmaRR.
 * 
 * This skeleton compiles and runs like TdmaRR but exposes slice weights
 * (URRLC > eMBB > mMTC) as attributes you can later use to bias scheduling.
 * Hook your logic by overriding internal selection/ranking methods if needed.
 */
class MyCustomScheduler : public NrMacSchedulerTdmaRR
{
public:
  static TypeId GetTypeId (void);
  MyCustomScheduler ();
  ~MyCustomScheduler () override = default;

private:
  double m_urllcWeight {3.0};
  double m_embbWeight  {2.0};
  double m_mmtcWeight  {1.0};
};

} // namespace ns3
