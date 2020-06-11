/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "fileutils.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/iversioncontrol.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/vcsmanager.h>
#include <utils/consoleprocess.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/unixutils.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextStream>
#include <QWidget>

using namespace Utils;

namespace Core {

// Show error with option to open settings.
static void showGraphicalShellError(QWidget *parent, const QString &app, const QString &error)
{
    const QString title = QApplication::translate("Core::Internal",
                                                  "Launching a file browser failed");
    const QString msg = QApplication::translate("Core::Internal",
                                                "Unable to start the file manager:\n\n%1\n\n").arg(app);
    QMessageBox mbox(QMessageBox::Warning, title, msg, QMessageBox::Close, parent);
    if (!error.isEmpty())
        mbox.setDetailedText(QApplication::translate("Core::Internal",
                                                     "\"%1\" returned the following error:\n\n%2").arg(app, error));
    QAbstractButton *settingsButton = mbox.addButton(Core::ICore::msgShowOptionsDialog(),
                                                     QMessageBox::ActionRole);
    mbox.exec();
    if (mbox.clickedButton() == settingsButton)
        ICore::showOptionsDialog(Constants::SETTINGS_ID_INTERFACE, parent);
}

void FileUtils::showInGraphicalShell(QWidget *parent, const QString &pathIn)
{
    const QFileInfo fileInfo(pathIn);
    // Mac, Windows support folder or file.
    if (HostOsInfo::isWindowsHost()) {
        const FilePath explorer = Environment::systemEnvironment().searchInPath(QLatin1String("explorer.exe"));
        if (explorer.isEmpty()) {
            QMessageBox::warning(parent,
                                 QApplication::translate("Core::Internal",
                                                         "Launching Windows Explorer Failed"),
                                 QApplication::translate("Core::Internal",
                                                         "Could not find explorer.exe in path to launch Windows Explorer."));
            return;
        }
        QStringList param;
        if (!fileInfo.isDir())
            param += QLatin1String("/select,");
        param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
        QProcess::startDetached(explorer.toString(), param);
    } else if (HostOsInfo::isMacHost()) {
        QStringList scriptArgs;
        scriptArgs << QLatin1String("-e")
                   << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                                         .arg(fileInfo.canonicalFilePath());
        QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
        scriptArgs.clear();
        scriptArgs << QLatin1String("-e")
                   << QLatin1String("tell application \"Finder\" to activate");
        QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    } else {
        // we cannot select a file here, because no file browser really supports it...
        const QString folder = fileInfo.isDir() ? fileInfo.absoluteFilePath() : fileInfo.filePath();
        const QString app = UnixUtils::fileBrowser(ICore::settings());
        QProcess browserProc;
        const QString browserArgs = UnixUtils::substituteFileBrowserParameters(app, folder);
        bool success = browserProc.startDetached(browserArgs);
        const QString error = QString::fromLocal8Bit(browserProc.readAllStandardError());
        success = success && error.isEmpty();
        if (!success)
            showGraphicalShellError(parent, app, error);
    }
}

void FileUtils::openTerminal(const QString &path)
{
    openTerminal(path, Environment::systemEnvironment());
}

void FileUtils::openTerminal(const QString &path, const Environment &env)
{
    const QFileInfo fileInfo(path);
    const QString pwd = QDir::toNativeSeparators(fileInfo.isDir() ?
                                                 fileInfo.absoluteFilePath() :
                                                 fileInfo.absolutePath());
    ConsoleProcess::startTerminalEmulator(ICore::settings(), pwd, env);
}

QString FileUtils::msgFindInDirectory()
{
    return QApplication::translate("Core::Internal", "Find in This Directory...");
}

QString FileUtils::msgGraphicalShellAction()
{
    if (HostOsInfo::isWindowsHost())
        return QApplication::translate("Core::Internal", "Show in Explorer");
    if (HostOsInfo::isMacHost())
        return QApplication::translate("Core::Internal", "Show in Finder");
    return QApplication::translate("Core::Internal", "Show Containing Folder");
}

QString FileUtils::msgTerminalHereAction()
{
    if (HostOsInfo::isWindowsHost())
        return QApplication::translate("Core::Internal", "Open Command Prompt Here");
    return QApplication::translate("Core::Internal", "Open Terminal Here");
}

QString FileUtils::msgTerminalWithAction()
{
    if (HostOsInfo::isWindowsHost())
        return QApplication::translate("Core::Internal", "Open Command Prompt With",
                        "Opens a submenu for choosing an environment, such as \"Run Environment\"");
    return QApplication::translate("Core::Internal", "Open Terminal With",
                        "Opens a submenu for choosing an environment, such as \"Run Environment\"");
}

void FileUtils::removeFile(const QString &filePath, bool deleteFromFS)
{
    // remove from version control
    VcsManager::promptToDelete(filePath);

    // remove from file system
    if (deleteFromFS) {
        QFile file(filePath);

        if (file.exists()) {
            // could have been deleted by vc
            if (!file.remove())
                QMessageBox::warning(ICore::dialogParent(),
                    QApplication::translate("Core::Internal", "Deleting File Failed"),
                    QApplication::translate("Core::Internal", "Could not delete file %1.").arg(filePath));
        }
    }
}

static inline bool fileSystemRenameFile(const QString &orgFilePath,
                                        const QString &newFilePath)
{
    // QTBUG-3570 is also valid for Qt 5 but QAbstractFileEngine is now in a private header file and
    // the symbol is not exported.
    return QFile::rename(orgFilePath, newFilePath);
}

bool FileUtils::renameFile(const QString &orgFilePath, const QString &newFilePath,
                           HandleIncludeGuards handleGuards)
{
    if (orgFilePath == newFilePath)
        return false;

    QString dir = QFileInfo(orgFilePath).absolutePath();
    IVersionControl *vc = VcsManager::findVersionControlForDirectory(dir);

    bool result = false;
    if (vc && vc->supportsOperation(IVersionControl::MoveOperation))
        result = vc->vcsMove(orgFilePath, newFilePath);
    if (!result) // The moving via vcs failed or the vcs does not support moving, fall back
        result = fileSystemRenameFile(orgFilePath, newFilePath);
    if (result) {
        // yeah we moved, tell the filemanager about it
        DocumentManager::renamedFile(orgFilePath, newFilePath);
    }

    if (result && handleGuards == HandleIncludeGuards::Yes) {
        QFileInfo fi(orgFilePath);
        bool headerUpdateSuccess = updateHeaderFileGuardAfterRename(newFilePath, fi.baseName());
        if (!headerUpdateSuccess) {
            Core::MessageManager::write(QCoreApplication::translate(
                                            "Core::FileUtils",
                                            "Failed to rename the include guard in file \"%1\".")
                                        .arg(newFilePath));
        }
    }

    return result;
}

bool FileUtils::updateHeaderFileGuardAfterRename(const QString &headerPath,
                                                 const QString &oldHeaderBaseName)
{
    bool ret = true;
    QFile headerFile(headerPath);
    if (!headerFile.open(QFile::ReadOnly | QFile::Text))
        return false;

    QRegularExpression guardConditionRegExp(
                QString("(#ifndef)(\\s*)(_*)%1_H(_*)\\s*").arg(oldHeaderBaseName.toUpper()));
    QRegularExpression guardDefineRegexp, guardCloseRegExp;
    QRegularExpressionMatch guardConditionMatch, guardDefineMatch, guardCloseMatch;
    int guardStartLine = -1;
    int guardCloseLine = -1;

    QTextStream inStream(&headerFile);
    int lineCounter = 0;
    QString line;
    while (!inStream.atEnd()) {
        // use trimmed line to get rid from the maunder leading spaces
        inStream.readLineInto(&line);
        line = line.trimmed();
        if (line == QStringLiteral("#pragma once")) {
            // if pragma based guard found skip reading the whole file
            break;
        }

        if (guardStartLine == -1) {
            // we are still looking for the guard condition
            guardConditionMatch = guardConditionRegExp.match(line);
            if (guardConditionMatch.hasMatch()) {
                guardDefineRegexp.setPattern(QString("(#define\\s*%1)%2(_H%3\\s*)")
                                             .arg(guardConditionMatch.captured(3),
                                                  oldHeaderBaseName.toUpper(),
                                                  guardConditionMatch.captured(4)));
                // read the next line for the guard define
                line = inStream.readLine();
                if (!inStream.atEnd()) {
                    guardDefineMatch = guardDefineRegexp.match(line);
                    if (guardDefineMatch.hasMatch()) {
                        // if a proper guard define present in the next line store the line number
                        guardCloseRegExp
                                .setPattern(
                                    QString("(#endif\\s*)(\\/\\/|\\/\\*)(\\s*%1)%2_H(%3\\s*)((\\*\\/)?)")
                                                    .arg(
                                                        guardConditionMatch.captured(3),
                                                        oldHeaderBaseName.toUpper(),
                                                        guardConditionMatch.captured(4)));
                        guardStartLine = lineCounter;
                        lineCounter++;
                    }
                } else {
                    // it the line after the guard opening is not something what we expect
                    // then skip the whole guard replacing process
                    break;
                }
            }
        } else {
            // guard start found looking for the guard closing endif
            guardCloseMatch = guardCloseRegExp.match(line);
            if (guardCloseMatch.hasMatch()) {
                guardCloseLine = lineCounter;
                break;
            }
        }
        lineCounter++;
    }

    if (guardStartLine != -1) {
        // At least the guard have been found ->
        // copy the contents of the header to a temporary file with the updated guard lines
        inStream.seek(0);

        QFileInfo fi(headerFile);
        const auto guardCondition = QString("#ifndef%1%2_H%3\n").arg(
                    guardConditionMatch.captured(1),
                    fi.baseName().toUpper(),
                    guardConditionMatch.captured(2));
        const auto guardDefine = QString("#define %1%2_H%3\n").arg(
                    guardDefineMatch.captured(1),
                    fi.baseName().toUpper(),
                    guardDefineMatch.captured(2));
        const auto guardClose = QString("#endif%1%2%3%4_H%5\n").arg(
                    guardCloseMatch.captured(1),
                    guardCloseMatch.captured(2),
                    guardCloseMatch.captured(3),
                    fi.baseName().toUpper(),
                    guardCloseMatch.captured(4));

        QFile tmpHeader(headerPath + ".tmp");
        if (tmpHeader.open(QFile::WriteOnly)) {
            QTextStream outStream(&tmpHeader);
            int lineCounter = 0;
            while (!inStream.atEnd()) {
                inStream.readLineInto(&line);
                if (lineCounter == guardStartLine) {
                    outStream << guardCondition;
                    outStream << guardDefine;
                    inStream.readLine();
                    lineCounter++;
                } else if (lineCounter == guardCloseLine) {
                    outStream << guardClose;
                } else {
                    outStream << line << '\n';
                }
                lineCounter++;
            }
            tmpHeader.close();
        } else {
            // if opening the temp file failed report error
            ret = false;
        }
    }
    headerFile.close();

    if (ret && guardStartLine != -1) {
        // if the guard was found (and updated updated properly) swap the temp and the target file
        ret = QFile::remove(headerPath);
        if (ret)
            ret = QFile::rename(headerPath + ".tmp", headerPath);
    }

    return ret;
}

} // namespace Core
