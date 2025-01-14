/*
 * This file is part of the KDE Baloo project.
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

#include "enginedebug.h"
#include "positiondb.h"
#include "positioncodec.h"
#include "positioninfo.h"
#include "postingiterator.h"
#include "vectorpositioninfoiterator.h"

using namespace Baloo;

PositionDB::PositionDB(MDB_dbi dbi, MDB_txn* txn)
    : m_txn(txn)
    , m_dbi(dbi)
{
    Q_ASSERT(txn != nullptr);
    Q_ASSERT(dbi != 0);
}

PositionDB::~PositionDB()
{
}

MDB_dbi PositionDB::create(MDB_txn* txn)
{
    MDB_dbi dbi = 0;
    int rc = mdb_dbi_open(txn, "positiondb", MDB_CREATE, &dbi);
    if (rc) {
        qCWarning(ENGINE) << "PositionDB::create" << mdb_strerror(rc);
        return 0;
    }

    return dbi;
}

MDB_dbi PositionDB::open(MDB_txn* txn)
{
    MDB_dbi dbi = 0;
    int rc = mdb_dbi_open(txn, "positiondb", 0, &dbi);
    if (rc) {
        qCWarning(ENGINE) << "PositionDB::open" << mdb_strerror(rc);
        return 0;
    }

    return dbi;
}

void PositionDB::put(const QByteArray& term, const QVector<PositionInfo>& list)
{
    Q_ASSERT(!term.isEmpty());
    Q_ASSERT(!list.isEmpty());

    MDB_val key;
    key.mv_size = term.size();
    key.mv_data = static_cast<void*>(const_cast<char*>(term.constData()));

    PositionCodec codec;
    QByteArray data = codec.encode(list);

    MDB_val val;
    val.mv_size = data.size();
    val.mv_data = static_cast<void*>(data.data());

    int rc = mdb_put(m_txn, m_dbi, &key, &val, 0);
    if (rc) {
        qCWarning(ENGINE) << "PositionDB::put" << mdb_strerror(rc);
    }
}

QVector<PositionInfo> PositionDB::get(const QByteArray& term)
{
    Q_ASSERT(!term.isEmpty());

    MDB_val key;
    key.mv_size = term.size();
    key.mv_data = static_cast<void*>(const_cast<char*>(term.constData()));

    MDB_val val{0, nullptr};
    int rc = mdb_get(m_txn, m_dbi, &key, &val);
    if (rc) {
        if (rc != MDB_NOTFOUND) {
            qCDebug(ENGINE) << "PositionDB::get" << term << mdb_strerror(rc);
        }
        return QVector<PositionInfo>();
    }

    QByteArray data = QByteArray::fromRawData(static_cast<char*>(val.mv_data), val.mv_size);

    PositionCodec codec;
    return codec.decode(data);
}

void PositionDB::del(const QByteArray& term)
{
    Q_ASSERT(!term.isEmpty());

    MDB_val key;
    key.mv_size = term.size();
    key.mv_data = static_cast<void*>(const_cast<char*>(term.constData()));

    int rc = mdb_del(m_txn, m_dbi, &key, nullptr);
    if (rc != 0 && rc != MDB_NOTFOUND) {
        qCDebug(ENGINE) << "PositionDB::del" << term << mdb_strerror(rc);
    }
}

//
// Query
//

VectorPositionInfoIterator* PositionDB::iter(const QByteArray& term)
{
    Q_ASSERT(!term.isEmpty());

    MDB_val key;
    key.mv_size = term.size();
    key.mv_data = static_cast<void*>(const_cast<char*>(term.constData()));

    MDB_val val{0, nullptr};
    int rc = mdb_get(m_txn, m_dbi, &key, &val);
    if (rc) {
        qCDebug(ENGINE) << "PositionDB::iter" << term << mdb_strerror(rc);
        return nullptr;
    }

    PositionCodec codec;
    QByteArray ba(static_cast<char*>(val.mv_data), val.mv_size);
    return new VectorPositionInfoIterator(codec.decode(ba));
}

QMap<QByteArray, QVector<PositionInfo>> PositionDB::toTestMap() const
{
    MDB_cursor* cursor;
    mdb_cursor_open(m_txn, m_dbi, &cursor);

    MDB_val key = {0, nullptr};
    MDB_val val;

    QMap<QByteArray, QVector<PositionInfo>> map;
    while (1) {
        int rc = mdb_cursor_get(cursor, &key, &val, MDB_NEXT);
        if (rc) {
            qCDebug(ENGINE) << "PostingDB::toTestMap" << mdb_strerror(rc);
            break;
        }

        const QByteArray ba(static_cast<char*>(key.mv_data), key.mv_size);
        const QVector<PositionInfo> vinfo = PositionCodec().decode(QByteArray(static_cast<char*>(val.mv_data), val.mv_size));
        map.insert(ba, vinfo);
    }

    mdb_cursor_close(cursor);
    return map;
}
