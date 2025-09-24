# SplitLearning-B5G

<p align='center' style="margin-bottom: -4px">Cleyber B. dos Reis<sup>1</sup>, Maria do Rosário C. Ribeiro <sup>1</sup>, Antonio Oliveira-JR<sup>1</sup></p>
<p align='center'style="margin-bottom: -4px"><sup>1</sup>Universidade Federal de Goiás - Goiânia, GO - Brazil</p>
<p align='center' style="margin-bottom: -4px">cleyber.bezerra@discente.ufg.br, rosario.ribeiro@ufg.br, antonio@inf.ufg.br</p>

The repository hosts the full development carried out for the master’s dissertation Split Learning as an enabler of wireless networks for future generations, undertaken within the Academic Master’s Programme in Computer Science at the Institute of Informatics (INF), Federal University of Goiás (UFG), Goiânia.

Beyond serving as the dissertation’s experimental foundation, the repository also contains the implementation of SplitLearning-ns3, the network-aware Split Learning architecture introduced in the article SLArch: A Network Metric-aware Split Learning Architecture for B5G/6G Mobile Networks. This framework bridges wireless communication metrics — latency, throughput, jitter, packet loss rate, and energy consumption — with distributed training processes. In doing so, it enables reproducible and extensible experimentation in B5G/6G scenarios, providing both a research tool and a reference point for future studies.

# Table of Contents
- [Article Summary](#getting-started)
	- [Abstract](#abstract)
	- [Baselines](#baselines)
	- [Results](#results)
- [Replicating the Experiment](#replicating-the-experiment)
	- [Requirements](#requirements)
	- [Preparing Environment](#preparing-environment)
 	- [Run Experiments](#run-experiments)


## Abstract
This paper introduces SplitLearning-ns3, a network-aware Split Learning (SL) architecture developed on top of ns3-ai for the NS-3 simulator (5G-LENA). More than a simple integration, SplitLearning-ns3 provides a reproducible and extensible framework that couples wireless communication metrics with distributed training processes. The experimental protocol subjects Convolutional Neural Network (CNN) training to realistic network dynamics—including latency, throughput, jitter, packet loss rate (PLR), and energy consumption—that characterise Beyond 5G (B5G) and 6G environments.

The results show that PLR is the dominant factor hindering convergence, whereas latency and throughput have a moderate influence, and jitter and energy consumption remain secondary though still measurable. Slice-level analysis further reveals that URLLC consistently ensures lower latency, while eMBB captures the largest share of throughput. These findings highlight the decisive importance of link reliability and energy awareness in evaluating the feasibility of SL for next-generation mobile networks, and motivate further research into resilient and energy-efficient learning at the wireless edge.

[Back to TOC](#table-of-contents)

## Baselines

In this study, the baselines are defined by a reproducible experimental protocol implemented in SplitLearning-ns3. A single CNN model (MNIST) is trained across client–server partitions, with network dynamics injected from ns-3/5G-LENA. The baseline simulation considers two gNBs and 102 UEs operating under heterogeneous slices (URLLC, eMBB, and mMTC), with numerologies µ = 4 and µ = 2, a 100 MHz bandwidth, On–Off traffic, and transmission powers of 26 dBm (gNBs) and 13 dBm (UEs).

Both network metrics (latency, jitter, throughput, PLR, and energy consumption) and ML outcomes (validation accuracy and training time) are assessed jointly. Within this baseline, packet loss rate (PLR) emerges as the dominant constraint, outweighing delay and throughput, which contribute meaningfully only once link reliability is ensured.

[Back to TOC](#table-of-contents)

## Results
### Communication network and Split Learning environment

The proposed framework integrates network simulation with distributed training, enabling a joint analysis of key metrics such as **latency, throughput, jitter, packet loss rate (PLR), energy consumption, and validation accuracy**. Results are presented for heterogeneous slices (URLLC, eMBB, and mMTC).

<div style="display: flex; justify-content: center; align-items: center;">
    <div style="text-align: center; margin-right: 20px;">
        <img src="/images/fig1.png" width="550">
        <figcaption>Fig. 1. Latency distribution by slice (URLLC, eMBB, mMTC).</figcaption>
    </div>
    <div style="text-align: center;">
        <img src="/images/fig2.png" width="550">
        <figcaption>Fig. 2. Throughput share and stability across slices.</figcaption>
    </div>
</div>

Figures 1 and 2 show that **URLLC consistently achieved the lowest latency** due to prioritised scheduling, while **eMBB dominated throughput allocation (≈79% of total capacity)**. Although mMTC contributed little in terms of throughput, it introduced traffic bursts that increased delay variability.

<div style="display: flex; justify-content: center; align-items: center;">
    <div style="text-align: center; margin-right: 20px;">
        <img src="/images/fig3.png" width="550">
        <figcaption>Fig. 3. Jitter behaviour under bursty mMTC traffic.</figcaption>
    </div>
    <div style="text-align: center;">
        <img src="/images/fig4.png" width="550">
        <figcaption>Fig. 4. Impact of Packet Loss Rate (PLR) on SL convergence.</figcaption>
    </div>
</div>

Figures 3 and 4 highlight that jitter alone had a limited impact, but became critical when combined with PLR. Even small losses (**PLR < 2%**) caused a sharp reduction in validation accuracy, confirming **reliability as a decisive factor** for convergence.

<div style="display: flex; justify-content: center; align-items: center;">
    <div style="text-align: center; margin-right: 20px;">
        <img src="/images/fig5.png" width="550">
        <figcaption>Fig. 5. Energy consumption across slices.</figcaption>
    </div>
    <div style="text-align: center;">
        <img src="/images/fig6.png" width="550">
        <figcaption>Fig. 6. Validation accuracy as a function of delay and PLR.</figcaption>
    </div>
</div>

Figures 5 and 6 demonstrate that **energy consumption remained stable across slices**, with only minor variations, while **validation accuracy was highly sensitive to network reliability**. PLR degraded accuracy by up to −70.1%, overshadowing the effects of delay (+57.6%) and throughput (+12.2%).  

**Key insight:** These results confirm that **link reliability is the cornerstone for ensuring stable Split Learning in B5G/6G environments**.

[Back to TOC](#table-of-contents)

# Replicating The Experiment

## Requirements

- GNU (>=8.0.0)
  
  command to know the version of KERNEL, GCC and GNU Binutils in the terminal
  ```bash
	cat /proc/version
  ```
- GCC (>=11.4.0)
  
  commands to know the GCC version in the terminal.
  ```bash
	gcc --version
	ls -l /usr/bin/gcc*
  ```
- CMAKE (>=3.24)
  
  command to know the version of CMAKE in the terminal.
  ```bash
	cmake --version
  ```
- python (>=3.11.5)
  
  commands to know the version of PYTHON in the terminal.
  ```bash
	python --version
	python3 --version
  ```
- [ns-allinone (3.45)](https://www.nsnam.org/releases/ns-3-45/download/ ) or [ns-3-dev](https://gitlab.com/nsnam/ns-3-dev/ )
  
[Back to TOC](#table-of-contents)

## Preparing Environment

INSTALL GCC AND MAKE
```bash
    sudo apt update
    sudo apt install build-essential
    sudo apt install gcc-10 g++-10
```

```bash
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100
```

```bash
    gcc --version
    make --version
```

INSTALl THE CMAKE

download the file in the desired version:
https://cmake.org/download/

extract it, access the folder and run the command:
```bash
    ./bootstrap && make && sudo make install
```

INSTALL THE GIT
```bash
    sudo apt-get install git -y
```
INSTALL THE NS-3.45
```bash
    git clone https://gitlab.com/nsnam/ns-3.45.git
    git checkout -b ns-3.45-release ns-3.45
```
CONFIGURE AND COMPILE NS-3

In the ns-3.45 folder use the commands:
```bash
    ./ns3 configure --enable-examples --enable-tests
    ./ns3 build
```

INSTALL THE NR-LENA
```bash
    sudo apt-get install libc6-dev
    sudo apt-get install sqlite sqlite3 libsqlite3-dev
    sudo apt-get install libeigen3-dev
```

```bash
    cd contrib
    git clone https://gitlab.com/cttc-lena/nr.git
    cd nr
    git checkout -b 5g-lena-v3.1.y origin/5g-lena-v3.1.y
```
INSTALL THE NS3-IA
```bash
    sudo apt install python3-pip
    
    pip install tensorflow==2.17.0
    pip install cloudpickle==1.2.0
    pip install pyzmq
    pip install protobuf==3.20.3
    pip install tensorflow-estimator==2.15.0
    pip install tensorboard==2.17.0

    pip install numpy==1.18.1
    pip install Keras==3.2.0
    pip install Keras-Applications==1.0.8
    pip install Keras-Preprocessing==1.1.2

    pip install matplotlib==3.3.2
    pip install psutil==5.7.2
```

```bash
    cd contrib/
    git clone https://github.com/hust-diangroup/ns3-ai.git

    cd ns3-ai/py_interface
    pip3 install . --user
```

ENABLE MODULES
```bash
    ./ns3 configure --enable-modules=nr,internet-apps,flow-monitor,config-store,buildings,applications,network,core,wifi,energy,spectrum,propagation,mobility,antenna
```

DOWNLOAD PROJECT

inside the ns-3.45/scratch folder use the commands:
```bash
git clone https://github.com/LABORA-INF-UFG/SplitLearning-B5G.git
```
[Back to TOC](#table-of-contents)

## Run Experiments

EXECUTE PROJECT

inside the Split Learning folder run the script file.
```bash
    ./simulator_ns3.sh
```

[Back to TOC](#table-of-contents)

