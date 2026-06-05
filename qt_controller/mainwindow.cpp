#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QMessageBox>

static const char *SERVO_NAMES[6] = {
    "Gripper", "Rotate1", "ArmX-1", "ArmX-2", "ArmX-3", "Base"
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), serial(new QSerialPort(this)),
      presetRunning(false), presetStep(0), waitingResponse(false)
{
    setupUi();
    setWindowTitle("Robot Arm Controller");
    resize(520, 680);

    // Presets
    presets_home = {{0,90,500,100},{1,90,500,100},{2,90,500,100},
                    {3,90,500,100},{4,90,500,100},{5,90,500,0}};
    presets_grab = {{5,90,800,200},{4,60,800,200},{3,100,600,200},
                    {2,70,600,200},{0,30,500,800},{4,100,600,200},
                    {3,120,500,700},{0,150,400,600},{3,80,800,200},
                    {4,40,800,800},{5,140,1000,1200},{4,100,600,200},
                    {3,120,500,700},{0,30,400,600},{3,80,600,200},
                    {4,40,600,200},{5,90,800,200},{2,70,600,200},{0,90,500,0}};
    presets_wave = {{5,60,400,500},{5,120,400,500},{5,60,400,500},
                    {5,120,400,500},{5,90,400,0}};
    presetMap["home"] = presets_home;
    presetMap["grab"] = presets_grab;
    presetMap["wave"] = presets_wave;

    presetTimer = new QTimer(this);
    presetTimer->setSingleShot(true);
    connect(presetTimer, &QTimer::timeout, this, &MainWindow::onPresetStep);

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerial);
    connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err == QSerialPort::ResourceError) {
            log("Connection lost", "#e94560");
            serial->close();
            connectBtn->setText("Connect");
            statusLabel->setText("Disconnected");
            portCombo->setEnabled(true);
            refreshBtn->setEnabled(true);
        }
    });

    refreshPorts();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // --- Style ---
    central->setStyleSheet(
        "QWidget { background: #1a1a2e; color: #eee; font-family: Arial; }"
        "QGroupBox { background: #16213e; border: 1px solid #333; border-radius: 8px; "
        "  margin-top: 10px; padding-top: 14px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QPushButton { background: #0f3460; color: #eee; border: none; border-radius: 6px; "
        "  padding: 8px 16px; font-size: 13px; }"
        "QPushButton:pressed { background: #e94560; }"
        "QPushButton:disabled { background: #333; color: #666; }"
        "QComboBox, QSpinBox { background: #0f3460; color: #eee; border: 1px solid #333; "
        "  border-radius: 4px; padding: 4px; }"
        "QComboBox::drop-down { border: none; }"
        "QSlider::groove:horizontal { height: 6px; background: #333; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #e94560; width: 16px; height: 16px; "
        "  margin: -5px 0; border-radius: 8px; }"
        "QTextEdit { background: #0a0a1a; color: #eee; border: 1px solid #333; "
        "  border-radius: 6px; font-family: Consolas, monospace; font-size: 12px; }"
    );

    // --- Title ---
    QLabel *title = new QLabel("Robot Arm Controller");
    title->setStyleSheet("font-size: 20px; font-weight: bold; color: #e94560; padding: 8px;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // --- Connection ---
    QGroupBox *connGroup = new QGroupBox("Connection");
    QHBoxLayout *connLayout = new QHBoxLayout(connGroup);
    portCombo = new QComboBox;
    refreshBtn = new QPushButton("Refresh");
    connectBtn = new QPushButton("Connect");
    statusLabel = new QLabel("Disconnected");
    statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    connLayout->addWidget(new QLabel("Port:"));
    connLayout->addWidget(portCombo, 1);
    connLayout->addWidget(refreshBtn);
    connLayout->addWidget(connectBtn);
    connLayout->addWidget(statusLabel);
    mainLayout->addWidget(connGroup);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    // --- Servos ---
    QGroupBox *servoGroup = new QGroupBox("Servo Control");
    QVBoxLayout *servoLayout = new QVBoxLayout(servoGroup);
    for (int i = 0; i < 6; i++) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *name = new QLabel(QString("%1 %2").arg(i).arg(SERVO_NAMES[i]));
        name->setStyleSheet("font-size: 13px;");
        name->setFixedWidth(90);

        sliders[i] = new QSlider(Qt::Horizontal);
        sliders[i]->setRange(0, 180);
        sliders[i]->setValue(90);

        valueLabels[i] = new QLabel("90");
        valueLabels[i]->setStyleSheet("font-weight: bold; color: #e94560; font-size: 14px;");
        valueLabels[i]->setFixedWidth(35);
        valueLabels[i]->setAlignment(Qt::AlignCenter);

        QLabel *tLabel = new QLabel("ms:");
        tLabel->setStyleSheet("font-size: 11px;");
        timeSpins[i] = new QSpinBox;
        timeSpins[i]->setRange(100, 5000);
        timeSpins[i]->setValue(1000);
        timeSpins[i]->setSingleStep(100);
        timeSpins[i]->setFixedWidth(70);

        QPushButton *sendBtn = new QPushButton("Send");
        sendBtn->setFixedWidth(55);

        row->addWidget(name);
        row->addWidget(sliders[i], 1);
        row->addWidget(valueLabels[i]);
        row->addWidget(tLabel);
        row->addWidget(timeSpins[i]);
        row->addWidget(sendBtn);
        servoLayout->addLayout(row);

        connect(sliders[i], &QSlider::valueChanged, this, [this, i](int v) {
            valueLabels[i]->setText(QString::number(v));
        });
        connect(sendBtn, &QPushButton::clicked, this, [this, i]() { sendServo(i); });
    }
    mainLayout->addWidget(servoGroup);

    // --- Buttons ---
    QGroupBox *btnGroup = new QGroupBox("Actions");
    QVBoxLayout *btnLayout = new QVBoxLayout(btnGroup);

    QPushButton *stopBtn = new QPushButton("STOP");
    stopBtn->setStyleSheet(
        "QPushButton { background: #e94560; font-size: 18px; font-weight: bold; padding: 12px; }"
        "QPushButton:pressed { background: #c73e54; }"
    );
    btnLayout->addWidget(stopBtn);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::sendStop);

    QHBoxLayout *presetRow = new QHBoxLayout;
    QPushButton *homeBtn = new QPushButton("Home");
    homeBtn->setStyleSheet("QPushButton { background: #0f3460; font-size: 14px; padding: 10px; }");
    QPushButton *grabBtn = new QPushButton("Grab");
    grabBtn->setStyleSheet("QPushButton { background: #533483; font-size: 14px; padding: 10px; }");
    QPushButton *waveBtn = new QPushButton("Wave");
    waveBtn->setStyleSheet("QPushButton { background: #2b6777; font-size: 14px; padding: 10px; }");
    presetRow->addWidget(homeBtn);
    presetRow->addWidget(grabBtn);
    presetRow->addWidget(waveBtn);
    btnLayout->addLayout(presetRow);

    connect(homeBtn, &QPushButton::clicked, this, [this]() { runPreset("home"); });
    connect(grabBtn, &QPushButton::clicked, this, [this]() { runPreset("grab"); });
    connect(waveBtn, &QPushButton::clicked, this, [this]() { runPreset("wave"); });

    mainLayout->addWidget(btnGroup);

    // --- Log ---
    QGroupBox *logGroup = new QGroupBox("Log");
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    logView = new QTextEdit;
    logView->setReadOnly(true);
    logView->setMaximumHeight(150);
    logLayout->addWidget(logView);
    mainLayout->addWidget(logGroup);
}

void MainWindow::refreshPorts()
{
    portCombo->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QString desc = info.portName();
        if (!info.description().isEmpty())
            desc += " (" + info.description() + ")";
        portCombo->addItem(desc, info.portName());
    }
}

void MainWindow::toggleConnection()
{
    if (serial->isOpen()) {
        if (presetRunning) {
            presetRunning = false;
            presetTimer->stop();
        }
        cmdQueue.clear();
        waitingResponse = false;
        serial->close();
        connectBtn->setText("Connect");
        statusLabel->setText("Disconnected");
        statusLabel->setStyleSheet("color: #888; font-size: 12px;");
        portCombo->setEnabled(true);
        refreshBtn->setEnabled(true);
        log("Disconnected", "#888");
    } else {
        if (portCombo->currentIndex() < 0) {
            log("No port selected", "#e94560");
            return;
        }
        serial->setPortName(portCombo->currentData().toString());
        serial->setBaudRate(QSerialPort::Baud115200);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        if (serial->open(QIODevice::ReadWrite)) {
            connectBtn->setText("Disconnect");
            statusLabel->setText("Connected");
            statusLabel->setStyleSheet("color: #81c784; font-size: 12px;");
            portCombo->setEnabled(false);
            refreshBtn->setEnabled(false);
            log("Connected to " + serial->portName(), "#81c784");
        } else {
            log("Open failed: " + serial->errorString(), "#e94560");
        }
    }
}

void MainWindow::readSerial()
{
    while (serial->canReadLine()) {
        QString line = QString::fromUtf8(serial->readLine()).trimmed();
        if (line.isEmpty()) continue;

        // Filter debug lines from ESP32
        if (line.startsWith("[USB]") || line.startsWith("[TX->STM32]") ||
            line.startsWith("[RX<-STM32]") || line.startsWith("===") ||
            line.startsWith("Serial1") || line.startsWith("WiFi") ||
            line.startsWith("Web server") || line.startsWith("natapp") ||
            line.startsWith("---") || line.startsWith("Connecting") ||
            line.startsWith("Local IP") || line.startsWith("RSSI")) {
            log("[ESP32] " + line, "#4fc3f7");
        } else {
            log("[STM32] " + line, "#81c784");
            // Response from STM32 (OK/ERR) → release queue
            if (line == "OK" || line == "ERR" || line == "TIMEOUT") {
                waitingResponse = false;
                processQueue();
            }
        }
    }
}

void MainWindow::sendCommand(const QString &cmd, bool expectResponse)
{
    if (!serial->isOpen()) {
        log("Not connected", "#e94560");
        return;
    }
    cmdQueue.enqueue(qMakePair(cmd, expectResponse));
    if (!waitingResponse) {
        trySendNext();
    }
}

void MainWindow::trySendNext()
{
    if (cmdQueue.isEmpty()) return;
    QPair<QString, bool> item = cmdQueue.dequeue();
    log("TX: " + item.first, "#4fc3f7");
    serial->write((item.first + "\n").toUtf8());
    if (item.second) {
        waitingResponse = true;
    } else {
        processQueue();
    }
}

void MainWindow::processQueue()
{
    trySendNext();
}

void MainWindow::sendServo(int id)
{
    QString cmd = QString("S%1,%2,%3").arg(id).arg(sliders[id]->value()).arg(timeSpins[id]->value());
    sendCommand(cmd);
}

void MainWindow::sendStop()
{
    if (presetRunning) {
        presetRunning = false;
        presetTimer->stop();
        log("--- preset cancelled ---", "#e94560");
    }
    sendCommand("STOP");
}

void MainWindow::runPreset(const QString &name)
{
    if (!presetMap.contains(name)) return;
    if (presetRunning) {
        presetRunning = false;
        presetTimer->stop();
        return;
    }
    currentPresetName = name;
    presetStep = 0;
    presetRunning = true;
    log("--- " + name + " start ---", "#81c784");
    onPresetStep();
}

void MainWindow::onPresetStep()
{
    if (!presetRunning) return;
    const QList<PresetStep> &steps = presetMap[currentPresetName];
    if (presetStep >= steps.size()) {
        presetRunning = false;
        log("--- done ---", "#81c784");
        return;
    }
    const PresetStep &s = steps[presetStep];

    // Update UI
    sliders[s.servoId]->setValue(s.angle);
    timeSpins[s.servoId]->setValue(s.moveTime);

    // Send command
    sendCommand(QString("S%1,%2,%3").arg(s.servoId).arg(s.angle).arg(s.moveTime), true);

    presetStep++;
    if (s.delayAfter > 0) {
        presetTimer->start(s.moveTime + s.delayAfter);
    } else {
        // Last step, no more delay
        presetRunning = false;
        log("--- done ---", "#81c784");
    }
}

void MainWindow::log(const QString &msg, const QString &color)
{
    logView->append(QString("<span style='color:%1'>%2</span>").arg(color, msg.toHtmlEscaped()));
    QScrollBar *sb = logView->verticalScrollBar();
    sb->setValue(sb->maximum());
}
