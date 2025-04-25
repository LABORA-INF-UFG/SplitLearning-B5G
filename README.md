# SplitLearning-B5G

<p align='center' style="margin-bottom: -4px">Cleyber B. dos Reis<sup>1</sup>, Maria do Rosário C. Ribeiro <sup>1</sup>, Antonio Oliveira-JR<sup>1</sup></p>
<p align='center'style="margin-bottom: -4px"><sup>1</sup>Universidade Federal de Goiás - Goiânia, GO - Brazil</p>
<p align='center' style="margin-bottom: -4px">cleyber.bezerra@discente.ufg.br, rosario.ribeiro@ufg.br, antonio@inf.ufg.br</p>

# Description
The repository contains all the development related to the master's dissertation entitled "Split Learning as an enabler of wireless networks for future generations". Being developed through the Academic Master's Course in Computer Science - Goiânia, by the Institute of Informatics (INF) of the Federal University of Goiás (UFG).

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

Split Learning (SL) is an innovative and effective approach to address security and privacy concerns in the training of deep neural networks (DNN). This technique combines the protection of raw data with the division of the model between client devices and a central server, significantly reducing the risks of data leaks and cyberattacks. In addition, it enables the training of deep neural networks on devices with limited computational resources.

However, splitting the model leads to an increase in the communication flow between the distributed devices and the central server, which can not only generate a communication overhead in environments with computational constraints, but also negatively impact the training accuracy, compromising the final results of the model.

This paper covers the inference problem of improving accuracy. Through a case study of integrating (ns3-ai) with distributed Split Learning to train a Convolutional Neural Network (CNN) and MNIST dataset. The NS3-LENA simulator used the characteristics of a B5G network environment with mobile devices (UE) and a gNB (5G access module).

In this integrated scenario, network experiments were simulated with distance variations between 200 mt. With powers of 13 and 26 dBm and loss exponents of 2, 3 and 4 dB. Based on the network output results, with regard to latency, a policy was defined that values above 10 seconds are considered timeouts and are not included in machine learning experiments. With the objective of training and testing the split learning model, the impacts of changes in the network simulation on training accuracy were observed.


[Back to TOC](#table-of-contents)

## Baselines

The methods defined as baselines for our proposal are: (1) the use of the synchronous algorithm in the training of the Split Learning Vanilla model and (2) the definition of the training based on events provided by the simulation in NS3, following the latency policy established in 15 distinct seeds for each exponent. In the simulation, six (06) mobile devices and the base station are considered, with the policy defined for network latency stipulating nodes with latency below 23 seconds.

[Back to TOC](#table-of-contents)

## Results
### Results in the communication network environment.

Demonstrations of results within the scope of network simulation, in two different scenarios, graphically presenting: latencies, througput, packet loss rates and energy consumption.

<div style="display: flex; justify-content: center; align-items: center;">
    <div style="text-align: center; margin-right: 20px;">
        <img src="/images/graficoAtrasos.png" width="550">
        <figcaption>Fig. 1. Latency.</figcaption>
    </div>
    <div style="text-align: center;">
        <img src="/images/graficoVazao.png" width="550">
        <figcaption>Fig. 2. Throughput.</figcaption>
    </div>
</div>


Figures 1 and 2 show the latency and throughput results in the network simulation.


<div style="display: flex; justify-content: center; align-items: center;">
    <div style="text-align: center; margin-right: 20px;">
        <img src="/images/graficoPerda.png" width="550">
        <figcaption>Fig. 3. Packet Loss.</figcaption>
    </div>
    <div style="text-align: center;">
        <img src="/images/graficoEnergia.png" width="550">
        <figcaption>Fig. 4. Energy Consumed .</figcaption>
    </div>
</div>

Figures 3 and 4 show the results of packet loss and energy consumption in the network simulation.

<div style="text-align: center; margin-right: 20px;">
    <img src="/images/graficoAcuracia.png" width="550">
    <figcaption>Fig. 5. Accuracy.</figcaption>
</div>

Figure 5 shows the accuracy result in training.

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
- [ns-allinone (3.42)](https://www.nsnam.org/releases/ns-3-42/download/ ) or [ns-3-dev](https://gitlab.com/nsnam/ns-3-dev/ )
  
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
INSTALL THE NS-3-DEV
```bash
    git clone https://gitlab.com/nsnam/ns-3-dev.git
    git checkout -b ns-3.43-release ns-3.43
```
CONFIGURE AND COMPILE NS-3

In the ns-3-dev folder use the commands:
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

inside the ns-3-dev/scratch folder use the commands:
```bash
git clone https://github.com/cleyber-bezerra/SplitLearning-B5G.git
```
[Back to TOC](#table-of-contents)

## Run Experiments

EXECUTE PROJECT

inside the Split Learning folder run the script file.
```bash
    ./simulator_ns3.sh
```

[Back to TOC](#table-of-contents)

