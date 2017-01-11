/*
 * solver.h
 *
 *  Created on: Jan 10, 2017
 *      Author: bjoo
 */

#ifndef INCLUDE_LATTICE_SOLVER_H_
#define INCLUDE_LATTICE_SOLVER_H_

namespace MG {
	enum ResiduumType { ABSOLUTE, RELATIVE };

	struct LinearSolverResults {
		ResiduumType resid_type;
		int n_count;
		double resid;
	};

	template<typename Spinor, typename Gauge>
	class LinearSolver {
		LinearSolverResults operator()(Spinor& out, const Spinor& in, ResiduumType = RELATIVE ) const;
	};

	// Base Parameter Struct
	class LinearSolverParamsBase {
	public:
		double RsdTarget;
		int MaxIter;
		bool VerboseP;
		LinearSolverParamsBase() {
			MaxIter=-1;
			VerboseP=false;

		}
	};

	// A Smoother Is much like a solver, but there are some 'don't care'-s
	// E.g. I may not care about the residua, and the iteration count may
	// be fixed.
	template<typename Spinor, typename Gauge>
	class Smoother {
		void operator()(Spinor& out, const Spinor& in) const;
	};

	// Base Parameter Struct
		class SmootherParamsBase {
		public:
			int MaxIter;
			bool VerboseP;
			SmootherParamsBase() {
				MaxIter = -1;
				VerboseP=false;

			}
		};
};




#endif /* INCLUDE_LATTICE_SOLVER_H_ */
