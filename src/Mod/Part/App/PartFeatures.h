/***************************************************************************
 *   Copyright (c) 2011 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

#ifndef PART_FEATURES_H
#define PART_FEATURES_H

#include <App/PropertyStandard.h>
#include <App/PropertyUnits.h>

#include "PartFeature.h"


namespace Part
{

class PartExport RuledSurface : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::RuledSurface);

public:
    RuledSurface();

    App::PropertyEnumeration Orientation;
    App::PropertyLinkSub Curve1;
    App::PropertyLinkSub Curve2;

    /** @name methods override feature */
    //@{
    /// recalculate the feature
    App::DocumentObjectExecReturn *execute() override;
    short mustExecute() const override;
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderRuledSurface";
    }
    //@}

protected:
    void onChanged (const App::Property* prop) override;

private:
    App::DocumentObjectExecReturn* getShape(const App::PropertyLinkSub& link, TopoDS_Shape&) const;

private:
    static const char* OrientationEnums[];
};

class PartExport Loft : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::Loft);

public:
    Loft();

    App::PropertyLinkList Sections;
    App::PropertyBool Solid;
    App::PropertyBool Ruled;
    App::PropertyBool Closed;
    App::PropertyBool Linearize;
    App::PropertyIntegerConstraint MaxDegree;

    /** @name methods override feature */
    //@{
    /// recalculate the feature
    App::DocumentObjectExecReturn *execute() override;
    short mustExecute() const override;
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderLoft";
    }
    //@}

protected:
    void onChanged (const App::Property* prop) override;

private:
    static App::PropertyIntegerConstraint::Constraints Degrees;
};

class PartExport Sweep : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::Sweep);

public:
    Sweep();

    App::PropertyLinkList Sections;
    App::PropertyLinkSub Spine;
    App::PropertyBool Solid;
    App::PropertyBool Frenet;
    App::PropertyBool Linearize;
    App::PropertyEnumeration Transition;

    /** @name methods override feature */
    //@{
    /// recalculate the feature
    App::DocumentObjectExecReturn *execute() override;
    short mustExecute() const override;
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderSweep";
    }
    //@}

protected:
    void onChanged (const App::Property* prop) override;

private:
    static const char* TransitionEnums[];
};

class PartExport Thickness : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::Thickness);

public:
    Thickness();

    App::PropertyLinkSub Faces;
    App::PropertyQuantity Value;
    App::PropertyEnumeration Mode;
    App::PropertyEnumeration Join;
    App::PropertyBool Intersection;
    App::PropertyBool SelfIntersection;

    /** @name methods override feature */
    //@{
    /// recalculate the feature
    App::DocumentObjectExecReturn *execute() override;
    short mustExecute() const override;
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderThickness";
    }
    //@}

protected:
    void handleChangedPropertyType(Base::XMLReader &reader, const char *TypeName, App::Property *prop) override;

private:
    static const char* ModeEnums[];
    static const char* JoinEnums[];
};

class Refine : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::Refine);

public:
    Refine();

    App::PropertyLink Source;

    /** @name methods override feature */
    //@{
    /// recalculate the feature
    App::DocumentObjectExecReturn *execute() override;
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderRefine";
    }
    //@}
};

class Reverse : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::Reverse);

public:
    Reverse();

    App::PropertyLink Source;

    /** @name methods override feature */
    //@{
    /// recalculate the feature
    App::DocumentObjectExecReturn* execute() override;
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderReverse";
    }
    //@}
};

} //namespace Part


#endif // PART_FEATURES_H
