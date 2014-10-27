/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://www.qt.io/licensing.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "qmlprofileranimationsmodel.h"
#include "qmlprofilermodelmanager.h"
#include "qmlprofilerdatamodel.h"
#include "abstracttimelinemodel_p.h"
#include <utils/qtcassert.h>
#include <QCoreApplication>

#include <QVector>
#include <QHash>
#include <QUrl>
#include <QString>
#include <QStack>

#include <QDebug>

namespace QmlProfiler {
namespace Internal {

class QmlProfilerAnimationsModel::QmlProfilerAnimationsModelPrivate : public AbstractTimelineModelPrivate
{
public:
    QVector<QmlProfilerAnimationsModel::QmlPaintEventData> data;
    int maxGuiThreadAnimations;
    int maxRenderThreadAnimations;
    int rowFromThreadId(QmlDebug::AnimationThread threadId) const;

private:
    Q_DECLARE_PUBLIC(QmlProfilerAnimationsModel)
};

QmlProfilerAnimationsModel::QmlProfilerAnimationsModel(QObject *parent)
    : AbstractTimelineModel(new QmlProfilerAnimationsModelPrivate,
                            tr(QmlProfilerModelManager::featureName(QmlDebug::ProfileAnimations)),
                            QmlDebug::Event, QmlDebug::MaximumRangeType, parent)
{
    Q_D(QmlProfilerAnimationsModel);
    d->maxGuiThreadAnimations = d->maxRenderThreadAnimations = 0;
}

quint64 QmlProfilerAnimationsModel::features() const
{
    return 1 << QmlDebug::ProfileAnimations;
}

void QmlProfilerAnimationsModel::clear()
{
    Q_D(QmlProfilerAnimationsModel);
    d->maxGuiThreadAnimations = d->maxRenderThreadAnimations = 0;
    d->data.clear();
    AbstractTimelineModel::clear();
}

bool QmlProfilerAnimationsModel::accepted(const QmlProfilerDataModel::QmlEventTypeData &event) const
{
    return AbstractTimelineModel::accepted(event) &&
            event.detailType== QmlDebug::AnimationFrame;
}

void QmlProfilerAnimationsModel::loadData()
{
    Q_D(QmlProfilerAnimationsModel);
    clear();
    QmlProfilerDataModel *simpleModel = d->modelManager->qmlModel();
    if (simpleModel->isEmpty())
        return;

    // collect events
    const QVector<QmlProfilerDataModel::QmlEventData> &referenceList = simpleModel->getEvents();
    const QVector<QmlProfilerDataModel::QmlEventTypeData> &typeList = simpleModel->getEventTypes();

    QmlPaintEventData lastEvent;
    qint64 minNextStartTimes[] = {0, 0};

    foreach (const QmlProfilerDataModel::QmlEventData &event, referenceList) {
        const QmlProfilerDataModel::QmlEventTypeData &type = typeList[event.typeIndex];
        if (!accepted(type))
            continue;

        lastEvent.threadId = (QmlDebug::AnimationThread)event.numericData3;

        // initial estimation of the event duration: 1/framerate
        qint64 estimatedDuration = event.numericData1 > 0 ? 1e9/event.numericData1 : 1;

        // the profiler registers the animation events at the end of them
        qint64 realEndTime = event.startTime;

        // ranges should not overlap. If they do, our estimate wasn't accurate enough
        qint64 realStartTime = qMax(event.startTime - estimatedDuration, minNextStartTimes[lastEvent.threadId]);

        // Sometimes our estimate is far off or the server has miscalculated the frame rate
        if (realStartTime >= realEndTime)
            realEndTime = realStartTime + 1;

        // Don't "fix" the framerate even if we've fixed the duration.
        // The server should know better after all and if it doesn't we want to see that.
        lastEvent.framerate = (int)event.numericData1;
        lastEvent.animationcount = (int)event.numericData2;
        QTC_ASSERT(lastEvent.animationcount > 0, continue);

        d->data.insert(insert(realStartTime, realEndTime - realStartTime, event.typeIndex),
                       lastEvent);

        if (lastEvent.threadId == QmlDebug::GuiThread)
            d->maxGuiThreadAnimations = qMax(lastEvent.animationcount, d->maxGuiThreadAnimations);
        else
            d->maxRenderThreadAnimations =
                    qMax(lastEvent.animationcount, d->maxRenderThreadAnimations);

        minNextStartTimes[lastEvent.threadId] = event.startTime + 1;

        d->modelManager->modelProxyCountUpdated(d->modelId, count(), referenceList.count());
    }

    computeNesting();
    d->expandedRowCount = d->collapsedRowCount =
            (d->maxGuiThreadAnimations == 0 || d->maxRenderThreadAnimations == 0) ? 2 : 3;
    d->modelManager->modelProxyCountUpdated(d->modelId, 1, 1);
}

/////////////////// QML interface

int QmlProfilerAnimationsModel::QmlProfilerAnimationsModelPrivate::rowFromThreadId(
        QmlDebug::AnimationThread threadId) const
{
    return (threadId == QmlDebug::GuiThread || maxGuiThreadAnimations == 0) ? 1 : 2;
}

int QmlProfilerAnimationsModel::row(int index) const
{
    Q_D(const QmlProfilerAnimationsModel);
    return d->rowFromThreadId(d->data[index].threadId);
}

int QmlProfilerAnimationsModel::rowMaxValue(int rowNumber) const
{
    Q_D(const QmlProfilerAnimationsModel);
    switch (rowNumber) {
    case 1:
        return d->maxGuiThreadAnimations > 0 ? d->maxGuiThreadAnimations :
                                               d->maxRenderThreadAnimations;
    case 2:
        return d->maxRenderThreadAnimations;
    default:
        return AbstractTimelineModel::rowMaxValue(rowNumber);
    }
}

int QmlProfilerAnimationsModel::selectionId(int index) const
{
    Q_D(const QmlProfilerAnimationsModel);
    return d->data[index].threadId;
}

QColor QmlProfilerAnimationsModel::color(int index) const
{
    Q_D(const QmlProfilerAnimationsModel);
    double fpsFraction = d->data[index].framerate / 60.0;
    if (fpsFraction > 1.0)
        fpsFraction = 1.0;
    if (fpsFraction < 0.0)
        fpsFraction = 0.0;
    return colorByFraction(fpsFraction);
}

float QmlProfilerAnimationsModel::relativeHeight(int index) const
{
    Q_D(const QmlProfilerAnimationsModel);
    const QmlPaintEventData &data = d->data[index];

    // Add some height to the events if we're far from the scale threshold of 2 * DefaultRowHeight.
    // Like that you can see the smaller events more easily.
    int scaleThreshold = 2 * defaultRowHeight() - rowHeight(d->rowFromThreadId(data.threadId));
    float boost = scaleThreshold > 0 ? (0.15 * scaleThreshold / defaultRowHeight()) : 0;

    return boost + (1.0 - boost) * (float)data.animationcount /
            (float)(data.threadId == QmlDebug::GuiThread ? d->maxGuiThreadAnimations :
                                                           d->maxRenderThreadAnimations);
}

QVariantList QmlProfilerAnimationsModel::labels() const
{
    Q_D(const QmlProfilerAnimationsModel);
    QVariantList result;

    if (!d->hidden && d->maxGuiThreadAnimations > 0) {
        QVariantMap element;
        element.insert(QLatin1String("displayName"), QVariant(tr("Animations")));
        element.insert(QLatin1String("description"), QVariant(tr("GUI Thread")));
        element.insert(QLatin1String("id"), QVariant(QmlDebug::GuiThread));
        result << element;
    }

    if (!d->hidden && d->maxRenderThreadAnimations > 0) {
        QVariantMap element;
        element.insert(QLatin1String("displayName"), QVariant(tr("Animations")));
        element.insert(QLatin1String("description"), QVariant(tr("Render Thread")));
        element.insert(QLatin1String("id"), QVariant(QmlDebug::RenderThread));
        result << element;
    }

    return result;
}

QVariantMap QmlProfilerAnimationsModel::details(int index) const
{
    Q_D(const QmlProfilerAnimationsModel);
    QVariantMap result;

    result.insert(QStringLiteral("displayName"), displayName());
    result.insert(tr("Duration"), QmlProfilerBaseModel::formatTime(duration(index)));
    result.insert(tr("Framerate"), QString::fromLatin1("%1 FPS").arg(d->data[index].framerate));
    result.insert(tr("Animations"), QString::fromLatin1("%1").arg(d->data[index].animationcount));
    result.insert(tr("Context"), tr(d->data[index].threadId == QmlDebug::GuiThread ?
                                    "GUI Thread" : "Render Thread"));
    return result;
}

}
}
