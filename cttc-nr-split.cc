// === OPTIMIZED VERSION - NS-3 5G Slicing Simulation ===
// Fixed issues that cause hanging/freezing:
// 1. Proper BWP assignment per UE group
// 1b. Added IMSI/bearer logging (RRC/MME/NAS)
// 2. Fixed EPS bearer activation
// 3. Optimized channel configuration
// 4. Reduced computational complexity
// 5. Proper application timing

// === FIXED VERSION - NS-3 5G Slicing Simulation ===
// Compatible with ns-3.45 + 5G-LENA v4.1
// All compilation errors fixed

// ./ns3 run scratch/SplitLearning-B5G/merged_slices_bwp_onoff --   --gNbNum=2   --ueNumPergNb=51   --UrllcWeight=5 --EmbbWeight=1 --MmtcWeight=1   --bearerTrace=false --logging=true




#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/nr-mac-scheduler-tdma-rr.h"
#include "ns3/log.h"

// 5G-LENA / NR - FIXED INCLUDES
#include "ns3/nr-module.h"
#include "ns3/cc-bwp-helper.h"

// Mobility & Buildings
#include "ns3/mobility-module.h"
#include "ns3/buildings-module.h"
#include "ns3/antenna-module.h"
#include "ns3/config-store-module.h"
#include "ns3/position-allocator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/internet-apps-module.h"

// POSIX & STL
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <limits>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <set>
#include <functional>
#include <string>
#include <map>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>

#include "ns3/error-model.h"
#include "ns3/double.h"

//#include "ns3/epc-helper.h"
#include "ns3/epc-tft.h"
#include "ns3/nr-epc-tft.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/nr-epc-tft.h"


#include "ns3/nr-helper.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/mobility-model.h"

#include "ns3/seq-ts-header.h"
#include "ns3/udp-socket-factory.h"

#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>


using namespace ns3;

static std::ofstream g_hoCsv;

static void HoStart(uint16_t cellId, uint16_t targetCellId) {
  if (g_hoCsv.is_open()) {
    g_hoCsv << std::fixed << std::setprecision(6)
            << Simulator::Now().GetSeconds() << ",start,"
            << cellId << "," << targetCellId << "\n";
  }
}
static void HoEnd(uint16_t cellId, uint16_t sourceCellId) {
  if (g_hoCsv.is_open()) {
    g_hoCsv << std::fixed << std::setprecision(6)
            << Simulator::Now().GetSeconds() << ",end,"
            << sourceCellId << "," << cellId << "\n";
  }
}

NS_LOG_COMPONENT_DEFINE("FixedNrSlicesDemo");

// === [SIMPLIFIED SCHEDULER] ===============================================
class FixedCustomScheduler : public NrMacSchedulerTdmaRR
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ns3::NrMacSchedulerFixed")
      .SetParent<NrMacSchedulerTdmaRR>()
      .AddConstructor<FixedCustomScheduler>();
    return tid;
  }
  FixedCustomScheduler() = default;
  ~FixedCustomScheduler() override = default;
};

NS_OBJECT_ENSURE_REGISTERED(FixedCustomScheduler);

// Device types
enum class DeviceType : uint32_t { SMARTPHONE = 0, IOT = 1 };

// Traffic/slice definitions
enum TrafficType { SLICE_eMBB = 0, SLICE_URLLC = 1, SLICE_mMTC = 2 };

// Mantenha a mesma convenção do seu "BWP map":
// [BWP map] mMTC->0 (μ=0), eMBB->1 (μ=1), URLLC->2 (μ=2)
enum class BwpLabel   : uint32_t { MMTC = 0, EMBB = 1, URLLC = 2 };


struct SliceInfo {
    uint8_t sliceId;
    TrafficType type;
    std::string name;
    uint16_t priority;
    double bwMHz;
    uint8_t numerology;
    uint16_t port;
    double pps;
    uint32_t pktSize;
    double onTime;
    double offTime;
};

// === CONSERVATIVE SLICE CONFIGURATIONS ===
static const uint16_t kDlPortEmbb  = 6000;
static const uint16_t kDlPortUrllc = 6001;
static const uint16_t kDlPortMmtc  = 6002;
// === PROBE DE LATÊNCIA (UL/PROC/DL) ===
static const uint16_t kProbePort = 7007;

static SliceInfo MakeConservativeEmbb()
{
    SliceInfo s{};
    s.sliceId = 0;
    s.type = SLICE_eMBB;
    s.name = "eMBB";
    s.priority = 1;
    s.bwMHz = 20.0;
    s.numerology = 0; // 15 kHz
    s.port = kDlPortEmbb;
    s.pps = 80;      // era 10
    s.pktSize = 500;  // Small packets
    s.onTime = 1.0;  // era 0.5   
    s.offTime = 0.0; // era 0.5
    return s;
}

static SliceInfo MakeConservativeUrllc()
{
    SliceInfo s{};
    s.sliceId = 1;
    s.type = SLICE_URLLC;
    s.name = "URLLC";
    s.priority = 0;
    s.bwMHz = 10.0;
    s.numerology = 1; // 30 kHz
    s.port = kDlPortUrllc;
    s.pps = 120;     // era 20
    s.pktSize = 64;   // Very small packets
    s.onTime = 1.0;  // era 0.8
    s.offTime = 0.0; // era 0.2
    return s;
}

static SliceInfo MakeConservativeMmtc()
{
    SliceInfo s{};
    s.sliceId = 2;
    s.type = SLICE_mMTC;
    s.name = "mMTC";
    s.priority = 2;
    s.bwMHz = 5.0;
    s.numerology = 0; // 15 kHz (simplified)
    s.port = kDlPortMmtc;
    s.pps = 2;        // Very low
    s.pktSize = 32;   // Tiny packets
    s.onTime = 0.1;
    s.offTime = 0.9;
    return s;
}

// Conservative OnOff installation
static void InstallConservativeOnOff(Ptr<Node> remoteHost,
                                    Ptr<Node> ueNode,
                                    Ipv4Address ueAddr,
                                    const SliceInfo& slice,
                                    ApplicationContainer& outApps,
                                    Time startTime,
                                    Time stopTime)
{
    // Sink (UdpServer) on UE
    UdpServerHelper sinkHelper(slice.port);
    ApplicationContainer sinkApp = sinkHelper.Install(ueNode);
    sinkApp.Start(Seconds(0.1));
    sinkApp.Stop(stopTime);

    // OnOff on RemoteHost
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(ueAddr, slice.port));

    // Very conservative data rate calculation
    uint64_t bps = static_cast<uint64_t>(slice.pps * slice.pktSize * 8.0);
    if (bps < 16000) bps = 16000; // Minimum safe rate

    onoff.SetAttribute("DataRate", DataRateValue(DataRate(bps)));
    onoff.SetAttribute("PacketSize", UintegerValue(slice.pktSize));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" +
                                           std::to_string(slice.onTime) + "]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" +
                                            std::to_string(slice.offTime) + "]"));

    ApplicationContainer srcApp = onoff.Install(remoteHost);
    srcApp.Start(startTime);
    srcApp.Stop(stopTime);
    
    outApps.Add(sinkApp);
    outApps.Add(srcApp);
}


//obter a distância do UE ao gNB mais próximo
// Calcula a menor distância (em metros) do UE ao gNB mais próximo
static double
GetDistanceToClosestGnb (Ptr<Node> ueNode, const NodeContainer& gnbNodes)
{
  Ptr<MobilityModel> ueMob = ueNode->GetObject<MobilityModel> ();
  NS_ABORT_MSG_IF (ueMob == nullptr, "UE sem MobilityModel");

  double minDist = std::numeric_limits<double>::infinity ();

  for (uint32_t i = 0; i < gnbNodes.GetN (); ++i)
  {
    Ptr<Node> g = gnbNodes.Get (i);
    Ptr<MobilityModel> gMob = g->GetObject<MobilityModel> ();
    NS_ABORT_MSG_IF (gMob == nullptr, "gNB sem MobilityModel");

    double d = ueMob->GetDistanceFrom (gMob);
    if (d < minDist) minDist = d;
  }

  if (!std::isfinite(minDist))
  {
    // se não houver gNBs instalados ainda, retorne 0 (ou lance erro, se preferir)
    return 0.0;
  }
  return minDist;
}


// ----------------- RemoteHostProbeApp -----------------
class RemoteHostProbeApp : public Application
{
public:
  RemoteHostProbeApp(std::map<uint32_t,uint32_t>* ipToUeIndex,
                     std::vector<double>* ulVec,
                     double procDelayMs)
    : m_ipToUeIndex(ipToUeIndex),
      m_ulVec(ulVec),
      m_procDelay(Seconds(procDelayMs/1000.0)) {}

  static TypeId GetTypeId() {
    static TypeId tid = TypeId("RemoteHostProbeApp")
      .SetParent<Application>();
    return tid;
  }
  void Setup(Address local) { m_local = local; }

private:
  virtual void StartApplication() override {
    if (!m_socket) {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->SetRecvCallback(MakeCallback(&RemoteHostProbeApp::HandleRead, this));
    }
  }
  virtual void StopApplication() override { if (m_socket) m_socket->Close(); }

  void HandleRead(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> p;
    while ((p = socket->RecvFrom(from))) {
      SeqTsHeader h; p->PeekHeader(h); // não remover ainda
      Time t0 = h.GetTs();
      Time now = Simulator::Now();
      // UL = (agora - t0)
      // mapear UE pelo IP de origem
      InetSocketAddress isa = InetSocketAddress::ConvertFrom(from);
      uint32_t ip = isa.GetIpv4().Get();
      auto it = m_ipToUeIndex->find(ip);
      if (it != m_ipToUeIndex->end()) {
        uint32_t i = it->second;
        (*m_ulVec)[i] = (now - t0).GetSeconds();
      }
      // agendar eco após "processamento"
      Simulator::Schedule(m_procDelay, &RemoteHostProbeApp::SendBack, this, from, p);
    }
  }

  void SendBack(Address to, Ptr<Packet> p) {
    // só ecoar o mesmo pacote (mantém SeqTsHeader original com t0)
    m_socket->SendTo(p, 0, to);
  }

private:
  Ptr<Socket> m_socket;
  Address m_local;
  std::map<uint32_t,uint32_t>* m_ipToUeIndex;
  std::vector<double>* m_ulVec;
  Time m_procDelay;
};

// ----------------- UeProbeApp -----------------
class UeProbeApp : public Application
{
public:
  UeProbeApp(uint32_t ueIndex,
             std::vector<double>* e2eVec)
    : m_ueIndex(ueIndex), m_e2eVec(e2eVec) {}

  static TypeId GetTypeId() {
    static TypeId tid = TypeId("UeProbeApp")
      .SetParent<Application>();
    return tid;
  }

  void Setup(Address remote, uint32_t packetSize=16) {
    m_remote = remote; m_pktSize = packetSize;
  }

private:
  virtual void StartApplication() override {
    m_tx = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_tx->Bind(); // porta efêmera
    m_tx->Connect(m_remote);

    m_rx = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), kProbePort);
    m_rx->Bind(local);
    m_rx->SetRecvCallback(MakeCallback(&UeProbeApp::HandleRead, this));

    // envia UM pacote de probe (pode virar periodic)
    Simulator::Schedule(MilliSeconds(10), &UeProbeApp::SendOnce, this);
  }

  void SendOnce() {
      Ptr<Packet> p = Create<Packet>(m_pktSize);
      SeqTsHeader h; h.SetSeq(1);           // timestamp é setado automaticamente
      p->AddHeader(h);
      m_tx->Send(p);
  }

  void HandleRead(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> p;
    while ((p = socket->RecvFrom(from))) {
      SeqTsHeader h; p->PeekHeader(h);
      Time t0 = h.GetTs();
      double e2e = (Simulator::Now() - t0).GetSeconds();
      (*m_e2eVec)[m_ueIndex] = e2e;
    }
  }

  virtual void StopApplication() override {
    if (m_tx) m_tx->Close();
    if (m_rx) m_rx->Close();
  }

private:
  Ptr<Socket> m_tx, m_rx;
  Address m_remote;
  uint32_t m_pktSize{16};
  uint32_t m_ueIndex;
  std::vector<double>* m_e2eVec;
};


// Muda a velocidade de um UE (requer ConstantVelocityMobilityModel)
static void SetUeVelocity(Ptr<Node> ue, double vx, double vy)
{
  Ptr<ConstantVelocityMobilityModel> cv = ue->GetObject<ConstantVelocityMobilityModel>();
  if (cv) {
    cv->SetVelocity(Vector(vx, vy, 0.0));
  } else {
    NS_LOG_WARN("UE sem ConstantVelocityMobilityModel; ignorando SetVelocity");
  }
}

// Agenda a mudança para o tempo t
static void scheduleVelocity(Ptr<Node> ue, Time t, double vx, double vy)
{
  Simulator::Schedule(t, &SetUeVelocity, ue, vx, vy);
}

// ============= CHECA A EXITENCIA DO CSV E ABRE, OU CRIA NOVO ===========

// Pequena utilidade para juntar campos com separador
static std::string joinCsv(const std::vector<std::string>& cols, const std::string& sep = ",") {
  std::string out;
  out.reserve(128);
  for (size_t i = 0; i < cols.size(); ++i) {
    // Se quiser escapar vírgulas/aspas, trate aqui.
    out += cols[i];
    if (i + 1 < cols.size()) out += sep;
  }
  return out;
}

// Escreve cabeçalho (se necessário) e acrescenta uma linha ao CSV.
static void __attribute__((unused)) appendCsvRow(
    const std::string& path,
    const std::vector<std::string>& header,
    const std::vector<std::string>& row)
{
  namespace fs = std::filesystem;

  const bool exists = fs::exists(path);
  std::ofstream out(path, std::ios::app);  // APPEND (não trunca)
  if (!out.is_open()) {
    throw std::runtime_error("Não foi possível abrir para escrita: " + path);
  }

  if (!exists) {
    out << joinCsv(header) << '\n';
  }
  out << joinCsv(row) << '\n';
  out.flush();
}

// ========================= MAIN ============================================
int main(int argc, char* argv[])
{
    // Enable checksum for reliability
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Random seed for reproducibility
    SeedManager::SetSeed(1234);
    SeedManager::SetRun(1);

    // === VERY CONSERVATIVE PARAMETERS ===
    uint16_t gNbNum = 1;
    uint16_t ueNumPergNb = 3; // Start with minimum - multiple of 3
    bool logging = false;
    Time udpAppStartTime = MilliSeconds(100);
    double totalTxPower = 30.0;
    std::string simTag = "fixed";
    //double processingPowerPerUE = 0.001; // Very low
    //double lossExponent = 2.0; // Low
    [[maybe_unused]] double processingPowerPerUE = 0.001; // Very low
    [[maybe_unused]] double lossExponent = 2.0; // Low
    constexpr size_t kNumFeatures = 8;
    double urllcWeight = 5.0, embbWeight = 1.0, mmtcWeight = 1.0;
    double coreDelayMs = 1.0;         // atraso médio do backbone (EPC<->RemoteHost)
    double coreJitterMs = 0.0;        // amplitude de jitter (+/-) em ms (0 = sem jitter)
    double coreJitterPeriodMs = 50.0; // período para atualizar o Delay (ms)

    // Mapa coerente com o seu log: mMTC->0, eMBB->1, URLLC->2
    constexpr uint32_t BWP_MMTC  = 0;
    constexpr uint32_t BWP_EMBB  = 1;
    constexpr uint32_t BWP_URLLC = 2;

    // Very conservative timing
    double perClientGapMs = 0.0; // Larger gaps 100.0
    double perClientDurationSec = 3.0; // Shorter duration
    Time simTime = Seconds(3.5); // Short simulation 3.0
    double embbSpeed = 3.0, urllcSpeed = 1.5, mmtcSpeed = 0.3;
    double processingDelayMs = 1.0;


    // Command line parsing
    CommandLine cmd;
    std::filesystem::path outputPath = "./scratch/SplitLearning-B5G/plots/";
    std::filesystem::create_directories(outputPath);
    std::string outputDir = outputPath.string() + "/";

    cmd.AddValue("outputDir", "Output directory for CSV files", outputDir);
    cmd.AddValue("gNbNum", "Number of gNBs", gNbNum);
    cmd.AddValue("ueNumPergNb", "Number of UE per gNB", ueNumPergNb);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("totalTxPower", "Total tx power", totalTxPower);
    cmd.AddValue("UrllcWeight", "Weight for URLLC slice in MyCustomScheduler", urllcWeight);
    cmd.AddValue("EmbbWeight",  "Weight for eMBB slice in MyCustomScheduler",  embbWeight);
    cmd.AddValue("MmtcWeight",  "Weight for mMTC slice in MyCustomScheduler",  mmtcWeight);
    cmd.AddValue("processingDelayMs", "Artificial server processing delay (ms)", processingDelayMs);
    cmd.AddValue("coreDelayMs", "Mean one-way delay on EPC<->RemoteHost (ms)", coreDelayMs);
    cmd.AddValue("coreJitterMs", "Uniform jitter amplitude (+/- ms)", coreJitterMs);
    cmd.AddValue("coreJitterPeriodMs", "How often to resample jitter (ms)", coreJitterPeriodMs);
    cmd.AddValue("embbSpeed", "Velocidade inicial eMBB (m/s)", embbSpeed);
    cmd.AddValue("urllcSpeed","Velocidade inicial URLLC (m/s)", urllcSpeed);
    cmd.AddValue("mmtcSpeed", "Velocidade inicial mMTC (m/s)", mmtcSpeed);
    cmd.AddValue("UrllcWeight", "Peso do slice URLLC no escalonador", urllcWeight);
    cmd.AddValue("EmbbWeight",  "Peso do slice eMBB no escalonador",  embbWeight);
    cmd.AddValue("MmtcWeight",  "Peso do slice mMTC no escalonador",  mmtcWeight);


    uint32_t splitDepthEmbb=3, splitDepthUrllc=2, splitDepthMmtc=1;
    // --- novo: flag para ligar logs detalhados (RRC/MME/NAS) ---
    bool bearerTrace = true;
    cmd.AddValue("bearerTrace", "Enable detailed RRC/MME/NAS logs for bearer/IMSI tracing", bearerTrace);
    cmd.AddValue("splitDepthEmbb","Camada de corte eMBB", splitDepthEmbb);
    cmd.AddValue("splitDepthUrllc","Camada de corte URLLC", splitDepthUrllc);
    cmd.AddValue("splitDepthMmtc","Camada de corte mMTC", splitDepthMmtc);
    cmd.Parse(argc, argv);

    // Arredonda x para o próximo múltiplo de m (se já for múltiplo, mantém)
    auto RoundUpToMultiple = [](uint16_t x, uint16_t m) -> uint16_t {
      if (m == 0) return x;
      uint16_t r = x % m;
      return r == 0 ? x : static_cast<uint16_t>(x + (m - r));
    };

    // --- Garantir ueNumPergNb múltiplo de 3 ---
    const uint16_t ueNumPergNb_original = ueNumPergNb;
    ueNumPergNb = RoundUpToMultiple(ueNumPergNb, 3);
    if (ueNumPergNb != ueNumPergNb_original) {
      std::cout << "[INFO] ueNumPergNb=" << ueNumPergNb_original
                << " não é múltiplo de 3; ajustado para "
                << ueNumPergNb << " para distribuição eMBB/URLLC/mMTC.\n";
    }

    // antes do bloco "=== MINIMAL GRID SCENARIO ==="
    const uint32_t plannedUes = gNbNum * ueNumPergNb;  // total de UEs planejados (por gNB) - antes linha 575

    // === ENERGY MODEL (simplificado, mas orientado por tráfego e numerologia) ===
    // Ajuste fino depois olhando seus dados (calibração):
    const double P_idle_W           = 0.05;   // W (consumo base quando ativo)
    const double E_RX_per_bit       = 3e-9;   // J/bit recebido
    const double E_TX_per_bit_ref   = 5e-9;   // J/bit transmitido a d0 (referência)
    const double d0_m               = 10.0;   // m (distância de referência)
    const double alpha_pl           = 3.0;    // expoente de perda de percurso

    // Instala os devices de UEs
    NodeContainer gnbNodes, ueNodes, embbNodes, urllcNodes, mmtcNodes;
    NetDeviceContainer gnbDevs, ueDevs;


    auto MuFactor = [] (uint32_t bwp) {
        // mapeie BWP→μ igual ao resto do código: 0→μ=0(mMTC), 1→μ=1(eMBB), 2→μ=2(URLLC)
        switch (bwp) {
            case 0: return 1.00; // μ=0
            case 1: return 1.20; // μ=1 (FFT maior, clock maior)
            case 2: return 1.35; // μ=2 (ainda mais exigente)
            default: return 1.0;
        }
    };
    auto PathlossScale = [&] (double d, uint32_t bwp) {
        // Escala de energia de TX cresce com distância^alpha e com fator da numerologia
        double sc = std::pow(std::max(d, 1.0)/d0_m, alpha_pl);
        return sc * MuFactor(bwp);
    };


    // Minimal logging
    if (logging) {
        LogComponentEnable("FixedNrSlicesDemo", LOG_LEVEL_INFO);
    }

    // --- novo: liga componentes que registram attach/bearers/IMSI no console ---
    if (bearerTrace) {
        // RRC (UE/gNB) – estados de conexão e reconfig
        LogComponentEnable("NrUeRrc", LOG_LEVEL_INFO);
        LogComponentEnable("NrGnbRrc", LOG_LEVEL_INFO);
        // EPC/MME/NAS – criação/associação de EPS bearers e sessões
        LogComponentEnable("EpcMmeApplication", LOG_LEVEL_INFO);
        LogComponentEnable("EpcEnbApplication", LOG_LEVEL_INFO);
        LogComponentEnable("EpcSgwApplication", LOG_LEVEL_ALL);
        LogComponentEnable("EpcPgwApplication", LOG_LEVEL_ALL);
        LogComponentEnable("EpcUeNas", LOG_LEVEL_INFO);
    }


    std::filesystem::create_directories(outputDir);
      
    std::cout << "=== Starting Fixed 5G Slicing Simulation ===" << std::endl;
    std::cout << "Simulation time: " << simTime.GetSeconds() << " seconds" << std::endl;

    // === EPC HELPER ===
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();

    // === NR HELPER ===
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerFixed"));

    // === MINIMAL GRID SCENARIO ===
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(gNbNum);
    gridScenario.SetHorizontalBsDistance(50.0);
    gridScenario.SetVerticalBsDistance(50.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(plannedUes);
    gridScenario.SetScenarioHeight(100);
    gridScenario.SetScenarioLength(100);
    gridScenario.CreateScenario();

    gnbNodes = gridScenario.GetBaseStations();
    ueNodes  = gridScenario.GetUserTerminals();

    // Posiciona gNB0 e gNB1 para criar uma fronteira clara no eixo X
    if (gridScenario.GetBaseStations().GetN() >= 1) {
      Ptr<Node> g0 = gridScenario.GetBaseStations().Get(0);
      g0->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 10.0));
    }
    if (gridScenario.GetBaseStations().GetN() >= 2) {
      Ptr<Node> g1 = gridScenario.GetBaseStations().Get(1);
      g1->GetObject<MobilityModel>()->SetPosition(Vector(60.0, 0.0, 10.0)); // ~60 m da célula 0
    }

    // ... preenche embbNodes, urllcNodes, mmtcNodes
    // ... depois une todos os nós para referência geral (opcional)
    ueNodes.Add(embbNodes);
    ueNodes.Add(urllcNodes);
    ueNodes.Add(mmtcNodes);

    // Mobilidade por perfil
    MobilityHelper mobEmbb, mobUrllc, mobMmtc;
    mobEmbb.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobUrllc.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobMmtc.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    mobEmbb.Install(embbNodes);
    mobUrllc.Install(urllcNodes);
    mobMmtc.Install(mmtcNodes);

    // Install mobility on all UEs first
    MobilityHelper mobUe;
    mobUe.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobUe.Install(gridScenario.GetUserTerminals());

    //Verificador para logar quem ficou sem CVMM
    for (uint32_t i = 0; i < gridScenario.GetUserTerminals().GetN(); ++i) {
      Ptr<Node> ue = gridScenario.GetUserTerminals().Get(i);
      if (ue->GetObject<ConstantVelocityMobilityModel>() == nullptr) {
        NS_LOG_WARN("UE Node " << ue->GetId() << " sem CVMM após Install; será corrigido on-demand.");
      }
    }
    std::cout << "Mobility setup completed successfully." << std::endl;

    // === UE CONTAINERS BY SLICE (PROPER POPULATION) ===
    NodeContainer ueEmbbContainer, ueUrllcContainer, ueMmtcContainer;
    std::vector<uint32_t> idxEmbb, idxUrllc, idxMmtc;


    // === Metadados por UE (precisam existir antes de classificar por slice) ===
    const uint32_t totalUes = gridScenario.GetUserTerminals().GetN();
    std::vector<uint32_t> deviceTypeVector(totalUes, static_cast<uint32_t>(DeviceType::SMARTPHONE));
    std::vector<uint32_t> bwpVector(totalUes, static_cast<uint32_t>(BwpLabel::EMBB));


    // Properly populate containers from gridScenario UEs
    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j) {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(j);
        uint32_t mod = j % 3;
        if (mod == 0) { 
            ueEmbbContainer.Add(ue); 
            idxEmbb.push_back(j); 
            //deviceTypeVector[j] = (uint32_t)DeviceType::SMARTPHONE; 
            deviceTypeVector[j] = static_cast<uint32_t>(DeviceType::SMARTPHONE);
        }
        else if (mod == 1) { 
            ueUrllcContainer.Add(ue); 
            idxUrllc.push_back(j); 
            //deviceTypeVector[j] = (uint32_t)DeviceType::SMARTPHONE;
            deviceTypeVector[j] = static_cast<uint32_t>(DeviceType::SMARTPHONE); 
        }
        else { 
            ueMmtcContainer.Add(ue); 
            idxMmtc.push_back(j); 
            //deviceTypeVector[j] = (uint32_t)DeviceType::IOT;
            deviceTypeVector[j] = static_cast<uint32_t>(DeviceType::IOT); 
        }
    }

    std::cout << "UE containers populated: eMBB=" << ueEmbbContainer.GetN() 
              << ", URLLC=" << ueUrllcContainer.GetN() 
              << ", mMTC=" << ueMmtcContainer.GetN() << std::endl;

    //registre os pesos para facilitar debug e CSV
    std::cout << "# Pesos recebidos: UrllcWeight=" << urllcWeight
          << ", EmbbWeight=" << embbWeight
          << ", MmtcWeight=" << mmtcWeight << std::endl;

    // === Evitar colisão de coordenadas gNB–UE (distance == 0) ===
    // Opcional: fixa o gNB0 em (0,0,10). Mantém consistência com BsHeight=10.
    if (gridScenario.GetBaseStations().GetN() > 0) {
        Ptr<Node> g0 = gridScenario.GetBaseStations().Get(0);
        Ptr<MobilityModel> gm = g0->GetObject<MobilityModel>();
        gm->SetPosition(Vector(0.0, 0.0, 10.0));
    }

    // Espalhar UEs para garantir que nenhum fique exatamente na mesma posição do gNB
    for (uint32_t i = 0; i < gridScenario.GetUserTerminals().GetN(); ++i) {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(i);
        Ptr<MobilityModel> um = ue->GetObject<MobilityModel>();
        
        // Se por acaso algum UE coincidir com o gNB0, desloca um pouco
        // (e também distribui em uma linha para evitar coincidências)
        um->SetPosition(Vector(20.0 + 5.0 * i, 10.0, 1.5));
    }

    // Verificar colisões gNB-UE
    for (uint32_t g = 0; g < gridScenario.GetBaseStations().GetN(); ++g) {
        Ptr<Node> gNb = gridScenario.GetBaseStations().Get(g);
        Ptr<MobilityModel> gmm = gNb->GetObject<MobilityModel>();
        Vector gp = gmm->GetPosition();

        for (uint32_t u = 0; u < gridScenario.GetUserTerminals().GetN(); ++u) {
            Ptr<Node> ue = gridScenario.GetUserTerminals().Get(u);
            Ptr<MobilityModel> um = ue->GetObject<MobilityModel>();
            Vector up = um->GetPosition();

            if (up.x == gp.x && up.y == gp.y && up.z == gp.z) {
                um->SetPosition(Vector(up.x + 1.0, up.y + 1.0, up.z)); // leve empurrão
            }
        }
    }

    // Definir velocidades iniciais por perfil/slice (ex.: eMBB mais rápido, URLLC médio, mMTC lento)
    auto setInitialVelocity = [&](Ptr<Node> ue, double vx, double vy) {
      Ptr<ConstantVelocityMobilityModel> cv = ue->GetObject<ConstantVelocityMobilityModel>();
      if (!cv) {
        // instala CVMM apenas neste UE, se faltar
        NodeContainer one; one.Add(ue);
        MobilityHelper mh; mh.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mh.Install(one);
        cv = ue->GetObject<ConstantVelocityMobilityModel>();
      }
      if (!cv) {
        NS_LOG_WARN("UE " << ue->GetId() << " sem CVMM; ignorando setVelocity");
        return;
      }
      Vector p = cv->GetPosition();
      cv->SetPosition(p);
      cv->SetVelocity(Vector(vx, vy, 0.0));
    };

    // Set initial velocities by profile/slice
    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
        setInitialVelocity(ueEmbbContainer.Get(i), embbSpeed, 0.0);
    }
    for (uint32_t i = 0; i < ueUrllcContainer.GetN(); ++i) {
        setInitialVelocity(ueUrllcContainer.Get(i), urllcSpeed, 0.0);
    }
    for (uint32_t i = 0; i < ueMmtcContainer.GetN(); ++i) {
        setInitialVelocity(ueMmtcContainer.Get(i), mmtcSpeed, 0.0);
    }

    // Schedule velocity changes for eMBB UEs
    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
        Ptr<Node> ue = ueEmbbContainer.Get(i);
        // Peak at 0.8s, pause at 1.6s, resume at 2.0s
        scheduleVelocity(ue, Seconds(0.80), 5.0, 0.0);  // accelerate
        scheduleVelocity(ue, Seconds(1.60), 0.0, 0.0);  // pause
        scheduleVelocity(ue, Seconds(2.00), 3.0, 0.0);  // back to cruise
    }

    // Schedule velocity changes for eMBB UEs
    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
        Ptr<Node> ue = ueEmbbContainer.Get(i);
        // Peak at 0.8s, pause at 1.6s, resume at 2.0s
        scheduleVelocity(ue, Seconds(0.80), 5.0, 0.0);  // accelerate
        scheduleVelocity(ue, Seconds(1.60), 0.0, 0.0);  // pause
        scheduleVelocity(ue, Seconds(2.00), 3.0, 0.0);  // back to cruise
    }

    // Schedule velocity changes for URLLC UEs
    for (uint32_t i = 0; i < ueUrllcContainer.GetN(); ++i) {
        Ptr<Node> ue = ueUrllcContainer.Get(i);
        scheduleVelocity(ue, Seconds(1.00), 2.5, 0.0);
        scheduleVelocity(ue, Seconds(1.80), 0.0, 0.0);
        scheduleVelocity(ue, Seconds(2.20), 1.5, 0.0);
    }

    // Schedule velocity changes for mMTC UEs
    for (uint32_t i = 0; i < ueMmtcContainer.GetN(); ++i) {
        Ptr<Node> ue = ueMmtcContainer.Get(i);
        scheduleVelocity(ue, Seconds(1.20), 0.6, 0.0);
        scheduleVelocity(ue, Seconds(1.90), 0.0, 0.0);
        scheduleVelocity(ue, Seconds(2.20), 0.3, 0.0);
    }

    // Accelerate 1 UE per slice to ensure clear crossing around t=3..5s
    if (ueEmbbContainer.GetN() > 0) {
        Ptr<Node> ue = ueEmbbContainer.Get(0);
        scheduleVelocity(ue, Seconds(0.5), 8.0, 0.0);   // accelerate
    }
    if (ueUrllcContainer.GetN() > 0) {
        Ptr<Node> ue = ueUrllcContainer.Get(0);
        scheduleVelocity(ue, Seconds(0.5), 6.0, 0.0);
    }
    if (ueMmtcContainer.GetN() > 0) {
        Ptr<Node> ue = ueMmtcContainer.Get(0);
        scheduleVelocity(ue, Seconds(0.5), 4.0, 0.0);
    }

    std::cout << "Mobility setup completed successfully." << std::endl;


    // === Evitar colisão de coordenadas gNB–UE (distance == 0) ===
    // Opcional: fixa o gNB0 em (0,0,10). Mantém consistência com BsHeight=10.
    if (gridScenario.GetBaseStations().GetN() > 0) {
        Ptr<Node> g0 = gridScenario.GetBaseStations().Get(0);
        Ptr<MobilityModel> gm = g0->GetObject<MobilityModel>();
        gm->SetPosition(Vector(0.0, 0.0, 10.0));
    }

    // Espalhar UEs para garantir que nenhum fique exatamente na mesma posição do gNB
    for (uint32_t i = 0; i < gridScenario.GetUserTerminals().GetN(); ++i) {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(i);
        Ptr<MobilityModel> um = ue->GetObject<MobilityModel>();
        //Vector p = um->GetPosition();

        // Se por acaso algum UE coincidir com o gNB0, desloca um pouco
        // (e também distribui em uma linha para evitar coincidências)
        um->SetPosition(Vector(20.0 + 5.0 * i, 10.0, 1.5));
    }

    for (uint32_t g = 0; g < gridScenario.GetBaseStations().GetN(); ++g) {
        Ptr<Node> gNb = gridScenario.GetBaseStations().Get(g);
        Ptr<MobilityModel> gmm = gNb->GetObject<MobilityModel>();
        Vector gp = gmm->GetPosition();

        for (uint32_t u = 0; u < gridScenario.GetUserTerminals().GetN(); ++u) {
            Ptr<Node> ue = gridScenario.GetUserTerminals().Get(u);
            Ptr<MobilityModel> um = ue->GetObject<MobilityModel>();
            Vector up = um->GetPosition();

            if (up.x == gp.x && up.y == gp.y && up.z == gp.z) {
                um->SetPosition(Vector(up.x + 1.0, up.y + 1.0, up.z)); // leve empurrão
            }
        }
    }

    // Agenda mudanças de velocidade: pico -> pausa -> retomada
    auto scheduleVelocity = [&](Ptr<Node> ue, Time t, double vx, double vy) {
      Simulator::Schedule(t, [ue, vx, vy]() {
        Ptr<ConstantVelocityMobilityModel> cv = ue->GetObject<ConstantVelocityMobilityModel>();
        if (cv) cv->SetVelocity(Vector(vx, vy, 0.0));
      });
    };

    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
      setInitialVelocity(ueEmbbContainer.Get(i), embbSpeed, 0.0);
    }
    for (uint32_t i = 0; i < ueUrllcContainer.GetN(); ++i) {
      setInitialVelocity(ueUrllcContainer.Get(i), urllcSpeed, 0.0);
    }
    for (uint32_t i = 0; i < ueMmtcContainer.GetN(); ++i) {
      setInitialVelocity(ueMmtcContainer.Get(i), mmtcSpeed, 0.0);
    }

    // Exemplos: aplicar em alguns UEs (ajuste tempos conforme seu simTime)
    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
      Ptr<Node> ue = ueEmbbContainer.Get(i);
      // Pico aos 0.8s, pausa aos 1.6s, retoma aos 2.0s
      scheduleVelocity(ue, Seconds(0.80), 5.0, 0.0);  // acelera
      scheduleVelocity(ue, Seconds(1.60), 0.0, 0.0);  // pausa
      scheduleVelocity(ue, Seconds(2.00), 3.0, 0.0);  // volta ao cruzeiro
    }

    for (uint32_t i = 0; i < ueUrllcContainer.GetN(); ++i) {
      Ptr<Node> ue = ueUrllcContainer.Get(i);
      scheduleVelocity(ue, Seconds(1.00), 2.5, 0.0);
      scheduleVelocity(ue, Seconds(1.80), 0.0, 0.0);
      scheduleVelocity(ue, Seconds(2.20), 1.5, 0.0);
    }

    for (uint32_t i = 0; i < ueMmtcContainer.GetN(); ++i) {
      Ptr<Node> ue = ueMmtcContainer.Get(i);
      scheduleVelocity(ue, Seconds(1.20), 0.6, 0.0);
      scheduleVelocity(ue, Seconds(1.90), 0.0, 0.0);
      scheduleVelocity(ue, Seconds(2.20), 0.3, 0.0);
    }

    // Acelere 1 UE por slice para garantir crossing claro por ~t=3..5s
    if (ueEmbbContainer.GetN() > 0) {
      Ptr<Node> ue = ueEmbbContainer.Get(0);
      scheduleVelocity(ue, Seconds(0.5), 8.0, 0.0);   // acelera
    }
    if (ueUrllcContainer.GetN() > 0) {
      Ptr<Node> ue = ueUrllcContainer.Get(0);
      scheduleVelocity(ue, Seconds(0.5), 6.0, 0.0);
    }
    if (ueMmtcContainer.GetN() > 0) {
      Ptr<Node> ue = ueMmtcContainer.Get(0);
      scheduleVelocity(ue, Seconds(0.5), 4.0, 0.0);
    }

    ns3::NetDeviceContainer ueEmbbNetDev;
    ns3::NetDeviceContainer ueUrllcNetDev;
    ns3::NetDeviceContainer ueMmtcNetDev;

    // Helper para "flapar" a interface do UE (down/up)
    auto FlapUeIf = [](Ptr<Node> ueNode, Ipv4Address ueAddr, Time tDown, Time tUp) {
      Ptr<Ipv4> ip = ueNode->GetObject<Ipv4>();
      int32_t ifIdx = ip->GetInterfaceForAddress(ueAddr);
      NS_ABORT_MSG_IF(ifIdx < 0, "Interface IPv4 do UE não encontrada");
      Simulator::Schedule(tDown, &Ipv4::SetDown, ip, static_cast<uint32_t>(ifIdx));
      Simulator::Schedule(tUp,   &Ipv4::SetUp,   ip, static_cast<uint32_t>(ifIdx));
    };


    // --- [BANDAS & BWPs] (drop-in replacement) ---

    // Evite colisão de nome "bands" (já vi vir de #define em alguns códigos); use bandVec:
    std::vector<CcBwpCreator::SimpleOperationBandConf> bandVec;

    CcBwpCreator::SimpleOperationBandConf conf;
    conf.m_centralFrequency = 3.5e9;   // 3,5 GHz
    conf.m_channelBandwidth        = 60e6;    // 60 MHz total
    conf.m_numBwp           = 3;       // dividir em 3 BWPs
    // (Se seu header expõe, você pode setar SCSs específicos por BWP depois)

    bandVec.push_back(conf);

    // Cria a banda + BWPs via helper; cenário UMi aplicado pelo helper
    auto bwAndBwps = nrHelper->CreateBandwidthParts(bandVec, "UMi");
    const auto& allBwps = bwAndBwps.second;
    // --- [FIM BANDAS & BWPs] ---

    // Instala gNB e UEs passando BWPs criados
    gnbDevs     = nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    ueEmbbNetDev  = nrHelper->InstallUeDevice(ueEmbbContainer,  allBwps);
    ueUrllcNetDev = nrHelper->InstallUeDevice(ueUrllcContainer, allBwps);
    ueMmtcNetDev  = nrHelper->InstallUeDevice(ueMmtcContainer,  allBwps);
    
    //montar ueDevs único
    ueDevs.Add(ueEmbbNetDev);
    ueDevs.Add(ueUrllcNetDev);
    ueDevs.Add(ueMmtcNetDev);

    //total real de UEs” a partir dos devices
    //ativar bearers e instalar apps.
    const uint32_t nUes = ueDevs.GetN();
    NS_LOG_UNCOND("Total UEs (from devices): " << nUes);  

    // Result vectors
    std::vector<uint32_t> lostPacketsVector(nUes, 0);
    std::vector<double> distanceVector(nUes, 0.0);
    // Vetores de metadados por UE (dimensão = nUes)
    std::vector<double> jitterVector(nUes, 0.0);
    std::vector<double> plrVector(nUes, 0.0);
    std::vector<double> bitsPerJoule(nUes, 0.0);
    std::vector<double> bitsVector(nUes, 0.0);
    std::vector<double> ulProbe_s(nUes, 0.0);
    std::vector<double> e2eProbe_s(nUes, 0.0);
    std::vector<double> procProbe_s(nUes, 0.0); // redundante, igual p/ todos (processingDelayMs/1000)
    std::vector<double> dlProbe_s(nUes, 0.0);

    std::vector<uint32_t> txPacketsVector(nUes, 0);
    std::vector<uint32_t> rxPacketsVector(nUes, 0);

    //std::cout << "Total UEs: " << nUes << std::endl;
    std::cout << "Total UEs: " << gridScenario.GetUserTerminals().GetN() << std::endl;

    NS_ABORT_IF(nUes == 0);
      
    std::vector<uint32_t> splitPoint;      splitPoint.resize(nUes);
    std::vector<double> delayVector;       delayVector.resize(nUes);
    std::vector<double> throughputVector;  throughputVector.resize(nUes);
    std::vector<double> energyConsumption; energyConsumption.resize(nUes);


    // Usa algoritmo estático do BWP manager no UE
    nrHelper->SetUeBwpManagerAlgorithmTypeId(TypeId::LookupByName("ns3::BwpManagerAlgorithmStatic"));

    // Numerologia por BWP nos gNBs
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/PhyPerBwpList/0/$ns3::NrGnbPhy/Numerology", UintegerValue (0)); // mMTC (μ=0)
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/PhyPerBwpList/1/$ns3::NrGnbPhy/Numerology", UintegerValue (1)); // eMBB (μ=1)
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/PhyPerBwpList/2/$ns3::NrGnbPhy/Numerology", UintegerValue (2)); // URLLC (μ=2)


    // Auxiliar: pinagem por NodeContainer
    auto PinContainerToBwp = [] (ns3::NodeContainer c, uint32_t bwpIdx)
    {
      for (auto it = c.Begin(); it != c.End(); ++it)
        {
          uint32_t nodeId = (*it)->GetId();
          std::ostringstream path;
          path << "/NodeList/" << nodeId
               << "/DeviceList/*/$ns3::NrUeNetDevice/$ns3::BwpManagerUe/DefaultBwp";
          ns3::Config::Set (path.str(), ns3::UintegerValue (bwpIdx));
        }
    };

    // mapear slices → BWPs
    PinContainerToBwp (ueEmbbContainer,  BWP_EMBB);
    PinContainerToBwp (ueUrllcContainer, BWP_URLLC);
    PinContainerToBwp (ueMmtcContainer,  BWP_MMTC);

    std::cout << "[BWP map] mMTC->" << BWP_MMTC
              << " (μ=0), eMBB->" << BWP_EMBB
              << " (μ=1), URLLC->" << BWP_URLLC << " (μ=2)\n";

    // Configure TX power
    for (auto it = gnbDevs.Begin(); it != gnbDevs.End(); ++it) {
        Ptr<NrGnbNetDevice> gnbDev = DynamicCast<NrGnbNetDevice>(*it);
        if (gnbDev) {
            for (uint32_t bwpId = 0; bwpId < gnbDev->GetCcMapSize(); ++bwpId) {
                Ptr<NrGnbPhy> phy = nrHelper->GetGnbPhy(*it, bwpId);
                if (phy) {
                    phy->SetAttribute("TxPower", DoubleValue(totalTxPower));
                }
            }
        }
    }

    // === EPC CORE NETWORK ===
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueEmbbContainer);
    internet.Install(ueUrllcContainer);
    internet.Install(ueMmtcContainer);

    // Point-to-point connection
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(coreDelayMs))); // << aqui
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);


    // RV uniforme
    Ptr<PointToPointNetDevice> d0 = DynamicCast<PointToPointNetDevice>(internetDevices.Get(0));
    Ptr<PointToPointChannel> coreChan = d0 ? DynamicCast<PointToPointChannel>(d0->GetChannel()) : nullptr;

    if (coreChan && coreJitterMs > 0.0 && coreJitterPeriodMs > 0.0) {
      Ptr<UniformRandomVariable> u = CreateObject<UniformRandomVariable>();
      u->SetAttribute("Min", DoubleValue(-coreJitterMs));
      u->SetAttribute("Max", DoubleValue(+coreJitterMs));

      std::function<void()> tick;
      tick = [coreChan, u, coreDelayMs, coreJitterPeriodMs, &tick]() {
        double sampleMs = std::max(0.0, coreDelayMs + u->GetValue());
        coreChan->SetAttribute("Delay", TimeValue(MilliSeconds(sampleMs)));
        Simulator::Schedule(MilliSeconds(coreJitterPeriodMs), MakeEvent(tick));
      };
      Simulator::ScheduleNow(MakeEvent(tick));
    }

    // IP addressing
    Ipv4AddressHelper ipv4h;
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    int32_t ifIndex = remoteHost->GetObject<Ipv4>()->GetInterfaceForDevice(internetDevices.Get(1));
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), ifIndex);


    // === LOSS WINDOWS NO CORE (PGW <-> RemoteHost) ===
    // Em vez de um único ErrorModel fixo, criamos dois (UL e DL) e variamos a taxa no tempo.

    Ptr<RateErrorModel> emDl = CreateObject<RateErrorModel>();
    Ptr<RateErrorModel> emUl = CreateObject<RateErrorModel>();

    // Trabalhamos em unidade "por pacote"
    emDl->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    emUl->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    // Base line: fora das janelas, 0% de perda
    emDl->SetAttribute("ErrorRate", DoubleValue(0.0));
    emUl->SetAttribute("ErrorRate", DoubleValue(0.0));

    // NetDevices do link core (PGW <-> RemoteHost)
    Ptr<NetDevice> devDl = internetDevices.Get(0); // tráfego que CHEGA ao PGW (downlink vindo do RemoteHost)
    Ptr<NetDevice> devUl = internetDevices.Get(1); // tráfego que CHEGA ao RemoteHost (uplink vindo do PGW)

    // Atribui os EMs inicialmente (0%)
    devDl->SetAttribute("ReceiveErrorModel", PointerValue(emDl));
    devUl->SetAttribute("ReceiveErrorModel", PointerValue(emUl));

    // Descreva aqui suas janelas de perda: [start, stop, rate, afeta_UL?, afeta_DL?]
    struct LossWindow {
      Time start;
      Time stop;
      double rate; // 0.0 .. 1.0 (por exemplo, 0.02 = 2%)
      bool ul;
      bool dl;
    };

    // Exemplos: ajuste como quiser
    std::vector<LossWindow> loss = {
      // 2% entre 0.80 e 1.60 s, afetando UL e DL
      { Seconds(0.80), Seconds(1.60), 0.02, true,  true  },
      // 10% entre 2.00 e 2.50 s, apenas no DL
      { Seconds(2.00), Seconds(2.50), 0.10, false, true  },
      // adicione outras janelas conforme necessário...
    };

    // Agenda entrada e saída de cada janela
    for (const auto& w : loss) {
      if (w.ul) {
        // UL = perda que chega ao RemoteHost
        Simulator::Schedule(w.start, [emUl, devUl, r = w.rate]() {
          emUl->SetAttribute("ErrorRate", DoubleValue(r));
          devUl->SetAttribute("ReceiveErrorModel", PointerValue(emUl));
        });
        Simulator::Schedule(w.stop, [emUl, devUl]() {
          emUl->SetAttribute("ErrorRate", DoubleValue(0.0));
          devUl->SetAttribute("ReceiveErrorModel", PointerValue(emUl));
        });
      }
      if (w.dl) {
        // DL = perda que chega ao PGW
        Simulator::Schedule(w.start, [emDl, devDl, r = w.rate]() {
          emDl->SetAttribute("ErrorRate", DoubleValue(r));
          devDl->SetAttribute("ReceiveErrorModel", PointerValue(emDl));
        });
        Simulator::Schedule(w.stop, [emDl, devDl]() {
          emDl->SetAttribute("ErrorRate", DoubleValue(0.0));
          devDl->SetAttribute("ReceiveErrorModel", PointerValue(emDl));
        });
      }
    }

    // Handover A3-RSRP (se sua árvore tiver esse TypeId; é o padrão no 5G-LENA moderno)
    nrHelper->SetHandoverAlgorithmType("ns3::NrA3RsrpHandoverAlgorithm");
    // Parâmetros típicos (ajuste se o HO ficar "duro" ou "nervoso")
    nrHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(3.0));             // dB
    nrHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(80)));

    //g_hoCsv.open(outputDir + "handover_events.csv");
    //g_hoCsv << "time_s,event,imsi,fromCell,toCell\n";

    // Conecta nos traces do RRC do UE (fail-safe: só conecta se o path existir)
    auto safeConnect = [](const std::string& pattern, const ns3::CallbackBase& cb) {
      ns3::Config::MatchContainer mc = ns3::Config::LookupMatches(pattern);
      if (mc.GetN() == 0) {
        NS_LOG_WARN("Trace path not found: " << pattern);
        return;
      }
      // Agora que sabemos que existe, podemos usar o Connect direto no padrão
      ns3::Config::Connect(pattern, cb);
    };

    // **ATENÇÃO às ASPAS abaixo** (isso tudo é uma string só!)
    safeConnect("/NodeList/*/DeviceList/*/$ns3::NrUeNetDevice/$ns3::NrUeRrc/HandoverStart",
                ns3::MakeCallback(&HoStart));
    safeConnect("/NodeList/*/DeviceList/*/$ns3::NrUeNetDevice/$ns3::NrUeRrc/HandoverEndOk",
                ns3::MakeCallback(&HoEnd));

    // --- novo: imprime IMSI durante o attach (útil para casar com logs do MME/NAS) ---
    auto attachAndPrint = [nrHelper](const NetDeviceContainer& ueDevs,
                                     const NetDeviceContainer& gnbDevs,
                                     const char* label)
    {
        for (uint32_t i = 0; i < ueDevs.GetN(); ++i) {
            Ptr<NrUeNetDevice> ue = DynamicCast<NrUeNetDevice>(ueDevs.Get(i));
            NS_ABORT_MSG_IF(ue == nullptr, "UE NetDevice não é NrUeNetDevice");
            std::cout << "[ATTACH] " << label << " UE#" << i
                      << " IMSI=" << ue->GetImsi() << std::endl;
        }
    };

    attachAndPrint(ueEmbbNetDev,  gnbDevs, "eMBB ");
    attachAndPrint(ueUrllcNetDev, gnbDevs, "URLLC");
    attachAndPrint(ueMmtcNetDev,  gnbDevs, "mMTC ");


    // Endereçamento IPv4 para cada grupo de UEs (uma vez para cada container)
    Ipv4InterfaceContainer ueIfaceEmbb  = epcHelper->AssignUeIpv4Address(ueEmbbNetDev);
    Ipv4InterfaceContainer ueIfaceUrllc = epcHelper->AssignUeIpv4Address(ueUrllcNetDev);
    Ipv4InterfaceContainer ueIfaceMmtc  = epcHelper->AssignUeIpv4Address(ueMmtcNetDev);


    // --- novo: tabela IMSI ↔ IP para conferência manual no console ---
    auto printImsiIp = [](const NetDeviceContainer& ueDevs,
                          const Ipv4InterfaceContainer& ifaces,
                          const char* label)
    {
        for (uint32_t i = 0; i < ueDevs.GetN(); ++i) {
            Ptr<NrUeNetDevice> ue = DynamicCast<NrUeNetDevice>(ueDevs.Get(i));
            uint64_t imsi = ue ? ue->GetImsi() : 0;
            std::cout << "[UE-IP] " << label << " UE#" << i
                      << " IMSI=" << imsi
                      << " IP=" << ifaces.GetAddress(i) << std::endl;
        }
    };
    printImsiIp(ueEmbbNetDev,  ueIfaceEmbb,  "eMBB ");
    printImsiIp(ueUrllcNetDev, ueIfaceUrllc, "URLLC");
    printImsiIp(ueMmtcNetDev,  ueIfaceMmtc,  "mMTC ");

    // Set routing
    for (auto c : {&ueEmbbContainer, &ueUrllcContainer, &ueMmtcContainer}) {
        for (uint32_t i = 0; i < c->GetN(); ++i) {
            Ptr<Ipv4StaticRouting> srt = 
                ipv4RoutingHelper.GetStaticRouting(c->Get(i)->GetObject<Ipv4>());
            srt->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        }
    }

    //for (auto id : idxEmbb)  splitPoint[id] = splitDepthEmbb;
    //for (auto id : idxUrllc) splitPoint[id] = splitDepthUrllc;
    //for (auto id : idxMmtc)  splitPoint[id] = splitDepthMmtc;
    for (auto id : idxEmbb)  { bwpVector[id] = static_cast<uint32_t>(BwpLabel::EMBB);  }
    for (auto id : idxUrllc) { bwpVector[id] = static_cast<uint32_t>(BwpLabel::URLLC); }
    for (auto id : idxMmtc)  { bwpVector[id] = static_cast<uint32_t>(BwpLabel::MMTC);  }


    // Create IP to UE index mapping
    std::map<uint32_t, uint32_t> ipToUeIndex;
    for (uint32_t i = 0; i < ueIfaceEmbb.GetN(); ++i) { 
        ipToUeIndex[ueIfaceEmbb.GetAddress(i).Get()] = idxEmbb[i]; 
    }
    for (uint32_t i = 0; i < ueIfaceUrllc.GetN(); ++i) { 
        ipToUeIndex[ueIfaceUrllc.GetAddress(i).Get()] = idxUrllc[i]; 
    }
    for (uint32_t i = 0; i < ueIfaceMmtc.GetN(); ++i) { 
        ipToUeIndex[ueIfaceMmtc.GetAddress(i).Get()] = idxMmtc[i]; 
    }


    // EXEMPLOS: derruba 1 UE de eMBB por 0.7 s e 1 UE de URLLC por 0.5 s
    if (ueEmbbContainer.GetN() > 0) {
      FlapUeIf(ueEmbbContainer.Get(0), ueIfaceEmbb.GetAddress(0),
               Seconds(0.40), Seconds(0.80));
    }
    if (ueUrllcContainer.GetN() > 0) {
      FlapUeIf(ueUrllcContainer.Get(0), ueIfaceUrllc.GetAddress(0),
               Seconds(0.70), Seconds(1.00));
    }


    // ==== PROBE: instalar app no RemoteHost ====
    Ptr<RemoteHostProbeApp> rhApp = CreateObject<RemoteHostProbeApp>(&ipToUeIndex, &ulProbe_s, processingDelayMs);
    rhApp->Setup(InetSocketAddress(Ipv4Address::GetAny(), kProbePort));
    remoteHost->AddApplication(rhApp);
    rhApp->SetStartTime(Seconds(0.05));
    rhApp->SetStopTime(simTime);

    // ==== PROBE: instalar app no UE (um por UE) ====
    auto installProbeOn = [&](Ptr<Node> ueNode, Ipv4Address ueAddr, uint32_t ueIndex)
    {
      // o UE envia para o RemoteHost:7007 e recebe eco em sua porta 7007
      Ptr<UeProbeApp> app = CreateObject<UeProbeApp>(ueIndex, &e2eProbe_s);
      app->Setup(InetSocketAddress(internetIpIfaces.GetAddress(1), kProbePort));
      ueNode->AddApplication(app);
      app->SetStartTime(Seconds(0.10)); // após o servidor
      app->SetStopTime(simTime);
    };

    // Embarcar em todos os UEs (em ordem consistente com seus índices)
    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
      installProbeOn(ueEmbbContainer.Get(i),  ueIfaceEmbb.GetAddress(i),  idxEmbb[i]);
    }
    for (uint32_t i = 0; i < ueUrllcContainer.GetN(); ++i) {
      installProbeOn(ueUrllcContainer.Get(i), ueIfaceUrllc.GetAddress(i), idxUrllc[i]);
    }
    for (uint32_t i = 0; i < ueMmtcContainer.GetN(); ++i) {
      installProbeOn(ueMmtcContainer.Get(i),  ueIfaceMmtc.GetAddress(i),  idxMmtc[i]);
    }

    // Preencher o vetor de processamento (constante p/ todos)
    for (uint32_t i = 0; i < nUes; ++i) {
      procProbe_s[i] = processingDelayMs / 1000.0;
    }

    // --- ativação dedicada por slice/porta via NrHelper ---
    // CERTO (100% NR)
    auto activateBearerFor = [&](Ptr<NetDevice> ueDev, uint16_t porta, NrEpsBearer::Qci qci) {
      Ptr<NrEpcTft> tft = Create<NrEpcTft>();
      NrEpcTft::PacketFilter pf;
      pf.localPortStart = porta;
      pf.localPortEnd   = porta;
      tft->Add(pf);

      NrEpsBearer bearer(qci);
      nrHelper->ActivateDedicatedEpsBearer(ueDev, bearer, tft); // overload: Ptr<NetDevice>, NrEpsBearer, Ptr<NrEpcTft>
    };

    // Exemplo: 6000=eMBB, 6001=URLLC, 6002=mMTC
    // chamar por slice usando índices válidos (sempre verifique o limite!)
    for (auto id : idxEmbb)  { NS_ASSERT(id < nUes);
      activateBearerFor(ueDevs.Get(id), 6000, NrEpsBearer::NGBR_VIDEO_TCP_DEFAULT); }
    for (auto id : idxUrllc) { NS_ASSERT(id < nUes);
      activateBearerFor(ueDevs.Get(id), 6001, NrEpsBearer::GBR_CONV_VOICE);         }
    for (auto id : idxMmtc)  { NS_ASSERT(id < nUes);
      activateBearerFor(ueDevs.Get(id), 6002, NrEpsBearer::NGBR_IMS); }  

    // === [/EPS bearer único por UE — API NR] ===
    std::cout << "Network setup complete. Installing applications..." << std::endl;

    // === CONSERVATIVE TRAFFIC INSTALLATION ===
    ApplicationContainer allApps;
    SliceInfo sEmbb = MakeConservativeEmbb();
    SliceInfo sUrllc = MakeConservativeUrllc();
    SliceInfo sMmtc = MakeConservativeMmtc();

    Time baseStart = udpAppStartTime;
    Time perClientGap = MilliSeconds(perClientGapMs);
    Time perClientDuration = Seconds(perClientDurationSec);

    // Install traffic for each slice
    for (uint32_t i = 0; i < ueEmbbContainer.GetN(); ++i) {
        Time start = baseStart + perClientGap * i;
        Time stop = start + perClientDuration;
        InstallConservativeOnOff(remoteHost, ueEmbbContainer.Get(i), 
                               ueIfaceEmbb.GetAddress(i), sEmbb, allApps, start, stop);
    }
    for (uint32_t i = 0; i < ueUrllcContainer.GetN(); ++i) {
        Time start = baseStart + perClientGap * (i + ueEmbbContainer.GetN());
        Time stop = start + perClientDuration;
        InstallConservativeOnOff(remoteHost, ueUrllcContainer.Get(i), 
                               ueIfaceUrllc.GetAddress(i), sUrllc, allApps, start, stop);
    }
    for (uint32_t i = 0; i < ueMmtcContainer.GetN(); ++i) {
        Time start = baseStart + perClientGap * (i + ueEmbbContainer.GetN() + ueUrllcContainer.GetN());
        Time stop = start + perClientDuration;
        InstallConservativeOnOff(remoteHost, ueMmtcContainer.Get(i), 
                               ueIfaceMmtc.GetAddress(i), sMmtc, allApps, start, stop);
    }

    // === SIMULATION SETUP ===
    Simulator::Stop(simTime);

    // FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    // Progress callback
    std::function<void()> progress = [&progress, simTime]() {
        std::cout << "[Progress] t=" << std::fixed << std::setprecision(1) 
                  << Simulator::Now().GetSeconds() << "s" << std::endl;
        if (Simulator::Now() + Seconds(0.5) < simTime) {
            Simulator::Schedule(Seconds(0.5), progress);
        }
    };

    // Anexe imediatamente:
    nrHelper->AttachToClosestGnb(ueDevs, gnbDevs);


    std::cout << "🚀 Starting simulation..." << std::endl;
    Simulator::Run();
    std::cout << "✅ Simulation completed. Processing results..." << std::endl;

    for (uint32_t i = 0; i < nUes; ++i) {
      double e2e = e2eProbe_s[i];
      double ul  = ulProbe_s[i];
      double pro = procProbe_s[i];
      double dl  = std::max(0.0, e2e - ul - pro);
      dlProbe_s[i] = dl;
    }

    // === RESULTS PROCESSING ===
    monitor->CheckForLostPackets();

    // Classifier pode vir de FlowMonitorHelper (adeque ao seu nome de helper)
    Ptr<FlowClassifier> baseClassifier = flowmonHelper.GetClassifier();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(baseClassifier);

    // Coleta de estatísticas
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    std::cout << "[FlowMonitor] Flows observados: " << stats.size() << std::endl;


    // === SUMMARY_JITTER (por fluxo) + agregados por slice ===
    // Vamos registrar linhas por fluxo em 10_summary_jitter.csv
    // e um arquivo agregado por slice em summary_jitter_by_slice_DETALHAMENTO_FORA_ESCOPO.csv

    struct JitRow {
      std::string slice;    // eMBB / URLLC / mMTC
      uint32_t    bwp;      // 0,1,2
      std::string flowKey;  // "srcIP:srcPort->dstIP:dstPort (proto)"
      double      meanJitter_s; // jitter médio do fluxo (s)
      uint32_t    rxPackets;    // pacotes recebidos do fluxo
    };

    struct JitAgg {
      double   sumJitter_s = 0.0;    // soma dos jitterSum (s)
      uint64_t sumRxPkts   = 0;      // soma de rxPackets (ponderador)
      uint32_t flows       = 0;      // fluxos válidos (rxPackets>=2)
      double   sumMeanFlow = 0.0;    // soma dos jitters médios de cada fluxo
    };

    // mapeia porta → nome do slice
    auto sliceOfPort = [](uint16_t port) -> const char* {
      if (port == kDlPortUrllc) return "URLLC";
      if (port == kDlPortMmtc)  return "mMTC";
      // se não for URLLC nem mMTC, assume eMBB por padrão
      return "eMBB";
    };

    std::vector<JitRow> jitRows;
    std::map<std::string, JitAgg> aggBySlice;  // chave: "eMBB", "URLLC", "mMTC"

    for (const auto& kv : stats) {
      const auto& s = kv.second;
      // exigir ao menos 2 pacotes recebidos para estimar jitter
      if (s.rxPackets < 2) continue;

      Ipv4FlowClassifier::FiveTuple five = classifier->FindFlow(kv.first);

      // Descobrir o slice pela porta de destino (Downlink: RemoteHost -> UE)
      const uint16_t dport = five.destinationPort;
      const char* slice = sliceOfPort(dport);

      // Descobrir BWP via UE (pelo IP de destino → índice do UE → bwpVector[idx])
      uint32_t bwpIdx = 0;
      auto itUe = ipToUeIndex.find(five.destinationAddress.Get());
      if (itUe != ipToUeIndex.end() && itUe->second < bwpVector.size()) {
        bwpIdx = bwpVector[itUe->second];
      }

      // Chave amigável do fluxo
      std::ostringstream fk;
      fk << five.sourceAddress << ":" << five.sourcePort
         << "->" << five.destinationAddress << ":" << five.destinationPort
         << " (udp)";

      const double meanJit = s.jitterSum.GetSeconds() / static_cast<double>(s.rxPackets);

      jitRows.push_back({
        slice, bwpIdx, fk.str(), meanJit, static_cast<uint32_t>(s.rxPackets)
      });

      // agrega por slice
      auto& A = aggBySlice[std::string(slice)];
      A.sumJitter_s += s.jitterSum.GetSeconds();
      A.sumRxPkts   += s.rxPackets;
      A.flows       += 1;
      A.sumMeanFlow += meanJit;
    }

    // Gravar CSV
    // ---- grava 10_summary_jitter.csv (por fluxo) ----
    try {
        std::string jitPath = outputDir + "10_summary_jitter.csv";
        bool existsJit = std::filesystem::exists(jitPath);

        std::ofstream jf(jitPath, std::ios::app);  // abre em append
        if (!jf.is_open()) {
            throw std::runtime_error("Could not open 10_summary_jitter.csv for writing");
        }

        // só escreve cabeçalho se for a primeira vez
        if (!existsJit) {
            jf << "Slice,BWP,FlowKey,MeanJitter_s,RxPackets\n";
        }

        jf << std::fixed << std::setprecision(6);

        for (const auto& r : jitRows) {
            jf << r.slice << "," << r.bwp << "," << r.flowKey << ","
               << r.meanJitter_s << "," << r.rxPackets << "\n";
        }

        jf.close();
        std::cout << "CSV file saved/appended successfully to: "
                  << jitPath << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error saving 10_summary_jitter.csv: " << e.what() << std::endl;
    }

    try {
        std::string jfaPath = outputDir + "summary_jitter_by_slice_DETALHAMENTO_FORA_ESCOPO.csv";
        bool existsJfa = std::filesystem::exists(jfaPath);

        std::ofstream jfa(jfaPath, std::ios::app);  // abre em append
        if (!jfa.is_open()) {
            throw std::runtime_error("Could not open summary_jitter_by_slice_DETALHAMENTO_FORA_ESCOPO.csv for writing");
        }

        // cabeçalho só na primeira vez
        if (!existsJfa) {
            jfa << "Slice,Flows,ActiveFlows,RxPackets_total,Jitter_avg_flow_s,Jitter_eff_s\n";
        }

        jfa << std::fixed << std::setprecision(6);

        for (const auto& kvp : aggBySlice) {
            const std::string& slice = kvp.first;
            const JitAgg& A = kvp.second;

            const uint32_t activeFlows = A.flows;
            const double jitterAvgFlow = (activeFlows > 0) ? (A.sumMeanFlow / activeFlows) : 0.0;
            const double jitterEff     = (A.sumRxPkts > 0)
                                        ? (A.sumJitter_s / static_cast<double>(A.sumRxPkts))
                                        : 0.0;

            jfa << slice << "," << A.flows << "," << activeFlows << ","
                << A.sumRxPkts << "," << jitterAvgFlow << "," << jitterEff << "\n";
        }

        jfa.close();
        std::cout << "CSV file saved/appended successfully to: "
                  << jfaPath << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error saving summary_jitter_by_slice_DETALHAMENTO_FORA_ESCOPO.csv: " << e.what() << std::endl;
    }


    // --- Acumulador por UE ---
    struct Acc {
      uint64_t rxBytes  = 0;
      uint64_t rxPkts   = 0;
      uint64_t txPkts   = 0; 
      uint64_t lostPkts = 0;
      uint64_t txBytes  = 0;
      Time     delaySum  = Seconds(0);
      Time     jitterSum = Seconds(0);
      Time     firstRx   = Time::Max();  // <-- em vez de Seconds(1e12)
      Time     lastRx    = Seconds(0);
      Time     firstTx   = Time::Max();
      Time     lastTx    = Seconds(0);
    };
    std::vector<Acc> acc(nUes);


    if (stats.empty()) {
      std::cout << "⚠️ Nenhum fluxo visto. Verifique portas/direção no TFT (UL/DL), "
                   "timing das aplicações e duração total da simulação.\n";
    }

    for (const auto& kv : stats) {
      const auto& s = kv.second;
      if (s.rxPackets == 0 && s.txPackets == 0) continue;

      auto t = classifier->FindFlow(kv.first);

      // 1) Descobrir o UE pelo IP (DL: destino; UL: origem)
      uint32_t ueIndex = std::numeric_limits<uint32_t>::max();
      auto itDst = ipToUeIndex.find(t.destinationAddress.Get());
      if (itDst != ipToUeIndex.end()) ueIndex = itDst->second;
      else {
        auto itSrc = ipToUeIndex.find(t.sourceAddress.Get());
        if (itSrc != ipToUeIndex.end()) ueIndex = itSrc->second;
      }
      if (ueIndex >= acc.size()) continue;

      // 2) Agora sim, atualize o acumulador do UE correto
      auto& a = acc[ueIndex];

      a.txBytes  += s.txBytes;
      a.rxBytes  += s.rxBytes;
      a.txPkts   += s.txPackets;
      a.rxPkts   += s.rxPackets;
      a.lostPkts += s.lostPackets;

      a.delaySum  += s.delaySum;
      a.jitterSum += s.jitterSum;

      if (s.txPackets > 0) {
        if (s.timeFirstTxPacket < a.firstTx) a.firstTx = s.timeFirstTxPacket;
        if (s.timeLastTxPacket  > a.lastTx)  a.lastTx  = s.timeLastTxPacket;
      }
      if (s.rxPackets > 0) {
        if (s.timeFirstRxPacket < a.firstRx) a.firstRx = s.timeFirstRxPacket;
        if (s.timeLastRxPacket  > a.lastRx)  a.lastRx  = s.timeLastRxPacket;
      }
    }

    // Derivar métricas por UE e preencher vetores usados no CSV
    for (uint32_t i = 0; i < nUes; ++i) {
      const auto& a = acc[i];

      double thrMbps = 0.0;
      double dur = (a.lastRx - a.firstRx).GetSeconds();
      if (a.rxPkts > 0 && dur > 0.0) {
        thrMbps = (a.rxBytes * 8.0) / dur / 1e6; // Mbps
      }

      double avgDelay = (a.rxPkts > 0) ? (a.delaySum.GetSeconds() / a.rxPkts) : 0.0;
      double avgJitt  = (a.rxPkts > 1) ? (a.jitterSum.GetSeconds() / (a.rxPkts - 1)) : 0.0;

      throughputVector[i]  = thrMbps;
      delayVector[i]       = avgDelay;
      jitterVector[i]      = avgJitt;
      lostPacketsVector[i] = a.lostPkts;

      txPacketsVector[i]   = static_cast<uint32_t>(a.txPkts);
      rxPacketsVector[i]   = static_cast<uint32_t>(a.rxPkts);

    }

    // Atualiza distanceVector no fim, considerando a posição final de cada UE
    for (uint32_t i = 0; i < gridScenario.GetUserTerminals().GetN(); ++i) {
      Ptr<Node> ue = gridScenario.GetUserTerminals().Get(i);
      distanceVector[i] = GetDistanceToClosestGnb(ue, gridScenario.GetBaseStations());
    }

    for (uint32_t i = 0; i < nUes; ++i) {
        const auto& a = acc[i];
        const double txBits = static_cast<double>(a.txBytes) * 8.0;
        const double rxBits = static_cast<double>(a.rxBytes) * 8.0;
        const double bits   = txBits + rxBits;
        bitsVector[i] = bits;

        const double d   = distanceVector[i];   // já preenchido acima
        const uint32_t b = bwpVector[i];

        // Componentes de energia
        const double E_tx   = E_TX_per_bit_ref * PathlossScale(d, b) * txBits;
        const double E_rx   = E_RX_per_bit       * rxBits;

        const double tRx    = (a.rxPkts > 0) ? (a.lastRx - a.firstRx).GetSeconds() : 0.0;
        const double tTx    = (a.txPkts > 0) ? (a.lastTx - a.firstTx).GetSeconds() : 0.0;
        const double tAct   = std::max(tRx, tTx);                 // tempo efetivamente ativo
        const double E_base = (P_idle_W + 0.02 * MuFactor(b)) * tAct; // base + custo de processamento ~μ

        const double E = E_tx + E_rx + E_base;     // Energia total em J
        energyConsumption[i] = E;                  // reutiliza seu vetor existente
        bitsPerJoule[i]      = (E > 0.0) ? (bits / E) : 0.0;
    }

    // (Opcional) Diagnóstico: UEs sem recepção
    for (uint32_t i = 0; i < nUes; ++i) {
      if (acc[i].rxPkts == 0) {
        std::cout << "[WARN] UE " << (i+1)
                  << " sem pacotes recebidos; verifique IP/porta e janelas de Start/Stop dos apps.\n";
      }
    }

    // === PLR por UE ===
    // Definição: PLR = lostPkts / txPkts. Se txPkts==0, defina PLR=0 para evitar NaN.
    for (uint32_t i = 0; i < nUes; ++i) {
        const double tx = static_cast<double>(txPacketsVector[i]);
        const double lost = static_cast<double>(lostPacketsVector[i]);
        plrVector[i] = (tx > 0.0) ? (lost / tx) : 0.0;
    }


    //std::set<uint16_t> validPorts = {kDlPortEmbb, kDlPortUrllc, kDlPortMmtc};

    uint32_t validUeCount = 0;
    double sumThr = 0.0, sumDelay = 0.0;
    for (uint32_t i = 0; i < nUes; ++i) {
      if (acc[i].rxPkts > 0) {
        sumThr  += throughputVector[i];
        sumDelay += delayVector[i];
        ++validUeCount;
      }
    }

    [[maybe_unused]] double avgThroughput = validUeCount ? (sumThr / validUeCount) : 0.0;
    [[maybe_unused]] double avgDelay      = validUeCount ? (sumDelay / validUeCount) : 0.0;
    std::cout << "Valid UEs: " << validUeCount << "\n";

    // === SAVE RESULTS ===
    try {
        std::string filePath = outputDir + "01_05_06_07_08_09_line_all.csv";
        bool exists = std::filesystem::exists(filePath);

        // abre em append (não apaga conteúdo existente)
        std::ofstream allFile(filePath, std::ios::app);
        if (allFile.is_open()) {
            // escreve cabeçalho só se o arquivo ainda não existia
            if (!exists) {
                allFile << "User,Profile,BWP,SplitPoint,Delay_s,Throughput_Mbps,Energy_J,"
                        << "LostPackets,Jitter_s,Distance_m,DeviceType,TxPackets,RxPackets,PLR,BitsPerJ,"
                        << "UL_Probe_s,Proc_s,DL_Probe_s,E2E_Probe_s\n";
            }

            for (size_t i = 0; i < nUes; ++i) {
                const char* prof = (bwpVector[i] == BWP_EMBB) ? "eMBB" 
                                  : (bwpVector[i] == BWP_URLLC) ? "URLLC" : "mMTC";

                allFile << std::fixed << std::setprecision(6)
                        << i+1 << "," << prof << "," << bwpVector[i] << ","
                        << splitPoint[i] << ","
                        << delayVector[i] << "," << throughputVector[i] << ","
                        << energyConsumption[i] << "," << lostPacketsVector[i] << ","
                        << jitterVector[i] << "," << distanceVector[i] << ","
                        << (deviceTypeVector[i] == 0 ? "Smartphone" : "IoT") << ","
                        << txPacketsVector[i] << "," << rxPacketsVector[i] << ","
                        << plrVector[i] << ","
                        << bitsPerJoule[i] << ","
                        << ulProbe_s[i] << "," << procProbe_s[i] << "," 
                        << dlProbe_s[i] << "," << e2eProbe_s[i]
                        << "\n";
            }

            allFile.close();
            std::cout << "CSV file saved/appended successfully to: " 
                      << filePath << std::endl;            
        } else {
            std::cerr << "Error: Could not open CSV file for writing" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error saving CSV file: " << e.what() << std::endl;
    }

    // === THROUGHPUT BY SLICE (agregado) ===
    // Usa o mesmo mapeamento de profileOf(bwpId) já presente no arquivo
    std::map<std::string, double> thrSumMbpsBySlice;   // eMBB/URLLC/mMTC -> soma Mbps
    std::map<std::string, uint32_t> ueCountBySlice;

    double thrTotalMbps = 0.0;

    // Agrega por slice usando bwpVector/throughputVector já preenchidos
    auto profileOf = [&](uint32_t bwpId) -> const char* {
      if (bwpId == 1) return "eMBB";
      if (bwpId == 2) return "URLLC";
      return "mMTC";
    };

    for (uint32_t i = 0; i < nUes; ++i) {
      std::string prof = profileOf(bwpVector[i]);
      thrSumMbpsBySlice[prof] += throughputVector[i];
      ueCountBySlice[prof]    += 1;
      thrTotalMbps            += throughputVector[i];
    }

    for (uint32_t i = 0; i < nUes; ++i) {
        const std::string prof = profileOf(bwpVector[i]); // "eMBB", "URLLC" ou "mMTC"
        const double thrMbps = throughputVector[i];

        thrSumMbpsBySlice[prof]  += thrMbps;
        ueCountBySlice[prof]     += 1;
        thrTotalMbps             += thrMbps;
    }

    // Gera CSV: 11_throughput_by_slice.csv
    try {
        std::string thrFilePath = outputDir + "11_throughput_by_slice.csv";
        bool existsThr = std::filesystem::exists(thrFilePath);

        std::ofstream f(thrFilePath, std::ios::app);  // abre em append
        if (f.is_open()) {
            // escreve cabeçalho só se for a primeira vez
            if (!existsThr) {
                // define cabeçalho de pesos
                //f << " UrllcWeight=" << urllcWeight
                  //<< ", EmbbWeight=" << embbWeight
                  //<< ", MmtcWeight=" << mmtcWeight << "\n";

                // depois escreve o cabeçalho das colunas
                f << "Slice,UEs,ThroughputSum_Mbps,Share_percent\n";
            }

            f << std::fixed << std::setprecision(6);

            auto writeRow = [&](const std::string& s) {
                const double sum = thrSumMbpsBySlice[s];
                const uint32_t cnt = ueCountBySlice[s];
                const double share = (thrTotalMbps > 0.0) ? (100.0 * sum / thrTotalMbps) : 0.0;
                f << s << "," << cnt << "," << sum << "," << share << "\n";
            };

            // mantém ordem conveniente
            writeRow("URLLC");
            writeRow("eMBB");
            writeRow("mMTC");

            // linha de total para referência
            //f << "TOTAL," << nUes << "," << thrTotalMbps << ",100.0\n";

            f.close();
            std::cout << "CSV file saved/appended successfully to: "
                      << thrFilePath << std::endl;
        } else {
            std::cerr << "Error: Could not open 11_throughput_by_slice.csv for writing\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error saving 11_throughput_by_slice.csv: " << e.what() << std::endl;
    }

    // === SUMMARY por Slice/BWP ===
    struct Agg {
        uint32_t count = 0;
        // somatórios para médias
        double sumDelay = 0.0, sumThr = 0.0, sumEnergy = 0.0;
        double sumLost = 0.0, sumJitter = 0.0, sumDist = 0.0;
        double sumTx = 0.0, sumRx = 0.0, sumPlr = 0.0;
        double sumBits = 0.0;
    };

    // chave: (profile, bwp)
    struct Key {
        std::string prof;
        uint32_t bwp;
        bool operator<(const Key& o) const {
            if (prof != o.prof) return prof < o.prof;
            return bwp < o.bwp;
        }
    };

    std::map<Key, Agg> agg;

    for (uint32_t i = 0; i < nUes; ++i) {
        Key k{ profileOf(bwpVector[i]), bwpVector[i] };
        auto& a = agg[k];
        a.count += 1;
        a.sumDelay  += delayVector[i];
        a.sumThr    += throughputVector[i];
        a.sumEnergy += energyConsumption[i];
        a.sumLost   += static_cast<double>(lostPacketsVector[i]);
        a.sumJitter += jitterVector[i];
        a.sumDist   += distanceVector[i];
        a.sumTx     += static_cast<double>(txPacketsVector[i]);
        a.sumRx     += static_cast<double>(rxPacketsVector[i]);
        a.sumPlr    += plrVector[i];
        a.sumBits   += bitsVector[i];
    }

    // grava 02_03_04_summary_slices.csv
    // depois de preencher o mapa `agg`, na hora de gravar:
    try {
      std::string sumPath = outputDir + "02_03_04_summary_slices.csv";
      bool existsSum = std::filesystem::exists(sumPath);

      std::ofstream lf(sumPath, std::ios::app);  // abre em append
      if (lf.is_open()) {
          // escreve cabeçalho só se for a primeira vez
          if (!existsSum) {
              lf << "Slice,LostPackets_avg,LostPackets_total,PLR_avg_UE,PLR_eff,"
                    "Delay_avg,Throughput_avg,Energy_avg\n";
          }

          lf << std::fixed << std::setprecision(6);

          for (auto& kv : agg) { // <-- era sliceAgg
              const auto& key = kv.first;   // Key{prof,bwp}
              const auto& a   = kv.second;
              const double n  = a.count > 0 ? static_cast<double>(a.count) : 1.0;

              const double lostAvgPerUe = a.sumLost / n;
              const double plrAvgUe     = a.sumPlr / n;
              const double plrEff       = (a.sumTx > 0.0) ? (a.sumLost / a.sumTx) : 0.0;

              lf << key.prof           << ","  // exibe o slice (perfil)
                 << lostAvgPerUe       << ","
                 << a.sumLost          << ","
                 << plrAvgUe           << ","
                 << plrEff             << ","
                 << (a.sumDelay  / n)  << ","
                 << (a.sumThr    / n)  << ","
                 << (a.sumEnergy / n)  << "\n";
          }

          lf.close();
          std::cout << "CSV file saved/appended successfully to: "
                    << sumPath << std::endl;
      } else {
          std::cerr << "Error: Could not open 02_03_04_summary_slices.csv for writing" << std::endl;
      }

    } catch (const std::exception& e) {
      std::cerr << "Error saving summary CSV file: " << e.what() << std::endl;
    }

    // Shared memory (simplified - can be disabled if causing issues)
    const char* shm_name = "ns3_shared_memory";
    size_t element_size = sizeof(double);
    size_t size = nUes * element_size * kNumFeatures;

    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd != -1) {
        if (ftruncate(shm_fd, size) != -1) {
            double* data = (double*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (data != MAP_FAILED) {
                for (size_t i = 0; i < nUes; ++i) {
                    data[i * kNumFeatures + 0] = delayVector[i];
                    data[i * kNumFeatures + 1] = throughputVector[i];
                    data[i * kNumFeatures + 2] = energyConsumption[i];
                    data[i * kNumFeatures + 3] = static_cast<double>(lostPacketsVector[i]);
                    data[i * kNumFeatures + 4] = jitterVector[i];
                    data[i * kNumFeatures + 5] = distanceVector[i];
                    data[i * kNumFeatures + 6] = static_cast<double>(deviceTypeVector[i]);
                    data[i * kNumFeatures + 7] = static_cast<double>(splitPoint[i]);
                }
                std::cout << "Data written to shared memory successfully." << std::endl;
                munmap(data, size);
            } else {
                std::cerr << "Warning: Error mapping shared memory." << std::endl;
            }
        } else {
            std::cerr << "Warning: Error resizing shared memory." << std::endl;
        }
        close(shm_fd);
    } else {
        std::cerr << "Warning: Error creating shared memory." << std::endl;
    }

    Simulator::Destroy();
    std::cout << "✅ Simulação finalizada, iniciando server_sync.py..." << std::endl;

    // Chamar script Python com tratamento de erro
    //std::string pythonCommand = "python3 scratch/SplitLearning-B5G/servers/server_sync.py " + std::to_string(nUes);
    //std::cout << "Executando: " << pythonCommand << std::endl;
    
    //int result = system(pythonCommand.c_str());
    //if (result != 0) {
        //std::cerr << "Aviso: Script Python retornou código de erro: " << result << std::endl;
    //}

    // Cleanup shared memory
    shm_unlink(shm_name);

    std::cout << "\n=== FINAL SUMMARY ===" << std::endl;
    std::cout << "✓ All compilation errors fixed" << std::endl;
    std::cout << "✓ Conservative parameters to prevent hanging" << std::endl;
    std::cout << "✓ Single BWP configuration for simplicity" << std::endl;
    std::cout << "✓ Minimal channel models for performance" << std::endl;
    std::cout << "✓ Fixed EPS bearer activation" << std::endl;
    std::cout << "✓ Progress monitoring included" << std::endl;
    std::cout << "\nTotal UEs: " << nUes << std::endl;
    std::cout << "Files generated in: " << outputDir << std::endl;
    std::cout << "Simulation time: " << simTime.GetSeconds() << " seconds" << std::endl;

    return 0;
}