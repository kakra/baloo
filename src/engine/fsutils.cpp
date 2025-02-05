/*
 * Copyright (C) 2010 Tobias Koenig <tokoe@kde.org>
 * Copyright (C) 2014 Daniel Vrátil <dvratil@redhat.com>
 * Copyright (C) 2015 Vishesh Handa <vhanda@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "fsutils.h"
#include "enginedebug.h"

#ifdef Q_OS_LINUX
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

using namespace Baloo;

void FSUtils::disableCoW(const QString &path)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(path);
#else
    // from linux/fs.h, so that Baloo does not depend on Linux header files
#ifndef FS_IOC_GETFLAGS
#define FS_IOC_GETFLAGS     _IOR('f', 1, long)
#endif
#ifndef FS_IOC_SETFLAGS
#define FS_IOC_SETFLAGS     _IOW('f', 2, long)
#endif

    // Disable COW on file
#ifndef FS_NOCOW_FL
#define FS_NOCOW_FL         0x00800000
#endif

    ulong flags = 0;
    const int fd = open(qPrintable(path), O_RDONLY);
    if (fd == -1) {
        qCWarning(ENGINE) << "Failed to open" << path << "to modify flags (" << errno << ")";
        return;
    }

    if (ioctl(fd, FS_IOC_GETFLAGS, &flags) == -1) {
        const int errno_ioctl = errno;
        // ignore ENOTTY, filesystem does not support attrs (and likely neither supports COW)
        if (errno_ioctl != ENOTTY) {
            qCWarning(ENGINE) << "ioctl error: failed to get file flags (" << errno_ioctl << ")";
        }
        close(fd);
        return;
    }
    if (!(flags & FS_NOCOW_FL)) {
        flags |= FS_NOCOW_FL;
        if (ioctl(fd, FS_IOC_SETFLAGS, &flags) == -1) {
            const int errno_ioctl = errno;
            // ignore EOPNOTSUPP, returned on filesystems not supporting COW
            if (errno_ioctl != EOPNOTSUPP) {
                qCWarning(ENGINE) << "ioctl error: failed to set file flags (" << errno_ioctl << ")";
            }
            close(fd);
            return;
        }
    }
    close(fd);
#endif
}

