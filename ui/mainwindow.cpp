/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
* ========================================================================== *
*/

#include "mainwindow.h"
#include "global.h"
#include "settingdialog.h"
#include "ui_mainwindow.h"
#include "globalsetting.h"

#include "host/HostManager.h"
#include "serial/SerialPortManager.h"
#include "loghandler.h"
#include "ui/imagesettings.h"
#include "ui/settingdialog.h"
#include "ui/helppane.h"
#include "ui/serialportdebugdialog.h"

#include "ui/videopane.h"
#include "video/videohid.h"

#include <QCameraDevice>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaMetaData>
#include <QMediaRecorder>
#include <QVideoWidget>
#include <QStackedLayout>
#include <QMessageBox>
#include <QImageCapture>
#include <QToolBar>
#include <QClipboard>
#include <QInputMethod>
#include <QAction>
#include <QActionGroup>
#include <QImage>
#include <QKeyEvent>
#include <QPalette>
#include <QSystemTrayIcon>
#include <QDir>
#include <QTimer>
#include <QLabel>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>
#include <QMessageBox>
#include <QDesktopServices>
#include <QSysInfo>
#include <QMenuBar>
#include <QPushButton>
#include <QComboBox>

#include <QGuiApplication>

Q_LOGGING_CATEGORY(log_ui_mainwindow, "opf.ui.mainwindow")

/*
  * QT Permissions API is not compatible with Qt < 6.5 and will cause compilation failure on
  * expanding the QT_CONFIG macro if it isn't set as a feature in qtcore-config.h. QT < 6.5
  * is still true for a large number of linux distros in 2024. This ifdef or another
  * workaround needs to be used anywhere the QPermissions class is called, for distros to
  * be able to use their package manager's native Qt libs, if they are < 6.5.
  *
  * See qtconfigmacros.h, qtcore-config.h, etc. in the relevant Qt includes directory, and:
  * https://doc-snapshots.qt.io/qt6-6.5/whatsnew65.html
  * https://doc-snapshots.qt.io/qt6-6.5/permissions.html
*/

#ifdef QT_FEATURE_permissions
#if QT_CONFIG(permissions)
#include <QPermission>
#endif
#endif

Camera::Camera() : ui(new Ui::Camera), m_audioManager(new AudioManager(this)),
                                        videoPane(new VideoPane(this)),
                                        stackedLayout(new QStackedLayout(this)),
                                        toolbarManager(new ToolbarManager(this)),
                                        statusWidget(new StatusWidget(this)),
                                        toggleSwitch(new ToggleSwitch(this))
{
    qCDebug(log_ui_mainwindow) << "Init camera...";
    ui->setupUi(this);
    ui->statusbar->addPermanentWidget(statusWidget);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(stackedLayout);

    HelpPane *helpPane = new HelpPane;
    stackedLayout->addWidget(helpPane);

    stackedLayout->addWidget(videoPane);

    stackedLayout->setCurrentIndex(0);

    centralWidget->setMouseTracking(true);

    ui->menubar->setCornerWidget(ui->cornerWidget, Qt::TopRightCorner);

    setCentralWidget(centralWidget);
    qCDebug(log_ui_mainwindow) << "Set host manager event callback...";
    HostManager::getInstance().setEventCallback(this);

    qCDebug(log_ui_mainwindow) << "Observe Video HID connected...";
    VideoHid::getInstance().setEventCallback(this);

    qCDebug(log_ui_mainwindow) << "Observe video input changed...";
    connect(&m_source, &QMediaDevices::videoInputsChanged, this, &Camera::updateCameras);

    //connect(videoDevicesGroup, &QActionGroup::triggered, this, &Camera::updateCameraDevice);

    qCDebug(log_ui_mainwindow) << "Observe Relative/Absolute toggle...";
    connect(ui->actionRelative, &QAction::triggered, this, &Camera::onActionRelativeTriggered);
    connect(ui->actionAbsolute, &QAction::triggered, this, &Camera::onActionAbsoluteTriggered);

    qCDebug(log_ui_mainwindow) << "Observe reset HID triggerd...";
    connect(ui->actionResetHID, &QAction::triggered, this, &Camera::onActionResetHIDTriggered);

    qCDebug(log_ui_mainwindow) << "Observe factory reset HID triggerd...";
    connect(ui->actionFactory_reset_HID, &QAction::triggered, this, &Camera::onActionFactoryResetHIDTriggered);

    qCDebug(log_ui_mainwindow) << "Observe reset Serial Port triggerd...";
    connect(ui->actionResetSerialPort, &QAction::triggered, this, &Camera::onActionResetSerialPortTriggered);

    qDebug() << "Observe Hardware change Camera triggerd...";

    qCDebug(log_ui_mainwindow) << "Creating and setting up ToggleSwitch...";
    toggleSwitch->setFixedSize(78, 28);  // Adjust size as needed
    connect(toggleSwitch, &ToggleSwitch::stateChanged, this, &Camera::onToggleSwitchStateChanged);

    // Add the ToggleSwitch as the last button in the cornerWidget's layout
    QHBoxLayout *cornerLayout = qobject_cast<QHBoxLayout*>(ui->cornerWidget->layout());
    if (cornerLayout) {
        cornerLayout->addWidget(toggleSwitch);
    } else {
        qCWarning(log_ui_mainwindow) << "Corner widget layout is not a QHBoxLayout. Unable to add ToggleSwitch.";
    }

    // load the settings
    qDebug() << "Loading settings";
    GlobalSetting::instance().loadLogSettings();
    GlobalSetting::instance().loadVideoSettings();
    LogHandler::instance().enableLogStore();

    qCDebug(log_ui_mainwindow) << "Observe switch usb connection trigger...";
    connect(ui->actionTo_Host, &QAction::triggered, this, &Camera::onActionSwitchToHostTriggered);
    connect(ui->actionTo_Target, &QAction::triggered, this, &Camera::onActionSwitchToTargetTriggered);

    qCDebug(log_ui_mainwindow) << "Observe action paste from host...";
    connect(ui->actionPaste, &QAction::triggered, this, &Camera::onActionPasteToTarget);
    connect(ui->pasteButton, &QPushButton::released, this, &Camera::onActionPasteToTarget);

    connect(ui->screensaverButton, &QPushButton::released, this, &Camera::onActionScreensaver);

    connect(ui->virtualKeyboardButton, &QPushButton::released, this, &Camera::onToggleVirtualKeyboard);

    qDebug() << "Init...";
    init();

    qDebug() << "Init status bar...";
    initStatusBar();

    addToolBar(Qt::TopToolBarArea, toolbarManager->getToolbar());
    toolbarManager->getToolbar()->setVisible(false);

    connect(toolbarManager, &ToolbarManager::functionKeyPressed, this, &Camera::onFunctionKeyPressed);
    connect(toolbarManager, &ToolbarManager::ctrlAltDelPressed, this, &Camera::onCtrlAltDelPressed);
    connect(toolbarManager, &ToolbarManager::repeatingKeystrokeChanged, this, &Camera::onRepeatingKeystrokeChanged);
    connect(toolbarManager, &ToolbarManager::specialKeyPressed, this, &Camera::onSpecialKeyPressed);
}

void Camera::init()
{
    qCDebug(log_ui_mainwindow) << "Camera init...";
#ifdef QT_FEATURE_permissions //Permissions API not compatible with Qt < 6.5 and will cause compilation failure on expanding macro in qtconfigmacros.h
#if QT_CONFIG(permissions)
    // camera
    QCameraPermission cameraPermission;
    switch (qApp->checkPermission(cameraPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(cameraPermission, this, &Camera::init);
        return;
    case Qt::PermissionStatus::Denied:
        qWarning("Camera permission is not granted!");
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
    // microphone
    QMicrophonePermission microphonePermission;
    switch (qApp->checkPermission(microphonePermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(microphonePermission, this, &Camera::init);
        return;
    case Qt::PermissionStatus::Denied:
        qWarning("Microphone permission is not granted!");
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif
#endif
    // Camera devices:
    updateCameras();

    loadCameraSettingAndSetCamera();

    GlobalVar::instance().setWinWidth(this->width());
    GlobalVar::instance().setWinHeight(this->height());

    // Initialize the virtual keyboard button icon
    QIcon icon(":/images/keyboard-down.svg");
    ui->virtualKeyboardButton->setIcon(icon);

    // Add this after other menu connections
    connect(ui->menuBaudrate, &QMenu::triggered, this, &Camera::onBaudrateMenuTriggered);
    connect(&SerialPortManager::getInstance(), &SerialPortManager::connectedPortChanged, this, &Camera::onPortConnected);
}

void Camera::initStatusBar()
{
    qCDebug(log_ui_mainwindow) << "Init status bar...";

    // Create a QLabel to hold the SVG icon
    mouseLabel = new QLabel(this);
    mouseLocationLabel = new QLabel(QString("(0,0)"), this);
    mouseLocationLabel->setFixedWidth(80);

    // Mouse container widget
    QWidget *mouseContainer = new QWidget(this);
    QHBoxLayout *mouseLayout = new QHBoxLayout(mouseContainer);

    mouseLayout->setContentsMargins(0, 0, 0, 0); // Remove margins
    mouseLayout->addWidget(mouseLabel);
    mouseLayout->addWidget(mouseLocationLabel);
    ui->statusbar->addWidget(mouseContainer);

    onLastMouseLocation(QPoint(0, 0), nullptr);
    keyPressedLabel = new QLabel(this);
    keyLabel = new QLabel(this);
    keyLabel->setFixedWidth(18);
    // Key container widget
    QWidget *keyContainer = new QWidget(this);
    QHBoxLayout *keyLayout = new QHBoxLayout(keyContainer);
    keyLayout->setContentsMargins(0, 0, 0, 0); // Remove margins
    keyLayout->addWidget(keyPressedLabel);
    keyLayout->addWidget(keyLabel);
    ui->statusbar->addWidget(keyContainer);

    onLastKeyPressed("");
}

void Camera::loadCameraSettingAndSetCamera(){
    QSettings settings("Techxartisan", "Openterface");
    QString deviceDescription = settings.value("camera/device", "Openterface").toString();
    const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        qDebug() << "No video input devices found.";
    } else {
        for (const QCameraDevice &cameraDevice : devices) {
            if (cameraDevice.description() == deviceDescription) {
                setCamera(cameraDevice);
                break;
            }
        }
    }
}

void Camera::setCamera(const QCameraDevice &cameraDevice)
{
    // if(cameraDevice.description().contains("Openterface") == false){
    // qCDebug(log_ui_mainwindow) << "The camera("<<cameraDevice.description()<<") is not an Openterface Mini-KVM, skip it.";
    // return;
    // }
    qCDebug(log_ui_mainwindow) << "Set Camera, device name: " << cameraDevice.description();

    m_camera.reset(new QCamera(cameraDevice));
    m_captureSession.setCamera(m_camera.get());

    connect(m_camera.get(), &QCamera::activeChanged, this, &Camera::updateCameraActive);
    connect(m_camera.get(), &QCamera::errorOccurred, this, &Camera::displayCameraError);
    qCDebug(log_ui_mainwindow) << "Observe congigure setting";


    queryResolutions();

    m_captureSession.setVideoOutput(this->videoPane);
    qCDebug(log_ui_mainwindow) << "Camera start..";
    m_camera->start();

    VideoHid::getInstance().start();
}

void Camera::queryResolutions()
{
    QPair<int, int> resolution = VideoHid::getInstance().getResolution();
    qCDebug(log_ui_mainwindow) << "Input resolution: " << resolution;
    GlobalVar::instance().setInputWidth(resolution.first);
    GlobalVar::instance().setInputHeight(resolution.second);
    video_width = GlobalVar::instance().getCaptureWidth();
    video_height = GlobalVar::instance().getCaptureHeight();

    float input_fps = VideoHid::getInstance().getFps();
    updateResolutions(resolution.first, resolution.second, input_fps, video_width, video_height, GlobalVar::instance().getCaptureFps());
}

void Camera::resizeEvent(QResizeEvent *event) {
    qCDebug(log_ui_mainwindow) << "Handle window resize event.";
    QMainWindow::resizeEvent(event);  // Call base class implementation

    // Define the desired aspect ratio
    qreal aspect_ratio = static_cast<qreal>(video_width) / video_height;

    int titleBarHeight = this->frameGeometry().height() - this->geometry().height();
    qCDebug(log_ui_mainwindow) << "Aspect ratio:" << aspect_ratio << ", Width:" << video_width << "Height:" << video_height;
    qCDebug(log_ui_mainwindow) << "menuBar height:" << this->menuBar()->height() << ", statusbar height:" << ui->statusbar->height() << ", titleBarHeight" << titleBarHeight;

    // Calculate the new height based on the width and the aspect ratio
    // int new_width = static_cast<int>((height() -  this->menuBar()->height() - ui->statusbar->height()) * aspect_ratio);
    int new_height = static_cast<int>(width() / aspect_ratio) + this->menuBar()->height() + ui->statusbar->height();

    // Set the new size of the window
    qCDebug(log_ui_mainwindow) << "Resize to " << width() << "x" << new_height;
    resize(width(), new_height);

    GlobalVar::instance().setWinWidth(this->width());
    GlobalVar::instance().setWinHeight(this->height());
}


void Camera::moveEvent(QMoveEvent *event) {
    // Get the old and new positions
    QPoint oldPos = event->oldPos();
    QPoint newPos = event->pos();

    // Calculate the position delta
    QPoint delta = newPos - oldPos;

    qCDebug(log_ui_mainwindow) << "Window move delta: " << delta;

    // Call the base class implementation
    QWidget::moveEvent(event);

    //calculate_video_position();
}

void Camera::calculate_video_position(){

    double aspect_ratio = static_cast<double>(video_width) / video_height;

    int scaled_window_width, scaled_window_height;
    int titleBarHeight = this->frameGeometry().height() - this->geometry().height();
    int statusBarHeight = ui->statusbar->height();
    QMenuBar *menuBar = this->menuBar();
    int menuBarHeight = menuBar->height();

    double widget_ratio = static_cast<double>(width()) / (height()-titleBarHeight-statusBarHeight-menuBarHeight);

    qCDebug(log_ui_mainwindow) << "titleBarHeight: " << titleBarHeight;
    qCDebug(log_ui_mainwindow) << "statusBarHeight: " << statusBarHeight;
    qCDebug(log_ui_mainwindow) << "menuBarHeight: " << menuBarHeight;

    if (widget_ratio < aspect_ratio) {
        // Window is relatively shorter, scale the window by video width
        scaled_window_width =  static_cast<int>(ui->centralwidget->height() * aspect_ratio);
        scaled_window_height = ui->centralwidget->height() + titleBarHeight + statusBarHeight+menuBarHeight;
    } else {
        // Window is relatively taller, scale the window by video height
        scaled_window_width = ui->centralwidget->width();
        scaled_window_height =static_cast<int>(ui->centralwidget->width()) / aspect_ratio + titleBarHeight + statusBarHeight+menuBarHeight;
    }
    resize(scaled_window_width, scaled_window_height);

    GlobalVar::instance().setMenuHeight(menuBarHeight);
    GlobalVar::instance().setTitleHeight(titleBarHeight);
    GlobalVar::instance().setStatusbarHeight(statusBarHeight);
    QSize windowSize = this->size();
    GlobalVar::instance().setWinWidth(windowSize.width());
    GlobalVar::instance().setWinHeight(windowSize.height());
}


void Camera::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_CameraFocus:
        displayViewfinder();
        event->accept();
        break;
    case Qt::Key_Camera:
        if (m_doImageCapture) {
            takeImage();
        } else {
            if (m_mediaRecorder->recorderState() == QMediaRecorder::RecordingState)
                stop();
            else
                record();
        }
        event->accept();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void Camera::onActionRelativeTriggered()
{
    QPoint globalPosition = videoPane->mapToGlobal(QPoint(0, 0));

    QRect globalGeometry = QRect(globalPosition, videoPane->geometry().size());

    // move the mouse to window center
    QPoint center = globalGeometry.center();
    QCursor::setPos(center);

    GlobalVar::instance().setAbsoluteMouseMode(false);
    videoPane->hideHostMouse();

    this->popupMessage("Long press ESC to exit.");
}

void Camera::onActionAbsoluteTriggered()
{
    GlobalVar::instance().setAbsoluteMouseMode(true);
}

void Camera::onActionResetHIDTriggered()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(this, "Confirm Reset Keyboard and Mouse?",
                                        "Resetting the Keyboard & Mouse chip will apply new settings. Do you want to proceed?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        qCDebug(log_ui_mainwindow) << "onActionResetHIDTriggered";
        HostManager::getInstance().resetHid();
    } else {
        qCDebug(log_ui_mainwindow) << "Reset HID canceled by user.";
    }
}

void Camera::onActionFactoryResetHIDTriggered()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(this, "Confirm Factory Reset HID Chip?",
                                        "Factory reset the HID chip. Proceed?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        qCDebug(log_ui_mainwindow) << "onActionFactoryResetHIDTriggered";
        SerialPortManager::getInstance().factoryResetHipChip();
        // HostManager::getInstance().resetHid();
    } else {
        qCDebug(log_ui_mainwindow) << "Factory reset HID chip canceled by user.";
    }
}

void Camera::onActionResetSerialPortTriggered()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Reset Serial Port?",
                                        "Resetting the serial port will close and re-open it without changing settings. Proceed?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        qCDebug(log_ui_mainwindow) << "onActionResetSerialPortTriggered";
        HostManager::getInstance().resetSerialPort();
    } else {
        qCDebug(log_ui_mainwindow) << "Serial port reset canceled by user.";
    }
}

void Camera::onActionSwitchToHostTriggered()
{
    qCDebug(log_ui_mainwindow) << "Switchable USB to host...";
    VideoHid::getInstance().switchToHost();
    ui->actionTo_Host->setChecked(true);
    ui->actionTo_Target->setChecked(false);
}

void Camera::onActionSwitchToTargetTriggered()
{
    qCDebug(log_ui_mainwindow) << "Switchable USB to target...";
    VideoHid::getInstance().switchToTarget();
    ui->actionTo_Host->setChecked(false);
    ui->actionTo_Target->setChecked(true);
}

void Camera::onToggleSwitchStateChanged(int state)
{
    qCDebug(log_ui_mainwindow) << "Toggle switch state changed to:" << state;
    if (state == Qt::Checked) {
        onActionSwitchToTargetTriggered();
    } else {
        onActionSwitchToHostTriggered();
    }
}

void Camera::onResolutionChange(const int& width, const int& height, const float& fps)
{
    GlobalVar::instance().setInputWidth(width);
    GlobalVar::instance().setInputHeight(height);
    statusWidget->setInputResolution(width, height, fps);
}

void Camera::onTargetUsbConnected(const bool isConnected)
{
    statusWidget->setTargetUsbConnected(isConnected);
}

void Camera::updateBaudrateMenu(int baudrate){
    // Find the QAction corresponding to the current baudrate and check it
    QList<QAction*> actions = ui->menuBaudrate->actions();
    for (QAction* action : actions) {
        bool ok;
        int actionBaudrate = action->text().toInt(&ok);
        if (ok && actionBaudrate == baudrate) {
            action->setChecked(true);
        } else {
            action->setChecked(false);
        }
    }

    // If the current baudrate is not in the menu, add a new option and check it
    bool baudrateFound = false;
    for (QAction* action : actions) {
        if (action->text().toInt() == baudrate) {
            baudrateFound = true;
            break;
        }
    }
    if (!baudrateFound) {
        QAction* newAction = new QAction(QString::number(baudrate), this);
        newAction->setCheckable(true);
        newAction->setChecked(true);
        ui->menuBaudrate->addAction(newAction);
        connect(newAction, &QAction::triggered, this, [this, newAction]() {
            onBaudrateMenuTriggered(newAction);
        });
    }
}

void Camera::onActionPasteToTarget()
{
    HostManager::getInstance().pasteTextToTarget(QGuiApplication::clipboard()->text());
}

void Camera::onActionScreensaver()
{
    HostManager::getInstance().autoMoveMouse();
}

void Camera::onToggleVirtualKeyboard()
{
    bool isVisible = toolbarManager->getToolbar()->isVisible();
    toolbarManager->getToolbar()->setVisible(!isVisible);

    // Toggle the icon
    QString iconPath = isVisible ? ":/images/keyboard-down.svg" : ":/images/keyboard-up.svg";
    QIcon icon(iconPath);
    ui->virtualKeyboardButton->setIcon(icon);
}

void Camera::popupMessage(QString message)
{
    QDialog dialog;
    dialog.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    QVBoxLayout layout;
    dialog.setLayout(&layout);

    // Set the font of the message box
    QFont font;
    font.setPointSize(18); // Set the size of the font
    font.setBold(true); // Make the font bold

    QLabel label(message);
    label.setFont(font); // Use the same font as before
    layout.addWidget(&label);

    dialog.adjustSize(); // Resize the dialog to fit its content

    // Show the dialog off-screen
    dialog.move(-1000, -1000);
    dialog.show();

    // Now that the dialog is shown, we can get its correct dimensions
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    int x = screenGeometry.width() - dialog.frameGeometry().width();
    int y = 0;
    qCDebug(log_ui_mainwindow) << "x: " << x << "y:" << y;

    // Move the dialog to the desired position
    dialog.move(x, y);

    // Auto hide in 3 seconds
    QTimer::singleShot(3000, &dialog, &QDialog::accept);
    dialog.exec();
}

void Camera::updateCameraActive(bool active) {
    qCDebug(log_ui_mainwindow) << "Camera active: " << active;
    if(active){
        qCDebug(log_ui_mainwindow) << "Set index to : " << 1;
        stackedLayout->setCurrentIndex(1);
    }else {
        qCDebug(log_ui_mainwindow) << "Set index to : " << 0;
        stackedLayout->setCurrentIndex(0);
    }
    queryResolutions();
}

void Camera::updateRecordTime()
{
    QString str = tr("Recorded %1 sec").arg(m_mediaRecorder->duration() / 1000);
    ui->statusbar->showMessage(str);
}

void Camera::processCapturedImage(int requestId, const QImage &img)
{
    Q_UNUSED(requestId);
    QImage scaledImage =
            img.scaled(ui->centralwidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

   // ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImage));

    // Display captured image for 4 seconds.
    displayCapturedImage();
    QTimer::singleShot(4000, this, &Camera::displayViewfinder);
}

// void Camera::configureCaptureSettings()
// {
//     // if (m_doImageCapture)
//     //     configureImageSettings();
//     // else
//     configureVideoSettings();

// }

// void Camera::configureVideoSettings()
// {
//     VideoSettings settingsDialog(m_camera.data());

//     if (settingsDialog.exec())
//         settingsDialog.applySettings();
// }

// void Camera::configureImageSettings()
// {
//     ImageSettings settingsDialog(m_imageCapture.get());

//     if (settingsDialog.exec() == QDialog::Accepted)
//         settingsDialog.applyImageSettings();
// }

void Camera::configureSettings() {
    qDebug() << "configureSettings";
    qDebug() << "settingsDialog: " << settingsDialog;
    if (!settingsDialog){
        qDebug() << "Creating settings dialog";
        settingsDialog = new SettingDialog(m_camera.data(), this);
        connect(settingsDialog, &SettingDialog::cameraSettingsApplied, this, &Camera::loadCameraSettingAndSetCamera);
        // connect the finished signal to the set the dialog pointer to nullptr
        connect(settingsDialog, &QDialog::finished, this, [this](){
            settingsDialog = nullptr;
        });
        settingsDialog->show();
    }else{
        settingsDialog->raise();
        settingsDialog->activateWindow();
    }
}

void Camera::debugSerialPort() {
    qDebug() << "debug dialog" ;
    qDebug() << "serialPortDebugDialog: " << serialPortDebugDialog;
    if (!serialPortDebugDialog){
        qDebug() << "Creating serial port debug dialog";
        serialPortDebugDialog = new SerialPortDebugDialog();
        // connect the finished signal to the set the dialog pointer to nullptr
        connect(serialPortDebugDialog, &QDialog::finished, this, [this]() {
            serialPortDebugDialog = nullptr;
        });
        serialPortDebugDialog->show();
    }else{
        serialPortDebugDialog->raise();
        serialPortDebugDialog->activateWindow();
    }
}

void Camera::purchaseLink(){
    QDesktopServices::openUrl(QUrl("https://www.crowdsupply.com/techxartisan/openterface-mini-kvm"));
}

void Camera::feedbackLink(){
    QDesktopServices::openUrl(QUrl("https://forms.gle/KNQPTNfXCPUPybgG9"));
}

void Camera::aboutLink(){
    QDesktopServices::openUrl(QUrl("https://openterface.com/"));
}

void Camera::versionInfo() {
    QString applicationName = QApplication::applicationName();
    QString organizationName = QApplication::organizationName();
    QString applicationVersion = QApplication::applicationVersion();
    QString osVersion = QSysInfo::prettyProductName();
    QString title = tr("%1").arg(applicationName);
    QString message = tr("Version:\t %1 \nQT:\t %2\nOS:\t %3")
                          .arg(applicationVersion)
                          .arg(qVersion())
                          .arg(osVersion);

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(message);

    QPushButton *copyButton = msgBox.addButton(tr("Copy"), QMessageBox::ActionRole);
    QPushButton *closeButton = msgBox.addButton(QMessageBox::Close);

    connect(copyButton, &QPushButton::clicked, this, &Camera::copyToClipboard);

    msgBox.exec();

}

void Camera::copyToClipboard(){
    QString applicationName = QApplication::applicationName();
    QString organizationName = QApplication::organizationName();
    QString applicationVersion = QApplication::applicationVersion();

    QString message = tr("Version:\t %1 \nOrganization:\t %2")
                          .arg(applicationVersion)
                          .arg(organizationName);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(message);
}

void Camera::onFunctionKeyPressed(int key)
{
    HostManager::getInstance().handleFunctionKey(key);
}

void Camera::onCtrlAltDelPressed()
{
    HostManager::getInstance().sendCtrlAltDel();
}

void Camera::onRepeatingKeystrokeChanged(int interval)
{
    HostManager::getInstance().setRepeatingKeystroke(interval);
}

void Camera::record()
{
    m_mediaRecorder->record();
    updateRecordTime();
}

void Camera::pause()
{
    m_mediaRecorder->pause();
}

void Camera::setMuted(bool muted)
{
   // m_captureSession.audioInput()->setMuted(muted);
}

void Camera::takeImage()
{
    m_isCapturingImage = true;
    m_imageCapture->captureToFile();
}

void Camera::displayCaptureError(int id, const QImageCapture::Error error,
                                 const QString &errorString)
{
    Q_UNUSED(id);
    Q_UNUSED(error);
    QMessageBox::warning(this, tr("Image Capture Error"), errorString);
    m_isCapturingImage = false;
}

void Camera::setExposureCompensation(int index)
{
    m_camera->setExposureCompensation(index * 0.5);
}


void Camera::displayCameraError()
{
    qWarning() << "Camera error: " << m_camera->errorString();
    if (m_camera->error() != QCamera::NoError){
        qCDebug(log_ui_mainwindow) << "A camera has been disconnected.";

        stackedLayout->setCurrentIndex(0);

        stop();
    }
}

void Camera::stop(){
    qDebug() << "Stop camera data...";
    disconnect(m_camera.data());
    qDebug() << "Camera data stopped.";
    m_audioManager->disconnect();
    qDebug() << "Audio manager stopped.";

    m_captureSession.disconnect();
    qDebug() << "Capture session stopped.";
    VideoHid::getInstance().stop();
    qDebug() << "Video HID stopped.";
    m_camera->stop();
    qDebug() << "Camera stopped.";
}

void Camera::updateCameraDevice(QAction *action)
{
    setCamera(qvariant_cast<QCameraDevice>(action->data()));
}

void Camera::displayViewfinder()
{
    //ui->stackedWidget->setCurrentIndex(0);
}

void Camera::displayCapturedImage()
{
    //ui->stackedWidget->setCurrentIndex(1);
}

void Camera::onBaudrateMenuTriggered(QAction* action)
{
    bool ok;
    int baudrate = action->text().toInt(&ok);
    if (ok) {
        SerialPortManager::getInstance().setBaudRate(baudrate);
    }
}

void Camera::onSpecialKeyPressed(const QString &keyText)
{
    // Handle the special key press
    // For example, you might want to send this key to the remote desktop connection
    if (keyText == ToolbarManager::KEY_ESC) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Escape);
    } else if (keyText == ToolbarManager::KEY_INS) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Insert);
    } else if (keyText == ToolbarManager::KEY_DEL) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Delete);
    } else if (keyText == ToolbarManager::KEY_HOME) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Home);
    } else if (keyText == ToolbarManager::KEY_END) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_End);
    } else if (keyText == ToolbarManager::KEY_PGUP) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_PageUp);
    } else if (keyText == ToolbarManager::KEY_PGDN) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_PageDown);
    } else if (keyText == ToolbarManager::KEY_PRTSC) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Print);
    } else if (keyText == ToolbarManager::KEY_SCRLK) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_ScrollLock);
    } else if (keyText == ToolbarManager::KEY_PAUSE) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Pause);
    } else if (keyText == ToolbarManager::KEY_NUMLK) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_NumLock);
    } else if (keyText == ToolbarManager::KEY_CAPSLK) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_CapsLock);
    } else if (keyText == ToolbarManager::KEY_WIN) {
        HostManager::getInstance().handleFunctionKey(Qt::Key_Meta);
    }
}

void Camera::imageSaved(int id, const QString &fileName)
{
    Q_UNUSED(id);
    ui->statusbar->showMessage(tr("Captured \"%1\"").arg(QDir::toNativeSeparators(fileName)));

    m_isCapturingImage = false;
    if (m_applicationExiting)
        close();
}

void Camera::closeEvent(QCloseEvent *event)
{
    if (m_isCapturingImage) {
        setEnabled(false);
        m_applicationExiting = true;
        event->ignore();
    } else {
        event->accept();
    }
}

void Camera::updateCameras()
{
    qCDebug(log_ui_mainwindow) << "Update cameras...";
    // ui->menuSource->clear();
    const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();

    for (const QCameraDevice &camera : availableCameras) {
        if (!m_lastCameraList.contains(camera)) {
            qCDebug(log_ui_mainwindow) << "A new camera has been connected:" << camera.description();

            if (!camera.description().contains("Openterface"))
                continue;

            qCDebug(log_ui_mainwindow) << "Update openterface layer to top layer.";
            stackedLayout->setCurrentIndex(1);

            //If the default camera is not an Openterface camera, set the camera to the first Openterface camera
            if (!QMediaDevices::defaultVideoInput().description().contains("Openterface")) {
                qCDebug(log_ui_mainwindow) << "Set defualt camera to the Openterface camera...";
            } else {
                qCDebug(log_ui_mainwindow) << "The default camera is" << QMediaDevices::defaultVideoInput().description();
            }
            m_audioManager->initializeAudio();
            setCamera(camera);
            break;
        }
    }
}

void Camera::checkCameraConnection()
{
    const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();

    if (availableCameras != m_lastCameraList) {
        // The list of available cameras has changed
        if (availableCameras.count() > m_lastCameraList.count()) {
            // A new camera has been connected
            // Find out which camera was connected
        }
        m_lastCameraList = availableCameras;
    }
}


void Camera::onPortConnected(const QString& port, const int& baudrate) {
    statusWidget->setConnectedPort(port, baudrate);
    updateBaudrateMenu(baudrate);
}

void Camera::onStatusUpdate(const QString& status) {
    statusWidget->setStatusUpdate(status);
}

void Camera::onLastKeyPressed(const QString& key) {
    QString svgPath;
    if(key == ""){
        svgPath = QString(":/images/keyboard.svg");
    }else{
        svgPath = QString(":/images/keyboard-pressed.svg");
    }

    // Load the SVG into a QPixmap
    QSvgRenderer svgRenderer(svgPath);
    QPixmap pixmap(18, 18); // Adjust the size as needed
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    svgRenderer.render(&painter);

    // Set the QPixmap to the QLabel
    keyPressedLabel->setPixmap(pixmap);
    keyLabel->setText(QString("%1").arg(key));
}

void Camera::onLastMouseLocation(const QPoint& location, const QString& mouseEvent) {
    // Load the SVG into a QPixmap
    QString svgPath;
    if (mouseEvent == "L") {
        svgPath = ":/images/mouse-left-button.svg";
    } else if (mouseEvent == "R") {
        svgPath = ":/images/mouse-right-button.svg";
    } else if (mouseEvent == "M") {
        svgPath = ":/images/mouse-middle-button.svg";
    } else {
        svgPath = ":/images/mouse-default.svg";
    }

    // Load the SVG into a QPixmap
    QSvgRenderer svgRenderer(svgPath);
    QPixmap pixmap(12, 12); // Adjust the size as needed
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    svgRenderer.render(&painter);

    // Set the QPixmap to the QLabel
    mouseLabel->setPixmap(pixmap);
    mouseLocationLabel->setText(QString("(%1,%2)").arg(location.x()).arg(location.y()));
}

void Camera::onSwitchableUsbToggle(const bool isToTarget) {
    if (isToTarget) {
        qDebug() << "UI Switchable USB to target...";
        ui->actionTo_Host->setChecked(false);
        ui->actionTo_Target->setChecked(true);
        toggleSwitch->setChecked(true);
    } else {
        qDebug() << "UI Switchable USB to host...";
        ui->actionTo_Host->setChecked(true);
        ui->actionTo_Target->setChecked(false);
        toggleSwitch->setChecked(false);
    }
    SerialPortManager::getInstance().restartSwitchableUSB();
}

void Camera::updateResolutions(const int input_width, const int input_height, const float input_fps, const int capture_width, const int capture_height, const int capture_fps)
{
    statusWidget->setInputResolution(input_width, input_height, input_fps);
    statusWidget->setCaptureResolution(capture_width, capture_height, capture_fps);
}
