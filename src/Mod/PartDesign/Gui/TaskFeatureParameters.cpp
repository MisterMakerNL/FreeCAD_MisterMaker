/***************************************************************************
 *   Copyright (C) 2015 Alexander Golubev (Fat-Zer) <fatzer2@gmail.com>    *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
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
#include <QMessageBox>
#endif

#include <App/DocumentObserver.h>
#include <Gui/Application.h>
#include <Gui/CommandT.h>
#include <Gui/MainWindow.h>
#include <Gui/BitmapFactory.h>
#include <Mod/PartDesign/App/Feature.h>
#include <Mod/PartDesign/App/Body.h>

#include "TaskFeatureParameters.h"
#include "TaskSketchBasedParameters.h"

using namespace PartDesignGui;
using namespace Gui;

/*********************************************************************
 *                      Task Feature Parameters                      *
 *********************************************************************/

TaskFeatureParameters::TaskFeatureParameters(PartDesignGui::ViewProvider *vp, QWidget *parent,
                                             const std::string& pixmapname, const QString& parname)
    : TaskBox(Gui::BitmapFactory().pixmap(pixmapname.c_str()), parname, true, parent)
    , vp(vp)
    , blockUpdate(false)
{
    Gui::Document* doc = vp->getDocument();
    this->attachDocument(doc);
}

void TaskFeatureParameters::slotDeletedObject(const Gui::ViewProviderDocumentObject& Obj)
{
    if (this->vp == &Obj) {
        this->vp = nullptr;
    }
}

void TaskFeatureParameters::onUpdateView(bool on)
{
    blockUpdate = !on;
    recomputeFeature();
}

void TaskFeatureParameters::recomputeFeature()
{
    if (!blockUpdate) {
        App::DocumentObject* obj = getObject();
        assert (obj);
        obj->recomputeFeature();
    }
}

/*********************************************************************
 *                            Task Dialog                            *
 *********************************************************************/
TaskDlgFeatureParameters::TaskDlgFeatureParameters(PartDesignGui::ViewProvider *vp)
    : vp(vp)
{
    assert(vp);
}

TaskDlgFeatureParameters::~TaskDlgFeatureParameters() = default;

bool TaskDlgFeatureParameters::accept()
{
    App::DocumentObject* feature = getObject();
    bool isUpdateBlocked = false;
    try {
        // Iterate over parameter dialogs and apply all parameters from them
        for ( QWidget *wgt : Content ) {
            TaskFeatureParameters *param = qobject_cast<TaskFeatureParameters *> (wgt);
            if(!param)
                continue;

            param->saveHistory ();
            param->apply ();
            isUpdateBlocked |= param->isUpdateBlocked();
        }
        // Make sure the feature is what we are expecting
        // Should be fine but you never know...
        if ( !feature->isDerivedFrom<PartDesign::Feature>() ) {
            throw Base::TypeError("Bad object processed in the feature dialog.");
        }

        if(isUpdateBlocked){
            Gui::cmdAppDocument(feature, "recompute()");
        } else {
            // object was already computed, nothing more to do with it...
            Gui::cmdAppDocument(feature, "purgeTouched()");

            // ...but touch parents to signal the change...
            for (auto obj : feature->getInList()){
                obj->touch();
            }
            // ...and recompute them
            Gui::cmdAppDocument(feature->getDocument(), "recompute()");
        }

        if (!feature->isValid()) {
            throw Base::RuntimeError(getObject()->getStatusString());
        }

        App::DocumentObject* previous = static_cast<PartDesign::Feature*>(feature)->getBaseObject(/* silent = */ true );
        Gui::cmdAppObjectHide(previous);

        // detach the task panel from the selection to avoid to invoke
        // eventually onAddSelection when the selection changes
        std::vector<QWidget*> subwidgets = getDialogContent();
        for (auto it : subwidgets) {
            TaskSketchBasedParameters* param = qobject_cast<TaskSketchBasedParameters*>(it);
            if (param)
                param->detachSelection();
        }

        Gui::cmdGuiDocument(feature, "resetEdit()");
        Gui::Command::commitCommand();
    } catch (const Base::Exception& e) {
        // Generally the only thing that should fail is feature->isValid() others should be fine
        QString errorText = QApplication::translate(feature->getTypeId().getName(), e.what());
        QMessageBox::warning(Gui::getMainWindow(), tr("Input error"), errorText);
        return false;
    }

    return true;
}

bool TaskDlgFeatureParameters::reject()
{
    auto feature = getObject<PartDesign::Feature>();
    App::DocumentObjectWeakPtrT weakptr(feature);
    App::Document* document = feature->getDocument();

    PartDesign::Body* body = PartDesign::Body::findBodyOf(feature);

    // Find out previous feature we won't be able to do it after abort
    // (at least in the body case)
    App::DocumentObject* previous = feature->getBaseObject(/* silent = */ true );

    // detach the task panel from the selection to avoid to invoke
    // eventually onAddSelection when the selection changes
    std::vector<QWidget*> subwidgets = getDialogContent();
    for (auto it : subwidgets) {
        TaskSketchBasedParameters* param = qobject_cast<TaskSketchBasedParameters*>(it);
        if (param)
            param->detachSelection();
    }

    // roll back the done things which may delete the feature
    Gui::Command::abortCommand();

    // if abort command deleted the object make the previous feature visible again
    if (weakptr.expired()) {
        // Make the tip or the previous feature visible again with preference to the previous one
        // TODO: ViewProvider::onDelete has the same code. May be this one is excess?
        if (previous && Gui::Application::Instance->getViewProvider(previous)) {
            Gui::Application::Instance->getViewProvider(previous)->show();
        }
        else if (body) {
            App::DocumentObject* tip = body->Tip.getValue();
            if (tip && Gui::Application::Instance->getViewProvider(tip)) {
                Gui::Application::Instance->getViewProvider(tip)->show();
            }
        }
    }

    Gui::cmdAppDocument(document, "recompute()");
    Gui::cmdGuiDocument(document, "resetEdit()");

    return true;
}

#include "moc_TaskFeatureParameters.cpp"
