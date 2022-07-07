//
// Copyright (c) 2022, INRIA
//
/**
 * @file wrapper.hpp 
*/

#ifndef PROXSUITE_QP_SPARSE_WRAPPER_HPP
#define PROXSUITE_QP_SPARSE_WRAPPER_HPP
#include <tl/optional.hpp>
#include <qp/results.hpp>
#include <qp/settings.hpp>
#include <qp/sparse/solver.hpp>
#include <qp/sparse/helpers.hpp>

namespace proxsuite {
namespace qp {
namespace sparse {
///
/// @brief This class defines the API of PROXQP solver with sparse backend.
///
/*!
 * Wrapper class for using proxsuite API with dense backend
 * for solving linearly constrained convex QP problem using ProxQp algorithm.  
 * 
 * Example usage:
 * ```cpp
#include <Eigen/Core>
#include <Eigen/Cholesky>
#include <qp/dense/dense.hpp>
#include <veg/util/dbg.hpp>
#include <test/include/util.hpp>

using T = double;
using I = c_int;
auto main() -> int {

	// Generate a random QP problem with primal variable dimension of size dim; n_eq equality constraints and n_in inequality constraints
	ldlt_test::rand::set_seed(1);
	qp::isize dim = 10;
	qp::isize n_eq(dim / 4);
	qp::isize n_in(dim / 4);
	T strong_convexity_factor(1.e-2);
	T sparsity_factor = 0.15; // controls the sparsity of each matrix of the problem generated
	T eps_abs = T(1e-9);
	double p = 1.0;
	T conditioning(10.0);
	auto H = ldlt_test::rand::sparse_positive_definite_rand(n, conditioning, p);
	auto g = ldlt_test::rand::vector_rand<T>(n);
	auto A = ldlt_test::rand::sparse_matrix_rand<T>(n_eq,n, p);
	auto b = ldlt_test::rand::vector_rand<T>(n_eq);
	auto C = ldlt_test::rand::sparse_matrix_rand<T>(n_in,n, p);
	auto l = ldlt_test::rand::vector_rand<T>(n_in);
	auto u = (l.array() + 1).matrix().eval();

	qp::sparse::QP<T,I> Qp(n, n_eq, n_in);
	Qp.settings.eps_abs = 1.E-9;
	Qp.settings.verbose = true;
	Qp.setup_sparse_matrices(H,g,A,b,C,u,l);
	Qp.solve();
	
	// Solve the problem
	qp::sparse::QP<T,I> Qp(n, n_eq, n_in);
	Qp.settings.eps_abs = 1.E-9;
	Qp.settings.verbose = true;
	Qp.setup_sparse_matrices(H,g,A,b,C,u,l);
	Qp.solve();

	// Verify solution accuracy
	T pri_res = std::max(
			(qp.A * Qp.results.x - qp.b).lpNorm<Eigen::Infinity>(),
			(qp::dense::positive_part(qp.C * Qp.results.x - qp.u) +
			qp::dense::negative_part(qp.C * Qp.results.x - qp.l))
					.lpNorm<Eigen::Infinity>());
	T dua_res = (qp.H * Qp.results.x + qp.g + qp.A.transpose() * Qp.results.y +
					qp.C.transpose() * Qp.results.z)
					.lpNorm<Eigen::Infinity>();
	VEG_ASSERT(pri_res <= eps_abs);
	VEG_ASSERT(dua_res <= eps_abs);

	// Some solver statistics
	std::cout << "------solving qp with dim: " << dim
						<< " neq: " << n_eq << " nin: " << n_in << std::endl;
	std::cout << "primal residual: " << pri_res << std::endl;
	std::cout << "dual residual: " << dua_res << std::endl;
	std::cout << "total number of iteration: " << Qp.results.info.iter
						<< std::endl;
}
 * ```
 */
template <typename T, typename I>
struct QP {
	Results<T> results;
	Settings<T> settings;
	Model<T, I> model;
	Workspace<T, I> work;
	preconditioner::RuizEquilibration<T, I> ruiz;
	/*!
	 * Default constructor using the dimension of the matrices in entry.
	 * @param _dim primal variable dimension.
	 * @param _n_eq number of equality constraints.
	 * @param _n_in number of inequality constraints.
	 */
	QP(isize _dim, isize _n_eq, isize _n_in)
			: results(_dim, _n_eq, _n_in),
				settings(),
				model(),
				work(),
				ruiz(_dim, _n_eq + _n_in, 1e-3, 10, preconditioner::Symmetry::UPPER) {
					
			work.timer.stop();
			work.internal.do_symbolic_fact=true;
				}
	/*!
	 * Default constructor using the sparsity structure of the matrices in entry.
	 * @param H boolean mask of the quadratic cost input defining the QP model.
	 * @param A boolean mask of the equality constraint matrix input defining the QP model.
	 * @param C boolean mask of the inequality constraint matrix input defining the QP model.
	 */
	QP(const SparseMat<bool, I>& H,const SparseMat<bool,I>& A,const SparseMat<bool,I>& C)
			:QP( H.rows(),A.rows(),C.rows()){
			if (settings.compute_timings){
				work.timer.stop();
				work.timer.start();
			}
			isize _dim = H.rows();
			isize _n_eq = A.rows();
			isize _n_in = C.rows();
			SparseMat<bool, I> H_triu = H.template triangularView<Eigen::Upper>();
			SparseMat<bool, I> AT = A.transpose();
			SparseMat<bool, I> CT = C.transpose();
			linearsolver::sparse::MatRef<bool,I> Href = {linearsolver::sparse::from_eigen, H_triu};
			linearsolver::sparse::MatRef<bool,I> ATref = {linearsolver::sparse::from_eigen, AT};
			linearsolver::sparse::MatRef<bool,I> CTref = {linearsolver::sparse::from_eigen, CT};
			work.setup_symbolic_factorizaton(results,model,settings,preconditioner::RuizEquilibration<T, I>::scale_qp_in_place_req(veg::Tag<T>{}, _dim, _n_eq, _n_in),
			Href.symbolic(),ATref.symbolic(),CTref.symbolic());
			if (settings.compute_timings){
				results.info.setup_time = work.timer.elapsed().user; // in microseconds
			}
		}

	/*!
	 * Setups the QP model (with sparse matrix format) and equilibrates it. 
	 * @param H quadratic cost input defining the QP model.
	 * @param g linear cost input defining the QP model.
	 * @param A equality constraint matrix input defining the QP model.
	 * @param b equality constraint vector input defining the QP model.
	 * @param C inequality constraint matrix input defining the QP model.
	 * @param u lower inequality constraint vector input defining the QP model.
	 * @param l lower inequality constraint vector input defining the QP model.
	 * @param compute_preconditioner bool parameter for executing or not the preconditioner.
	 */
	void init(
			const SparseMat<T, I>& H,
			VecRef<T> g,
			const SparseMat<T, I>& A,
			VecRef<T> b,
			const SparseMat<T, I>& C,
			VecRef<T> u,
			VecRef<T> l,
			bool compute_preconditioner_=true,
			std::optional<T> rho = std::nullopt,
			std::optional<T> mu_eq = std::nullopt,
			std::optional<T> mu_in = std::nullopt) {
		if (settings.compute_timings){
			work.timer.stop();
			work.timer.start();
		}
		work.internal.proximal_parameter_update= false;
		PreconditionerStatus preconditioner_status;
		if (compute_preconditioner_){
			preconditioner_status = proxsuite::qp::PreconditionerStatus::EXECUTE;
		}else{
			preconditioner_status = proxsuite::qp::PreconditionerStatus::IDENTITY;
		}
		//settings.compute_preconditioner = compute_preconditioner_;
		SparseMat<T, I> H_triu = H.template triangularView<Eigen::Upper>();
		SparseMat<T, I> AT = A.transpose();
		SparseMat<T, I> CT = C.transpose();
		sparse::QpView<T, I> qp = {
				{linearsolver::sparse::from_eigen, H_triu},
				{linearsolver::sparse::from_eigen, g},
				{linearsolver::sparse::from_eigen, AT},
				{linearsolver::sparse::from_eigen, b},
				{linearsolver::sparse::from_eigen, CT},
				{linearsolver::sparse::from_eigen, l},
				{linearsolver::sparse::from_eigen, u}};
		proxsuite::qp::sparse::update_proximal_parameters(results, work, rho, mu_eq, mu_in);
		qp_setup(qp, results, model, work, settings, ruiz, preconditioner_status);
		if (settings.compute_timings){
			results.info.setup_time += work.timer.elapsed().user; // in microseconds
		}
	};
	/*!
	 * Updates the QP model (with sparse matrix format) and re-equilibrates it if specified by the user. 
	 * If matrices in entry are not null, the update is effective only if the sparsity structure of entry is the same as the one used for the initialization.
	 * @param H_ quadratic cost input defining the QP model.
	 * @param g_ linear cost input defining the QP model.
	 * @param A_ equality constraint matrix input defining the QP model.
	 * @param b_ equality constraint vector input defining the QP model.
	 * @param C_ inequality constraint matrix input defining the QP model.
	 * @param u_ lower inequality constraint vector input defining the QP model.
	 * @param l_ lower inequality constraint vector input defining the QP model.
	 * @param update_preconditioner_ bool parameter for updating or not the preconditioner and the associated scaled model.
	 */
	void update(const std::optional<SparseMat<T, I>> H_,
			std::optional<VecRef<T>> g_,
			const std::optional<SparseMat<T, I>> A_,
			std::optional<VecRef<T>> b_,
			const std::optional<SparseMat<T, I>> C_,
			std::optional<VecRef<T>> u_,
			std::optional<VecRef<T>> l_,
			bool update_preconditioner_ = true,
			std::optional<T> rho = std::nullopt,
			std::optional<T> mu_eq = std::nullopt,
			std::optional<T> mu_in = std::nullopt){
		if (settings.compute_timings){
			work.timer.stop();
			work.timer.start();
		}
		work.internal.dirty = false;
		work.internal.proximal_parameter_update= false;
		PreconditionerStatus preconditioner_status;
		if (update_preconditioner_){
			preconditioner_status = proxsuite::qp::PreconditionerStatus::EXECUTE;
		}else{
			preconditioner_status = proxsuite::qp::PreconditionerStatus::KEEP;
		}
		isize n = model.dim;
		isize n_eq = model.n_eq;
		isize n_in = model.n_in;
		linearsolver::sparse::MatMut<T, I> kkt_unscaled = model.kkt_mut_unscaled();

		auto kkt_top_n_rows = detail::top_rows_mut_unchecked(veg::unsafe, kkt_unscaled, n);

		linearsolver::sparse::MatMut<T, I> H_unscaled = 
				detail::middle_cols_mut(kkt_top_n_rows, 0, n, model.H_nnz);

		linearsolver::sparse::MatMut<T, I> AT_unscaled =
				detail::middle_cols_mut(kkt_top_n_rows, n, n_eq, model.A_nnz);

		linearsolver::sparse::MatMut<T, I> CT_unscaled =
				detail::middle_cols_mut(kkt_top_n_rows, n + n_eq, n_in, model.C_nnz);

		// update the model
		
		if (g_ != std::nullopt) {
			model.g = g_.value();
		} 
		if (b_ != std::nullopt) {
			model.b = b_.value();
		}
		if (u_ != std::nullopt) {
			model.u = u_.value();
		}
		if (l_ != std::nullopt) {
			model.l = l_.value();
		} 
		if (H_ != std::nullopt) {
			SparseMat<T, I> H_triu = H_.value().template triangularView<Eigen::Upper>();
			if (A_ != std::nullopt) {
				if (C_ != std::nullopt) {
					bool res = have_same_structure(H_unscaled.as_const(),{linearsolver::sparse::from_eigen,H_triu}) &&
						have_same_structure(AT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())}) &&
						have_same_structure(CT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())}) ;
					/* TO PUT IN DEBUG MODE
					std::cout << "have same structure = " << res << std::endl;
					*/
					if (res){
						copy(H_unscaled,{linearsolver::sparse::from_eigen,H_triu}); // copy rhs into lhs
						copy(AT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())}); // copy rhs into lhs
						copy(CT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())}); // copy rhs into lhs
					}
				} else {
					bool res = have_same_structure(H_unscaled.as_const(),{linearsolver::sparse::from_eigen,H_triu})&&
						 have_same_structure(AT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())});
					/* TO PUT IN DEBUG MODE
					std::cout << "have same structure = " << res << std::endl;
					*/
					if (res){
						copy(H_unscaled,{linearsolver::sparse::from_eigen,H_triu}); // copy rhs into lhs
						copy(AT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())}); // copy rhs into lhs
					}
				}
			} else if (C_ != std::nullopt) {
				bool res = have_same_structure(H_unscaled.as_const(),{linearsolver::sparse::from_eigen,H_triu}) &&
					have_same_structure(CT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())});
				/* TO PUT IN DEBUG MODE
				std::cout << "have same structure = " << res << std::endl;
				*/
				if (res){
					copy(H_unscaled,{linearsolver::sparse::from_eigen,H_triu}); // copy rhs into lhs
					copy(CT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())}); // copy rhs into lhs
				}
			} else {
				
				bool res = have_same_structure(H_unscaled.as_const(),{linearsolver::sparse::from_eigen,H_triu}) ;
				/* TO PUT IN DEBUG MODE
				std::cout << "have same structure = " << res << std::endl;
				*/
				if (res){
						copy(H_unscaled,{linearsolver::sparse::from_eigen,H_.value()}); // copy rhs into lhs
				}
			}
		} else if (A_ != std::nullopt) {
			if (C_ != std::nullopt) {
				bool res = have_same_structure(AT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())})&&
					have_same_structure(CT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())});
				/* TO PUT IN DEBUG MODE
				std::cout << "have same structure = " << res << std::endl;
				*/
				if (res){
					copy(AT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())}); // copy rhs into lhs
					copy(CT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())}); // copy rhs into lhs
				}
			} else {
				bool res = have_same_structure(AT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())});
				/* TO PUT IN DEBUG MODE
				std::cout << "have same structure = " << res << std::endl;
				*/
				if (res){
					copy(AT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(A_.value().transpose())}); // copy rhs into lhs
				}
			}
		} else if (C_ != std::nullopt) {
			bool res = have_same_structure(CT_unscaled.as_const(),{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())});
			/* TO PUT IN DEBUG MODE
			std::cout << "have same structure = " << res << std::endl;
			*/
			if (res){
					copy(CT_unscaled,{linearsolver::sparse::from_eigen,SparseMat<T,I>(C_.value().transpose())}); // copy rhs into lhs
			}
		}
		
		SparseMat<T, I> H_triu = H_unscaled.to_eigen().template triangularView<Eigen::Upper>();
		sparse::QpView<T, I> qp = {
				{linearsolver::sparse::from_eigen, H_triu},
				{linearsolver::sparse::from_eigen, model.g},
				{linearsolver::sparse::from_eigen, AT_unscaled.to_eigen()},
				{linearsolver::sparse::from_eigen, model.b},
				{linearsolver::sparse::from_eigen, CT_unscaled.to_eigen()},
				{linearsolver::sparse::from_eigen, model.l},
				{linearsolver::sparse::from_eigen, model.u}};
		proxsuite::qp::sparse::update_proximal_parameters(results, work, rho, mu_eq, mu_in);
		qp_setup(qp, results, model, work, settings, ruiz, preconditioner_status); // store model value + performs scaling according to chosen options
		if (settings.compute_timings){
			results.info.setup_time = work.timer.elapsed().user; // in microseconds
		}
	};
	
	/*!
	 * Solves the QP problem using PRXOQP algorithm.
	 */
	void solve() {
		qp_solve( //
				results,
				model,
				settings,
				work,
				ruiz);
	};
	/*!
	 * Solves the QP problem using PROXQP algorithm and a warm start.
	 * @param x primal warm start.
	 * @param y dual equality warm start.
	 * @param z dual inequality warm start.
	 */
	void solve(std::optional<VecRef<T>> x,
			std::optional<VecRef<T>> y,
			std::optional<VecRef<T>> z) {
		proxsuite::qp::sparse::warm_start(x, y, z, results, settings);
		qp_solve( //
				results,
				model,
				settings,
				work,
				ruiz);
	};
	/*!
	 * Clean-ups solver's results.
	 */
	void cleanup() { results.cleanup(); }
};

template <typename T, typename I>
qp::Results<T> solve(
		const std::optional<SparseMat<T, I>> H,
		std::optional<VecRef<T>> g,
		const std::optional<SparseMat<T, I>> A,
		std::optional<VecRef<T>> b,
		const std::optional<SparseMat<T, I>> C,
		std::optional<VecRef<T>> u,
		std::optional<VecRef<T>> l,

		std::optional<T> eps_abs,
		std::optional<T> eps_rel,
		std::optional<T> rho,
		std::optional<T> mu_eq,
		std::optional<T> mu_in,
		std::optional<VecRef<T>> x,
		std::optional<VecRef<T>> y,
		std::optional<VecRef<T>> z,
		std::optional<bool> verbose,
		std::optional<isize> max_iter,
		std::optional<T> alpha_bcl,
		std::optional<T> beta_bcl,
		std::optional<T> refactor_dual_feasibility_threshold,
		std::optional<T> refactor_rho_threshold,
		std::optional<T> mu_max_eq,
		std::optional<T> mu_max_in,
		std::optional<T> mu_update_factor,
		std::optional<T> cold_reset_mu_eq,
		std::optional<T> cold_reset_mu_in,
		std::optional<isize> max_iter_in,
		std::optional<T> eps_refact,
		std::optional<isize> nb_iterative_refinement,
		std::optional<T> eps_primal_inf,
		std::optional<T> eps_dual_inf) {

	isize n = H.value().rows();
	isize n_eq = A.value().rows();
	isize n_in = C.value().rows();

	qp::sparse::QP<T, I> Qp(n, n_eq, n_in);
	Qp.setup(H, g, A, b, C, u, l); // symbolic factorisation done here

	Qp.update_proximal_parameters(rho, mu_eq, mu_in);
	Qp.warm_start(x, y, z);

	if (eps_abs != std::nullopt) {
		Qp.settings.eps_abs = eps_abs.value();
	}
	if (eps_rel != std::nullopt) {
		Qp.settings.eps_rel = eps_rel.value();
	}
	if (verbose != std::nullopt) {
		Qp.settings.verbose = verbose.value();
	}
	if (alpha_bcl != std::nullopt) {
		Qp.settings.alpha_bcl = alpha_bcl.value();
	}
	if (beta_bcl != std::nullopt) {
		Qp.settings.beta_bcl = beta_bcl.value();
	}
	if (refactor_dual_feasibility_threshold != std::nullopt) {
		Qp.settings.refactor_dual_feasibility_threshold =
				refactor_dual_feasibility_threshold.value();
	}
	if (refactor_rho_threshold != std::nullopt) {
		Qp.settings.refactor_rho_threshold = refactor_rho_threshold.value();
	}
	if (mu_max_eq != std::nullopt) {
		Qp.settings.mu_max_eq = mu_max_eq.value();
		Qp.settings.mu_max_eq_inv = T(1) / mu_max_eq.value();
	}
	if (mu_max_in != std::nullopt) {
		Qp.settings.mu_max_in = mu_max_in.value();
		Qp.settings.mu_max_in_inv = T(1) / mu_max_in.value();
	}
	if (mu_update_factor != std::nullopt) {
		Qp.settings.mu_update_factor = mu_update_factor.value();
		Qp.settings.mu_update_inv_factor = T(1) / mu_update_factor.value();
	}
	if (cold_reset_mu_eq != std::nullopt) {
		Qp.settings.cold_reset_mu_eq = cold_reset_mu_eq.value();
		Qp.settings.cold_reset_mu_eq_inv = T(1) / cold_reset_mu_eq.value();
	}
	if (cold_reset_mu_in != std::nullopt) {
		Qp.settings.cold_reset_mu_in = cold_reset_mu_in.value();
		Qp.settings.cold_reset_mu_in_inv = T(1) / cold_reset_mu_in.value();
	}
	if (max_iter != std::nullopt) {
		Qp.settings.max_iter = max_iter.value();
	}
	if (max_iter_in != std::nullopt) {
		Qp.settings.max_iter_in = max_iter_in.value();
	}
	if (eps_refact != std::nullopt) {
		Qp.settings.eps_refact = eps_refact.value();
	}
	if (nb_iterative_refinement != std::nullopt) {
		Qp.settings.nb_iterative_refinement = nb_iterative_refinement.value();
	}
	if (eps_primal_inf != std::nullopt) {
		Qp.settings.eps_primal_inf = eps_primal_inf.value();
	}
	if (eps_dual_inf != std::nullopt) {
		Qp.settings.eps_dual_inf = eps_dual_inf.value();
	}

	Qp.solve(); // numeric facotisation done here

	return Qp.results;
};

} // namespace sparse
} // namespace qp
} // namespace proxsuite

#endif /* end of include guard PROXSUITE_QP_SPARSE_WRAPPER_HPP */
