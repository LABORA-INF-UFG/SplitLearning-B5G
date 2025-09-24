import socket
import pickle

DAM = b'ok!'  # Dummy message to send as acknowledgment

def recv_size_n_msg(s):
    # Recebe os primeiros 16 bytes que indicam o tamanho esperado da mensagem
    data = s.recv(16)
    print(f"Tamanho recebido: {data}")  # Mensagem de depuração para verificar o conteúdo recebido
    
    if not data:
        print("Erro: nenhum dado recebido. Verifique a conexão.")
        raise ValueError("Conexão perdida ou nenhum dado recebido")
    
    try:
        exp_size = int(data)
        #print(f"Tamanho esperado da mensagem: {exp_size} bytes")  # Depuração
    except ValueError:
        raise ValueError(f"Valor inválido recebido: {data}")
    
    # Envia confirmação que recebeu o tamanho da mensagem
    s.sendall(DAM)
    
    recv_size = 0
    recv_data = b''
    
    # Continuar recebendo dados até atingir o tamanho esperado
    while recv_size < exp_size:
        packet = s.recv(524288)  # Tamanho do pacote de 512KB
        if not packet:  # Se não receber nada, pode indicar um problema de conexão
            print("Erro: conexão interrompida durante a recepção de dados.")
            break
        
        recv_size += len(packet)
        recv_data += packet
        #print(f"Recebido até agora: {recv_size}/{exp_size} bytes")  # Depuração
    
    # Envia confirmação que todos os dados foram recebidos
    s.sendall(DAM)
    
    # Desserializa os dados recebidos
    try:
        recv_data = pickle.loads(recv_data)
        #print("Mensagem recebida e desserializada com sucesso.")  # Depuração
    except pickle.UnpicklingError as e:
        print(f"Erro ao desserializar os dados: {e}")
        raise
    
    return recv_data

def send_size_n_msg(msg, s):
    # Serializa a mensagem
    bytes_msg = pickle.dumps(msg)
    msg_size = len(bytes_msg)
    
    # Converte o tamanho da mensagem para um formato de 16 bytes
    msg_size_bytes = str(format(msg_size, '16d')).encode()
    #print(f"Enviando tamanho da mensagem: {msg_size} bytes")  # Depuração
    
    # Envia o tamanho da mensagem
    s.sendall(msg_size_bytes)
    
    # Recebe confirmação do recebimento do tamanho
    dammy = s.recv(4)
    #print(f"Confirmação recebida: {dammy}")  # Depuração
    
    # Envia a mensagem serializada
    s.sendall(bytes_msg)
    
    # Recebe confirmação do recebimento da mensagem
    dammy = s.recv(4)
    #print(f"Confirmação de mensagem recebida: {dammy}")  # Depuração

# Exemplo de uso dentro de um servidor ou cliente
def main():
    # Exemplo de inicialização de um socket
    # Aqui está uma configuração básica de um cliente socket
    host = '127.0.0.1'  # Endereço do servidor
    port = 19089        # Porta de comunicação
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))  # Conecta ao servidor
        
        # Exemplo de envio de uma mensagem
        mensagem_para_enviar = {'data': 'Olá, servidor!'}
        send_size_n_msg(mensagem_para_enviar, s)
        
        # Exemplo de recepção de uma mensagem
        resposta = recv_size_n_msg(s)
        print(f"Resposta do servidor: {resposta}")

if __name__ == "__main__":
    main()
