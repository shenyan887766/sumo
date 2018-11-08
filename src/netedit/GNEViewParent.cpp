/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNEViewParent.cpp
/// @author  Jakob Erdmann
/// @date    Feb 2011
/// @version $Id$
///
// A single child window which contains a view of the edited network (adapted
// from GUISUMOViewParent)
// While we don't actually need MDI for netedit it is easier to adapt existing
// structures than to write everything from scratch.
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#include <string>
#include <vector>
#include <utils/foxtools/MFXUtils.h>
#include <utils/foxtools/MFXCheckableButton.h>
#include <utils/foxtools/MFXImageHelper.h>
#include <utils/common/UtilExceptions.h>
#include <utils/geom/Position.h>
#include <utils/geom/Boundary.h>
#include <utils/gui/globjects/GUIGlObjectTypes.h>
#include <utils/gui/globjects/GUIGlObjectStorage.h>
#include <utils/gui/images/GUIIcons.h>
#include <utils/gui/images/GUIIconSubSys.h>
#include <utils/gui/div/GUIDesigns.h>
#include <utils/gui/globjects/GUIGlObjectStorage.h>
#include <utils/gui/div/GUIIOGlobals.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/common/MsgHandler.h>
#include <netedit/frames/GNETLSEditorFrame.h>
#include <netedit/frames/GNESelectorFrame.h>
#include <netedit/frames/GNEConnectorFrame.h>
#include <netedit/frames/GNETLSEditorFrame.h>
#include <netedit/frames/GNEAdditionalFrame.h>
#include <netedit/frames/GNECrossingFrame.h>
#include <netedit/frames/GNETAZFrame.h>
#include <netedit/frames/GNEDeleteFrame.h>
#include <netedit/frames/GNEPolygonFrame.h>
#include <netedit/frames/GNEProhibitionFrame.h>
#include <netedit/frames/GNEInspectorFrame.h>
#include <netedit/netelements/GNEJunction.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/additionals/GNEAdditional.h>
#include <netedit/additionals/GNEPOI.h>
#include <netedit/additionals/GNEPoly.h>
#include <netedit/dialogs/GNEDialogACChooser.h>

#include "GNENet.h"
#include "GNEViewNet.h"
#include "GNEViewParent.h"
#include "GNEUndoList.h"
#include "GNEApplicationWindow.h"


// ===========================================================================
// FOX callback mapping
// ===========================================================================

FXDEFMAP(GNEViewParent) GNEViewParentMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_MAKESNAPSHOT,                       GNEViewParent::onCmdMakeSnapshot),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEJUNCTION,                     GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEEDGE,                         GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATETLS,                          GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEADD,                          GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEPOI,                          GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  MID_LOCATEPOLY,                         GNEViewParent::onCmdLocate),
    FXMAPFUNC(SEL_COMMAND,  FXMDIChild::ID_MDI_MENUCLOSE,           GNEViewParent::onCmdClose),
    FXMAPFUNC(SEL_CHANGED,  MID_GNE_VIEWPARENT_FRAMEAREAWIDTH,      GNEViewParent::onCmdUpdateFrameAreaWidth),
};

// Object implementation
FXIMPLEMENT(GNEViewParent, GUIGlChildWindow, GNEViewParentMap, ARRAYNUMBER(GNEViewParentMap))


// ===========================================================================
// member method definitions
// ===========================================================================

GNEViewParent::GNEViewParent(FXMDIClient* p, FXMDIMenu* mdimenu, const FXString& name, GNEApplicationWindow* parentWindow,
                             FXGLCanvas* share, GNENet* net, GNEUndoList* undoList, FXIcon* ic, FXuint opts, FXint x, FXint y, FXint w, FXint h) :
    GUIGlChildWindow(p, parentWindow, mdimenu, name, ic, opts, x, y, w, h),
    myGNEAppWindows(parentWindow) {
    // Add child to parent
    myParent->addChild(this, false);

    // add undo/redo buttons
    new FXButton(myNavigationToolBar, "\tUndo\tUndo the last Change.", GUIIconSubSys::getIcon(ICON_UNDO), parentWindow->getUndoList(), FXUndoList::ID_UNDO, GUIDesignButtonToolbar);
    new FXButton(myNavigationToolBar, "\tRedo\tRedo the last Change.", GUIIconSubSys::getIcon(ICON_REDO), parentWindow->getUndoList(), FXUndoList::ID_REDO, GUIDesignButtonToolbar);

    // Create Vertical separator
    new FXVerticalSeparator(myNavigationToolBar, GUIDesignVerticalSeparator);

    // Create Frame Splitter
    myFramesSplitter = new FXSplitter(myContentFrame, this, MID_GNE_VIEWPARENT_FRAMEAREAWIDTH, GUIDesignSplitter | SPLITTER_HORIZONTAL);

    // Create frames Area
    myFramesArea = new FXHorizontalFrame(myFramesSplitter, GUIDesignFrameArea);

    // Set default width of frames area
    myFramesArea->setWidth(220);

    // Create view area
    myViewArea = new FXHorizontalFrame(myFramesSplitter, GUIDesignViewnArea);

    // Add the view to a temporary parent so that we can add items to myViewArea in the desired order
    FXComposite* tmp = new FXComposite(this);

    // Create view net
    GNEViewNet* viewNet = new GNEViewNet(tmp, myViewArea, *myParent, this, net, undoList, myParent->getGLVisual(), share, myNavigationToolBar);

    // Set pointer myView with the created view net
    myView = viewNet;

    // Create frames
    myFrames.inspectorFrame = new GNEInspectorFrame(myFramesArea, viewNet);
    myFrames.selectorFrame = new GNESelectorFrame(myFramesArea, viewNet);
    myFrames.connectorFrame = new GNEConnectorFrame(myFramesArea, viewNet);
    myFrames.prohibitionFrame = new GNEProhibitionFrame(myFramesArea, viewNet);
    myFrames.TLSEditorFrame = new GNETLSEditorFrame(myFramesArea, viewNet);
    myFrames.additionalFrame = new GNEAdditionalFrame(myFramesArea, viewNet);
    myFrames.crossingFrame = new GNECrossingFrame(myFramesArea, viewNet);
    myFrames.TAZFrame = new GNETAZFrame(myFramesArea, viewNet);
    myFrames.deleteFrame = new GNEDeleteFrame(myFramesArea, viewNet);
    myFrames.polygonFrame = new GNEPolygonFrame(myFramesArea, viewNet);

    // Update frame areas after creation
    onCmdUpdateFrameAreaWidth(nullptr, 0, nullptr);

    // Hidde all Frames Area
    hideFramesArea();

    //  Buld view toolBars
    myView->buildViewToolBars(*this);

    // create windows
    GUIGlChildWindow::create();
}


GNEViewParent::~GNEViewParent() {
    // Remove child before remove
    myParent->removeChild(this);
}


void
GNEViewParent::hideAllFrames() {
    myFrames.hideFrames();
}

GNEInspectorFrame*
GNEViewParent::getInspectorFrame() const {
    return myFrames.inspectorFrame;
}


GNESelectorFrame*
GNEViewParent::getSelectorFrame() const {
    return myFrames.selectorFrame;
}


GNEConnectorFrame*
GNEViewParent::getConnectorFrame() const {
    return myFrames.connectorFrame;
}


GNETLSEditorFrame*
GNEViewParent::getTLSEditorFrame() const {
    return myFrames.TLSEditorFrame;
}


GNEAdditionalFrame*
GNEViewParent::getAdditionalFrame() const {
    return myFrames.additionalFrame;
}


GNECrossingFrame*
GNEViewParent::getCrossingFrame() const {
    return myFrames.crossingFrame;
}


GNETAZFrame* 
GNEViewParent::getTAZFrame() const {
    return myFrames.TAZFrame;
}


GNEDeleteFrame*
GNEViewParent::getDeleteFrame() const {
    return myFrames.deleteFrame;
}


GNEPolygonFrame*
GNEViewParent::getPolygonFrame() const {
    return myFrames.polygonFrame;
}


GNEProhibitionFrame*
GNEViewParent::getProhibitionFrame() const {
    return myFrames.prohibitionFrame;
}


void
GNEViewParent::showFramesArea() {
    // show and recalc framesArea if at least there is a frame shown
    if (myFrames.isFrameShown()) {
        myFramesArea->recalc();
        myFramesArea->show();
    }
}


void
GNEViewParent::hideFramesArea() {
    // hide and recalc frames Area if all frames are hidden is enabled
    if (!myFrames.isFrameShown()) {
        myFramesArea->hide();
        myFramesArea->recalc();
    }
}


GUIMainWindow*
GNEViewParent::getGUIMainWindow() const {
    return myParent;
}


GNEApplicationWindow*
GNEViewParent::getGNEAppWindows() const {
    return myGNEAppWindows;
}


void
GNEViewParent::eraseACChooserDialog(GNEDialogACChooser* chooserDialog) {
    if (chooserDialog == nullptr) {
        throw ProcessError("ChooserDialog already deleted");
    } else if (chooserDialog == myACChoosers.ACChooserJunction) {
        myACChoosers.ACChooserJunction = nullptr;
    } else if (chooserDialog == myACChoosers.ACChooserEdges) {
        myACChoosers.ACChooserEdges = nullptr;
    } else if (chooserDialog == myACChoosers.ACChooserTLS) {
        myACChoosers.ACChooserTLS = nullptr;
    } else if (chooserDialog == myACChoosers.ACChooserAdditional) {
        myACChoosers.ACChooserAdditional = nullptr;
    } else if (chooserDialog == myACChoosers.ACChooserPOI) {
        myACChoosers.ACChooserPOI = nullptr;
    } else if (chooserDialog == myACChoosers.ACChooserPolygon) {
        myACChoosers.ACChooserPolygon = nullptr;
    } else if (chooserDialog == myACChoosers.ACChooserProhibition) {
        myACChoosers.ACChooserProhibition = nullptr;
    } else {
        throw ProcessError("Unregistered chooserDialog");
    }
}


long
GNEViewParent::onCmdMakeSnapshot(FXObject*, FXSelector, void*) {
    // get the new file name
    FXFileDialog opendialog(this, "Save Snapshot");
    opendialog.setIcon(GUIIconSubSys::getIcon(ICON_EMPTY));
    opendialog.setSelectMode(SELECTFILE_ANY);
    opendialog.setPatternList("All Image Files (*.gif, *.bmp, *.xpm, *.pcx, *.ico, *.rgb, *.xbm, *.tga, *.png, *.jpg, *.jpeg, *.tif, *.tiff, *.ps, *.eps, *.pdf, *.svg, *.tex, *.pgf)\n"
                              "GIF Image (*.gif)\nBMP Image (*.bmp)\nXPM Image (*.xpm)\nPCX Image (*.pcx)\nICO Image (*.ico)\n"
                              "RGB Image (*.rgb)\nXBM Image (*.xbm)\nTARGA Image (*.tga)\nPNG Image  (*.png)\n"
                              "JPEG Image (*.jpg, *.jpeg)\nTIFF Image (*.tif, *.tiff)\n"
                              "Postscript (*.ps)\nEncapsulated Postscript (*.eps)\nPortable Document Format (*.pdf)\n"
                              "Scalable Vector Graphics (*.svg)\nLATEX text strings (*.tex)\nPortable LaTeX Graphics (*.pgf)\n"
                              "All Files (*)");
    if (gCurrentFolder.length() != 0) {
        opendialog.setDirectory(gCurrentFolder);
    }
    if (!opendialog.execute() || !MFXUtils::userPermitsOverwritingWhenFileExists(this, opendialog.getFilename())) {
        return 1;
    }
    gCurrentFolder = opendialog.getDirectory();
    std::string file = opendialog.getFilename().text();
    std::string error = myView->makeSnapshot(file);
    if (error != "") {
        // write warning if netedit is running in testing mode
        WRITE_DEBUG("Opening FXMessageBox 'error saving snapshot'");
        // open message box
        FXMessageBox::error(this, MBOX_OK, "Saving failed.", "%s", error.c_str());
        // write warning if netedit is running in testing mode
        WRITE_DEBUG("Closed FXMessageBox 'error saving snapshot' with 'OK'");
    }
    return 1;
}


long
GNEViewParent::onCmdClose(FXObject*, FXSelector /* sel */, void*) {
    myParent->handle(this, FXSEL(SEL_COMMAND, MID_CLOSE), nullptr);
    return 1;
}


long
GNEViewParent::onCmdLocate(FXObject*, FXSelector sel, void*) {
    GNEViewNet* view = dynamic_cast<GNEViewNet*>(myView);
    assert(view);
    std::vector<GNEAttributeCarrier*> ACsToLocate;
    switch (FXSELID(sel)) {
        case MID_LOCATEJUNCTION: {
            if (myACChoosers.ACChooserJunction) {
                // set focus in the existent chooser dialog
                myACChoosers.ACChooserJunction->setFocus();
            } else {
                // fill ACsToLocate with junctions
                std::vector<GNEJunction*> junctions = view->getNet()->retrieveJunctions();
                ACsToLocate.reserve(junctions.size());
                for (auto i : junctions) {
                    ACsToLocate.push_back(i);
                }
                myACChoosers.ACChooserJunction = new GNEDialogACChooser(this, GUIIconSubSys::getIcon(ICON_LOCATEJUNCTION), "Junction Chooser", ACsToLocate);
            }
            break;
        }
        case MID_LOCATEEDGE: {
            if (myACChoosers.ACChooserEdges) {
                // set focus in the existent chooser dialog
                myACChoosers.ACChooserEdges->setFocus();
            } else {
                // fill ACsToLocate with edges
                std::vector<GNEEdge*> edges = view->getNet()->retrieveEdges();
                ACsToLocate.reserve(edges.size());
                for (auto i : edges) {
                    ACsToLocate.push_back(i);
                }
                myACChoosers.ACChooserEdges = new GNEDialogACChooser(this, GUIIconSubSys::getIcon(ICON_LOCATEEDGE), "Edge Chooser", ACsToLocate);
            }
            break;
        }
        case MID_LOCATETLS: {
            if (myACChoosers.ACChooserTLS) {
                // set focus in the existent chooser dialog
                myACChoosers.ACChooserTLS->setFocus();
            } else {
                // fill ACsToLocate with junctions that haven TLS
                std::vector<GNEJunction*> junctions = view->getNet()->retrieveJunctions();
                ACsToLocate.reserve(junctions.size());
                for (auto i : junctions) {
                    if (i->getNBNode()->getControllingTLS().size() > 0) {
                        ACsToLocate.push_back(i);
                    }
                }
                myACChoosers.ACChooserTLS = new GNEDialogACChooser(this, GUIIconSubSys::getIcon(ICON_LOCATETLS), "TLS Chooser", ACsToLocate);
            }
            break;
        }
        case MID_LOCATEADD: {
            if (myACChoosers.ACChooserAdditional) {
                // set focus in the existent chooser dialog
                myACChoosers.ACChooserAdditional->setFocus();
            } else {
                // fill ACsToLocate with additionals
                std::vector<GNEAdditional*> additionals = view->getNet()->retrieveAdditionals();
                ACsToLocate.reserve(additionals.size());
                for (auto i : additionals) {
                    ACsToLocate.push_back(i);
                }
                myACChoosers.ACChooserAdditional = new GNEDialogACChooser(this, GUIIconSubSys::getIcon(ICON_LOCATEADD), "Additional Chooser", ACsToLocate);
            }
            break;
        }
        case MID_LOCATEPOI: {
            if (myACChoosers.ACChooserPOI) {
                // set focus in the existent chooser dialog
                myACChoosers.ACChooserPOI->setFocus();
            } else {
                // fill ACsToLocate with POIs
                for (auto i : view->getNet()->getPOIs()) {
                    ACsToLocate.push_back(dynamic_cast<GNEAttributeCarrier*>(i.second));
                }
                myACChoosers.ACChooserPOI = new GNEDialogACChooser(this, GUIIconSubSys::getIcon(ICON_LOCATEPOI), "POI Chooser", ACsToLocate);
            }
            break;
        }
        case MID_LOCATEPOLY: {
            if (myACChoosers.ACChooserPolygon) {
                // set focus in the existent chooser dialog
                myACChoosers.ACChooserPolygon->setFocus();
            } else {
                // fill ACsToLocate with polys
                for (auto i : view->getNet()->getPolygons()) {
                    ACsToLocate.push_back(dynamic_cast<GNEAttributeCarrier*>(i.second));
                }
                myACChoosers.ACChooserPolygon = new GNEDialogACChooser(this, GUIIconSubSys::getIcon(ICON_LOCATEPOLY), "Poly Chooser", ACsToLocate);
            }
            break;
        }
        default:
            throw ProcessError("Unknown Message ID in onCmdLocate");
    }
    // update locator popup
    myLocatorPopup->popdown();
    myLocatorButton->killFocus();
    myLocatorPopup->update();
    return 1;
}


long
GNEViewParent::onKeyPress(FXObject* o, FXSelector sel, void* eventData) {
    myView->onKeyPress(o, sel, eventData);
    return 0;
}


long
GNEViewParent::onKeyRelease(FXObject* o, FXSelector sel, void* eventData) {
    myView->onKeyRelease(o, sel, eventData);
    return 0;
}


long
GNEViewParent::onCmdUpdateFrameAreaWidth(FXObject*, FXSelector, void*) {
    // set width of FramesArea in all frames
    myFrames.setWidth(myFramesArea->getWidth());
    return 0;
}


bool
GNEViewParent::isSelected(GUIGlObject* o) const {
    GNEAttributeCarrier* ac = dynamic_cast<GNEAttributeCarrier*>(o);
    return ac && ac->isAttributeCarrierSelected();
}

// ---------------------------------------------------------------------------
// GNEViewParent::Frames - methods
// ---------------------------------------------------------------------------

GNEViewParent::Frames::Frames() : 
    inspectorFrame(nullptr),
    selectorFrame(nullptr),
    connectorFrame(nullptr),
    TLSEditorFrame(nullptr),
    additionalFrame(nullptr),
    crossingFrame(nullptr),
    TAZFrame(nullptr),
    deleteFrame(nullptr),
    polygonFrame(nullptr),
    prohibitionFrame(nullptr) {
}


void
GNEViewParent::Frames::hideFrames() {
    inspectorFrame->hide();
    selectorFrame->hide();
    connectorFrame->hide();
    TLSEditorFrame->hide();
    additionalFrame->hide();
    crossingFrame->hide();
    TAZFrame->hide();
    deleteFrame->hide();
    polygonFrame->hide();
    prohibitionFrame->hide();
}


void 
GNEViewParent::Frames::setWidth(double width) {
    // set width in all frames
    inspectorFrame->setFrameWidth(width);
    selectorFrame->setFrameWidth(width);
    connectorFrame->setFrameWidth(width);
    TLSEditorFrame->setFrameWidth(width);
    additionalFrame->setFrameWidth(width);
    crossingFrame->setFrameWidth(width);
    TAZFrame->setFrameWidth(width);
    deleteFrame->setFrameWidth(width);
    polygonFrame->setFrameWidth(width);
    prohibitionFrame->setFrameWidth(width);
}


bool 
GNEViewParent::Frames::isFrameShown() const {
    // check all frames
    if (inspectorFrame->shown()) {
        return true;
    } else if (selectorFrame->shown()) {
        return true;
    } else if (connectorFrame->shown()) {
        return true;
    } else if (TLSEditorFrame->shown()) {
        return true;
    } else if (additionalFrame->shown()) {
        return true;
    } else if (crossingFrame->shown()) {
        return true;
    } else if (TAZFrame->shown()) {
        return true;
    } else if (deleteFrame->shown()) {
        return true;
    } else if (polygonFrame->shown()) {
        return true;
    } else if (prohibitionFrame->shown()) {
        return true;
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------
// GNEViewParent::ACChoosers - methods
// ---------------------------------------------------------------------------

GNEViewParent::ACChoosers::ACChoosers() : 
    ACChooserJunction(nullptr),
    ACChooserEdges(nullptr),
    ACChooserTLS(nullptr),
    ACChooserAdditional(nullptr),
    ACChooserPOI(nullptr),
    ACChooserPolygon(nullptr),
    ACChooserProhibition(nullptr) {
}


GNEViewParent::ACChoosers::~ACChoosers() {
    // remove all  dialogs if are active
    if (ACChooserJunction) {
        delete ACChooserJunction ;
    }
    if (ACChooserEdges) {
        delete ACChooserEdges ;
    }
    if (ACChooserTLS) {
        delete ACChooserTLS ;
    }
    if (ACChooserAdditional) {
        delete ACChooserAdditional ;
    }
    if (ACChooserPOI) {
        delete ACChooserPOI ;
    }
    if (ACChooserPolygon) {
        delete ACChooserPolygon ;
    }
    if (ACChooserProhibition) {
        delete ACChooserProhibition;
    }
}

/****************************************************************************/

