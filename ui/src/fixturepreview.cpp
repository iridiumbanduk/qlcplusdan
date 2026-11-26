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
#include <QContextMenuEvent>
#include <QActionGroup>
#include <QMenu>
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
#define SETTINGS_GAMMA "fixturepreview/gamma"
#define SETTINGS_AWBOOST "fixturepreview/awboost"
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
    m_layout = o.value("layout").toString("stb448");
    m_name = o.value("name").toString();
    m_manufacturer = o.value("manufacturer").toString();
    m_modelContains = o.value("modelContains").toString();
    m_genericNameContains = o.value("genericNameContains").toString();
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

    // Super Strobe
    m_programmes = readRanges(o.value("programmes").toArray());
    auto readBands = [](const QJsonArray &a) {
        QList<Band> list;
        for (const QJsonValue &v : a)
        {
            QJsonArray r = v.toArray();
            QJsonArray c = r.at(3).toArray();
            list.append({ r.at(0).toInt(), r.at(1).toInt(), r.at(2).toInt(),
                          QColor(c.at(0).toInt(), c.at(1).toInt(), c.at(2).toInt()) });
        }
        return list;
    };
    m_presetColours = readBands(o.value("presetColours").toArray());
    m_backgrounds = readBands(o.value("backgrounds").toArray());
    m_speedCurve = readCurve(o.value("speedCurve").toArray());
    m_whiteStrobe = readCurve(o.value("whiteStrobeCurve").toArray());
    m_whiteFlashMsPerStep = o.value("whiteFlashMsPerStep").toDouble(2.6);

    QJsonObject clips = o.value("clips").toObject();
    for (auto it = clips.begin(); it != clips.end(); ++it)
    {
        QJsonObject c = it.value().toObject();
        PreviewClip clip;
        clip.rows = c.value("rows").toInt();
        clip.cols = c.value("cols").toInt();
        clip.isRgb = (c.value("kind").toString() == "rgb") || clip.rows > 0;
        clip.fps = c.value("fps").toDouble();
        clip.recSpeed = c.value("recSpeed").toInt();
        clip.frames = c.value("frames").toInt();
        clip.data = qUncompress(QByteArray::fromBase64(c.value("data").toString().toLatin1()));
        int frameSize = clip.rows > 0 ? clip.rows * clip.cols * 3 : (clip.isRgb ? 32 * 3 : 16 * 2);
        // a Super Strobe clip must be the 4 x 24 grid the preview draws
        bool badGrid = clip.rows > 0 && (clip.rows != 4 || clip.cols != 24);
        if (badGrid || clip.fps <= 0 || clip.frames <= 0 || clip.data.size() < clip.frames * frameSize)
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
    // a fixture patched as Generic is recognised by its name
    if (m_genericNameContains.isEmpty() == false &&
        fixture->fixtureDef()->manufacturer() == KXMLFixtureGeneric &&
        fixture->name().contains(m_genericNameContains, Qt::CaseInsensitive))
        return true;
    if (fixture->fixtureDef()->manufacturer().compare(m_manufacturer, Qt::CaseInsensitive) != 0)
        return false;
    return fixture->fixtureDef()->model().contains(m_modelContains, Qt::CaseInsensitive);
}

int PreviewPack::band(const QList<Band> &bands, int value, QColor *color)
{
    for (const Band &b : bands)
    {
        if (value >= b.min && value <= b.max)
        {
            if (color)
                *color = b.color;
            return b.index;
        }
    }
    return 0;
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
    , m_gamma(2.2)
    , m_awBoost(1.0)
{
    setWindowTitle(tr("Fixture preview"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(160, 240);

    QSettings settings;
    m_gamma = settings.value(SETTINGS_GAMMA, 2.2).toDouble();
    m_awBoost = settings.value(SETTINGS_AWBOOST, 1.0).toDouble();
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
        if (pack->m_layout == "superstrobe")
            simulateSuperStrobe(pack, values, st, dt);
        else
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
        // filmed: when any colour fader is up, the effect's pattern is drawn in the
        // fader colour (keeping the effect's own brightness steps) instead of its own colours
        int fr = val("red"), fg = val("green"), fb = val("blue");
        bool tinted = (fr + fg + fb) > 0;
        for (int s = 0; s < 32; s++)
        {
            int r = f[s * 3], g = f[s * 3 + 1], b = f[s * 3 + 2];
            // the background colour fills every section the effect leaves dark
            if (r + g + b < 30)
                st.rgb[s] = bg;
            else if (tinted)
            {
                double level = qMax(r, qMax(g, b)) / 255.0;
                st.rgb[s] = QColor(int(fr * level), int(fg * level), int(fb * level));
            }
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
        // filmed: with the amber and/or white fader up, the whole pattern (amber and white
        // parts together) runs only in those colours, at the faders' levels
        int fa = val("amber"), fw = val("white");
        for (int r = 0; r < 16; r++)
        {
            double a = f[r * 2] / 255.0, w = f[r * 2 + 1] / 255.0;
            if (fa + fw > 0)
            {
                double level = qMax(a, w);
                st.amber[r] = level * fa / 255.0;
                st.white[r] = level * fw / 255.0;
            }
            else
            {
                st.amber[r] = a;
                st.white[r] = w;
            }
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

/* Eurolite LED Super Strobe ABL, 22 channels. Everything here was filmed:
   - the built-in programmes, each with every colour preset and background
     combination the show uses (other combinations: the programme drawn in the
     preset's colour)
   - the 4 colour segments are rows, bottom to top
   - colour presets do nothing without a programme, and don't change colours set directly
   - speed: time per step is proportional to (262 - speed) */
void FixturePreview::simulateSuperStrobe(const PreviewPack *pack, const QByteArray &v, PreviewState &st, double dt)
{
    const int rows = 4, cols = 24;
    auto val = [&](const char *name) { return int(uchar(v.at(pack->m_ch.value(name)))); };

    QString key;
    const PreviewClip *clip = pack->clipFor(pack->m_programmes, val("programme"), &key);
    if (clip)
    {
        QColor presetColour;
        int preset = PreviewPack::band(pack->m_presetColours, val("preset"), &presetColour);
        int bg = PreviewPack::band(pack->m_backgrounds, val("background"));
        QColor tint;    // invalid: the clip's own colours
        if (preset > 0 || bg > 0)
        {
            QString combo = QString("%1_c%2_b%3").arg(key).arg(preset).arg(bg);
            auto it = pack->m_clips.constFind(combo);
            if (it != pack->m_clips.constEnd())
            {
                clip = &it.value();
                key = combo;
            }
            else if (preset > 0)
            {
                // not filmed: the programme's pattern in the preset's colour
                tint = presetColour;
                key = QString("%1_tint%2").arg(key).arg(preset);
            }
        }
        if (key != st.rgbClip)
        {
            st.rgbClip = key;
            st.rgbFrame = 0;   // a new programme starts from its beginning
        }
        double rate = PreviewPack::curve(pack->m_speedCurve, val("speed"), true) /
                      PreviewPack::curve(pack->m_speedCurve, clip->recSpeed, true);
        st.rgbFrame = fmod(st.rgbFrame + dt * clip->fps * rate, clip->frames);
        int frame = qBound(0, int(st.rgbFrame), clip->frames - 1);
        const uchar *f = reinterpret_cast<const uchar *>(clip->data.constData()) + frame * rows * cols * 3;
        bool flip = val("direction") >= 128;   // filmed: reverses the effect top to bottom
        for (int r = 0; r < rows; r++)
        {
            const uchar *row = f + (flip ? rows - 1 - r : r) * cols * 3;
            for (int c = 0; c < cols; c++)
            {
                int R = row[c * 3], G = row[c * 3 + 1], B = row[c * 3 + 2];
                if (tint.isValid())
                {
                    double level = qMax(R, qMax(G, B)) / 255.0;
                    st.cells[r * cols + c] = QColor(int(tint.red() * level), int(tint.green() * level), int(tint.blue() * level));
                }
                else
                    st.cells[r * cols + c] = QColor(R, G, B);
            }
        }
    }
    else
    {
        // no programme: the segment colours. Segment 1 is the bottom row
        st.rgbClip.clear();
        const char *segs[4] = { "seg4", "seg3", "seg2", "seg1" };
        for (int r = 0; r < rows; r++)
        {
            int ch = pack->m_ch.value(segs[r]);
            QColor c(uchar(v.at(ch)), uchar(v.at(ch + 1)), uchar(v.at(ch + 2)));
            for (int col = 0; col < cols; col++)
                st.cells[r * cols + col] = c;
        }
    }

    /* ---- colour strobe (0-8 = no strobe) and dimmer */
    auto flashOn = [&](int value, double hz, double flashMs, double &phase) {
        if (value < 9)
        {
            phase = 0;
            return true;
        }
        phase = fmod(phase + dt * hz, 1.0);
        double on = flashMs / 1000.0 * hz;
        if (on >= 1.0)
            return true;              // flashes longer than the gap between them: steady
        // short flashes are shown for at least one screen refresh
        on = qMin(0.5, qMax(on, TICK_MS / 1000.0 * hz));
        return phase < on;
    };
    int strobe = val("strobe");
    bool rgbOn = flashOn(strobe, PreviewPack::curve(pack->m_strobe, strobe, true), pack->m_flashMs, st.strobePhase);
    double master = rgbOn ? val("master") / 255.0 : 0.0;
    for (int i = 0; i < rows * cols; i++)
    {
        QColor c = st.cells[i];
        st.cells[i] = QColor(int(c.red() * master), int(c.green() * master), int(c.blue() * master));
    }

    /* ---- white strip: dimmer, strobe and flash duration (255 = stays on) */
    int wStrobe = val("whiteStrobe");
    int flash = val("whiteFlash");
    double flashMs = flash >= 255 ? 1e6 : flash * pack->m_whiteFlashMsPerStep;
    bool whiteOn = flashOn(wStrobe, PreviewPack::curve(pack->m_whiteStrobe, wStrobe, true), flashMs, st.strobeAWPhase);
    st.whiteStrip = whiteOn ? val("whiteDimmer") / 255.0 : 0.0;
}

QColor FixturePreview::ledColor(const QColor &c) const
{
    // unlit LEDs are shown as dark grey
    if (c.red() + c.green() + c.blue() < 3)
        return QColor(28, 28, 32);
    // LEDs put out light in proportion to the DMX value, but both eyes and screens are
    // far from linear: a dim LED looks much brighter in a room than the same value drawn
    // on screen. The curve maps light output to a screen value that looks similar.
    auto curve = [this](int x) { return x <= 0 ? 0 : qBound(0, int(qRound(255.0 * qPow(x / 255.0, 1.0 / m_gamma))), 255); };
    return QColor(curve(c.red()), curve(c.green()), curve(c.blue()));
}

void FixturePreview::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    menu.addSection(tr("Brightness of dim LEDs"));
    QActionGroup group(&menu);
    const QList<QPair<QString, double>> curves = {
        { tr("As DMX values (linear)"), 1.0 },
        { tr("Natural (like the real LEDs)"), 2.2 },
        { tr("Brighter"), 3.0 },
        { tr("Much brighter"), 4.0 } };
    for (const auto &c : curves)
    {
        QAction *a = menu.addAction(c.first);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(m_gamma, c.second));
        a->setData(QVariantList() << 0 << c.second);
        group.addAction(a);
    }

    // white and amber LEDs are much more powerful than the RGB ones on some fixtures
    menu.addSection(tr("White and amber LEDs compared to RGB"));
    QActionGroup boostGroup(&menu);
    const QList<QPair<QString, double>> boosts = {
        { tr("Same as RGB"), 1.0 },
        { tr("1.5x brighter"), 1.5 },
        { tr("2x brighter"), 2.0 },
        { tr("3x brighter"), 3.0 } };
    for (const auto &b : boosts)
    {
        QAction *a = menu.addAction(b.first);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(m_awBoost, b.second));
        a->setData(QVariantList() << 1 << b.second);
        boostGroup.addAction(a);
    }

    QAction *chosen = menu.exec(e->globalPos());
    if (chosen != NULL)
    {
        QVariantList d = chosen->data().toList();
        QSettings settings;
        if (d.at(0).toInt() == 0)
        {
            m_gamma = d.at(1).toDouble();
            settings.setValue(SETTINGS_GAMMA, m_gamma);
        }
        else
        {
            m_awBoost = d.at(1).toDouble();
            settings.setValue(SETTINGS_AWBOOST, m_awBoost);
        }
        update();
    }
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
            else if (col == 1)
            {
                double a = qMin(1.0, st.amber[section] * m_awBoost);
                c = QColor::fromRgbF(a, a * 0.6, 0);
            }
            else
            {
                double w = qMin(1.0, st.white[section] * m_awBoost);
                c = QColor::fromRgbF(w, w, w);
            }
            led.setLeft(inner.left() + colX[col] * inner.width());
            led.setWidth(colW[col] * inner.width());
            p.setBrush(ledColor(c));
            p.drawRect(led);
        }
    }
}

void FixturePreview::drawSuperStrobe(QPainter &p, const QRectF &r, const PreviewState &st)
{
    // body
    p.setPen(QPen(QColor(70, 70, 76), 1.5));
    p.setBrush(QColor(12, 12, 14));
    p.drawRoundedRect(r, r.height() * 0.06, r.height() * 0.06);

    // filmed proportions of the face: colour panel 41%, white strip 18%, colour panel 41%
    QRectF face = r.adjusted(r.height() * 0.07, r.height() * 0.07, -r.height() * 0.07, -r.height() * 0.07);
    double panelH = face.height() * 0.41, stripH = face.height() * 0.18;
    p.setPen(Qt::NoPen);

    // 4 rows of 36 LEDs (12 groups of 3); the filmed 24 cells are interpolated across them
    const int leds = 36;
    double ledW = face.width() / leds;
    for (int row = 0; row < 4; row++)
    {
        double y = face.top() + (row < 2 ? row * panelH / 2 : panelH + stripH + (row - 2) * panelH / 2);
        for (int i = 0; i < leds; i++)
        {
            double x = (i + 0.5) / leds * 24 - 0.5;
            int c0 = qBound(0, int(qFloor(x)), 23), c1 = qMin(23, c0 + 1);
            double f = qBound(0.0, x - c0, 1.0);
            const QColor &a = st.cells[row * 24 + c0], &b = st.cells[row * 24 + c1];
            QColor c(int(a.red() + (b.red() - a.red()) * f), int(a.green() + (b.green() - a.green()) * f),
                     int(a.blue() + (b.blue() - a.blue()) * f));
            p.setBrush(ledColor(c));
            p.drawRect(QRectF(face.left() + i * ledW + ledW * 0.06, y + panelH * 0.03, ledW * 0.88, panelH / 2 - panelH * 0.06));
        }
    }

    // white strip: 3 rows of 72 cold white LEDs
    double w = qMin(1.0, st.whiteStrip * m_awBoost);
    QColor white = ledColor(QColor::fromRgbF(w, w, w * 1.0));
    if (w > 0)
        white = QColor(qMin(255, int(white.red() * 0.94)), qMin(255, int(white.green() * 0.97)), white.blue());   // cold white
    p.setBrush(white);
    double sy = face.top() + panelH, dot = qMin(face.width() / 72, stripH / 3) * 0.7;
    for (int row = 0; row < 3; row++)
        for (int i = 0; i < 72; i++)
            p.drawEllipse(QPointF(face.left() + (i + 0.5) * face.width() / 72, sy + (row + 0.5) * stripH / 3), dot / 2, dot / 2);
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
                   tr("No previewable fixtures in this project.\n\nSupported: beamZ STB448 in 13-channel mode, "
                      "Eurolite LED Super Strobe ABL in 22-channel mode."));
        return;
    }

    // as on stage: the tall bars (STB448s, on stands) to the left and right, and the wide
    // fixtures (the Super Strobe, on the floor in front of the drums) between them at the bottom
    QList<quint32> bars, wides;
    for (const auto &s : m_shown)
        (s.second->m_layout == "superstrobe" ? wides : bars).append(s.first);

    const double labelH = 22, margin = 12;
    const double fullH = height() - 2 * margin - labelH;      // everything stands on one line
    auto label = [&](quint32 id, double cx, double slotW) {
        Fixture *fxi = m_doc->fixture(id);
        p.setPen(QColor(220, 220, 220));
        p.drawText(QRectF(cx - slotW / 2, margin + fullH + 4, slotW, labelH),
                   Qt::AlignHCenter | Qt::AlignTop, fxi ? fxi->name() : QString());
    };
    if (fullH < 20)
        return;

    // horizontal space: the wide fixtures get the middle half when there are bars as well
    double gapW = bars.isEmpty() ? width() : (wides.isEmpty() ? 0 : width() * 0.5);
    double gapLeft = (width() - gapW) / 2;

    if (bars.isEmpty() == false)
    {
        int n = bars.count();
        int leftCount = wides.isEmpty() ? n : (n + 1) / 2;    // first half on the left
        double sideW = wides.isEmpty() ? width() - margin : gapLeft;
        double barW = fullH / 8.0;                               // the bar is about 10:1, keep it slim
        for (int i = 0; i < n; i++)
        {
            bool left = i < leftCount;
            int count = left ? leftCount : n - leftCount;
            int idx = left ? i : i - leftCount;
            double slotW = sideW / count;
            double x0 = left ? (wides.isEmpty() ? margin / 2 : 0) : width() - sideW;
            double cx = x0 + slotW * (idx + 0.5);
            double w = qMin(slotW - margin, barW);
            drawSTB448(p, QRectF(cx - w / 2, margin, w, fullH), m_states[bars.at(i)]);
            label(bars.at(i), cx, slotW);
        }
    }

    if (wides.isEmpty() == false)
    {
        double slotW = (gapW - margin) / wides.count();
        // the Super Strobe's face is about 1.9:1; next to bars it stays low, like on the floor
        double h = qMin((slotW - margin) / 1.9, bars.isEmpty() ? fullH : fullH * 0.4);
        double w = h * 1.9;
        for (int i = 0; i < wides.count(); i++)
        {
            double cx = gapLeft + margin / 2 + slotW * (i + 0.5);
            drawSuperStrobe(p, QRectF(cx - w / 2, margin + fullH - h, w, h), m_states[wides.at(i)]);
            label(wides.at(i), cx, slotW);
        }
    }
}
