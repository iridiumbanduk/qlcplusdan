/*
  Q Light Controller Plus
  headeritems.h

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

#ifndef HEADERITEM_H
#define HEADERITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QAction>
#include <QFont>

#include "show.h"

/* QLC+DAN: a lane for markers sits above the time ruler */
#define MARKER_LANE_HEIGHT  18
#define RULER_HEIGHT        35
#define HEADER_HEIGHT       (MARKER_LANE_HEIGHT + RULER_HEIGHT)
#define HALF_SECOND_WIDTH   25

/** @addtogroup ui_functions
 * @{
 */

/**
 *
 * Show Manager Header class. Clickable time line header
 *
 */

class ShowHeaderItem final : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    ShowHeaderItem(int width);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTimeScale(double val);
    double getTimeScale() const;

    void setTimeDivisionType(Show::TimeDivision type);
    Show::TimeDivision getTimeDivisionType() const;
    void setBPMValue(int value);

    int getHalfSecondWidth() const;
    float getTimeDivisionStep() const;

    /** Set/Get the grid offset in milliseconds. In BPM mode this is the
     *  time where bar 1 of the grid starts. Ignored in Time mode. */
    void setGridOffset(quint32 msec);
    quint32 getGridOffset() const;

    /** Return the effective grid offset (0 in Time mode) */
    double getGridOffsetMs() const;

    /** Return the snapping step of the grid in milliseconds
     *  (a beat in BPM mode, or a bar when zoomed out too far) */
    double getGridStepMs() const;

    /** QLC+DAN: note value of the BPM grid ("bar", "1/2", "1/4", "1/8",
     *  "1/16", "1/32", "1/4T", "1/8T", "1/16T") */
    void setGridDivision(const QString &division);
    QString getGridDivision() const;

    /** QLC+DAN: a position as bar|beat|tick, counted from where bar 1
     *  starts (the grid offset). Before bar 1 the bars go 0, -1, -2...
     *  960 ticks to a beat. Empty in Time mode */
    QString positionToBars(qint64 ms) const;
    /** A length as bars|beats|ticks. Empty in Time mode */
    QString lengthToBars(qint64 ms) const;

    /** Return the grid position closest to the given time. The grid restarts
     *  at every bar, so divisions that don't fit a bar exactly (e.g. halves
     *  in 3/4) still line up with the bars, like in a DAW */
    quint32 snapTime(quint32 time) const;

    void setWidth(int);
    void setHeight(int);

signals:
    void itemClicked(QGraphicsSceneMouseEvent *);
    /** Double click in the marker lane (x is relative to the header) */
    void markerLaneDoubleClicked(qreal x, bool noSnap);
    /** Right click in the marker lane */
    void markerLaneContextMenu(qreal x, QPoint screenPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    /** Total width of the item */
    int m_width;
    /** Total height of the item */
    int m_height;
    /** Distance in pixels between the time division bars */
    float m_timeStep;
    /** Divisor of the time division hit bar (the highest bar) */
    char m_timeHit;
    /** Scale of the time division */
    double m_timeScale;
    /** When BPM mode is active, this holds the number of BPM to display */
    int m_BPMValue;
    /** The type of time division */
    Show::TimeDivision m_type;
    /** Time in milliseconds where bar 1 of the BPM grid starts */
    quint32 m_gridOffset;

    /** Width in pixels of a single beat in BPM mode */
    float beatWidth() const;

    /** Note value of the grid */
    QString m_gridDivision;
    /** Draw the ruler and grid lines in BPM mode */
    void paintBPMGrid(QPainter *painter);
    /** Length of a grid division in beats (crotchets) */
    double divisionBeats() const;
    /** The division drawn at this zoom level: when its lines would be too
     *  close together, beats, then bars, are drawn instead. Snapping always
     *  uses the chosen division */
    double effectiveDivisionBeats() const;
};

/**
 *
 * Show Manager Cursor class. Cursor which marks the time position in a scene
 *
 */
class ShowCursorItem final : public QGraphicsItem
{
public:
    ShowCursorItem(int h);

    void setHeight(int height);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTime(quint32 t);
    quint32 getTime() const;
private:
    int m_height;
    quint32 m_time;
};

/**
 *
 * QLC+DAN: a marker on the timeline, like the markers in Pro Tools.
 * A flag with a name in the marker lane, a band of the marker's colour that
 * runs until the next marker, and a line down through the tracks.
 *
 */
class ShowMarkerItem final : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    ShowMarkerItem(int index, quint32 time, const QString &name, const QColor &color, int height);

    int index() const { return m_index; }
    quint32 time() const { return m_time; }
    QString name() const { return m_name; }
    QColor color() const { return m_color; }

    /** The colours offered for markers. New markers take them in turn */
    static QList<QColor> palette();
    /** White or black, whichever reads better on the given colour */
    static QColor textColorFor(const QColor &background);

    void setHeight(int height);
    /** Length of the coloured section, up to the next marker */
    void setBandWidth(int width);
    /** Left edge of the visible timeline (scene x). When the flag has
     *  scrolled out of view the name is repeated there */
    void setViewLeft(qreal sceneX);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    /** Clicked without dragging */
    void clicked(ShowMarkerItem *item);
    /** Being dragged: the view decides where it goes (snapping) */
    void dragged(ShowMarkerItem *item, qreal x, bool noSnap);
    /** Drag finished */
    void dropped(ShowMarkerItem *item);
    void doubleClicked(ShowMarkerItem *item);
    void contextMenuRequested(ShowMarkerItem *item, QPoint screenPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    /** Width of the flag at the start of the section */
    int flagWidth() const;

    int m_index;
    quint32 m_time;
    QString m_name;
    QColor m_color;
    int m_height;
    int m_bandWidth;
    qreal m_viewLeft;
    QFont m_font;
    bool m_dragging;
    qreal m_pressSceneX;
    qreal m_pressItemX;
};

/** @} */

#endif
