/*
  Q Light Controller Plus
  show.h

  Copyright (c) Massimo Callegari

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

#ifndef SHOW_H
#define SHOW_H

#include <QColor>
#include <QMutex>
#include <QList>
#include <QSet>

#include "function.h"
#include "track.h"

class QXmlStreamReader;
class ShowRunner;

/** @addtogroup engine_functions Functions
 * @{
 */

class Show final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(Show)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    Show(Doc* doc);
    virtual ~Show();

    /** @reimp */
    QIcon getIcon() const override;

    /** @reimp */
    quint32 totalDuration() override;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** Copy the contents for this function from another function */
    bool copyFrom(const Function* function) override;

    /*********************************************************************
     * Time division
     *********************************************************************/
public:
    enum TimeDivision
    {
        Time = 0,
        BPM_4_4,
        BPM_3_4,
        BPM_2_4,
        Invalid
    };
    Q_ENUM(TimeDivision)

    /** Set the show time division type (Time, BPM) */
    void setTimeDivision(Show::TimeDivision type, int BPM);

    Show::TimeDivision timeDivisionType() const;
    void setTimeDivisionType(Show::TimeDivision type);
    int beatsDivision() const;

    int timeDivisionBPM() const;
    void setTimeDivisionBPM(int BPM);

    /** Get/Set the grid offset in milliseconds. When a BPM time division
     *  is used, this is the time where the first bar of the grid starts.
     *  It is a design-time aid only and does not affect playback. */
    quint32 gridOffset() const;
    void setGridOffset(quint32 msec);

    /** QLC+DAN: note value of the BPM grid, like a DAW's grid setting:
     *  "bar", "1/2", "1/4" (the default), "1/8", "1/16", "1/32",
     *  or triplets "1/4T", "1/8T", "1/16T" */
    QString gridDivision() const;
    void setGridDivision(const QString &division);

    static QString tempoToString(Show::TimeDivision type);
    static Show::TimeDivision stringToTempo(const QString& tempo);

private:
    TimeDivision m_timeDivisionType;
    int m_timeDivisionBPM;
    quint32 m_gridOffset;
    QString m_gridDivision;

    /*********************************************************************
     * Markers (QLC+DAN)
     *********************************************************************/
public:
    /** A named position on the timeline, like the markers in a DAW.
     *  Design-time aid only: markers do not affect playback. */
    struct Marker
    {
        quint32 time;
        QString name;
        /** Colour of the section from this marker to the next one */
        QColor color;
    };

    /** All markers, sorted by time */
    QList<Marker> markers() const;

    /** Add a marker and return its index in markers() */
    int addMarker(quint32 time, const QString &name, const QColor &color = QColor());

    /** Change a marker. Returns its new index (the list stays sorted) */
    int setMarker(int index, quint32 time, const QString &name);

    void setMarkerColor(int index, const QColor &color);

    void removeMarker(int index);

private:
    void sortMarkers();
    QList<Marker> m_markers;

    /*********************************************************************
     * Tracks
     *********************************************************************/
public:
    /**
     * Add a track to this show. If the track is already a
     * member of the show, this call fails.
     *
     * @param id The track to add
     * @return true if successful, otherwise false
     */
    bool addTrack(Track *track, quint32 id = Track::invalidId());

    /**
     * Remove a track from this show. If the track is not a
     * member of the show, this call fails.
     *
     * @param id The track to remove
     * @return true if successful, otherwise false
     */
    bool removeTrack(quint32 id);

    /** Get a track by id */
    Track *track(quint32 id) const;

    /** Get a reference to a Track from the provided Scene ID */
    Track *getTrackFromSceneID(quint32 id) const;

    /** Get a reference to a Track from the provided ShowFunction ID */
    Track *getTrackFromShowFunctionID(quint32 id) const;

    /** Get the number of tracks in the Show */
    int getTracksCount() const;

    /** Move a track ID up or down */
    void moveTrack(Track *track, int direction);

    /** Get a list of available tracks */
    QList <Track*> tracks() const;

private:
    /** Create a new track ID */
    quint32 createTrackId();

protected:
    /** Map of the available tracks coupled by ID */
    QMap <quint32,Track*> m_tracks;

    /** Latest assigned track ID */
    quint32 m_latestTrackId;

    /*********************************************************************
     * Show Functions
     *********************************************************************/
public:
    /** Get a unique ID for the creation of a new ShowFunction */
    quint32 getLatestShowFunctionId();

    /** Get a reference to a ShowFunction from the provided uinique ID */
    ShowFunction *showFunction(quint32 id) const;

protected:
    /** Latest assigned unique ShowFunction ID */
    quint32 m_latestShowFunctionID;

    /*********************************************************************
     * Save & Load
     *********************************************************************/
public:
    /** Save function's contents to an XML document */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /** Load function's contents from an XML document */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    void postLoad() override;

public:
    /** @reimp */
    bool contains(quint32 functionId) const override;

    /** @reimp */
    QList<quint32> components() const override;

    /*********************************************************************
     * Running
     *********************************************************************/
public:
    /** @reimp */
    void preRun(MasterTimer* timer) override;

    /** @reimp */
    void setPause(bool enable) override;

    /** @reimp */
    void write(MasterTimer* timer, QList<Universe*> universes) override;

    /** @reimp */
    void postRun(MasterTimer* timer, QList<Universe*> universes) override;

protected slots:
    /** Called whenever one of this function's child functions stops */
    void slotChildStopped(quint32 fid);

signals:
    void timeChanged(quint32);
    void showFinished();

protected:
    ShowRunner *m_runner;
    /** Number of currently running children */
    QSet <quint32> m_runningChildren;

    /*************************************************************************
     * Attributes
     *************************************************************************/
public:
    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId = 0) override;
};

/** @} */

#endif
