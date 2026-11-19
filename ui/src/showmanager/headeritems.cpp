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

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QFontMetrics>
#include <QPainter>
#include <cmath>
#include <QCursor>

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
    , m_gridDivision("1/4")
{
}

void ShowHeaderItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
    emit itemClicked(event);
}

void ShowHeaderItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->pos().y() < MARKER_LANE_HEIGHT)
        emit markerLaneDoubleClicked(event->pos().x(), event->modifiers() & Qt::ShiftModifier);
    else
        QGraphicsItem::mouseDoubleClickEvent(event);
}

void ShowHeaderItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (event->pos().y() < MARKER_LANE_HEIGHT)
    {
        emit markerLaneContextMenu(event->pos().x(), event->screenPos());
        event->accept();
    }
    else
        QGraphicsItem::contextMenuEvent(event);
}

QRectF ShowHeaderItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void ShowHeaderItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // QLC+DAN: marker lane
    painter->setPen(QPen(QColor(100, 100, 100, 255), 1));
    painter->setBrush(QBrush(QColor(118, 118, 118, 255)));
    painter->drawRect(0, 0, m_width, MARKER_LANE_HEIGHT);

    // the ruler sits below the marker lane
    painter->save();
    painter->translate(0, MARKER_LANE_HEIGHT);

    // draw base background
    painter->setPen(QPen(QColor(100, 100, 100, 255), 1));
    painter->setBrush(QBrush(QColor(150, 150, 150, 255)));
    painter->drawRect(0, 0, m_width, RULER_HEIGHT);

    if (m_type > Show::Time)
        m_timeStep = beatWidth();

    if (m_timeStep <= 0)
    {
        painter->restore();
        return;
    }

    // QLC+DAN: in BPM mode, draw bars, beats and the chosen note divisions
    if (m_type != Show::Time)
    {
        paintBPMGrid(painter);
        painter->restore();
        return;
    }

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
                painter->drawLine(xpos, RULER_HEIGHT, xpos, m_height - MARKER_LANE_HEIGHT);
            }
            painter->setPen(QPen(Qt::black, 1));
            if (m_type == Show::Time)
            {
                // QLC+DAN: the closest zoom levels label every quarter or half second
                qint64 ms = qRound64((i / 2) * m_timeScale * 1000.0);
                tmpSec = ms / 1000;
                int frac = ms % 1000;
                QString secText = frac == 0 ? QString::number(tmpSec % 60)
                                            : QString::number((tmpSec % 60) + frac / 1000.0, 'g', 4);
                // label quarter seconds at most, so the labels don't run together
                if (ms % 250 != 0)
                    ;
                else if (tmpSec < 60)
                    painter->drawText(xpos - 4, 15, QString("%1s").arg(secText));
                else
                    painter->drawText(xpos - 4, 15, QString("%1m%2s").arg(tmpSec / 60).arg(secText));
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
                    painter->drawLine(xpos, RULER_HEIGHT, xpos, m_height - MARKER_LANE_HEIGHT);
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

    painter->restore();
}

float ShowHeaderItem::beatWidth() const
{
    if (m_BPMValue <= 0 || m_timeScale <= 0)
        return HALF_SECOND_WIDTH;
    return ((float)(120 * HALF_SECOND_WIDTH) / (float)m_BPMValue) / (float)m_timeScale;
}

void ShowHeaderItem::setTimeScale(double val)
{
    m_timeScale = val;
    update();
}

double ShowHeaderItem::getTimeScale() const
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

/*********************************************************************
 * Grid divisions (QLC+DAN)
 *********************************************************************/

void ShowHeaderItem::setGridDivision(const QString &division)
{
    m_gridDivision = division;
    update();
}

QString ShowHeaderItem::getGridDivision() const
{
    return m_gridDivision;
}

double ShowHeaderItem::divisionBeats() const
{
    // one beat is a crotchet
    if (m_gridDivision == "bar")   return m_timeHit;
    if (m_gridDivision == "1/2")   return 2.0;
    if (m_gridDivision == "1/8")   return 0.5;
    if (m_gridDivision == "1/16")  return 0.25;
    if (m_gridDivision == "1/32")  return 0.125;
    if (m_gridDivision == "1/4T")  return 2.0 / 3.0;
    if (m_gridDivision == "1/8T")  return 1.0 / 3.0;
    if (m_gridDivision == "1/16T") return 1.0 / 6.0;
    return 1.0;
}

double ShowHeaderItem::effectiveDivisionBeats() const
{
    double beats = divisionBeats();
    float bw = beatWidth();
    if (beats * bw >= 4)
        return beats;
    if (beats < 1.0 && bw >= 4)
        return 1.0;
    return m_timeHit;
}

quint32 ShowHeaderItem::snapTime(quint32 time) const
{
    double snapped;

    if (m_type == Show::Time || m_BPMValue <= 0)
    {
        double step = getGridStepMs();
        if (step <= 0)
            return time;
        snapped = qRound64((double)time / step) * step;
    }
    else
    {
        double beatMs = 60000.0 / (double)m_BPMValue;
        double barMs = beatMs * m_timeHit;
        // snap to the chosen note value even when it is too fine to draw
        double stepMs = beatMs * divisionBeats();
        double offset = getGridOffsetMs();

        double rel = (double)time - offset;
        double bar = std::floor(rel / barMs);
        double within = rel - bar * barMs;

        // the division before, the one after (if still in this bar), and the next bar
        double before = std::floor(within / stepMs) * stepMs;
        double best = before;
        double after = before + stepMs;
        if (after < barMs - 0.001 && qAbs(after - within) < qAbs(best - within))
            best = after;
        if (qAbs(barMs - within) < qAbs(best - within))
            best = barMs;

        snapped = offset + bar * barMs + best;

        // grid lines before the start of the show are not reachable
        if (snapped < 0)
            snapped = 0;
    }

    // the start of the show is always a valid position
    if ((double)time < qAbs(snapped - (double)time))
        return 0;

    return (quint32)qRound64(snapped);
}

void ShowHeaderItem::paintBPMGrid(QPainter *painter)
{
    float bw = beatWidth();
    if (bw <= 0)
        return;

    double barPx = bw * m_timeHit;
    double divBeats = effectiveDivisionBeats();
    bool beatsVisible = bw > 5;
    bool gridLines = m_height > HEADER_HEIGHT;
    int lineBottom = m_height - MARKER_LANE_HEIGHT;

    /* The grid can be shifted to the right, so that bar 1 starts at
     * m_gridOffset milliseconds. Bars before the offset are drawn too
     * (counting backwards), but without labels */
    double offsetPx = getGridOffsetMs() * (double)HALF_SECOND_WIDTH / (500.0 * (double)m_timeScale);
    int firstBar = -(int)std::ceil(offsetPx / barPx);

    const QColor barLine(150, 150, 150);
    const QColor beatLine(112, 112, 112);
    const QColor subLine(98, 98, 98);

    // when zoomed out, number every 2nd, 4th, 8th... bar so the numbers don't overlap
    int labelEvery = 1;
    while (barPx * labelEvery < 26 && labelEvery < 1024)
        labelEvery *= 2;

    for (int b = firstBar; ; b++)
    {
        double barX = offsetPx + b * barPx + 1;
        if (barX > m_width)
            break;

        // subdivisions and beats first, so the bar line is drawn on top
        QList<double> positions; // in beats from the start of the bar
        for (int k = 1; k * divBeats < m_timeHit - 0.0001; k++)
            positions.append(k * divBeats);
        if (beatsVisible && divBeats > 1.0)
        {
            // coarse grid: still mark the beats on the ruler
            for (int k = 1; k < m_timeHit; k++)
                if (positions.contains(k) == false)
                    positions.append(k);
        }

        foreach (double beats, positions)
        {
            double x = barX + beats * bw;
            if (x < 0)
                continue;
            if (x > m_width)
                break;
            bool onBeat = qAbs(beats - qRound(beats)) < 0.0001;
            bool onGrid = qAbs(beats / divBeats - qRound(beats / divBeats)) < 0.0001;

            painter->setPen(QPen(QColor(250, 250, 250), 1));
            painter->drawLine(QPointF(x, onBeat ? 25 : 29), QPointF(x, 34));
            if (gridLines && onGrid)
            {
                painter->setPen(QPen(onBeat ? beatLine : subLine, 1));
                painter->drawLine(QPointF(x, RULER_HEIGHT), QPointF(x, lineBottom));
            }
        }

        if (barX < 0)
            continue;

        // the bar line and its number
        painter->setPen(QPen(QColor(250, 250, 250), 1));
        painter->drawLine(QPointF(barX, 20), QPointF(barX, 34));
        if (gridLines)
        {
            painter->setPen(QPen(barLine, 1));
            painter->drawLine(QPointF(barX, RULER_HEIGHT), QPointF(barX, lineBottom));
        }
        if (b >= 0 && b % labelEvery == 0)
        {
            painter->setPen(QPen(Qt::black, 1));
            painter->drawText(QPointF(barX - 4, 15), QString::number(b + 1));
        }
    }

    // highlight where bar 1 starts when the grid is offset
    if (offsetPx > 0)
    {
        painter->setPen(QPen(QColor(255, 140, 0, 255), 2));
        painter->drawLine(QPointF(offsetPx + 1, 0), QPointF(offsetPx + 1, 34));
    }
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
    const qreal top = MARKER_LANE_HEIGHT;
    CursorHead.append(QPointF(-5.0, top + 22.0));
    CursorHead.append(QPointF(5.0, top + 22.0));
    CursorHead.append(QPointF(5.0, top + 25.0));
    CursorHead.append(QPointF(0.0, top + 35.0));
    CursorHead.append(QPointF(-5.0, top + 25.0));
    CursorHead.append(QPointF(-5.0, top + 22.0));
    painter->drawPolygon(CursorHead);
    painter->setPen(Qt::NoPen);
    painter->drawRect(0, HEADER_HEIGHT, 1, m_height - HEADER_HEIGHT);
}


/****************************************************************************
 *
 * Marker item (QLC+DAN)
 *
 ****************************************************************************/

QList<QColor> ShowMarkerItem::palette()
{
    return QList<QColor>()
        << QColor(214, 60, 90)    // red
        << QColor(235, 135, 40)   // orange
        << QColor(240, 200, 40)   // yellow
        << QColor(70, 175, 80)    // green
        << QColor(30, 165, 170)   // teal
        << QColor(60, 120, 220)   // blue
        << QColor(140, 85, 200)   // purple
        << QColor(225, 95, 175);  // pink
}

QColor ShowMarkerItem::textColorFor(const QColor &background)
{
    // perceived brightness
    int lum = (background.red() * 299 + background.green() * 587 + background.blue() * 114) / 1000;
    return lum > 150 ? QColor(20, 20, 20) : Qt::white;
}

ShowMarkerItem::ShowMarkerItem(int index, quint32 time, const QString &name, const QColor &color, int height)
    : m_index(index)
    , m_time(time)
    , m_name(name)
    , m_color(color.isValid() ? color : palette().first())
    , m_height(height)
    , m_bandWidth(100)
    , m_viewLeft(0)
    , m_dragging(false)
    , m_pressSceneX(0)
    , m_pressItemX(0)
{
    m_font.setPixelSize(11);
    m_font.setBold(true);
    setToolTip(name);
    setCursor(Qt::PointingHandCursor);
    setZValue(998); // above the show items, below the cursor
}

void ShowMarkerItem::setHeight(int height)
{
    prepareGeometryChange();
    m_height = height;
}

void ShowMarkerItem::setBandWidth(int width)
{
    prepareGeometryChange();
    m_bandWidth = qMax(1, width);
    update();
}

void ShowMarkerItem::setViewLeft(qreal sceneX)
{
    if (qFuzzyCompare(m_viewLeft, sceneX))
        return;
    m_viewLeft = sceneX;
    update(QRectF(0, 0, m_bandWidth, MARKER_LANE_HEIGHT));
}

int ShowMarkerItem::flagWidth() const
{
    QFontMetrics fm(m_font);
    int w = fm.horizontalAdvance(m_name) + 10;
    return qBound(6, w, qMax(6, m_bandWidth - 1));
}

QRectF ShowMarkerItem::boundingRect() const
{
    return QRectF(-1, 0, m_bandWidth + 2, m_height);
}

QPainterPath ShowMarkerItem::shape() const
{
    // only the flag can be clicked: the rest of the band and the line
    // behave like the empty lane and timeline under them
    QPainterPath path;
    path.addRect(QRectF(-3, 0, flagWidth() + 3, MARKER_LANE_HEIGHT));
    return path;
}

void ShowMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    QColor c = m_color;
    if (m_dragging)
        c = c.lighter(125);

    // the section: a band of colour up to the next marker
    QColor band = c;
    band.setAlpha(120);
    painter->setPen(Qt::NoPen);
    painter->setBrush(band);
    painter->drawRect(QRectF(0, 0, m_bandWidth, MARKER_LANE_HEIGHT));

    // where the section starts: a line through the ruler and the tracks
    QColor lineColor = c;
    lineColor.setAlpha(200);
    painter->setPen(QPen(lineColor, 1, Qt::DashLine));
    painter->drawLine(QPointF(0.5, MARKER_LANE_HEIGHT), QPointF(0.5, m_height));

    // flag with the name
    QFontMetrics fm(m_font);
    int fw = flagWidth();
    painter->setPen(Qt::NoPen);
    painter->setBrush(c);
    painter->drawRect(QRectF(0, 0, fw, MARKER_LANE_HEIGHT));
    painter->setFont(m_font);
    painter->setPen(textColorFor(c));
    painter->drawText(QRectF(5, 0, fw - 6, MARKER_LANE_HEIGHT), Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(m_name, Qt::ElideRight, fw - 6));

    // when the flag has scrolled out of view, repeat the name at the left
    // edge, so you can always tell which section you are in
    qreal left = m_viewLeft - x();
    if (left > fw && left < m_bandWidth - 20)
    {
        qreal room = m_bandWidth - left;
        int w = qMin<int>(room, fm.horizontalAdvance(m_name) + 10);
        QColor tag = c;
        tag.setAlpha(210);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tag);
        painter->drawRect(QRectF(left, 0, w, MARKER_LANE_HEIGHT));
        painter->setPen(textColorFor(c));
        painter->drawText(QRectF(left + 5, 0, w - 6, MARKER_LANE_HEIGHT), Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(m_name, Qt::ElideRight, w - 6));
    }
}

void ShowMarkerItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        event->ignore();
        return;
    }
    m_dragging = false;
    m_pressSceneX = event->scenePos().x();
    m_pressItemX = x();
    event->accept();
}

void ShowMarkerItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    qreal dx = event->scenePos().x() - m_pressSceneX;
    if (m_dragging == false && qAbs(dx) < 3)
        return;
    if (m_dragging == false)
    {
        m_dragging = true;
        update();
    }
    emit dragged(this, m_pressItemX + dx, event->modifiers() & Qt::ShiftModifier);
}

void ShowMarkerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_dragging)
    {
        m_dragging = false;
        update();
        emit dropped(this);
    }
    else
    {
        emit clicked(this);
    }
}

void ShowMarkerItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
    emit doubleClicked(this);
}

void ShowMarkerItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    event->accept();
    emit contextMenuRequested(this, event->screenPos());
}
