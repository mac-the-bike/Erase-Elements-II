// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   Copyright (c) 2026 Peter McB                                          *
 *   Copyright (c) 2023 Peter McB                                          *
 *   Copyright (c) 2013 Jürgen Riegel (FreeCAD@juergen-riegel.net)         *
 *                                                                         *
 *   This file is part of FreeCAD.                                         *
 *                                                                         *
 *   FreeCAD is free software: you can redistribute it and/or modify it    *
 *   under the terms of the GNU Lesser General Public License as           *
 *   published by the Free Software Foundation, either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful, but        *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with FreeCAD. If not, see                               *
 *   <https://www.gnu.org/licenses/>.                                      *
 *                                                                         *
 ***************************************************************************/

#include <fstream>

#include <boost/algorithm/string.hpp>
#include <format>

#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoEventCallback.h>

#include <QAction>
#include <QApplication>
#include <QMessageBox>
#include <SMESH_Mesh.hxx>
#include <SMESHDS_Mesh.hxx>
// #include <Standard_math.hxx>
#include <SMESH_MeshEditor.hxx>
// #endif

#include <App/Annotation.h>
#include <Base/Console.h>
#include <Gui/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/Command.h>
#include <Gui/Document.h>
#include <Gui/Utilities.h>
#include <Gui/View3DInventor.h>
#include <Gui/View3DInventorViewer.h>
#include <Gui/ViewProvider.h>
#include <Gui/WaitCursor.h>

#include <Mod/Fem/App/FemMeshObject.h>
#include <Mod/Fem/App/FemSetElementNodesObject.h>

#include "FemSelectionGate.h"
#include "TaskCreateElementSet.h"
#include "ViewProviderFemMesh.h"
#include "ui_TaskCreateElementSet.h"

#include <Gui/CommandT.h>
#include <Gui/MainWindow.h>

using namespace Gui;
// using namespace Fem;
using namespace FemGui;
using namespace std;
#include <bits/stdc++.h>

// std::string elementsName = "ElementsSet";
std::string TaskCreateElementSet::currentProject = "";

std::string annotation = "StartResultName";
std::string createdMesh = "cDUMMY";
std::string createdMeshSaved = createdMesh;
std::string delimiter = "&&";
std::string destinationfile = "";
std::string direction = "";
std::string firstMesh = "FIRSTMESH"; // first modified file
std::string highLightMesh;           //  name of highlighted mesh
std::string inp_file = "FreeCADOutputFileEraseElements.inp";
std::string lastName = "";
std::string lastRes = "LASTRES"; //  last modified results file
std::string meshType;            // type of - either 'result' or 'femmesh'
std::string newFemMesh = "NewFemMesh";
std::string newProject = "";
std::string newResultMesh = "NewResultMesh";
std::string othernum = "OTHERNUM"; // last other number
std::string otherMesh = "OTHER";
std::string resultMesh = "Result";
std::string sourcefile;
std::string startResultMesh = "StartResultMesh"; // StartResultMesh";
std::string uniqueMesh = "";

int checkResult = 0;
// int maxProject = 10;
double **nodeCoords; // these are node coords
int *nodeNumbers;    // these are node numbers - probably not required[100];
int otherMeshNumber = 0;
int passResult = 0;
int passFemMesh = 0;
map<string, string> resultNames;

App::DocumentObject *keepObj;

// these are required for VStudio
#include <string>
#include <map>

string myToUpper(std::string str)
{
    long unsigned int i;
    for (i = 0; i < str.length(); i++)
    {
        str[i] = toupper(str[i]);
    }
    return str;
}

// void myPrint()
//  {
//  prototype output statements
//      //    Gui::Command::doCommand(Gui::Command::Doc, "print(\"name9a1: \", '%d, ')", checkResult);
//      //    Gui::Command::doCommand(Gui::Command::Doc, "print(\"name9a0: \", '%s, ')", lastRes.c_str());
//      //    Base::Console().warning("Can't copy Result Mesh: {} to Result Mesh: {}\n",
//      //                            oldName.c_str(), newName.c_str());
// }

string getmeshtype(std::string name)
{
    // check for CCX_EigenMode_7_Results_Mesh i.e. mesh in name
    std::string meshType = "NULL";
    std::size_t found1 = myToUpper(name).find(myToUpper(resultMesh));
    std::size_t found2 = myToUpper(name).find(myToUpper("Mesh"));

    // found Result and Mesh
    if (found1 != std::string::npos && found2 != std::string::npos)
    {
        meshType = "result";
    }
    else if (found2 != std::string::npos)
    // found Mesh
    {
        meshType = "femmesh";
    }
    //    Gui::Command::doCommand(Gui::Command::Doc, "print(\"name3: \", '%s, %s')", highLightMesh.c_str(), meshType.c_str());
    return meshType;
}

void addFaceToMesh(const std::vector<const SMDS_MeshNode *> nodes, SMESHDS_Mesh *MeshDS, int EID)
{
    int nbNodes = nodes.size();
    switch (nbNodes)
    {
    case 3: // 3 node triangle
        MeshDS->AddFaceWithID(nodes[0], nodes[1], nodes[2], EID);
        break;
    case 4: // 4 node quadrilateral
        MeshDS->AddFaceWithID(nodes[0], nodes[1], nodes[2], nodes[3], EID);
        break;
    case 6: // 6 node triangle
        MeshDS->AddFaceWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                              nodes[5], EID);
        break;
    case 8: // 8 node quadrilateral
        MeshDS->AddFaceWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                              nodes[5], nodes[6], nodes[7], EID);
        break;
    }
} // addFaceToMesh

void addVolumeToMesh(const std::vector<const SMDS_MeshNode *> nodes, SMESHDS_Mesh *MeshDS, int EID)
{

    int nbNodes = nodes.size();

    switch (nbNodes)
    {
    case 4: // 4 node tetrahedron
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], EID);
        break;
    case 5: // 5 node pyramid
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], EID);
        break;
    case 6: // 6 node pentahedron
        MeshDS
            ->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5],
                              EID);
        break;
    case 8: // 8 node hexahedron
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                                nodes[5], nodes[6], nodes[7], EID);
        break;
    case 10: // 10 node tetrahedron
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                                nodes[5], nodes[6], nodes[7], nodes[8], nodes[9],
                                EID);
        break;
    case 13: // 13 node pyramid
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                                nodes[5], nodes[6], nodes[7], nodes[8], nodes[9],
                                nodes[10], nodes[11], nodes[12], EID);
        break;
    case 15: // 15 node pentahedron
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                                nodes[5], nodes[6], nodes[7], nodes[8], nodes[9],
                                nodes[10], nodes[11], nodes[12], nodes[13], nodes[14],
                                EID);
        break;
    case 20: // 20 node hexahedron
        MeshDS->AddVolumeWithID(nodes[0], nodes[1], nodes[2], nodes[3], nodes[4],
                                nodes[5], nodes[6], nodes[7], nodes[8], nodes[9],
                                nodes[10], nodes[11], nodes[12], nodes[13], nodes[14],
                                nodes[15], nodes[16], nodes[17], nodes[18], nodes[19],
                                EID);
        break;
    } // default
} // addVolumeToMesh

int CopyResultsData(std::string oldName, std::string newName, bool makeVis)
{
    // copy results mesh to results mesh
    int error = 0;
    if (oldName.compare(newName) == 0)
    {
        error = 2;
        QMessageBox::warning(
            Gui::getMainWindow(),
            qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
            qApp->translate("CmdFemCreateElementsSet", "Can't copy Result Mesh to Result Mesh"));
        Base::Console().message("Warning in copy: source: {}, destination: {}\n",
                                oldName, newName);
        // Base::Console().message("Can't copy Result Mesh: {} to Result Mesh: {}\n",
        // oldName, newName);
    }
    // if either mesh is NOT results
    if (oldName.find("Result") == std::string::npos || newName.find("Result") == std::string::npos)
    {
        error = 1;
        // Base::Console().warning("Meshes must be a Result meshes: {} and {}\n", oldName, newName);
        Base::Console().message("Warning in copy: source: {}, destination: {}\n",
                                oldName, newName);
        QMessageBox::warning(
            Gui::getMainWindow(),
            qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
            qApp->translate("CmdFemCreateElementsSet", "Meshes must be Results meshes"));
    }
    if (newName.find("") == std::string::npos)
    {
    }
    if (error == 0)
    {
        Gui::Command::doCommand(Gui::Command::Doc,
                                "co = FreeCADGui.ActiveDocument.getObject(\'%s\')",
                                oldName.c_str());
        Gui::Command::doCommand(Gui::Command::Doc,
                                "FreeCAD.ActiveDocument.%s.FemMesh = co.Object.FemMesh",
                                newName.c_str());
        Gui::Command::doCommand(Gui::Command::Doc,
                                "Gui.ActiveDocument.getObject(\'%s\').BackfaceCulling = False",
                                newName.c_str());
        Gui::Command::doCommand(Gui::Command::Doc,
                                "Gui.ActiveDocument.getObject(\'%s\').Visibility = False",
                                oldName.c_str());
        if (makeVis)
        {
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "Gui.ActiveDocument.getObject(\'%s\').Visibility = True",
                                    newName.c_str());
        }
    }
    return error;
} // CopyResultsData

map<string, string> SplitString(string strArr, string delimiter)
{
    //    char strArr[] = FIRSTMESH&&CCX_1_Mesh&&LASTRES&&CCX_4_Mesh&&OTHER1&&CCX_1_Mesh&&OTHERNUM&&1&&

    int starting = 0;
    int counter = 0;
    int ending = strArr.find(delimiter);
    string key, val;
    map<string, string> TempResultNames;
    // While Loop.
    while (ending != -1)
    {
        if (counter % 2 == 0)
        { // save key
            key = strArr.substr(starting, ending - starting);
        }
        else
        { // save value and store
            val = strArr.substr(starting, ending - starting);
            TempResultNames[key] = val;
        }
        counter += 1;
        starting = ending + delimiter.size();
        ending = strArr.find(delimiter, starting);
    }
    return TempResultNames;
} // SplitString

int repeatNames(map<string, string> resultNames, std::string name)
{
   for (auto const &[key, val] : resultNames)
   {
        /* does name match with value when key = OTHERn */
      if ((val == name) && (key.find(otherMesh) != std::string::npos))
      {
            return 1;
      }
   }
    return 0;
} // repeatNames

void checkResultName()
{
    // checkResult = 1 - no annotation found
    // checkResult = 2 - annoatation found
    //  the annotation saves for modified files:
    //  FIRSTMESH && results or fem mesh - first modified file
    //  LASTRES && CCX_EigenMode_8_Results_Mesh - the latest modified result mesh
    //  OTHERn && CCX_EigenMode_8_Results_Mesh - the n'th result file modified, repeats poss.
    //  OTHERNUM && otherMeshNumber - n, the last number used for OTHER mesh

    int xpos = 0;
    // checkResult if there is an annotation present
    App::Document *doc = App::GetApplication().getActiveDocument();
    const std::vector<App::DocumentObject *> &all = doc->getObjects();
    checkResult = 1; // stop doing checkResult again
    for (std::vector<App::DocumentObject *>::const_iterator it = all.begin();
         it != all.end(); ++it)
    {
        // if present find the name
        std::string objectN = all[xpos]->getNameInDocument();
        if (objectN.find(annotation) != std::string::npos)
        {
            keepObj = all[xpos];
            checkResult = 2; // found
            resultNames = SplitString(keepObj->Label2.getValue(), delimiter);
            break;
        }
        xpos++;
    }
} // checkResultName

void generateMesh(std::string meshType)
{
    App::Document *doc = App::GetApplication().getActiveDocument();
    // checkResult = 1  no annotation found
    // checkResult = 2 - annoatation found
    checkResultName();
    //    Gui::Command::doCommand(Gui::Command::Doc, "print(\"name30: \", '%d' '%d')", passResult, checkResult);
    Gui::Command::doCommand(Gui::Command::Doc, "import Fem,os");
    if (checkResult == 1)
    {
        Gui::Command::doCommand(Gui::Command::Doc,
                                "ann = App.ActiveDocument.addObject('App::AnnotationLabel', \'%s\')",
                                annotation.c_str());
        Gui::Command::doCommand(Gui::Command::Doc,
                                "ann.Visibility = False");
        //        Gui::Command::doCommand(Gui::Command::Doc, "print(\"name10: \", '%s, %s ')", firstMesh.c_str(), highLightMesh.c_str());
        checkResultName();
        resultNames[firstMesh] = highLightMesh;
        //        Gui::Command::doCommand(Gui::Command::Doc, "print(\"name 1: \", '%s, and %d')", firstMesh.c_str(), checkResult);
    }
    else if (checkResult == 2)
    {
        if (resultNames.find(othernum) != resultNames.end())
        {
            passResult = 1;
        }
    }
    //    Gui::Command::doCommand(Gui::Command::Doc, "print(\"pass key: \", '%d %s ')", passResult, meshType.c_str());
    if (strcmp(meshType.c_str(), "result") == 0)
    {
        createdMesh = newResultMesh.c_str();
        if (passResult == 0)
        {
            // first pass
            string xstr(startResultMesh.c_str());
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "obj1 = App.ActiveDocument.addObject('Fem::FemMeshObject', \'%s\')",
                                    startResultMesh.c_str());
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "Gui.ActiveDocument.getObject(\'%s\').Visibility = False",
                                    startResultMesh.c_str());
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "ne = Gui.ActiveDocument.getObject(\'%s\')",
                                    highLightMesh.c_str());
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "obj1.FemMesh = ne.Object.FemMesh");
            /* checkResult that lastres is the right variable */
            //            Gui::Command::doCommand(Gui::Command::Doc, "print(\"name 2: \", '%s, ')", lastRes.c_str());
            resultNames[lastRes] = highLightMesh;
        } // passresult
        //        Gui::Command::doCommand(Gui::Command::Doc, "print(\"name 8: \", '%d %s ')", otherMeshNumber, highLightMesh.c_str());
        // if ((highLightMesh.find(startResultMesh) == std::string::npos) &&
        // highLightMesh.find(newResultMesh) == std::string::npos)
        if (highLightMesh.find(startResultMesh) == std::string::npos)
        {
            resultNames[lastRes] = highLightMesh;
            if (resultNames.find(othernum) != resultNames.end())
            {
                otherMeshNumber = stoi(resultNames[othernum]);
            }
            // Gui::Command::doCommand(Gui::Command::Doc, "print(\"name 81: \", '%d %s ')", otherMeshNumber, highLightMesh.c_str());
            // Base::Console().warning("add to annote: {} and: {}\n",
            // otherMeshNumber, highLightMesh.c_str());

            if (0 == repeatNames(resultNames, highLightMesh))
            {
                otherMeshNumber += 1;
                resultNames[otherMesh + std::to_string(otherMeshNumber)] = highLightMesh;
                resultNames[othernum] = std::to_string(otherMeshNumber);
            }
        }
        passResult += 1;
    } // result
    else if (strcmp(meshType.c_str(), "femmesh") == 0)
    {
        if (passFemMesh == 0)
        {
            string xstr("no rename required");
            createdMesh = newFemMesh.c_str();
        }
        else if (passFemMesh > 0)
        {
            createdMesh = newFemMesh.c_str();
        }
        passFemMesh += 1;
    } // femmesh
    else if (strcmp(meshType.c_str(), "NULL") == 0)
    {
        //        Gui::Command::doCommand(Gui::Command::Doc, "print(\"name 881: \", ' %s ')", meshType.c_str());
        Gui::Command::doCommand(Gui::Command::Doc, "Gui.activeDocument().resetEdit()");
        return;
    } // neither type

    uniqueMesh = doc->getUniqueObjectName(createdMesh.c_str());

    Gui::Command::doCommand(Gui::Command::Doc,
                            "Gui.ActiveDocument.getObject(\'%s\').Visibility = False",
                            highLightMesh.c_str());
    Gui::Command::doCommand(Gui::Command::Doc,
                            "newermesh = Fem.read(\'%s\')",
                            inp_file.c_str());
    Gui::Command::doCommand(Gui::Command::Doc,
                            "obj = App.ActiveDocument.addObject('Fem::FemMeshObject', \'%s\')",
                            uniqueMesh.c_str());
    Gui::Command::doCommand(Gui::Command::Doc,
                            "obj.FemMesh = newermesh");
    Gui::Command::doCommand(Gui::Command::Doc,
                            "Gui.ActiveDocument.getObject(\'%s\').BackfaceCulling = False",
                            uniqueMesh.c_str());
    Gui::Command::doCommand(Gui::Command::Doc,
                            "Gui.ActiveDocument.getObject(\'%s\').Visibility = True",
                            uniqueMesh.c_str());

    if (strcmp(meshType.c_str(), "result") == 0)
    {
        // copy the new results mesh to the original mesh
        Gui::Command::doCommand(Gui::Command::Doc,
                                "cu = FreeCADGui.ActiveDocument.getObject(\'%s\')",
                                uniqueMesh.c_str());
        Gui::Command::doCommand(Gui::Command::Doc,
                                "FreeCAD.ActiveDocument.%s.FemMesh = cu.Object.FemMesh",
                                resultNames[lastRes].c_str());
        Gui::Command::doCommand(Gui::Command::Doc,
                                "Gui.ActiveDocument.getObject(\'%s\').BackfaceCulling = False",
                                highLightMesh.c_str());
        //        Gui::Command::doCommand(Gui::Command::Doc, "print(\"name 331: \", '%s %s')", uniqueMesh.c_str(), highLightMesh.c_str());

        if (highLightMesh.find(newResultMesh) != std::string::npos)
        {
            // if we modify a NewResultMesh
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "Gui.ActiveDocument.getObject(\'%s\').Visibility = True",
                                    uniqueMesh.c_str());
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "Gui.ActiveDocument.getObject(\'%s\').Visibility = False",
                                    highLightMesh.c_str());
        }
        // if we modify a Results Mesh
        else if (highLightMesh.find(resultMesh) != std::string::npos)
        {
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "Gui.ActiveDocument.getObject(\'%s\').Visibility = False",
                                    uniqueMesh.c_str());
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "Gui.ActiveDocument.getObject(\'%s\').Visibility = True",
                                    highLightMesh.c_str());
        }
    } // result

} // generate mesh

void writeToFile(std::string fileName,
                 SMESHDS_Mesh *newMesh,
                 int *nodeNumbers,
                 double **nodeCoords,
                 int rows,
                 int requiredType)
{
    std::map<int, std::string> elType2D;
    std::map<int, std::string> elType3D;

    elType2D[3] = "S3"; // 3 node triangle
    elType2D[6] = "S6"; // 4 node quadrilateral
    elType2D[4] = "S4"; // 6 node triangle
    elType2D[8] = "S8"; // 8 node quadrilateral

    elType3D[4] = "C3D4";   // 4 node tetrahedron
    elType3D[6] = "C3D6";   // 6 node pentahedron
    elType3D[8] = "C3D8";   // 8 node hexahedron
    elType3D[10] = "C3D10"; // 10 node tetrahedron
    elType3D[15] = "C3D15"; // 15 node pentahedron
    elType3D[20] = "C3D20"; // 20 node hexahedron
                            // no pyramid elements
    std::ofstream out(fileName);
    if (!out)
    {
        return;
    }
    out << "** written by Erase Elements inp file writer for CalculiX,Abaqus meshes\n";
    out << "** all mesh elements.\n";

    out << "\n";
    out << "\n";
    out << "** Nodes\n";
    out << "*Node, NSET=Nall\n";

    for (int i = 1; i < rows + 1; i++)
    {
        if (nodeNumbers[i] > 0)
        {
            out << std::format(
                "{}, {:e}, {:e}, {:e}\n",
                nodeNumbers[i],
                nodeCoords[i][0],
                nodeCoords[i][1],
                nodeCoords[i][2]);
        }
    }

    SMDS_ElemIteratorPtr srcElemIt;
    SMDS_NodeIteratorPtr srcNodeIt;
    srcElemIt = newMesh->elementsIterator();
    srcNodeIt = newMesh->nodesIterator();
    const SMDS_MeshNode *nSrc;
    int NID, EID;
    while (srcNodeIt->more())
    {
        const SMDS_MeshNode *node = srcNodeIt->next();
        NID = node->GetID();
    }
    int numberNodes = -1;
    while (srcElemIt->more())
    {
        const SMDS_MeshElement *elem = srcElemIt->next();
        EID = elem->GetID();
        if (elem->GetType() != requiredType)
        {
            continue;
        }

        if (numberNodes != elem->NbNodes())
        {
            if (requiredType == 4)
            {
                out << "\n";
                out << "\n";
                out << "** Volume elements\n";
                out << std::format("*Element, TYPE={}, ELSET=Evolumes\n", elType3D[elem->NbNodes()]);
            }
            else if (requiredType == 3)
            {
                out << "** Face elements\n";
                out << std::format("*Element, TYPE={}, ELSET=Efaces\n", elType2D[elem->NbNodes()]);
            }
            numberNodes = elem->NbNodes();
        }
        SMDS_ElemIteratorPtr nIt = elem->nodesIterator();
        out << EID;
        while (nIt->more())
        {
            nSrc = static_cast<const SMDS_MeshNode *>(nIt->next());
            NID = nSrc->GetID();
            out << ", " << NID;
        } // for iN
        out << "\n";
    } // while print
    if (requiredType == 4)
    {
        out << "\n";
        out << "\n";
        out << "** Define element set Eall\n";
        out << "*ELSET, ELSET=Eall\n";
        out << "Evolumes\n";
    }
    else if (requiredType == 3)
    {
        out << "** Define element set Eall\n";
        out << "*ELSET, ELSET=Eall\n";
        out << "Efaces\n";
    }

    out.close();
    return;
}

TaskCreateElementSet::TaskCreateElementSet(Fem::FemSetElementNodesObject *pcObject, QWidget *parent)
    : TaskBox(Gui::BitmapFactory().pixmap("FEM_CreateElementsSet"),
              tr("Elements set"),
              true,
              parent),
      pcObject(pcObject), selectionMode(none)
{
    proxy = new QWidget(this);
    ui = new Ui_TaskCreateElementSet();
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);
    this->groupLayout()->addWidget(proxy);
    QObject::connect(ui->toolButton_Poly, &QToolButton::clicked, this, &TaskCreateElementSet::Poly);
    QObject::connect(ui->toolButton_Restore, &QToolButton::clicked, this, &TaskCreateElementSet::Restore);
    QObject::connect(ui->toolButton_Rename, &QToolButton::clicked, this, &TaskCreateElementSet::CopyResultsMesh);
    // checkResult if the Link to the FemMesh is defined
    assert(pcObject->FemMesh.getValue<Fem::FemMeshObject *>());
    MeshViewProvider = freecad_cast<ViewProviderFemMesh *>(
        Gui::Application::Instance->getViewProvider(pcObject->FemMesh.getValue<Fem::FemMeshObject *>()));
    assert(MeshViewProvider);

    elementTempSet = pcObject->Elements.getValues();
    std::set<long int>::iterator it;
    std::string info;
    info = "Delete the generated data in the other project: " + std::string(currentProject);
    App::Document *doc = App::GetApplication().getActiveDocument();
    newProject = doc->Label.getValue();
    if (strcmp(currentProject.c_str(), newProject.c_str()) != 0 && (passResult + passFemMesh != 0))
    {
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
                             qApp->translate("CmdFemCreateElementsSet", info.c_str()));
        return;
    }
}

void TaskCreateElementSet::Poly(void)
{
    std::vector<Gui::SelectionSingleton::SelObj> selection = Gui::Selection().getSelection(); // [0];
    highLightMesh = selection[0].FeatName;
    meshType = getmeshtype(highLightMesh);
    if (strcmp(meshType.c_str(), "NULL") == 0)
    {
        std::string mess;
        mess = "Not a mesh object: ";
        mess.append(highLightMesh);
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Invalid Selection"),
                             qApp->translate("CmdFemCreateElementsSet", mess.c_str()));
        return;
    }

    // add in new mod of newresultmesh
    if (highLightMesh.find(startResultMesh) != std::string::npos)
    {
        std::string mess;
        mess = "Not allowed to modify: ";
        mess.append(highLightMesh);
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Invalid Selection"),
                             qApp->translate("CmdFemCreateElementsSet", mess.c_str()));
        return;
    }

    Gui::Document *doc = Gui::Application::Instance->activeDocument();
    Gui::MDIView *view = doc->getActiveView();

    if (view->isDerivedFrom<Gui::View3DInventor>())
    {
        Gui::View3DInventorViewer *viewer = ((Gui::View3DInventor *)view)->getViewer();
        viewer->setEditing(true);
        viewer->startSelection(Gui::View3DInventorViewer::Clip);
        viewer->addEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                 DefineElementsCallback,
                                 this);
    }
}

void TaskCreateElementSet::CopyResultsMesh(void)
{
    // in the "Copy Results Mesh" option
    std::vector<Gui::SelectionSingleton::SelObj> selection =
        Gui::Selection().getSelection(); // [0];

    std::string mess;
    if (selection.size() != 2)
    {
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
                             qApp->translate("CmdFemCreateElementsSet", "Select 2 meshes"));
        return;
    }
    sourcefile = selection[0].FeatName;
    destinationfile = selection[1].FeatName;
    for (int i = 0; i < 2; i++)
    {
        std::string name;
        if (i == 0)
        {
            direction = "Source";
            name = sourcefile;
            meshType = getmeshtype(sourcefile);
        }
        else if (i == 1)
        {
            direction = "Destination";
            name = destinationfile;
            meshType = getmeshtype(destinationfile);
        }

        if (meshType != "result")
        {
            mess = direction + " mesh: ";
            mess.append(name);
            mess.append(" must be a Results mesh");
            QMessageBox::warning(Gui::getMainWindow(),
                                 qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
                                 qApp->translate("CmdFemCreateElementsSet", mess.c_str()));
            Base::Console().message("Warning in copy: source: {}, destination: {}\n",
                                    sourcefile, destinationfile);
            return;
        }
    }
    if (destinationfile.find(startResultMesh) != std::string::npos)
    {
        mess = "Can't Copy To: ";
        mess.append(destinationfile.c_str());
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
                             qApp->translate("CmdFemCreateElementsSet", mess.c_str()));
        Base::Console().message("Warning in copy: source: {}, destination: {}\n",
                                sourcefile, destinationfile);
        return;
    }
    if (destinationfile.find(newResultMesh) != std::string::npos)
    {
        mess = "Can't Copy To: ";
        mess.append(destinationfile.c_str());
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
                             qApp->translate("CmdFemCreateElementsSet", mess.c_str()));
        Base::Console().message("Warning in copy: source: {}, destination: {}\n",
                                sourcefile, destinationfile);
        return;
    }
    Base::Console().message("Copy Source: {}, Destination: {}\n", sourcefile.c_str(), destinationfile.c_str());
    CopyResultsData(sourcefile, destinationfile, true);
    checkResultName();
    if (checkResult == 2)
    {
        if (resultNames.find(othernum) != resultNames.end())
        {
            otherMeshNumber = stoi(resultNames[othernum]);
        }
    }
    if (destinationfile.find(newResultMesh) == std::string::npos)
    {
        otherMeshNumber += 1;
        resultNames[otherMesh + std::to_string(otherMeshNumber)] = destinationfile;
        resultNames[othernum] = std::to_string(otherMeshNumber);
    }

} // copy result mesh

void TaskCreateElementSet::Restore(void)
{
    otherMeshNumber = 0;
    passResult = 0;
    passFemMesh = 0;
    checkResult = 0;
    // checkResult = 1  no annotation found
    // checkResult = 2 - annoatation found
    checkResultName();
    if (checkResult == 2)
    {
        //    nothing
    }
    App::Document *doc = App::GetApplication().getActiveDocument();
    const std::vector<App::DocumentObject *> &all = doc->getObjects();
    int number = 0, xpos = 0, found = 0;
    int elList = 0;
    // put reverse here
    std::vector<string> listObjects;
    for (std::vector<App::DocumentObject *>::const_iterator it = all.begin(); it != all.end();
         ++it)
    {
        std::string objectN = all[xpos]->getNameInDocument();
        listObjects.push_back(objectN);
        xpos++;
    }

    // iterate through in reverse order
    for (std::vector<string>::reverse_iterator it = listObjects.rbegin(); it != listObjects.rend(); ++it)
    {
        std::string objectN = (*it);
        if (objectN.find(startResultMesh) != std::string::npos)
        {
            number++;
            for (auto const &[key, val] : resultNames)
            {
                auto itt = std::find(listObjects.begin(), listObjects.end(), val);
                found = 0;
                if (itt != listObjects.end())
                {
                    found = 1;
                }
                if (found == 1)
                {
                    // check on the destination of the copy - it must exist
                    //   If the sub-string is not found, it returns string::npos.
                    // is OTHER<number> found and OTHERNUM not found
                    // can't copy to newResultMesh
                    if (key.find(otherMesh) != std::string::npos &&  // found
                        key.find(othernum) == std::string::npos &&   // not found
                        val.find(newResultMesh) == std::string::npos //  not found
                    )
                    {
                        CopyResultsData(objectN, val, false);
                    }
                    // found firstmesh
                    meshType = getmeshtype(val);
                    // is FIRSTMESH found
                    if (key.find(firstMesh) != std::string::npos && // found
                        meshType == "result")
                    {
                        CopyResultsData(objectN, val, true);
                    }
                }
            } // for
            Gui::Command::doCommand(Gui::Command::Doc, "App.ActiveDocument.removeObject(\'%s\')", objectN.c_str());
        }

        else if (objectN.find(newResultMesh) != std::string::npos)
        {
            number++;
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "App.ActiveDocument.removeObject(\'%s\')",
                                    objectN.c_str());
        }
        else if (objectN.find(annotation) != std::string::npos)
        {
            number++;
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "App.ActiveDocument.removeObject(\'%s\')",
                                    objectN.c_str());
        }
        else if (objectN.find(newFemMesh) != std::string::npos)
        {
            number++;
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "App.ActiveDocument.removeObject(\'%s\')",
                                    objectN.c_str());
        }
        else if (objectN.find(Fem::FemSetElementNodesObject::getElementName()) != std::string::npos)
        {
            // save the first elementslist name to use when object is destroyed

            if (elList > 0)
            {
                Gui::Command::doCommand(Gui::Command::Doc,
                                        "App.ActiveDocument.removeObject(\'%s\')",
                                        objectN.c_str());
            }
            else if (elList == 0)
            {
                elList++;
                lastName = objectN;
            }
        }
        else if (objectN.find(createdMeshSaved) != std::string::npos)
        {
            number++;
            Gui::Command::doCommand(Gui::Command::Doc,
                                    "App.ActiveDocument.removeObject(\'%s\')",
                                    objectN.c_str());
        }
    } // for
    Gui::Command::doCommand(Gui::Command::Doc,
                            "App.ActiveDocument.recompute()");
    if (number == 0)
    {
        QMessageBox::warning(Gui::getMainWindow(),
                             qApp->translate("CmdFemCreateElementsSet", "Wrong Selection"),
                             qApp->translate("CmdFemCreateElementsSet", "No Data To Restore\n"));
        return;
    }
    otherMeshNumber = 0;
    passResult = 0;
    passFemMesh = 0;
    currentProject = "";
    checkResult = 0;
    Gui::Command::doCommand(Gui::Command::Doc, "Gui.activeDocument().resetEdit()");
    // highlight the firstMesh mesh - if present
    resultNames.clear();
    return;
} // restore

void TaskCreateElementSet::DefineElementsCallback(void *ud, SoEventCallback *n)
{
    Gui::WaitCursor wc;
    TaskCreateElementSet *taskBox = static_cast<TaskCreateElementSet *>(ud);
    // When this callback function is invoked we must in either case leave the edit mode
    Gui::View3DInventorViewer *view =
        reinterpret_cast<Gui::View3DInventorViewer *>(n->getUserData());
    view->setEditing(false);
    view->removeEventCallback(SoMouseButtonEvent::getClassTypeId(), DefineElementsCallback, ud);
    n->setHandled();

    Gui::SelectionRole role;
    std::vector<SbVec2f> clPoly = view->getGLPolygon(&role);
    if (clPoly.size() < 3)
    {
        return;
    }
    if (clPoly.front() != clPoly.back())
    {
        clPoly.push_back(clPoly.front());
    }

    SoCamera *cam = view->getSoRenderManager()->getCamera();
    SbViewVolume vv = cam->getViewVolume();
    Gui::ViewVolumeProjection proj(vv);
    Base::Polygon2d polygon;
    for (std::vector<SbVec2f>::const_iterator it = clPoly.begin(); it != clPoly.end(); ++it)
    {
        polygon.Add(Base::Vector2d((*it)[0], (*it)[1]));
    }

    taskBox->DefineNodes(polygon, proj, role == Gui::SelectionRole::Inner ? true : false);
} // DefineElementsCallback

// pmcb
void TaskCreateElementSet::DefineNodes(const Base::Polygon2d &polygon,
                                       const Gui::ViewVolumeProjection &proj,
                                       bool inner)
{
    const SMESHDS_Mesh *srcMeshDS =
        const_cast<SMESH_Mesh *>(
            pcObject->FemMesh.getValue<Fem::FemMeshObject *>()->FemMesh.getValue().getSMesh())
            ->GetMeshDS();

    std::vector<Gui::SelectionSingleton::SelObj> selection =
        Gui::Selection().getSelection(); // [0];
    highLightMesh = selection[0].FeatName;
    meshType = getmeshtype(highLightMesh);
    elementTempSet.clear();
    int nElements = srcMeshDS->GetMeshInfo().NbElements();
    int nVolumes = srcMeshDS->GetMeshInfo().NbVolumes();
    int requiredType = nVolumes > 0 ? 4 : 3; // type = 4 - 3D, type = 3 - 2D

    double cOfGX, cOfGY, cOfGZ;
    const SMDS_MeshNode *nSrc;
    int EID;
    currentProject = newProject;

    SMESHDS_Mesh *newMeshDS = new SMESHDS_Mesh(nElements, true);
    Base::Vector3f pt2d;

    SMDS_ElemIteratorPtr srcElemIt = srcMeshDS->elementsIterator();
    SMDS_NodeIteratorPtr srcNode = srcMeshDS->nodesIterator();

    const SMDS_MeshNode *nTgt;
    std::vector<const SMDS_MeshNode *> nodes;
    int keepElement = 0, maxNode = -1;
    while (srcNode->more())
    {
        const SMDS_MeshNode *aNode = srcNode->next();
        if (aNode->GetID() > maxNode)
        {
            maxNode = aNode->GetID();
        }
    }

    nodeNumbers = new int[maxNode + 2];
    nodeCoords = new double *[maxNode + 2]; // these are node coords
    for (int i = 0; i < maxNode + 2; i++)
    {
        nodeCoords[i] = new double[3]; // x,y,z
        nodeNumbers[i] = 0;
    }

    elementTempSet.insert(requiredType * -1); // the type of elements
    int pNodes;                               // the first pnodes are used in the cofg calc

    while (srcElemIt->more())
    {

        pNodes = 4; // the first pnodes are used in the cofg calc
        const SMDS_MeshElement *elem = srcElemIt->next();
        nodes.resize(elem->NbNodes());
        EID = elem->GetID();
        if (elem->GetType() != requiredType)
        {
            continue;
        }
        SMDS_ElemIteratorPtr nIt = elem->nodesIterator();

        if (requiredType == 4) // 3D
        {
            if (elem->NbNodes() == 8 || elem->NbNodes() == 20)
            { // 8 or 20 node brick
                pNodes = 8;
            }
            if (elem->NbNodes() == 6 || elem->NbNodes() == 15)
            { // 6 or 15 node penta
                pNodes = 8;
            }
        }
        else if (requiredType == 3) // 2D
        {
            if (elem->NbNodes() == 3 || elem->NbNodes() == 6)
            { // 3 or 6 node triangles
                pNodes = 3;
            }
        }
        cOfGX = 0.0;
        cOfGY = 0.0;
        cOfGZ = 0.0;
        for (int iN = 0; nIt->more(); ++iN)
        {
            nSrc = static_cast<const SMDS_MeshNode *>(nIt->next());
            nTgt = srcMeshDS->FindNode(nSrc->GetID());
            nodes[iN] = nTgt;
            newMeshDS->AddNodeWithID(nSrc->X(), nSrc->Y(), nSrc->Z(), nSrc->GetID());
            if (nodeNumbers[nSrc->GetID()] == 0)
            {
                nodeCoords[nSrc->GetID()][0] = nSrc->X();
                nodeCoords[nSrc->GetID()][1] = nSrc->Y();
                nodeCoords[nSrc->GetID()][2] = nSrc->Z();
            }
            if (iN < pNodes)
            {
                cOfGX += nSrc->X() / pNodes;
                cOfGY += nSrc->Y() / pNodes;
                cOfGZ += nSrc->Z() / pNodes;
            }
        } // for iN

        SMESH_MeshEditor::ElemFeatures elemFeat(elem->GetType(), elem->IsPoly());
        elemFeat.SetID(EID);

        /* add the bit to determine which elements are outside the poly */
        Base::Vector3f vec(cOfGX, cOfGY, cOfGZ);
        pt2d = proj(vec);
        if (polygon.Contains(Base::Vector2d(pt2d.x, pt2d.y)) != inner)
        {
            if (strcmp(meshType.c_str(), "femmesh") == 0)
            {
                for (long unsigned int i = 0; i < nodes.size(); i++)
                {
                    nodeNumbers[nodes[i]->GetID()] = nodes[i]->GetID();
                }
            }
            elementTempSet.insert(EID);
            keepElement += 1;
            if (requiredType == 4)
            {
                addVolumeToMesh(nodes, newMeshDS, EID);
            }
            else if (requiredType == 3)
            {
                addFaceToMesh(nodes, newMeshDS, EID);
            }
        }
    } // while

    int erase = -1;
    if (nVolumes != 0)
    {
        erase = nVolumes - keepElement;
    }
    else
    {
        erase = nElements - keepElement;
    }
    if (keepElement > 0)
    {
        if (erase > 0)
        {
            Base::Console().message("Number of Elements Kept: {}, Number of Elements Erased: {}\n",
                                    keepElement,
                                    erase);
            if (strcmp(meshType.c_str(), "result") == 0)
            {
                srcNode = srcMeshDS->nodesIterator();
                while (srcNode->more())
                {
                    const SMDS_MeshNode *nSrc = srcNode->next();
                    nodeCoords[nSrc->GetID()][0] = nSrc->X();
                    nodeCoords[nSrc->GetID()][1] = nSrc->Y();
                    nodeCoords[nSrc->GetID()][2] = nSrc->Z();
                    nodeNumbers[nSrc->GetID()] = nSrc->GetID();
                }
            }
            writeToFile(inp_file, newMeshDS, nodeNumbers, nodeCoords, maxNode, requiredType);
            generateMesh(meshType);
        }
        else
        {
            QMessageBox::warning(
                Gui::getMainWindow(),
                qApp->translate("CmdFemCreateElementsSet", "Erased Elements"),
                qApp->translate("CmdFemCreateElementsSet", "No Elements Erased - no mesh generated."));
            return;
        }
    }
    else
    {
        QMessageBox::warning(
            Gui::getMainWindow(),
            qApp->translate("CmdFemCreateElementsSet", "Erased Elements"),
            qApp->translate("CmdFemCreateElementsSet", "All Elements Erased - no mesh generated."));
        return;
    }
    newMeshDS->Modified();
    Gui::Command::doCommand(Gui::Command::Doc, "Gui.activeDocument().resetEdit()");

} // void TaskCreateElementSet::DefineNodes

void TaskCreateElementSet::onSelectionChanged(const Gui::SelectionChanges &msg)
{
    if (selectionMode == none)
    {
        return;
    }

    if (msg.Type == Gui::SelectionChanges::AddSelection)
    {
        std::string subName(msg.pSubName);
        unsigned int i = 0;
        for (; i < subName.size(); i++)
        {
            if (msg.pSubName[i] == 'F')
            {
                break;
            }
        }
        int elem = atoi(subName.substr(4).c_str());
        int face = atoi(subName.substr(i + 1).c_str());
        elementTempSet.clear();
        std::set<long> tmp =
            pcObject->FemMesh.getValue<Fem::FemMeshObject *>()->FemMesh.getValue().getSurfaceNodes(
                elem,
                face);
        elementTempSet.insert(tmp.begin(), tmp.end());

        selectionMode = none;
        Gui::Selection().rmvSelectionGate();
    }
}

/*********************************************************/

TaskCreateElementSet::~TaskCreateElementSet()
{
    if (checkResult == 2)
    {
        string temp = "";
        for (auto const &[key, val] : resultNames)
        {
            temp += key.c_str() + delimiter;
            temp += val.c_str() + delimiter;
        }
        keepObj->Label2.setValue(temp);
    }
    // delete the mesh file
    Gui::Command::doCommand(Gui::Command::Doc,
                            "os.remove(\'%s\') if (os.path.exists(\'%s\'))  else False",
                            inp_file.c_str(),
                            inp_file.c_str());
    // delete last elementsset
    if (strcmp(lastName.c_str(), "") != 0)
    {
        Gui::Command::doCommand(Gui::Command::Doc,
                                "App.ActiveDocument.removeObject(\'%s\')",
                                lastName.c_str());
        lastName = "";
    }
    Gui::Command::doCommand(Gui::Command::Doc, "App.ActiveDocument.recompute()");
    delete ui;
}

#include "moc_TaskCreateElementSet.cpp"
