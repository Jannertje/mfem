// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.
//
//       --------------------------------------------------------------
//       Shaper Miniapp: Resolve material interfaces by mesh refinement
//       --------------------------------------------------------------
//
// This miniapp performs multiple levels of adaptive mesh refinement to resolve
// the interfaces between different "materials" in the mesh, as specified by the
// given material() function. It can be used as a simple initial mesh generator,
// for example in the case when the interface is too complex to describe without
// local refinement. Both conforming and non-conforming refinements are supported.
//
// Compile with: make shaper
//
// Sample runs:  shaper
//               shaper -m ../../data/inline-tri.mesh
//               shaper -m ../../data/inline-hex.mesh
//               shaper -m ../../data/inline-tet.mesh
//               shaper -m ../../data/amr-quad.mesh
//               shaper -m ../../data/beam-quad.mesh
//               shaper -m ../../data/ball-nurbs.mesh
//               shaper -m ../../data/mobius-strip.mesh
//               shaper -m ../../data/square-disc-surf.mesh
//               shaper -m ../../data/star-q3.mesh -sd 2 -ncl -1

#include "mfem.hpp"
#include <fstream>
#include <iostream>

using namespace mfem;
using namespace std;

// Given a point X, return its material id as an integer. The ids should be
// positive. If the point is exactly on the interface, return 0.
//
// This particular implementation, rescales the mesh to [-1,1]^sdim given the
// xmin/xmax bounding box, and shapes in a simple annulus/shell with respect to
// the rescaled coordinates.
int material(Vector &x, Vector &xmin, Vector &xmax)
{
   static double p = 2.0;

   // Rescaling to [-1,1]^sdim
   for (int i = 0; i < x.Size(); i++)
   {
      x(i) = (2*x(i)-xmin(i)-xmax(i))/(xmax(i)-xmin(i));
   }

   // A simple annulus/shell
   if (x.Normlp(p) > 0.4 && x.Normlp(p) < 0.6) { return 1; }
   if (x.Normlp(p) < 0.4 || x.Normlp(p) > 0.6) { return 2; }
   return 0;
}

int main (int argc, char *argv[])
{
   int sd = 2;
   int nclimit = 1;
   const char *mesh_file = "../../data/inline-quad.mesh";

   // Parse command line
   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Input mesh file to shape materials in.");
   args.AddOption(&sd, "-sd", "--sub-divisions",
                  "Number of element subdivisions for interface detection.");
   args.AddOption(&nclimit, "-ncl", "--nc-limit",
                  "Level of hanging nodes allowed (-1 = unlimited).");
   args.Parse();
   if (!args.Good()) { args.PrintUsage(cout); return 1; }
   args.PrintOptions(cout);

   // Read initial mesh, get dimensions and bounding box
   Mesh mesh(mesh_file, 1, 1);
   int dim = mesh.Dimension();
   int sdim = mesh.SpaceDimension();
   Vector xmin, xmax;
   mesh.GetBoundingBox(xmin, xmax);

   // NURBS meshes don't support non-conforming refinement for now
   if (mesh.NURBSext) { mesh.SetCurvature(2); }

   // Mesh attributes will be visualized as piece-wise constants
   L2_FECollection attr_fec(0, dim);
   FiniteElementSpace attr_fespace(&mesh, &attr_fec);
   GridFunction attr(&attr_fespace);

   // GLVis server to visualize to
   char vishost[] = "localhost";
   int  visport   = 19916;
   socketstream sol_sock(vishost, visport);
   sol_sock.precision(8);

   // Shaping loop
   for (int iter = 0; 1; iter++)
   {
      Array<Refinement> refs;
      for (int i = 0; i < mesh.GetNE(); i++)
      {
         bool refine = false;

         // Sample materials in each element using "sd" sub-divisions
         Vector pt;
         int geom = mesh.GetElementBaseGeometry(i);
         ElementTransformation *T = mesh.GetElementTransformation(i);
         RefinedGeometry *RefG = GlobGeometryRefiner.Refine(geom, sd, 1);
         IntegrationRule &ir = RefG->RefPts;

         // Refine any element where different materials are detected. A more
         // sophisticated logic can be implemented here -- e.g. don't refine
         // the interfaces between certain materials.
         int mat = 0;
         double matsum = 0.0;
         for (int j = 0; j < ir.GetNPoints(); j++)
         {
            T->Transform(ir.IntPoint(j), pt);
            mat = material(pt, xmin, xmax);
            matsum += mat;
            if ((int)matsum != mat*(j+1))
            {
               refine = true;
            }
         }

         // Set the element attribute as the "average". Other choices are
         // possible here too, e.g. attr(i) = mat;
         attr(i) = (int)(matsum/ir.GetNPoints());
         mesh.SetAttribute(i, attr(i));

         // Mark the element for refinement
         if (refine)
         {
            refs.Append(Refinement(i));
         }
      }

      // Visualization
      sol_sock << "solution\n" << mesh << attr;
      if (iter == 0 && sdim == 2)
      {
         sol_sock << "keys 'RjlmpppppppppppppA*************'\n";
      }
      if (iter == 0 && sdim == 3)
      {
         sol_sock << "keys 'YYYYYYYYYXXXXXXXmA********8888888pppttt";
         if (dim == 3) { sol_sock << "iiM"; }
         sol_sock << "'\n";
      }
      sol_sock << flush;

      // Ask the user if we should continue refining
      char yn;
      cout << "Mesh has " << mesh.GetNE() << " elements. \n"
           << "Continue shaping? --> ";
      cin >> yn;
      if (yn == 'n' || yn == 'q') { break; }

      // Perform refinement, update spaces and grid functions
      mesh.GeneralRefinement(refs, -1, nclimit);
      attr_fespace.Update();
      attr.Update();
   }

   // Save the final mesh
   ofstream mesh_ofs("shaper.mesh");
   mesh_ofs.precision(8);
   mesh.Print(mesh_ofs);
}