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

#ifndef MFEM_COMPLEX_FEM
#define MFEM_COMPLEX_FEM

#include "../linalg/complex_operator.hpp"
#include "gridfunc.hpp"
#include "linearform.hpp"
#include "bilinearform.hpp"
#ifdef MFEM_USE_MPI
#include "pgridfunc.hpp"
#include "plinearform.hpp"
#include "pbilinearform.hpp"
#endif
#include <complex>

namespace mfem
{

#ifdef MFEM_USE_MPI

/// Class for complex-valued grid function - Vector with associated FE space.
class ParComplexGridFunction : public Vector
{
private:

   ParGridFunction * pgfr_;
   ParGridFunction * pgfi_;

protected:
   void Destroy() { delete pgfr_; delete pgfi_; }

public:

   /* @brief Construct a ParComplexGridFunction associated with the
      ParFiniteElementSpace @a *f. */
   ParComplexGridFunction(ParFiniteElementSpace *f);

   void Update() { pgfr_->Update(); pgfi_->Update(); }

   /// Assign constant values to the ParComplexGridFunction data.
   ParComplexGridFunction &operator=(const std::complex<double> & value)
   { *pgfr_ = value.real(); *pgfi_ = value.imag(); return *this; }

   virtual void ProjectCoefficient(Coefficient &real_coeff,
                                   Coefficient &imag_coeff);
   virtual void ProjectCoefficient(VectorCoefficient &real_vcoeff,
                                   VectorCoefficient &imag_vcoeff);

   void Distribute(const Vector *tv);
   void Distribute(const Vector &tv) { Distribute(&tv); }

   /// Returns the vector restricted to the true dofs.
   void ParallelProject(Vector &tv) const;

   FiniteElementSpace *FESpace() { return pgfr_->FESpace(); }
   const FiniteElementSpace *FESpace() const { return pgfr_->FESpace(); }

   ParGridFunction & real() { return *pgfr_; }
   ParGridFunction & imag() { return *pgfi_; }
   const ParGridFunction & real() const { return *pgfr_; }
   const ParGridFunction & imag() const { return *pgfi_; }

   /// Destroys grid function.
   virtual ~ParComplexGridFunction() { Destroy(); }

};

class ParComplexLinearForm : public Vector
{
private:
   ComplexOperator::Convention conv_;

protected:
   ParLinearForm * plfr_;
   ParLinearForm * plfi_;

   HYPRE_Int * tdof_offsets_;

public:

   ParComplexLinearForm(ParFiniteElementSpace *pf,
                        ComplexOperator::Convention
                        convention = ComplexOperator::HERMITIAN);

   virtual ~ParComplexLinearForm();

   /// Adds new Domain Integrator.
   void AddDomainIntegrator(LinearFormIntegrator *lfi_real,
                            LinearFormIntegrator *lfi_imag);

   ParFiniteElementSpace *ParFESpace() const { return plfr_->ParFESpace(); }

   ParLinearForm & real() { return *plfr_; }
   ParLinearForm & imag() { return *plfi_; }
   const ParLinearForm & real() const { return *plfr_; }
   const ParLinearForm & imag() const { return *plfi_; }

   void Update(ParFiniteElementSpace *pf = NULL);

   /// Assembles the linear form i.e. sums over all domain/bdr integrators.
   void Assemble();

   /// Assemble the vector on the true dofs, i.e. P^t v.
   void ParallelAssemble(Vector &tv);

   /// Returns the vector assembled on the true dofs, i.e. P^t v.
   HypreParVector *ParallelAssemble();

   std::complex<double> operator()(const ParComplexGridFunction &gf) const;

};

// Class for parallel sesquilinear form
class ParSesquilinearForm
{
private:
   ComplexOperator::Convention conv_;

  //protected:
   ParBilinearForm *pblfr_;
   ParBilinearForm *pblfi_;

public:
   ParSesquilinearForm(ParFiniteElementSpace *pf,
                       ComplexOperator::Convention
                       convention = ComplexOperator::HERMITIAN);

   ComplexOperator::Convention GetConvention() const { return conv_; }
   void SetConvention(const ComplexOperator::Convention &
                      convention) { conv_  = convention; }

   /** Enable the use of static condensation. For details see the description
       for class StaticCondensation in fem/staticcond.hpp This method should be
       called before assembly. If the number of unknowns after static
       condensation is not reduced, it is not enabled. */
   void EnableStaticCondensation();

   ParBilinearForm & real() { return *pblfr_; }
   ParBilinearForm & imag() { return *pblfi_; }
   const ParBilinearForm & real() const { return *pblfr_; }
   const ParBilinearForm & imag() const { return *pblfi_; }

   /// Adds new Domain Integrator.
   void AddDomainIntegrator(BilinearFormIntegrator *bfi_real,
                            BilinearFormIntegrator *bfi_imag);

   /// Adds new Boundary Integrator.
   void AddBoundaryIntegrator(BilinearFormIntegrator *bfi_real,
                              BilinearFormIntegrator *bfi_imag);

   /// Adds new Boundary Integrator, restricted to specific boundary attributes.
   void AddBoundaryIntegrator(BilinearFormIntegrator *bfi_real,
                              BilinearFormIntegrator *bfi_imag,
                              Array<int> &bdr_marker);

   /// Assemble the local matrix
   void Assemble(int skip_zeros = 1);

   /// Finalizes the matrix initialization.
   void Finalize(int skip_zeros = 1);

   /// Returns the matrix assembled on the true dofs, i.e. P^t A P.
   /** The returned matrix has to be deleted by the caller. */
   ComplexHypreParMatrix *ParallelAssemble();

   /// Return the parallel FE space associated with the ParBilinearForm.
   ParFiniteElementSpace *ParFESpace() const { return pblfr_->ParFESpace(); }

   void FormLinearSystem(const Array<int> &ess_tdof_list, Vector &x, Vector &b,
                         OperatorHandle &A, Vector &X, Vector &B,
                         int copy_interior = 0);

   /** Call this method after solving a linear system constructed using the
       FormLinearSystem method to recover the solution as a ParGridFunction-size
       vector in x. Use the same arguments as in the FormLinearSystem call. */
   virtual void RecoverFEMSolution(const Vector &X, const Vector &b, Vector &x);

   virtual void Update(FiniteElementSpace *nfes = NULL);

   virtual ~ParSesquilinearForm();
};
  
#endif // MFEM_USE_MPI
  
}

#endif // MFEM_COMPLEX_FEM