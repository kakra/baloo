/*
 * This file is part of the KDE Baloo Project
 * Copyright (C) 2013-2015 Vishesh Handa <me@vhanda.in>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "basicindexingjob.h"
#include "termgenerator.h"
#include "idutils.h"

#include <QStringList>
#include <QFile>

#include <KFileMetaData/Types>
#include <KFileMetaData/TypeInfo>
#include <KFileMetaData/UserMetaData>

using namespace Baloo;

BasicIndexingJob::BasicIndexingJob(const QString& filePath, const QString& mimetype,
                                   IndexingLevel level)
    : m_filePath(filePath)
    , m_mimetype(mimetype)
    , m_indexingLevel(level)
{
}

namespace {

void indexXAttr(const QString& url, Document& doc)
{
    KFileMetaData::UserMetaData userMetaData(url);
    TermGenerator tg(doc);

    const QStringList tags = userMetaData.tags();
    for (const QString& tag : tags) {
        tg.indexXattrText(tag, QByteArray("TA"));
        doc.addXattrTerm(QByteArray("TAG-") + tag.toUtf8());
    }

    int rating = userMetaData.rating();
    if (rating) {
        doc.addXattrTerm(QByteArray("R") + QByteArray::number(rating));
    }

    QString comment = userMetaData.userComment();
    if (!comment.isEmpty()) {
        tg.indexXattrText(comment, QByteArray("C"));
    }
}

QVector<KFileMetaData::Type::Type> typesForMimeType(const QString& mimeType)
{
    using namespace KFileMetaData;
    QVector<Type::Type> types;
    types.reserve(2);

    // Basic types
    if (mimeType.startsWith(QLatin1String("audio/")))
        types << Type::Audio;
    if (mimeType.startsWith(QLatin1String("video/")))
        types << Type::Video;
    if (mimeType.startsWith(QLatin1String("image/")))
        types << Type::Image;
    if (mimeType.startsWith(QLatin1String("text/")))
        types << Type::Text;
    if (mimeType.contains(QLatin1String("document")))
        types << Type::Document;

    if (mimeType.contains(QLatin1String("powerpoint"))) {
        types << Type::Presentation;
        types << Type::Document;
    }
    if (mimeType.contains(QLatin1String("excel"))) {
        types << Type::Spreadsheet;
        types << Type::Document;
    }

    static QMultiHash<QString, Type::Type> typeMapper = {
        {"text/plain", Type::Document},
        // MS Office
        {"application/msword", Type::Document},
        {"application/x-scribus", Type::Document},
        // The old pre-XML MS Office formats are already covered by the excel/powerpoint "contains" above:
        // - application/vnd.ms-powerpoint
        // - application/vnd.ms-excel
        // "openxmlformats-officedocument" and "opendocument" contain "document", i.e. already have Type::Document
        // - application/vnd.openxmlformats-officedocument.wordprocessingml.document
        // - application/vnd.openxmlformats-officedocument.spreadsheetml.sheet
        // - application/vnd.openxmlformats-officedocument.presentationml.presentation
        // - application/vnd.oasis.opendocument.text
        // - application/vnd.oasis.opendocument.spreadsheet
        // - application/vnd.oasis.opendocument.presentation
        // Office 2007
        {"application/vnd.openxmlformats-officedocument.presentationml.presentation", Type::Presentation},
        {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", Type::Spreadsheet},
        // Open Document Formats - http://en.wikipedia.org/wiki/OpenDocument_technical_specification
        {"application/vnd.oasis.opendocument.presentation", Type::Presentation},
        {"application/vnd.oasis.opendocument.spreadsheet", Type::Spreadsheet},
        {"application/pdf", Type::Document},
        {"application/postscript", Type::Document},
        {"application/x-dvi", Type::Document},
        {"application/rtf", Type::Document},
        // EBooks
        {"application/epub+zip", Type::Document},
        {"application/x-mobipocket-ebook", Type::Document},
        // Archives - http://en.wikipedia.org/wiki/List_of_archive_formats
        {"application/x-tar", Type::Archive},
        {"application/x-bzip2", Type::Archive},
        {"application/x-gzip", Type::Archive},
        {"application/x-lzip", Type::Archive},
        {"application/x-lzma", Type::Archive},
        {"application/x-lzop", Type::Archive},
        {"application/x-compress", Type::Archive},
        {"application/x-7z-compressed", Type::Archive},
        {"application/x-ace-compressed", Type::Archive},
        {"application/x-astrotite-afa", Type::Archive},
        {"application/x-alz-compressed", Type::Archive},
        {"application/vnd.android.package-archive", Type::Archive},
        {"application/x-arj", Type::Archive},
        {"application/vnd.ms-cab-compressed", Type::Archive},
        {"application/x-cfs-compressed", Type::Archive},
        {"application/x-dar", Type::Archive},
        {"application/x-lzh", Type::Archive},
        {"application/x-lzx", Type::Archive},
        {"application/x-rar-compressed", Type::Archive},
        {"application/x-stuffit", Type::Archive},
        {"application/x-stuffitx", Type::Archive},
        {"application/x-gtar", Type::Archive},
        {"application/zip", Type::Archive},
        // WPS office
        {"application/wps-office.doc", Type::Document},
        {"application/wps-office.xls", Type::Document},
        {"application/wps-office.xls", Type::Spreadsheet},
        {"application/wps-office.pot", Type::Document},
        {"application/wps-office.pot", Type::Presentation},
        {"application/wps-office.wps", Type::Document},
        {"application/wps-office.docx", Type::Document},
        {"application/wps-office.xlsx", Type::Document},
        {"application/wps-office.xlsx", Type::Spreadsheet},
        {"application/wps-office.pptx", Type::Document},
        {"application/wps-office.pptx", Type::Presentation},
        // Other
        {"text/markdown", Type::Document},
        {"image/vnd.djvu+multipage", Type::Document},
        {"application/x-lyx", Type::Document}
    };

    auto hashIt = typeMapper.find(mimeType);
    while (hashIt != typeMapper.end() && hashIt.key() == mimeType) {
        types.append(hashIt.value());
        ++hashIt;
    }

    return types;
}
} // namespace

BasicIndexingJob::~BasicIndexingJob()
{
}

bool BasicIndexingJob::index()
{
    const QByteArray url = QFile::encodeName(m_filePath);

    QT_STATBUF statBuf;
    if (filePathToStat(url, statBuf) != 0) {
        return false;
    }

    Document doc;
    doc.setId(statBufToId(statBuf));
    doc.setUrl(url);

    QString fileName = url.mid(url.lastIndexOf('/') + 1);

    TermGenerator tg(doc);
    tg.indexFileNameText(fileName);
    tg.indexFileNameText(fileName, QByteArray("F"));
    tg.indexText(m_mimetype, QByteArray("M"));

    // (Content) Modification time, Metadata (e.g. XAttr) change time
    doc.setMTime(statBuf.st_mtime);
    doc.setCTime(statBuf.st_ctime);

    if (S_ISDIR(statBuf.st_mode)) {
        static const QByteArray type = QByteArray("T") + QByteArray::number(static_cast<int>(KFileMetaData::Type::Folder));
        doc.addTerm(type);
        // For folders we do not need to go through file indexing, so we do not set contentIndexing

    } else {
        if (m_indexingLevel == MarkForContentIndexing) {
            doc.setContentIndexing(true);
        }
        // Types
        const QVector<KFileMetaData::Type::Type> tList = typesForMimeType(m_mimetype);
        for (KFileMetaData::Type::Type type : tList) {
            QByteArray num = QByteArray::number(static_cast<int>(type));
            doc.addTerm(QByteArray("T") + num);
        }
    }

    indexXAttr(m_filePath, doc);

    m_doc = doc;
    return true;
}
