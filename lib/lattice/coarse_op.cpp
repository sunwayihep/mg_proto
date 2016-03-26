
#include <omp.h>
#include <cstdio>

#include "lattice/coarse/coarse_op.h"
#include "lattice/thread_info.h"
#include "lattice/cmat_mult.h"
#include "utils/memory.h"
#include <complex>

#include <immintrin.h>
namespace MG {


CoarseDiracOp::CoarseDiracOp(const LatticeInfo& l_info, IndexType n_smt)
	: _lattice_info(l_info),
	  _n_color(l_info.GetNumColors()),
	  _n_spin(l_info.GetNumSpins()),
	  _n_colorspin(_n_color*_n_spin),
	  _n_smt(n_smt),
	  _n_vrows(2*_n_colorspin/VECLEN),
	  _n_xh( l_info.GetCBLatticeDimensions()[0] ),
	  _n_x( l_info.GetLatticeDimensions()[0] ),
	  _n_y( l_info.GetLatticeDimensions()[1] ),
	  _n_z( l_info.GetLatticeDimensions()[2] ),
	  _n_t( l_info.GetLatticeDimensions()[3] )
{
#pragma omp parallel
	{
#pragma omp master
		{
			// Set the number of threads
			_n_threads = omp_get_num_threads();

			// ThreadLimits give iteration bounds for a specific thread with a tid.
			// These are things like min_site, max_site, min_row, max_row etc.
			// So here I allocate one for each thread.
			_thread_limits = (ThreadLimits*) MG::MemoryAllocate(_n_threads*sizeof(ThreadLimits), MG::REGULAR);
		} // omp master: barrier implied

#pragma omp barrier

		// Set the number of threads and break down into SIMD ID and Core ID
		// This requires knowledge about the order in which threads are assigned.
		//
		const int tid = omp_get_thread_num();
		const int n_cores = _n_threads/_n_smt;

		// Decompose tid into site_par_id (parallelism over sites)
		// and mv_par_id ( parallelism over rows of the matvec )
		// Order is: mv_par_id + _n_mv_parallel*site_par_id
        // Same as   smt_id + n_smt * core_id

		const int core_id = tid/_n_smt;
		const int smt_id = tid - _n_smt*core_id;
		const int n_floats_per_cacheline =MG_DEFAULT_CACHE_LINE_SIZE/sizeof(float);
		int n_cachelines = _n_vrows*VECLEN/n_floats_per_cacheline;
		int cl_per_smt = n_cachelines/_n_smt;
		if( n_cachelines % _n_smt != 0 ) cl_per_smt++;
		int min_cl = smt_id*cl_per_smt;
		int max_cl = MinInt((smt_id+1)*cl_per_smt, n_cachelines);
		int min_vrow = (min_cl*n_floats_per_cacheline)/VECLEN;
		int max_vrow = (max_cl*n_floats_per_cacheline)/VECLEN;

#if 1
		_thread_limits[tid].min_vrow = min_vrow;
		_thread_limits[tid].max_vrow = max_vrow;
#else
		// Hack so that we get 1 thread per core running even in SMT mode
		if ( smt_id == 0 ) {
			_thread_limits[tid].min_vrow = 0;
			_thread_limits[tid].max_vrow = (n_complex*_n_colorspin)/VECLEN;
		}
		else {
			// Non SMT threads will idle (loop limits too high)
			_thread_limits[tid].min_vrow =1+ (n_complex*_n_colorspin)/VECLEN;
			_thread_limits[tid].max_vrow =1+ (n_complex*_n_colorspin)/VECLEN;
		}
#endif

		// Find minimum and maximum site -- assume
		// small lattice so no blocking at this point
		// just linearly divide the sites
		const int n_sites_cb = _lattice_info.GetNumCBSites();
		int sites_per_core = n_sites_cb/n_cores;
		if( n_sites_cb % n_cores != 0 ) sites_per_core++;
		int min_site = core_id*sites_per_core;
		int max_site = MinInt((core_id+1)*sites_per_core, n_sites_cb);
		_thread_limits[tid].min_site = min_site;
		_thread_limits[tid].max_site = max_site;

#pragma omp critical
		{
			std::printf("Thread=%d smtid=%d n_sites_cb=%d n_vrows=%d min_vrow=%d max_vrow=%d min_site=%d max_site=%d\n",
					tid,smt_id,n_sites_cb, _n_vrows, _thread_limits[tid].min_vrow, _thread_limits[tid].max_vrow, min_site, max_site);
		}
	} // omp parallel

}


void CoarseDiracOp::operator()(CoarseSpinor& spinor_out,
			const CoarseGauge& gauge_in,
			const CoarseClover& clover_in,
			const CoarseSpinor& spinor_in,
			const IndexType target_cb,
			const IndexType tid) const
{
	IndexType min_site = _thread_limits[tid].min_site;
	IndexType max_site = _thread_limits[tid].max_site;

	// Site is output site
	for(IndexType site=min_site; site < max_site;++site) {

		// Turn site into x,y,z,t coords assuming we run as
		//  site = x_cb + Nxh*( y + Ny*( z + Nz*t ) ) )

		IndexType tmp_yzt = site / _n_xh;
		IndexType xcb = site - _n_xh * tmp_yzt;
		IndexType tmp_zt = tmp_yzt / _n_y;
		IndexType y = tmp_yzt - _n_y * tmp_zt;
		IndexType t = tmp_zt / _n_z;
		IndexType z = tmp_zt - _n_z * t;

		float* output = spinor_out.GetSiteDataPtr(target_cb, site);
		const float* gauge_base = gauge_in.GetSiteDataPtr(target_cb,site);
		const float* spinor_cb = spinor_in.GetSiteDataPtr(target_cb,site);
		const float* clover_cb_0 = clover_in.GetSiteChiralDataPtr(target_cb,site,0);
		const float* clover_cb_1 = clover_in.GetSiteChiralDataPtr(target_cb,site,1);
		const IndexType gdir_offset = gauge_in.GetLinkOffset();

		const float *gauge_links[8]={ gauge_base,                    // X forward
							gauge_base+gdir_offset,        // X backward
							gauge_base+2*gdir_offset,      // Y forward
							gauge_base+3*gdir_offset,      // Y backward
							gauge_base+4*gdir_offset,      // Z forward
							gauge_base+5*gdir_offset,      // Z backward
							gauge_base+6*gdir_offset,      // T forward
							gauge_base+7*gdir_offset       // T backward
		};

		// Neighbouring spinors
		IndexType x = 2*xcb + ((target_cb+y+z+t)&0x1);  // Global X

		// Boundaries -- we can indirect here to
		// some face buffers if needs be
		IndexType x_plus = (x < _n_x-1 ) ? (x + 1) : 0;
		IndexType x_minus = ( x > 0 ) ?  (x - 1) : _n_x-1;

		x_plus /= 2; // Convert to checkerboard
		x_minus /=2; // Covert to checkerboard

		IndexType y_plus = ( y < _n_y - 1) ? y+1 : 0;
		IndexType y_minus = ( y > 0 ) ? y-1 : _n_y - 1;

		IndexType z_plus = ( z < _n_z - 1) ? z+1 : 0;
		IndexType z_minus = ( z > 0 ) ? z-1 : _n_z - 1;

		IndexType t_plus = ( t < _n_t - 1) ? t+1 : 0;
		IndexType t_minus = ( t > 0 ) ? t-1 : _n_t - 1;

		const IndexType source_cb = 1 - target_cb;
		const float *neigh_spinors[8] = {
			spinor_in.GetSiteDataPtr(source_cb, x_plus  + _n_xh*(y + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, x_minus + _n_xh*(y + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y_plus + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y_minus + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z_plus + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z_minus + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z + _n_z*t_plus))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z + _n_z*t_minus))),
		};


		siteApply(output, gauge_links, clover_cb_0, clover_cb_1, spinor_cb, neigh_spinors);
	}

}

// Run in thread
void CoarseDiracOp::Dslash(CoarseSpinor& spinor_out,
			const CoarseGauge& gauge_in,
			const CoarseSpinor& spinor_in,
			const IndexType target_cb,
			const IndexType tid) const
{
	IndexType min_site = _thread_limits[tid].min_site;
	IndexType max_site = _thread_limits[tid].max_site;
	// Site is output site
	for(IndexType site=min_site; site < max_site;++site) {

		// Turn site into x,y,z,t coords assuming we run as
		//  site = x_cb + Nxh*( y + Ny*( z + Nz*t ) ) )

		IndexType tmp_yzt = site / _n_xh;
		IndexType xcb = site - _n_xh * tmp_yzt;
		IndexType tmp_zt = tmp_yzt / _n_y;
		IndexType y = tmp_yzt - _n_y * tmp_zt;
		IndexType t = tmp_zt / _n_z;
		IndexType z = tmp_zt - _n_z * t;

		float* output = spinor_out.GetSiteDataPtr(target_cb, site);
		const float* gauge_base = gauge_in.GetSiteDataPtr(target_cb,site);
		const float* spinor_cb = spinor_in.GetSiteDataPtr(target_cb,site);
		const IndexType gdir_offset = gauge_in.GetLinkOffset();

		const float *gauge_links[8]={ gauge_base,                    // X forward
							gauge_base+gdir_offset,        // X backward
							gauge_base+2*gdir_offset,      // Y forward
							gauge_base+3*gdir_offset,      // Y backward
							gauge_base+4*gdir_offset,      // Z forward
							gauge_base+5*gdir_offset,      // Z backward
							gauge_base+6*gdir_offset,      // T forward
							gauge_base+7*gdir_offset       // T backward
		};

		// Neighbouring spinors
		IndexType x = 2*xcb + ((target_cb+y+z+t)&0x1);  // Global X

		// Boundaries -- we can indirect here to
		// some face buffers if needs be
		IndexType x_plus = (x < _n_x-1 ) ? (x + 1) : 0;
		IndexType x_minus = ( x > 0 ) ?  (x - 1) : _n_x-1;

		x_plus /= 2; // Convert to checkerboard
		x_minus /=2; // Covert to checkerboard

		IndexType y_plus = ( y < _n_y - 1) ? y+1 : 0;
		IndexType y_minus = ( y > 0 ) ? y-1 : _n_y - 1;

		IndexType z_plus = ( z < _n_z - 1) ? z+1 : 0;
		IndexType z_minus = ( z > 0 ) ? z-1 : _n_z - 1;

		IndexType t_plus = ( t < _n_t - 1) ? t+1 : 0;
		IndexType t_minus = ( t > 0 ) ? t-1 : _n_t - 1;

		const IndexType source_cb = 1 - target_cb;
		const float *neigh_spinors[8] = {
			spinor_in.GetSiteDataPtr(source_cb, x_plus  + _n_xh*(y + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, x_minus + _n_xh*(y + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y_plus + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y_minus + _n_y*(z + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z_plus + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z_minus + _n_z*t))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z + _n_z*t_plus))),
			spinor_in.GetSiteDataPtr(source_cb, xcb + _n_xh*(y + _n_y*(z + _n_z*t_minus))),
		};


		siteApplyDslash(output, gauge_links, spinor_cb, neigh_spinors);
	}

}


inline
void CoarseDiracOp::siteApplyDslash( float *output,
		  	  	  	  	 	 const float* gauge_links[8],
							 const float* spinor_cb,
							 const float* neigh_spinors[8]) const
{
	const int N_color = GetNumColor();
	const int N_colorspin = GetNumColorSpin();

#pragma omp simd
	for(int i=0; i < N_colorspin*n_complex; ++i) {
		output[i] = 0;
	}

#if 1
	// Apply the Dslash term.
	for(int mu=0; mu < 8; ++mu) {
		CMatMultNaiveAdd(output, gauge_links[mu], neigh_spinors[mu], N_colorspin);

	}
#endif

}

inline
void CoarseDiracOp::siteApply( float *output,
		  	  	  	  	 	 const float* gauge_links[8],
							 const float* clover_cb_0,
							 const float* clover_cb_1,
							 const float* spinor_cb,
							 const float* neigh_spinors[8]) const
{
	const int N_color = GetNumColor();
	const int N_colorspin = GetNumColorSpin();

	// Apply clover term
	CMatMultNaive(output, clover_cb_0, spinor_cb, N_color);
	CMatMultNaive(&output[2*N_color], clover_cb_1, &spinor_cb[2*N_color], N_color);

	// Apply the Dslash term.
	for(int mu=0; mu < 8; ++mu) {
		CMatMultNaiveAdd(output, gauge_links[mu], neigh_spinors[mu], N_colorspin);

	}

}

} // Namespace

