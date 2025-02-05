/*
   This file is part of the KDE Baloo project.
 * Copyright (C) 2015  Vishesh Handa <vhanda@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "documenturldb.h"
#include "idutils.h"
#include "postingiterator.h"

#include <algorithm>
#include <QPair>

using namespace Baloo;

DocumentUrlDB::DocumentUrlDB(MDB_dbi idTreeDb, MDB_dbi idFilenameDb, MDB_txn* txn)
    : m_txn(txn)
    , m_idFilenameDbi(idFilenameDb)
    , m_idTreeDbi(idTreeDb)
{
}

DocumentUrlDB::~DocumentUrlDB()
{
}

bool DocumentUrlDB::put(quint64 docId, const QByteArray& url)
{
    if (!docId || url.isEmpty() || url.endsWith('/')) {
        return false;
    }

    IdFilenameDB idFilenameDb(m_idFilenameDbi, m_txn);

    typedef QPair<quint64, QByteArray> IdNamePath;

    QByteArray arr = url;
    quint64 parentId;
    //
    // id and parent
    //
    {
        quint64 id = filePathToId(arr);
        if (!id) {
            return false;
        }
        Q_ASSERT(id == docId);

        int pos = arr.lastIndexOf('/');
        QByteArray name = arr.mid(pos + 1);

        if (pos == 0) {
            add(id, 0, name);
            return true;
        } else {
            arr.resize(pos);
            parentId = filePathToId(arr);
            if (!parentId) {
                return false;
            }
            add(id, parentId, name);
        }

        if (idFilenameDb.contains(parentId))
            return true;
    }

    //
    // The rest of the path
    //
    QVector<IdNamePath> list;
    while (parentId) {
        quint64 id = parentId;

        int pos = arr.lastIndexOf('/');
        QByteArray name = arr.mid(pos + 1);

        list.prepend(qMakePair(id, name));
        if (pos == 0) {
            break;
        }

        arr.resize(pos);

        parentId = filePathToId(arr);
    }


    for (int i = 0; i < list.size(); i++) {
        quint64 id = list[i].first;
        QByteArray name = list[i].second;

        // Update the IdTree
        quint64 parentId = 0;
        if (i) {
            parentId = list[i-1].first;
        }

        add(id, parentId, name);
    }

    return true;
}

void DocumentUrlDB::add(quint64 id, quint64 parentId, const QByteArray& name)
{
    if (!id || name.isEmpty()) {
        return;
    }

    IdFilenameDB idFilenameDb(m_idFilenameDbi, m_txn);
    IdTreeDB idTreeDb(m_idTreeDbi, m_txn);

    QVector<quint64> subDocs = idTreeDb.get(parentId);

    // insert if not there
    sortedIdInsert(subDocs, id);

    idTreeDb.put(parentId, subDocs);

    // Update the IdFileName
    IdFilenameDB::FilePath path;
    path.parentId = parentId;
    path.name = name;

    idFilenameDb.put(id, path);
}

QByteArray DocumentUrlDB::get(quint64 docId) const
{
    if (!docId) {
        return QByteArray();
    }

    IdFilenameDB idFilenameDb(m_idFilenameDbi, m_txn);

    auto path = idFilenameDb.get(docId);
    if (path.name.isEmpty()) {
        return QByteArray();
    }

    QByteArray ret = path.name;
    quint64 id = path.parentId;
    // arbitrary path depth limit - we have to deal with
    // possibly corrupted DBs out in the wild
    int depth_limit = 512;

    while (id) {
        auto p = idFilenameDb.get(id);
        if (p.name.isEmpty()) {
            return QByteArray();
        }
        if (!depth_limit--) {
            return QByteArray();
        }

        ret = p.name + '/' + ret;
        id = p.parentId;
    }

    return '/' + ret;
}

QVector<quint64> DocumentUrlDB::getChildren(quint64 docId) const
{
    IdTreeDB idTreeDb(m_idTreeDbi, m_txn);
    return idTreeDb.get(docId);
}

quint64 DocumentUrlDB::getId(quint64 docId, const QByteArray& fileName) const
{
    if (fileName.isEmpty()) {
        return 0;
    }

    IdFilenameDB idFilenameDb(m_idFilenameDbi, m_txn);
    IdTreeDB idTreeDb(m_idTreeDbi, m_txn);

    const QVector<quint64> subFiles = idTreeDb.get(docId);
    for (quint64 id : subFiles) {
        IdFilenameDB::FilePath path = idFilenameDb.get(id);
        if (path.name == fileName) {
            return id;
        }
    }

    return 0;
}

QMap<quint64, QByteArray> DocumentUrlDB::toTestMap() const
{
    IdTreeDB idTreeDb(m_idTreeDbi, m_txn);

    QMap<quint64, QVector<quint64>> idTreeMap = idTreeDb.toTestMap();
    QSet<quint64> allIds;

    for (auto it = idTreeMap.cbegin(); it != idTreeMap.cend(); it++) {
        allIds.insert(it.key());
        for (quint64 id : it.value()) {
            allIds.insert(id);
        }
    }

    QMap<quint64, QByteArray> map;
    for (quint64 id : allIds) {
        if (id) {
            QByteArray path = get(id);
            //FIXME: this prevents sanitizing  
            // reactivate  Q_ASSERT(!path.isEmpty());
            map.insert(id, path);
        }
    }

    return map;
}
