/*
 * This file is part of the KDE Baloo Project
 * Copyright (C) 2013  Vishesh Handa <me@vhanda.in>
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

#ifndef _BALOO_FILE_H
#define _BALOO_FILE_H

#include "core_export.h"
#include <KFileMetaData/Properties>

namespace Baloo {

/**
 * @short Provides acess to all File Metadata
 *
 * The File class acts as a temporary container for all the file metadata.
 */
class BALOO_CORE_EXPORT File
{
public:
    File();
    File(const File& f);

    /**
     * Constructor
     *
     * \p url the local url of the file
     */
    File(const QString& url);
    ~File();

    const File& operator =(const File& f);

    /**
     * The local url of the file
     */
    QString path() const;

    /**
     * Represents a unique identifier for this file. This identifier
     * is unique and will never change unlike the url of the file
     */
    QByteArray id() const;

    /**
     * Gives a variant map of the properties that have been extracted
     * from the file by the indexer
     */
    KFileMetaData::PropertyMap properties() const;
    QVariant property(KFileMetaData::Property::Property property) const;

    // FIXME: More descriptive error?
    bool load();
    bool load(const QByteArray& id);
    bool load(const QString& url);

    // FIXME: From id?
private:
    class Private;
    Private* d;
};

}

#endif // _BALOO_FILE_H