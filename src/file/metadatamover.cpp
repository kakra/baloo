/* This file is part of the KDE Project
   Copyright (c) 2009-2011 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2013-2014 Vishesh Handa <vhanda@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "metadatamover.h"
#include "database.h"
#include "transaction.h"
#include "basicindexingjob.h"
#include "idutils.h"
#include "mainhub.h"
#include "baloodebug.h"

#include "baloowatcherapplication_interface.h"

#include <QFile>
#include <QDBusConnection>

using namespace Baloo;

MetadataMover::MetadataMover(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{
    m_serviceWatcher.setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &MetadataMover::watcherServiceUnregistered);
}


MetadataMover::~MetadataMover()
{
}

bool MetadataMover::hasWatcher() const
{
    return !m_watcherApplications.isEmpty();
}


static void buildRecursiveList(quint64 parentId, QList<QString> &fileList, Transaction &tr)
{
    fileList.push_back(QFile::decodeName(tr.documentUrl(parentId)));

    const auto childrenIds = tr.childrenDocumentId(parentId);

    for (const auto oneChildren : childrenIds) {
        buildRecursiveList(oneChildren, fileList, tr);
    }
}

void MetadataMover::moveFileMetadata(const QString& from, const QString& to)
{
//    qCDebug(BALOO) << from << to;
    Q_ASSERT(!from.isEmpty() && from != QLatin1String("/"));
    Q_ASSERT(!to.isEmpty() && to != QLatin1String("/"));

    Transaction tr(m_db, Transaction::ReadWrite);

    quint64 id = tr.documentId(QFile::encodeName(from));
    QList<QString> filesList;
    qCDebug(BALOO) << "MetadataMover::moveFileMetadata" << (hasWatcher() ? "has watcher" : "has no watcher");
    qCDebug(BALOO) << "MetadataMover::moveFileMetadata" << "id" << id;
    if (id && hasWatcher()) {
        buildRecursiveList(id, filesList, tr);
    }

    // We do NOT get deleted messages for overwritten files! Thus, we
    // have to remove all metadata for overwritten files first.
    removeMetadata(&tr, to);

    // and finally update the old statements
    updateMetadata(&tr, from, to);

    tr.commit();

    if (hasWatcher()) {
        qCDebug(BALOO) << "MetadataMover::moveFileMetadata" << "notifyWatchers" << filesList;
        notifyWatchers(from, to, filesList);
    }
}

void MetadataMover::removeFileMetadata(const QString& file)
{
    Q_ASSERT(!file.isEmpty() && file != QLatin1String("/"));

    Transaction tr(m_db, Transaction::ReadWrite);
    removeMetadata(&tr, file);
    tr.commit();
}

void MetadataMover::registerBalooWatcher(const QString &service)
{
    int firstSlash = service.indexOf('/');
    if (firstSlash == -1) {
        return;
    }

    QString dbusServiceName = service.left(firstSlash);
    QString dbusPath = service.mid(firstSlash);

    m_serviceWatcher.addWatchedService(dbusServiceName);

    m_watcherApplications.insert(dbusServiceName, new org::kde::BalooWatcherApplication(dbusServiceName, dbusPath, QDBusConnection::sessionBus(), this));

    qCDebug(BALOO) << "MetadataMover::registerBalooWatcher" << service << dbusServiceName << dbusPath;
}

void MetadataMover::removeMetadata(Transaction* tr, const QString& url)
{
    Q_ASSERT(!url.isEmpty());

    quint64 id = tr->documentId(QFile::encodeName(url));
    if (!id) {
        Q_EMIT fileRemoved(url);
        return;
    }

    bool isDir = url.endsWith('/');
    if (!isDir) {
        tr->removeDocument(id);
    } else {
        tr->removeRecursively(id);
    }

    Q_EMIT fileRemoved(url);
}

void MetadataMover::updateMetadata(Transaction* tr, const QString& from, const QString& to)
{
    qCDebug(BALOO) << from << "->" << to;
    Q_ASSERT(!from.isEmpty() && !to.isEmpty());
    Q_ASSERT(from[from.size()-1] != QLatin1Char('/'));
    Q_ASSERT(to[to.size()-1] != QLatin1Char('/'));

    QByteArray toPath = QFile::encodeName(to);
    quint64 id = filePathToId(toPath);
    if (!id) {
        qWarning() << "File moved to path which now no longer exists -" << to;
        return;
    }

    if (!tr->hasDocument(id)) {
        //
        // If we have no metadata yet we need to tell the file indexer so it can
        // create the metadata in case the target folder is configured to be indexed.
        //
        qCDebug(BALOO) << "Moved without data";
        Q_EMIT movedWithoutData(to);
        return;
    }

    BasicIndexingJob job(toPath, QString(), BasicIndexingJob::NoLevel);
    job.index();
    tr->replaceDocument(job.document(), DocumentUrl | FileNameTerms);

    // Possible scenarios
    // 1. file moves to the same device - id is preserved
    // 2. file moves to a different device - id is not preserved
}

void MetadataMover::notifyWatchers(const QString &from, const QString &to, const QList<QString> &filesList)
{
    for (org::kde::BalooWatcherApplication *watcherApplication : qAsConst(m_watcherApplications)) {
        qCDebug(BALOO) << "MetadataMover::notifyWatchers" << watcherApplication->service() << watcherApplication->objectName() << watcherApplication->path();
        watcherApplication->renamedFiles(from, to, filesList);
    }
}

void MetadataMover::watcherServiceUnregistered(const QString &serviceName)
{
    auto itService = m_watcherApplications.find(serviceName);
    if (itService == m_watcherApplications.end()) {
        return;
    }

    qCDebug(BALOO) << "MetadataMover::watcherServiceUnregistered" << itService.key();

    delete itService.value();
    m_watcherApplications.erase(itService);
}
