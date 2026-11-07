/*
  Q Light Controller Plus
  fixturepreview.cpp

  QLC+DAN: floating fixture preview window. See fixturepreview.h

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

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCloseEvent>
#include <QSettings>
#include <QPainter>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QDebug>

#include "fixturepreview.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

#define SETTINGS_GEOMETRY "fixturepreview/geometry"
#define TICK_MS 33

/*****************************************************************************
 * PreviewPack
 *****************************************************************************/

static QVector<QPointF> readCurve(const QJsonArray &a)
{
    QVector<QPointF> c;
    for (const QJsonValue &v : a)
        c.append(QPointF(v.toArray().at(0).toDouble(), v.toArray().at(1).toDouble()));
    return c;
}

bool PreviewPack::load(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (doc.isNull())
    {
        qWarning() << "Fixture preview pack" << path << "is not valid:" << err.errorString();
        return false;
    }

    QJsonObject o = doc.object();
    m_name = o.value("name").toString();
    m_manufacturer = o.value("manufacturer").toString();
    m_modelContains = o.value("modelContains").toString();
    m_channelCount = o.value("channelCount").toInt();

    QJsonObject ch = o.value("channels").toObject();
    for (auto it = ch.begin(); it != ch.end(); ++it)
        m_ch[it.key()] = it.value().toInt();

    auto readRanges = [](const QJsonArray &a) {
        QList<Range> list;
        for (const QJsonValue &v : a)
        {
            QJsonArray r = v.toArray();
            list.append({ r.at(0).toInt(), r.at(1).toInt(), r.at(2).toString() });
        }
        return list;
    };
    m_rgbEffects = readRanges(o.value("rgbEffects").toArray());
    m_awEffects = readRanges(o.value("awEffects").toArray());

    for (const QJsonValue &v : o.value("background").toArray())
    {
        QJsonArray r = v.toArray();
        QJsonArray c = r.at(2).toArray();
        m_background.append({ r.at(0).toInt(), r.at(1).toInt(),
                              QColor(c.at(0).toInt(), c.at(1).toInt(), c.at(2).toInt()) });
    }

    m_rgbSpeed = readCurve(o.value("rgbSpeedCurve").toArray());
    m_awSpeed = readCurve(o.value("awSpeedCurve").toArray());
    m_strobe = readCurve(o.value("strobeCurve").toArray());
    m_flashMs = o.value("strobeFlashMs").toInt(30);

    QJsonObject clips = o.value("clips").toObject();
    for (auto it = clips.begin(); it != clips.end(); ++it)
    {
        QJsonObject c = it.value().toObject();
        PreviewClip clip;
        clip.isRgb = (c.value("kind").toString() == "rgb");
        clip.fps = c.value("fps").toInt();
        clip.recSpeed = c.value("recSpeed").toInt();
        clip.frames = c.value("frames").toInt();
        clip.data = qUncompress(QByteArray::fromBase64(c.value("data").toString().toLatin1()));
        int frameSize = clip.isRgb ? 32 * 3 : 16 * 2;
        if (clip.frames <= 0 || clip.data.size() < clip.frames * frameSize)
        {
            qWarning() << "Fixture preview clip" << it.key() << "is incomplete";
            continue;
        }
        m_clips.insert(it.key(), clip);
    }

    qDebug() << "Loaded fixture preview pack" << m_name << "with" << m_clips.count() << "effects";
    return m_channelCount > 0;
}

bool PreviewPack::matches(const Fixture *fixture) const
{
    if (fixture == NULL || fixture->fixtureDef() == NULL)
        return false;
    if (fixture->channels() != quint32(m_channelCount))
        return false;
    if (fixture->fixtureDef()->manufacturer().compare(m_manufacturer, Qt::CaseInsensitive) != 0)
        return false;
    return fixture->fixtureDef()->model().contains(m_modelContains, Qt::CaseInsensitive);
}

double PreviewPack::curve(const QVector<QPointF> &c, double x, bool logInterp)
{
    if (c.isEmpty())
        return 1.0;
    if (x <= c.first().x())
        return c.first().y();
    for (int i = 1; i < c.count(); i++)
    {
        if (x <= c.at(i).x())
        {
            double f = (x - c.at(i - 1).x()) / (c.at(i).x() - c.at(i - 1).x());
            if (logInterp)
                return qExp(qLn(c.at(i - 1).y()) + f * (qLn(c.at(i).y()) - qLn(c.at(i - 1).y())));
            return c.at(i - 1).y() + f * (c.at(i).y() - c.at(i - 1).y());
        }
    }
    return c.last().y();
}

const PreviewClip *PreviewPack::clipFor(const QList<Range> &ranges, int value, QString *key) const
{
    for (const Range &r : ranges)
    {
        if (value >= r.min && value <= r.max)
        {
            auto it = m_clips.constFind(r.clip);
            if (it == m_clips.constEnd())
                return NULL;
            if (key)
                *key = r.clip;
            return &it.value();
        }
    }
    return NULL;
}

QColor PreviewPack::background(int value) const
{
    for (const Background &b : m_background)
        if (value >= b.min && value <= b.max)
            return b.color;
    return Qt::black;
}

/*****************************************************************************
 * FixturePreview window
 *****************************************************************************/

FixturePreview *FixturePreview::s_instance = NULL;

FixturePreview *FixturePreview::instance(QWidget *parent, Doc *doc)
{
    if (s_instance == NULL)
        s_instance = new FixturePreview(parent, doc);
    return s_instance;
}

FixturePreview::FixturePreview(QWidget *parent, Doc *doc)
    : QWidget(parent, Qt::Tool)
    , m_doc(doc)
    , m_lastMs(0)
{
    setWindowTitle(tr("Fixture preview"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(160, 240);

    QSettings settings;
    QVariant geo = settings.value(SETTINGS_GEOMETRY);
    if (geo.isValid())
        restoreGeometry(geo.toByteArray());
    else
        resize(320, 640);

    loadPacks();

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTick()));
    m_clock.start();
}

FixturePreview::~FixturePreview()
{
    qDeleteAll(m_packs);
    s_instance = NULL;
}

void FixturePreview::loadPacks()
{
    QDir dir(":/fixturepreviews");
    for (const QString &f : dir.entryList(QStringList() << "*.json", QDir::Files))
    {
        PreviewPack *pack = new PreviewPack();
        if (pack->load(dir.absoluteFilePath(f)))
            m_packs.append(pack);
        else
            delete pack;
    }
}

void FixturePreview::showEvent(QShowEvent *e)
{
    m_lastMs = m_clock.elapsed();
    m_timer->start(TICK_MS);
    QWidget::showEvent(e);
    emit visibilityChanged(true);
}

void FixturePreview::hideEvent(QHideEvent *e)
{
    m_timer->stop();
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
    QWidget::hideEvent(e);
    emit visibilityChanged(false);
}

void FixturePreview::closeEvent(QCloseEvent *e)
{
    // just hide: the window keeps its state for the next time
    e->accept();
    hide();
}

void FixturePreview::slotTick()
{
    qint64 now = m_clock.elapsed();
    double dt = qBound(0.0, (now - m_lastMs) / 1000.0, 0.25);
    m_lastMs = now;

    m_shown.clear();
    if (m_doc == NULL)
        return;

    for (Fixture *fxi : m_doc->fixtures())
    {
        const PreviewPack *pack = NULL;
        for (const PreviewPack *p : m_packs)
            if (p->matches(fxi)) { pack = p; break; }
        if (pack == NULL)
            continue;

        Universe *uni = m_doc->inputOutputMap()->universe(fxi->universe());
        QByteArray values(pack->m_channelCount, 0);
        if (uni != NULL)
        {
            const QByteArray *out = uni->postGMValues();
            for (int i = 0; i < pack->m_channelCount; i++)
            {
                int addr = int(fxi->address()) + i;
                if (out != NULL && addr < out->size())
                    values[i] = out->at(addr);
            }
        }

        PreviewState &st = m_states[fxi->id()];
        simulate(pack, values, st, dt);
        m_shown.append(qMakePair(fxi->id(), pack));
    }
    update();
}

void FixturePreview::simulate(const PreviewPack *pack, const QByteArray &v, PreviewState &st, double dt)
{
    auto val = [&](const char *name) { return int(uchar(v.at(pack->m_ch.value(name)))); };
    double master = val("master") / 255.0;

    /* ---- RGB sections: built-in effect, or the colour faders */
    QString key;
    const PreviewClip *clip = pack->clipFor(pack->m_rgbEffects, val("rgbEffect"), &key);
    if (clip)
    {
        if (key != st.rgbClip)
        {
            st.rgbClip = key;
            st.rgbFrame = 0;   // a new effect starts from its beginning
        }
        double rate = PreviewPack::curve(pack->m_rgbSpeed, val("rgbSpeed"), true) /
                      PreviewPack::curve(pack->m_rgbSpeed, clip->recSpeed, true);
        st.rgbFrame = fmod(st.rgbFrame + dt * clip->fps * rate, clip->frames);
        const uchar *f = reinterpret_cast<const uchar *>(clip->data.constData()) + int(st.rgbFrame) * 32 * 3;
        QColor bg = pack->background(val("background"));
        for (int s = 0; s < 32; s++)
        {
            int r = f[s * 3], g = f[s * 3 + 1], b = f[s * 3 + 2];
            // the background colour fills every section the effect leaves dark
            if (r + g + b < 30)
                st.rgb[s] = bg;
            else
                st.rgb[s] = QColor(r, g, b);
        }
    }
    else
    {
        // no effect (or a sound-active programme, which can't be previewed): faders
        st.rgbClip.clear();
        QColor c(val("red"), val("green"), val("blue"));
        for (int s = 0; s < 32; s++)
            st.rgb[s] = c;
    }

    /* ---- amber / white sections */
    const PreviewClip *aw = pack->clipFor(pack->m_awEffects, val("awEffect"), &key);
    if (aw)
    {
        if (key != st.awClip)
        {
            st.awClip = key;
            st.awFrame = 0;
        }
        double rate = PreviewPack::curve(pack->m_awSpeed, val("awSpeed"), true) /
                      PreviewPack::curve(pack->m_awSpeed, aw->recSpeed, true);
        st.awFrame = fmod(st.awFrame + dt * aw->fps * rate, aw->frames);
        const uchar *f = reinterpret_cast<const uchar *>(aw->data.constData()) + int(st.awFrame) * 16 * 2;
        for (int r = 0; r < 16; r++)
        {
            st.amber[r] = f[r * 2] / 255.0;
            st.white[r] = f[r * 2 + 1] / 255.0;
        }
    }
    else
    {
        st.awClip.clear();
        for (int r = 0; r < 16; r++)
        {
            st.amber[r] = val("amber") / 255.0;
            st.white[r] = val("white") / 255.0;
        }
    }

    /* ---- strobes (values 0-9 = no strobe) */
    auto strobeOn = [&](int value, double &phase) {
        if (value < 10)
        {
            phase = 0;
            return true;
        }
        double hz = PreviewPack::curve(pack->m_strobe, value, false);
        phase = fmod(phase + dt * hz, 1.0);
        // flashes are ~30 ms; show them for at least one screen refresh
        double onFraction = qMin(0.5, qMax(pack->m_flashMs / 1000.0, TICK_MS / 1000.0) * hz);
        return phase < onFraction;
    };
    bool rgbOn = strobeOn(val("strobe"), st.strobePhase);
    bool awOn = strobeOn(val("strobeAW"), st.strobeAWPhase);

    /* ---- master dimmer */
    for (int s = 0; s < 32; s++)
    {
        QColor c = st.rgb[s];
        double m = rgbOn ? master : 0;
        st.rgb[s] = QColor(int(c.red() * m), int(c.green() * m), int(c.blue() * m));
    }
    for (int r = 0; r < 16; r++)
    {
        st.amber[r] *= awOn ? master : 0;
        st.white[r] *= awOn ? master : 0;
    }
}

static QColor ledColor(const QColor &c)
{
    // lift dim colours slightly so they stay visible, and show unlit LEDs as dark grey
    if (c.red() + c.green() + c.blue() < 6)
        return QColor(28, 28, 32);
    auto lift = [](int x) { return x == 0 ? 0 : int(40 + x * 215 / 255.0); };
    return QColor(lift(c.red()), lift(c.green()), lift(c.blue()));
}

void FixturePreview::drawSTB448(QPainter &p, const QRectF &r, const PreviewState &st)
{
    // body
    p.setPen(QPen(QColor(70, 70, 76), 1.5));
    p.setBrush(QColor(12, 12, 14));
    p.drawRoundedRect(r, r.width() * 0.12, r.width() * 0.12);

    QRectF inner = r.adjusted(r.width() * 0.08, r.width() * 0.25, -r.width() * 0.08, -r.width() * 0.25);
    const int rows = 16 * 7;
    double rowH = inner.height() / rows;
    // column x positions and widths (fractions of the inner width): RGB | amber | white | RGB
    const double colX[4] = { 0.00, 0.34, 0.54, 0.74 };
    const double colW[4] = { 0.26, 0.12, 0.12, 0.26 };
    p.setPen(Qt::NoPen);

    for (int row = 0; row < rows; row++)
    {
        int section = row / 7;                       // 0 = bottom
        double y = inner.bottom() - (row + 1) * rowH;
        QRectF led(0, y + rowH * 0.15, 0, rowH * 0.7);
        for (int col = 0; col < 4; col++)
        {
            QColor c;
            if (col == 0) c = st.rgb[section];
            else if (col == 3) c = st.rgb[16 + section];
            else if (col == 1) c = QColor::fromRgbF(qMin(1.0, st.amber[section]), qMin(1.0, st.amber[section] * 0.6), 0);
            else c = QColor::fromRgbF(qMin(1.0, st.white[section]), qMin(1.0, st.white[section]), qMin(1.0, st.white[section]));
            led.setLeft(inner.left() + colX[col] * inner.width());
            led.setWidth(colW[col] * inner.width());
            p.setBrush(ledColor(c));
            p.drawRect(led);
        }
    }
}

void FixturePreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30, 30, 34));

    if (m_shown.isEmpty())
    {
        p.setPen(QColor(200, 200, 200));
        p.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("No previewable fixtures in this project.\n\nSupported: beamZ STB448 in 13-channel mode."));
        return;
    }

    const double labelH = 22, margin = 12;
    int n = m_shown.count();
    double slotW = (width() - margin) / n;
    double barH = height() - 2 * margin - labelH;
    double barW = qMin(slotW - margin, barH / 8.0);      // the bar is about 10:1, keep it slim

    for (int i = 0; i < n; i++)
    {
        quint32 id = m_shown.at(i).first;
        double cx = margin / 2 + slotW * (i + 0.5);
        QRectF bar(cx - barW / 2, margin, barW, barH);
        drawSTB448(p, bar, m_states[id]);

        Fixture *fxi = m_doc->fixture(id);
        p.setPen(QColor(220, 220, 220));
        p.drawText(QRectF(cx - slotW / 2, height() - margin - labelH + 4, slotW, labelH),
                   Qt::AlignHCenter | Qt::AlignTop, fxi ? fxi->name() : QString());
    }
}
