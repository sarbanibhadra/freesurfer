#ifndef __kvlAtlasMeshPositionGradientCalculator_h
#define __kvlAtlasMeshPositionGradientCalculator_h

#include "kvlAtlasMeshRasterizor.h"
#include "vnl/vnl_inverse.h"
#include "vnl/vnl_matrix_fixed.h"


namespace kvl
{


namespace FragmentProcessor 
{

/**
 *
 */
class CalculatePositionGradient
{
public:
  
  CalculatePositionGradient() 
    {
    m_SourcePointer = 0;
    
    m_Mesh = 0;
    m_GradientInVertex0 = 0;
    m_GradientInVertex1 = 0;
    m_GradientInVertex2 = 0;
    m_GradientInVertex3 = 0;
    m_MinLogLikelihoodTimesPrior = 0;

    m_IgnoreDeformationPrior = false;

    m_mapCompToComp = 0;

    }

  ~CalculatePositionGradient() {};

  inline void operator()( const float& pi0, const float& pi1, const float& pi2, const float& pi3 )
    {
   
    float tmpX = 0;
    float tmpY = 0;
    float tmpZ = 0;

    if ( m_mapCompToComp == 0) // no collapsed labels
      {

      // Collect the data terms for each vertex
      float  alpha0 = m_AlphasInVertex0[ *m_SourcePointer ];
      float  alpha1 = m_AlphasInVertex1[ *m_SourcePointer ];
      float  alpha2 = m_AlphasInVertex2[ *m_SourcePointer ];
      float  alpha3 = m_AlphasInVertex3[ *m_SourcePointer ];
    
      // Add contribution of the likelihood
      float  likelihood = alpha0 * pi0 + alpha1 * pi1 + alpha2 * pi2 + alpha3 * pi3 + 1e-15;

      if (std::isnan(likelihood))  // Eugenio added this check
        {
          m_MinLogLikelihoodTimesPrior = itk::NumericTraits< double >::max();
          tmpX = 0; tmpY=0; tmpZ=0;
        }
      else
        {
          m_MinLogLikelihoodTimesPrior -= log( likelihood );
  
          tmpX = ( m_XGradientBasis[ *m_SourcePointer ] ) / likelihood;
          tmpY = ( m_YGradientBasis[ *m_SourcePointer ] ) / likelihood;
          tmpZ = ( m_ZGradientBasis[ *m_SourcePointer ] ) / likelihood;
        }

      }
    else // version with collapsed labels
      {
      float  likelihood =  1e-15;
      for(int ind=0; ind<m_mapCompToComp[*m_SourcePointer].size(); ind++)
        {
        const unsigned char k = m_mapCompToComp[*m_SourcePointer][ind];

        float  alpha0 = m_AlphasInVertex0[ k ];
        float  alpha1 = m_AlphasInVertex1[ k ];
        float  alpha2 = m_AlphasInVertex2[ k ];
        float  alpha3 = m_AlphasInVertex3[ k ];

        likelihood += alpha0 * pi0 + alpha1 * pi1 + alpha2 * pi2 + alpha3 * pi3;

        tmpX += ( m_XGradientBasis[ k ] ); 
        tmpY += ( m_YGradientBasis[ k ] );
        tmpZ += ( m_ZGradientBasis[ k ] );
        }

       if (std::isnan(likelihood))  // Eugenio added this check
        {
          m_MinLogLikelihoodTimesPrior = itk::NumericTraits< double >::max();
          tmpX = 0; tmpY=0; tmpZ=0;
        }
      else
        {
          m_MinLogLikelihoodTimesPrior -= log( likelihood );
  
          tmpX = tmpX / likelihood; 
          tmpY = tmpY / likelihood; 
          tmpZ = tmpZ / likelihood; 
        }

      }
     
    // Add contribution to gradient in vertex 0
    AtlasPositionGradientType  gradientContributionToVertex0;
    gradientContributionToVertex0[ 0 ] = tmpX * pi0;
    gradientContributionToVertex0[ 1 ] = tmpY * pi0;
    gradientContributionToVertex0[ 2 ] = tmpZ * pi0;
    *m_GradientInVertex0 += gradientContributionToVertex0;

    // Add contribution to gradient in vertex 0
    AtlasPositionGradientType  gradientContributionToVertex1;
    gradientContributionToVertex1[ 0 ] = tmpX * pi1;
    gradientContributionToVertex1[ 1 ] = tmpY * pi1;
    gradientContributionToVertex1[ 2 ] = tmpZ * pi1;
    *m_GradientInVertex1 += gradientContributionToVertex1;

    // Add contribution to gradient in vertex 2
    AtlasPositionGradientType  gradientContributionToVertex2;
    gradientContributionToVertex2[ 0 ] = tmpX * pi2;
    gradientContributionToVertex2[ 1 ] = tmpY * pi2;
    gradientContributionToVertex2[ 2 ] = tmpZ * pi2;
    *m_GradientInVertex2 += gradientContributionToVertex2;

    // Add contribution to gradient in vertex 3
    AtlasPositionGradientType  gradientContributionToVertex3;
    gradientContributionToVertex3[ 0 ] = tmpX * pi3;
    gradientContributionToVertex3[ 1 ] = tmpY * pi3;
    gradientContributionToVertex3[ 2 ] = tmpZ * pi3;
    *m_GradientInVertex3 += gradientContributionToVertex3;

              
    // Move on to the next pixel
    m_SourcePointer++;
    }
    
  inline void StartNewSpan( int x, int y, int z, const unsigned char* sourcePointer )
    {
    m_SourcePointer = sourcePointer;


    }
    
  inline bool StartNewTetrahedron( AtlasMesh::CellIdentifier cellId )
    {
    //
    // Cache relevant elements of the vertices of this triangle. The notation used is Y = [ p0 p1 p2 p3; 1 1 1 1 ]
    //
    AtlasMesh::CellAutoPointer  cell;
    m_Mesh->GetCell( cellId, cell );
          
    AtlasMesh::CellType::PointIdIterator  pit = cell->PointIdsBegin();
    AtlasMesh::PointType  p;
    m_Mesh->GetPoint( *pit, &p );
    const float y11 = p[ 0 ];
    const float y21 = p[ 1 ];
    const float y31 = p[ 2 ];
    m_AlphasInVertex0 = m_Mesh->GetPointData()->ElementAt( *pit ).m_Alphas;
    m_GradientInVertex0 = &( m_PositionGradient->ElementAt( *pit ) );

#if 0
    m_DebugP0 = p;
#endif

    ++pit;
    m_Mesh->GetPoint( *pit, &p );
    const float y12 = p[ 0 ];
    const float y22 = p[ 1 ];
    const float y32 = p[ 2 ];
    m_AlphasInVertex1 = m_Mesh->GetPointData()->ElementAt( *pit ).m_Alphas;
    m_GradientInVertex1 = &( m_PositionGradient->ElementAt( *pit ) );

#if 0
    m_DebugP1 = p;
#endif

    ++pit;
    m_Mesh->GetPoint( *pit, &p );
    const float y13 = p[ 0 ];
    const float y23 = p[ 1 ];
    const float y33 = p[ 2 ];
    m_AlphasInVertex2 = m_Mesh->GetPointData()->ElementAt( *pit ).m_Alphas;
    m_GradientInVertex2 = &( m_PositionGradient->ElementAt( *pit ) );

#if 0
    m_DebugP2 = p;
#endif

    ++pit;
    m_Mesh->GetPoint( *pit, &p );
    const float y14 = p[ 0 ];
    const float y24 = p[ 1 ];
    const float y34 = p[ 2 ];
    m_AlphasInVertex3 = m_Mesh->GetPointData()->ElementAt( *pit ).m_Alphas;
    m_GradientInVertex3 = &( m_PositionGradient->ElementAt( *pit ) );

#if 0
    m_DebugP3 = p;
#endif

    if ( !m_IgnoreDeformationPrior )
      {
      //
      // Retrieve reference triangle info components. Z is inv( [ p0 p1 p2 p3; 1 1 1 1 ] ) of the
      // tetrahedron in reference position
      //
      ReferenceTetrahedronInfo  info;
      m_Mesh->GetCellData( cellId, &info );

      const float  referenceVolumeTimesK = info.m_ReferenceVolumeTimesK;

      const float  z11 = info.m_Z11;
      const float  z21 = info.m_Z21;
      const float  z31 = info.m_Z31;
      const float  z41 = info.m_Z41;

      const float  z12 = info.m_Z12;
      const float  z22 = info.m_Z22;
      const float  z32 = info.m_Z32;
      const float  z42 = info.m_Z42;

      const float  z13 = info.m_Z13;
      const float  z23 = info.m_Z23;
      const float  z33 = info.m_Z33;
      const float  z43 = info.m_Z43;


      //
      // Now let's add Ashburner's prior cost for the tethrahedron deformation from its reference position
      //
      const float  m11 = z11*y11 + z21*y12 + z31*y13 + z41*y14;
      const float  m21 = z11*y21 + z21*y22 + z31*y23 + z41*y24;
      const float  m31 = z11*y31 + z21*y32 + z31*y33 + z41*y34;
      const float  m12 = z12*y11 + z22*y12 + z32*y13 + z42*y14;
      const float  m22 = z12*y21 + z22*y22 + z32*y23 + z42*y24;
      const float  m32 = z12*y31 + z22*y32 + z32*y33 + z42*y34;
      const float  m13 = z13*y11 + z23*y12 + z33*y13 + z43*y14;
      const float  m23 = z13*y21 + z23*y22 + z33*y23 + z43*y24;
      const float  m33 = z13*y31 + z23*y32 + z33*y33 + z43*y34;

      const float  detJ = m11 * ( m22*m33 - m32*m23 ) - m12 * ( m21*m33 - m31*m23 ) + m13 * ( m21*m32 - m31*m22 );
      if ( detJ <= 0 )
        {
        //// std::cout << "Oooooops: tetrahedron " << cellId << " has managed to turn bad (detJ: " << detJ << ")" << std::endl;
        m_MinLogLikelihoodTimesPrior = itk::NumericTraits< float >::max();
        //// std::cout << "PositionGradientCalculator: setting m_MinLogLikelihoodTimesPrior to: " << m_MinLogLikelihoodTimesPrior << std::endl;
        return false;
        }
      


      // Let's define K as inv( J ) * det( J )
      const float  k11 = ( m22*m33 - m23*m32 );
      const float  k12 = -( m12*m33 - m32*m13 );
      const float  k13 = ( m12*m23 - m22*m13 );
      const float  k21 = -( m21*m33 - m31*m23 );
      const float  k22 = ( m11*m33 - m13*m31 );
      const float  k23 = -( m11*m23 - m21*m13 );
      const float  k31 = ( m21*m32 - m31*m22 );
      const float  k32 = -( m11*m32 - m31*m12 ); 
      const float  k33 = ( m11*m22 - m12*m21 );

      // Trace of J' * J is actually the sum of the squares of the singular values of J: s1^2 + s2^2 + s3^2
      const float  sumOfSquaresOfSingularValuesOfJ = m11*m11 + m12*m12 + m13*m13 + m21*m21 + m22*m22 + m23*m23 + m31*m31 + m32*m32 + m33*m33;

      // Trace of ( inv(J) )' * inv( J ) is actually the sum of the squares of the reciprocals of the singular values
      // of J: 1/s1^2 + 1/s2^2 + 1/s3^2
      const float  traceOfKTransposeTimesK = ( k11*k11 + k12*k12 + k13*k13 + k21*k21 + k22*k22 + k23*k23 + k31*k31 + k32*k32 + k33*k33 );
      const float  sumOfSquaresOfReciprocalsOfSingularValuesOfJ = traceOfKTransposeTimesK / ( detJ * detJ );

      const float  priorCost = referenceVolumeTimesK * ( 1 + detJ ) *
                              ( sumOfSquaresOfSingularValuesOfJ + sumOfSquaresOfReciprocalsOfSingularValuesOfJ - 6 );
      m_MinLogLikelihoodTimesPrior += priorCost;
      //// std::cout << "PositionGradientCalculator: setting m_MinLogLikelihoodTimesPrior to: " << m_MinLogLikelihoodTimesPrior << std::endl;



      //
      // OK, now add contribution to derivatives of Ashburner's prior cost in each of the tetrahedron's vertices
      //
      const float  ddetJdm11 = m22*m33 - m32*m23;
      const float  ddetJdm21 = m13*m32 - m12*m33;
      const float  ddetJdm31 = m12*m23 - m13*m22;
      const float  ddetJdm12 = m31*m23 - m21*m33;
      const float  ddetJdm22 = m11*m33 - m13*m31;
      const float  ddetJdm32 = m13*m21 - m11*m23;
      const float  ddetJdm13 = m21*m32 - m31*m22;
      const float  ddetJdm23 = m12*m31 - m11*m32;
      const float  ddetJdm33 = m11*m22 - m12*m21;


      const float  tmp1 = referenceVolumeTimesK * ( sumOfSquaresOfSingularValuesOfJ + sumOfSquaresOfReciprocalsOfSingularValuesOfJ - 6 );
      const float  tmp2 = 2 * referenceVolumeTimesK * ( 1 + detJ );
      const float  tmp3 = pow( detJ,  3 );


      const float  dcostdm11 = tmp1 * ddetJdm11 + tmp2 * ( m11 + ( ( k22*m33 - k23*m23 - k32*m32 + k33*m22 ) * detJ - traceOfKTransposeTimesK * ddetJdm11 ) / tmp3 ); 
      const float  dcostdm21 = tmp1 * ddetJdm21 + tmp2 * ( m21 + ( ( -k21*m33 + k23*m13 + k31*m32 - k33*m12 ) * detJ - traceOfKTransposeTimesK * ddetJdm21 ) / tmp3 ); 
      const float  dcostdm31 = tmp1 * ddetJdm31 + tmp2 * ( m31 + ( ( k21*m23 - k22*m13 - k31*m22 + k32*m12 ) * detJ - traceOfKTransposeTimesK * ddetJdm31 ) / tmp3 ); 

      const float  dcostdm12 = tmp1 * ddetJdm12 + tmp2 * ( m12 + ( ( -k12*m33 + k13*m23 + k32*m31 - k33*m21 ) * detJ - traceOfKTransposeTimesK * ddetJdm12 ) / tmp3 ); 
      const float  dcostdm22 = tmp1 * ddetJdm22 + tmp2 * ( m22 + ( ( k11*m33 - k13*m13 - k31*m31 + k33*m11 ) * detJ - traceOfKTransposeTimesK * ddetJdm22 ) / tmp3 ); 
      const float  dcostdm32 = tmp1 * ddetJdm32 + tmp2 * ( m32 + ( ( -k11*m23 + k12*m13 + k31*m21 - k32*m11 ) * detJ - traceOfKTransposeTimesK * ddetJdm32 ) / tmp3 ); 

      const float  dcostdm13 = tmp1 * ddetJdm13 + tmp2 * ( m13 + ( ( k12*m32 - k13*m22 - k22*m31 + k23*m21 ) * detJ - traceOfKTransposeTimesK * ddetJdm13 ) / tmp3 ); 
      const float  dcostdm23 = tmp1 * ddetJdm23 + tmp2 * ( m23 + ( ( -k11*m32 + k13*m12 + k21*m31 -k23*m11 ) * detJ - traceOfKTransposeTimesK * ddetJdm23 ) / tmp3 ); 
      const float  dcostdm33 = tmp1 * ddetJdm33 + tmp2 * ( m33 + ( ( k11*m22 - k12*m12 - k21*m21 + k22*m11 ) * detJ - traceOfKTransposeTimesK * ddetJdm33 ) / tmp3 ); 


      const float  dcostdy11 = dcostdm11*z11 + dcostdm12*z12 + dcostdm13*z13;
      const float  dcostdy21 = dcostdm21*z11 + dcostdm22*z12 + dcostdm23*z13;
      const float  dcostdy31 = dcostdm31*z11 + dcostdm32*z12 + dcostdm33*z13;

      const float  dcostdy12 = dcostdm11*z21 + dcostdm12*z22 + dcostdm13*z23;
      const float  dcostdy22 = dcostdm21*z21 + dcostdm22*z22 + dcostdm23*z23;
      const float  dcostdy32 = dcostdm31*z21 + dcostdm32*z22 + dcostdm33*z23;

      const float  dcostdy13 = dcostdm11*z31 + dcostdm12*z32 + dcostdm13*z33;
      const float  dcostdy23 = dcostdm21*z31 + dcostdm22*z32 + dcostdm23*z33;
      const float  dcostdy33 = dcostdm31*z31 + dcostdm32*z32 + dcostdm33*z33;

      const float  dcostdy14 = dcostdm11*z41 + dcostdm12*z42 + dcostdm13*z43;
      const float  dcostdy24 = dcostdm21*z41 + dcostdm22*z42 + dcostdm23*z43;
      const float  dcostdy34 = dcostdm31*z41 + dcostdm32*z42 + dcostdm33*z43;


      // Add the stuff to the existing gradients in each of the tetrahedron's four vertices
      AtlasPositionGradientType  gradientContributionToVertex0;
      gradientContributionToVertex0[ 0 ] = dcostdy11;
      gradientContributionToVertex0[ 1 ] = dcostdy21;
      gradientContributionToVertex0[ 2 ] = dcostdy31;
      *m_GradientInVertex0 += gradientContributionToVertex0;
      
      AtlasPositionGradientType  gradientContributionToVertex1;
      gradientContributionToVertex1[ 0 ] = dcostdy12;
      gradientContributionToVertex1[ 1 ] = dcostdy22;
      gradientContributionToVertex1[ 2 ] = dcostdy32;
      *m_GradientInVertex1 += gradientContributionToVertex1;

      AtlasPositionGradientType  gradientContributionToVertex2;
      gradientContributionToVertex2[ 0 ] = dcostdy13;
      gradientContributionToVertex2[ 1 ] = dcostdy23;
      gradientContributionToVertex2[ 2 ] = dcostdy33;
      *m_GradientInVertex2 += gradientContributionToVertex2;

      AtlasPositionGradientType  gradientContributionToVertex3;
      gradientContributionToVertex3[ 0 ] = dcostdy14;
      gradientContributionToVertex3[ 1 ] = dcostdy24;
      gradientContributionToVertex3[ 2 ] = dcostdy34;
      *m_GradientInVertex3 += gradientContributionToVertex3;
      }



    //
    // Finally, precalculate some stuff that will be used over and over again, each time a voxel is visited
    //

    // Get Gamma, defined as Gamma = [ 0 1 0 0; 0 0 1 0; 0 0 0 1; 1 1 1 1] * inv( Y )
    // where Y = [ p0 p1 p2 p3; 1 1 1 1 ]
    vnl_matrix_fixed< float, 4, 4 >  Y;
    Y.put( 0, 0, y11 );
    Y.put( 1, 0, y21 );
    Y.put( 2, 0, y31 );
    Y.put( 3, 0, 1.0f );

    Y.put( 0, 1, y12 );
    Y.put( 1, 1, y22 );
    Y.put( 2, 1, y32 );
    Y.put( 3, 1, 1.0f );

    Y.put( 0, 2, y13 );
    Y.put( 1, 2, y23 );
    Y.put( 2, 2, y33 );
    Y.put( 3, 2, 1.0f );

    Y.put( 0, 3, y14 );
    Y.put( 1, 3, y24 );
    Y.put( 2, 3, y34 );
    Y.put( 3, 3, 1.0f );

    vnl_matrix_fixed< float, 4, 4 >  invY = vnl_inverse( Y );
    const float  gamma11 = invY( 1, 0 );
    const float  gamma12 = invY( 1, 1 );
    const float  gamma13 = invY( 1, 2 );
    const float  gamma21 = invY( 2, 0 );
    const float  gamma22 = invY( 2, 1 );
    const float  gamma23 = invY( 2, 2 );
    const float  gamma31 = invY( 3, 0 );
    const float  gamma32 = invY( 3, 1 );
    const float  gamma33 = invY( 3, 2 );


    m_XGradientBasis = AtlasAlphasType( m_AlphasInVertex0.Size() );
    m_YGradientBasis = AtlasAlphasType( m_AlphasInVertex0.Size() );
    m_ZGradientBasis = AtlasAlphasType( m_AlphasInVertex0.Size() );
    for ( unsigned int labelNumber = 0; labelNumber < m_AlphasInVertex0.Size(); labelNumber++ )
      {
      float  alpha0 = m_AlphasInVertex0[ labelNumber ];
      float  alpha1 = m_AlphasInVertex1[ labelNumber ];
      float  alpha2 = m_AlphasInVertex2[ labelNumber ];
      float  alpha3 = m_AlphasInVertex3[ labelNumber ];
  

      m_XGradientBasis[ labelNumber ] = alpha1 * gamma11 + alpha2 * gamma21 + alpha3 * gamma31 - alpha0 * ( gamma11 + gamma21 + gamma31 );
      m_YGradientBasis[ labelNumber ] = alpha1 * gamma12 + alpha2 * gamma22 + alpha3 * gamma32 - alpha0 * ( gamma12 + gamma22 + gamma32 );
      m_ZGradientBasis[ labelNumber ] = alpha1 * gamma13 + alpha2 * gamma23 + alpha3 * gamma33 - alpha0 * ( gamma13 + gamma23 + gamma33 );
      }



    return true;
    }
    
  inline void SetMesh( const AtlasMesh* mesh )
    {
    m_Mesh = mesh;
    
    // Create a container to hold the position gradient
    m_PositionGradient = AtlasPositionGradientContainerType::New();
    
    // Initialize to zero
    AtlasPositionGradientType  zeroEntry( 0.0f );
    
    AtlasMesh::PointsContainer::ConstIterator pointIt = m_Mesh->GetPoints()->Begin();
    while ( pointIt != m_Mesh->GetPoints()->End() )
      {
      m_PositionGradient->InsertElement( pointIt.Index(), zeroEntry );
      ++pointIt;
      }
      
    m_MinLogLikelihoodTimesPrior = 0.0f;
    }
    
  //
  void SetIgnoreDeformationPrior( bool ignoreDeformationPrior )
    { m_IgnoreDeformationPrior = ignoreDeformationPrior; }

  const AtlasPositionGradientContainerType* GetPositionGradient() const
    {
    return m_PositionGradient;
    }

  AtlasPositionGradientContainerType* GetPositionGradient()
    {
    return m_PositionGradient;
    }
  
  float GetMinLogLikelihoodTimesPrior() const
    {
    //// std::cout << "PositionGradientCalculator: returning m_MinLogLikelihoodTimesPrior " << m_MinLogLikelihoodTimesPrior << std::endl;
    return m_MinLogLikelihoodTimesPrior;
    }

  void SetMapCompToComp( std::vector<unsigned char > *mapCompToComp )
    { m_mapCompToComp = mapCompToComp; } 
  std::vector<unsigned char > * GetMapCompToComp()
    { return m_mapCompToComp; }
        
private:

  const unsigned char*  m_SourcePointer;
    
  AtlasAlphasType  m_AlphasInVertex0;
  AtlasAlphasType  m_AlphasInVertex1;
  AtlasAlphasType  m_AlphasInVertex2;
  AtlasAlphasType  m_AlphasInVertex3;
  
  AtlasPositionGradientType*  m_GradientInVertex0;
  AtlasPositionGradientType*  m_GradientInVertex1;
  AtlasPositionGradientType*  m_GradientInVertex2;
  AtlasPositionGradientType*  m_GradientInVertex3;
    
  AtlasMesh::ConstPointer  m_Mesh;
  AtlasPositionGradientContainerType::Pointer  m_PositionGradient;

  AtlasAlphasType  m_XGradientBasis;
  AtlasAlphasType  m_YGradientBasis;
  AtlasAlphasType  m_ZGradientBasis;

  float  m_MinLogLikelihoodTimesPrior;

  bool  m_IgnoreDeformationPrior;

  std::vector<unsigned char > *m_mapCompToComp;


};

 
 

} // End namespace FragmentProcessor





/**
 *
 */
class AtlasMeshPositionGradientCalculator : 
  public AtlasMeshRasterizor< FragmentProcessor::CalculatePositionGradient >
{
public :
  
  /** Standard class typedefs */
  typedef AtlasMeshPositionGradientCalculator  Self;
  typedef AtlasMeshRasterizor< FragmentProcessor::CalculatePositionGradient >  Superclass;
  typedef itk::SmartPointer< Self >  Pointer;
  typedef itk::SmartPointer< const Self >  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro( Self );

  /** Run-time type information (and related methods). */
  itkTypeMacro( AtlasMeshPositionGradientCalculator, itk::Object );

  /** Some typedefs */
  typedef Superclass::FragmentProcessorType  FragmentProcessorType;
  typedef Superclass::LabelImageType  LabelImageType;
      

  /** */
  void  SetMapCompToComp( std::vector<unsigned char > *mapCompToComp )
    {
    for ( std::vector< FragmentProcessorType >::iterator  it = this->GetFragmentProcessors().begin();
          it != this->GetFragmentProcessors().end(); ++it )
      {
      it->SetMapCompToComp( mapCompToComp );  
      }
    }

  /** */
  std::vector<unsigned char > * GetMapCompToComp()
    {
    return this->GetFragmentProcessor().GetMapCompToComp();
    }


  /** */
  float GetMinLogLikelihoodTimesPrior() const
    { 
    return this->GetFragmentProcessor().GetMinLogLikelihoodTimesPrior(); 
    }
  
  /** */
  const AtlasPositionGradientContainerType* GetPositionGradient() const
    {
    return this->GetFragmentProcessor().GetPositionGradient();
    }
  
  /** */
  AtlasPositionGradientContainerType* GetPositionGradient()
    {
    return this->GetFragmentProcessor().GetPositionGradient();
    }
  
  
protected:
  AtlasMeshPositionGradientCalculator() {};
  virtual ~AtlasMeshPositionGradientCalculator() {};

private:
  AtlasMeshPositionGradientCalculator(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented
  
  
};


} // end namespace kvl

#endif

