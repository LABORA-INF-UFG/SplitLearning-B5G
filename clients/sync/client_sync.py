#!/usr/bin/env python3
import gc
import csv
import os
import socket
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import torchvision
import torchvision.transforms as transforms
import time
import sys
import copy
import torch.optim as optim
from tqdm import tqdm
from torch.utils.data import DataLoader
from datetime import datetime


import warnings
warnings.simplefilter("ignore", category=FutureWarning)

# Adiciona o caminho da pasta onde o arquivo está localizado ml_model
file_path = '/home/cleyber/Documentos/ns-3.45/scratch/SplitLearning-B5G/models'
if file_path not in sys.path:
    sys.path.append(file_path)

import ml_model
import socket_fun as sf

# variáveis globais
DAM = b'ok!'    # dummy ACK
MODE = 0        # 0->train, 1->test

BATCH_SIZE = 256

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
print("device:", device)

# Argumento de linha de comando para cliente
if len(sys.argv) < 2:
    print("Uso: client_sync.py <client_id>")
    sys.exit(1)
client_id = int(sys.argv[1])
print(f"Cliente ID: {client_id}")

# libera GPU
torch.cuda.empty_cache()

# -------------------- conexão ----------------------
host = '127.0.0.1'
port = 19089
ADDR = (host, port)

# CONNECT
s = socket.socket()
s.connect(ADDR)

# Handshake
handshake_msg = f"READY:{client_id}"
s.sendall(handshake_msg.encode())
ack = s.recv(4)
if ack != DAM:
    raise RuntimeError("Handshake com servidor falhou.")


# MNIST
root = './datasets/mnist_data'
transform = transforms.Compose([
    transforms.ToTensor(),
    transforms.Normalize((0.1307,), (0.3081,))  # Normalização para MNIST
])

# download dataset
trainset = torchvision.datasets.MNIST(root=root, download=True, train=True, transform=transform)
testset = torchvision.datasets.MNIST(root=root, download=True, train=False, transform=transform)

print("trainset_len:", len(trainset))
print("testset_len:", len(testset))
image, label = trainset[0]
print("sample image size:", image.size())

trainloader = DataLoader(trainset, batch_size=BATCH_SIZE, shuffle=True, num_workers=0)
testloader = DataLoader(testset, batch_size=BATCH_SIZE, shuffle=False, num_workers=0)

mymodel1 = ml_model.ml_model_in().to(device)
mymodel2 = ml_model.ml_model_out(NUM_CLASSES=10).to(device)

# ------------------ treinamento -----------------
epochs = 1
lr = 0.005

criterion = nn.CrossEntropyLoss()
optimizer1 = optim.SGD(mymodel1.parameters(), lr=lr, momentum=0.9, weight_decay=5e-4)
optimizer2 = optim.SGD(mymodel2.parameters(), lr=lr, momentum=0.9, weight_decay=5e-4)

def train():
    train_acc_list, val_acc_list = [], []

    def forward_prop(MODEL, data):
        output = None
        if MODEL == 1:
            optimizer1.zero_grad()
            output_1 = mymodel1(data)
            output = output_1
        elif MODEL == 2:
            optimizer2.zero_grad()
            output_2 = mymodel2(data)
            output = output_2
        else:
            print("!!!!! MODEL not found !!!!!")
        return output

    for e in range(epochs):
        print("--------------- Epoch:", e + 1, "--------------")
        train_acc = 0
        val_acc = 0

        # ================= train mode ================
        mymodel1.train()
        mymodel2.train()
        MODE = 0    # train mode
        MODEL = 1   # train model
        for data, labels in tqdm(trainloader):
            sf.send_size_n_msg(MODE, s)  # send mode

            data = data.to(device)
            labels = labels.to(device)

            output_1 = forward_prop(MODEL, data)

            # SEND ----------- feature data 1 ----------------
            sf.send_size_n_msg(output_1, s)
            # RECEIVE ------------ feature data 2 -------------
            recv_data2 = sf.recv_size_n_msg(s)

            MODEL = 2
            OUTPUT = forward_prop(MODEL, recv_data2)

            loss = criterion(OUTPUT, labels)
            loss.backward()     # parts out-layer
            optimizer2.step()

            # SEND ------------- grad 2 -----------
            sf.send_size_n_msg(recv_data2.grad, s)
            # RECEIVE ----------- grad 1 -----------
            recv_grad = sf.recv_size_n_msg(s)

            MODEL = 1
            train_acc += (OUTPUT.max(1)[1] == labels).sum().item()

            output_1.backward(recv_grad)    # parts in-layer
            optimizer1.step()

        avg_train_acc = train_acc / len(trainloader.dataset)
        print("train mode finished!!!!")
        train_acc_list.append(avg_train_acc)

        # =============== test mode ================
        mymodel1.eval()
        mymodel2.eval()
        with torch.no_grad():
            print("start test mode!")
            MODE = 1
            for data, labels in tqdm(testloader):
                sf.send_size_n_msg(MODE, s)

                data = data.to(device)
                labels = labels.to(device)
                output = mymodel1(data)

                # SEND --------- feature data 1 -----------
                sf.send_size_n_msg(output, s)
                # RECEIVE ----------- feature data 2 ------------
                recv_data2 = sf.recv_size_n_msg(s)

                OUTPUT = mymodel2(recv_data2)
                val_acc += (OUTPUT.max(1)[1] == labels).sum().item()

        avg_val_acc = val_acc / len(testloader.dataset)
        print('Epoch [{}/{}], Acc: {:.5f}, val_acc: {:.5f}'.format(e+1, epochs, avg_train_acc, avg_val_acc))
        val_acc_list.append(avg_val_acc)

        if e == epochs - 1:
            MODE = 3  # finished test
            sf.send_size_n_msg(MODE, s)  # notifica o servidor que finalizou
            time.sleep(0.2)  # pequena pausa para garantir o envio
            ack = s.recv(4)
            print(f"Finished the socket connection (USER {client_id})")

    return train_acc_list, val_acc_list

def write_to_csv(train_acc_list, val_acc_list):
    print("Inicio funcao CSV")
    print(train_acc_list)
    print(val_acc_list)

    file_dir = '/home/cleyber/Documentos/ns-3.45/scratch/SplitLearning-B5G/plots'
    if not os.path.exists(file_dir):
        os.makedirs(file_dir)
    file = os.path.join(file_dir, '13_result_train_sync.csv')

    try:
        file_exists = os.path.isfile(file)
        with open(file, 'a', newline='') as f:
            csv_writer = csv.writer(f)

            if not file_exists:
                csv_writer.writerow(['User', 'Train Accuracy', 'Validation Accuracy', 'Timestamp'])

            from datetime import datetime
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

            result = [
                f"user {client_id}",
                ','.join(map(str, train_acc_list)),
                ','.join(map(str, val_acc_list)),
                timestamp
            ]
            csv_writer.writerow(result)
            print(result)

    except Exception as e:
        print(f"Erro ao tentar escrever no CSV: {e}")

if __name__ == '__main__':
    try:
        train_acc_list, val_acc_list = train()
        print("Fim de treino!")
        write_to_csv(train_acc_list, val_acc_list)

    except Exception as e:
        print(f"❌ ERRO durante o treino do cliente: {e}")

    finally:
        try:
            s.shutdown(socket.SHUT_RDWR)
        except Exception as e:
            print(f"(Aviso) Não foi possível realizar shutdown do socket: {e}")
        try:
            s.close()
        except Exception:
            pass
        gc.collect()
        torch.cuda.empty_cache()
        print("✅ Conexão encerrada corretamente no cliente.")
