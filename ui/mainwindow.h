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

#ifndef CAMERA_H
#define CAMERA_H

#include "transwindow.h"
#include "serial/serialportevents.h"

#include <QAudioInput>
#include <QCamera>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaMetaData>
#include <QMediaRecorder>
#include <QScopedPointer>
#include <QVideoWidget>
#include <QMainWindow>
#include <QStackedLayout>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(log_ui_mainwindow)

QT_BEGIN_NAMESPACE
namespace Ui {
class Camera;
}
class QActionGroup;
QT_END_NAMESPACE

class MetaDataDialog;

class Camera : public QMainWindow, public SerialPortEventCallback
{
    Q_OBJECT

public:
    Camera();
    void calculate_video_position();
    void stop();

private slots:
    void init();

    void setCamera(const QCameraDevice &cameraDevice);

    void record();
    void pause();
    void setMuted(bool);

    void takeImage();
    void displayCaptureError(int, QImageCapture::Error, const QString &errorString);

    void configureCaptureSettings();
    void configureVideoSettings();
    void configureImageSettings();

    void displayRecorderError();
    void displayCameraError();

    void updateCameraDevice(QAction *action);

    void updateCameraActive(bool active);
    void setExposureCompensation(int index);

    void updateRecordTime();

    void processCapturedImage(int requestId, const QImage &img);

    void displayViewfinder();
    void displayCapturedImage();
    void imageSaved(int id, const QString &fileName);

    void updateCameras();
    void popupMessage(QString message);

    void onPortConnected(const QString& port) override;

    void onLastKeyPressed(const QString& key) override;

    void onLastMouseLocation(const QPoint& location) override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent *event) override;

    void checkCameraConnection();
    
    void onActionRelativeTriggered();
    void onActionResetHIDTriggered();
    void onActionResetSerialPortTriggered();
private:
    Ui::Camera *ui;
    TransWindow *transWindow;

    QVideoWidget *videoPane;
    QStackedLayout *stackedLayout;
    //QActionGroup *videoDevicesGroup = nullptr;

    QMediaDevices m_source;
    QScopedPointer<QImageCapture> m_imageCapture;
    QMediaCaptureSession m_captureSession;
    QScopedPointer<QCamera> m_camera;
    // QScopedPointer<QAudioInput> m_audioInput;
    QScopedPointer<QMediaRecorder> m_mediaRecorder;

    bool videoReady = false;
    bool m_isCapturingImage = false;
    bool m_applicationExiting = false;
    bool m_doImageCapture = true;
    int video_width = 1920;
    int video_height = 1080;
    QList<QCameraDevice> m_lastCameraList;

    MetaDataDialog *m_metaDataDialog = nullptr;
};

#endif
