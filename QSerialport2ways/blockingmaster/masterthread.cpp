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

#include "masterthread.h"

#include <QSerialPort>
#include <QTime>

#include <QDebug>

MasterThread::MasterThread(QObject *parent) :
    QThread(parent)
{
}

//! [0]
MasterThread::~MasterThread()
{
    m_mutex.lock();
    //--临界资源

    //--析构退出while循环
    m_quit = true;

    //--唤醒等待条件下休眠的线程
    //--从阻塞的地方(m_cond.wait(&m_mutex);)继续执行
    m_cond.wakeOne();

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
void MasterThread::transaction(const QString &portName, int waitTimeout, const QString &request)
{

    qDebug()<<Q_FUNC_INFO<<QThread::currentThread();

//! [1]
    const QMutexLocker locker(&m_mutex);
    m_portName = portName;
    m_waitTimeout = waitTimeout;
    m_request = request;
//! [3]
    if (!isRunning()){
        start();
    }else{
        //--唤醒等待条件下休眠的线程
        //--从阻塞的地方(m_cond.wait(&m_mutex);)继续执行
        m_cond.wakeOne();
    }
}
//! [2] //! [3]

//! [4]
void MasterThread::run()
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
    QString currentRequest = m_request;
    m_mutex.unlock();

//! [5] //! [6]
    QSerialPort serial;

    if (currentPortName.isEmpty()) {
        emit error(tr("No port name specified"));
        return;
    }

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
//! [7] //! [8]
        // write request
        //--向串口写请求
        const QByteArray requestData = currentRequest.toUtf8();
        serial.write(requestData);

        //对于阻塞传输，应该在每次写方法write()调用之后使用waitforbyteswrite()方法。这将处理所有的I/O例程，而不是Qt事件循环。
        //这个函数会阻塞，直到至少有一个字节被写入串口并发出byteswrite()信号。
        //函数超时时间毫秒;默认超时时间为30000毫秒。如果msecs是-1，这个函数就不会超时。
        //如果发出byteswrite()信号，函数返回true;否则返回false(如果发生错误或操作超时)。
        if (serial.waitForBytesWritten(m_waitTimeout)) {
//! [8] //! [10]
            // read response
            //--读取串口响应
            //对于阻塞传输，应该在每次read()调用之前使用waitForReadyRead()方法。这将处理所有I/O例程，而不是Qt事件循环。
            //如果接收数据时发生超时错误，将发出timeout()信号。
            //这个函数会阻塞，直到有新的数据可读，并发出readyRead()信号。函数超时时间超过毫秒;默认超时时间为30000毫秒。如果msecs是-1，这个函数就不会超时。
            //如果发出readyRead()信号，并且有新的数据可供读取，则函数返回true;否则返回false(如果发生错误或操作超时)。
            if (serial.waitForReadyRead(currentWaitTimeout)) {
                QByteArray responseData = serial.readAll();
                while (serial.waitForReadyRead(10))
                    responseData += serial.readAll();

                const QString response = QString::fromUtf8(responseData);
//! [12]
                emit this->response(response);
//! [10] //! [11] //! [12]
            } else {
                emit timeout(tr("Wait read response timeout %1")
                             .arg(QTime::currentTime().toString()));
            }
//! [9] //! [11]
        } else {
            emit timeout(tr("Wait write request timeout %1")
                         .arg(QTime::currentTime().toString()));
        }
//! [9]  //! [13]
        //--线程进入休眠状态，直到下一个事务出现。线程在使用成员唤醒后读取新数据，并从头开始运行循环。
        //m_cond.wait(&m_mutex)释放锁住互斥锁，并在等待条件下等待。锁住互斥锁必须首先被调用线程锁住m_mutex.lock()。
        //如果锁住互斥锁没有处于锁定状态，则该行为是未定义的。如果锁互斥是一个递归互斥，这个函数立即返回。
        //--锁住互斥锁将被解锁，调用线程将阻塞，直到满足以下条件之一:
        //1 另一个线程使用wakeOne()或wakeAll()发出信号。在这种情况下，这个函数将返回true。
        //2 时间已经过了几毫秒。如果时间是ULONG_MAX(默认值)，那么等待将永远不会超时(必须通知事件)。如果等待超时，此函数将返回false。
        //  锁住互斥锁将返回到相同的锁态。wait()此函数是为了提供从锁定状态转换到等待状态的原子性。
        m_mutex.lock();
        m_cond.wait(&m_mutex);
        //--记忆当前配置
        if (currentPortName != m_portName) {
            currentPortName = m_portName;
            currentPortNameChanged = true;
        } else {
            currentPortNameChanged = false;
        }
        currentWaitTimeout = m_waitTimeout;
        currentRequest = m_request;
        m_mutex.unlock();
    }
//! [13]
}
