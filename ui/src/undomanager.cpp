/*
  Q Light Controller Plus
  undomanager.cpp

  QLC+DAN: project-wide Undo / Redo. See undomanager.h for how it works.

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

#include <QCryptographicHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QApplication>
#include <QTransform>
#include <QPixmap>
#include <QAction>
#include <QDialog>
#include <QBuffer>
#include <QDebug>
#include <QIcon>

#include "undomanager.h"
#include "functionmanager.h"
#include "virtualconsole.h"
#include "showmanager.h"
#include "mastertimer.h"
#include "monitor.h"
#include "function.h"
#include "doc.h"

#define KXMLQLCWorkspace QStringLiteral("Workspace")
#define KXMLQLCEngine QStringLiteral("Engine")
#define KXMLQLCVirtualConsole QStringLiteral("VirtualConsole")

static QByteArray hashOf(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Md5);
}

UndoManager::UndoManager(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_waitingBaseline(true)
    , m_restoring(false)
{
    Q_ASSERT(doc != NULL);

    m_undoAction = new QAction(QIcon(":/undo.png"), tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence("CTRL+Z"));
    connect(m_undoAction, SIGNAL(triggered()), this, SLOT(undo()));

    QPixmap redoPm = QPixmap(":/undo.png").transformed(QTransform().scale(-1, 1));
    m_redoAction = new QAction(QIcon(redoPm), tr("Redo"), this);
    m_redoAction->setShortcuts(QList<QKeySequence>() << QKeySequence("CTRL+Y") << QKeySequence("CTRL+SHIFT+Z"));
    connect(m_redoAction, SIGNAL(triggered()), this, SLOT(redo()));

    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(SettleMs);
    connect(&m_settleTimer, SIGNAL(timeout()), this, SLOT(slotSettled()));

    // keep the buttons' enabled state and tooltips up to date
    m_actionsTimer.setInterval(300);
    connect(&m_actionsTimer, SIGNAL(timeout()), this, SLOT(slotUpdateActions()));
    m_actionsTimer.start();

    connect(m_doc, SIGNAL(modified(bool)), this, SLOT(slotDocModified(bool)));
    connect(m_doc, SIGNAL(clearing()), this, SLOT(slotDocClearing()));
    // a project can also be loaded without clearing first (e.g. on startup)
    connect(m_doc, SIGNAL(loading()), this, SLOT(slotDocClearing()));

    // the starting point, once the application has finished starting up
    QTimer::singleShot(0, this, SLOT(slotTakeBaseline()));
    slotUpdateActions();
}

UndoManager::~UndoManager()
{
}

/*********************************************************************
 * Snapshots
 *********************************************************************/

QByteArray UndoManager::takeSnapshot() const
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    // formatted exactly like a project file: some Virtual Console loaders rely on it
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(1);
    writer.writeStartDocument();
    writer.writeStartElement(KXMLQLCWorkspace);

    m_doc->saveXML(&writer);
    if (VirtualConsole::instance() != NULL)
        VirtualConsole::instance()->saveXML(&writer);

    writer.writeEndElement();
    writer.writeEndDocument();
    buffer.close();

    QByteArray xml = buffer.data();

    // The I/O patching is never restored, so leave it out: changing it
    // shouldn't create undo steps that do nothing
    int start = xml.indexOf("<InputOutputMap");
    if (start >= 0)
    {
        int end = -1;
        int close = xml.indexOf("</InputOutputMap>", start);
        int selfClose = xml.indexOf("/>", start);
        int firstTagEnd = xml.indexOf('>', start);
        if (selfClose >= 0 && selfClose + 1 == firstTagEnd)
            end = selfClose + 2;
        else if (close >= 0)
            end = close + int(strlen("</InputOutputMap>"));
        if (end > start)
            xml.remove(start, end - start);
    }

    return xml;
}

void UndoManager::slotTakeBaseline()
{
    if (m_doc->loadStatus() == Doc::Loading)
    {
        // a project is still being loaded: try again in a moment
        QTimer::singleShot(50, this, SLOT(slotTakeBaseline()));
        return;
    }

    QByteArray snap = takeSnapshot();
    m_current = qCompress(snap);
    m_currentHash = hashOf(snap);
    m_savedHash = m_doc->isModified() ? QByteArray() : m_currentHash;
    m_waitingBaseline = false;
    slotUpdateActions();
}

void UndoManager::resetHistory()
{
    m_settleTimer.stop();
    m_undoStack.clear();
    m_redoStack.clear();
    m_current.clear();
    m_currentHash.clear();
    m_savedHash.clear();
    m_waitingBaseline = true;
    QTimer::singleShot(0, this, SLOT(slotTakeBaseline()));
    slotUpdateActions();
}

void UndoManager::flush()
{
    if (m_waitingBaseline || m_restoring)
        return;

    m_settleTimer.stop();

    QByteArray snap = takeSnapshot();
    QByteArray hash = hashOf(snap);

    if (m_current.isEmpty())
    {
        m_current = qCompress(snap);
        m_currentHash = hash;
        return;
    }

    if (hash == m_currentHash)
        return; // nothing actually changed

    m_undoStack.append(m_current);
    while (m_undoStack.count() > MaxSteps)
        m_undoStack.removeFirst();
    m_redoStack.clear();

    m_current = qCompress(snap);
    m_currentHash = hash;
    slotUpdateActions();
}

void UndoManager::slotSettled()
{
    if (m_doc->loadStatus() == Doc::Loading)
    {
        m_settleTimer.start();
        return;
    }
    flush();
}

void UndoManager::slotDocModified(bool modified)
{
    if (m_restoring || m_waitingBaseline)
        return;

    if (modified)
    {
        // take a copy once the changes have settled
        m_settleTimer.start();
    }
    else
    {
        // the project has just been saved: this is the saved state
        flush();
        m_savedHash = m_currentHash;
    }
}

void UndoManager::slotDocClearing()
{
    if (m_restoring)
        return;

    // a new or different project: start a new history once it has loaded
    resetHistory();
}

/*********************************************************************
 * Undo / Redo
 *********************************************************************/

QString UndoManager::blockedReason() const
{
    if (m_restoring || m_waitingBaseline || m_doc->loadStatus() == Doc::Loading)
        return tr("The project is loading");
    if (m_doc->mode() == Doc::Operate)
        return tr("Switch to Design mode to undo or redo");
    if (m_doc->masterTimer()->runningFunctions() > 0)
        return tr("Stop everything that is playing to undo or redo");
    if (VirtualConsole::instance() != NULL && VirtualConsole::instance()->liveEdit())
        return tr("Leave Virtual Console live edit to undo or redo");
    if (QApplication::mouseButtons() != Qt::NoButton)
        return tr("Release the mouse button to undo or redo");

    foreach (QWidget *w, QApplication::topLevelWidgets())
    {
        if (w->isVisible() && qobject_cast<QDialog *>(w) != NULL)
            return tr("Close the open dialog to undo or redo");
    }

    return QString();
}

void UndoManager::slotUpdateActions()
{
    QString reason = blockedReason();
    bool ok = reason.isEmpty();
    bool pending = m_settleTimer.isActive();

    // a change waiting to be copied can always be undone
    m_undoAction->setEnabled(ok && (m_undoStack.isEmpty() == false || pending));
    m_redoAction->setEnabled(ok && m_redoStack.isEmpty() == false && pending == false);

    QString undoTip = tr("Undo (Ctrl+Z)");
    QString redoTip = tr("Redo (Ctrl+Y)");
    if (!ok)
    {
        undoTip += "\n" + reason;
        redoTip += "\n" + reason;
    }
    m_undoAction->setToolTip(undoTip);
    m_redoAction->setToolTip(redoTip);
}

void UndoManager::undo()
{
    step(m_undoStack, m_redoStack);
}

void UndoManager::redo()
{
    step(m_redoStack, m_undoStack);
}

void UndoManager::step(QList<QByteArray> &from, QList<QByteArray> &to)
{
    if (blockedReason().isEmpty() == false)
        return;

    // a change still waiting to be copied becomes its own step first
    // (for redo, that also means there is nothing left to redo).
    // Only real edits count: some windows (e.g. the DMX Monitor) quietly
    // add details to the project without marking it as changed
    if (m_settleTimer.isActive())
        flush();

    if (from.isEmpty())
    {
        slotUpdateActions();
        return;
    }

    QByteArray target = from.takeLast();
    QByteArray xml = qUncompress(target);
    if (xml.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "Damaged undo copy, ignoring it";
        slotUpdateActions();
        return;
    }

    to.append(m_current);
    m_current = target;
    m_currentHash = hashOf(xml);

    restore(xml);

    // what is loaded now, including anything windows quietly added while
    // rebuilding, is the new reference point
    QByteArray now = takeSnapshot();
    m_current = qCompress(now);
    if (m_savedHash == m_currentHash)
        m_savedHash = hashOf(now);
    m_currentHash = hashOf(now);

    slotUpdateActions();
}

void UndoManager::restore(const QByteArray &xml)
{
    m_restoring = true;
    m_settleTimer.stop();

    /* Remember what is on screen */
    QVariantMap showState;
    quint32 selectedFunction = Function::invalidId();
    if (ShowManager::instance() != NULL)
        showState = ShowManager::instance()->viewState();
    if (FunctionManager::instance() != NULL)
        selectedFunction = FunctionManager::instance()->currentFunctionId();

    /* Clear the project, the same way opening a file does, but leave the
     * I/O patching, the universes and Simple Desk alone */
    m_doc->masterTimer()->stop();
    if (VirtualConsole::instance() != NULL)
        VirtualConsole::instance()->resetContents();
    if (ShowManager::instance() != NULL)
        ShowManager::instance()->clearContents();
    m_doc->clearContents();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();

    /* Load the copy, the same way opening a file does. It has to come
     * through a device: some loaders refuse a reader without one */
    QBuffer buffer;
    buffer.setData(xml);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    if (reader.readNextStartElement() && reader.name() == KXMLQLCWorkspace)
    {
        while (reader.readNextStartElement())
        {
            if (reader.name() == KXMLQLCEngine)
                m_doc->loadXML(reader, false);
            else if (reader.name() == KXMLQLCVirtualConsole && VirtualConsole::instance() != NULL)
                VirtualConsole::instance()->loadXML(reader);
            else
                reader.skipCurrentElement();
        }
    }
    if (reader.hasError())
        qWarning() << Q_FUNC_INFO << "Undo copy could not be read:" << reader.errorString();

    if (VirtualConsole::instance() != NULL)
        VirtualConsole::instance()->postLoad();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
    m_doc->masterTimer()->start();

    /* Back to what was on screen */
    if (ShowManager::instance() != NULL)
        ShowManager::instance()->restoreViewState(showState);
    if (FunctionManager::instance() != NULL && selectedFunction != Function::invalidId())
        FunctionManager::instance()->selectFunction(selectedFunction);

    /* Unsaved, unless this is exactly the saved project */
    if (m_currentHash == m_savedHash)
        m_doc->resetModified();
    else
        m_doc->setModified();

    m_restoring = false;
}
