import socket
import time
import random

def generate_random_numbers():
    return ','.join(str(random.randint(-90, 90)) for _ in range(4))

def main():
    ip_address = "192.168.1.212"  # Change this to your target IP
    port = 3333
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect((ip_address, port))
            print(f"Connected to {ip_address}:{port}")
            
            i = -90
            while True:
                if i > 90:
                    i = -90
                else: 
                    i = i + 1 

                message = ','.join(str(i) for _ in range(4))
                s.sendall(message.encode('utf-8'))
                print(f"Sent: {message}")
                time.sleep(0.01)  # 500ms delay
        except Exception as e:
            print(f"Error: {e}")

if __name__ == "__main__":
    main()
