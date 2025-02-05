/*
   Copyright (c) 2012-2013 Vishesh Handa <me@vhanda.in>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedString>

#include <QJsonDocument>
#include <QJsonObject>

#include "global.h"
#include "idutils.h"
#include "database.h"
#include "transaction.h"

#include <unistd.h>
#include <KFileMetaData/PropertyInfo>

QString colorString(const QString& input, int color)
{
    if(isatty(fileno(stdout))) {
        QString colorStart = QStringLiteral("\033[0;%1m").arg(color);
        QLatin1String colorEnd("\033[0;0m");

        return colorStart + input + colorEnd;
    } else {
        return input;
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    KAboutData aboutData(QStringLiteral("balooshow"),
                         i18n("Baloo Show"),
                         PROJECT_VERSION,
                         i18n("The Baloo data Viewer - A debugging tool"),
                         KAboutLicense::GPL,
                         i18n("(c) 2012, Vishesh Handa"));
    aboutData.addAuthor(i18n("Vishesh Handa"), i18n("Maintainer"), QStringLiteral("me@vhanda.in"));

    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    parser.addPositionalArgument(QStringLiteral("files"), i18n("Urls, document ids or inodes of the files"), QStringLiteral("[file|id|inode...]"));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("x"),
                                        i18n("Print internal info")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("i"),
                                        i18n("Arguments are interpreted as inode numbers (requires -d)")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("d"),
                                        i18n("Device id for the files"), QStringLiteral("deviceId"), QString()));
    parser.addHelpOption();
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.isEmpty()) {
        parser.showHelp(1);
    }

    QTextStream stream(stdout);

    bool useInodes = parser.isSet(QStringLiteral("i"));
    quint32 devId;
    if (useInodes) {
        bool ok;
        devId = parser.value(QStringLiteral("d")).toULong(&ok, 10);
        if (!ok) {
            devId = parser.value(QStringLiteral("d")).toULong(&ok, 0);
        }
    }

    if (useInodes && devId == 0) {
        stream << i18n("Error: -i requires specifying a device (-d <deviceId>)") << endl;
        parser.showHelp(1);
    }

    Baloo::Database *db = Baloo::globalDatabaseInstance();
    if (!db->open(Baloo::Database::ReadOnlyDatabase)) {
        stream << i18n("The Baloo index could not be opened. Please run \"%1\" to see if Baloo is enabled and working.", QStringLiteral("balooctl status"))
               << endl;
        return 1;
    }

    Baloo::Transaction tr(db, Baloo::Transaction::ReadOnly);

    for (QString url : args) {
        quint64 fid = 0;
        QString internalUrl;
        if (!useInodes) {
            if (QFile::exists(url)) {
                quint64 fsFid = Baloo::filePathToId(QFile::encodeName(url));
                fid = tr.documentId(QFile::encodeName(url));
                internalUrl = QFile::decodeName(tr.documentUrl(fsFid));

                if (fid && fid != fsFid) {
                    stream << i18n("The document IDs of the Baloo DB and the filesystem are different:") <<  "\n";
                    auto dbInode = Baloo::idToInode(fid);
                    auto fsInode = Baloo::idToInode(fsFid);
                    auto dbDevId = Baloo::idToDeviceId(fid);
                    auto fsDevId = Baloo::idToDeviceId(fsFid);

                    stream << "Url: " << url << "\n";
                    stream << "ID:       " << fid << " (DB) <-> " << fsFid << " (FS)\n";
                    stream << "Inode:    " << dbInode << " (DB) " << (dbInode == fsInode ? "== " : "<-> ") << fsInode << " (FS)\n";
                    stream << "DeviceID: " << dbDevId << " (DB) " << (dbDevId == fsDevId ? "== " : "<-> ") << fsDevId << " (FS)" << endl;
                }
                fid = fsFid;
            } else {
                bool ok;
                fid = url.toULongLong(&ok);
                if (!ok) {
                    stream << i18n("%1: Not a valid url or document id", url) << endl;
                    continue;
                }
                if (!tr.hasDocument(fid)) {
                    stream << i18n("%1: No index information found", url) << endl;
                    continue;
                }
                url = QFile::decodeName(tr.documentUrl(fid));
                internalUrl = url;
            }

        } else {
            bool ok;
            quint32 inode = url.toULong(&ok);
            if (!ok) {
                stream << i18n("%1: Failed to parse inode number", url) << endl;
                continue;
            }

            fid = Baloo::devIdAndInodeToId(devId, inode);
            url = QFile::decodeName(tr.documentUrl(fid));
            internalUrl = url;
        }

        bool hasFile = tr.hasDocument(fid);
        if (hasFile) {
            stream << colorString(QString::number(fid), 31);
            stream << QLatin1String(" ");
            stream << colorString(QString::number(Baloo::idToDeviceId(fid)), 28);
            stream << QLatin1String(" ");
            stream << colorString(QString::number(Baloo::idToInode(fid)), 28);
            stream << QLatin1String(" ");
            stream << colorString(url, 32);
            if (!internalUrl.isEmpty() && internalUrl != url) {
                // The document is know by a different name inside the DB,
                // e.g. a hardlink, or untracked rename
                stream << QLatin1String(" [") << internalUrl << QChar(']');
            }
            stream << endl;
        }
        else {
            stream << i18n("%1: No index information found", url) << endl;
            continue;
        }

        Baloo::DocumentTimeDB::TimeInfo time = tr.documentTimeInfo(fid);
        stream << QLatin1String("\tMtime: ") << time.mTime << QChar(' ')
               << QDateTime::fromSecsSinceEpoch(time.mTime).toString(Qt::ISODate)
               << QLatin1String("\n\tCtime: ") << time.cTime << QChar(' ')
               << QDateTime::fromSecsSinceEpoch(time.cTime).toString(Qt::ISODate)
               << endl;

        const QJsonDocument jdoc = QJsonDocument::fromJson(tr.documentData(fid));
        const QVariantMap varMap = jdoc.object().toVariantMap();
        KFileMetaData::PropertyMap propMap = KFileMetaData::toPropertyMap(varMap);
        KFileMetaData::PropertyMap::const_iterator it = propMap.constBegin();
        if (!propMap.isEmpty()) {
            stream << "\tCached properties:" << endl;
        }
        for (; it != propMap.constEnd(); ++it) {
            QString str;
            if (it.value().type() == QVariant::List) {
                QStringList list;
                const auto vars = it.value().toList();
                for (const QVariant& var : vars) {
                    list << var.toString();
                }
                str = list.join(QLatin1String(", "));
            } else {
                str = it.value().toString();
            }

            KFileMetaData::PropertyInfo pi(it.key());
            stream << "\t\t" << pi.displayName() << ": " << str << endl;
        }

        if (parser.isSet(QStringLiteral("x"))) {
            QVector<QByteArray> terms = tr.documentTerms(fid);
            QVector<QByteArray> fileNameTerms = tr.documentFileNameTerms(fid);
            QVector<QByteArray> xAttrTerms = tr.documentXattrTerms(fid);

            auto join = [](const QVector<QByteArray>& v) {
                QByteArray ba;
                for (const QByteArray& arr : v) {
                    ba.append(arr);
                    ba.append(' ');
                }
                return QString(ba);
            };

            stream << "\n" << i18n("Internal Info") <<  "\n";
            stream << i18n("Terms: %1", join(terms)) <<  "\n";
            stream << i18n("File Name Terms: %1", join(fileNameTerms)) <<  "\n";
            stream << i18n("%1 Terms: %2", QStringLiteral("XAttr"), join(xAttrTerms)) << endl;

            QHash<int, QStringList> propertyWords;
            KLocalizedString errorPrefix = ki18nc("Prefix string for internal errors", "Internal Error - %1");

            for (const QByteArray& arr : terms) {
                auto arrAsPrintable = [arr]() {
                    return QString::fromLatin1(arr.toPercentEncoding());
                };

                if (arr.length() < 1) {
                    auto error = QString("malformed term (short): '%1'\n").arg(arrAsPrintable());
                    stream << errorPrefix.subs(error).toString();
                    continue;
                }
                QString word = QString::fromUtf8(arr);

                if (word[0].isUpper()) {
                    if (word[0] == QLatin1Char('X')) {
                        if (word.length() < 4) {
                            // 'X<num>-<value>
                            auto error = QString("malformed property term (short): '%1' in '%2'\n").arg(word).arg(arrAsPrintable());
                            stream << errorPrefix.subs(error).toString();
                            continue;
                        }
                        int posOfNonNumeric = word.indexOf('-', 2);
                        if ((posOfNonNumeric < 0) || ((posOfNonNumeric + 1) == word.length())) {
                            auto error = QString("malformed property term (no data): '%1' in '%2'\n").arg(word).arg(arrAsPrintable());
                            stream << errorPrefix.subs(error).toString();
                            continue;
                        }

                        bool ok;
                        QStringRef prop = word.midRef(1, posOfNonNumeric-1);
                        int propNum = prop.toInt(&ok);
                        QString value = word.mid(posOfNonNumeric + 1);
                        if (!ok) {
                            auto error = QString("malformed property term (bad index): '%1' in '%2'\n").arg(prop).arg(arrAsPrintable());
                            stream << errorPrefix.subs(error).toString();
                            continue;
                        }

                        propertyWords[propNum].append(value);
                    }
                }
            }

            for (auto it = propertyWords.constBegin(); it != propertyWords.constEnd(); it++) {
                auto prop = static_cast<KFileMetaData::Property::Property>(it.key());
                KFileMetaData::PropertyInfo pi(prop);

                stream << pi.name() << ": " << it.value().join(QLatin1Char(' ')) << endl;
            }
        }
    }

    return 0;
}
