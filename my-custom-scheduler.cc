#include "my-custom-scheduler.h"
#include "ns3/log.h"
#include "ns3/attribute.h"
#include "ns3/double.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyCustomScheduler");
NS_OBJECT_ENSURE_REGISTERED (MyCustomScheduler);

TypeId
MyCustomScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyCustomScheduler")
    .SetParent<NrMacSchedulerTdmaRR> ()
    .SetGroupName ("nr")
    .AddConstructor<MyCustomScheduler> ()
    .AddAttribute ("UrllcWeight",
                   "Relative priority/weight for URLLC slice.",
                   DoubleValue (3.0),
                   MakeDoubleAccessor (&MyCustomScheduler::m_urllcWeight),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("EmbbWeight",
                   "Relative priority/weight for eMBB slice.",
                   DoubleValue (2.0),
                   MakeDoubleAccessor (&MyCustomScheduler::m_embbWeight),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("MmtcWeight",
                   "Relative priority/weight for mMTC slice.",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&MyCustomScheduler::m_mmtcWeight),
                   MakeDoubleChecker<double> (0.0));
  return tid;
}

MyCustomScheduler::MyCustomScheduler ()
{
  NS_LOG_FUNCTION (this);
  // TIP: To prioritize, you can later override selection logic in TdmaRR
  // or intercept LC configuration (5QI/QCI) and map to the weights above.
  // This skeleton keeps TdmaRR behavior until you extend it.
}

} // namespace ns3
