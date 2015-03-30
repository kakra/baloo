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

#include "database.h"
#include "postingdb.h"
#include "documentdb.h"
#include "documenturldb.h"
#include "documentiddb.h"
#include "positiondb.h"
#include "documenttimedb.h"
#include "documentdatadb.h"

#include "document.h"
#include "enginequery.h"

#include "andpostingiterator.h"
#include "orpostingiterator.h"
#include "phraseanditerator.h"

#include "writetransaction.h"
#include "idutils.h"

#include <QFile>
#include <QFileInfo>

using namespace Baloo;

Database::Database(const QString& path)
    : m_path(path)
    , m_env(0)
    , m_txn(0)
    , m_postingDB(0)
    , m_positionDB(0)
    , m_documentTermsDB(0)
    , m_documentXattrTermsDB(0)
    , m_documentFileNameTermsDB(0)
    , m_docUrlDB(0)
    , m_docTimeDB(0)
    , m_docDataDB(0)
    , m_contentIndexingDB(0)
    , m_writeTrans(0)
{
}

Database::~Database()
{
    delete m_postingDB;
    delete m_positionDB;
    delete m_documentTermsDB;
    delete m_documentXattrTermsDB;
    delete m_documentFileNameTermsDB;
    delete m_docUrlDB;
    delete m_docTimeDB;
    delete m_docDataDB;
    delete m_contentIndexingDB;

    if (m_txn) {
        abort();
    }
    mdb_env_close(m_env);
}

bool Database::open()
{
    QFileInfo dirInfo(m_path);
    if (!dirInfo.permission(QFile::WriteOwner)) {
        qCritical() << m_path << "does not have write permissions. Aborting";
        return false;
    }

    mdb_env_create(&m_env);
    mdb_env_set_maxdbs(m_env, 9);
    mdb_env_set_mapsize(m_env, 1048576000);

    // The directory needs to be created before opening the environment
    QByteArray arr = QFile::encodeName(m_path) + "/index";
    mdb_env_open(m_env, arr.constData(), MDB_NOSUBDIR, 0664);

    return true;
}

void Database::transaction(Database::TransactionType type)
{
    Q_ASSERT(m_txn == 0);

    uint flags = type == ReadOnly ? MDB_RDONLY : 0;
    int rc = mdb_txn_begin(m_env, NULL, flags, &m_txn);
    Q_ASSERT_X(rc == 0, "Database::transaction", mdb_strerror(rc));

    if (!m_postingDB)
        m_postingDB = new PostingDB(m_txn);
    else
        m_postingDB->setTransaction(m_txn);

    if (!m_positionDB)
        m_positionDB = new PositionDB(m_txn);
    else
        m_positionDB->setTransaction(m_txn);

    if (!m_documentTermsDB)
        m_documentTermsDB = new DocumentDB(m_txn);
    else
        m_documentTermsDB->setTransaction(m_txn);

    if (!m_documentXattrTermsDB)
        m_documentXattrTermsDB = new DocumentDB(m_txn);
    else
        m_documentXattrTermsDB->setTransaction(m_txn);

    if (!m_documentFileNameTermsDB)
        m_documentFileNameTermsDB = new DocumentDB(m_txn);
    else
        m_documentFileNameTermsDB->setTransaction(m_txn);

    if (!m_docUrlDB)
        m_docUrlDB = new DocumentUrlDB(m_txn);
    else
        m_docUrlDB->setTransaction(m_txn);

    if (!m_docTimeDB)
        m_docTimeDB = new DocumentTimeDB(m_txn);
    else
        m_docTimeDB->setTransaction(m_txn);

    if (!m_docDataDB)
        m_docDataDB = new DocumentDataDB("documentdatadb", m_txn);
    else
        m_docDataDB->setTransaction(m_txn);

    if (!m_contentIndexingDB)
        m_contentIndexingDB = new DocumentIdDB(m_txn);
    else
        m_contentIndexingDB->setTransaction(m_txn);

    if (type == ReadWrite) {
        Q_ASSERT(m_writeTrans == 0);
        m_writeTrans = new WriteTransaction(m_postingDB, m_positionDB, m_documentTermsDB, m_documentXattrTermsDB,
                                            m_documentFileNameTermsDB, m_docUrlDB, m_docTimeDB, m_docDataDB, m_contentIndexingDB);
    }
}

QString Database::path() const
{
    return m_path;
}

bool Database::hasDocument(quint64 id)
{
    Q_ASSERT(id > 0);
    return m_documentTermsDB->contains(id);
}

QByteArray Database::documentUrl(quint64 id)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(id > 0);
    return m_docUrlDB->get(id);
}

quint64 Database::documentId(quint64 parentId, const QByteArray& fileName)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(parentId > 0);
    Q_ASSERT(!fileName.isEmpty());

    return m_docUrlDB->getId(parentId, fileName);
}

quint64 Database::documentMTime(quint64 id)
{
    Q_ASSERT(m_txn);
    return m_docTimeDB->get(id).mTime;
}

quint64 Database::documentCTime(quint64 id)
{
    Q_ASSERT(m_txn);
    return m_docTimeDB->get(id).cTime;
}

QByteArray Database::documentData(quint64 id)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(id > 0);
    return m_docDataDB->get(id);
}

bool Database::hasChanges() const
{
    Q_ASSERT(m_txn);
    Q_ASSERT(m_writeTrans);
    return m_writeTrans->hasChanges();
}

QVector<quint64> Database::fetchPhaseOneIds(int size)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(size > 0);
    return m_contentIndexingDB->fetchItems(size);
}

QList<QByteArray> Database::fetchTermsStartingWith(const QByteArray& term)
{
    Q_ASSERT(term.size() > 0);
    return m_postingDB->fetchTermsStartingWith(term);
}

uint Database::phaseOneSize()
{
    Q_ASSERT(m_txn);
    return m_contentIndexingDB->size();
}

uint Database::size()
{
    Q_ASSERT(m_txn);
    return m_documentTermsDB->size();
}

//
// Write Operations
//
void Database::setPhaseOne(quint64 id)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(id > 0);
    Q_ASSERT(m_writeTrans);
    m_contentIndexingDB->put(id);
}

void Database::addDocument(const Document& doc)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(doc.id() > 0);
    Q_ASSERT(m_writeTrans);

    m_writeTrans->addDocument(doc);
}

void Database::removeDocument(quint64 id)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(id > 0);
    Q_ASSERT(m_writeTrans);

    m_writeTrans->removeDocument(id);
}

void Database::replaceDocument(const Document& doc, Database::DocumentOperations operations)
{
    Q_ASSERT(m_txn);
    Q_ASSERT(doc.id() > 0);
    Q_ASSERT(m_writeTrans);
    Q_ASSERT_X(hasDocument(doc.id()), "Database::replaceDocument", "Document does not exist");

    m_writeTrans->replaceDocument(doc, operations);
}

void Database::commit()
{
    Q_ASSERT(m_txn);
    Q_ASSERT(m_writeTrans);

    m_writeTrans->commit();
    delete m_writeTrans;
    m_writeTrans = 0;

    int rc = mdb_txn_commit(m_txn);
    Q_ASSERT_X(rc == 0, "Database::commit", mdb_strerror(rc));

    m_txn = 0;
}

void Database::abort()
{
    Q_ASSERT(m_txn);

    mdb_txn_abort(m_txn);
    m_txn = 0;

    delete m_writeTrans;
    m_writeTrans = 0;
}

//
// Queries
//

PostingIterator* Database::toPostingIterator(const EngineQuery& query)
{
    if (query.leaf()) {
        if (query.op() == EngineQuery::Equal) {
            return m_postingDB->iter(query.term());
        } else if (query.op() == EngineQuery::StartsWith) {
            return m_postingDB->prefixIter(query.term());
        } else {
            Q_ASSERT(0);
            // FIXME: Implement position iterator
        }
    }

    Q_ASSERT(!query.subQueries().isEmpty());

    QVector<PostingIterator*> vec;
    vec.reserve(query.subQueries().size());

    if (query.op() == EngineQuery::Phrase) {
        for (const EngineQuery& q : query.subQueries()) {
            Q_ASSERT_X(q.leaf(), "Database::toPostingIterator", "Phrase queries must contain leaf queries");
            vec << m_positionDB->iter(q.term());
        }

        return new PhraseAndIterator(vec);
    }

    for (const EngineQuery& q : query.subQueries()) {
        vec << toPostingIterator(q);
    }

    if (query.op() == EngineQuery::And) {
        return new AndPostingIterator(vec);
    } else if (query.op() == EngineQuery::Or) {
        return new OrPostingIterator(vec);
    }

    Q_ASSERT(0);
    return 0;
}

QVector<quint64> Database::exec(const EngineQuery& query, int limit)
{
    Q_ASSERT(m_txn);

    QVector<quint64> results;
    PostingIterator* it = toPostingIterator(query);
    if (!it) {
        return results;
    }

    while (it->next() && limit) {
        results << it->docId();
        limit--;
    }

    return results;
}

//
// File path rename
//
void Database::renameFilePath(quint64 id, const Document& newDoc)
{
    Q_ASSERT(id);

    const QByteArray newFilePath = newDoc.url();
    const QByteArray newFileName = newFilePath.mid(newFilePath.lastIndexOf('/') + 1);

    // Update the id -> url db
    m_docUrlDB->rename(id, newFileName);

    replaceDocument(newDoc, FileNameTerms);
}

