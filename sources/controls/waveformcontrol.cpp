//*******************************************************************************
// ZX Tape Reviver
//-----------------
//
// Author: Leonid Golouz
// E-mail: lgolouz@list.ru
// YouTube channel: https://www.youtube.com/channel/UCz_ktTqWVekT0P4zVW8Xgcg
// YouTube channel e-mail: computerenthusiasttips@mail.ru
//
// Code modification and distribution of any kind is not allowed without direct
// permission of the Author.
//*******************************************************************************

#include "waveformcontrol.h"
#include <cmath>
#include <climits>
#include <QList>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QPainter>
#include <QDebug>
#include <QGuiApplication>

WaveformControl::WaveformControl(QQuickItem* parent) :
    QQuickPaintedItem(parent),
    mWavReader(*WavReader::instance()),
    mWavParser(*WaveformParser::instance()),
    m_channelNumber(0),
    m_isWaveformRepaired(false),
    m_wavePos(0),
    m_xScaleFactor(1),
    m_yScaleFactor(80000),
    m_clickState(WaitForFirstPress),
    m_clickPosition(0),
    m_operationMode(WaveformControlOperationModes::WaveformRepairMode),
    m_rangeSelected(false),
    m_clickCount(0)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setEnabled(true);
}

QColor WaveformControl::getBackgroundColor() const {
    switch (m_operationMode) {
        case WaveformSelectionMode:
            return m_customData.selectioModeBgColor();

        case WaveformMeasurementMode:
            return m_customData.measurementModeBgColor();

        default:
            return m_customData.operationModeBgColor();
    }
}

QWavVector* WaveformControl::getChannel(uint* chNum) const
{
    const auto c = chNum ? *chNum : m_channelNumber;
    return c == 0 ? mWavReader.getChannel0() : mWavReader.getChannel1();
}

int WaveformControl::getWavPositionByMouseX(int x, int* point, double* dx) const
{
    const int xinc = getXScaleFactor() > 16.0 ? getXScaleFactor() / 16 : 1;
    const double scale = boundingRect().width() * getXScaleFactor();
    double tdx;
    double& rdx = (dx ? *dx : tdx) = (boundingRect().width() / scale) * xinc;

    int tpoint;
    int& rpoint = (point ? *point : tpoint) = std::round(x / rdx);

    return rpoint + getWavePos() * xinc;
}

void WaveformControl::paint(QPainter* painter)
{
    auto p = painter->pen();
 //   const auto paintXAxis = [painter, this]() {};
    painter->setBackground(QBrush(getBackgroundColor()));
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setPen(m_customData.xAxisColor());
    const auto& bRect = boundingRect();
    const double waveHeight = bRect.height() - 100;
    const double halfHeight = waveHeight / 2;
    painter->fillRect(bRect, painter->background());
    painter->drawLine(0, halfHeight, bRect.width(), halfHeight);
    int32_t scale = bRect.width() * getXScaleFactor();
    int32_t pos = getWavePos();
    const auto* channel = getChannel();
    if (channel == nullptr) {
        return;
    }

    // Time Marker
    // It should be painted only once
    p.setColor(m_customData.textColor());
    painter->setPen(p);
    uint32_t sampleRate = mWavReader.getSampleRate();
    double posStartSec = (double) pos / sampleRate;
    double posMidSec = (double) (pos + scale / 2) / sampleRate;
    double posEndSec = (double) (pos + scale) / sampleRate;
    painter->drawText(3, 3, 100 - 3, 20, Qt::AlignTop | Qt::AlignLeft, QString::number(posStartSec, 'f', 3) + " sec");
    painter->drawText((int) bRect.width() / 2 + 3, 3, 100 - 3, 20, Qt::AlignTop | Qt::AlignLeft, QString::number(posMidSec, 'f', 3) + " sec");
    painter->drawText((int) bRect.width() - 100, 3, 100 - 3, 20, Qt::AlignTop | Qt::AlignRight, QString::number(posEndSec, 'f', 3) + " sec");
    painter->drawText(3, 19, bRect.width() - 6, 20, Qt::AlignTop | Qt::AlignLeft, mWavReader.currentFileName);
    p.setColor(m_customData.yAxisColor());
    p.setWidth(1);
    p.setStyle(Qt::DashLine);
    painter->setPen(p);
    painter->drawLine((int) bRect.width() / 2, 0, (int) bRect.width() / 2, waveHeight);

    //Restore default line style
    p.setStyle(Qt::SolidLine);
    painter->setPen(p);

    const double maxy = getYScaleFactor();
    double px = 0;
    double py = halfHeight;
    double x = 0;
    double y = py;
    const int xinc = getXScaleFactor() > 16.0 ? getXScaleFactor() / 16 : 1;
    double dx = (bRect.width() / (double) scale) * xinc;
    const auto parsedWaveform = mWavParser.getParsedWaveform(m_channelNumber);
    bool printHint = false;
    m_allowToGrabPoint = dx > 2;
    const auto chsize = channel->size();
    for (int32_t t = pos; t < pos + scale; t += xinc) {
        if (t >= 0 && t < chsize) {
            const int val = channel->operator[](t);
            y = halfHeight - ((double) (val) / maxy) * waveHeight;
            p.setWidth(m_customData.waveLineThickness());
            p.setColor(val >= 0 ? m_customData.wavePositiveColor() : m_customData.waveNegativeColor());
            painter->setPen(p);
            painter->drawLine(px, py, x, y);
            if (m_allowToGrabPoint) {
                painter->drawEllipse(QPoint(x, y), m_customData.circleRadius(), m_customData.circleRadius());
            }

            const auto pwf = parsedWaveform[t];
            if (pwf & mWavParser.sequenceMiddle) {
                p.setWidth(3);
                painter->setPen(p);
                painter->drawLine(px, bRect.height() - 15, x, bRect.height() - 15);
                if (pwf & mWavParser.zeroBit || pwf & mWavParser.oneBit) {
                    p.setColor(m_customData.blockMarkerColor());
                    painter->setPen(p);
                    painter->drawLine(px, bRect.height() - 3, x, bRect.height() - 3);
                }

                if (printHint) {
                    QString text = pwf & mWavParser.pilotTone
                            ? "PILOT"
                            : pwf & mWavParser.synchroSignal
                              ? "SYNC"
                              : pwf & mWavParser.zeroBit
                                ? "\"0\""
                                : "\"1\"";
                    p.setWidth(1);
                    p.setColor(m_customData.textColor());
                    painter->setPen(p);
                    painter->drawText(x + 3, bRect.height() - 15 - 10, text);
                    printHint = false;
                }
            }
            else if (pwf & mWavParser.sequenceBegin || pwf & mWavParser.sequenceEnd) {
                printHint = pwf & mWavParser.sequenceBegin;
                auto p = painter->pen();
                p.setWidth(3);
                painter->setPen(p);
                painter->drawLine(x, waveHeight + 2, x, bRect.height() - 15);

                if (pwf & mWavParser.byteBound) {
                    p.setColor(pwf & mWavParser.sequenceBegin ? m_customData.blockStartColor() : m_customData.blockEndColor());
                    painter->setPen(p);
                    painter->drawLine(x, bRect.height() - 10, x, bRect.height() - 3);
                }
            }
        }

        px = x;
        py = y;
        x += dx;
    }

    if (m_operationMode == WaveformSelectionMode && m_rangeSelected) {
        painter->setBackground(QBrush(m_customData.rangeSelectionColor()));
        auto bRect = boundingRect();
        bRect.setX(m_selectionRange.first);
        bRect.setRight(m_selectionRange.second);
        painter->fillRect(bRect, painter->background());
    }
}

int32_t WaveformControl::getWavePos() const
{
    return m_wavePos;
}

int32_t WaveformControl::getWaveLength() const
{
    return getChannel()->size();
}

double WaveformControl::getXScaleFactor() const
{
    return m_xScaleFactor;
}

double WaveformControl::getYScaleFactor() const
{
    return m_yScaleFactor;
}

bool WaveformControl::getIsWaveformRepaired() const
{
    return m_isWaveformRepaired;
}

WaveformControl::WaveformControlOperationModes WaveformControl::getOperationMode() const
{
    return m_operationMode;
}

void WaveformControl::setWavePos(int32_t wavePos)
{
    if (m_wavePos != wavePos) {
        m_wavePos = wavePos;
        update();

        emit wavePosChanged();
    }
}

void WaveformControl::setXScaleFactor(double xScaleFactor)
{
    if (m_xScaleFactor != xScaleFactor) {
        m_xScaleFactor = xScaleFactor;
        update();

        emit xScaleFactorChanged();
    }
}

void WaveformControl::setYScaleFactor(double yScaleFactor)
{
    if (m_yScaleFactor != yScaleFactor) {
        m_yScaleFactor = yScaleFactor;
        update();

        emit yScaleFactorChanged();
    }
}

void WaveformControl::setOperationMode(WaveformControlOperationModes mode)
{
    if (m_operationMode != mode) {
        m_operationMode = mode;
        m_rangeSelected = false;
        m_clickCount = 0;
        update();

        emit operationModeChanged();
    }
}

void WaveformControl::mousePressEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }

    //Checking for double-click
    if (event->button() == Qt::LeftButton) {
        auto now = QDateTime::currentDateTime();
        qDebug() << "Click state: " << m_clickState << "; time: " << m_clickTime.msecsTo(now);
        if (m_clickState == WaitForFirstPress) {
            m_clickTime = QDateTime::currentDateTime();
            m_clickState = WaitForFirstRelease;
        }
        else if (m_clickState == WaitForSecondPress && m_clickTime.msecsTo(now) <= 500) {
            m_clickState = WaitForSecondRelease;
        }
        else {
            m_clickState = WaitForFirstPress;
        }
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        double dx;
        int point;
        m_clickPosition = getWavPositionByMouseX(event->x(), &point, &dx);
        event->accept();

        if (m_operationMode == WaveformSelectionMode) {
            m_rangeSelected = true;
            m_selectionRange = { event->x(), event->x() };
            update();
        }

        if (dx <= 2.0) {
            if (m_operationMode == WaveformMeasurementMode) {
                emit cannotSetMeasurementPoint();
            }
            return;
        }
        double dpoint = point * dx;
        if (m_operationMode == WaveformRepairMode) {
            if (event->button() == Qt::MiddleButton) {
                //const auto p = point + getWavePos();
                const auto d = getChannel()->operator[](m_clickPosition);
                qDebug() << "Inserting point: " << m_clickPosition;
                getChannel()->insert(m_clickPosition + (dpoint > event->x() ? 1 : -1), d);
                update();
            }
            else {
                if (dpoint >= (event->x() - dx/2) && dpoint <= (event->x() + dx/2)) {
                    if (event->button() == Qt::LeftButton) {
                        m_pointIndex = point;
                        m_pointGrabbed = true;
                        qDebug() << "Grabbed point: " << getChannel()->operator[](m_clickPosition);
                    } else {
                        if (QGuiApplication::queryKeyboardModifiers() != Qt::ShiftModifier) {
                            m_pointGrabbed = false;
                            qDebug() << "Deleting point";
                            getChannel()->remove(m_clickPosition);
                            update();
                        }
                    }
                }
            }
        } // m_operationMode
    }
}

void WaveformControl::mouseReleaseEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }

    switch (event->button()) {
    case Qt::LeftButton:
        {
            //Double-click handling
            auto now = QDateTime::currentDateTime();
            qDebug() << "Click state: " << m_clickState << "; time: " << m_clickTime.msecsTo(now);

            if (m_operationMode == WaveformSelectionMode && m_rangeSelected && m_selectionRange.first == m_selectionRange.second) {
                m_rangeSelected = false;
            }

            if (m_operationMode == WaveformRepairMode || m_operationMode == WaveformSelectionMode) {
                if ( m_clickState == WaitForFirstRelease && m_clickTime.msecsTo(now) <= 500) {
                    m_clickState = WaitForSecondPress;
                }
                else if (m_clickState == WaitForSecondRelease && m_clickTime.msecsTo(now) <= 500) {
                    m_clickState = WaitForFirstPress;
                    emit doubleClick(m_clickPosition);
                }
                else {
                    m_clickState = WaitForFirstPress;
                }
            }
            else if (m_operationMode == WaveformMeasurementMode) {
                auto& clickPoint = m_clickCount == 0 ? m_selectionRange.first : m_selectionRange.second;
                clickPoint = getWavPositionByMouseX(event->x());
                if (m_clickCount == 1) {
                    auto len = std::abs(m_selectionRange.first - m_selectionRange.second);
                    int freq = mWavReader.getSampleRate() / (len == 0 ? 1 : len);
                    qDebug() << "Frequency: " << freq;
                    emit frequency(freq);
                }
                m_clickCount = (m_clickCount + 1) % 2;
            }

            m_pointGrabbed = false;
            event->accept();
        }
        break;

    default:
        event->ignore();
        break;
    }
}

void WaveformControl::mouseMoveEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }

    switch (event->buttons()) {
        case Qt::LeftButton: {
            if (m_operationMode == WaveformSelectionMode && m_rangeSelected) {
                if (event->x() <= m_selectionRange.first) {
                    m_selectionRange.first = event->x();
                }
                else {
                    m_selectionRange.second = event->x();
                }
            }
            else if (m_operationMode == WaveformRepairMode) {
                const auto* ch = getChannel();
                if (!ch) {
                    return;
                }

                const double waveHeight = boundingRect().height() - 100;
                const double halfHeight = waveHeight / 2;
                const auto pointerPos = halfHeight - event->y();
                double val = halfHeight + (m_yScaleFactor / waveHeight * pointerPos);
                if (m_pointIndex + getWavePos() >= 0 && m_pointIndex + getWavePos() < ch->size()) {
                    getChannel()->operator[](m_pointIndex + getWavePos()) = val;
                }
                qDebug() << "Setting point: " << m_pointIndex + getWavePos();
            }
            event->accept();
            update();
        }
        break;

        // Smooth drawing at Repair mode
        case Qt::RightButton: {
            if (m_operationMode == WaveformRepairMode && QGuiApplication::queryKeyboardModifiers() == Qt::ShiftModifier) {
                const auto* ch = getChannel();
                if (!ch) {
                    return;
                }
                double dx;
                int point;
                m_clickPosition = getWavPositionByMouseX(event->x(), &point, &dx);
                const double waveHeight = boundingRect().height() - 100;
                const double halfHeight = waveHeight / 2;
                const auto pointerPosY = halfHeight - event->y();
                double val = halfHeight + (m_yScaleFactor / waveHeight * pointerPosY);
                getChannel()->operator[](m_clickPosition) = val;
            }
            event->accept();
            update();
        }
        break;

        default:
            event->ignore();
            break;
    }
}

uint WaveformControl::getChannelNumber() const
{
    return m_channelNumber;
}

void WaveformControl::setChannelNumber(uint chNum)
{
    if (chNum != m_channelNumber) {
        m_channelNumber = chNum;
        emit channelNumberChanged();
    }
}

void WaveformControl::reparse()
{
    mWavParser.parse(m_channelNumber);
    update();
}

void WaveformControl::saveTap(const QString& fileUrl)
{
    QString fileName = fileUrl.isEmpty() ? fileUrl : QUrl(fileUrl).toLocalFile();
    mWavParser.saveTap(m_channelNumber, fileName);
}

void WaveformControl::saveWaveform()
{
    mWavReader.saveWaveform();
}

void WaveformControl::repairWaveform()
{
    if (!m_isWaveformRepaired) {
        //mWavReader.repairWaveform(m_channelNumber);
        mWavReader.normalizeWaveform2(m_channelNumber);
        update();
        m_isWaveformRepaired = true;
        emit isWaveformRepairedChanged();
    }
}

void WaveformControl::restoreWaveform()
{
    if (m_isWaveformRepaired) {
        mWavReader.restoreWaveform(m_channelNumber);
        update();
        m_isWaveformRepaired = false;
        emit isWaveformRepairedChanged();
    }
}

void WaveformControl::shiftWaveform()
{
    mWavReader.shiftWaveform(m_channelNumber);
    update();
}

void WaveformControl::copySelectedToAnotherChannel()
{
    if (m_operationMode == WaveformSelectionMode && m_rangeSelected) {
        uint destChNum = getChannelNumber() == 0 ? 1 : 0;
        const auto sourceChannel = getChannel();
        const auto destChannel = getChannel(&destChNum);
        const auto endIdx = getWavPositionByMouseX(m_selectionRange.second);
        for (auto i = getWavPositionByMouseX(m_selectionRange.first); i <= endIdx; ++i) {
            destChannel->operator[](i) = sourceChannel->operator[](i);
        }
    }
}
