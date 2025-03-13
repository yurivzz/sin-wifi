import sys
import socket
import struct
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget
from PySide6.QtCore import QTimer
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

class RealTimePlot(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Real-Time Data Viewer")
        self.setGeometry(100, 100, 800, 600)

        self.figure = Figure()
        self.canvas = FigureCanvas(self.figure)
        self.ax = self.figure.add_subplot(111)
        
        layout = QVBoxLayout()
        layout.addWidget(self.canvas)
        
        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect(('192.168.4.1', 3333))
        
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(50)

        self.data_buffer = np.array([], dtype=np.float32)

    def update_plot(self):
        try:
            raw_data = self.sock.recv(256 * 4)
            if len(raw_data) == 256 * 4:
                chunk = np.array(struct.unpack('256f', raw_data), dtype=np.float32)
                self.data_buffer = np.concatenate((self.data_buffer, chunk))
                
                self.ax.clear()
                self.ax.plot(chunk)
                self.ax.set_title("Último Pacote Recebido (256 amostras)")
                self.ax.set_xlabel("Amostras")
                self.ax.set_ylabel("Amplitude")
                self.canvas.draw()

        except Exception as e:
            print(f"Erro: {e}")
            self.timer.stop()
            self.sock.close()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = RealTimePlot()
    window.show()
    sys.exit(app.exec())