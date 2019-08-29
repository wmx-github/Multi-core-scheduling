/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include "renderthread.h"

#include <QtWidgets>
#include <cmath>

//! [0]
RenderThread::RenderThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;

    //--初始化颜色空间
    for (int i = 0; i < ColormapSize; ++i)
        colormap[i] = rgbFromWaveLength(380.0 + (i * 400.0 / ColormapSize));
}
//! [0]

//! [1]
RenderThread::~RenderThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}
//! [1]

//! [2]
/*!
 * \brief 外部调用,启动渲染，传入渲染参数(QMutexLocker保护)
 * \param centerX       中心x
 * \param centerY       中心y
 * \param scaleFactor   缩放比例
 * \param resultSize    结果大小
 */
void RenderThread::render(double centerX, double centerY, double scaleFactor,
                          QSize resultSize)
{
    QMutexLocker locker(&mutex);

    this->centerX = centerX;
    this->centerY = centerY;
    this->scaleFactor = scaleFactor;
    this->resultSize = resultSize;

    if (!isRunning()) {//--启动
        start(LowPriority);
    } else {//--重启唤醒休眠的渲染线程
        restart = true;
        condition.wakeOne();
    }
}
//! [2]

//! [3]
void RenderThread::run()
{
    forever {

        //--mutex保护访问临界区资源
        //--传入参数
        mutex.lock();
        QSize resultSize = this->resultSize;
        double scaleFactor = this->scaleFactor;
        double centerX = this->centerX;
        double centerY = this->centerY;
        mutex.unlock();
//! [3]

//! [4]
        int halfWidth = resultSize.width() / 2;
//! [4] //! [5]
        int halfHeight = resultSize.height() / 2;

        //--QImage::Format_RGB32 存储使用32位RGB格式的图像(0xffrrggbb)透明度a最大
        QImage image(resultSize, QImage::Format_RGB32);

        ///-此数越大 图像精细度越高
        const int NumPasses = 8;
        int pass = 0;

        while (pass < NumPasses) {

            /// \brief 最大迭代
            const int MaxIterations = (1 << (2 * pass + 6)) + 32;

            /// \brief 幅值 Limit = 2^2 = 4
            const int Limit = 4;

            bool allBlack = true;


             //--逐行扫描,生成图像
            for (int y = -halfHeight; y < halfHeight; ++y) {

                //--重启
                if (restart)
                    break;

                //--终止
                if (abort)
                    return;

                //--读取当前行第一个像素
                uint *scanLine =
                        reinterpret_cast<uint *>(image.scanLine(y + halfHeight));

                ///--虚部
                double ay = centerY + (y * scaleFactor);

                //--每行逐个扫描像素
                //--单个像素满足复平面函数关系Mandelbrot 集
                for (int x = -halfWidth; x < halfWidth; ++x) {

                    ///--实部
                    double ax = centerX + (x * scaleFactor);

                    double a1 = ax;
                    double b1 = ay;

                    /// \brief 当前迭代次数
                    int numIterations = 0;


                    //--单个像素点处,迭代计算
                    //--Mandelbrot 集:
                    // Z(n+1)=(Zn)^2+C
                    // 复数Z = a + bi
                    // 连续计算MaxIterations次
                    //--求得满足Mandelbrot集合的 第MaxIterations 个点(直到z的幅值大于2)
                    do {

                        //--增加迭代次数
                        ++numIterations;

                        //-- Z(n+1)=(Zn)^2+C
                        //-- 复数Z = a + bi

                        //---迭代一次-----
                        //--实部
                        double a2 = (a1 * a1) - (b1 * b1) + ax;
                        //--虚部
                        double b2 = (2 * a1 * b1) + ay;

                        //--直到z的幅值大于2, Limit = 2^2,退出
                        if ((a2 * a2) + (b2 * b2) > Limit)
                            break;

                        //---继续迭代一次，记录上一次结果，用于下一次迭代-----
                        ++numIterations;
                        a1 = (a2 * a2) - (b2 * b2) + ax;
                        b1 = (2 * a2 * b2) + ay;

                        //--直到z的幅值大于2, Limit = 2^2,退出
                        if ((a1 * a1) + (b1 * b1) > Limit)
                            break;

                    } while (numIterations < MaxIterations);//--循环迭代直到次数大于限制或者z的幅值大于2


                    //--退出循环时,幅值大于2,颜色取值
                    //--轮廓外部彩色
                    if (numIterations < MaxIterations) {

                        //--设置此行当前位(x位置) 像素颜色,取颜色空间的值
                        *scanLine++ = colormap[numIterations % ColormapSize];
                        allBlack = false;
                    }

                    //--退出循环时,迭代到最大次数,颜色取值
                    //--轮廓内部黑色
                    else {
                        //--设置此行当前位(x位置) 像素颜色
                        *scanLine++ = qRgb(0, 0, 0);
                    }

                }//--for每行逐个扫描
            }//--for逐行扫描

            //--第一轮,并且当前图像全黑,从第５(pass=4)轮开始
            //--因为这种情况下pass = 0 1 2 3 是全黑
            //--在轮廓内部
            if (allBlack && pass == 0) {
                pass = 4;
            } else {//--包含轮廓,触发GUI线程绘制图像
                if (!restart)
                    emit renderedImage(image, scaleFactor);
//! [5] //! [6]
                ++pass;
            }

//! [6] //! [7]
        }//--while
//! [7]

//! [8]
        //--渲染完毕休眠等待condition.wakeOne()唤醒
        mutex.lock();
//! [8] //! [9]
        if (!restart)
            condition.wait(&mutex);
        restart = false;
        mutex.unlock();
    }
}
//! [9]

//! [10]
/*!
 * \brief 一个辅助函数，它将波长转换为与32位QImages兼容的RGB值。
 *        从构造函数中调用它，以使用令人满意的颜色初始化colormap数组。
 * \param wave
 * \return
 */
uint RenderThread::rgbFromWaveLength(double wave)
{
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;

    if (wave >= 380.0 && wave <= 440.0) {
        r = -1.0 * (wave - 440.0) / (440.0 - 380.0);
        b = 1.0;
    } else if (wave >= 440.0 && wave <= 490.0) {
        g = (wave - 440.0) / (490.0 - 440.0);
        b = 1.0;
    } else if (wave >= 490.0 && wave <= 510.0) {
        g = 1.0;
        b = -1.0 * (wave - 510.0) / (510.0 - 490.0);
    } else if (wave >= 510.0 && wave <= 580.0) {
        r = (wave - 510.0) / (580.0 - 510.0);
        g = 1.0;
    } else if (wave >= 580.0 && wave <= 645.0) {
        r = 1.0;
        g = -1.0 * (wave - 645.0) / (645.0 - 580.0);
    } else if (wave >= 645.0 && wave <= 780.0) {
        r = 1.0;
    }

    double s = 1.0;
    if (wave > 700.0)
        s = 0.3 + 0.7 * (780.0 - wave) / (780.0 - 700.0);
    else if (wave <  420.0)
        s = 0.3 + 0.7 * (wave - 380.0) / (420.0 - 380.0);

    r = std::pow(r * s, 0.8);
    g = std::pow(g * s, 0.8);
    b = std::pow(b * s, 0.8);
    return qRgb(int(r * 255), int(g * 255), int(b * 255));
}
//! [10]
