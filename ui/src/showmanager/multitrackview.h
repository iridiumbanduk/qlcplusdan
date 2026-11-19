/*
  Q Light Controller Plus
  multitrackview.h

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

#ifndef MULTITRACKVIEW_H
#define MULTITRACKVIEW_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QSlider>
#include <QWidget>

#include "rgbmatrixitem.h"
#include "sequenceitem.h"
#include "headeritems.h"
#include "trackitem.h"
#include "audioitem.h"
#include "efxitem.h"
#include "videoitem.h"
#include "chaser.h"
#include "track.h"

/** @addtogroup ui_shows
 * @{
 */

class MultiTrackView final : public QGraphicsView
{
    Q_OBJECT

public:
    MultiTrackView(QWidget *parent = 0);

    /** Update tracks horizontal dividers when the view changes */
    void updateTracksDividers();

    /** Set the multitrack view size in pixels */
    void setViewSize(int width, int height);

    /** Auto calculation of view size based on items */
    void updateViewSize();

    /*********************************************************************
     * Contents
     *********************************************************************/

    /** Update the multitrack view with the scene elements */
    void resetView();

    /** Add a new track to the view */
    void addTrack(Track *track);

    /** Add a new sequence item to the given track */
    void addSequence(Chaser *chaser, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new audio item to the given track */
    void addAudio(Audio *audio, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new RGB Matrix item to the given track */
    void addRGBMatrix(RGBMatrix *rgbm, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new EFX item to the given track */
    void addEFX(EFX *efx, Track *track = NULL, ShowFunction *sf = NULL);

    /** Add a new video item to the given track */
    void addVideo(Video *video, Track *track = NULL, ShowFunction *sf = NULL);

    /** Delete the currently selected item */
    quint32 deleteSelectedItem();

    /** Delete a specific ShowFuntion and related ShowItem from the
     *  given track */
    void deleteShowItem(Track *track, ShowFunction *sf);

    /** Set the given track to active state */
    void activateTrack(Track *track);

    /** get the selected Show item. If none, returns NULL */
    ShowItem *getSelectedItem() const;

private:
    /** Retrieve the index of the given Track.
     *  If trk is NULL, this function returns the currently
     *  selected track.
     */
    int getTrackIndex(const Track *trk) const;

    void setItemCommonProperties(ShowItem *item, ShowFunction *func, int trackNum);

    /*********************************************************************
     * Header
     *********************************************************************/
public:
    /** Set the type of header. Can be Time (seconds) or BPM,
     *  in various forms (4/4, 3/4) */
    void setHeaderType(Show::TimeDivision type);

    Show::TimeDivision getHeaderType() const;

    /** When BPM is selected, this function can set a precise
     *  value of time division */
    void setBPMValue(int value);

    void setSnapToGrid(bool enable);

    /** Set the time (in msec) where bar 1 of the BPM grid starts */
    void setGridOffset(quint32 msec);

    /** QLC+DAN: note value of the BPM grid (see ShowHeaderItem) */
    void setGridDivision(const QString &division);

    /** Return the given time (in msec) snapped to the closest grid line.
     *  The start of the show (0) is always a valid snap point */
    quint32 snapTimeToGrid(quint32 time) const;

    /*********************************************************************
     * Markers (QLC+DAN)
     *********************************************************************/
public:
    /** Show these markers on the timeline (replaces any shown before) */
    void setMarkers(const QList<Show::Marker> &markers);

private:
    /** Place the markers for the current zoom level and view size */
    void layoutMarkers();
    /** Stretch each marker's coloured section up to the next marker */
    void layoutMarkerBands();
    /** Tell the markers where the visible part of the timeline starts */
    void updateMarkersViewLeft();
    void clearMarkers();
    /** Time for a marker at scene position x, snapped unless noSnap */
    quint32 markerTimeFromPosition(qreal x, bool noSnap) const;

    QList<ShowMarkerItem *> m_markerItems;
    QGraphicsSimpleTextItem *m_markerLaneLabel;
    quint32 m_markerDragTime;

protected slots:
    void slotMarkerClicked(ShowMarkerItem *item);
    void slotMarkerDragged(ShowMarkerItem *item, qreal x, bool noSnap);
    void slotMarkerDropped(ShowMarkerItem *item);
    void slotMarkerDoubleClicked(ShowMarkerItem *item);
    void slotMarkerContextMenu(ShowMarkerItem *item, QPoint screenPos);
    void slotMarkerLaneDoubleClicked(qreal x, bool noSnap);
    void slotMarkerLaneContextMenu(qreal x, QPoint screenPos);

signals:
    void markerAddRequested(quint32 time);
    void markerJumpRequested(quint32 time);
    void markerMoved(int index, quint32 time);
    void markerRenameRequested(int index);
    void markerDeleteRequested(int index);
    void markerColorRequested(int index, QColor color);

    /*********************************************************************
     * Cursor
     *********************************************************************/
public:
    /** Move cursor to a given time */
    void moveCursor(quint32 timePos);

    /** Reset cursor to initial position */
    void rewindCursor();

    /** Get time in milliseconds of the current cursor position */
    quint32 getTimeFromCursor() const;

    /** The exact time the cursor was last set to (not rounded to pixels) */
    quint32 cursorTime() const;

    /** Return position in pixel of a given time (in msec) */
    quint32 getPositionFromTime(quint32 time) const;

    /** Return the time (in msec) from a given X position */
    quint32 getTimeFromPosition(qreal pos) const;

private:
    QGraphicsScene *m_scene;
    QSlider *m_timeSlider;
    ShowHeaderItem *m_header;
    ShowCursorItem *m_cursor;
    QGraphicsItem * m_vdivider;
    QList <QGraphicsItem *> m_hdividers;
    QList <TrackItem *> m_tracks;
    QList <ShowItem *>m_items;
    bool m_snapToGrid;

public slots:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    /* QLC+DAN: hold the middle mouse button (press the wheel) and drag to
     * move around the timeline, like the hand tool in a DAW */
    bool m_panning;
    QPoint m_panStartPos;
    int m_panStartH;
    int m_panStartV;

protected slots:
    void slotHeaderClicked(QGraphicsSceneMouseEvent *event);
    void slotTimeScaleChanged(int val);

public:
    /** QLC+DAN: zoom level for a position of the zoom slider. 1 and up are the
     *  original levels; 0, -1 and -2 zoom in further (1/2, 1/4, 1/8) */
    static double timeScaleFromSlider(int value);
    void slotTrackClicked(TrackItem *track);
    void slotTrackDoubleClicked(TrackItem *track);
    void slotTrackSoloFlagChanged(TrackItem*, bool);
    void slotTrackMuteFlagChanged(TrackItem*, bool);
    void slotViewScrolled(int);

    void slotItemMoved(QGraphicsSceneMouseEvent *event, ShowItem *item);
    void slotAlignToCursor(ShowItem *item);

signals:
    void showItemMoved(ShowItem *item, quint32 time, bool moved);
    void viewClicked(QMouseEvent * e);
    void timeChanged(quint32 msec);
    void trackClicked(Track *track);
    void trackDoubleClicked(Track *track);
    void trackMoved(Track *, int);
    void trackDelete(Track *);
};

/** @} */

#endif
