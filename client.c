import os
import socket
import threading
import tkinter as tk
from tkinter import messagebox, colorchooser, simpledialog, scrolledtext
from datetime import datetime
import ssl

HOST = "10.219.232.60"
PORT = 5555
color = 'black'
shapes = []
last_x, last_y = None, None
username = None  

def send_data(data):
    message = data.encode()
    message_length = len(message)
    client_socket.sendall(message_length.to_bytes(4, 'big'))
    client_socket.sendall(message)

def receive_data():
    while True:
        try:
            message_length = int.from_bytes(client_socket.recv(4), 'big')
            data = client_socket.recv(message_length).decode()
            commands = data.strip().split("\n")
            for command in commands:
                if command.startswith("DRAW"):
                    _, x1, y1, x2, y2, col, thickness = command.split()
                    draw_on_canvas(int(x1), int(y1), int(x2), int(y2), col, int(thickness))
                elif command.startswith("CHAT"):
                    display_chat(command[5:].strip())
                elif command.strip() == "CLEAR":
                    clear_canvas(local=False)
        except Exception as e:
            messagebox.showerror("Error", f"Connection lost: {e}")
            break

def draw_on_canvas(x1, y1, x2, y2, color, thickness):
    shape = canvas.create_line(x1, y1, x2, y2, fill=color, width=thickness, capstyle=tk.ROUND, smooth=True)
    shapes.append(shape)

def on_mouse_down(event):
    global last_x, last_y
    last_x, last_y = event.x, event.y

def on_mouse_drag(event):
    global last_x, last_y
    x, y = event.x, event.y
    if last_x is not None and last_y is not None:
        draw_on_canvas(last_x, last_y, x, y, color, brush_thickness.get())
        send_data(f"DRAW {last_x} {last_y} {x} {y} {color} {brush_thickness.get()}")
    last_x, last_y = x, y

def on_mouse_up(event):
    global last_x, last_y
    last_x, last_y = None, None

def update_cursor():
    if color == "white":
        canvas.config(cursor="dotbox")  
    else:
        canvas.config(cursor="pencil")

def choose_color():
    global color
    selected = colorchooser.askcolor()
    if selected[1]:
        color = selected[1]
        update_cursor()

def use_eraser():
    global color
    color = "white"
    update_cursor()

def clear_canvas(local=True):
    for shape in shapes:
        canvas.delete(shape)
    shapes.clear()
    if local:
        send_data("CLEAR")

def send_chat(event=None):
    message = chat_entry.get().strip()
    if message:
        timestamp = datetime.now().strftime("%H:%M")
        formatted = f"[{timestamp}] You: {message}"
        display_chat(formatted)
        send_data(f"CHAT [{timestamp}] {username}: {message}")
        chat_entry.delete(0, tk.END)

def display_chat(msg):
    chat_display.config(state=tk.NORMAL)
    chat_display.insert(tk.END, msg + "\n")
    chat_display.config(state=tk.DISABLED)
    chat_display.see(tk.END)

def ask_username():
    global username
    root = tk.Tk()
    root.withdraw()
    while not username:
        username = simpledialog.askstring("Username", "Enter your username:")
    root.destroy()

def on_close():
    send_data(f"CHAT 🚪 {username} has left the session.")
    root.quit()

def main():
    global canvas, client_socket, root, chat_display, chat_entry, username, brush_thickness

    ask_username()

    root = tk.Tk()
    root.title(f"🎨 White Board - {username}")
    root.geometry("1150x720")
    root.configure(bg="#FFF7E6")
    root.resizable(False, False)
    root.protocol("WM_DELETE_WINDOW", on_close)

    font_style = ("Comic Sans MS", 10)
    button_font = ("Comic Sans MS", 10, "bold")

    # Main Layout
    main_frame = tk.Frame(root, bg="#FFF7E6")
    main_frame.pack(fill=tk.BOTH, expand=True)

    # Canvas Area (Fixed Width)
    left_panel = tk.Frame(main_frame, bg="#FFF7E6", width=900)
    left_panel.pack(side=tk.LEFT, fill=tk.BOTH, padx=10, pady=10)

    tool_bar = tk.Frame(left_panel, bg="#FFDF91")
    tool_bar.pack(fill=tk.X, pady=(0, 5))

    tk.Button(tool_bar, text="🖌️ Color", command=choose_color, font=button_font, bg="#FFB347", relief=tk.FLAT).pack(side=tk.LEFT, padx=5, pady=5)
    tk.Button(tool_bar, text="🧽 Eraser", command=use_eraser, font=button_font, bg="#FF6961", relief=tk.FLAT).pack(side=tk.LEFT, padx=5, pady=5)
    tk.Button(tool_bar, text="🧹 Clear", command=clear_canvas, font=button_font, bg="#77DD77", relief=tk.FLAT).pack(side=tk.LEFT, padx=5, pady=5)

    tk.Label(tool_bar, text="🖍️ Thickness:", bg="#FFDF91", font=font_style).pack(side=tk.LEFT, padx=5)
    brush_thickness = tk.Scale(tool_bar, from_=1, to=10, orient=tk.HORIZONTAL, bg="#FFDF91")
    brush_thickness.set(2)
    brush_thickness.pack(side=tk.LEFT, padx=5)

    canvas = tk.Canvas(left_panel, bg="white", width=900, height=600, highlightthickness=0, cursor="pencil")
    canvas.pack()
    canvas.bind("<Button-1>", on_mouse_down)
    canvas.bind("<B1-Motion>", on_mouse_drag)
    canvas.bind("<ButtonRelease-1>", on_mouse_up)

    # Chat Area
    right_panel = tk.Frame(main_frame, bg="#FFF0D9", width=250)
    right_panel.pack(side=tk.RIGHT, fill=tk.Y, padx=10, pady=10)

    tk.Label(right_panel, text="💬 Chat", font=("Comic Sans MS", 12, "bold"), bg="#FFF0D9").pack(pady=(5, 0))

    chat_display = scrolledtext.ScrolledText(right_panel, wrap=tk.WORD, state=tk.DISABLED, width=35, height=30, font=font_style, bg="#FFFFFF", fg="#333")
    chat_display.pack(padx=5, pady=5)

    chat_entry = tk.Entry(right_panel, font=font_style)
    chat_entry.pack(fill=tk.X, padx=5, pady=(0, 5))
    chat_entry.bind("<Return>", send_chat)

    # SSL Socket Setup
    try:
        raw_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        client_socket = context.wrap_socket(raw_socket, server_hostname=HOST)
        client_socket.connect((HOST, PORT))
    except Exception as e:
        messagebox.showerror("Connection Error", f"Could not connect to server: {e}")
        root.destroy()
        return

    send_data(f"CHAT ✅ {username} has joined the session.")
    threading.Thread(target=receive_data, daemon=True).start()

    root.mainloop()

if __name__ == "__main__":
    main()
