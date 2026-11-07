/*
  Q Light Controller Plus
  fixturepreview.h

  QLC+DAN: a floating window that draws what supported fixtures are doing,
  including their built-in effects, from the DMX values being sent to them.
  The look of each fixture comes from a "preview pack" (JSON) made by filming
  the real fixture.

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

#ifndef FIXTUREPREVIEW_H
#define FIXTUREPREVIEW_H

#include <QElapsedTimer>
#include <QByteArray>
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QHash>
#include <QList>
#include <QMap>

class QTimer;
class Fixture;
class Doc;

/** One recorded effect: a sequence of frames, looping */
struct PreviewClip
{
    bool isRgb;          // true: 32 RGB sections x 3 bytes; false: 16 rows x (amber, white)
    int fps;             // frames per second of the recording
    int recSpeed;        // value of the speed channel during the recording
    int frames;
    QByteArray data;
};

/** Everything needed to draw one fixture model in one mode */
class PreviewPack
{
public:
    bool load(const QString &path);
    bool matches(const Fixture *fixture) const;

    struct Range { int min; int max; QString clip; };
    struct Background { int min; int max; QColor color; };

    QString m_name;
    QString m_manufacturer;
    QString m_modelContains;
    int m_channelCount = 0;
    QMap<QString, int> m_ch;
    QList<Range> m_rgbEffects;
    QList<Range> m_awEffects;
    QList<Background> m_background;
    QVector<QPointF> m_rgbSpeed, m_awSpeed, m_strobe;
    int m_flashMs = 30;
    QHash<QString, PreviewClip> m_clips;

    /** Interpolate a curve (log-space for rates) */
    static double curve(const QVector<QPointF> &c, double x, bool logInterp);
    const PreviewClip *clipFor(const QList<Range> &ranges, int value, QString *key) const;
    QColor background(int value) const;
};

/** What one fixture shows right now */
struct PreviewState
{
    QColor rgb[32];
    double amber[16];
    double white[16];
    // effect playback
    QString rgbClip, awClip;
    double rgbFrame = 0, awFrame = 0;
    double strobePhase = 0, strobeAWPhase = 0;
};

class FixturePreview : public QWidget
{
    Q_OBJECT

public:
    static FixturePreview *instance(QWidget *parent, Doc *doc);
    ~FixturePreview();

protected:
    FixturePreview(QWidget *parent, Doc *doc);
    void paintEvent(QPaintEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void showEvent(QShowEvent *e) override;

signals:
    void visibilityChanged(bool visible);

private slots:
    void slotTick();

private:
    void loadPacks();
    void simulate(const PreviewPack *pack, const QByteArray &values, PreviewState &st, double dt);
    void drawSTB448(QPainter &p, const QRectF &r, const PreviewState &st);

private:
    static FixturePreview *s_instance;
    Doc *m_doc;
    QTimer *m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastMs;
    QList<PreviewPack *> m_packs;
    /** fixture ID -> state */
    QHash<quint32, PreviewState> m_states;
    /** fixtures drawn on the last tick: ID, pack */
    QList<QPair<quint32, const PreviewPack *>> m_shown;
};

#endif
