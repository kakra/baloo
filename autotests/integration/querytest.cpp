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
#include "transaction.h"
#include "document.h"
#include "termgenerator.h"
#include "enginequery.h"
#include "idutils.h"

#include <QTest>
#include <QTemporaryDir>

using namespace Baloo;

class QueryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() {
        dir.reset(new QTemporaryDir());

        auto touchFile = [](const QString& path) {
            QFile file(path);
            file.open(QIODevice::WriteOnly);
            file.write("data");
            file.close();

            return filePathToId(QFile::encodeName(path));
        };

        m_id1 = touchFile(dir->path() + "/file1");
        m_id2 = touchFile(dir->path() + "/file2");
        m_id3 = touchFile(dir->path() + "/file3");
        m_id4 = touchFile(dir->path() + "/file4");

        m_id5 = touchFile(dir->path() + "/tagFile1");
        m_id6 = touchFile(dir->path() + "/tagFile2");
    }

    void init() {
        dbDir = new QTemporaryDir();
        db = new Database(dbDir->path());
        db->open(Database::CreateDatabase);

        insertDocuments();
    }

    void cleanup() {
        delete db;
        delete dbDir;
    }

    void testTermEqual();
    void testTermStartsWith();
    void testTermAnd();
    void testTermOr();
    void testTermPhrase();

    void testTagTermAnd_data();
    void testTagTermAnd();
    void testTagTermPhrase_data();
    void testTagTermPhrase();

private:
    QScopedPointer<QTemporaryDir> dir;
    QTemporaryDir* dbDir;
    Database* db;

    void insertDocuments();
    void addDocument(Transaction* tr,const QString& text, quint64 id, const QString& url)
    {
        Document doc;
        doc.setUrl(QFile::encodeName(url));

        QString fileName = url.mid(url.lastIndexOf('/') + 1);

        TermGenerator tg(doc);
        tg.indexText(text);
        tg.indexFileNameText(fileName);
        doc.setId(id);
        doc.setMTime(1);
        doc.setCTime(2);

        tr->addDocument(doc);
    }

    void insertTagDocuments();
    void addTagDocument(Transaction* tr,const QStringList& tags, quint64 id, const QString& url)
    {
        Document doc;
        doc.setUrl(QFile::encodeName(url));

        QString fileName = url.mid(url.lastIndexOf('/') + 1);

        TermGenerator tg(doc);
        tg.indexText("text/plain", QByteArray("M"));
        for (const QString& tag : tags) {
            tg.indexXattrText(tag, QByteArray("TA"));
        }
        tg.indexFileNameText(fileName);
        doc.setId(id);
        doc.setMTime(3);
        doc.setCTime(4);

        tr->addDocument(doc);
    }

    quint64 m_id1;
    quint64 m_id2;
    quint64 m_id3;
    quint64 m_id4;
    quint64 m_id5;
    quint64 m_id6;
};


void QueryTest::insertDocuments()
{
    Transaction tr(db, Transaction::ReadWrite);
    addDocument(&tr, QStringLiteral("The quick brown foxed jumped over the crazy dog"), m_id1, dir->path() + "/file1");
    addDocument(&tr, QStringLiteral("The night is dark and full of terror"), m_id2, dir->path() + "/file2");
    addDocument(&tr, QStringLiteral("Don't feel sorry for yourself. Only assholes do that"), m_id3, dir->path() + "/file3");
    addDocument(&tr, QStringLiteral("Only the dead stay 17 forever. crazy"), m_id4, dir->path() + "/file4");
    tr.commit();
}

void QueryTest::insertTagDocuments()
{
    Transaction tr(db, Transaction::ReadWrite);
    addTagDocument(&tr, {"One", "Two", "Three", "Four", "F1"}, m_id5, dir->path() + "/tagFile1");
    addTagDocument(&tr, {"One", "Two-Three", "Four", "F2"}, m_id6, dir->path() + "/tagFile2");
    tr.commit();
}

void QueryTest::testTermEqual()
{
    EngineQuery q("the");

    QVector<quint64> result = {m_id1, m_id2, m_id4};
    Transaction tr(db, Transaction::ReadOnly);
    QCOMPARE(tr.exec(q), result);
}

void QueryTest::testTermStartsWith()
{
    EngineQuery q("for", EngineQuery::StartsWith);

    QVector<quint64> result = {m_id3, m_id4};
    Transaction tr(db, Transaction::ReadOnly);
    QCOMPARE(tr.exec(q), result);
}

void QueryTest::testTermAnd()
{
    QVector<EngineQuery> queries;
    queries << EngineQuery("for");
    queries << EngineQuery("sorry");

    EngineQuery q(queries, EngineQuery::And);

    QVector<quint64> result = {m_id3};
    Transaction tr(db, Transaction::ReadOnly);
    QCOMPARE(tr.exec(q), result);
}

void QueryTest::testTermOr()
{
    QVector<EngineQuery> queries;
    queries << EngineQuery("over");
    queries << EngineQuery("terror");

    EngineQuery q(queries, EngineQuery::Or);

    QVector<quint64> result = {m_id1, m_id2};
    Transaction tr(db, Transaction::ReadOnly);
    QCOMPARE(tr.exec(q), result);
}

void QueryTest::testTermPhrase()
{
    QVector<EngineQuery> queries;
    queries << EngineQuery("the");
    queries << EngineQuery("crazy");

    EngineQuery q(queries, EngineQuery::Phrase);

    QVector<quint64> result = {m_id1};
    Transaction tr(db, Transaction::ReadOnly);
    QCOMPARE(tr.exec(q), result);
}

void QueryTest::testTagTermAnd_data()
{
    QTest::addColumn<QByteArrayList>("terms");
    QTest::addColumn<QVector<quint64>>("matchIds");

    QTest::addRow("Simple match") << QByteArrayList({"one", "four"})
        << QVector<quint64> { m_id5, m_id6 };
    QTest::addRow("Only one") << QByteArrayList({"one", "f1"})
        << QVector<quint64> { m_id5 };
    QTest::addRow("Also from phrase") << QByteArrayList({"two", "three"})
        << QVector<quint64> { m_id5, m_id6 };
}

void QueryTest::testTagTermAnd()
{
    insertTagDocuments();
    QFETCH(QByteArrayList, terms);
    QFETCH(QVector<quint64>, matchIds);

    QByteArray prefix{"TA"};
    QVector<EngineQuery> queries;
    for (const QByteArray& term : terms) {
        queries << EngineQuery(prefix + term);
    }

    EngineQuery q(queries, EngineQuery::And);

    Transaction tr(db, Transaction::ReadOnly);
    qDebug() << matchIds << tr.exec(q);
    QCOMPARE(tr.exec(q), matchIds);
}

void QueryTest::testTagTermPhrase_data()
{
    QTest::addColumn<QByteArrayList>("terms");
    QTest::addColumn<QVector<quint64>>("matchIds");

    QTest::addRow("Simple match") << QByteArrayList({"one"})
        << QVector<quint64> { m_id5, m_id6 };
    QTest::addRow("Apart") << QByteArrayList({"two", "four"})
        << QVector<quint64> { };
    QTest::addRow("Adjacent") << QByteArrayList({"three", "four"})
        << QVector<quint64> { };
    QTest::addRow("Only phrase") << QByteArrayList({"two", "three"})
        << QVector<quint64> { m_id6 };
}

void QueryTest::testTagTermPhrase()
{
    insertTagDocuments();
    QFETCH(QByteArrayList, terms);
    QFETCH(QVector<quint64>, matchIds);

    QByteArray prefix{"TA"};
    QVector<EngineQuery> queries;
    for (const QByteArray& term : terms) {
        queries << EngineQuery(prefix + term);
    }

    EngineQuery q(queries, EngineQuery::Phrase);

    Transaction tr(db, Transaction::ReadOnly);
    auto res = tr.exec(q);
    qDebug() << matchIds << res;
    QCOMPARE(res, matchIds);
}

QTEST_MAIN(QueryTest)

#include "querytest.moc"
