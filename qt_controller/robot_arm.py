import sys
import time
from collections import deque
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLabel, QComboBox, QPushButton, QSlider, QSpinBox,
    QTextEdit, QScrollBar
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QThread
from PyQt5.QtGui import QFont, QColor
import serial
import serial.tools.list_ports


SERVO_NAMES = ['Gripper', 'Rotate1', 'ArmX-1', 'ArmX-2', 'ArmX-3', 'Base']

PRESETS = {
    'home': [
        (0,90,500,100),(1,90,500,100),(2,90,500,100),
        (3,90,500,100),(4,90,500,100),(5,90,500,0),
    ],
    'grab': [
        (5,90,800,200),(4,60,800,200),(3,100,600,200),
        (2,70,600,200),(0,30,500,800),(4,100,600,200),
        (3,120,500,700),(0,150,400,600),(3,80,800,200),
        (4,40,800,800),(5,140,1000,1200),(4,100,600,200),
        (3,120,500,700),(0,30,400,600),(3,80,600,200),
        (4,40,600,200),(5,90,800,200),(2,70,600,200),(0,90,500,0),
    ],
    'wave': [
        (5,60,400,500),(5,120,400,500),(5,60,400,500),
        (5,120,400,500),(5,90,400,0),
    ],
}


STYLE = """
QMainWindow, QWidget { background: #1a1a2e; color: #eee; font-family: Arial; }
QGroupBox {
    background: #16213e; border: 1px solid #333; border-radius: 8px;
    margin-top: 10px; padding-top: 14px; font-weight: bold; color: #eee;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QPushButton {
    background: #0f3460; color: #eee; border: none; border-radius: 6px;
    padding: 8px 16px; font-size: 13px;
}
QPushButton:pressed { background: #e94560; }
QPushButton:disabled { background: #333; color: #666; }
QComboBox, QSpinBox {
    background: #0f3460; color: #eee; border: 1px solid #333;
    border-radius: 4px; padding: 4px;
}
QComboBox::drop-down { border: none; }
QSlider::groove:horizontal { height: 6px; background: #333; border-radius: 3px; }
QSlider::handle:horizontal {
    background: #e94560; width: 16px; height: 16px;
    margin: -5px 0; border-radius: 8px;
}
QTextEdit {
    background: #0a0a1a; color: #eee; border: 1px solid #333;
    border-radius: 6px; font-family: Consolas, monospace; font-size: 12px;
}
"""


class SerialReaderThread(QThread):
    line_received = pyqtSignal(str)

    def __init__(self, ser):
        super().__init__()
        self.ser = ser
        self.running = True

    def run(self):
        while self.running:
            try:
                if self.ser and self.ser.is_open and self.ser.in_waiting:
                    raw = self.ser.readline()
                    line = raw.decode('utf-8', errors='replace').strip()
                    if line:
                        self.line_received.emit(line)
                else:
                    time.sleep(0.02)
            except Exception:
                time.sleep(0.05)

    def stop(self):
        self.running = False


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.ser = None
        self.reader_thread = None
        self.cmd_queue = deque()
        self.waiting_response = False
        self.preset_running = False
        self.preset_step = 0
        self.current_preset_name = ''
        self.sliders = []
        self.value_labels = []
        self.time_spins = []

        self.setWindowTitle('Robot Arm Controller')
        self.resize(520, 680)
        self.setStyleSheet(STYLE)

        self._build_ui()

        self.preset_timer = QTimer()
        self.preset_timer.setSingleShot(True)
        self.preset_timer.timeout.connect(self._on_preset_step)

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # Title
        title = QLabel('Robot Arm Controller')
        title.setStyleSheet('font-size: 20px; font-weight: bold; color: #e94560; padding: 8px;')
        title.setAlignment(Qt.AlignCenter)
        layout.addWidget(title)

        # Connection
        conn_group = QGroupBox('Connection')
        conn_layout = QHBoxLayout(conn_group)
        conn_layout.addWidget(QLabel('Port:'))
        self.port_combo = QComboBox()
        conn_layout.addWidget(self.port_combo, 1)
        self.refresh_btn = QPushButton('Refresh')
        self.refresh_btn.clicked.connect(self._refresh_ports)
        conn_layout.addWidget(self.refresh_btn)
        self.connect_btn = QPushButton('Connect')
        self.connect_btn.clicked.connect(self._toggle_connection)
        conn_layout.addWidget(self.connect_btn)
        self.status_label = QLabel('Disconnected')
        self.status_label.setStyleSheet('color: #888; font-size: 12px;')
        conn_layout.addWidget(self.status_label)
        layout.addWidget(conn_group)

        # Servos
        servo_group = QGroupBox('Servo Control')
        servo_layout = QVBoxLayout(servo_group)
        for i in range(6):
            row = QHBoxLayout()
            name = QLabel(f'{i} {SERVO_NAMES[i]}')
            name.setStyleSheet('font-size: 13px;')
            name.setFixedWidth(90)

            slider = QSlider(Qt.Horizontal)
            slider.setRange(0, 180)
            slider.setValue(90)
            self.sliders.append(slider)

            val_label = QLabel('90')
            val_label.setStyleSheet('font-weight: bold; color: #e94560; font-size: 14px;')
            val_label.setFixedWidth(35)
            val_label.setAlignment(Qt.AlignCenter)
            self.value_labels.append(val_label)

            t_label = QLabel('ms:')
            t_label.setStyleSheet('font-size: 11px;')
            time_spin = QSpinBox()
            time_spin.setRange(100, 5000)
            time_spin.setValue(1000)
            time_spin.setSingleStep(100)
            time_spin.setFixedWidth(70)
            self.time_spins.append(time_spin)

            send_btn = QPushButton('Send')
            send_btn.setFixedWidth(55)
            send_btn.clicked.connect(lambda _, idx=i: self._send_servo(idx))

            slider.valueChanged.connect(lambda v, idx=i: self.value_labels[idx].setText(str(v)))

            row.addWidget(name)
            row.addWidget(slider, 1)
            row.addWidget(val_label)
            row.addWidget(t_label)
            row.addWidget(time_spin)
            row.addWidget(send_btn)
            servo_layout.addLayout(row)
        layout.addWidget(servo_group)

        # Buttons
        btn_group = QGroupBox('Actions')
        btn_layout = QVBoxLayout(btn_group)

        stop_btn = QPushButton('STOP')
        stop_btn.setStyleSheet(
            'QPushButton { background: #e94560; font-size: 18px; font-weight: bold; padding: 12px; }'
            'QPushButton:pressed { background: #c73e54; }'
        )
        stop_btn.clicked.connect(self._send_stop)
        btn_layout.addWidget(stop_btn)

        preset_row = QHBoxLayout()
        for name, color in [('home', '#0f3460'), ('grab', '#533483'), ('wave', '#2b6777')]:
            btn = QPushButton(name.capitalize())
            btn.setStyleSheet(f'QPushButton {{ background: {color}; font-size: 14px; padding: 10px; }}')
            btn.clicked.connect(lambda _, n=name: self._run_preset(n))
            preset_row.addWidget(btn)
        btn_layout.addLayout(preset_row)
        layout.addWidget(btn_group)

        # Log
        log_group = QGroupBox('Log')
        log_layout = QVBoxLayout(log_group)
        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.setMaximumHeight(150)
        log_layout.addWidget(self.log_view)
        layout.addWidget(log_group)

        self._refresh_ports()

    def _refresh_ports(self):
        self.port_combo.clear()
        for info in serial.tools.list_ports.comports():
            desc = info.device
            if info.description and info.description != 'n/a':
                desc += f' ({info.description})'
            self.port_combo.addItem(desc, info.device)

    def _toggle_connection(self):
        if self.ser and self.ser.is_open:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        if self.port_combo.currentIndex() < 0:
            self._log('No port selected', '#e94560')
            return
        port_name = self.port_combo.currentData()
        try:
            self.ser = serial.Serial(port_name, 115200, timeout=0.1)
            self.connect_btn.setText('Disconnect')
            self.status_label.setText('Connected')
            self.status_label.setStyleSheet('color: #81c784; font-size: 12px;')
            self.port_combo.setEnabled(False)
            self.refresh_btn.setEnabled(False)
            self._log(f'Connected to {port_name}', '#81c784')

            self.reader_thread = SerialReaderThread(self.ser)
            self.reader_thread.line_received.connect(self._on_serial_line)
            self.reader_thread.start()
        except Exception as e:
            self._log(f'Open failed: {e}', '#e94560')

    def _disconnect(self):
        if self.preset_running:
            self.preset_running = False
            self.preset_timer.stop()
        if self.reader_thread:
            self.reader_thread.stop()
            self.reader_thread.wait()
            self.reader_thread = None
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.cmd_queue.clear()
        self.waiting_response = False
        self.connect_btn.setText('Connect')
        self.status_label.setText('Disconnected')
        self.status_label.setStyleSheet('color: #888; font-size: 12px;')
        self.port_combo.setEnabled(True)
        self.refresh_btn.setEnabled(True)
        self._log('Disconnected', '#888')

    def _on_serial_line(self, line):
        debug_prefixes = ('[USB]', '[TX->STM32]', '[RX<-STM32]', '===',
                          'Serial1', 'WiFi', 'Web server', 'natapp',
                          '---', 'Connecting', 'Local IP', 'RSSI', '.')
        if any(line.startswith(p) for p in debug_prefixes):
            self._log(f'[ESP32] {line}', '#4fc3f7')
        else:
            self._log(f'[STM32] {line}', '#81c784')
            if line in ('OK', 'ERR', 'TIMEOUT'):
                self.waiting_response = False
                self._process_queue()

    def _send_command(self, cmd, expect_response=True):
        if not self.ser or not self.ser.is_open:
            self._log('Not connected', '#e94560')
            return
        self.cmd_queue.append((cmd, expect_response))
        if not self.waiting_response:
            self._try_send_next()

    def _try_send_next(self):
        if not self.cmd_queue:
            return
        cmd, expect = self.cmd_queue.popleft()
        self._log(f'TX: {cmd}', '#4fc3f7')
        try:
            self.ser.write((cmd + '\n').encode('utf-8'))
        except Exception as e:
            self._log(f'Write error: {e}', '#e94560')
            return
        if expect:
            self.waiting_response = True
        else:
            self._process_queue()

    def _process_queue(self):
        self._try_send_next()

    def _send_servo(self, idx):
        cmd = f'S{idx},{self.sliders[idx].value()},{self.time_spins[idx].value()}'
        self._send_command(cmd)

    def _send_stop(self):
        if self.preset_running:
            self.preset_running = False
            self.preset_timer.stop()
            self._log('--- preset cancelled ---', '#e94560')
        self._send_command('STOP')

    def _run_preset(self, name):
        if name not in PRESETS:
            return
        if self.preset_running:
            self.preset_running = False
            self.preset_timer.stop()
            return
        self.current_preset_name = name
        self.preset_step = 0
        self.preset_running = True
        self._log(f'--- {name} start ---', '#81c784')
        self._on_preset_step()

    def _on_preset_step(self):
        if not self.preset_running:
            return
        steps = PRESETS[self.current_preset_name]
        if self.preset_step >= len(steps):
            self.preset_running = False
            self._log('--- done ---', '#81c784')
            return
        sid, angle, move_time, delay_after = steps[self.preset_step]

        self.sliders[sid].setValue(angle)
        self.time_spins[sid].setValue(move_time)

        self._send_command(f'S{sid},{angle},{move_time}', True)

        self.preset_step += 1
        if delay_after > 0:
            self.preset_timer.start(move_time + delay_after)
        else:
            self.preset_running = False
            self._log('--- done ---', '#81c784')

    def _log(self, msg, color='#eee'):
        self.log_view.append(f'<span style="color:{color}">{msg}</span>')
        sb = self.log_view.verticalScrollBar()
        sb.setValue(sb.maximum())

    def closeEvent(self, event):
        self._disconnect()
        event.accept()


if __name__ == '__main__':
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
