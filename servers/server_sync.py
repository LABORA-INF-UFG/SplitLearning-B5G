#!/usr/bin/env python3
import os
import socket
import torch
import torch.nn as nn
import torch.nn.functional as F
import torchvision.transforms as transforms
import time
import sys
from time import process_time
import subprocess
import gc
from multiprocessing import shared_memory, resource_tracker
import struct
import argparse
import warnings
import shlex

warnings.simplefilter("ignore", category=FutureWarning)

# --- Knobs de simulação de rede (valores seguros por padrão) ---
NET_DELAY_SCALE = float(os.getenv("NET_DELAY_SCALE", "0.05"))  # escala 5% do atraso calculado
MIN_THR_MBPS    = float(os.getenv("MIN_THR_MBPS", "5.0"))      # piso de throughput simulado
MAX_SLEEP_S     = float(os.getenv("MAX_SLEEP_S", "2.0"))       # no máximo 2 s por iteração
DELAY_UNIT      = os.getenv("DELAY_UNIT", "s")                 # "s" ou "ms" conforme a SHM

# --- Vetores globais preenchidos no main() ---
delay_vector = []
throughput_vector = []
energy_vector = []
loss_vector = []
jitter_vector = []
distance_vector = []
device_types_vector = []
split_point_vector = []

# Adiciona o caminho do modelo
file_path = '/home/cleyber/Documentos/ns-3.45/scratch/SplitLearning-B5G/models'
if file_path not in sys.path:
    sys.path.append(file_path)

import ml_model
import socket_fun as sf

DAM = b'ok!'  # Dummy ACK

def get_arguments():
    parser = argparse.ArgumentParser(description='Receber o valor de ueNumPergNb.')
    parser.add_argument('ueNumPergNb', type=int, help='Número de UEs a ser processado por cliente.')
    args = parser.parse_args()
    return args.ueNumPergNb

# CORREÇÃO 1: Obter ueNumPergNb no início
ueNumPergNb = get_arguments()
print(f"Valor de ueNumPergNb recebido: {ueNumPergNb}")

# Cria pastas
for pasta in ["./csv/ia", "./images", "./logs"]:
    os.makedirs(pasta, exist_ok=True)
    print(f"Pasta '{pasta}' verificada/criada.")

# Conexão (único bloco)
user_info = []
host = '127.0.0.1'
port = 19089
ADDR = (host, port)

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # reuso imediato da porta
s.settimeout(120)   # evita travar infinito no accept()
s.bind(ADDR)

# Inicializa vetores
throughput, energy_consumption, packet_loss, distances = [], [], [], []

# Buffer da memória compartilhada
double_size = 8
total_ue_num = ueNumPergNb
num_vectors = 8 
buffer_size = total_ue_num * num_vectors * double_size

def comm_time_seconds(delay_s, thr_mbps, feature_bytes):
    tx_s = feature_bytes / max((thr_mbps * 1e6) / 8.0, 1e-3)
    return delay_s + tx_s

def read_shared_memory(name, size, retries=10, delay=1):
    """
    Lê a memória compartilhada e retorna uma LISTA de vetores:
    [throughputs, energies, losses, distances, device_types, jitters]
    Se a escrita no C++ tiver só 4 métricas, os itens extras virão vazios.
    """
    for attempt in range(retries):
        try:
            shm = shared_memory.SharedMemory(name=name)
            buffer = bytes(shm.buf[:size])
            shm.close()

            # Evita warning do resource_tracker (não fomos nós que criamos a SHM)
            try:
                resource_tracker.unregister(shm._name, 'shared_memory')
            except Exception:
                pass

            values = struct.unpack(f'{total_ue_num * num_vectors}d', buffer)

            # Ordem EXACTA do C++:
            # 0=delay, 1=throughput, 2=energy, 3=loss, 4=jitter, 5=distance, 6=deviceType, 7=splitPoint
            delays        = values[0::num_vectors]
            throughputs   = values[1::num_vectors]
            energies      = values[2::num_vectors]
            losses        = values[3::num_vectors]
            jitters       = values[4::num_vectors]
            dists         = values[5::num_vectors]
            device_types  = values[6::num_vectors]
            split_points  = values[7::num_vectors]

            return [delays, throughputs, energies, losses, jitters, dists, device_types, split_points]


        except FileNotFoundError:
            print(f"Tentativa {attempt + 1}: Memória compartilhada '{name}' não encontrada. Aguardando...")
            time.sleep(delay)
        except Exception as e:
            print(f"Tentativa {attempt + 1}: Erro ao acessar memória compartilhada: {e}")
            time.sleep(delay)

    print(f"❌ Falha ao acessar memória compartilhada após {retries} tentativas")
    return [[], [], [], [], [], []]

def train(user):
    print(f"Iniciando treinamento para {user['name']} com {ueNumPergNb} UEs")

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print("device:", device)
    mymodel = ml_model.ml_model_hidden().to(device)
    print("mymodel:", mymodel)

    # Criar o otimizador uma vez, fora do loop while
    optimizer = torch.optim.SGD(
        mymodel.parameters(), lr=0.005, momentum=0.9, weight_decay=5e-4
    )

    p_start = process_time()
    ite_counter = -1
    total_comm_time = 0
    total_comm_data = 0

    while True:
        try:
            recv_mode = sf.recv_size_n_msg(user["conn"])

            if recv_mode == 0:
                mymodel.train()
                ite_counter += 1
                print(f"({user['name']}) TRAIN Iter {ite_counter}")
                recv_data1 = sf.recv_size_n_msg(user["conn"])

                # >>> Simulação de atraso (corrigida, com knobs)
                client_idx = int(user["name"].split()[1]) - 1

                # Delay: respeita unidade configurada
                raw_delay = delay_vector[client_idx]
                delay_s = raw_delay / 1000.0 if DELAY_UNIT.lower().startswith("ms") else raw_delay

                # Throughput com piso
                thr_mbps_raw = throughput_vector[client_idx]
                thr_mbps = max(thr_mbps_raw, MIN_THR_MBPS)

                # Tamanho real do payload recebido (bytes)
                try:
                    feature_bytes = int(recv_data1.nelement() * recv_data1.element_size())
                except Exception:
                    feature_bytes = 500 * 1024  # fallback

                # Tempo bruto e aplicado
                t_raw = comm_time_seconds(delay_s, thr_mbps, feature_bytes)
                t_net = min(t_raw * NET_DELAY_SCALE, MAX_SLEEP_S)

                print(f"[{user['name']}] NetSim: bruto={t_raw:.3f}s, aplicado={t_net:.3f}s, "
                      f"delay={delay_s:.6f}s ({DELAY_UNIT}), thr={thr_mbps:.3f}Mbps, bytes={feature_bytes}")
                time.sleep(t_net)

                optimizer.zero_grad()  # otimizador agora é global (criado fora do loop)


                output_2 = mymodel(recv_data1)
                sf.send_size_n_msg(output_2, user["conn"])
                recv_grad = sf.recv_size_n_msg(user["conn"])
                output_2.backward(recv_grad)
                optimizer.step()
                sf.send_size_n_msg(recv_data1.grad, user["conn"])

            elif recv_mode == 1:
                mymodel.eval()
                print(f"({user['name']}) TEST")
                recv_data = sf.recv_size_n_msg(user["conn"])
                output_2 = mymodel(recv_data)
                sf.send_size_n_msg(output_2, user["conn"])
            elif recv_mode == 2:
                print(f"{user['name']} finalizou treino.")
                try:
                    user["conn"].sendall(DAM)
                except:
                    pass
                try:
                    user["conn"].shutdown(socket.SHUT_RDWR)
                except:
                    pass
                user["conn"].close()
                break
            elif recv_mode == 3:
                print(f"{user['name']} desconectado.")
                try:
                    user["conn"].sendall(DAM)
                except Exception as e:
                    print(f"(Aviso) Não foi possível enviar ACK final para {user['name']}: {e}")
                try:
                    user["conn"].shutdown(socket.SHUT_RDWR)
                except:
                    pass
                user["conn"].close()
                break
            else:
                print("!!!!! MODE ERROR !!!!!")
                break

        except Exception as e:
            print(f"Erro durante comunicação com {user['name']}: {e}")
            break

    print(f"Fim do treinamento com {user['name']}")
    p_finish = process_time()
    print("Tempo de processamento:", p_finish - p_start)
    print("Total Communication Time:", total_comm_time)
    print("Total Communication Data:", total_comm_data, "bytes")

# Configurações gerais
shared_memory_name = "ns3_shared_memory"
shared_memory_size = total_ue_num * num_vectors * double_size
python_interpreter = "python3"
script_path = "scratch/SplitLearning-B5G/clients/sync/client_sync.py"
subprocesses = []

def main():
    try:
        print(f"🔍 Procurando por memória compartilhada com {total_ue_num} UEs...")
        expected_entries = total_ue_num

        # Lê a SHM (até 10 tentativas)
        results = read_shared_memory(shared_memory_name, shared_memory_size, retries=10, delay=2)

        global delay_vector, throughput_vector, energy_vector, loss_vector
        global jitter_vector, distance_vector, device_types_vector, split_point_vector
        (delay_vector, throughput_vector, energy_vector, loss_vector,
         jitter_vector, distance_vector, device_types_vector, split_point_vector) = results


        # Garante compatibilidade com 4, 5 ou 6 métricas
        #delay_vector, throughput_vector, energy_vector, loss_vector, distance_vector, device_types_vector, jitter_vector = results

        if all(len(v) == expected_entries for v in [
            delay_vector, throughput_vector, energy_vector, loss_vector,
            jitter_vector, distance_vector, device_types_vector, split_point_vector
        ]):
            print(f"✅ Dados da memória compartilhada lidos com sucesso: {expected_entries} UEs")
            print("🚦 Clientes serão iniciados apenas após a leitura completa da memória compartilhada.")

            s.listen(1)
            print(f"🧭 Servidor ouvindo em {host}:{port}")
            print("Servidor aguardando conexão de clientes...")

            for i in range(expected_entries):
                print(f"Entrada {i + 1}: Delay: {delay_vector[i]:.6f}s, "
                     f"Throughput: {throughput_vector[i]:.6f} Mbps, "
                     f"Energy: {energy_vector[i]:.6f}, "
                     f"Packet Loss: {loss_vector[i]}, "
                     f"Jitter: {jitter_vector[i]:.6f}s, "
                     f"Distance: {distance_vector[i]:.2f}, "
                     f"DeviceType: {int(device_types_vector[i])}, "
                     f"SplitPoint: {int(split_point_vector[i])}"
                )

                # acumula métricas deste UE (mantém o comportamento existente)
                throughput.append(throughput_vector[i])
                energy_consumption.append(energy_vector[i])
                packet_loss.append(loss_vector[i])
                distances.append(distance_vector[i])

                print(f"\n🟢 Iniciando cliente {i+1}.")

                max_retries = 3
                attempt = 0
                proc = None
                connected = False

                while attempt < max_retries and not connected:
                    # (re)lança o cliente na 1ª tentativa ou se ele tiver morrido
                    if attempt == 0 or (proc and proc.poll() is not None):
                        client_cmd = f'{python_interpreter} {shlex.quote(script_path)} {i + 1}'
                        #term_cmd = ['gnome-terminal', '--', 'bash', '-lc', client_cmd + '; exit']
                        term_cmd = [
                            'gnome-terminal', 
                            '--title', f'Cliente {i+1}',
                            '--', 'bash', '-lc', client_cmd + '; exit'
                        ]
                        try:
                            proc = subprocess.Popen(term_cmd)
                            subprocesses.append(proc)
                            print(f"🟢 Iniciando cliente {i+1} (tentativa {attempt+1}). PID={proc.pid}")
                        except Exception as e:
                            print(f"❌ Falha ao iniciar cliente {i+1}: {e}")
                            break  # sai do while para tratar abaixo

                    try:
                        conn, addr = s.accept()  # espera até 120s (timeout do socket)
                        print(f"🔗 Cliente {i+1} conectado de {addr}")
                        connected = True
                    except socket.timeout:
                        attempt += 1
                        print("⏳ Ninguém conectou em 120s; vou verificar o processo...")
                        if proc and proc.poll() is not None:  # morreu
                            print("↩️ Processo do cliente terminou; vou relançar.")
                            continue
                        else:
                            print("⏱️ Cliente ainda inicializando (download/dataset?); vou aguardar mais.")
                            continue  # mantém o mesmo proc vivo e tenta aceitar de novo

                # Se não conectou após as tentativas, falhe de forma clara
                if not connected:
                    raise RuntimeError(f"Cliente {i+1} não conectou após {max_retries} tentativas.")

                # Registro do usuário conectado
                user = {"name": f"Client {i + 1}", "conn": conn, "addr": addr}
                user_info.append(user)

                # Handshake
                recvreq = conn.recv(1024).decode()
                if not recvreq.startswith("READY:"):
                    raise RuntimeError(f"Handshake inválido recebido: {recvreq}")
                conn.sendall(DAM)

                # Treino
                train(user)
                print(f"✅ Cliente {i+1} finalizado.")

        else:
            print("❌ Erro: dados incompletos ou inconsistentes na memória compartilhada!")
            print(f"Tamanhos recebidos: delay={len(delay_vector)}, throughput={len(throughput_vector)}, "
                  f"energy={len(energy_vector)}, loss={len(loss_vector)}, distance={len(distance_vector)}, "
                  f"device_type={len(device_types_vector)}, jitter={len(jitter_vector)}")
            sys.exit(1)

    except Exception as e:
        print(f"❌ Erro ao executar o servidor: {e}")
        import traceback
        traceback.print_exc()

    finally:
        # Cleanup
        print("🧹 Limpando recursos...")
        for proc in subprocesses:
            try:
                proc.wait(timeout=5)
                if proc.poll() is None:  # ainda está rodando
                    proc.terminate()
                    proc.wait()
            except subprocess.TimeoutExpired:
                proc.terminate()
                proc.wait()
        # Fechar socket
        try:
            s.close()
        except Exception as e:
            print(f"(Aviso) Não foi possível fechar o socket: {e}")
        torch.cuda.empty_cache()
        gc.collect()
        print("✅ Cleanup finalizado.")

if __name__ == "__main__":
    main()
