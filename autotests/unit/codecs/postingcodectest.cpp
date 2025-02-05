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

#include "postingcodec.h"

#include <QObject>
#include <QTest>

using namespace Baloo;

class PostingCodecTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void test() {
        PostingCodec codec;

        QVector<quint64> vec = {1, 2, 9, 12};
        QByteArray arr = codec.encode(vec);
        QVERIFY(!arr.isEmpty());

        QVector<quint64> vec2 = codec.decode(arr);
        QCOMPARE(vec2, vec);
    }

};

QTEST_MAIN(PostingCodecTest)

#include "postingcodectest.moc"
