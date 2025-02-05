/*
 * Copyright (C) 2014  Vishesh Handa <me@vhanda.in>
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

#include "filtereddiriterator.h"
#include "fileindexerconfig.h"

#include <QFileInfo>

using namespace Baloo;

FilteredDirIterator::FilteredDirIterator(const FileIndexerConfig* config, const QString& folder, Filter filter)
    : m_config(config)
    , m_currentIter(nullptr)
    , m_filters(QDir::NoDotAndDotDot | QDir::Readable | QDir::NoSymLinks)
    , m_firstItem(false)
{
    if (filter == DirsOnly) {
        m_filters |= QDir::Dirs;
    } else if (filter == FilesAndDirs) {
        m_filters |= (QDir::Files | QDir::Dirs);
    }

    if (!m_config || m_config->shouldFolderBeIndexed(folder)) {
        m_currentIter = new QDirIterator(folder, m_filters);
        m_firstItem = true;
    }
}

FilteredDirIterator::~FilteredDirIterator()
{
    delete m_currentIter;
}

QString FilteredDirIterator::next()
{
    if (m_firstItem) {
        m_firstItem = false;
        m_filePath = m_currentIter->path();
        m_fileInfo = QFileInfo(m_filePath);
        return m_filePath;
    }

    m_filePath.clear();
    if (!m_currentIter) {
        m_fileInfo = QFileInfo();
        return QString();
    }

    while (!m_currentIter->hasNext()) {
        delete m_currentIter;
        m_currentIter = nullptr;

        if (!m_paths.isEmpty()) {
            const QString path = m_paths.pop();
            m_currentIter = new QDirIterator(path, m_filters);
        } else {
            m_fileInfo = QFileInfo();
            return QString();
        }
    }

    m_filePath = m_currentIter->next();
    m_fileInfo = m_currentIter->fileInfo();

    if (m_fileInfo.isDir()) {
        if (shouldIndexFolder(m_filePath)) {
            m_paths.push(m_filePath);
            return m_filePath;
        } else {
            return next();
        }
    }
    else if (m_fileInfo.isFile()) {
        bool shouldIndexHidden = false;
        if (m_config)
            shouldIndexHidden = m_config->indexHiddenFilesAndFolders();

        bool shouldIndexFile = (!m_fileInfo.isHidden() || shouldIndexHidden)
                               && (!m_config || m_config->shouldFileBeIndexed(m_fileInfo.fileName()));
        if (shouldIndexFile) {
            return m_filePath;
        } else {
            return next();
        }
    }
    else {
        return next();
    }
}

QString FilteredDirIterator::filePath() const
{
    return m_filePath;
}

QFileInfo FilteredDirIterator::fileInfo() const
{
    return m_fileInfo;
}

bool FilteredDirIterator::shouldIndexFolder(const QString& path) const
{
    if (!m_config) {
        return true;
    }

    QString folder;
    if (m_config->folderInFolderList(path, folder)) {
        // we always index the folders in the list
        // ignoring the name filters
        if (folder == path)
            return true;

        // check for hidden folders
        QFileInfo fi(path);
        if (!m_config->indexHiddenFilesAndFolders() && fi.isHidden())
            return false;

        return m_config->shouldFileBeIndexed(fi.fileName());
    }

    return false;
}

