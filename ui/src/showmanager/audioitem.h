/*
  Q Light Controller Plus
  audioitem.h

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

#ifndef AUDIOITEM_H
#define AUDIOITEM_H

#include <QGraphicsItem>
#include <QPointer>
#include <QObject>
#include <QAction>
#include <QThread>
#include <QImage>
#include <QFont>

#include "showitem.h"
#include "audio.h"

/** @addtogroup ui_functions
 * @{
 */

class AudioItem;
class AudioDecoder;

/**
 * Draws an audio waveform in the background.
 * QLC+DAN: the thread only gets copies of what it needs and hands the
 * finished picture back to the item on the GUI thread, so the item can
 * be deleted (e.g. switching shows) while a waveform is still being drawn.
 */
class PreviewThread final : public QThread
{
public:
    PreviewThread(AudioItem *item, AudioDecoder *decoder,
                  bool left, bool right, qint64 duration, int generation);
    ~PreviewThread();

private:
    /** Retrieve a sample value from an audio buffer, given the sample size */
    qint32 getSample(const unsigned char *data, quint32 idx, int sampleSize) const;
    void run() override;

    /** Safe to use while running: the item waits for this thread before it is deleted */
    AudioItem *m_item;
    AudioDecoder *m_decoder;
    bool m_left, m_right;
    qint64 m_duration;
    int m_generation;
};

/**
 *
 * Audio Item. Clickable and draggable object identifying an Audio object
 *
 */
class AudioItem final : public ShowItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    AudioItem(Audio *aud, ShowFunction *func);
    ~AudioItem();

    /** @reimp */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /** @reimp */
    void setTimeScale(double val) override;

    /** @reimp */
    void setDuration(quint32 msec, bool stretch) override;

    /** @reimp */
    QString functionName() const override;

    /** Return a pointer to a Audio Function associated to this item */
    Audio *getAudio() const;

protected:
    /** @reimp */
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

protected slots:
    void slotAudioChanged(quint32);

    void slotAudioPreviewLeft();
    void slotAudioPreviewRight();
    void slotAudioPreviewStereo();

private:
    /** Calculate sequence width for paint() and boundingRect() */
    void calculateWidth();

    /** Start a thread to elapse a waveform preview over the item */
    void updateWaveformPreview();

    /** Stop a waveform that is still being drawn, and wait for it */
    void stopWaveformPreview();

public:
    /** Called (on the GUI thread) when a waveform has been drawn */
    void setWaveformPreview(const QImage &image, int generation);

public:
    /** Reference to the actual Audio Function */
    Audio *m_audio;

    /** Context menu actions */
    QAction *m_previewLeftAction;
    QAction *m_previewRightAction;
    QAction *m_previewStereoAction;

    /** Pixmap holding the waveform (if enabled) */
    QPixmap *m_preview;

private:
    QPointer<PreviewThread> m_previewThread;
    int m_previewGeneration;
    /** The waveform scaled to the item's width, so it isn't rescaled on every paint */
    QPixmap m_previewScaled;
};


/** @} */

#endif
