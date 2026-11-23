/*
  Q Light Controller Plus
  sequenceitem.cpp

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
#include <QPainter>
#include <QMenu>

#include "sequenceitem.h"
#include "chaserstep.h"
#include "trackitem.h"

SequenceItem::SequenceItem(Chaser *seq, ShowFunction *func)
    : ShowItem(func)
    , m_chaser(seq)
    , m_selectedStep(-1)
{
    Q_ASSERT(seq != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::ChaserType));

    if (func->duration() == 0)
        func->setDuration(seq->totalDuration());

    calculateWidth();

    connect(m_chaser, SIGNAL(changed(quint32)),
            this, SLOT(slotSequenceChanged(quint32)));
}

void SequenceItem::calculateWidth()
{
    int newWidth = 0;
    quint32 seq_duration = m_chaser->totalDuration();
    float timeUnit = 50.0 / float(getTimeScale());

    if (seq_duration == Function::infiniteSpeed())
    {
        newWidth = timeUnit * 10000;
    }
    else
    {
        if (seq_duration != 0)
            newWidth = (timeUnit * float(seq_duration)) / 1000.0;

        if (newWidth < timeUnit)
            newWidth = timeUnit;
    }
    setWidth(newWidth);
}

void SequenceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    float xpos = 0;
    float timeUnit = 50.0 / float(m_timeScale);
    int stepIdx = 0;

    ShowItem::paint(painter, option, widget);

    if (this->isSelected() == false)
        m_selectedStep = -1;

    foreach (ChaserStep step, m_chaser->steps())
    {
        uint stepFadeIn = step.fadeIn;
        uint stepFadeOut = step.fadeOut;
        uint stepDuration = step.duration;

        if (m_chaser->fadeInMode() == Chaser::Common)
            stepFadeIn = m_chaser->fadeInSpeed();
        if (m_chaser->fadeOutMode() == Chaser::Common)
            stepFadeOut = m_chaser->fadeOutSpeed();
        if (m_chaser->durationMode() == Chaser::Common)
            stepDuration = m_chaser->duration();

        // avoid hanging on infinite duration
        if (stepDuration == Function::infiniteSpeed())
            stepDuration = 10 * 1000 * 1000;

        // draw fade in line
        if (stepFadeIn > 0)
        {
            int fadeXpos = xpos + ((timeUnit * (float)stepFadeIn) / 1000);
            // doesn't even draw it if too small
            if (fadeXpos - xpos > 5)
            {
                painter->setPen(QPen(Qt::gray, 1));
                painter->drawLine(xpos, TRACK_HEIGHT - 4, fadeXpos, 1);
            }
        }
        float stepWidth = ((timeUnit * (float)stepDuration) / 1000);
        // draw selected step
        if (stepIdx == m_selectedStep)
        {
            painter->setPen(QPen(Qt::yellow, 2));
            painter->setBrush(QBrush(Qt::NoBrush));
            painter->drawRect(xpos, 0, stepWidth, TRACK_HEIGHT - 3);
        }

        QRect textRect = QRect(xpos + 1, 0, stepWidth - 1, TRACK_HEIGHT - 3);
        painter->drawText(textRect, Qt::AlignBottom, step.note);

        xpos += stepWidth;

        // draw step vertical delimiter
        painter->setPen(QPen(Qt::white, 1));
        painter->drawLine(xpos, 1, xpos, TRACK_HEIGHT - 5);

        // draw fade out line
        if (stepFadeOut > 0)
        {
            int fadeXpos = xpos + ((timeUnit * (float)stepFadeOut) / 1000);
            // doesn't even draw it if too small
            if (fadeXpos - xpos > 5)
            {
                painter->setPen(QPen(Qt::gray, 1));
                painter->drawLine(xpos, 1, fadeXpos, TRACK_HEIGHT - 4);
            }
        }
        stepIdx++;
    }

    ShowItem::postPaint(painter);
}

void SequenceItem::setTimeScale(double val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void SequenceItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(stretch)
    m_chaser->setTotalDuration(msec);
}

bool SequenceItem::canResize() const
{
    return m_chaser != NULL && m_chaser->stepsCount() > 0;
}

bool SequenceItem::resizeTo(quint32 durationMs, QString *error)
{
    // QLC+DAN: only the last step changes, so the rest of the sequence keeps its timing
    int count = m_chaser->stepsCount();
    if (count == 0)
    {
        if (error) *error = tr("This sequence has no steps");
        return false;
    }

    if (m_chaser->durationMode() == Chaser::Common)
    {
        uint common = m_chaser->duration();
        if (common == Function::infiniteSpeed())
        {
            if (error) *error = tr("The steps of this sequence hold forever");
            return false;
        }
        if (count == 1)
        {
            m_chaser->setDuration(durationMs);
            return true;
        }
        if (durationMs <= (count - 1) * common + 10)
        {
            if (error) *error = tr("The last step starts at %1 into the sequence, so it can't end before that")
                                  .arg(Function::speedToString((count - 1) * common));
            return false;
        }
        // to change only the last step, each step needs its own time:
        // they all keep the common one
        for (int i = 0; i < count; i++)
        {
            ChaserStep step = *m_chaser->stepAt(i);
            uint fadeIn = (m_chaser->fadeInMode() == Chaser::Common) ? m_chaser->fadeInSpeed() : step.fadeIn;
            step.duration = common;
            step.hold = Function::speedSubtract(common, fadeIn);
            m_chaser->replaceStep(step, i);
        }
        m_chaser->setDurationMode(Chaser::PerStep);
    }

    quint32 before = 0;
    for (int i = 0; i < count - 1; i++)
    {
        uint d = m_chaser->stepAt(i)->duration;
        if (d == Function::infiniteSpeed())
        {
            if (error) *error = tr("Step %1 holds forever, so the sequence has no end").arg(i + 1);
            return false;
        }
        before += d;
    }

    if (durationMs <= before + 10)
    {
        if (error) *error = tr("The last step starts at %1 into the sequence, so it can't end before that")
                              .arg(Function::speedToString(before));
        return false;
    }

    ChaserStep last = *m_chaser->stepAt(count - 1);
    last.duration = durationMs - before;
    uint fadeIn = (m_chaser->fadeInMode() == Chaser::Common) ? m_chaser->fadeInSpeed() : last.fadeIn;
    if (m_chaser->fadeInMode() == Chaser::PerStep && last.fadeIn > last.duration)
    {
        last.fadeIn = last.duration;
        fadeIn = last.fadeIn;
    }
    last.hold = Function::speedSubtract(last.duration, fadeIn);
    m_chaser->replaceStep(last, count - 1);
    return true;
}

QString SequenceItem::functionName() const
{
    if (m_chaser)
        return m_chaser->name();
    return QString();
}

void SequenceItem::setSelectedStep(int idx)
{
    m_selectedStep = idx;
    update();
}

Chaser *SequenceItem::getChaser() const
{
    return m_chaser;
}

void SequenceItem::slotSequenceChanged(quint32)
{
    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_chaser->totalDuration());
    updateTooltip();
}

void SequenceItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}
