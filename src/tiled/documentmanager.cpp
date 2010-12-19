/*
 * documentmanager.cpp
 * Copyright 2010, Stefan Beller <stefanbeller@googlemail.com>
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "documentmanager.h"

#include "toolmanager.h"
#include "abstracttool.h"

#include <QTabWidget>
#include <QUndoGroup>
#include <QFileInfo>

using namespace Tiled;
using namespace Tiled::Internal;

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
    , mTabWidget(new QTabWidget)
    , mUndoGroup(new QUndoGroup(this))
    , mSelectedTool(0)
    , mSceneWithTool(0)
    , mUntitledFileName(tr("untitled.tmx"))
{
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    connect(mTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentIndexChanged()));
    connect(mTabWidget, SIGNAL(tabCloseRequested(int)),
            SIGNAL(documentCloseRequested(int)));

    ToolManager *toolManager = ToolManager::instance();
    setSelectedTool(toolManager->selectedTool());
    connect(toolManager, SIGNAL(selectedToolChanged(AbstractTool*)),
            SLOT(setSelectedTool(AbstractTool*)));
}

DocumentManager::~DocumentManager()
{
    // All documents should be closed gracefully beforehand
    Q_ASSERT(mDocuments.isEmpty());
    delete mTabWidget;
}

QWidget *DocumentManager::widget() const
{
    return mTabWidget;
}

MapDocument *DocumentManager::currentDocument() const
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return 0;

    return mDocuments.at(index);
}

MapView *DocumentManager::currentMapView() const
{
    return static_cast<MapView*>(mTabWidget->currentWidget());
}

MapScene *DocumentManager::currentMapScene() const
{
    if (MapView *mapView = currentMapView())
        return mapView->mapScene();

    return 0;
}

void DocumentManager::switchToDocument(int index)
{
    mTabWidget->setCurrentIndex(index);
}

void DocumentManager::addDocument(MapDocument *mapDocument)
{
    Q_ASSERT(mapDocument);
    Q_ASSERT(!mDocuments.contains(mapDocument));

    mDocuments.append(mapDocument);
    mUndoGroup->addStack(mapDocument->undoStack());

    MapView *view = new MapView(mTabWidget);
    MapScene *scene = new MapScene(view); // scene is owned by the view

    scene->setMapDocument(mapDocument);
    view->setScene(scene);
    view->centerOn(0, 0);

    QString tabTitle = QFileInfo(mapDocument->fileName()).fileName();
    if (tabTitle.isEmpty())
        tabTitle = mUntitledFileName;

    connect(mapDocument, SIGNAL(fileNameChanged()),
            SLOT(documentFileNameChanged()));

    const int documentIndex = mDocuments.size() - 1;

    mTabWidget->addTab(view, tabTitle);
    mTabWidget->setTabToolTip(documentIndex, mapDocument->fileName());
    switchToDocument(documentIndex);
}

void DocumentManager::closeCurrentDocument()
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return;

    MapDocument *mapDocument = mDocuments.takeAt(index);
    MapView *mapView = currentMapView();

    mTabWidget->removeTab(index);
    delete mapView;
    delete mapDocument;
}

void DocumentManager::closeAllDocuments()
{
    while (!mDocuments.isEmpty())
        closeCurrentDocument();
}

void DocumentManager::currentIndexChanged()
{
    if (mSceneWithTool) {
        mSceneWithTool->disableSelectedTool();
        mSceneWithTool = 0;
    }

    MapDocument *mapDocument = currentDocument();

    if (mapDocument)
        mUndoGroup->setActiveStack(mapDocument->undoStack());

    emit currentDocumentChanged(mapDocument);

    if (MapScene *mapScene = currentMapScene()) {
        mapScene->setSelectedTool(mSelectedTool);
        mapScene->enableSelectedTool();
        mSceneWithTool = mapScene;
    }
}

void DocumentManager::setSelectedTool(AbstractTool *tool)
{
    if (mSelectedTool == tool)
        return;

    mSelectedTool = tool;

    if (mSceneWithTool) {
        mSceneWithTool->disableSelectedTool();

        if (mSelectedTool) {
            mSceneWithTool->setSelectedTool(mSelectedTool);
            mSceneWithTool->enableSelectedTool();
        }
    }
}

void DocumentManager::documentFileNameChanged()
{
    MapDocument *mapDocument = static_cast<MapDocument*>(sender());
    const int index = mDocuments.indexOf(mapDocument);

    QString tabTitle = QFileInfo(mapDocument->fileName()).fileName();
    if (tabTitle.isEmpty())
        tabTitle = mUntitledFileName;

    mTabWidget->setTabText(index, tabTitle);
}
