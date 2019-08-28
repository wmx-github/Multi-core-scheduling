/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "slavethread.h"

#include <QSerialPort>
#include <QTime>

#include <QDebug>

SlaveThread::SlaveThread(QObject *parent) :
    QThread(parent)
{
}

//! [0]
SlaveThread::~SlaveThread()
{
    m_mutex.lock();
    //--临界资源

    //--析构退出while循环
    m_quit = true;

    m_mutex.unlock();

    /*!
     * @brief
     * 阻塞线程，直到满足以下条件之一:
     * 1 与此QThread对象关联的线程已经完成执行(即当它从run()返回时)。如果线程已经完成，这个函数将返回true。如果线程尚未启动，则返回true。
     * 2 时间已经过了几毫秒。如果时间是ULONG_MAX(默认值)，那么等待将永远不会超时(线程必须从run()返回)。如果等待超时，此函数将返回false。
     * 这提供了与POSIX pthread_join()函数类似的功能。
     */
    wait();
}
//! [0]

//! [1] //! [2]
void SlaveThread::startSlave(const QString &portName, int waitTimeout, const QString &response)
{
    qDebug()<<Q_FUNC_INFO<<QThread::currentThread();

    //! [1]
    const QMutexLocker locker(&m_mutex);
    m_portName = portName;
    m_waitTimeout = waitTimeout;
    m_response = response;
    //! [3]
    if (!isRunning())
        start();
}
//! [2] //! [3]

//! [4]
void SlaveThread::run()
{
    qDebug()<<Q_FUNC_INFO<<QThread::currentThread();

    bool currentPortNameChanged = false;

    //--加锁配置临界区数据
    m_mutex.lock();
//! [4] //! [5]
    QString currentPortName;
    if (currentPortName != m_portName) {
        currentPortName = m_portName;
        currentPortNameChanged = true;
    }

    int currentWaitTimeout = m_waitTimeout;
    QString currentRespone = m_response;
    m_mutex.unlock();
//! [5] //! [6]
    QSerialPort serial;

    while (!m_quit) {
//![6] //! [7]
        //--串口改变，关闭原串口，打开新串口
        if (currentPortNameChanged) {
            serial.close();
            serial.setPortName(currentPortName);

            if (!serial.open(QIODevice::ReadWrite)) {
                emit error(tr("Can't open %1, error code %2")
                           .arg(m_portName).arg(serial.error()));
                return;
            }
        }

        //--读取串口请求
        //对于阻塞传输，应该在每次read()调用之前使用waitForReadyRead()方法。这将处理所有I/O例程，而不是Qt事件循环。
        //如果接收数据时发生超时错误，将发出timeout()信号。
        //这个函数会阻塞，直到有新的数据可读，并发出readyRead()信号。函数超时时间超过毫秒;默认超时时间为30000毫秒。如果msecs是-1，这个函数就不会超时。
        //如果发出readyRead()信号，并且有新的数据可供读取，则函数返回true;否则返回false(如果发生错误或操作超时)。
        if (serial.waitForReadyRead(currentWaitTimeout)) {
//! [7] //! [8]
            // read request
            // 读请求(服务程序串口发来的)
            QByteArray requestData = serial.readAll();
            while (serial.waitForReadyRead(10))
                requestData += serial.readAll();
//! [8] //! [10]
            // write response
            // 写应答
            const QByteArray responseData = currentRespone.toUtf8();
            serial.write(responseData);

            //对于阻塞传输，应该在每次写方法write()调用之后使用waitforbyteswrite()方法。这将处理所有的I/O例程，而不是Qt事件循环。
            //这个函数会阻塞，直到至少有一个字节被写入串口并发出byteswrite()信号。
            //函数超时时间毫秒;默认超时时间为30000毫秒。如果msecs是-1，这个函数就不会超时。
            //如果发出byteswrite()信号，函数返回true;否则返回false(如果发生错误或操作超时)。
            if (serial.waitForBytesWritten(m_waitTimeout)) {
                const QString request = QString::fromUtf8(requestData);
//! [12]
                emit this->request(request);
//! [10] //! [11] //! [12]
            } else {
                emit timeout(tr("Wait write response timeout %1")
                             .arg(QTime::currentTime().toString()));
            }
//! [9] //! [11]
        } else {
            emit timeout(tr("Wait read request timeout %1")
                         .arg(QTime::currentTime().toString()));
        }
//! [9]  //! [13]

        //--加锁配置临界区数据
        //--记忆当前配置
        m_mutex.lock();
        if (currentPortName != m_portName) {
            currentPortName = m_portName;
            currentPortNameChanged = true;
        } else {
            currentPortNameChanged = false;
        }
        currentWaitTimeout = m_waitTimeout;
        currentRespone = m_response;
        m_mutex.unlock();
    }
//! [13]
}
