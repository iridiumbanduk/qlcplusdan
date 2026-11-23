/*
  Q Light Controller Plus
  showitem.cpp

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

#include <QGraphicsSceneEvent>
#include <QGraphicsSceneHoverEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QMenu>

#include "headeritems.h"
#include "trackitem.h"
#include "showitem.h"
#include "function.h"

ShowItem::ShowItem(ShowFunction *function, QObject *)
    : m_color(100, 100, 100)
    , m_locked(false)
    , m_pressed(false)
    , m_width(50)
    , m_timeScale(3)
    , m_trackIdx(-1)
    , m_function(function)
    , m_header(NULL)
    , m_alignToCursor(NULL)
    , m_lockAction(NULL)
{
    Q_ASSERT(function != NULL);

    setCursor(Qt::OpenHandCursor);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    // QLC+DAN: dragging the right edge changes the length
    m_resizing = false;
    m_resizePreviewEnd = 0;
    setAcceptHoverEvents(true);
    m_endAtCursorAction = new QAction(tr("End at cursor"), this);
    connect(m_endAtCursorAction, &QAction::triggered, this, [this]() { emit endAtCursor(this); });

    m_font = qApp->font();
    m_font.setBold(true);
    m_font.setPixelSize(12);

    setLocked(m_function->isLocked());

    m_alignToCursor = new QAction(tr("Align to cursor"), this);
    connect(m_alignToCursor, SIGNAL(triggered()),
            this, SLOT(slotAlignToCursorClicked()));
    m_lockAction = new QAction(tr("Lock item"), this);
    connect(m_lockAction, SIGNAL(triggered()),
            this, SLOT(slotLockItemClicked()));
}

void ShowItem::setHeader(const ShowHeaderItem *header)
{
    m_header = header;
    updateTooltip();
}

void ShowItem::updateTooltip()
{
    if (m_function == NULL)
        return;

    QString start = Function::speedToString(m_function->startTime());
    QString duration = Function::speedToString(getDuration());

    // QLC+DAN: in BPM shows, also in bars|beats|ticks
    QString startBars, durationBars;
    if (m_header != NULL)
    {
        startBars = m_header->positionToBars(m_function->startTime());
        durationBars = m_header->lengthToBars(getDuration());
    }

    if (startBars.isEmpty())
    {
        setToolTip(QString(tr("Name: %1\nStart time: %2\nDuration: %3\n%4"))
                  .arg(functionName()).arg(start).arg(duration)
                  .arg(tr("Click to move this item along the timeline")));
    }
    else
    {
        setToolTip(QString(tr("Name: %1\nStart time: %2   (bar %3)\nDuration: %4   (%5)\n%6\n%7"))
                  .arg(functionName()).arg(start).arg(startBars).arg(duration).arg(durationBars)
                  .arg(tr("Bars shown as bar|beat|tick (960 ticks per beat)"))
                  .arg(tr("Click to move this item along the timeline")));
    }
}

QList<QAction *> ShowItem::getDefaultActions() const
{
    QList<QAction *> actions;
    actions.append(m_alignToCursor);
    if (canResize() && isLocked() == false)
        actions.append(m_endAtCursorAction);

    if (isLocked())
    {
        m_lockAction->setText(tr("Unlock item"));
        m_lockAction->setIcon(QIcon(":/unlock.png"));
    }
    else
    {
        m_lockAction->setText(tr("Lock item"));
        m_lockAction->setIcon(QIcon(":/lock.png"));
    }
    actions.append(m_lockAction);

    return actions;
}

void ShowItem::setTimeScale(double val)
{
    prepareGeometryChange();
    m_timeScale = val;
}

double ShowItem::getTimeScale() const
{
    return m_timeScale;
}

void ShowItem::setStartTime(quint32 time)
{
    if (m_function == NULL)
        return;

    m_function->setStartTime(time);
    updateTooltip();
}

quint32 ShowItem::getStartTime() const
{
    if (m_function)
        return m_function->startTime();
    return 0;
}

void ShowItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(stretch)

    if (m_function == NULL)
        return;

    m_function->setDuration(msec);
    updateTooltip();
}

quint32 ShowItem::getDuration() const
{
    if (m_function)
        return m_function->duration();
    return 0;
}

void ShowItem::setWidth(int w)
{
    m_width = w;
    updateTooltip();
}

int ShowItem::getWidth() const
{
    return m_width;
}

QPointF ShowItem::getDraggingPos() const
{
    return m_pos;
}

void ShowItem::setTrackIndex(int idx)
{
    m_trackIdx = idx;
}

int ShowItem::getTrackIndex() const
{
    return m_trackIdx;
}

void ShowItem::setColor(QColor col)
{
    m_color = col;
    if (m_function)
        m_function->setColor(col);
    update();
}

QColor ShowItem::getColor() const
{
    return m_color;
}

void ShowItem::setLocked(bool locked)
{
    m_locked = locked;
    if (m_function)
        m_function->setLocked(locked);
    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    update();
}

bool ShowItem::isLocked() const
{
    return m_locked;
}

void ShowItem::setFunctionID(quint32 id)
{
    if (m_function != NULL)
        m_function->setFunctionID(id);
}

quint32 ShowItem::functionID() const
{
    if (m_function != NULL)
        return m_function->functionID();

    return Function::invalidId();
}

ShowFunction *ShowItem::showFunction() const
{
    return m_function;
}

QString ShowItem::functionName() const
{
    return QString();
}

void ShowItem::slotAlignToCursorClicked()
{
    emit alignToCursor(this);
}

void ShowItem::slotLockItemClicked()
{
    setLocked(!isLocked());
    //update();
}

bool ShowItem::resizeTo(quint32 durationMs, QString *error)
{
    Q_UNUSED(durationMs);
    if (error != NULL)
        *error = tr("The length of this item can't be changed");
    return false;
}

bool ShowItem::onResizeEdge(qreal x) const
{
    if (canResize() == false || isLocked())
        return false;
    // a few pixels at the right edge, less on very short items
    qreal zone = qMin<qreal>(6.0, m_width / 3.0);
    return x >= m_width - zone;
}

void ShowItem::setResizePreview(quint32 endMs)
{
    m_resizePreviewEnd = endMs;
    update();
}

void ShowItem::restoreWidth()
{
    // the subclasses work out their width again when the scale is set
    prepareGeometryChange();
    setTimeScale(m_timeScale);
    update();
}

void ShowItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_resizing == false)
        setCursor(onResizeEdge(event->pos().x()) ? Qt::SizeHorCursor : Qt::OpenHandCursor);
    QGraphicsItem::hoverMoveEvent(event);
}

void ShowItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizing)
    {
        emit resizing(this, event->scenePos().x(), event->modifiers() & Qt::ShiftModifier);
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void ShowItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // QLC+DAN: grabbing the right edge changes the length instead of moving
    if (event->button() == Qt::LeftButton && onResizeEdge(event->pos().x()))
    {
        m_resizing = true;
        m_resizePreviewEnd = m_function->startTime() + getDuration();
        setSelected(true);
        event->accept();
        return;
    }

    QGraphicsItem::mousePressEvent(event);
    m_pos = this->pos();
    if (event->button() == Qt::LeftButton)
        m_pressed = true;
    this->setSelected(true);
}

void ShowItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizing)
    {
        m_resizing = false;
        setCursor(Qt::OpenHandCursor);
        update();
        emit resizeFinished(this);
        return;
    }

    QGraphicsItem::mouseReleaseEvent(event);
    qDebug() << Q_FUNC_INFO << "mouse RELEASE event - <" << event->pos().toPoint().x() << "> - <" << event->pos().toPoint().y() << ">";
    setCursor(Qt::OpenHandCursor);
    m_pressed = false;
    emit itemDropped(event, this);
}

void ShowItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    Q_UNUSED(event)

    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    menu.addAction(m_alignToCursor);
    if (isLocked())
    {
        m_lockAction->setText(tr("Unlock item"));
        m_lockAction->setIcon(QIcon(":/unlock.png"));
    }
    else
    {
        m_lockAction->setText(tr("Lock item"));
        m_lockAction->setIcon(QIcon(":/lock.png"));
    }
    menu.addAction(m_lockAction);
    menu.exec(QCursor::pos());
}


QRectF ShowItem::boundingRect() const
{
    return QRectF(0, 0, m_width, TRACK_HEIGHT - 3);
}

void ShowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (this->isSelected() == true)
        painter->setPen(QPen(Qt::white, 3));
    else
        painter->setPen(QPen(Qt::white, 1));

    // draw item background
    painter->setBrush(QBrush(m_color));
    painter->drawRect(0, 0, m_width, TRACK_HEIGHT - 3);

    painter->setFont(m_font);
}

void ShowItem::postPaint(QPainter *painter)
{
    // draw the function name shadow
    painter->setPen(QPen(QColor(10, 10, 10, 150), 2));
    painter->drawText(QRect(4, 6, m_width - 6, 71), Qt::AlignLeft | Qt::TextWordWrap, functionName());

    // draw the function name
    painter->setPen(QPen(QColor(220, 220, 220, 255), 2));
    painter->drawText(QRect(3, 5, m_width - 5, 72), Qt::AlignLeft | Qt::TextWordWrap, functionName());

    if (m_locked)
        painter->drawPixmap(3, TRACK_HEIGHT >> 1, 24, 24, QIcon(":/lock.png").pixmap(24, 24));

    if (m_pressed)
    {
        quint32 s_time = 0;
        if (x() > TRACK_WIDTH)
            s_time = (double)(x() - TRACK_WIDTH - 2) * (m_timeScale * 500) /
                     (double)(HALF_SECOND_WIDTH);
        QString text = Function::speedToString(s_time);
        // QLC+DAN: and where that is in bars and beats
        if (m_header != NULL && m_header->positionToBars(s_time).isEmpty() == false)
            text += "   " + m_header->positionToBars(s_time);
        painter->drawText(3, TRACK_HEIGHT - 10, text);
    }

    // QLC+DAN: where the end will be while its edge is being dragged
    if (m_resizing)
    {
        QString text = tr("End: %1").arg(Function::speedToString(m_resizePreviewEnd));
        if (m_header != NULL && m_header->positionToBars(m_resizePreviewEnd).isEmpty() == false)
            text += "   " + m_header->positionToBars(m_resizePreviewEnd);
        QFontMetrics fm(painter->font());
        int w = fm.horizontalAdvance(text) + 6;
        int x = qMax(3, m_width - w);
        painter->fillRect(QRect(x - 2, TRACK_HEIGHT - 24, w, 16), QColor(0, 0, 0, 140));
        painter->setPen(Qt::white);
        painter->drawText(x, TRACK_HEIGHT - 12, text);
    }
}


