/*
 * coarse_l1_blas.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: bjoo
 */

#include "lattice/constants.h"
#include "lattice/lattice_info.h"
#include "lattice/coarse/coarse_l1_blas.h"


// for random numbers:
#include <random>

namespace MG
{

namespace GlobalComm {
	void GlobalSum( double& my_summand )
	{
		return; // Return Summand Unchanged -- MPI version should use an MPI_ALLREDUCE

	}
	void GlobalSum( double* array, int array_length ) {
		return;  // Single Node for now. Return the untouched array. -- MPI Version should use allreduce
	}
}


/** Performs:
 *  x <- x - y;
 *  returns: norm(x) after subtraction
 *  Useful for computing residua, where r = b and y = Ax
 *  then n2 = xmyNorm(r,y); will leave r as the residuum and return its square norm
 *
 * @param x  - CoarseSpinor ref
 * @param y  - CoarseSpinor ref
 * @return   double containing the square norm of the difference
 *
 */
double XmyNorm2Vec(CoarseSpinor& x, const CoarseSpinor& y)
{
	double norm_diff = (double)0;

	const LatticeInfo& x_info = x.GetInfo();


	const LatticeInfo& y_info = y.GetInfo();
	AssertCompatible(x_info, y_info);


	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();

	// Loop over the sites and sum up the norm
#pragma omp parallel for collapse(2) reduction(+:norm_diff)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			float* x_site_data = x.GetSiteDataPtr(cb,cbsite);
			const float* y_site_data = y.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {
				double diff_re = x_site_data[ RE + n_complex*cspin ] - y_site_data[ RE + n_complex*cspin ];
				double diff_im = x_site_data[ IM + n_complex*cspin ] - y_site_data[ IM + n_complex*cspin ];
				x_site_data[RE + n_complex * cspin] = diff_re;
				x_site_data[IM + n_complex * cspin] = diff_im;

				norm_diff += diff_re*diff_re + diff_im*diff_im;

			}

		}
	} // End of Parallel for reduction

	// I would probably need some kind of global reduction here  over the nodes which for now I will ignore.
	MG::GlobalComm::GlobalSum(norm_diff);

	return norm_diff;
}

/** returns || x ||^2
 * @param x  - CoarseSpinor ref
 * @return   double containing the square norm of the difference
 *
 */
double Norm2Vec(const CoarseSpinor& x)
{
	double norm_sq = (double)0;

	const LatticeInfo& x_info = x.GetInfo();


	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();

	// Loop over the sites and sum up the norm
#pragma omp parallel for collapse(2) reduction(+:norm_sq)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			const float* x_site_data = x.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {
				double x_re = x_site_data[ RE + n_complex*cspin ];
				double x_im = x_site_data[ IM + n_complex*cspin ];

				norm_sq += x_re*x_re + x_im*x_im;

			}

		}
	} // End of Parallel for reduction

	// I would probably need some kind of global reduction here  over the nodes which for now I will ignore.
	MG::GlobalComm::GlobalSum(norm_sq);

	return norm_sq;
}

/** returns < x | y > = x^H . y
 * @param x  - CoarseSpinor ref
 * @param y  - CoarseSpinor ref
 * @return   double containing the square norm of the difference
 *
 */
std::complex<double> InnerProductVec(const CoarseSpinor& x, const CoarseSpinor& y)
{

	const LatticeInfo& x_info = x.GetInfo();
	const LatticeInfo& y_info = y.GetInfo();
		AssertCompatible(x_info, y_info);

	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();

	double iprod_re=(double)0;
	double iprod_im=(double)0;

	// Loop over the sites and sum up the norm
#pragma omp parallel for collapse(2) reduction(+:iprod_re) reduction(+:iprod_im)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			const float* x_site_data = x.GetSiteDataPtr(cb,cbsite);
			const float* y_site_data = y.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {

				iprod_re += x_site_data[ RE + n_complex*cspin ]*y_site_data[ RE + n_complex*cspin ]
							+ x_site_data[ IM + n_complex*cspin ]*y_site_data[ IM + n_complex*cspin ];

				iprod_im += x_site_data[ RE + n_complex*cspin ]*y_site_data[ IM + n_complex*cspin ]
							- x_site_data[ IM + n_complex*cspin ]*y_site_data[ RE + n_complex*cspin ];


			}

		}
	} // End of Parallel for reduction

	// Global Reduce
	double iprod_array[2] = { iprod_re, iprod_im };
	MG::GlobalComm::GlobalSum(iprod_array,2);

	std::complex<double> ret_val(iprod_array[0],iprod_array[1]);

	return ret_val;
}

void ZeroVec(CoarseSpinor& x)
{
	const LatticeInfo& x_info = x.GetInfo();


	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();
#pragma omp parallel for collapse(2)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			float* x_site_data = x.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {

				x_site_data[RE + n_complex*cspin] = 0;
				x_site_data[IM + n_complex*cspin] = 0;

			}

		}
	} // End of Parallel for region

}

void CopyVec(CoarseSpinor& x, const CoarseSpinor& y)
{


	const LatticeInfo& x_info = x.GetInfo();
	const LatticeInfo& y_info = y.GetInfo();
	AssertCompatible(x_info, y_info);


	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();
#pragma omp parallel for collapse(2)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			float* x_site_data = x.GetSiteDataPtr(cb,cbsite);
			const float* y_site_data = y.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {

				x_site_data[RE + n_complex*cspin] = y_site_data[RE + n_complex*cspin];
				x_site_data[IM + n_complex*cspin] = y_site_data[IM + n_complex*cspin];

			}

		}
	} // End of Parallel for region

}

void ScaleVec(const float alpha, CoarseSpinor& x)
{


	const LatticeInfo& x_info = x.GetInfo();

	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();

#pragma omp parallel for collapse(2)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			float* x_site_data = x.GetSiteDataPtr(cb,cbsite);

			for(int cspin=0; cspin < num_colorspin; ++cspin) {

				x_site_data[RE + n_complex*cspin] *= alpha;
				x_site_data[IM + n_complex*cspin] *= alpha;
			}

		}
	} // End of Parallel for region

}

void ScaleVec(const std::complex<float>& alpha, CoarseSpinor& x)
{


	const LatticeInfo& x_info = x.GetInfo();

	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();
#pragma omp parallel for collapse(2)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {


			// Identify the site
			float* x_site_data = x.GetSiteDataPtr(cb,cbsite);

			for(int cspin=0; cspin < num_colorspin; ++cspin) {
				std::complex<float> t( x_site_data[RE + n_complex*cspin],
									   x_site_data[IM + n_complex*cspin]);

				t *= alpha;
				x_site_data[RE + n_complex*cspin] = real(t);
				x_site_data[IM + n_complex*cspin] = imag(t);
			}

		}
	} // End of Parallel for reduction

}

void AxpyVec(const std::complex<float>& alpha, const CoarseSpinor& x, CoarseSpinor& y)
{
	const LatticeInfo& x_info = x.GetInfo();
	const LatticeInfo& y_info = y.GetInfo();
	AssertCompatible(x_info, y_info);


	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();

#pragma omp parallel for collapse(2)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {



			// Identify the site
			const float* x_site_data = x.GetSiteDataPtr(cb,cbsite);
			float* y_site_data = y.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {


				std::complex<float> c_x( x_site_data[RE + n_complex*cspin],
													   x_site_data[IM + n_complex*cspin]);

				std::complex<float> c_y( y_site_data[RE + n_complex*cspin],
													   y_site_data[IM + n_complex*cspin]);


				c_y += alpha*c_x;

				y_site_data[ RE + n_complex*cspin] = real(c_y);
				y_site_data[ IM + n_complex*cspin] = imag(c_y);

			}

		}
	} // End of Parallel for region
}

void AxpyVec(const float& alpha, const CoarseSpinor&x, CoarseSpinor& y) {
	const LatticeInfo& x_info = x.GetInfo();
	const LatticeInfo& y_info = y.GetInfo();
	AssertCompatible(x_info, y_info);


	IndexType num_cbsites = x_info.GetNumCBSites();
	IndexType num_colorspin = x.GetNumColorSpin();

#pragma omp parallel for collapse(2)
	for(int cb=0; cb < 2; ++cb ) {
		for(int cbsite = 0; cbsite < num_cbsites; ++cbsite ) {



			// Identify the site
			const float* x_site_data = x.GetSiteDataPtr(cb,cbsite);
			float* y_site_data = y.GetSiteDataPtr(cb,cbsite);

			// Sum difference over the colorspins
			for(int cspin=0; cspin < num_colorspin; ++cspin) {


				y_site_data[ RE + n_complex*cspin] += alpha*x_site_data[ RE + n_complex*cspin];
				y_site_data[ IM + n_complex*cspin] += alpha*x_site_data[ IM + n_complex*cspin];

			}

		}
	} // End of Parallel for region
}


/**** NOT 100% sure how to test this easily ******/
void Gaussian(CoarseSpinor& x)
{
	const LatticeInfo& info = x.GetInfo();
	const IndexType num_colorspin = info.GetNumColors()*info.GetNumSpins();
	const IndexType num_cbsites = info.GetNumCBSites();

	/* FIXME: This is quick and dirty and nonreproducible. A better source of random
	 * numbers which is reproducible, and can scale with the number of sites in the
	 * info nicely and is thread safe is neeeded.
	 */

	// Create the thread team
#pragma omp parallel
	{
		// Each thread team create its own RNG
		std::random_device r;
		std::mt19937_64 twister_engine(r());  // An engine based on a 'truly random seed'

		// A normal distribution centred on 0, with width 1
		std::normal_distribution<> normal_dist(0.0,1.0);

#pragma omp for collapse(2)
		for(int cb=0; cb < n_checkerboard; ++cb) {
			for(int cbsite = 0; cbsite < num_cbsites; ++cbsite) {

				float *x_site_data = x.GetSiteDataPtr(cb,cbsite);
				for(int cspin=0; cspin < num_colorspin; ++cspin)  {

					// Using the Twister Engine, draw from the normal distribution
					x_site_data[ RE + n_complex*cspin ] = normal_dist( twister_engine );
					x_site_data[ IM + n_complex*cspin ] = normal_dist( twister_engine );
				}

			}
		}


	}

}

void ZeroGauge(CoarseGauge& gauge)
{
	const LatticeInfo& info = gauge.GetInfo();
	const int num_cbsites=info.GetNumCBSites();
	const int num_colorspins = gauge.GetNumColorSpin();
	for(int cb=0; cb < n_checkerboard; ++cb ) {
		for(int cbsite=0; cbsite < num_cbsites; ++cbsite) {

			// 8 + central piece
			for(int dir=0; dir < 9; ++dir ) {
				float* gauge_data=gauge.GetSiteDirDataPtr(cb,cbsite,dir);
				for(int cs=0; cs < num_colorspins*num_colorspins; ++cs) {
					gauge_data[RE + n_complex*cs ] = 0;
					gauge_data[IM + n_complex*cs ] = 0;
				} //cs
			} // dir
		} //cbsite
	} // cb
}

};


