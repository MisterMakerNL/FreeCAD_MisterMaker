/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *   Copyright (c) 2014 Luke Parry <l.parry@warwick.ac.uk>                 *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>
#include <vector>
#endif

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/Link.h>

#include <Base/Console.h>
#include <Base/Tools.h>

#include <Gui/Action.h>
#include <Gui/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/Command.h>
#include <Gui/Control.h>
#include <Gui/Document.h>
#include <Gui/FileDialog.h>
#include <Gui/MainWindow.h>
#include <Gui/Selection/Selection.h>
#include <Gui/Selection/SelectionObject.h>
#include <Gui/ViewProvider.h>
#include <Gui/WaitCursor.h>

#include <Mod/Spreadsheet/App/Sheet.h>

#include <Mod/TechDraw/App/DrawComplexSection.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawProjGroup.h>
#include <Mod/TechDraw/App/DrawUtil.h>
#include <Mod/TechDraw/App/DrawSVGTemplate.h>
#include <Mod/TechDraw/App/DrawViewArch.h>
#include <Mod/TechDraw/App/DrawViewClip.h>
#include <Mod/TechDraw/App/DrawViewDetail.h>
#include <Mod/TechDraw/App/DrawViewDraft.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/DrawViewSymbol.h>
#include <Mod/TechDraw/App/Preferences.h>
#include <Mod/TechDraw/App/DrawBrokenView.h>

#include "DrawGuiUtil.h"
#include "MDIViewPage.h"
#include "QGIViewPart.h"
#include "QGSPage.h"
#include "QGVPage.h"
#include "Rez.h"
#include "TaskActiveView.h"
#include "TaskComplexSection.h"
#include "TaskDetail.h"
#include "TaskProjGroup.h"
#include "TaskProjection.h"
#include "TaskSectionView.h"
#include "ViewProviderPage.h"
#include "ViewProviderDrawingView.h"
#include "CommandHelpers.h"

void execSimpleSection(Gui::Command* cmd);
void execComplexSection(Gui::Command* cmd);
void getSelectedShapes(Gui::Command* cmd,
                      std::vector<App::DocumentObject*>& shapes,
                      std::vector<App::DocumentObject*>& xShapes,
                      App::DocumentObject* faceObj,
                      std::string& faceName);

std::pair<App::DocumentObject*, std::string> faceFromSelection();
std::pair<Base::Vector3d, Base::Vector3d> viewDirection();

class Vertex;
using namespace TechDrawGui;
using namespace TechDraw;
using DU = DrawUtil;

//===========================================================================
// TechDraw_PageDefault
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawPageDefault)

CmdTechDrawPageDefault::CmdTechDrawPageDefault() : Command("TechDraw_PageDefault")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Default Page");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_PageDefault";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_PageDefault";
}

void CmdTechDrawPageDefault::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    QString templateFileName = Preferences::defaultTemplate();
    QFileInfo tfi(templateFileName);
    if (tfi.isReadable()) {
        Gui::WaitCursor wc;
        openCommand(QT_TRANSLATE_NOOP("Command", "Drawing create page"));

        auto page = getDocument()->addObject<TechDraw::DrawPage>("Page");
        if (!page) {
            throw Base::TypeError("CmdTechDrawPageDefault - page not created");
        }
        page->translateLabel("DrawPage", "Page", page->getNameInDocument());

        auto svgTemplate = getDocument()->addObject<TechDraw::DrawSVGTemplate>("Template");
        if (!svgTemplate) {
            throw Base::TypeError("CmdTechDrawPageDefault - template not created");
        }
        svgTemplate->translateLabel("DrawSVGTemplate", "Template", svgTemplate->getNameInDocument());

        page->Template.setValue(svgTemplate);
        auto filespec = DU::cleanFilespecBackslash(templateFileName.toStdString());
        svgTemplate->Template.setValue(filespec);

        updateActive();
        commitCommand();

        TechDrawGui::ViewProviderPage *dvp = dynamic_cast<TechDrawGui::ViewProviderPage *>
                                                 (Gui::Application::Instance->getViewProvider(page));
        if (dvp) {
            dvp->show();
        }
    }
    else {
        QMessageBox::critical(Gui::getMainWindow(), QLatin1String("No template"),
                              QLatin1String("No default template found"));
    }
}

bool CmdTechDrawPageDefault::isActive() { return hasActiveDocument(); }

//===========================================================================
// TechDraw_PageTemplate
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawPageTemplate)

CmdTechDrawPageTemplate::CmdTechDrawPageTemplate() : Command("TechDraw_PageTemplate")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Page using Template");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_PageTemplate";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_PageTemplate";
}

void CmdTechDrawPageTemplate::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    QString work_dir = Gui::FileDialog::getWorkingDirectory();
    QString templateDir = Preferences::defaultTemplateDir();
    QString templateFileName = Gui::FileDialog::getOpenFileName(
        Gui::getMainWindow(), QString::fromUtf8(QT_TR_NOOP("Select a Template File")), templateDir,
        QString::fromUtf8(QT_TR_NOOP("Template (*.svg)")));
    Gui::FileDialog::setWorkingDirectory(work_dir);// Don't overwrite WD with templateDir

    if (templateFileName.isEmpty()) {
        return;
    }

    QFileInfo tfi(templateFileName);
    if (tfi.isReadable()) {
        Gui::WaitCursor wc;
        openCommand(QT_TRANSLATE_NOOP("Command", "Drawing create page"));

        auto page = getDocument()->addObject<TechDraw::DrawPage>("Page");
        if (!page) {
            throw Base::TypeError("CmdTechDrawPageTemplate - page not created");
        }
        page->translateLabel("DrawPage", "Page", page->getNameInDocument());

        auto svgTemplate = getDocument()->addObject<TechDraw::DrawSVGTemplate>("Template");
        if (!svgTemplate) {
            throw Base::TypeError("CmdTechDrawPageTemplate - template not created");
        }
        svgTemplate->translateLabel("DrawSVGTemplate", "Template", svgTemplate->getNameInDocument());

        page->Template.setValue(svgTemplate);
        auto filespec = DU::cleanFilespecBackslash(templateFileName.toStdString());
        svgTemplate->Template.setValue(filespec);

        updateActive();
        commitCommand();

        TechDrawGui::ViewProviderPage *dvp = dynamic_cast<TechDrawGui::ViewProviderPage *>
                                                 (Gui::Application::Instance->getViewProvider(page));
        if (dvp) {
            dvp->show();
        }
    }
    else {
        QMessageBox::critical(Gui::getMainWindow(), QLatin1String("No template"),
                              QLatin1String("Template file is invalid"));
    }
}

bool CmdTechDrawPageTemplate::isActive() { return hasActiveDocument(); }

//===========================================================================
// TechDraw_RedrawPage
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawRedrawPage)

CmdTechDrawRedrawPage::CmdTechDrawRedrawPage() : Command("TechDraw_RedrawPage")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Redraw Page");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_RedrawPage";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_RedrawPage";
}

void CmdTechDrawRedrawPage::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    Gui::WaitCursor wc;

    page->redrawCommand();
}

bool CmdTechDrawRedrawPage::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveView = DrawGuiUtil::needView(this, false);
    return (havePage && haveView);
}

//===========================================================================
// TechDraw_PrintAll
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawPrintAll)

CmdTechDrawPrintAll::CmdTechDrawPrintAll() : Command("TechDraw_PrintAll")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Print All Pages");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_PrintAll";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_PrintAll";
}

void CmdTechDrawPrintAll::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    MDIViewPage::printAllPages();
}

bool CmdTechDrawPrintAll::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_View
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawView)

CmdTechDrawView::CmdTechDrawView() : Command("TechDraw_View")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert View");
    sToolTipText = QT_TR_NOOP("Insert a View in current page.\n"
        "Selected objects, spreadsheets or BIM section planes will be added.\n"
        "Without a selection, a file browser lets you select a SVG or image file.");
    sWhatsThis = "TechDraw_View";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_View";
}

void CmdTechDrawView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    bool viewCreated = false;
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    // switch to the page if it's not current active window
    auto* vpp = freecad_cast<ViewProviderPage*>
        (Gui::Application::Instance->getViewProvider(page));
    if (vpp) {
        vpp->show();
    }


    //set projection direction from selected Face
    //use first object with a face selected
    std::vector<App::DocumentObject*> shapes, xShapes;
    App::DocumentObject* partObj = nullptr;
    std::string faceName;
    auto selection = getSelection().getSelectionEx(nullptr, App::DocumentObject::getClassTypeId());
    for (auto& sel : selection) {
        bool is_linked = false;
        auto obj = sel.getObject();
        if (obj->isDerivedFrom<TechDraw::DrawPage>() || obj->isDerivedFrom<TechDraw::DrawView>()) {
            continue;
        }

        if (obj->isDerivedFrom<Spreadsheet::Sheet>()) {
            std::string SpreadName = obj->getNameInDocument();

            openCommand(QT_TRANSLATE_NOOP("Command", "Create spreadsheet view"));
            std::string FeatName = getUniqueObjectName("Sheet");
            doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewSpreadsheet', '%s')",
                FeatName.c_str());
            doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewSpreadsheet', 'Sheet', '%s')",
                FeatName.c_str(), FeatName.c_str());
            doCommand(Doc, "App.activeDocument().%s.Source = App.activeDocument().%s", FeatName.c_str(),
                SpreadName.c_str());
            doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
                FeatName.c_str());
            updateActive();
            commitCommand();
            viewCreated = true;
            continue;
        }
        else if (DrawGuiUtil::isArchSection(obj)) {
            std::string FeatName = getUniqueObjectName("BIM view");
            std::string SourceName = obj->getNameInDocument();
            openCommand(QT_TRANSLATE_NOOP("Command", "Create BIM view"));
            doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewArch', '%s')",
                FeatName.c_str());
            doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewArch', 'BIM view', '%s')",
                FeatName.c_str(), FeatName.c_str());
            doCommand(Doc, "App.activeDocument().%s.Source = App.activeDocument().%s", FeatName.c_str(),
                SourceName.c_str());
            doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
                FeatName.c_str());
            updateActive();
            commitCommand();
            viewCreated = true;
            continue;
        }

        if (obj->isDerivedFrom<App::LinkElement>()
            || obj->isDerivedFrom<App::LinkGroup>()
            || obj->isDerivedFrom<App::Link>()) {
            is_linked = true;
        }
        // If parent of the obj is a link to another document, we possibly need to treat non-link obj as linked, too
        // 1st, is obj in another document?
        if (obj->getDocument() != this->getDocument()) {
            std::set<App::DocumentObject*> parents = obj->getInListEx(true);
            for (auto& parent : parents) {
                // Only consider parents in the current document, i.e. possible links in this View's document
                if (parent->getDocument() != this->getDocument()) {
                    continue;
                }
                // 2nd, do we really have a link to obj?
                if (parent->isDerivedFrom<App::LinkElement>()
                    || parent->isDerivedFrom<App::LinkGroup>()
                    || parent->isDerivedFrom<App::Link>()) {
                    // We have a link chain from this document to obj, and obj is in another document -> it is an XLink target
                    is_linked = true;
                }
            }
        }
        if (is_linked) {
            xShapes.push_back(obj);
            continue;
        }
        //not a Link and not null.  assume to be drawable.  Undrawables will be
        // skipped later.
        shapes.push_back(obj);
        if (partObj) {
            continue;
        }
        //don't know if this works for an XLink
        for (auto& sub : sel.getSubNames()) {
            if (TechDraw::DrawUtil::getGeomTypeFromName(sub) == "Face") {
                faceName = sub;
                //
                partObj = obj;
                break;
            }
        }
    }

    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/TechDraw");
    if (shapes.empty() && xShapes.empty()) {
        if (!viewCreated) {
            // If nothing was selected, then we offer to insert SVG or Images files.
            bool dontShowAgain = hGrp->GetBool("DontShowInsertFileMessage", false);
            if (!dontShowAgain) {
                auto msgText = QObject::tr("If you want to insert a view from existing objects, please select them before invoking this tool. Without a selection, a file browser will open, to insert a SVG or image file.");
                QMessageBox msgBox(Gui::getMainWindow());
                msgBox.setText(msgText);
                auto dontShowMsg = QObject::tr("Do not show this message again");
                QCheckBox dontShowCheckBox(dontShowMsg, &msgBox);
                msgBox.setCheckBox(&dontShowCheckBox);
                QPushButton* okButton = msgBox.addButton(QMessageBox::Ok);

                msgBox.exec();

                if (msgBox.clickedButton() == okButton && dontShowCheckBox.isChecked()) {
                    // Save the preference to not show the message again
                    hGrp->SetBool("DontShowInsertFileMessage", true);
                }
            }

            QString filename = Gui::FileDialog::getOpenFileName(Gui::getMainWindow(),
                QObject::tr("Select a SVG or Image file to open"),
                Preferences::defaultSymbolDir(),
                QStringLiteral("%1 (*.svg *.svgz *.jpg *.jpeg *.png *.bmp);;%2 (*.*)")
                .arg(QObject::tr("SVG or Image files"), QObject::tr("All Files")));

            if (!filename.isEmpty()) {
                if (filename.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)
                    || filename.endsWith(QStringLiteral(".svgz"), Qt::CaseInsensitive)) {
                    std::string FeatName = getUniqueObjectName("Symbol");
                    filename = Base::Tools::escapeEncodeFilename(filename);
                    auto filespec = DU::cleanFilespecBackslash(filename.toStdString());
                    openCommand(QT_TRANSLATE_NOOP("Command", "Create Symbol"));
                    doCommand(Doc, "import codecs");
                    doCommand(Doc,
                              "f = codecs.open(\"%s\", 'r', encoding=\"utf-8\")",
                              filespec.c_str());
                    doCommand(Doc, "svg = f.read()");
                    doCommand(Doc, "f.close()");
                    doCommand(Doc,
                              "App.activeDocument().addObject('TechDraw::DrawViewSymbol', '%s')",
                              FeatName.c_str());
                    doCommand(
                        Doc,
                        "App.activeDocument().%s.translateLabel('DrawViewSymbol', 'Symbol', '%s')",
                        FeatName.c_str(),
                        FeatName.c_str());
                    doCommand(Doc, "App.activeDocument().%s.Symbol = svg", FeatName.c_str());
                    doCommand(Doc,
                              "App.activeDocument().%s.addView(App.activeDocument().%s)",
                              PageName.c_str(),
                              FeatName.c_str());
                }
                else {
                    std::string FeatName = getUniqueObjectName("Image");
                    filename = Base::Tools::escapeEncodeFilename(filename);
                    auto filespec = DU::cleanFilespecBackslash(filename.toStdString());
                    openCommand(QT_TRANSLATE_NOOP("Command", "Create Image"));
                    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewImage', '%s')", FeatName.c_str());
                    doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewImage', 'Image', '%s')",
                        FeatName.c_str(), FeatName.c_str());
                    doCommand(Doc, "App.activeDocument().%s.ImageFile = '%s'", FeatName.c_str(), filespec.c_str());
                    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(), FeatName.c_str());
                    updateActive();
                    commitCommand();
                }

                updateActive();
                commitCommand();
            }
        }
        return;
    }

    Gui::WaitCursor wc;
    openCommand(QT_TRANSLATE_NOOP("Command", "Create view"));
    std::string FeatName = getUniqueObjectName("View");
    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawProjGroupItem', '%s')",
        FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawProjGroupItem', 'View', '%s')",
        FeatName.c_str(), FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
        FeatName.c_str());

    App::DocumentObject* docObj = getDocument()->getObject(FeatName.c_str());
    auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(docObj);
    if (!dvp) {
        throw Base::TypeError("CmdTechDrawView DVP not found\n");
    }
    dvp->Source.setValues(shapes);
    dvp->XSource.setValues(xShapes);

    getDocument()->setStatus(App::Document::Status::SkipRecompute, true);
    auto dirs = viewDirection();
    doCommand(Doc,
              "App.activeDocument().%s.Direction = FreeCAD.Vector(%.12f, %.12f, %.12f)",
              FeatName.c_str(), dirs.first.x, dirs.first.y, dirs.first.z);
    doCommand(Doc,
              "App.activeDocument().%s.RotationVector = FreeCAD.Vector(%.12f, %.12f, %.12f)",
              FeatName.c_str(), dirs.second.x, dirs.second.y, dirs.second.z);
    doCommand(Doc,
              "App.activeDocument().%s.XDirection = FreeCAD.Vector(%.12f, %.12f, %.12f)",
              FeatName.c_str(), dirs.second.x, dirs.second.y, dirs.second.z);

    getDocument()->setStatus(App::Document::Status::SkipRecompute, false);
    doCommand(Doc, "App.activeDocument().%s.recompute()", FeatName.c_str());
    commitCommand();

    // create the rest of the desired views
    Gui::Control().showDialog(new TaskDlgProjGroup(dvp, true));
}

bool CmdTechDrawView::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_BrokenView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawBrokenView)

CmdTechDrawBrokenView::CmdTechDrawBrokenView()
  : Command("TechDraw_BrokenView")
{
    sAppModule      = "TechDraw";
    sGroup          = QT_TR_NOOP("TechDraw");
    sMenuText       = QT_TR_NOOP("Insert Broken View");
    sToolTipText    = QT_TR_NOOP("Insert Broken View");
    sWhatsThis      = "TechDraw_BrokenView";
    sStatusTip      = sToolTipText;
    sPixmap         = "actions/TechDraw_BrokenView";
}

void CmdTechDrawBrokenView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    // get shape objects from a base view
    std::vector<App::DocumentObject*> shapesFromBase;
    std::vector<App::DocumentObject*> xShapesFromBase;
    std::vector<App::DocumentObject*> baseViews =
        getSelection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId());
    TechDraw::DrawViewPart* dvp{nullptr};
    if (!baseViews.empty()) {
        dvp = static_cast<TechDraw::DrawViewPart*>(*baseViews.begin());
        shapesFromBase = dvp->Source.getValues();
        xShapesFromBase = dvp->XSource.getValues();
    }

    auto doc = getDocument();
    if (dvp) {
        doc = dvp->getDocument();
    }

    // get the shape objects from the selection
    std::vector<App::DocumentObject*> shapes;
    std::vector<App::DocumentObject*> xShapes;
    App::DocumentObject* faceObj = nullptr;
    std::string faceName;
    getSelectedShapes(this, shapes, xShapes, faceObj, faceName);

    // we need either a base view (dvp) or some shape objects in the selection
    if (!dvp && (shapes.empty() && xShapes.empty())) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Empty selection"),
            QObject::tr("Please select objects to break or a base view and break definition objects."));
        return;
    }

    shapes.insert(shapes.end(), shapesFromBase.begin(), shapesFromBase.end());
    shapes.insert(xShapes.end(), xShapesFromBase.begin(), xShapesFromBase.end());


    // pick the Break objects out of the selected pile
    std::vector<Gui::SelectionObject> selection = getSelection().getSelectionEx(
        nullptr, App::DocumentObject::getClassTypeId(), Gui::ResolveMode::NoResolve);

    std::vector<App::DocumentObject*> breakObjects;
    for (auto& selObj : selection) {
        auto temp = selObj.getObject();
        // a sketch outside a body is returned as an independent object in the selection
        if (selObj.getSubNames().empty()) {
            if (DrawBrokenView::isBreakObject(*temp)) {
                breakObjects.push_back(selObj.getObject());
            }
            continue;
        }
        // a sketch inside a body is returned as body + subelement, so we have to search through
        // subnames to find it.  This may(?) apply to App::Part and Group also?
        auto subname = selObj.getSubNames().front();
        if (subname.back() == '.') {
            subname = subname.substr(0, subname.length() - 1);
            auto objects = doc->getObjects();
            for (auto& obj : objects) {
                std::string objname{obj->getNameInDocument()};
                if (subname == objname  &&
                    DrawBrokenView::isBreakObject(*obj)) {
                    breakObjects.push_back(obj);
                }
            }
        }
    }
    if (breakObjects.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("No Break objects found in this selection"));
        return;
    }

    // remove Break objects from shape pile
    shapes = DrawBrokenView::removeBreakObjects(breakObjects, shapes);
    xShapes = DrawBrokenView::removeBreakObjects(breakObjects, xShapes);
    if (shapes.empty() &&
        xShapes.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("No Shapes, Groups or Links in this selection"));
        return;
    }

    Gui::WaitCursor wc;
    openCommand(QT_TRANSLATE_NOOP("Command", "Create broken view"));
    getDocument()->setStatus(App::Document::Status::SkipRecompute, true);
    std::string FeatName = getUniqueObjectName("BrokenView");
    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawBrokenView','%s')", FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(), FeatName.c_str());

    App::DocumentObject* docObj = getDocument()->getObject(FeatName.c_str());
    TechDraw::DrawBrokenView* dbv = dynamic_cast<TechDraw::DrawBrokenView*>(docObj);
    if (!dbv) {
        throw Base::TypeError("CmdTechDrawBrokenView DBV not found\n");
    }
    dbv->Source.setValues(shapes);
    dbv->XSource.setValues(xShapes);
    dbv->Breaks.setValues(breakObjects);

    //set projection direction from selected Face
    std::pair<Base::Vector3d, Base::Vector3d> dirs;
    if (faceName.size()) {
        dirs = DrawGuiUtil::getProjDirFromFace(faceObj, faceName);
    }
    else {
        dirs = DrawGuiUtil::get3DDirAndRot();
    }

    Base::Vector3d projDir = dirs.first;
    doCommand(Doc, "App.activeDocument().%s.Direction = FreeCAD.Vector(%.6f,%.6f,%.6f)",
              FeatName.c_str(), projDir.x, projDir.y, projDir.z);
    doCommand(Doc, "App.activeDocument().%s.XDirection = FreeCAD.Vector(%.6f,%.6f,%.6f)",
                  FeatName.c_str(), dirs.second.x, dirs.second.y, dirs.second.z);
    getDocument()->setStatus(App::Document::Status::SkipRecompute, true);

    commitCommand();

    dbv->recomputeFeature();
}

bool CmdTechDrawBrokenView::isActive(void)
{
    return DrawGuiUtil::needPage(this);
}


//===========================================================================
// TechDraw_ActiveView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawActiveView)

CmdTechDrawActiveView::CmdTechDrawActiveView() : Command("TechDraw_ActiveView")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Active View");
    sToolTipText = "Insert an image of the active 3D model in current page.\n"
               "If multiple 3D models are active, a selection dialog will be shown.";
    sWhatsThis = "TechDraw_ActiveView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ActiveView";
}

void CmdTechDrawActiveView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this, true);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();
    Gui::Control().showDialog(new TaskDlgActiveView(page));
}

bool CmdTechDrawActiveView::isActive() { return DrawGuiUtil::needPage(this, true); }

//===========================================================================
// TechDraw_SectionGroup
//===========================================================================

DEF_STD_CMD_ACL(CmdTechDrawSectionGroup)

CmdTechDrawSectionGroup::CmdTechDrawSectionGroup() : Command("TechDraw_SectionGroup")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert a simple or complex Section View");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_SectionGroup";
    sStatusTip = sToolTipText;
}

void CmdTechDrawSectionGroup::activated(int iMsg)
{
    //    Base::Console().message("CMD::SectionGrp - activated(%d)\n", iMsg);
    Gui::TaskView::TaskDialog* dlg = Gui::Control().activeDialog();
    if (dlg) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Task In Progress"),
                             QObject::tr("Close active task dialog and try again."));
        return;
    }

    Gui::ActionGroup* pcAction = qobject_cast<Gui::ActionGroup*>(_pcAction);
    pcAction->setIcon(pcAction->actions().at(iMsg)->icon());
    switch (iMsg) {
        case 0:
            execSimpleSection(this);
            break;
        case 1:
            execComplexSection(this);
            break;
        default:
            Base::Console().message("CMD::SectionGrp - invalid iMsg: %d\n", iMsg);
    };
}

Gui::Action* CmdTechDrawSectionGroup::createAction()
{
    Gui::ActionGroup* pcAction = new Gui::ActionGroup(this, Gui::getMainWindow());
    pcAction->setDropDownMenu(true);
    applyCommandData(this->className(), pcAction);

    QAction* p1 = pcAction->addAction(QString());
    p1->setIcon(Gui::BitmapFactory().iconFromTheme("actions/TechDraw_SectionView"));
    p1->setObjectName(QStringLiteral("TechDraw_SectionView"));
    p1->setWhatsThis(QStringLiteral("TechDraw_SectionView"));
    QAction* p2 = pcAction->addAction(QString());
    p2->setIcon(Gui::BitmapFactory().iconFromTheme("actions/TechDraw_ComplexSection"));
    p2->setObjectName(QStringLiteral("TechDraw_ComplexSection"));
    p2->setWhatsThis(QStringLiteral("TechDraw_ComplexSection"));

    _pcAction = pcAction;
    languageChange();

    pcAction->setIcon(p1->icon());
    int defaultId = 0;
    pcAction->setProperty("defaultAction", QVariant(defaultId));

    return pcAction;
}

void CmdTechDrawSectionGroup::languageChange()
{
    Command::languageChange();

    if (!_pcAction)
        return;
    Gui::ActionGroup* pcAction = qobject_cast<Gui::ActionGroup*>(_pcAction);
    QList<QAction*> a = pcAction->actions();

    QAction* arc1 = a[0];
    arc1->setText(QApplication::translate("CmdTechDrawSectionGroup", "Section View"));
    arc1->setToolTip(QApplication::translate("TechDraw_SectionView", "Insert simple Section View"));
    arc1->setStatusTip(arc1->toolTip());
    QAction* arc2 = a[1];
    arc2->setText(QApplication::translate("CmdTechDrawSectionGroup", "Complex Section"));
    arc2->setToolTip(
        QApplication::translate("TechDraw_ComplexSection", "Insert complex Section View"));
    arc2->setStatusTip(arc2->toolTip());
}

bool CmdTechDrawSectionGroup::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveView = DrawGuiUtil::needView(this, false);
    return (havePage && haveView);
}

//===========================================================================
// TechDraw_SectionView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawSectionView)

CmdTechDrawSectionView::CmdTechDrawSectionView() : Command("TechDraw_SectionView")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Section View");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_SectionView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_SectionView";
}

void CmdTechDrawSectionView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::TaskView::TaskDialog* dlg = Gui::Control().activeDialog();
    if (dlg) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Task In Progress"),
                             QObject::tr("Close active task dialog and try again."));
        return;
    }

    execSimpleSection(this);
}

bool CmdTechDrawSectionView::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveView = DrawGuiUtil::needView(this);
    bool taskInProgress = false;
    if (havePage) {
        taskInProgress = Gui::Control().activeDialog();
    }
    return (havePage && haveView && !taskInProgress);
}

void execSimpleSection(Gui::Command* cmd)
{
    std::vector<App::DocumentObject*> baseObj =
        cmd->getSelection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId());
    if (baseObj.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select at least 1 DrawViewPart object as Base."));
        return;
    }

    TechDraw::DrawPage* page = DrawGuiUtil::findPage(cmd);
    if (!page) {
        return;
    }

    TechDraw::DrawViewPart* dvp = static_cast<TechDraw::DrawViewPart*>(*baseObj.begin());
    Gui::Control().showDialog(new TaskDlgSectionView(dvp));

    cmd->updateActive();//ok here since dialog doesn't call doc.recompute()
    cmd->commitCommand();
}

//===========================================================================
// TechDraw_ComplexSection
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawComplexSection)

CmdTechDrawComplexSection::CmdTechDrawComplexSection() : Command("TechDraw_ComplexSection")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Complex Section View");
    sToolTipText = QT_TR_NOOP("Insert a Complex Section View");
    sWhatsThis = "TechDraw_ComplexSection";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ComplexSection";
}

void CmdTechDrawComplexSection::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::TaskView::TaskDialog* dlg = Gui::Control().activeDialog();
    if (dlg) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Task In Progress"),
                             QObject::tr("Close active task dialog and try again."));
        return;
    }

    execComplexSection(this);
}

bool CmdTechDrawComplexSection::isActive() { return DrawGuiUtil::needPage(this); }

//Complex Sections can be created without a baseView, so the gathering of input
//for the dialog is more involved that simple section
void execComplexSection(Gui::Command* cmd)
{
    TechDraw::DrawViewPart* baseView(nullptr);
    std::vector<App::DocumentObject*> shapes;
    std::vector<App::DocumentObject*> xShapes;
    App::DocumentObject* profileObject(nullptr);
    std::vector<std::string> profileSubs;
    Gui::ResolveMode resolve = Gui::ResolveMode::OldStyleElement;
    bool single = false;
    auto selection = cmd->getSelection().getSelectionEx(
        nullptr, App::DocumentObject::getClassTypeId(), resolve, single);
    for (auto& sel : selection) {
        bool is_linked = false;
        auto obj = sel.getObject();
        if (obj->isDerivedFrom<TechDraw::DrawPage>()) {
            continue;
        }
        if (obj->isDerivedFrom<TechDraw::DrawViewPart>()) {
            //use the dvp's Sources as sources for this ComplexSection &
            //check the subelement(s) to see if they can be used as a profile
            baseView = static_cast<TechDraw::DrawViewPart*>(obj);
            if (!sel.getSubNames().empty()) {
                //need to add profile subs as parameter
                profileObject = baseView;
                profileSubs = sel.getSubNames();
            }
            continue;
        }
        if (obj->isDerivedFrom<App::LinkElement>()
            || obj->isDerivedFrom<App::LinkGroup>()
            || obj->isDerivedFrom<App::Link>()) {
            is_linked = true;
        }
        // If parent of the obj is a link to another document, we possibly need to treat non-link obj as linked, too
        // 1st, is obj in another document?
        if (obj->getDocument() != cmd->getDocument()) {
            std::set<App::DocumentObject*> parents = obj->getInListEx(true);
            for (auto& parent : parents) {
                // Only consider parents in the current document, i.e. possible links in this View's document
                if (parent->getDocument() != cmd->getDocument()) {
                    continue;
                }
                // 2nd, do we really have a link to obj?
                if (parent->isDerivedFrom<App::LinkElement>()
                    || parent->isDerivedFrom<App::LinkGroup>()
                    || parent->isDerivedFrom<App::Link>()) {
                    // We have a link chain from this document to obj, and obj is in another document -> it is an XLink target
                    is_linked = true;
                }
            }
        }
        if (is_linked) {
            xShapes.push_back(obj);
            continue;
        }
        //not a Link and not null.  assume to be drawable.  Undrawables will be
        // skipped later.
        if (TechDraw::DrawComplexSection::isProfileObject(obj)) {
            profileObject = obj;
        }
        else {
            shapes.push_back(obj);
        }
    }

    if (!baseView) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("I do not know what base view to use."));
        return;
    }

    if (shapes.empty() && xShapes.empty() && !baseView) {
        QMessageBox::warning(
            Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("No Base View, Shapes, Groups or Links in this selection"));
        return;
    }
    if (!profileObject) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("No profile object found in selection"));
        return;
    }

    TechDraw::DrawPage* page = DrawGuiUtil::findPage(cmd);
    if (!page) {
        return;
    }

    Gui::Control().showDialog(
        new TaskDlgComplexSection(page, baseView, shapes, xShapes, profileObject, profileSubs));
}

//===========================================================================
// TechDraw_DetailView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawDetailView)

CmdTechDrawDetailView::CmdTechDrawDetailView() : Command("TechDraw_DetailView")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Detail View");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_DetailView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_DetailView";
}

void CmdTechDrawDetailView::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    std::vector<App::DocumentObject*> baseObj =
        getSelection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId());
    if (baseObj.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select at least 1 DrawViewPart object as Base."));
        return;
    }
    TechDraw::DrawViewPart* dvp = static_cast<TechDraw::DrawViewPart*>(*(baseObj.begin()));

    Gui::Control().showDialog(new TaskDlgDetail(dvp));
}

bool CmdTechDrawDetailView::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveView = DrawGuiUtil::needView(this);
    bool taskInProgress = false;
    if (havePage) {
        taskInProgress = Gui::Control().activeDialog();
    }
    return (havePage && haveView && !taskInProgress);
}

//===========================================================================
// TechDraw_ProjectionGroup
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawProjectionGroup)

CmdTechDrawProjectionGroup::CmdTechDrawProjectionGroup() : Command("TechDraw_ProjectionGroup")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Projection Group");
    sToolTipText = QT_TR_NOOP("Insert multiple linked views of drawable object(s)");
    sWhatsThis = "TechDraw_ProjectionGroup";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ProjectionGroup";
}

void CmdTechDrawProjectionGroup::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    //set projection direction from selected Face
    //use first object with a face selected
    std::vector<App::DocumentObject*> shapes;
    std::vector<App::DocumentObject*> xShapes;
    App::DocumentObject* partObj = nullptr;
    std::string faceName;
    Gui::ResolveMode resolve = Gui::ResolveMode::OldStyleElement;
    bool single = false;
    auto selection = getSelection().getSelectionEx(nullptr, App::DocumentObject::getClassTypeId(),
                                                   resolve, single);
    for (auto& sel : selection) {
        bool is_linked = false;
        auto obj = sel.getObject();
        if (obj->isDerivedFrom<TechDraw::DrawPage>()) {
            continue;
        }
        if (obj->isDerivedFrom<App::LinkElement>()
            || obj->isDerivedFrom<App::LinkGroup>()
            || obj->isDerivedFrom<App::Link>()) {
            is_linked = true;
        }
        // If parent of the obj is a link to another document, we possibly need to treat non-link obj as linked, too
        // 1st, is obj in another document?
        if (obj->getDocument() != this->getDocument()) {
            std::set<App::DocumentObject*> parents = obj->getInListEx(true);
            for (auto& parent : parents) {
                // Only consider parents in the current document, i.e. possible links in this View's document
                if (parent->getDocument() != this->getDocument()) {
                    continue;
                }
                // 2nd, do we really have a link to obj?
                if (parent->isDerivedFrom<App::LinkElement>()
                    || parent->isDerivedFrom<App::LinkGroup>()
                    || parent->isDerivedFrom<App::Link>()) {
                    // We have a link chain from this document to obj, and obj is in another document -> it is an XLink target
                    is_linked = true;
                }
            }
        }
        if (is_linked) {
            xShapes.push_back(obj);
            continue;
        }
        //not a Link and not null.  assume to be drawable.  Undrawables will be
        // skipped later.
        shapes.push_back(obj);
        if (partObj) {
            continue;
        }
        for (auto& sub : sel.getSubNames()) {
            if (TechDraw::DrawUtil::getGeomTypeFromName(sub) == "Face") {
                faceName = sub;
                partObj = obj;
                break;
            }
        }
    }
    if (shapes.empty() && xShapes.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("No Shapes, Groups or Links in this selection"));
        return;
    }

    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    Base::Vector3d projDir;
    Gui::WaitCursor wc;

    openCommand(QT_TRANSLATE_NOOP("Command", "Create Projection Group"));

    std::string multiViewName = getUniqueObjectName("ProjGroup");
    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawProjGroup', '%s')",
              multiViewName.c_str());
    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
              multiViewName.c_str());

    App::DocumentObject* docObj = getDocument()->getObject(multiViewName.c_str());
    auto multiView(static_cast<TechDraw::DrawProjGroup*>(docObj));
    multiView->Source.setValues(shapes);
    multiView->XSource.setValues(xShapes);
    doCommand(Doc, "App.activeDocument().%s.addProjection('Front')", multiViewName.c_str());

    std::pair<Base::Vector3d, Base::Vector3d> dirs = !faceName.empty() ?
        DrawGuiUtil::getProjDirFromFace(partObj, faceName)
        : DrawGuiUtil::get3DDirAndRot();

    getDocument()->setStatus(App::Document::Status::SkipRecompute, true);
    doCommand(Doc,
        "App.activeDocument().%s.Anchor.Direction = FreeCAD.Vector(%.12f, %.12f, %.12f)",
        multiViewName.c_str(), dirs.first.x, dirs.first.y, dirs.first.z);
    doCommand(Doc,
        "App.activeDocument().%s.Anchor.RotationVector = FreeCAD.Vector(%.12f, %.12f, %.12f)",
        multiViewName.c_str(), dirs.second.x, dirs.second.y, dirs.second.z);
    doCommand(Doc,
        "App.activeDocument().%s.Anchor.XDirection = FreeCAD.Vector(%.12f, %.12f, %.12f)",
        multiViewName.c_str(), dirs.second.x, dirs.second.y, dirs.second.z);
    getDocument()->setStatus(App::Document::Status::SkipRecompute, false);

    doCommand(Doc, "App.activeDocument().%s.Anchor.recompute()", multiViewName.c_str());
    commitCommand();
    updateActive();

    // create the rest of the desired views
    Gui::Control().showDialog(new TaskDlgProjGroup(multiView, true));
}

bool CmdTechDrawProjectionGroup::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool taskInProgress = false;
    if (havePage) {
        taskInProgress = Gui::Control().activeDialog();
    }
    return (havePage && !taskInProgress);
}

//! common checks of Selection for Dimension commands
//non-empty selection, no more than maxObjs selected and at least 1 DrawingPage exists
bool _checkSelectionBalloon(Gui::Command* cmd, unsigned maxObjs)
{
    std::vector<Gui::SelectionObject> selection = cmd->getSelection().getSelectionEx();
    if (selection.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Incorrect selection"),
                             QObject::tr("Select an object first"));
        return false;
    }

    const std::vector<std::string> SubNames = selection[0].getSubNames();
    if (SubNames.size() > maxObjs) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Incorrect selection"),
                             QObject::tr("Too many objects selected"));
        return false;
    }

    std::vector<App::DocumentObject*> pages =
        cmd->getDocument()->getObjectsOfType(TechDraw::DrawPage::getClassTypeId());
    if (pages.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Incorrect selection"),
                             QObject::tr("Create a page first."));
        return false;
    }
    return true;
}

bool _checkDrawViewPartBalloon(Gui::Command* cmd)
{
    std::vector<Gui::SelectionObject> selection = cmd->getSelection().getSelectionEx();
    auto objFeat(dynamic_cast<TechDraw::DrawViewPart*>(selection[0].getObject()));
    if (!objFeat) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Incorrect selection"),
                             QObject::tr("No View of a Part in selection."));
        return false;
    }
    return true;
}

bool _checkDirectPlacement(const QGIView* view, const std::vector<std::string>& subNames,
                           QPointF& placement)
{
    // Let's see, if we can help speed up the placement of the balloon:
    // As of now we support:
    //     Single selected vertex: place the balloon tip end here
    //     Single selected edge:   place the balloon tip at its midpoint (suggested placement for e.g. chamfer dimensions)
    //
    // Single selected faces are currently not supported, but maybe we could in this case use the center of mass?

    if (subNames.size() != 1) {
        // If nothing or more than one subjects are selected, let the user decide, where to place the balloon
        return false;
    }

    const QGIViewPart* viewPart = dynamic_cast<const QGIViewPart*>(view);
    if (!viewPart) {
        //not a view of a part, so no geometry to attach to
        return false;
    }

    std::string geoType = TechDraw::DrawUtil::getGeomTypeFromName(subNames[0]);
    if (geoType == "Vertex") {
        int index = TechDraw::DrawUtil::getIndexFromName(subNames[0]);
        TechDraw::VertexPtr vertex =
            static_cast<DrawViewPart*>(viewPart->getViewObject())->getProjVertexByIndex(index);
        if (vertex) {
            placement = viewPart->mapToScene(Rez::guiX(vertex->x()), Rez::guiX(vertex->y()));
            return true;
        }
    }
    else if (geoType == "Edge") {
        int index = TechDraw::DrawUtil::getIndexFromName(subNames[0]);
        TechDraw::BaseGeomPtr geo =
            static_cast<DrawViewPart*>(viewPart->getViewObject())->getGeomByIndex(index);
        if (geo) {
            Base::Vector3d midPoint(Rez::guiX(geo->getMidPoint()));
            placement = viewPart->mapToScene(midPoint.x, midPoint.y);
            return true;
        }
    }

    return false;
}

DEF_STD_CMD_A(CmdTechDrawBalloon)

CmdTechDrawBalloon::CmdTechDrawBalloon() : Command("TechDraw_Balloon")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Balloon Annotation");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_Balloon";
    sStatusTip = sToolTipText;
    sPixmap = "TechDraw_Balloon";
}

void CmdTechDrawBalloon::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    bool result = _checkSelectionBalloon(this, 1);
    if (!result) {
        return;
    }

    std::vector<Gui::SelectionObject> selection = getSelection().getSelectionEx();

    auto objFeat(dynamic_cast<TechDraw::DrawView*>(selection[0].getObject()));
    if (!objFeat) {
        return;
    }

    TechDraw::DrawPage* page = objFeat->findParentPage();
    std::string PageName = page->getNameInDocument();

    Gui::Document* guiDoc = Gui::Application::Instance->getDocument(page->getDocument());
    ViewProviderPage* pageVP = freecad_cast<ViewProviderPage*>(guiDoc->getViewProvider(page));
    ViewProviderDrawingView* viewVP =
        freecad_cast<ViewProviderDrawingView*>(guiDoc->getViewProvider(objFeat));

    if (pageVP && viewVP) {
        QGVPage* viewPage = pageVP->getQGVPage();
        QGSPage* scenePage = pageVP->getQGSPage();
        if (viewPage) {
            viewPage->startBalloonPlacing(objFeat);

            QGIView* view = dynamic_cast<QGIView*>(viewVP->getQView());
            QPointF placement;
            if (view && _checkDirectPlacement(view, selection[0].getSubNames(), placement)) {
                //this creates the balloon if something is already selected
                scenePage->createBalloon(placement, objFeat);
            }
        }
    }
}

bool CmdTechDrawBalloon::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveView = DrawGuiUtil::needView(this, false);
    bool taskInProgress = Gui::Control().activeDialog();
    return (havePage && haveView && !taskInProgress);
}

//===========================================================================
// TechDraw_ClipGroup
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawClipGroup)

CmdTechDrawClipGroup::CmdTechDrawClipGroup() : Command("TechDraw_ClipGroup")
{
    // setting the
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Clip Group");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_ClipGroup";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ClipGroup";
}

void CmdTechDrawClipGroup::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    std::string FeatName = getUniqueObjectName("Clip");
    openCommand(QT_TRANSLATE_NOOP("Command", "Create Clip"));
    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewClip', '%s')",
              FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
              FeatName.c_str());
    updateActive();
    commitCommand();
}

bool CmdTechDrawClipGroup::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_ClipGroupAdd
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawClipGroupAdd)

CmdTechDrawClipGroupAdd::CmdTechDrawClipGroupAdd() : Command("TechDraw_ClipGroupAdd")
{
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Add View to Clip Group");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_ClipGroupAdd";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ClipGroupAdd";
}

void CmdTechDrawClipGroupAdd::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    std::vector<Gui::SelectionObject> selection = getSelection().getSelectionEx();
    if (selection.size() != 2) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select one Clip group and one View."));
        return;
    }

    TechDraw::DrawViewClip* clip = nullptr;
    TechDraw::DrawView* view = nullptr;
    std::vector<Gui::SelectionObject>::iterator itSel = selection.begin();
    for (; itSel != selection.end(); itSel++) {
        if ((*itSel).getObject()->isDerivedFrom<TechDraw::DrawViewClip>()) {
            clip = static_cast<TechDraw::DrawViewClip*>((*itSel).getObject());
        }
        else if ((*itSel).getObject()->isDerivedFrom<TechDraw::DrawView>()) {
            view = static_cast<TechDraw::DrawView*>((*itSel).getObject());
        }
    }
    if (!view) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select exactly one View to add to group."));
        return;
    }
    if (!clip) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select exactly one Clip group."));
        return;
    }

    TechDraw::DrawPage* pageClip = clip->findParentPage();
    TechDraw::DrawPage* pageView = view->findParentPage();

    if (pageClip != pageView) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Clip and View must be from same Page."));
        return;
    }

    std::string PageName = pageClip->getNameInDocument();
    std::string ClipName = clip->getNameInDocument();
    std::string ViewName = view->getNameInDocument();

    openCommand(QT_TRANSLATE_NOOP("Command", "Add clip group"));
    doCommand(Doc, "App.activeDocument().%s.ViewObject.Visibility = False", ViewName.c_str());
    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", ClipName.c_str(),
              ViewName.c_str());
    doCommand(Doc, "App.activeDocument().%s.ViewObject.Visibility = True", ViewName.c_str());
    updateActive();
    commitCommand();
}

bool CmdTechDrawClipGroupAdd::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveClip = false;
    if (havePage) {
        auto drawClipType(TechDraw::DrawViewClip::getClassTypeId());
        auto selClips = getDocument()->getObjectsOfType(drawClipType);
        if (!selClips.empty()) {
            haveClip = true;
        }
    }
    return (havePage && haveClip);
}

//===========================================================================
// TechDraw_ClipGroupRemove
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawClipGroupRemove)

CmdTechDrawClipGroupRemove::CmdTechDrawClipGroupRemove() : Command("TechDraw_ClipGroupRemove")
{
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Remove View from Clip Group");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_ClipGroupRemove";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ClipGroupRemove";
}

void CmdTechDrawClipGroupRemove::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto dObj(getSelection().getObjectsOfType(TechDraw::DrawView::getClassTypeId()));
    if (dObj.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select exactly one View to remove from Group."));
        return;
    }

    auto view(static_cast<TechDraw::DrawView*>(dObj.front()));

    TechDraw::DrawPage* page = view->findParentPage();
    const std::vector<App::DocumentObject*> pViews = page->getViews();
    TechDraw::DrawViewClip* clip(nullptr);
    for (auto& v : pViews) {
        clip = dynamic_cast<TechDraw::DrawViewClip*>(v);
        if (clip && clip->isViewInClip(view)) {
            break;
        }
        clip = nullptr;
    }

    if (!clip) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("View does not belong to a Clip"));
        return;
    }

    std::string ClipName = clip->getNameInDocument();
    std::string ViewName = view->getNameInDocument();

    openCommand(QT_TRANSLATE_NOOP("Command", "Remove clip group"));
    doCommand(Doc, "App.activeDocument().%s.ViewObject.Visibility = False", ViewName.c_str());
    doCommand(Doc, "App.activeDocument().%s.removeView(App.activeDocument().%s)", ClipName.c_str(),
              ViewName.c_str());
    doCommand(Doc, "App.activeDocument().%s.ViewObject.Visibility = True", ViewName.c_str());
    updateActive();
    commitCommand();
}

bool CmdTechDrawClipGroupRemove::isActive()
{
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveClip = false;
    if (havePage) {
        auto drawClipType(TechDraw::DrawViewClip::getClassTypeId());
        auto selClips = getDocument()->getObjectsOfType(drawClipType);
        if (!selClips.empty()) {
            haveClip = true;
        }
    }
    return (havePage && haveClip);
}


//===========================================================================
// TechDraw_Symbol
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawSymbol)

CmdTechDrawSymbol::CmdTechDrawSymbol() : Command("TechDraw_Symbol")
{
    // setting the Gui eye-candy
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert SVG Symbol");
    sToolTipText = QT_TR_NOOP("Insert symbol from an SVG file");
    sWhatsThis = "TechDraw_Symbol";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_Symbol";
}

void CmdTechDrawSymbol::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    // Reading an image
    QString filename = Gui::FileDialog::getOpenFileName(
        Gui::getMainWindow(), QObject::tr("Choose an SVG file to open"),
        Preferences::defaultSymbolDir(),
        QStringLiteral("%1 (*.svg *.svgz);;%2 (*.*)")
            .arg(QObject::tr("Scalable Vector Graphic"), QObject::tr("All Files")));

    if (!filename.isEmpty()) {
        std::string FeatName = getUniqueObjectName("Symbol");
        filename = Base::Tools::escapeEncodeFilename(filename);
        auto filespec = DU::cleanFilespecBackslash(filename.toStdString());
        openCommand(QT_TRANSLATE_NOOP("Command", "Create Symbol"));
        doCommand(Doc, "import codecs");
        doCommand(Doc, "f = codecs.open(\"%s\", 'r', encoding=\"utf-8\")",  filespec.c_str());
        doCommand(Doc, "svg = f.read()");
        doCommand(Doc, "f.close()");
        doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewSymbol', '%s')",
                  FeatName.c_str());
        doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewSymbol', 'Symbol', '%s')",
              FeatName.c_str(), FeatName.c_str());
        doCommand(Doc, "App.activeDocument().%s.Symbol = svg", FeatName.c_str());

        auto baseView = CommandHelpers::firstViewInSelection(this);
        if (baseView) {
            auto baseName = baseView->getNameInDocument();
            doCommand(Doc, "App.activeDocument().%s.Owner = App.activeDocument().%s",
                      FeatName.c_str(), baseName);
        }

        doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
                  FeatName.c_str());

        updateActive();
        commitCommand();
    }
}

bool CmdTechDrawSymbol::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_DraftView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawDraftView)

CmdTechDrawDraftView::CmdTechDrawDraftView() : Command("TechDraw_DraftView")
{
    // setting the Gui eye-candy
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Draft Workbench Object");
    sToolTipText = QT_TR_NOOP("Insert a View of a Draft Workbench object");
    sWhatsThis = "TechDraw_NewDraft";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_DraftView";
}

void CmdTechDrawDraftView::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    std::vector<App::DocumentObject*> objects =
        getSelection().getObjectsOfType(App::DocumentObject::getClassTypeId());

    if (objects.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select at least one object."));
        return;
    }

    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    std::pair<Base::Vector3d, Base::Vector3d> dirs = DrawGuiUtil::get3DDirAndRot();
    for (auto* obj : objects) {
         if (obj->isDerivedFrom<TechDraw::DrawPage>() ||
             obj->isDerivedFrom<TechDraw::DrawView>()) {
            // skip over TechDraw objects as they are not valid subjects for a DraftView
            continue;
        }
        std::string FeatName = getUniqueObjectName("DraftView");
        std::string SourceName = obj->getNameInDocument();
        openCommand(QT_TRANSLATE_NOOP("Command", "Create DraftView"));
        doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewDraft', '%s')",
                  FeatName.c_str());
        doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewDraft', 'DraftView', '%s')",
              FeatName.c_str(), FeatName.c_str());
        doCommand(Doc, "App.activeDocument().%s.Source = App.activeDocument().%s", FeatName.c_str(),
                  SourceName.c_str());
        doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
                  FeatName.c_str());
        doCommand(Doc, "App.activeDocument().%s.Direction = FreeCAD.Vector(%.12f, %.12f, %.12f)",
                  FeatName.c_str(), dirs.first.x, dirs.first.y, dirs.first.z);
        updateActive();
        commitCommand();
    }
}

bool CmdTechDrawDraftView::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_ArchView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawArchView)

CmdTechDrawArchView::CmdTechDrawArchView() : Command("TechDraw_ArchView")
{
    // setting the Gui eye-candy
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert BIM Workbench Object");
    sToolTipText = QT_TR_NOOP("Insert a View of a BIM Workbench section plane");
    sWhatsThis = "TechDraw_NewArch";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ArchView";
}

void CmdTechDrawArchView::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    const std::vector<App::DocumentObject*> objects =
        getSelection().getObjectsOfType(App::DocumentObject::getClassTypeId());
    App::DocumentObject* archObject = nullptr;
    int archCount = 0;
    for (auto& obj : objects) {
        if (obj->isDerivedFrom<TechDraw::DrawPage>() ||
            obj->isDerivedFrom<TechDraw::DrawView>()) {
            // skip over TechDraw objects as they are not valid subjects for a ArchView
            continue;
        }
        if (DrawGuiUtil::isArchSection(obj)) {
            archCount++;
            archObject = obj;
        }
    }
    if (archCount > 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Please select only 1 BIM section plane."));
        return;
    }

    if (!archObject) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("No BIM section plane in selection."));
        return;
    }

    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    std::string FeatName = getUniqueObjectName("BIM view");
    std::string SourceName = archObject->getNameInDocument();
    openCommand(QT_TRANSLATE_NOOP("Command", "Create BIM view"));
    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewArch', '%s')",
              FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewArch', 'BIM view', '%s')",
              FeatName.c_str(), FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.Source = App.activeDocument().%s", FeatName.c_str(),
              SourceName.c_str());
    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
              FeatName.c_str());
    doCommand(Doc, "if App.activeDocument().%s.Scale: App.activeDocument().%s.Scale = App.activeDocument().%s.Scale",
        PageName.c_str(), FeatName.c_str(), PageName.c_str());
    updateActive();
    commitCommand();
}

bool CmdTechDrawArchView::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_SpreadsheetView
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawSpreadsheetView)

CmdTechDrawSpreadsheetView::CmdTechDrawSpreadsheetView() : Command("TechDraw_SpreadsheetView")
{
    // setting the
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Insert Spreadsheet View");
    sToolTipText = QT_TR_NOOP("Insert View to a spreadsheet");
    sWhatsThis = "TechDraw_SpreadsheetView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_SpreadsheetView";
}

void CmdTechDrawSpreadsheetView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    const std::vector<App::DocumentObject*> spreads =
        getSelection().getObjectsOfType(Spreadsheet::Sheet::getClassTypeId());
    if (spreads.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                             QObject::tr("Select exactly one Spreadsheet object."));
        return;
    }
    std::string SpreadName = spreads.front()->getNameInDocument();

    openCommand(QT_TRANSLATE_NOOP("Command", "Create spreadsheet view"));
    std::string FeatName = getUniqueObjectName("Sheet");
    doCommand(Doc, "App.activeDocument().addObject('TechDraw::DrawViewSpreadsheet', '%s')",
              FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.translateLabel('DrawViewSpreadsheet', 'Sheet', '%s')",
              FeatName.c_str(), FeatName.c_str());
    doCommand(Doc, "App.activeDocument().%s.Source = App.activeDocument().%s", FeatName.c_str(),
              SpreadName.c_str());

    // look for an owner view in the selection
    auto baseView = CommandHelpers::firstViewInSelection(this);
    if (baseView) {
        auto baseName = baseView->getNameInDocument();
        doCommand(Doc, "App.activeDocument().%s.Owner = App.activeDocument().%s",
                  FeatName.c_str(), baseName);
    }

    doCommand(Doc, "App.activeDocument().%s.addView(App.activeDocument().%s)", PageName.c_str(),
              FeatName.c_str());
    updateActive();
    commitCommand();
}

bool CmdTechDrawSpreadsheetView::isActive()
{
    //need a Page and a SpreadSheet::Sheet
    bool havePage = DrawGuiUtil::needPage(this);
    bool haveSheet = false;
    if (havePage) {
        auto spreadSheetType(Spreadsheet::Sheet::getClassTypeId());
        auto selSheets = getDocument()->getObjectsOfType(spreadSheetType);
        if (!selSheets.empty()) {
            haveSheet = true;
        }
    }
    return (havePage && haveSheet);
}


//===========================================================================
// TechDraw_ExportPageSVG
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawExportPageSVG)

CmdTechDrawExportPageSVG::CmdTechDrawExportPageSVG() : Command("TechDraw_ExportPageSVG")
{
    sGroup = QT_TR_NOOP("File");
    sMenuText = QT_TR_NOOP("Export Page as SVG");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_ExportPageSVG";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExportPageSVG";
}

void CmdTechDrawExportPageSVG::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }
    std::string PageName = page->getNameInDocument();

    Gui::Document* activeGui = Gui::Application::Instance->getDocument(page->getDocument());
    Gui::ViewProvider* vp = activeGui->getViewProvider(page);
    ViewProviderPage* vpPage = freecad_cast<ViewProviderPage*>(vp);

    if (vpPage) {
        vpPage->show();  // make sure a mdi will be available
        vpPage->getMDIViewPage()->saveSVG();
    }
    else {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("No Drawing Page"),
                             QObject::tr("FreeCAD could not find a page to export"));
        return;
    }
}

bool CmdTechDrawExportPageSVG::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_ExportPageDXF
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawExportPageDXF)

CmdTechDrawExportPageDXF::CmdTechDrawExportPageDXF() : Command("TechDraw_ExportPageDXF")
{
    sGroup = QT_TR_NOOP("File");
    sMenuText = QT_TR_NOOP("Export Page as DXF");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_ExportPageDXF";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExportPageDXF";
}

void CmdTechDrawExportPageDXF::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDraw::DrawPage* page = DrawGuiUtil::findPage(this);
    if (!page) {
        return;
    }

    std::vector<App::DocumentObject*> views = page->getViews();
    for (auto& v : views) {
        if (v->isDerivedFrom<TechDraw::DrawViewArch>()) {
            QMessageBox::StandardButton rc = QMessageBox::question(
                Gui::getMainWindow(), QObject::tr("Can not export selection"),
                QObject::tr("Page contains DrawViewArch which will not be exported. Continue?"),
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));
            if (rc == QMessageBox::No) {
                return;
            }
            else {
                break;
            }
        }
    }

    //WF? allow more than one TD Page per Dxf file??  1 TD page = 1 DXF file = 1 drawing?
    QString defaultDir;
    QString fileName = Gui::FileDialog::getSaveFileName(
        Gui::getMainWindow(), QString::fromUtf8(QT_TR_NOOP("Save DXF file")), defaultDir,
        QStringLiteral("DXF (*.dxf)"));

    if (fileName.isEmpty()) {
        return;
    }

    std::string PageName = page->getNameInDocument();
    openCommand(QT_TRANSLATE_NOOP("Command", "Save page to DXF"));
    doCommand(Doc, "import TechDraw");
    fileName = Base::Tools::escapeEncodeFilename(fileName);
    auto filespec = DU::cleanFilespecBackslash(fileName.toStdString());
    doCommand(Doc, "TechDraw.writeDXFPage(App.activeDocument().%s, u\"%s\")", PageName.c_str(),
              filespec.c_str());
    commitCommand();
}


bool CmdTechDrawExportPageDXF::isActive() { return DrawGuiUtil::needPage(this); }

//===========================================================================
// TechDraw_ProjectShape
//===========================================================================

DEF_STD_CMD_A(CmdTechDrawProjectShape)

CmdTechDrawProjectShape::CmdTechDrawProjectShape() : Command("TechDraw_ProjectShape")
{
    sAppModule = "TechDraw";
    sGroup = QT_TR_NOOP("TechDraw");
    sMenuText = QT_TR_NOOP("Project shape...");
    sToolTipText = sMenuText;
    sWhatsThis = "TechDraw_ProjectShape";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ProjectShape";
}

void CmdTechDrawProjectShape::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::TaskView::TaskDialog* dlg = Gui::Control().activeDialog();
    if (!dlg) {
        Gui::Control().showDialog(new TaskDlgProjection());
    }
}

bool CmdTechDrawProjectShape::isActive() { return true; }

void CreateTechDrawCommands()
{
    Gui::CommandManager& rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdTechDrawPageDefault());
    rcCmdMgr.addCommand(new CmdTechDrawPageTemplate());
    rcCmdMgr.addCommand(new CmdTechDrawRedrawPage());
    rcCmdMgr.addCommand(new CmdTechDrawPrintAll());
    rcCmdMgr.addCommand(new CmdTechDrawView());
    rcCmdMgr.addCommand(new CmdTechDrawActiveView());
    rcCmdMgr.addCommand(new CmdTechDrawSectionGroup());
    rcCmdMgr.addCommand(new CmdTechDrawSectionView());
    rcCmdMgr.addCommand(new CmdTechDrawComplexSection());
    rcCmdMgr.addCommand(new CmdTechDrawDetailView());
    rcCmdMgr.addCommand(new CmdTechDrawProjectionGroup());
    rcCmdMgr.addCommand(new CmdTechDrawClipGroup());
    rcCmdMgr.addCommand(new CmdTechDrawClipGroupAdd());
    rcCmdMgr.addCommand(new CmdTechDrawClipGroupRemove());
    rcCmdMgr.addCommand(new CmdTechDrawSymbol());
    rcCmdMgr.addCommand(new CmdTechDrawExportPageSVG());
    rcCmdMgr.addCommand(new CmdTechDrawExportPageDXF());
    rcCmdMgr.addCommand(new CmdTechDrawDraftView());
    rcCmdMgr.addCommand(new CmdTechDrawArchView());
    rcCmdMgr.addCommand(new CmdTechDrawSpreadsheetView());
    rcCmdMgr.addCommand(new CmdTechDrawBalloon());
    rcCmdMgr.addCommand(new CmdTechDrawProjectShape());
    rcCmdMgr.addCommand(new CmdTechDrawBrokenView());

}

//****************************************


//! extract the selected shapes and xShapes and determine if a face has been
//! selected to define the projection direction
void getSelectedShapes(Gui::Command* cmd,
                      std::vector<App::DocumentObject*>& shapes,
                      std::vector<App::DocumentObject*>& xShapes,
                      App::DocumentObject* faceObj,
                      std::string& faceName)
{
    Gui::ResolveMode resolve = Gui::ResolveMode::OldStyleElement;
    bool single = false;
    auto selection = cmd->getSelection().getSelectionEx(nullptr, App::DocumentObject::getClassTypeId(),
                                                   resolve, single);
    for (auto& sel : selection) {
        bool is_linked = false;
        auto obj = sel.getObject();
        if (obj->isDerivedFrom<TechDraw::DrawPage>()) {
            continue;
        }
        if (obj->isDerivedFrom<App::LinkElement>()
            || obj->isDerivedFrom<App::LinkGroup>()
            || obj->isDerivedFrom<App::Link>()) {
            is_linked = true;
        }
        // If parent of the obj is a link to another document, we possibly need to treat non-link obj as linked, too
        // 1st, is obj in another document?
        if (obj->getDocument() != cmd->getDocument()) {
            std::set<App::DocumentObject*> parents = obj->getInListEx(true);
            for (auto& parent : parents) {
                // Only consider parents in the current document, i.e. possible links in this View's document
                if (parent->getDocument() != cmd->getDocument()) {
                    continue;
                }
                // 2nd, do we really have a link to obj?
                if (parent->isDerivedFrom<App::LinkElement>()
                    || parent->isDerivedFrom<App::LinkGroup>()
                    || parent->isDerivedFrom<App::Link>()) {
                    // We have a link chain from this document to obj, and obj is in another document -> it is an XLink target
                    is_linked = true;
                }
            }
        }
        if (is_linked) {
            xShapes.push_back(obj);
            continue;
        }
        //not a Link and not null.  assume to be drawable.  Undrawables will be
        // skipped later.
        shapes.push_back(obj);
        if (faceObj) {
            continue;
        }
        //don't know if this works for an XLink
        for (auto& sub : sel.getSubNames()) {
            if (TechDraw::DrawUtil::getGeomTypeFromName(sub) == "Face") {
                faceName = sub;
                //
                faceObj = obj;
                break;
            }
        }
    }
}

std::pair<Base::Vector3d, Base::Vector3d> viewDirection()
{
    if (!Preferences::useCameraDirection()) {
        return { Base::Vector3d(0, -1, 0), Base::Vector3d(1, 0, 0) };
    }

    auto faceInfo = faceFromSelection();
    if (faceInfo.first) {
        return DrawGuiUtil::getProjDirFromFace(faceInfo.first, faceInfo.second);
    }

    return DrawGuiUtil::get3DDirAndRot();
}

std::pair<App::DocumentObject*, std::string> faceFromSelection()
{
    auto selection = Gui::Selection().getSelectionEx(
        nullptr, App::DocumentObject::getClassTypeId(), Gui::ResolveMode::NoResolve);

    if (selection.empty()) {
        return { nullptr, "" };
    }

    for (auto& sel : selection) {
        for (auto& sub : sel.getSubNames()) {
            if (TechDraw::DrawUtil::getGeomTypeFromName(sub) == "Face") {
                return { sel.getObject(), sub };
            }
        }
    }

    return { nullptr, "" };
}
