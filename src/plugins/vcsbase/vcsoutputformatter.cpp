/****************************************************************************
**
** Copyright (C) 2020 Miklos Marton <martonmiklosqdev@gmail.com>
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
#include "vcsoutputformatter.h"

#include <utils/theme/theme.h>

#include <QDebug>
#include <QDesktopServices>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextCursor>
#include <QUrl>

namespace VcsBase {

QRegularExpression VcsOutputFormatterPrivate::urlRegexp =
        QRegularExpression("(.*)(https?://[^\\s]*)(.*)", QRegularExpression::DotMatchesEverythingOption);

static QTextCharFormat linkFormat(const QTextCharFormat &inputFormat, const QString &href)
{
    QTextCharFormat result = inputFormat;
    result.setForeground(creatorTheme()->color(Theme::TextColorLink));
    result.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    result.setAnchor(true);
    result.setAnchorHref(href);
    return result;
}

VcsOutputFormatter::VcsOutputFormatter() :
    d(new VcsOutputFormatterPrivate)
{

}

VcsOutputFormatter::~VcsOutputFormatter()
{
    delete d;
}

void VcsOutputFormatter::appendMessage(const QString &text, OutputFormat format)
{
    QString out = text;
    const QRegularExpressionMatch match = d->urlRegexp.match(text);
    if (match.hasMatch()) {
        const QTextCharFormat frm = charFormat(format);
        QTextCursor tc = plainTextEdit()->textCursor();
        tc.insertText(match.captured(1), frm);
        tc.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        tc.insertText(match.captured(2), linkFormat(frm, match.captured(2)));
        tc.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        tc.insertText(match.captured(3), frm);
    } else {
        OutputFormatter::appendMessage(text, format);
    }

}

void VcsOutputFormatter::handleLink(const QString &href)
{
    if (!href.isEmpty()) {
        const QRegularExpressionMatch linkMatch = d->urlRegexp.match(href);
        if (linkMatch.hasMatch())
            QDesktopServices::openUrl(QUrl(linkMatch.captured(2)));
    }
}

}
