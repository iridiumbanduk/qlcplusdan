/*
  Q Light Controller Plus
  audioitem.cpp

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

#include <QApplication>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <qmath.h>
#include <QDebug>
#include <QMenu>

#include "audioitem.h"
#include "trackitem.h"
#include "audiodecoder.h"
#include "audioplugincache.h"

AudioItem::AudioItem(Audio *aud, ShowFunction *func)
    : ShowItem(func)
    , m_audio(aud)
    , m_previewLeftAction(NULL)
    , m_previewRightAction(NULL)
    , m_previewStereoAction(NULL)
    , m_preview(NULL)
    , m_previewGeneration(0)
{
    Q_ASSERT(aud != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::AudioType));

    if (func->duration() == 0)
        func->setDuration(aud->totalDuration());

    calculateWidth();
    connect(m_audio, SIGNAL(changed(quint32)),
            this, SLOT(slotAudioChanged(quint32)));

    /* Preview actions */
    m_previewLeftAction = new QAction(tr("Preview Left Channel"), this);
    m_previewLeftAction->setCheckable(true);
    connect(m_previewLeftAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAudioPreviewLeft()));
    m_previewRightAction = new QAction(tr("Preview Right Channel"), this);
    m_previewRightAction->setCheckable(true);
    connect(m_previewRightAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAudioPreviewRight()));
    m_previewStereoAction = new QAction(tr("Preview Stereo Channels"), this);
    m_previewStereoAction->setCheckable(true);
    connect(m_previewStereoAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAudioPreviewStereo()));

    /* QLC+DAN: lets paint() draw only the visible part of a long waveform */
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);

    /* QLC+DAN: show the left channel (or mono) waveform straight away */
    m_previewLeftAction->setChecked(true);
    updateWaveformPreview();
}

AudioItem::~AudioItem()
{
    stopWaveformPreview();
    delete m_preview;
}

void AudioItem::calculateWidth()
{
    int newWidth = 0;
    qint64 audio_duration = m_audio->totalDuration();

    if (audio_duration != 0)
        newWidth = ((50/(float)getTimeScale()) * (float)audio_duration) / 1000;
    else
        newWidth = 100;

    if (newWidth < (50 / m_timeScale))
        newWidth = 50 / m_timeScale;
    setWidth(newWidth);
}

void AudioItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    float timeScale = 50/(float)m_timeScale;

    ShowItem::paint(painter, option, widget);

    if (m_preview != NULL)
    {
        // show preview here
        if (m_width <= 16000)
        {
            if (m_previewScaled.isNull() || m_previewScaled.width() != m_width)
                m_previewScaled = m_preview->scaled(m_width, TRACK_HEIGHT - 4);
            painter->drawPixmap(0, 0, m_previewScaled);
        }
        else
        {
            // QLC+DAN: at the closest zoom levels a scaled copy of the whole
            // song would be enormous, so only draw the part on screen
            m_previewScaled = QPixmap();
            QRectF target = option->exposedRect.intersected(QRectF(0, 0, m_width, TRACK_HEIGHT - 4));
            if (target.isEmpty() == false)
            {
                double sx = (double)m_preview->width() / (double)m_width;
                double sy = (double)m_preview->height() / (double)(TRACK_HEIGHT - 4);
                QRectF source(target.x() * sx, target.y() * sy, target.width() * sx, target.height() * sy);
                painter->drawPixmap(target, *m_preview, source);
            }
        }
    }

    if (m_audio->fadeInSpeed() != 0)
    {
        int fadeXpos = (timeScale * (float)m_audio->fadeInSpeed()) / 1000;
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawLine(1, TRACK_HEIGHT - 4, fadeXpos, 2);
    }

    if (m_audio->fadeOutSpeed() != 0)
    {
        int fadeXpos = (timeScale * (float)m_audio->fadeOutSpeed()) / 1000;
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawLine(m_width - fadeXpos, 2, m_width - 1, TRACK_HEIGHT - 4);
    }

    ShowItem::postPaint(painter);
}

void AudioItem::setTimeScale(double val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void AudioItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(msec)
    Q_UNUSED(stretch)
    // nothing to do
}

QString AudioItem::functionName() const
{
    if (m_audio)
        return m_audio->name();
    return QString();
}

Audio *AudioItem::getAudio() const
{
    return m_audio;
}

void AudioItem::updateWaveformPreview()
{
    stopWaveformPreview();
    m_previewGeneration++;

    bool left = m_previewLeftAction->isChecked() || m_previewStereoAction->isChecked();
    bool right = m_previewRightAction->isChecked() || m_previewStereoAction->isChecked();

    if ((left == false && right == false) || m_audio->getAudioDecoder() == NULL)
    {
        setWaveformPreview(QImage(), m_previewGeneration);
        return;
    }

    /* A decoder of its own, so reading the file doesn't disturb playback */
    AudioDecoder *ad = m_audio->doc()->audioPluginCache()->getDecoderForFile(m_audio->getSourceFileName());
    if (ad == NULL)
    {
        setWaveformPreview(QImage(), m_previewGeneration);
        return;
    }

    PreviewThread *waveformThread = new PreviewThread(this, ad, left, right,
                                                      m_audio->totalDuration(), m_previewGeneration);
    connect(waveformThread, SIGNAL(finished()), waveformThread, SLOT(deleteLater()));
    m_previewThread = waveformThread;
    waveformThread->start();
}

void AudioItem::stopWaveformPreview()
{
    if (m_previewThread.isNull() == false && m_previewThread->isRunning())
    {
        m_previewThread->requestInterruption();
        m_previewThread->wait();
    }
}

void AudioItem::setWaveformPreview(const QImage &image, int generation)
{
    if (generation != m_previewGeneration)
        return;

    delete m_preview;
    m_preview = image.isNull() ? NULL : new QPixmap(QPixmap::fromImage(image));
    m_previewScaled = QPixmap();
    update();
}

void AudioItem::slotAudioChanged(quint32)
{
    updateWaveformPreview();
    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_audio->totalDuration());
}

void AudioItem::slotAudioPreviewLeft()
{
    m_previewRightAction->setChecked(false);
    m_previewStereoAction->setChecked(false);
    updateWaveformPreview();
}

void AudioItem::slotAudioPreviewRight()
{
    m_previewLeftAction->setChecked(false);
    m_previewStereoAction->setChecked(false);
    updateWaveformPreview();
}

void AudioItem::slotAudioPreviewStereo()
{
    m_previewLeftAction->setChecked(false);
    m_previewRightAction->setChecked(false);
    updateWaveformPreview();
}

void AudioItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    if (m_audio->getAudioDecoder() != NULL)
    {
        AudioDecoder *ad = m_audio->getAudioDecoder();
        AudioParameters ap = ad->audioParameters();

        if (ap.channels() == 1)
            m_previewLeftAction->setText(tr("Preview Mono"));
        menu.addAction(m_previewLeftAction);
        if (ap.channels() == 2)
        {
            m_previewLeftAction->setText(tr("Preview Left Channel"));
            menu.addAction(m_previewRightAction);
            menu.addAction(m_previewStereoAction);
        }
        menu.addSeparator();
    }

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}

PreviewThread::PreviewThread(AudioItem *item, AudioDecoder *decoder,
                             bool left, bool right, qint64 duration, int generation)
    : m_item(item)
    , m_decoder(decoder)
    , m_left(left)
    , m_right(right)
    , m_duration(duration)
    , m_generation(generation)
{
}

PreviewThread::~PreviewThread()
{
    delete m_decoder;
}

qint32 PreviewThread::getSample(const unsigned char *data, quint32 idx, int sampleSize) const
{
    qint32 value = 0;
    if (sampleSize == 1)
    {
        value = (qint32)data[idx];
    }
    else if (sampleSize == 2)
    {
        qint16 *array = (qint16 *)data;
        value = array[idx / 2];
    }
    else if (sampleSize == 3 || sampleSize == 4)
    {
        qint32 *array = (qint32 *)data;
        value = array[idx / 4] >> 16;
    }
    //qDebug() << "sampleValue:" << value;
    return value;
}

void PreviewThread::run()
{
    bool left = m_left;
    bool right = m_right;
    QImage preview;

    {
        AudioDecoder *ad = m_decoder;
        AudioParameters ap = ad->audioParameters();
        ad->seek(0);
        // 1- find out how many samples have to be represented on a single pixel on a 1:1 time scale
        int sampleSize = ap.sampleSize();
        int channels = ap.channels();
        int oneSecondSamples = ap.sampleRate() * channels;
        int onePixelSamples = oneSecondSamples / 50;

        qint32 maxValue = 0;
        // 24 and 32 bit samples would produce a RMS too high, so let's
        // work on 16bit values
        if (sampleSize > 2)
            sampleSize = 2;

        if (left && right)
            maxValue = 0x7F << (8 * (sampleSize - 1));
        else
            maxValue = 0x3F << (8 * (sampleSize - 1));

        quint32 onePixelReadLen = onePixelSamples * sampleSize;

        // 2- decode the whole file and fill a QPixmap with a sample block RMS value for each pixel
        qint64 dataRead = 1;
        unsigned char audioData[onePixelReadLen * 4];
        quint32 audioDataOffset = 0;
        preview = QImage(qMax<qint64>(1, (50 * m_duration) / 1000), 76, QImage::Format_ARGB32_Premultiplied);
        preview.fill(Qt::transparent);
        QPainter p(&preview);
        int xpos = 0;

        qDebug() << "Audio duration:" << m_duration <<
                    ", channels:" << channels << ", pixmap width:" << preview.width() <<
                    ", maxValue:" << maxValue << ", samples:" << sampleSize;
        qDebug() << "Samples per second:" << oneSecondSamples << ", for one pixel:" << onePixelSamples <<
                    ", onePixelReadLen:" << onePixelReadLen;

        while (dataRead)
        {
            if (isInterruptionRequested())
                return;

            quint32 tmpExceedData = 0;
            if (audioDataOffset < onePixelReadLen)
            {
                dataRead = ad->read((char *)audioData + audioDataOffset, onePixelReadLen * 2);
                if (dataRead > 0)
                {
                    if ((quint32)dataRead + audioDataOffset >= onePixelReadLen)
                    {
                        tmpExceedData = (dataRead + audioDataOffset) - onePixelReadLen;
                        dataRead = onePixelReadLen;
                    }
                    else
                    {
                        qDebug() << "Not enough data. Requested:" << onePixelReadLen << "got:" << dataRead;
                        audioDataOffset = dataRead;
                        continue;
                    }
                }
            }
            else
            {
                dataRead = onePixelReadLen;
                tmpExceedData = audioDataOffset - onePixelReadLen;
            }

            if (dataRead == onePixelReadLen)
            {
                quint32 i = 0;
                // calculate the RMS value (peak) for this data block
                qint64 rmsLeft = 0;
                qint64 rmsRight = 0;
                bool done = false;
                while (!done)
                {
                    if (left)
                    {
                        qint32 sampleVal = getSample(audioData, i, sampleSize);
                        rmsLeft += (sampleVal * sampleVal);
                    }
                    i += sampleSize;

                    if (channels == 2)
                    {
                        if (right)
                        {
                            qint32 sampleVal = getSample(audioData, i, sampleSize);
                            rmsRight += (sampleVal * sampleVal);
                        }
                        i += sampleSize;
                    }

                    if (i >= dataRead)
                    {
                        //qDebug() << "i:" << i << "xpos:" << xpos;
                        done = true;
                    }
                }

                if (left)
                    rmsLeft = sqrt(rmsLeft / onePixelSamples);
                if (right)
                    rmsRight = sqrt(rmsRight / onePixelSamples);
                //qDebug() << "sample" << i << "RMS right:" << rmsRight << ", RMS left:" << rmsLeft;

                // 3- Draw the actual waveform
                unsigned short lineHeightLeft = 0, lineHeightRight = 0;

                if (left)
                    lineHeightLeft = (76 * rmsLeft) / maxValue;
                if (right)
                    lineHeightRight = (76 * rmsRight) / maxValue;

                if (left && right)
                {
                    if (lineHeightLeft > 1)
                        p.drawLine(xpos, 19 - (lineHeightLeft / 2), xpos, 19 + (lineHeightLeft / 2));
                    else
                        p.drawLine(xpos, 19, xpos + 1, 19);

                    if (lineHeightRight > 1)
                        p.drawLine(xpos, 51 - (lineHeightRight / 2), xpos, 51 + (lineHeightRight / 2));
                    else
                        p.drawLine(xpos, 51, xpos + 1, 51);
                }
                else
                {
                    unsigned short lineHeight = left ? lineHeightLeft : lineHeightRight;

                    if (lineHeight > 1)
                        p.drawLine(xpos, 38 - (lineHeight / 2), xpos, 38 + (lineHeight / 2));
                    else
                        p.drawLine(xpos, 38, xpos + 1, 38);
                    //qDebug() << "Data read: " << dataRead << ", rms: " << rmsRight << ", line height: " << lineHeight << ", xpos = " << xpos;
                }
                xpos++;

                if (tmpExceedData > 0)
                {
                    //qDebug() << "Exceed data found: " << tmpExceedData;
                    memmove(audioData, audioData + onePixelReadLen, tmpExceedData);
                    audioDataOffset = tmpExceedData;
                }
                else
                    audioDataOffset = 0;
            }
        }
        //qDebug() << "Iterations done: " << xpos;
    }

    /* Hand the picture to the item on the GUI thread. If the item has gone
     * by then, Qt drops the call. */
    AudioItem *item = m_item;
    int generation = m_generation;
    QMetaObject::invokeMethod(item, [item, preview, generation]()
        { item->setWaveformPreview(preview, generation); }, Qt::QueuedConnection);
}
