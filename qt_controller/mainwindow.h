#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QSpinBox>
#include <QTimer>
#include <QQueue>
#include <QPair>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshPorts();
    void toggleConnection();
    void readSerial();
    void sendServo(int id);
    void sendStop();
    void runPreset(const QString &name);
    void onPresetStep();
    void processQueue();

private:
    void setupUi();
    void sendCommand(const QString &cmd, bool expectResponse = true);
    void log(const QString &msg, const QString &color = "#eee");

    QSerialPort *serial;
    QComboBox *portCombo;
    QPushButton *refreshBtn;
    QPushButton *connectBtn;
    QLabel *statusLabel;

    QSlider *sliders[6];
    QLabel *valueLabels[6];
    QSpinBox *timeSpins[6];

    QTextEdit *logView;

    // Preset sequencer
    bool presetRunning;
    QString currentPresetName;
    int presetStep;
    QTimer *presetTimer;

    // Command queue
    QQueue<QPair<QString, bool>> cmdQueue;
    bool waitingResponse;
    void trySendNext();

    // Preset data: {servo_id, angle, move_time_ms, delay_after_ms}
    struct PresetStep {
        int servoId;
        int angle;
        int moveTime;
        int delayAfter;
    };
    QList<PresetStep> presets_home;
    QList<PresetStep> presets_grab;
    QList<PresetStep> presets_wave;
    QMap<QString, QList<PresetStep>> presetMap;
};

#endif // MAINWINDOW_H
