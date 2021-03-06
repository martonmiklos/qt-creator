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

#include "displaysettingspage.h"

#include "displaysettings.h"
#include "marginsettings.h"
#include "texteditorconstants.h"
#include "ui_displaysettingspage.h"

#include <coreplugin/icore.h>

#include <QPointer>

namespace TextEditor {

struct DisplaySettingsPage::DisplaySettingsPagePrivate
{
    DisplaySettingsPagePrivate();

    QPointer<QWidget> m_widget;
    Internal::Ui::DisplaySettingsPage *m_page = nullptr;
    DisplaySettings m_displaySettings;
    MarginSettings m_marginSettings;
    QString m_settingsPrefix;
};

DisplaySettingsPage::DisplaySettingsPagePrivate::DisplaySettingsPagePrivate()
{
    m_settingsPrefix = QLatin1String("text");
    m_displaySettings.fromSettings(m_settingsPrefix, Core::ICore::settings());
    m_marginSettings.fromSettings(m_settingsPrefix, Core::ICore::settings());
}

DisplaySettingsPage::DisplaySettingsPage()
  : d(new DisplaySettingsPagePrivate)
{
    setId(Constants::TEXT_EDITOR_DISPLAY_SETTINGS);
    setDisplayName(tr("Display"));
    setCategory(TextEditor::Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
    setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
    setCategoryIconPath(TextEditor::Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
}

DisplaySettingsPage::~DisplaySettingsPage()
{
    delete d;
}

QWidget *DisplaySettingsPage::widget()
{
    if (!d->m_widget) {
        d->m_widget = new QWidget;
        d->m_page = new Internal::Ui::DisplaySettingsPage;
        d->m_page->setupUi(d->m_widget);
        settingsToUI();
    }
    return d->m_widget;
}

void DisplaySettingsPage::apply()
{
    if (!d->m_page) // page was never shown
        return;
    DisplaySettings newDisplaySettings;
    MarginSettings newMarginSettings;

    settingsFromUI(newDisplaySettings, newMarginSettings);
    setDisplaySettings(newDisplaySettings, newMarginSettings);
}

void DisplaySettingsPage::finish()
{
    delete d->m_widget;
    if (!d->m_page) // page was never shown
        return;
    delete d->m_page;
    d->m_page = nullptr;
}

void DisplaySettingsPage::settingsFromUI(DisplaySettings &displaySettings,
                                         MarginSettings &marginSettings) const
{
    displaySettings.m_displayLineNumbers = d->m_page->displayLineNumbers->isChecked();
    displaySettings.m_textWrapping = d->m_page->enableTextWrapping->isChecked();
    marginSettings.m_showMargin = d->m_page->showWrapColumn->isChecked();
    marginSettings.m_marginColumn = d->m_page->wrapColumn->value();
    displaySettings.m_visualizeWhitespace = d->m_page->visualizeWhitespace->isChecked();
    displaySettings.m_displayFoldingMarkers = d->m_page->displayFoldingMarkers->isChecked();
    displaySettings.m_highlightCurrentLine = d->m_page->highlightCurrentLine->isChecked();
    displaySettings.m_highlightBlocks = d->m_page->highlightBlocks->isChecked();
    displaySettings.m_animateMatchingParentheses = d->m_page->animateMatchingParentheses->isChecked();
    displaySettings.m_highlightMatchingParentheses = d->m_page->highlightMatchingParentheses->isChecked();
    displaySettings.m_markTextChanges = d->m_page->markTextChanges->isChecked();
    displaySettings.m_autoFoldFirstComment = d->m_page->autoFoldFirstComment->isChecked();
    displaySettings.m_centerCursorOnScroll = d->m_page->centerOnScroll->isChecked();
    displaySettings.m_openLinksInNextSplit = d->m_page->openLinksInNextSplit->isChecked();
    displaySettings.m_displayFileEncoding = d->m_page->displayFileEncoding->isChecked();
    displaySettings.m_scrollBarHighlights = d->m_page->scrollBarHighlights->isChecked();
    displaySettings.m_animateNavigationWithinFile = d->m_page->animateNavigationWithinFile->isChecked();
    displaySettings.m_displayAnnotations = d->m_page->displayAnnotations->isChecked();
    if (d->m_page->leftAligned->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::NextToContent;
    else if (d->m_page->atMargin->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::NextToMargin;
    else if (d->m_page->rightAligned->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::RightSide;
    else if (d->m_page->betweenLines->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::BetweenLines;
}

void DisplaySettingsPage::settingsToUI()
{
    const DisplaySettings &displaySettings = d->m_displaySettings;
    const MarginSettings &marginSettings = d->m_marginSettings;
    d->m_page->displayLineNumbers->setChecked(displaySettings.m_displayLineNumbers);
    d->m_page->enableTextWrapping->setChecked(displaySettings.m_textWrapping);
    d->m_page->showWrapColumn->setChecked(marginSettings.m_showMargin);
    d->m_page->wrapColumn->setValue(marginSettings.m_marginColumn);
    d->m_page->visualizeWhitespace->setChecked(displaySettings.m_visualizeWhitespace);
    d->m_page->displayFoldingMarkers->setChecked(displaySettings.m_displayFoldingMarkers);
    d->m_page->highlightCurrentLine->setChecked(displaySettings.m_highlightCurrentLine);
    d->m_page->highlightBlocks->setChecked(displaySettings.m_highlightBlocks);
    d->m_page->animateMatchingParentheses->setChecked(displaySettings.m_animateMatchingParentheses);
    d->m_page->highlightMatchingParentheses->setChecked(displaySettings.m_highlightMatchingParentheses);
    d->m_page->markTextChanges->setChecked(displaySettings.m_markTextChanges);
    d->m_page->autoFoldFirstComment->setChecked(displaySettings.m_autoFoldFirstComment);
    d->m_page->centerOnScroll->setChecked(displaySettings.m_centerCursorOnScroll);
    d->m_page->openLinksInNextSplit->setChecked(displaySettings.m_openLinksInNextSplit);
    d->m_page->displayFileEncoding->setChecked(displaySettings.m_displayFileEncoding);
    d->m_page->scrollBarHighlights->setChecked(displaySettings.m_scrollBarHighlights);
    d->m_page->animateNavigationWithinFile->setChecked(displaySettings.m_animateNavigationWithinFile);
    d->m_page->displayAnnotations->setChecked(displaySettings.m_displayAnnotations);
    switch (displaySettings.m_annotationAlignment) {
    case AnnotationAlignment::NextToContent: d->m_page->leftAligned->setChecked(true); break;
    case AnnotationAlignment::NextToMargin: d->m_page->atMargin->setChecked(true); break;
    case AnnotationAlignment::RightSide: d->m_page->rightAligned->setChecked(true); break;
    case AnnotationAlignment::BetweenLines: d->m_page->betweenLines->setChecked(true); break;
    }
}

const DisplaySettings &DisplaySettingsPage::displaySettings() const
{
    return d->m_displaySettings;
}

const MarginSettings &DisplaySettingsPage::marginSettings() const
{
    return d->m_marginSettings;
}

void DisplaySettingsPage::setDisplaySettings(const DisplaySettings &newDisplaySettings,
                                             const MarginSettings &newMarginSettings)
{
    if (newDisplaySettings != d->m_displaySettings) {
        d->m_displaySettings = newDisplaySettings;
        d->m_displaySettings.toSettings(d->m_settingsPrefix, Core::ICore::settings());

        emit displaySettingsChanged(newDisplaySettings);
    }

    if (newMarginSettings != d->m_marginSettings) {
        d->m_marginSettings = newMarginSettings;
        d->m_marginSettings.toSettings(d->m_settingsPrefix, Core::ICore::settings());

        emit marginSettingsChanged(newMarginSettings);
    }
}

} // TextEditor
