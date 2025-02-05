/*
 * This file is part of the KDE Baloo project.
 * Copyright (C) 2014-2015 Vishesh Handa <vhanda@kde.org>
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

#include "termgenerator.h"
#include "document.h"

#include <QTextBoundaryFinder>
#include <QByteArray>

using namespace Baloo;

TermGenerator::TermGenerator(Document& doc)
    : m_doc(doc)
    , m_position(1)
{
}

void TermGenerator::indexText(const QString& text)
{
    indexText(text, QByteArray());
}

QByteArrayList TermGenerator::termList(const QString& text_)
{
    QString text(text_);
    text.replace(QLatin1Char('_'), QLatin1Char(' '));

    int start = 0;
    int end = 0;

    QByteArrayList list;
    QTextBoundaryFinder bf(QTextBoundaryFinder::Word, text);
    for (; bf.position() != -1; bf.toNextBoundary()) {
        if (bf.boundaryReasons() & QTextBoundaryFinder::StartOfItem) {
            start = bf.position();
            continue;
        }
        else if (bf.boundaryReasons() & QTextBoundaryFinder::EndOfItem) {
            end = bf.position();

            QString str = text.mid(start, end - start);

            // Remove all accents. It is important to call toLower after normalization,
            // since some exotic unicode symbols can remain uppercase
            const QString denormalized = str.normalized(QString::NormalizationForm_KD).toLower();

            QString cleanString;
            cleanString.reserve(denormalized.size());
            for (const QChar& ch : denormalized) {
                auto cat = ch.category();
                if (cat != QChar::Mark_NonSpacing && cat != QChar::Mark_SpacingCombining && cat != QChar::Mark_Enclosing) {
                    cleanString.append(ch);
                }
            }

            str = cleanString.normalized(QString::NormalizationForm_KC);
            if (!str.isEmpty()) {
                // Truncate the string to avoid arbitrarily long terms
                list << str.leftRef(maxTermSize).toUtf8();
            }
        }
    }

    return list;
}

void TermGenerator::indexText(const QString& text, const QByteArray& prefix)
{
    const QByteArrayList terms = termList(text);
    if (terms.size() == 1) {
        QByteArray finalArr = prefix + terms[0];
        m_doc.addTerm(finalArr);
        return;
    }
    for (const QByteArray& term : terms) {
        QByteArray finalArr = prefix + term;

        m_doc.addPositionTerm(finalArr, m_position);
        m_position++;
    }
    m_position++;
}

void TermGenerator::indexFileNameText(const QString& text, const QByteArray& prefix)
{
    const QByteArrayList terms = termList(text);
    if (terms.size() == 1) {
        QByteArray finalArr = prefix + terms[0];
        m_doc.addFileNameTerm(finalArr);
        return;
    }
    for (const QByteArray& term : terms) {
        QByteArray finalArr = prefix + term;

        m_doc.addFileNamePositionTerm(finalArr, m_position);
        m_position++;
    }
    m_position++;
}

void TermGenerator::indexFileNameText(const QString& text)
{
    indexFileNameText(text, QByteArray());
}

void TermGenerator::indexXattrText(const QString& text, const QByteArray& prefix)
{
    const QByteArrayList terms = termList(text);
    if (terms.size() == 1) {
        QByteArray finalArr = prefix + terms[0];
        m_doc.addXattrTerm(finalArr);
        return;
    }
    for (const QByteArray& term : terms) {
        QByteArray finalArr = prefix + term;

        m_doc.addXattrPositionTerm(finalArr, m_position);
        m_position++;
    }
    m_position++;
}

int TermGenerator::position() const
{
    return m_position;
}

void TermGenerator::setPosition(int position)
{
    m_position = position;
}

