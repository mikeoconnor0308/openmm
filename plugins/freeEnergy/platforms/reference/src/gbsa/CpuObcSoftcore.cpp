/* Portions copyright (c) 2006-2009 Stanford University and Simbios.
 * Contributors: Pande Group
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <sstream>
#include <stdlib.h>

#include <cmath>
#include <cstdio>
#include <vector>

#include "../SimTKUtilities/SimTKOpenMMCommon.h"
#include "../SimTKReference/ReferenceForce.h"
#include "CpuObcSoftcore.h"

using std::vector;
using OpenMM::RealVec;

/**---------------------------------------------------------------------------------------

   CpuObcSoftcore constructor

   obcSoftcoreParameters      obcSoftcoreParameters object
   
   --------------------------------------------------------------------------------------- */

CpuObcSoftcore::CpuObcSoftcore( ObcSoftcoreParameters* obcSoftcoreParameters ){
    _obcSoftcoreParameters   = obcSoftcoreParameters;
    _includeAceApproximation = 1;
    _obcChain.resize(_obcSoftcoreParameters->getNumberOfAtoms());
}

/**---------------------------------------------------------------------------------------

    CpuObcSoftcore destructor

    --------------------------------------------------------------------------------------- */

CpuObcSoftcore::~CpuObcSoftcore( ){
}

/**---------------------------------------------------------------------------------------

    Get ObcSoftcoreParameters reference

    @return ObcSoftcoreParameters reference

    --------------------------------------------------------------------------------------- */

ObcSoftcoreParameters* CpuObcSoftcore::getObcSoftcoreParameters( void ) const {
    return _obcSoftcoreParameters;
}

/**---------------------------------------------------------------------------------------

    Set ObcSoftcoreParameters reference

    @param ObcSoftcoreParameters reference

    --------------------------------------------------------------------------------------- */

void CpuObcSoftcore::setObcSoftcoreParameters( ObcSoftcoreParameters* obcSoftcoreParameters ){
    _obcSoftcoreParameters = obcSoftcoreParameters;
}

/**---------------------------------------------------------------------------------------

    Return OBC chain derivative: size = _obcSoftcoreParameters->getNumberOfAtoms()
    On first call, memory for array is allocated if not set

    @return array

    --------------------------------------------------------------------------------------- */

RealOpenMMVector& CpuObcSoftcore::getObcChain( void ){
    return _obcChain;
}

/**---------------------------------------------------------------------------------------

   Return flag signalling whether AceApproximation for nonpolar term is to be included

   @return flag

   --------------------------------------------------------------------------------------- */

int CpuObcSoftcore::includeAceApproximation( void ) const {
    return _includeAceApproximation;
}

/**---------------------------------------------------------------------------------------

   Set flag indicating whether AceApproximation is to be included

   @param includeAceApproximation new includeAceApproximation value

   --------------------------------------------------------------------------------------- */

void CpuObcSoftcore::setIncludeAceApproximation( int includeAceApproximation ){
    _includeAceApproximation = includeAceApproximation;
}

/**---------------------------------------------------------------------------------------

    Calculation of Born radii based on papers:

       J. Phys. Chem. 1996 100, 19824-19839 (HCT paper)
       Proteins: Structure, Function, and Bioinformatcis 55:383-394 (2004) (OBC paper)

    @param atomCoordinates     atomic coordinates
    @param bornRadii           output array of Born radii

    --------------------------------------------------------------------------------------- */

void CpuObcSoftcore::computeBornRadii( vector<RealVec>& atomCoordinates,  RealOpenMMVector& bornRadii ){

    // ---------------------------------------------------------------------------------------

    static const RealOpenMM zero    = static_cast<RealOpenMM>( 0.0 );
    static const RealOpenMM one     = static_cast<RealOpenMM>( 1.0 );
    static const RealOpenMM two     = static_cast<RealOpenMM>( 2.0 );
    static const RealOpenMM three   = static_cast<RealOpenMM>( 3.0 );
    static const RealOpenMM half    = static_cast<RealOpenMM>( 0.5 );
    static const RealOpenMM fourth  = static_cast<RealOpenMM>( 0.25 );

    // ---------------------------------------------------------------------------------------

    ObcSoftcoreParameters* obcSoftcoreParameters   = getObcSoftcoreParameters();

    int numberOfAtoms                              = obcSoftcoreParameters->getNumberOfAtoms();

    const RealOpenMMVector& atomicRadii            = obcSoftcoreParameters->getAtomicRadii();
    const RealOpenMMVector& scaledRadiusFactor     = obcSoftcoreParameters->getScaledRadiusFactors();
    RealOpenMMVector& obcChain                     = getObcChain();
    const RealOpenMMVector& nonPolarScaleFactors   = obcSoftcoreParameters->getNonPolarScaleFactors();

    RealOpenMM dielectricOffset                    = obcSoftcoreParameters->getDielectricOffset();
    RealOpenMM alphaObc                            = obcSoftcoreParameters->getAlphaObc();
    RealOpenMM betaObc                             = obcSoftcoreParameters->getBetaObc();
    RealOpenMM gammaObc                            = obcSoftcoreParameters->getGammaObc();

    // ---------------------------------------------------------------------------------------

    // calculate Born radii

    for( int atomI = 0; atomI < numberOfAtoms; atomI++ ){
      
        RealOpenMM radiusI         = atomicRadii[atomI];
        RealOpenMM offsetRadiusI   = radiusI - dielectricOffset;
 
        RealOpenMM radiusIInverse  = one/offsetRadiusI;
        RealOpenMM sum             = zero;
 
        // HCT code
 
        for( int atomJ = 0; atomJ < numberOfAtoms; atomJ++ ){
 
            if( atomJ != atomI ){
  
                RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
                if (_obcSoftcoreParameters->getPeriodic())
                    ReferenceForce::getDeltaRPeriodic( atomCoordinates[atomI], atomCoordinates[atomJ], _obcSoftcoreParameters->getPeriodicBox(), deltaR );
                else
                    ReferenceForce::getDeltaR( atomCoordinates[atomI], atomCoordinates[atomJ], deltaR );
                RealOpenMM r               = deltaR[ReferenceForce::RIndex];
                if (_obcSoftcoreParameters->getUseCutoff() && r > _obcSoftcoreParameters->getCutoffDistance())
                    continue;
   
                RealOpenMM offsetRadiusJ   = atomicRadii[atomJ] - dielectricOffset; 
                RealOpenMM scaledRadiusJ   = offsetRadiusJ*scaledRadiusFactor[atomJ];
                RealOpenMM rScaledRadiusJ  = r + scaledRadiusJ;
   
                if( offsetRadiusI < rScaledRadiusJ ){
                    RealOpenMM rInverse = one/r;
                    RealOpenMM l_ij     = offsetRadiusI > FABS( r - scaledRadiusJ ) ? offsetRadiusI : FABS( r - scaledRadiusJ );
                               l_ij     = one/l_ij;
     
                    RealOpenMM u_ij     = one/rScaledRadiusJ;
     
                    RealOpenMM l_ij2    = l_ij*l_ij;
                    RealOpenMM u_ij2    = u_ij*u_ij;
      
                    RealOpenMM ratio    = LN( (u_ij/l_ij) );
                    RealOpenMM term     = l_ij - u_ij + fourth*r*(u_ij2 - l_ij2)  + ( half*rInverse*ratio) + (fourth*scaledRadiusJ*scaledRadiusJ*rInverse)*(l_ij2 - u_ij2);
     
                    // this case (atom i completely inside atom j) is not considered in the original paper
                    // Jay Ponder and the authors of Tinker recognized this and
                    // worked out the details
     
                    if( offsetRadiusI < (scaledRadiusJ - r) ){
                        term += two*( radiusIInverse - l_ij);
                    }
                    sum += nonPolarScaleFactors[atomJ]*term;
                }
            }
        }
  
        // OBC-specific code (Eqs. 6-8 in paper)
 
        sum                  *= nonPolarScaleFactors[atomI]*half*offsetRadiusI;
        RealOpenMM sum2       = sum*sum;
        RealOpenMM sum3       = sum*sum2;
        RealOpenMM tanhSum    = TANH( alphaObc*sum - betaObc*sum2 + gammaObc*sum3 );
        
        bornRadii[atomI]      = one/( one/offsetRadiusI - tanhSum/radiusI ); 
  
        obcChain[atomI]       = offsetRadiusI*( alphaObc - two*betaObc*sum + three*gammaObc*sum2 );
        obcChain[atomI]       = (one - tanhSum*tanhSum)*obcChain[atomI]/radiusI;

    }

    return;
}

/**---------------------------------------------------------------------------------------

    Get nonpolar solvation force constribution via ACE approximation

    @param obcSoftcoreParameters     parameters
    @param vdwRadii                  Vdw radii
    @param bornRadii                 Born radii
    @param energy                    energy (output): value is incremented from input value 
    @param forces                    forces: values are incremented from input values

    --------------------------------------------------------------------------------------- */

void CpuObcSoftcore::computeAceNonPolarForce( const ObcSoftcoreParameters* obcSoftcoreParameters,
                                              const vector<RealOpenMM>& bornRadii, RealOpenMM* energy,
                                              vector<RealOpenMM>& forces ) const {

    static const RealOpenMM minusSix = -6.0;

    // ---------------------------------------------------------------------------------------

    // compute the nonpolar solvation via ACE approximation

    const RealOpenMM probeRadius                  = obcSoftcoreParameters->getProbeRadius();
    const RealOpenMM surfaceAreaFactor            = obcSoftcoreParameters->getPi4Asolv();

    const RealOpenMMVector& atomicRadii           = obcSoftcoreParameters->getAtomicRadii();
    const RealOpenMMVector& nonPolarScaleFactors  = obcSoftcoreParameters->getNonPolarScaleFactors();

    int numberOfAtoms                             = obcSoftcoreParameters->getNumberOfAtoms();

    // the original ACE equation is based on Eq.2 of

    // M. Schaefer, C. Bartels and M. Karplus, "Solution Conformations
    // and Thermodynamics of Structured Peptides: Molecular Dynamics
    // Simulation with an Implicit Solvation Model", J. Mol. Biol.,
    // 284, 835-848 (1998)  (ACE Method)

    // The original equation includes the factor (atomicRadii[atomI]/bornRadii[atomI]) to the first power,
    // whereas here the ratio is raised to the sixth power: (atomicRadii[atomI]/bornRadii[atomI])**6

    // This modification was made by Jay Ponder and is based on observations that the change yields better correlations w/
    // expected values. Jay did not think it was important enough to write up, so there is
    // no paper to cite.

    for( int atomI = 0; atomI < numberOfAtoms; atomI++ ){
        if( bornRadii[atomI] > 0.0 ){
            RealOpenMM r            = atomicRadii[atomI] + probeRadius;
            RealOpenMM ratio6       = POW( atomicRadii[atomI]/bornRadii[atomI], static_cast<RealOpenMM>( 6.0 ) );
            RealOpenMM saTerm       = nonPolarScaleFactors[atomI]*surfaceAreaFactor*r*r*ratio6;
            *energy                += saTerm;
            forces[atomI]          += minusSix*saTerm/bornRadii[atomI]; 
        }
    }
}

/**---------------------------------------------------------------------------------------

    Get Obc Born energy and forces

    @param atomCoordinates     atomic coordinates
    @param partialCharges      partial charges
    @param forces              forces

    The array bornRadii is also updated and the obcEnergy

    --------------------------------------------------------------------------------------- */

RealOpenMM CpuObcSoftcore::computeBornEnergyForces( vector<RealVec>& atomCoordinates,
                                                    const RealOpenMMVector& partialCharges,
                                                    vector<RealVec>& inputForces ){

    // ---------------------------------------------------------------------------------------

    static const RealOpenMM zero    = static_cast<RealOpenMM>( 0.0 );
    static const RealOpenMM one     = static_cast<RealOpenMM>( 1.0 );
    static const RealOpenMM two     = static_cast<RealOpenMM>( 2.0 );
    static const RealOpenMM three   = static_cast<RealOpenMM>( 3.0 );
    static const RealOpenMM four    = static_cast<RealOpenMM>( 4.0 );
    static const RealOpenMM half    = static_cast<RealOpenMM>( 0.5 );
    static const RealOpenMM fourth  = static_cast<RealOpenMM>( 0.25 );
    static const RealOpenMM eighth  = static_cast<RealOpenMM>( 0.125 );

    // ---------------------------------------------------------------------------------------

    const ObcSoftcoreParameters* obcSoftcoreParameters = getObcSoftcoreParameters();
    const int numberOfAtoms                            = obcSoftcoreParameters->getNumberOfAtoms();

    // ---------------------------------------------------------------------------------------

    // constants

    const RealOpenMM preFactor                         = two*obcSoftcoreParameters->getElectricConstant()*( 
                                                         (one/obcSoftcoreParameters->getSoluteDielectric()) -
                                                         (one/obcSoftcoreParameters->getSolventDielectric()) );

    const RealOpenMM dielectricOffset                  = obcSoftcoreParameters->getDielectricOffset();

    // ---------------------------------------------------------------------------------------

    // compute Born radii

    RealOpenMMVector bornRadii( numberOfAtoms );
    computeBornRadii( atomCoordinates,  bornRadii );

    RealOpenMM obcEnergy                 = zero;
    RealOpenMMVector bornForces( numberOfAtoms );
    for( int ii = 0; ii < numberOfAtoms; ii++ ){
       bornForces[ii] = zero;
    }

    // ---------------------------------------------------------------------------------------

    // N*( 8 + pow) ACE
    // compute the nonpolar solvation via ACE approximation
     
    if( includeAceApproximation() ){
       computeAceNonPolarForce( obcSoftcoreParameters, bornRadii, &obcEnergy, bornForces );
    }
 
    const RealOpenMMVector& nonPolarScaleFactors   = obcSoftcoreParameters->getNonPolarScaleFactors();

    // ---------------------------------------------------------------------------------------

    // first main loop

    for( int atomI = 0; atomI < numberOfAtoms; atomI++ ){
 
       RealOpenMM partialChargeI = preFactor*partialCharges[atomI];
       for( int atomJ = atomI; atomJ < numberOfAtoms; atomJ++ ){

          RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
          if (_obcSoftcoreParameters->getPeriodic())
              ReferenceForce::getDeltaRPeriodic( atomCoordinates[atomI], atomCoordinates[atomJ], _obcSoftcoreParameters->getPeriodicBox(), deltaR );
          else
              ReferenceForce::getDeltaR( atomCoordinates[atomI], atomCoordinates[atomJ], deltaR );
          if (_obcSoftcoreParameters->getUseCutoff() && deltaR[ReferenceForce::RIndex] > _obcSoftcoreParameters->getCutoffDistance())
              continue;
          RealOpenMM r2                     = deltaR[ReferenceForce::R2Index];
          RealOpenMM deltaX                 = deltaR[ReferenceForce::XIndex];
          RealOpenMM deltaY                 = deltaR[ReferenceForce::YIndex];
          RealOpenMM deltaZ                 = deltaR[ReferenceForce::ZIndex];

          RealOpenMM alpha2_ij              = bornRadii[atomI]*bornRadii[atomJ];
          RealOpenMM D_ij                   = r2/(four*alpha2_ij);

          RealOpenMM expTerm                = EXP( -D_ij );
          RealOpenMM denominator2           = r2 + alpha2_ij*expTerm; 
          RealOpenMM denominator            = SQRT( denominator2 ); 

          // charges are assumed to be scaled on input so nonPolarScaleFactor is not needed

          RealOpenMM Gpol                   = (partialChargeI*partialCharges[atomJ])/denominator; 

          RealOpenMM dGpol_dalpha2_ij       = -half*Gpol*expTerm*( one + D_ij )/denominator2;

          RealOpenMM dGpol_dr               = -Gpol*( one - fourth*expTerm )/denominator2;  

          if( atomI != atomJ ){

              bornForces[atomJ] += dGpol_dalpha2_ij*bornRadii[atomI];

              deltaX            *= dGpol_dr;
              deltaY            *= dGpol_dr;
              deltaZ            *= dGpol_dr;

              inputForces[atomI][0]  += deltaX;
              inputForces[atomI][1]  += deltaY;
              inputForces[atomI][2]  += deltaZ;

              inputForces[atomJ][0]  -= deltaX;
              inputForces[atomJ][1]  -= deltaY;
              inputForces[atomJ][2]  -= deltaZ;

          } else {
             Gpol *= half;
          }

          obcEnergy         += Gpol;
          bornForces[atomI] += dGpol_dalpha2_ij*bornRadii[atomJ];

       }
    }

    // ---------------------------------------------------------------------------------------

    // second main loop

    RealOpenMMVector& obcChain                  = getObcChain();
    const RealOpenMMVector& atomicRadii         = obcSoftcoreParameters->getAtomicRadii();

    const RealOpenMM alphaObc                   = obcSoftcoreParameters->getAlphaObc();
    const RealOpenMM betaObc                    = obcSoftcoreParameters->getBetaObc();
    const RealOpenMM gammaObc                   = obcSoftcoreParameters->getGammaObc();
    const RealOpenMMVector& scaledRadiusFactor  = obcSoftcoreParameters->getScaledRadiusFactors();

     // compute factor that depends only on the outer loop index

    for( int atomI = 0; atomI < numberOfAtoms; atomI++ ){
       bornForces[atomI] *= bornRadii[atomI]*bornRadii[atomI]*obcChain[atomI];      
    }

    for( int atomI = 0; atomI < numberOfAtoms; atomI++ ){
 
       // radius w/ dielectric offset applied

       RealOpenMM radiusI        = atomicRadii[atomI];
       RealOpenMM offsetRadiusI  = radiusI - dielectricOffset;

       for( int atomJ = 0; atomJ < numberOfAtoms; atomJ++ ){

          if( atomJ != atomI ){

             RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
             if (_obcSoftcoreParameters->getPeriodic())
                ReferenceForce::getDeltaRPeriodic( atomCoordinates[atomI], atomCoordinates[atomJ], _obcSoftcoreParameters->getPeriodicBox(), deltaR );
             else 
                ReferenceForce::getDeltaR( atomCoordinates[atomI], atomCoordinates[atomJ], deltaR );
             if (_obcSoftcoreParameters->getUseCutoff() && deltaR[ReferenceForce::RIndex] > _obcSoftcoreParameters->getCutoffDistance())
                    continue;
    
             RealOpenMM deltaX             = deltaR[ReferenceForce::XIndex];
             RealOpenMM deltaY             = deltaR[ReferenceForce::YIndex];
             RealOpenMM deltaZ             = deltaR[ReferenceForce::ZIndex];
             RealOpenMM r                  = deltaR[ReferenceForce::RIndex];
 
             // radius w/ dielectric offset applied

             RealOpenMM offsetRadiusJ      = atomicRadii[atomJ] - dielectricOffset;

             RealOpenMM scaledRadiusJ      = offsetRadiusJ*scaledRadiusFactor[atomJ];
             RealOpenMM scaledRadiusJ2     = scaledRadiusJ*scaledRadiusJ;
             RealOpenMM rScaledRadiusJ     = r + scaledRadiusJ;

             // dL/dr & dU/dr are zero (this can be shown analytically)
             // removed from calculation

             if( offsetRadiusI < rScaledRadiusJ ){

                RealOpenMM l_ij          = offsetRadiusI > FABS( r - scaledRadiusJ ) ? offsetRadiusI : FABS( r - scaledRadiusJ );
                     l_ij                = one/l_ij;

                RealOpenMM u_ij          = one/rScaledRadiusJ;

                RealOpenMM l_ij2         = l_ij*l_ij;

                RealOpenMM u_ij2         = u_ij*u_ij;
 
                RealOpenMM rInverse      = one/r;
                RealOpenMM r2Inverse     = rInverse*rInverse;

                RealOpenMM t3            = eighth*(one + scaledRadiusJ2*r2Inverse)*(l_ij2 - u_ij2) + fourth*LN( u_ij/l_ij )*r2Inverse;
                           t3           *= nonPolarScaleFactors[atomI]*nonPolarScaleFactors[atomJ];

                RealOpenMM de            = bornForces[atomI]*t3*rInverse;

                deltaX                  *= de;
                deltaY                  *= de;
                deltaZ                  *= de;
    
                inputForces[atomI][0]   -= deltaX;
                inputForces[atomI][1]   -= deltaY;
                inputForces[atomI][2]   -= deltaZ;
  
                inputForces[atomJ][0]   += deltaX;
                inputForces[atomJ][1]   += deltaY;
                inputForces[atomJ][2]   += deltaZ;
 
             }
          }
       }
    }

    return obcEnergy;;

}
