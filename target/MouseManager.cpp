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

#include "MouseManager.h"

Q_LOGGING_CATEGORY(log_core_mouse, "opf.host.mouse")

MouseManager::MouseManager(QObject *parent) : QObject(parent){
    // Initialization code here...
}

void MouseManager::handleAbsoluteMouseAction(int x, int y, int mouse_event, int wheelMovement) {
    // build a array
    QByteArray data;
    if (mouse_event > 0){
        qCDebug(log_core_mouse) << "mouse_event:" << mouse_event;
    }
    uint8_t mappedWheelMovement = mapScrollWheel(wheelMovement);
    if(mappedWheelMovement>0){    qCDebug(log_core_mouse) << "mappedWheelMovement:" << mappedWheelMovement; }
    data.append(SerialPortManager::MOUSE_ABS_ACTION_PREFIX);
    data.append(static_cast<char>(mouse_event));
    data.append(static_cast<char>(x & 0xFF));
    data.append(static_cast<char>((x >> 8) & 0xFF));
    data.append(static_cast<char>(y & 0xFF));
    data.append(static_cast<char>((y >> 8) & 0xFF));
    data.append(static_cast<char>(mappedWheelMovement & 0xFF));

    // send the data to serial
    SerialPortManager::getInstance().sendCommand(data, false);
}

void MouseManager::handleRelativeMouseAction(int dx, int dy, int mouse_event, int wheelMovement) {
    // Handle relative mouse action here...
}

uint8_t MouseManager::mapScrollWheel(int delta){
    if(delta == 0){
        return 0;
    }else if(delta > 0){
        return uint8_t(delta / 100);
    }else{
        return 0xFF - uint8_t(-1*delta / 100)+1;
    }
}
