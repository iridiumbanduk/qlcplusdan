/*
  Q Light Controller Plus
  headeritems.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QPainter>

#include "headeritems.h"

/****************************************************************************
 *
 * Header item
 *
 ****************************************************************************/
ShowHeaderItem::ShowHeaderItem(int width)
    : m_width(width)
    , m_height(HEADER_HEIGHT)
    , m_timeStep(HALF_SECOND_WIDTH)
    , m_timeHit(2)
    , m_timeScale(3)
    , m_BPMValue(120)
    , m_type(Show::Time)
    , m_gridOffset(0)
{
}

void ShowHeaderItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
    emit itemClicked(event);
}

QRectF ShowHeaderItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void ShowHeaderItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // draw base background
    painter->setPen(QPen(QColor(100, 100, 100, 255), 1));
    painter->setBrush(QBrush(QColor(150, 150, 150, 255)));
    painter->drawRect(0, 0, m_width, 35);

    if (m_type > Show::Time)
        m_timeStep = beatWidth();

    if (m_timeStep <= 0)
        return;

    /* In BPM mode the grid can be shifted to the right, so that bar 1
     * starts at m_gridOffset milliseconds. Grid lines are also drawn in
     * the area before the offset (counting backwards), but without labels */
    float offsetPx = (float)(getGridOffsetMs() * (double)HALF_SECOND_WIDTH /
                             (500.0 * (double)m_timeScale));
    int firstIdx = -(int)(offsetPx / m_timeStep);

    // draw vertical timing lines and time labels
    int tmpSec = 0;
    for (int i = firstIdx; ; i++)
    {
        float xpos = offsetPx + ((float)i * m_timeStep) + 1;
        if (xpos > m_width)
            break;

        bool isHit = ((i % m_timeHit) + m_timeHit) % m_timeHit == 0;

        painter->setPen(QPen(QColor(250, 250, 250, 255), 1));
        if (isHit)
        {
            painter->drawLine(xpos, 20, xpos, 34);
            if (m_height > HEADER_HEIGHT)
            {
                painter->setPen(QPen(QColor(105, 105, 105, 255), 1));
                painter->drawLine(xpos, HEADER_HEIGHT, xpos, m_height);
            }
            painter->setPen(QPen(Qt::black, 1));
            if (m_type == Show::Time)
            {
                tmpSec = (i/2) * m_timeScale;
                if (tmpSec < 60)
                    painter->drawText(xpos - 4, 15, QString("%1s").arg(tmpSec));
                else
                {
                    int tmpMin = tmpSec / 60;
                    tmpSec = tmpSec - (tmpMin * 60);
                    painter->drawText(xpos - 4, 15, QString("%1m%2s").arg(tmpMin).arg(tmpSec));
                }
            }
            else if (i >= 0)
            {
                painter->drawText(xpos - 4, 15, QString("%1").arg((i / m_timeHit) + 1));
            }
        }
        else
        {
            if (m_timeStep > 5)
            {
                painter->drawLine(xpos, 25, xpos, 34);
                if (m_height > HEADER_HEIGHT)
                {
                    painter->setPen(QPen(QColor(105, 105, 105, 255), 1));
                    painter->drawLine(xpos, HEADER_HEIGHT, xpos, m_height);
                }
            }
        }
    }

    // highlight where bar 1 starts when the grid is offset
    if (offsetPx > 0)
    {
        painter->setPen(QPen(QColor(255, 140, 0, 255), 2));
        painter->drawLine(offsetPx + 1, 0, offsetPx + 1, 34);
    }
}

float ShowHeaderItem::beatWidth() const
{
    if (m_BPMValue <= 0 || m_timeScale <= 0)
        return HALF_SECOND_WIDTH;
    return ((float)(120 * HALF_SECOND_WIDTH) / (float)m_BPMValue) / (float)m_timeScale;
}

void ShowHeaderItem::setTimeScale(int val)
{
    m_timeScale = val;
    update();
}

int ShowHeaderItem::getTimeScale() const
{
    return m_timeScale;
}

void ShowHeaderItem::setTimeDivisionType(Show::TimeDivision type)
{
    if (type >= Show::Invalid)
        return;

    m_type = type;
    if (m_type == Show::Time)
    {
        m_timeStep = HALF_SECOND_WIDTH;
        m_timeHit = 2;
    }
    else
    {
        if (m_type == Show::BPM_4_4)
            m_timeHit = 4;
        else if (m_type == Show::BPM_3_4)
            m_timeHit = 3;
        else if (m_type == Show::BPM_2_4)
            m_timeHit = 2;
    }
    update();
}

Show::TimeDivision ShowHeaderItem::getTimeDivisionType() const
{
    return m_type;
}

void ShowHeaderItem::setBPMValue(int value)
{
    if (value > 1)
    {
        m_BPMValue = value;
    }
    update();
}

int ShowHeaderItem::getHalfSecondWidth() const
{
    return HALF_SECOND_WIDTH;
}

float ShowHeaderItem::getTimeDivisionStep() const
{
    if (m_type > Show::Time && m_timeStep <= 5)
        return m_timeStep * m_timeHit;
    return m_timeStep;
}

void ShowHeaderItem::setGridOffset(quint32 msec)
{
    m_gridOffset = msec;
    update();
}

quint32 ShowHeaderItem::getGridOffset() const
{
    return m_gridOffset;
}

double ShowHeaderItem::getGridOffsetMs() const
{
    if (m_type == Show::Time)
        return 0;
    return m_gridOffset;
}

double ShowHeaderItem::getGridStepMs() const
{
    // in Time mode a division is half a second, scaled by the zoom level
    if (m_type == Show::Time)
        return 500.0 * m_timeScale;

    if (m_BPMValue <= 0)
        return 500.0;

    // one beat. When zoomed out so far that beats are not drawn, snap to bars
    double beatMs = 60000.0 / (double)m_BPMValue;
    if (beatWidth() <= 5)
        return beatMs * m_timeHit;
    return beatMs;
}

void ShowHeaderItem::setWidth(int w)
{
    prepareGeometryChange();
    m_width = w;
}

void ShowHeaderItem::setHeight(int h)
{
    prepareGeometryChange();
    m_height = h;
}


/****************************************************************************
 *
 * Cursor item
 *
 ****************************************************************************/
ShowCursorItem::ShowCursorItem(int h)
    : m_height(h)
    , m_time(0)
{
}

void ShowCursorItem::setHeight(int height)
{
    prepareGeometryChange();
    m_height = height;
}

void ShowCursorItem::setTime(quint32 t)
{
    m_time = t;
}

quint32 ShowCursorItem::getTime() const
{
    return m_time;
}

QRectF ShowCursorItem::boundingRect() const
{
    return QRectF(-5, 0, 10, m_height);
}

void ShowCursorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setBrush(QBrush(Qt::yellow, Qt::SolidPattern));
    painter->setPen(QPen(Qt::yellow, 1));
    QPolygonF CursorHead;
    CursorHead.append(QPointF(-5.0, 22.0));
    CursorHead.append(QPointF(5.0, 22.0));
    CursorHead.append(QPointF(5.0, 25.0));
    CursorHead.append(QPointF(0.0, 35.0));
    CursorHead.append(QPointF(-5.0, 25.0));
    CursorHead.append(QPointF(-5.0, 22.0));
    painter->drawPolygon(CursorHead);
    painter->setPen(Qt::NoPen);
    painter->drawRect(0, 35, 1, m_height - 35);
}
