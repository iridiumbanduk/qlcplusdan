/*
  Q Light Controller Plus
  undomanager.h

  QLC+DAN: project-wide Undo / Redo.

  How it works: a short while after every change, a copy of the whole
  project (fixtures, functions, shows, Virtual Console) is kept in memory.
  Undo puts the previous copy back, using the same code QLC+ uses to open
  a project file. Because everything is rebuilt from scratch, nothing is
  left pointing at objects that no longer exist.

  The Input/Output patching and Simple Desk are left alone, so undo never
  disturbs the DMX interfaces or live desk levels.

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

#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <QList>

class QAction;
class Doc;

class UndoManager final : public QObject
{
    Q_OBJECT

public:
    UndoManager(Doc *doc, QObject *parent);
    ~UndoManager();

    QAction *undoAction() const { return m_undoAction; }
    QAction *redoAction() const { return m_redoAction; }

    int undoCount() const { return m_undoStack.count(); }
    int redoCount() const { return m_redoStack.count(); }

    /** Why undo/redo can't run right now (empty when it can) */
    QString blockedReason() const;

    /** Forget all history and take the current project as the starting point */
    void resetHistory();

    /** Take a copy now if a change is waiting for one */
    void flush();

    /** How many steps are kept */
    static const int MaxSteps = 50;
    /** How long to wait after a change before taking a copy */
    static const int SettleMs = 500;

public slots:
    void undo();
    void redo();

private slots:
    void slotDocModified(bool modified);
    void slotDocClearing();
    void slotUpdateActions();
    /** Changes have settled: take a copy */
    void slotSettled();
    /** Take the starting point of the history */
    void slotTakeBaseline();

private:
    /** The project as XML, without the I/O patching */
    QByteArray takeSnapshot() const;
    /** Replace the project with a copy */
    void restore(const QByteArray &xml);
    void step(QList<QByteArray> &from, QList<QByteArray> &to);

private:
    Doc *m_doc;
    QAction *m_undoAction;
    QAction *m_redoAction;
    QTimer m_settleTimer;
    QTimer m_actionsTimer;

    /** Compressed copies, newest last */
    QList<QByteArray> m_undoStack;
    QList<QByteArray> m_redoStack;

    /** The copy matching the project as it is now (compressed) and its hash */
    QByteArray m_current;
    QByteArray m_currentHash;
    /** Hash of the project as last saved or opened */
    QByteArray m_savedHash;

    /** True from a new project being opened until its starting point is taken */
    bool m_waitingBaseline;
    bool m_restoring;
};

#endif
