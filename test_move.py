import socket
import sys

def send_move(move_str, host='127.0.0.1', port=8080):
    if len(move_str) != 4:
        print("Move must be exactly 4 characters, e.g., e2e4")
        return
        
    try:
        # Create a TCP socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            # Connect to the chess game server
            s.connect((host, port))
            # Send the move as a byte string
            s.sendall(move_str.encode('utf-8'))
            print(f"Successfully sent move: {move_str}")
            
    except ConnectionRefusedError:
        print(f"Error: Could not connect to {host}:{port}.")
        print("Ensure the chess game is running, and you are in the 'Play' screen!")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    # If the user provides a command-line argument (like `python test_move.py e2e4`)
    if len(sys.argv) > 1:
        send_move(sys.argv[1])
    else:
        # Otherwise, ask for input interactively
        while True:
            move = input("Enter your move (e.g., e2e4) or 'q' to quit: ").strip()
            if move.lower() == 'q':
                break
            if len(move) == 4:
                send_move(move)
            else:
                print("Invalid format. Please use 4 characters, like 'e2e4'.")
