/*
  Q Light Controller Plus
  chaserstep.cpp

  Copyright (C) 2004 Heikki Junnila
                2015 Massimo Callegari

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "chaserstep.h"
#include "function.h"
#include "doc.h"

#define KXMLQLCSequenceSceneValues  QStringLiteral("Values")
#define KXMLQLCStepNote             QStringLiteral("Note")
#define KXMLQLCStepParkedValues     QStringLiteral("DanParkedValues")

ChaserStep::ChaserStep(quint32 aFid, uint aFadeIn, uint aHold, uint aFadeOut)
    : fid(aFid)
    , fadeIn(aFadeIn)
    , hold(aHold)
    , fadeOut(aFadeOut)
    , duration(fadeIn + hold)
    , note(QString())
{
}

ChaserStep::ChaserStep(const ChaserStep& cs)
    : fid(cs.fid)
    , fadeIn(cs.fadeIn)
    , hold(cs.hold)
    , fadeOut(cs.fadeOut)
    , duration(cs.duration)
    , values(cs.values)
    , parkedValues(cs.parkedValues)
    , note(cs.note)
{
}

ChaserStep &ChaserStep::operator=(const ChaserStep &step)
{
    if (this != &step)
    {
        fid = step.fid;
        fadeIn = step.fadeIn;
        hold = step.hold;
        fadeOut = step.fadeOut;
        duration = step.duration;
        values = step.values;
        parkedValues = step.parkedValues;
        note = step.note;
    }

    return *this;
}

bool ChaserStep::operator==(const ChaserStep& cs) const
{
    return (fid == cs.fid) ? true : false;
}

Function* ChaserStep::resolveFunction(const Doc* doc) const
{
    if (doc == NULL)
        return NULL;
    else
        return doc->function(fid);
}

int ChaserStep::setValue(SceneValue value, int index, bool *created)
{
    if (index == -1)
    {
        index = values.indexOf(value);
        if (index == -1)
        {
            values.append(value);
            std::sort(values.begin(), values.end());
            if (created != NULL)
                *created = true;
            return values.indexOf(value);
        }
    }

    /* do not allow creation past the values begin/end */
    if (index < 0 || index > values.count())
    {
        if (created != NULL)
            *created = false;
        qWarning() << "[ChaserStep] index not allowed:" << index;
        return -1;
    }

    /* but do allow appending a new value */
    if (index == values.count())
    {
        values.append(value);
        if (created != NULL)
            *created = true;
    }
    else if (values.at(index) == value)
    {
        values.replace(index, value);
        if (created != NULL)
            *created = false;
    }
    else
    {
        values.insert(index, value);
        if (created != NULL)
            *created = true;
    }

    return index;
}

int ChaserStep::unSetValue(SceneValue value, int index)
{
    if (index == -1)
        index = values.indexOf(value);

    if (index < 0 || index >= values.count())
        return -1;

    values.removeAt(index);

    return index;
}

void ChaserStep::parkValue(const SceneValue &value)
{
    takeParkedValue(value.fxi, value.channel);
    // a value of 0 is what the channel comes back as anyway
    if (value.value != 0)
    {
        parkedValues.append(value);
        std::sort(parkedValues.begin(), parkedValues.end());
    }
}

int ChaserStep::takeParkedValue(quint32 fxi, quint32 channel)
{
    for (int i = 0; i < parkedValues.count(); i++)
    {
        if (parkedValues.at(i).fxi == fxi && parkedValues.at(i).channel == channel)
            return parkedValues.takeAt(i).value;
    }
    return -1;
}

/* Values as "fixtureID:channel,value,channel,value:fixtureID:..." (non-zero only) */
static QString valuesToString(const QList <SceneValue> &list)
{
    QString str;
    quint32 fixtureID = Fixture::invalidId();
    foreach (SceneValue scv, list)
    {
        if (scv.value == 0)
            continue;
        if (scv.fxi != fixtureID)
        {
            if (str.isEmpty() == false)
                str.append(QString(":"));
            str.append(QString("%1:").arg(scv.fxi));
            fixtureID = scv.fxi;
        }
        else
            str.append(QString(","));
        str.append(QString("%1,%2").arg(scv.channel).arg(scv.value));
    }
    return str;
}

static QList <SceneValue> valuesFromString(const QString &str)
{
    QList <SceneValue> list;
    QStringList fxArray = str.split(":");
    for (int f = 0; f + 1 < fxArray.count(); f += 2)
    {
        quint32 fxID = fxArray.at(f).toUInt();
        QStringList varray = fxArray.at(f + 1).split(",");
        for (int i = 0; i + 1 < varray.count(); i += 2)
            list.append(SceneValue(fxID, varray.at(i).toUInt(), uchar(varray.at(i + 1).toInt())));
    }
    return list;
}

QVariant ChaserStep::toVariant() const
{
    QList <QVariant> list;
    list << fid;
    list << fadeIn;
    list << hold;
    list << fadeOut;
    list << duration;
    list << note;
    return list;
}

ChaserStep ChaserStep::fromVariant(const QVariant& var)
{
    ChaserStep cs;
    QList <QVariant> list(var.toList());
    if (list.size() == 6)
    {
        cs.fid = list.takeFirst().toUInt();
        cs.fadeIn = list.takeFirst().toUInt();
        cs.hold = list.takeFirst().toUInt();
        cs.fadeOut = list.takeFirst().toUInt();
        cs.duration = list.takeFirst().toUInt();
        cs.note = list.takeFirst().toString();
    }
    return cs;
}

bool ChaserStep::loadXML(QXmlStreamReader &root, int& stepNumber, Doc *doc)
{
    bool holdFound = false;
    if (root.name() != KXMLQLCFunctionStep)
    {
        qWarning() << Q_FUNC_INFO << "ChaserStep node not found";
        return false;
    }
    QXmlStreamAttributes attrs = root.attributes();

    if (attrs.hasAttribute(KXMLQLCFunctionSpeedFadeIn) == true)
        fadeIn = attrs.value(KXMLQLCFunctionSpeedFadeIn).toString().toUInt();
    if (attrs.hasAttribute(KXMLQLCFunctionSpeedHold) == true)
    {
        hold = attrs.value(KXMLQLCFunctionSpeedHold).toString().toUInt();
        holdFound = true;
    }
    if (attrs.hasAttribute(KXMLQLCFunctionSpeedFadeOut) == true)
        fadeOut = attrs.value(KXMLQLCFunctionSpeedFadeOut).toString().toUInt();
    if (attrs.hasAttribute(KXMLQLCFunctionSpeedDuration) == true)
        duration = attrs.value(KXMLQLCFunctionSpeedDuration).toString().toUInt();
    if (attrs.hasAttribute(KXMLQLCFunctionNumber) == true)
        stepNumber = attrs.value(KXMLQLCFunctionNumber).toString().toInt();
    if (attrs.hasAttribute(KXMLQLCStepNote) == true)
        note = attrs.value(KXMLQLCStepNote).toString();
    if (attrs.hasAttribute(KXMLQLCStepParkedValues) == true)
    {
        parkedValues = valuesFromString(attrs.value(KXMLQLCStepParkedValues).toString());
        std::sort(parkedValues.begin(), parkedValues.end());
    }

    if (attrs.hasAttribute(KXMLQLCSequenceSceneValues) == true)
    {
        QString stepValues = root.readElementText();
        if (stepValues.isEmpty() == false)
        {
            int sIdx = 0;

            // step values are saved as a string with the following syntax:
            // fixtureID:channel,value,channel,value:fixtureID:channel,value ... etc

            // split the string by Fixture chunks
            QStringList fxArray = stepValues.split(":");

            for (int f = 0; f < fxArray.count(); f+=2)
            {
                if (f + 1 >= fxArray.count())
                    break;

                quint32 fxID = QString(fxArray.at(f)).toUInt();
                if (doc != NULL && doc->fixture(fxID) == NULL)
                    continue;

                // now split the chunk into channel/values
                QStringList varray = fxArray.at(f + 1).split(",");
                for (int i = 0; i < varray.count(); i+=2)
                {
                    quint32 chIndex = QString(varray.at(i)).toUInt();
                    SceneValue scv = SceneValue(fxID, chIndex, uchar(QString(varray.at(i + 1)).toInt()));

                    while (sIdx < values.count())
                    {
                        if (values.at(sIdx).fxi == scv.fxi && values.at(sIdx).channel == scv.channel)
                            break;
                        sIdx++;
                    }

                    if (sIdx < values.count())
                        values.replace(sIdx, scv);
                    else
                        values.append(scv);
                }
            }
            //qSort(values.begin(), values.end());
        }
    }
    else
    {
        QString text = root.readElementText();
        if (text.isEmpty() == false)
            fid = text.toUInt();
    }

    if (holdFound == true)
    {
        if ((int)hold < 0)
            duration = hold;
        else
            duration = fadeIn + hold;
    }
    else
    {
        if ((int)duration < 0)
            hold = duration;
        else
            hold = duration - fadeIn;
    }

    return true;
}

bool ChaserStep::saveXML(QXmlStreamWriter *doc, int stepNumber, bool isSequence) const
{
    /* Step tag */
    doc->writeStartElement(KXMLQLCFunctionStep);

    /* Step number */
    doc->writeAttribute(KXMLQLCFunctionNumber, QString::number(stepNumber));

    /* Speeds */
    doc->writeAttribute(KXMLQLCFunctionSpeedFadeIn, QString::number(fadeIn));
    doc->writeAttribute(KXMLQLCFunctionSpeedHold, QString::number(hold));
    doc->writeAttribute(KXMLQLCFunctionSpeedFadeOut, QString::number(fadeOut));
    if (note.isEmpty() == false)
        doc->writeAttribute(KXMLQLCStepNote, note);
    /* QLC+DAN: values of switched-off channels. Official QLC+ ignores this */
    if (isSequence && parkedValues.isEmpty() == false)
    {
        QString parked = valuesToString(parkedValues);
        if (parked.isEmpty() == false)
            doc->writeAttribute(KXMLQLCStepParkedValues, parked);
    }

    if (isSequence)
    {
        /* it's a sequence step. Save values accordingly */
        doc->writeAttribute(KXMLQLCSequenceSceneValues, QString::number(values.count()));
        QString stepValues;
        quint32 fixtureID = Fixture::invalidId();
        foreach (SceneValue scv, values)
        {
            // step values are saved as a string with the following syntax:
            // fixtureID:channel,value,channel,value:fixtureID:channel,value ... etc

            // save non-zero values only
            if (scv.value != 0)
            {
                if (scv.fxi != fixtureID)
                {
                    if (stepValues.isEmpty() == false)
                        stepValues.append(QString(":"));
                    stepValues.append(QString("%1:").arg(scv.fxi));
                    fixtureID = scv.fxi;
                }
                else
                    stepValues.append(QString(","));

                stepValues.append(QString("%1,%2").arg(scv.channel).arg(scv.value));
            }
        }
        if (stepValues.isEmpty() == false)
            doc->writeCharacters(stepValues);
    }
    else
    {
        /* Step function ID */
        doc->writeCharacters(QString::number(fid));
    }

    /* End the <Step> tag */
    doc->writeEndElement();

    return true;
}
