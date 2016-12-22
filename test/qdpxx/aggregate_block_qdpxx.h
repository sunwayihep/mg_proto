/*
 * aggregate_qdpxx.h
 *
 *  Created on: Dec 9, 2016
 *      Author: bjoo
 */

#ifndef TEST_QDPXX_AGGREGATE_BLOCK_QDPXX_H_
#define TEST_QDPXX_AGGREGATE_BLOCK_QDPXX_H_

#include "qdp.h"
#include "clover_term_qdp_w.h"
#include "lattice/coarse/coarse_types.h"


using namespace MG;
using namespace MGTesting;
using namespace QDP;


namespace MGTesting {

class Block {
public:
	// I am doing it this way so we can make a std::vector of these...
	// That  means it needs an argument free constructor

	Block(void) {
		_created = false;
		_num_sites = 0;
	}

	void create(const IndexArray local_lattice_dimensions,
				const IndexArray block_origin,
				const IndexArray block_dimensions);

	inline
	const std::vector<IndexType>& getSiteList(void) const {
		return _site_list;
	}

	inline
	const bool isCreated(void) {
		return _created;
	}

	inline
	unsigned int getNumSites() const {
		return _num_sites;
	}

	// Destructor is automatic
	~Block() {}
private:
	IndexArray _origin;
	IndexArray _dimensions;
	unsigned int _num_sites;
	bool _created = false;
	std::vector<IndexType> _site_list;
};


/* --------- BLOCK BASED STUFF -------------*/
void CreateBlockList(std::vector<Block>& blocklist,
		IndexArray& blocked_lattice_dimensions,
		const IndexArray& local_lattice_dimensions,
		const IndexArray& block_dimensions );

//! v *= alpha (alpha is real) over and aggregate in a block, v is a QDP++ Lattice Fermion
void axBlockAggrQDPXX(const double alpha, LatticeFermion& v, const Block& block, int aggr);

//! y += alpha * x (alpha is complex) over aggregate in a block, x, y are QDP++ LatticeFermions;
void caxpyBlockAggrQDPXX(const std::complex<double>& alpha, const LatticeFermion& x, LatticeFermion& y,  const Block& block, int aggr);

//! return || v ||^2 over an aggregate in a block, v is a QDP++ LatticeFermion
double norm2BlockAggrQDPXX(const LatticeFermion& v, const Block& block, int aggr);

//! return < left | right > = sum left^\dagger_i * right_i for an aggregate, over a block
std::complex<double>
innerProductBlockAggrQDPXX(const LatticeFermion& left, const LatticeFermion& right, const Block& block, int aggr);

//! Extract the spins belonging to a given aggregate from QDP++ source vector src, into QDP++ target vector target
void extractAggregateQDPXX(LatticeFermion& target, const LatticeFermion& src, const Block& block, int aggr );

//! Orthonormalize vecs over the spin aggregates within the sites
void orthonormalizeBlockAggregatesQDPXX(multi1d<LatticeFermion>& vecs, const std::vector<Block>& block_list);


//! 'Restrict' a QDP++ spinor to a CoarseSpinor with the same geometry
void restrictSpinorQDPXXFineToCoarse( const std::vector<Block>& blocklist, const multi1d<LatticeFermion>& v, const LatticeFermion& ferm_in, CoarseSpinor& out);

//! 'Prolongate' a CoarseSpinor to a QDP++ Fine Spinor
void prolongateSpinorCoarseToQDPXXFine(const std::vector<Block>& blocklist, const multi1d<LatticeFermion>& v, const CoarseSpinor& coarse_in, LatticeFermion& fine_out);

//! Coarsen one direction of a 'dslash' link
void dslashTripleProductDirQDPXX(const std::vector<Block>& blocklist, int dir, const multi1d<LatticeColorMatrix>& u, const multi1d<LatticeFermion>& in_vecs, CoarseGauge& u_coarse);

//! Coarsen the clover term (1 block = 1 site )
void clovTripleProductQDPXX(const std::vector<Block>& blocklist, const QDPCloverTerm& clov,const multi1d<LatticeFermion>& in_vecs, CoarseClover& cl_coarse);

};

#endif /* TEST_QDPXX_AGGREGATE_QDPXX_H_ */
