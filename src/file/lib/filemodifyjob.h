/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2013  Vishesh Handa <me@vhanda.in>
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

#ifndef FILEMODIFYJOB_H
#define FILEMODIFYJOB_H

#include "itemsavejob.h"
#include "file.h"
#include "file_export.h"

namespace Baloo {

class BALOO_FILE_EXPORT FileModifyJob : public ItemSaveJob
{
    Q_OBJECT
public:
    explicit FileModifyJob(QObject* parent = 0);
    FileModifyJob(const File& file, QObject* parent = 0);

    virtual void start();

    static FileModifyJob* modifyRating(const QStringList& files, int rating);
    static FileModifyJob* modifyUserComment(const QStringList& files, const QString& comment);
    static FileModifyJob* modifyTags(const QStringList& files, const QStringList& tags);

private Q_SLOTS:
    void doStart();

private:
    void doSingleFile();
    void doMultipleFiles();

    class Private;
    Private* d;
};

}
#endif // FILEMODIFYJOB_H