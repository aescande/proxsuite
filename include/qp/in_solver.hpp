#ifndef INRIA_LDLT_EQ_SOLVER_HPP_HDWGZKCLS
#define INRIA_LDLT_EQ_SOLVER_HPP_HDWGZKCLS

#include "ldlt/views.hpp"
#include <ldlt/ldlt.hpp>
#include "qp/views.hpp"
#include "ldlt/factorize.hpp"
#include "ldlt/detail/meta.hpp"
#include "ldlt/solve.hpp"
#include "ldlt/update.hpp"
#include "qp/line_search.hpp"
#include "cnpy.hpp"
#include <cmath>

namespace qp {
inline namespace tags {
using namespace ldlt::tags;
}
namespace preconditioner {
struct IdentityPrecond {
	template <typename T>
	void scale_qp_in_place(QpViewBoxMut<T> /*qp*/) const noexcept {}

	template <typename T>
	void scale_primal_in_place(VectorViewMut<T> /*x*/) const noexcept {}
	template <typename T>
	void scale_dual_in_place_in(VectorViewMut<T> /*y*/) const noexcept {}
	template <typename T>
	void scale_dual_in_place_eq(VectorViewMut<T> /*y*/) const noexcept {}

	template <typename T>
	void scale_primal_residual_in_place(VectorViewMut<T> /*x*/) const noexcept {}
	template <typename T>
	void scale_dual_residual_in_place(VectorViewMut<T> /*y*/) const noexcept {}

	template <typename T>
	void unscale_primal_in_place(VectorViewMut<T> /*x*/) const noexcept {}
	template <typename T>
	void unscale_dual_in_place_in(VectorViewMut<T> /*y*/) const noexcept {}
	template <typename T>
	void unscale_dual_in_place_eq(VectorViewMut<T> /*y*/) const noexcept {}

	template <typename T>
	void unscale_primal_residual_in_place_in(VectorViewMut<T> /*x*/) const noexcept {
	}
	template <typename T>
	void unscale_primal_residual_in_place_eq(VectorViewMut<T> /*x*/) const noexcept {
	}
	template <typename T>
	void unscale_dual_residual_in_place(VectorViewMut<T> /*y*/) const noexcept {}
};
} // namespace preconditioner

namespace detail {
using namespace cnpy;
template <typename T>
struct TypeIdentityImpl {
	using Type = T;
};

template <typename T>
using DoNotDeduce = typename TypeIdentityImpl<T>::Type;

template <typename Dst, typename Lhs, typename Rhs>
void mul_add_no_alias(Dst& dst, Lhs const& lhs, Rhs const& rhs) {
	dst.noalias().operator+=(lhs.operator*(rhs));
}
template <typename Dst, typename Lhs, typename Rhs>
void mul_no_alias(Dst& dst, Lhs const& lhs, Rhs const& rhs) {
	dst.setZero();
	mul_add_no_alias(dst, lhs, rhs);
}

struct EqSolverTimer {};
struct QpSolveStats {
	isize n_ext;
	isize n_mu_updates;
	isize n_tot;
};

template <typename T>
void iterative_solve_with_permut_fact( //
		Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
		Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>&  mat,
		T eps,
		isize max_it) {
  
	
	//LDLT_DECL_SCOPE_TIMER("in solver", "factorization", T);
	ldlt::Ldlt<T> ldl{decompose, mat};

	i32 it = 0;
	sol = rhs;
	ldl.solve_in_place(sol);
	auto res = (mat * sol - rhs).eval();
	std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	while (qp::infty_norm(res) >= eps) {
		it += 1;
		if (it >= max_it) {
			break;
		}
		res = -res;
		ldl.solve_in_place(res);
		sol += res;
		res = (mat * sol - rhs);
	}
}

template <typename T>
void refactorize(
	qp::QpViewBox<T> qp_scaled,
	VectorViewMut<isize> current_bijection_map_,
	MatrixViewMut<T,colmajor> kkt,
	isize dim,
	isize n_eq,
	isize n_c,
	isize n_in,
	T mu_eq,
	T mu_in,
	T rho_old,
	T rho_new,
	ldlt::Ldlt<T>& ldl
	){
		auto current_bijection_map = current_bijection_map_.to_eigen();
		
		/*
		auto C_p = qp_scaled.C.to_eigen().eval();
		for (isize i = 0;i<n_in ; i++){
			C_p.row(current_bijection_map(i)) = qp_scaled.C.to_eigen().row(i);
		}
		*/
		auto Htot = kkt.to_eigen(); 
		for (isize i = 0; i < dim; ++i) {
			Htot(i, i) += rho_new - rho_old;
		}
		

		/*
		LDLT_MULTI_WORKSPACE_MEMORY(
			(_htot,Uninit, Mat(dim+n_eq, dim+n_eq),LDLT_CACHELINE_BYTES, T)
		);
		
		auto Htot = _htot.to_eigen().eval();
		Htot.setZero();

		Htot.topLeftCorner(dim, dim) = qp_scaled.H.to_eigen();
		for (isize i = 0; i < dim; ++i) {
			Htot(i, i) += rho;
		}
		
		Htot.block(0,dim,dim,n_eq) = qp_scaled.A.to_eigen().transpose();
		Htot.block(dim,0,n_eq,dim) = qp_scaled.A.to_eigen();
		{
			T tmp_eq = -T(1) / mu_eq;
			for (isize i = 0; i < n_eq; ++i) {
				Htot(dim + i, dim + i) = tmp_eq;
			}
		}
		
		for (isize i = 0; i< n_c ; ++i){
			Htot.block(i+dim+n_eq,0,1,dim) = C_p.row(i) ; 
			Htot.block(0,i+dim+n_eq,dim,1) = C_p.transpose().col(i) ; 
		}
		*/
 
		ldl.factorize(Htot);
		/*
		auto ldl2 = ldlt::Ldlt<T>{decompose, Htot};
		std::cout <<  "diff_mat norm for dim+n_eq " << qp::infty_norm(ldl.reconstructed_matrix() - Htot) << std::endl;
		std::cout <<  "diff_mat norm for dim+n_eq bis " << qp::infty_norm(ldl2.reconstructed_matrix() - Htot) << std::endl;
		cnpy::npy_save_mat(
			"Htot_eq",
			Htot, 
			"w"); 
		*/
		// Add rows

		for (isize j = 0 ; j < n_c; ++j){
			for (isize i = 0 ; i<n_in;++i){
				if (j == current_bijection_map(i)){
					[&]{
					LDLT_MULTI_WORKSPACE_MEMORY(
					(row_,Init, Vec(j+1+n_eq+dim),LDLT_CACHELINE_BYTES, T)
					);
					auto row = row_.to_eigen();
					row.topRows(dim) = qp_scaled.C.to_eigen().row(i) ;
					row(dim+n_eq+j) = -T(1)/mu_in;
					ldl.insert_at(n_eq+dim+j, row); 
					}();
				}
			}
		}
		/*  NEW VERSION TO FINISH
		for (isize i = 0 ; i < n_in ; ++i){
			isize j = current_bijection_map(i); /!\ won't work ; need to get n_c smallest coefficient and add rows in order
			if (j <n_c){
				[&]{
					LDLT_MULTI_WORKSPACE_MEMORY(
					(row_,Init, Vec(j+1+n_eq+dim),LDLT_CACHELINE_BYTES, T)
					);
					auto row = row_.to_eigen();
					row.topRows(dim) = qp_scaled.C.to_eigen().row(i) ;
					row(dim+n_eq+j) = -T(1)/mu_in;
					ldl.insert_at(n_eq+dim+j, row); 
				}
			}
		}
		
		
		for (isize j = 0; j< n_c ; ++j){
			[&]{
			LDLT_MULTI_WORKSPACE_MEMORY(
					(row_,Init, Vec(j+1+n_eq+dim),LDLT_CACHELINE_BYTES, T)
			);
			auto row = row_.to_eigen();
			row.topRows(dim) = C_p.row(j);
			row(dim+n_eq+j) = -T(1)/mu_in;

 			ldl.insert_at(n_eq+dim+j, row); 
			
			// TEST
			/*
			LDLT_MULTI_WORKSPACE_MEMORY(
			(_htot_,Uninit, Mat(dim+n_eq+j+1, dim+n_eq+j+1),LDLT_CACHELINE_BYTES, T)
			);
			
			auto Htot_ = _htot_.to_eigen().eval();
			Htot_.setZero();
			Htot_.topLeftCorner(dim, dim) = qp_scaled.H.to_eigen();
			for (isize i = 0; i < dim; ++i) {
				Htot_(i, i) += rho;
			}
			
			Htot_.block(0,dim,dim,n_eq) = qp_scaled.A.to_eigen().transpose();
			Htot_.block(dim,0,n_eq,dim) = qp_scaled.A.to_eigen();
			{
				T tmp_eq = -T(1) / mu_eq;
				T tmp_in = -T(1) / mu_in;
				for (isize i = 0; i < n_eq; ++i) {
					Htot_(dim + i, dim + i) = tmp_eq;
				}
				
				for (isize i = 0; i <= j; ++i) {
					Htot_(dim + n_eq + i, dim + n_eq + i) = tmp_in;
				}
				
			}
			
			for (isize i = 0; i<= j ; ++i){
				Htot_.block(i+dim+n_eq,0,1,dim) = C_p.row(i) ; 
				Htot_.block(0,i+dim+n_eq,dim,1) = C_p.transpose().col(i) ; 
			}

			std::cout <<  "diff_mat norm for dim+n_eq+j " << qp::infty_norm(ldl.reconstructed_matrix() - Htot_) << std::endl;
			auto ldl_tmp = ldlt::Ldlt<T>{decompose, Htot_};
			std::cout <<  "diff_mat norm for dim+n_eq+j tmp " << qp::infty_norm(ldl_tmp.reconstructed_matrix() - Htot_) << std::endl;
			
			}();
		}
		*/
		
}


template <typename T>
void iterative_residual_osqp(
	qp::QpViewBox<T> qp_scaled,
	isize dim,
	isize n_eq,
	isize n_in,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& err,
	T mu_eq,
	T mu_in,
	T rho,
	ldlt::Ldlt<T>& ldl
	){
		err = (-rhs).eval();
		err.topRows(dim) += (qp_scaled.H).to_eigen() * sol.topRows(dim) + rho * sol.topRows(dim) + (qp_scaled.A).to_eigen().transpose()*sol.middleRows(dim,n_eq) + (qp_scaled.C).to_eigen().transpose()*sol.tail(n_in);
		err.middleRows(dim,n_eq) += (qp_scaled.A).to_eigen() * sol.topRows(dim) - sol.middleRows(dim,n_eq) / mu_eq ;
		err.tail(n_in) += (qp_scaled.C).to_eigen() * sol.topRows(dim) - sol.tail(n_in) / mu_in ;
}

template <typename T>
void iterative_solve_with_permut_fact_osqp( //
		Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& res,
		ldlt::Ldlt<T>& ldl,
		T eps,
		isize max_it,
		qp::QpViewBox<T> qp_scaled,
		isize dim,
		isize n_eq,
		isize n_in,
		T mu_eq,
		T mu_in,
		T rho
		) {

	i32 it = 0;
	sol = rhs;
	ldl.solve_in_place(sol);

	qp::detail::iterative_residual_osqp<T>(
						qp_scaled,
						dim,
						n_eq,
						n_in,
						rhs,
						sol,
						res,
						mu_eq,
						mu_in,
						rho,
						ldl
						);
	std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	
	while (qp::infty_norm(res) >= eps) {
		it += 1;
		if (it >= max_it) {
			break;
		}
		res = -res;
		ldl.solve_in_place(res);
		sol += res; 

		res.setZero();
		qp::detail::iterative_residual_osqp<T>(
						qp_scaled,
						dim,
						n_eq,
						n_in,
						rhs,
						sol,
						res,
						mu_eq,
						mu_in,
						rho,
						ldl
						);
		std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	}
	
}

template <typename T>
void iterative_residual(
	qp::QpViewBox<T> qp_scaled,
	VectorViewMut<isize> current_bijection_map_,
	isize dim,
	isize n_eq,
	isize n_c,
	isize n_in,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& err,
	T mu_eq,
	T mu_in,
	T rho,
	ldlt::Ldlt<T>& ldl
	){
		auto current_bijection_map = current_bijection_map_.to_eigen();

		err = (-rhs).eval();
		err.topRows(dim) += (qp_scaled.H).to_eigen() * sol.topRows(dim) + rho * sol.topRows(dim) + (qp_scaled.A).to_eigen().transpose()*sol.middleRows(dim,n_eq) ;

		for (isize i = 0 ; i< n_in ; i++){
				isize j = current_bijection_map(i);
				if (j<n_c){
					err.topRows(dim) += sol(dim+n_eq+j) * qp_scaled.C.to_eigen().row(i);
					err(dim+n_eq+j) += (  qp_scaled.C.to_eigen().row(i)  ).dot(sol.topRows(dim)) - sol(dim+n_eq+j) / mu_in ;
				}
		}

		err.middleRows(dim,n_eq) += (qp_scaled.A).to_eigen() * sol.topRows(dim) - sol.middleRows(dim,n_eq) / mu_eq ;

}

template <typename T>
void iterative_residual_QPALM(
	qp::QpViewBox<T> qp_scaled,
	VectorViewMut<isize> current_bijection_map_,
	isize dim,
	isize n_eq,
	isize n_c,
	isize n_in,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& err,
	Eigen::Matrix<T, Eigen::Dynamic, 1>& mu,
	T rho,
	ldlt::Ldlt<T>& ldl
	){
		auto current_bijection_map = current_bijection_map_.to_eigen();

		err = (-rhs).eval();
		err.topRows(dim) += (qp_scaled.H).to_eigen() * sol.topRows(dim) + rho * sol.topRows(dim) + (qp_scaled.A).to_eigen().transpose()*sol.middleRows(dim,n_eq) ;

		for (isize i = 0 ; i< n_in ; i++){
				isize j = current_bijection_map(i);
				if (j<n_c){
					err.topRows(dim) += sol(dim+n_eq+j) * qp_scaled.C.to_eigen().row(i);
					err(dim+n_eq+j) += (  qp_scaled.C.to_eigen().row(i)  ).dot(sol.topRows(dim)) - sol(dim+n_eq+j) / mu(n_eq+i) ;
				}
		}

		err.middleRows(dim,n_eq).array() += ((qp_scaled.A).to_eigen() * sol.topRows(dim)).array() - sol.middleRows(dim,n_eq).array() / mu.topRows(n_eq).array() ;

}

template <typename T>
void iterative_solve_with_permut_fact_new( //
		Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& res,
		ldlt::Ldlt<T>& ldl,
		T eps,
		isize max_it,
		qp::QpViewBox<T> qp_scaled,
		VectorViewMut<isize> current_bijection_map_,
		isize dim,
		isize n_eq,
		isize& n_c,
		isize n_in,
		T mu_eq,
		T mu_in,
		T rho
		) {

	i32 it = 0;
	sol = rhs;
	ldl.solve_in_place(sol);

	/*
	LDLT_MULTI_WORKSPACE_MEMORY(
		(err_,Init, Vec(dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T)
	);
	
	auto res = err_.to_eigen().eval();
	res.setZero();
	*/
	qp::detail::iterative_residual<T>(
						qp_scaled,
						current_bijection_map_,
						dim,
						n_eq,
						n_c,
						n_in,
						rhs,
						sol,
						res,
						mu_eq,
						mu_in,
						rho,
						ldl
						);
	std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	
	while (qp::infty_norm(res) >= eps) {
		it += 1;
		if (it >= max_it) {
			break;
		}
		res = -res;
		ldl.solve_in_place(res);
		sol += res; 

		res.setZero();
		qp::detail::iterative_residual<T>(
						qp_scaled,
						current_bijection_map_,
						dim,
						n_eq,
						n_c,
						n_in,
						rhs,
						sol,
						res,
						mu_eq,
						mu_in,
						rho,
						ldl
						);
		std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	}
	
}

template <typename T>
void iterative_solve_with_permut_fact_QPALM( //
		Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& sol,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& res,
		ldlt::Ldlt<T>& ldl,
		T eps,
		isize max_it,
		qp::QpViewBox<T> qp_scaled,
		VectorViewMut<isize> current_bijection_map_,
		isize dim,
		isize n_eq,
		isize& n_c,
		isize n_in,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& mu,
		T rho
		) {

	i32 it = 0;
	sol = rhs;
	ldl.solve_in_place(sol);

	qp::detail::iterative_residual_QPALM<T>(
						qp_scaled,
						current_bijection_map_,
						dim,
						n_eq,
						n_c,
						n_in,
						rhs,
						sol,
						res,
						mu,
						rho,
						ldl
						);
	std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	
	while (qp::infty_norm(res) >= eps) {
		it += 1;
		if (it >= max_it) {
			break;
		}
		res = -res;
		ldl.solve_in_place(res);
		sol += res; 

		res.setZero();
		qp::detail::iterative_residual_QPALM<T>(
						qp_scaled,
						current_bijection_map_,
						dim,
						n_eq,
						n_c,
						n_in,
						rhs,
						sol,
						res,
						mu,
						rho,
						ldl
						);
		std::cout <<"infty_norm(res) " << qp::infty_norm(res) << std::endl;
	}
	
}

template <typename T>
void mu_update(
	T mu_eq_old,
	T mu_eq_new,
	T mu_in_old,
	T mu_in_new,
	isize dim,
	isize n_eq,
	isize& n_c,
	ldlt::Ldlt<T>& ldl,

	qp::QpViewBox<T> qp_scaled,
	T rho
	//VectorViewMut<isize> current_bijection_map_
	){
	T diff = T(0); 
	LDLT_MULTI_WORKSPACE_MEMORY(
	    (e_k_,Init, Vec(dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T)
	);
	auto e_k = e_k_.to_eigen().eval();
	
	if (n_eq > 0){
		diff = T(1)/mu_eq_old - T(1)/mu_eq_new ;

		for (isize i = 0;i<n_eq;i++){
			e_k(dim+i) = T(1);
			ldl.rank_one_update(e_k,diff);
			e_k(dim+i) = T(0);
    	}

	}
	if(n_c >0){
		diff = T(1)/mu_in_old - T(1)/mu_in_new ;
		for (isize i = 0;i<n_c;i++){
			e_k(dim+n_eq+i) = T(1);
			ldl.rank_one_update(e_k,diff);
			e_k(dim+n_eq+i) = T(0);
		}
	}

	// TESTS
	/*
	auto current_bijection_map = current_bijection_map_.to_eigen();
	auto C_p = qp_scaled.C.to_eigen().eval();
	isize n_in = qp_scaled.C.to_eigen().rows();
	for (isize i = 0;i<n_in ; i++){
			C_p.row(current_bijection_map(i)) = qp_scaled.C.to_eigen().row(i);
	}
	LDLT_MULTI_WORKSPACE_MEMORY(
		(_htot,Uninit, Mat(dim+n_eq+n_c, dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T),
		(Htot_reconstruct_,Uninit, Mat(dim+n_eq+n_c, dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T)
	);
	
	auto Htot = _htot.to_eigen().eval();
	auto Htot_reconstruct = Htot_reconstruct_.to_eigen().eval();
	Htot_reconstruct.setZero();
	
	{
			LDLT_DECL_SCOPE_TIMER("in solver", "set H", T);
			Htot.topLeftCorner(dim, dim) = qp_scaled.H.to_eigen();
			for (isize i = 0; i < dim; ++i) {
				Htot(i, i) += rho;
			}
			
			Htot.block(0,dim,dim,n_eq) = qp_scaled.A.to_eigen().transpose();
			Htot.block(dim,0,n_eq,dim) = qp_scaled.A.to_eigen();
			Htot.bottomRightCorner(n_eq+n_c, n_eq+n_c).setZero();
			{
				T tmp_eq = -T(1) / mu_eq_new;
				T tmp_in = -T(1) / mu_in_new;
				for (isize i = 0; i < n_eq; ++i) {
					Htot(dim + i, dim + i) = tmp_eq;
				}
				for (isize i = 0; i < n_c; ++i) {
					Htot(dim + n_eq + i, dim + n_eq + i) = tmp_in;
				}
			}
			for (isize i = 0; i< n_c ; ++i){
				Htot.block(i+dim+n_eq,0,1,dim) = C_p.row(i) ; 
				Htot.block(0,i+dim+n_eq,dim,1) = C_p.transpose().col(i) ; 
			}
	} 
	Htot_reconstruct = ldl.reconstructed_matrix() ; 
	std::cout <<  "diff_mat norm for mu update " << qp::infty_norm(Htot_reconstruct - Htot) << std::endl;
	*/
}

template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
void BCL_update(
		T& primal_feasibility_lhs,
		VectorViewMut<T> primal_residual_in_scaled_u,
		VectorViewMut<T> primal_residual_in_scaled_l,
		VectorViewMut<T> primal_residual_eq_scaled,
		Preconditioner precond,
		T& bcl_eta_ext,
		T& bcl_eta_in,
		T eps_abs,
		isize& n_mu_updates,
		T& bcl_mu_in,
		T& bcl_mu_eq,
		VectorViewMut<T> ye,
		VectorViewMut<T> ze,
		VectorViewMut<T> y,
		VectorViewMut<T> z
		){
		precond.scale_primal_residual_in_place_eq(primal_residual_eq_scaled);
		precond.scale_primal_residual_in_place_in(primal_residual_in_scaled_l);
		precond.scale_primal_residual_in_place_in(primal_residual_in_scaled_u);
		T primal_feasibility_eq_lhs = infty_norm(primal_residual_eq_scaled.to_eigen());
		T primal_feasibility_in_lhs = max2(infty_norm(primal_residual_in_scaled_l.to_eigen()),infty_norm(primal_residual_in_scaled_u.to_eigen()));
		T tmp = max2(primal_feasibility_eq_lhs,primal_feasibility_in_lhs);
		//std::cout << "tmp for BCL " << tmp << std::endl;
		if (tmp <= bcl_eta_ext) {
			std::cout << "good step"<< std::endl;
			bcl_eta_ext = bcl_eta_ext / pow(bcl_mu_in, T(0.9));
			bcl_eta_in = max2(bcl_eta_in /bcl_mu_in,eps_abs);
		} else {
			std::cout << "bad step"<< std::endl;
			y.to_eigen() = ye.to_eigen();
			z.to_eigen() = ze.to_eigen();
			T new_bcl_mu_in = std::min(bcl_mu_in * T(10), T(1e8));
			T new_bcl_mu_eq = std::min(bcl_mu_eq * T(10), T(1e10));
			if (bcl_mu_in != new_bcl_mu_in || bcl_mu_eq != new_bcl_mu_eq) {
					{
					++n_mu_updates;
					}
			}	
			
			bcl_mu_eq = new_bcl_mu_eq;
			bcl_mu_in = new_bcl_mu_in;
			bcl_eta_ext = (T(1)/pow(T(10),T(0.1))) / pow(bcl_mu_in, T(0.1));
			bcl_eta_in = max2(  T(1) / bcl_mu_in  ,eps_abs);
	}
}


template <typename T>
void BCL_update_fact(
		T& primal_feasibility_lhs,
		T& bcl_eta_ext,
		T& bcl_eta_in,
		T eps_abs,
		isize& n_mu_updates,
		T& bcl_mu_in,
		T& bcl_mu_eq,
		VectorViewMut<T> ye,
		VectorViewMut<T> ze,
		VectorViewMut<T> y,
		VectorViewMut<T> z,

		isize dim,
		isize n_eq,
		isize& n_c,
		ldlt::Ldlt<T>& ldl,
		qp::QpViewBox<T> qp_scaled,
		T rho,
		T beta
		){
		/*
		precond.scale_primal_residual_in_place_eq(primal_residual_eq_scaled);
		precond.scale_primal_residual_in_place_in(primal_residual_in_scaled_l);
		precond.scale_primal_residual_in_place_in(primal_residual_in_scaled_u);
		T primal_feasibility_eq_lhs = infty_norm(primal_residual_eq_scaled.to_eigen());
		T primal_feasibility_in_lhs = max2(infty_norm(primal_residual_in_scaled_l.to_eigen()),infty_norm(primal_residual_in_scaled_u.to_eigen()));
		T tmp = max2(primal_feasibility_eq_lhs,primal_feasibility_in_lhs); 
		std::cout << "tmp for BCL " << tmp << std::endl;
		*/
		if (primal_feasibility_lhs <= bcl_eta_ext) {
			std::cout << "good step"<< std::endl;
			bcl_eta_ext = bcl_eta_ext / pow(bcl_mu_in, beta);
			bcl_eta_in = max2(bcl_eta_in /bcl_mu_in,eps_abs);
		} else {
			std::cout << "bad step"<< std::endl;
			y.to_eigen() = ye.to_eigen();
			z.to_eigen() = ze.to_eigen();
			T new_bcl_mu_in = std::min(bcl_mu_in * T(10), T(1e8));
			T new_bcl_mu_eq = std::min(bcl_mu_eq * T(10), T(1e10));
			if (bcl_mu_in != new_bcl_mu_in || bcl_mu_eq != new_bcl_mu_eq) {
					{
					++n_mu_updates;
					}
			}	
			qp::detail::mu_update(
				bcl_mu_eq,
				new_bcl_mu_eq,
				bcl_mu_in,
				new_bcl_mu_in,
				dim,
				n_eq,
				n_c,
				ldl,

				qp_scaled,
				rho
			);
			bcl_mu_eq = new_bcl_mu_eq;
			bcl_mu_in = new_bcl_mu_in;
			bcl_eta_ext = (T(1)/pow(T(10),T(0.1))) / pow(bcl_mu_in, T(0.1));
			bcl_eta_in = max2(  T(1) / bcl_mu_in  ,eps_abs);
	}
}


template <typename T>
void QPALM_mu_update(
		T& primal_feasibility_lhs,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq_scaled,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_in_scaled_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq_scaled_old,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_in_scaled_in_old,	
		T& bcl_eta_ext,
		T& bcl_eta_in,
		T eps_abs,
		isize& n_mu_updates,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& mu,
		isize dim,
		isize n_eq,
		isize& n_c,
		ldlt::Ldlt<T>& ldl,
		qp::QpViewBox<T> qp_scaled,
		T rho,
		T theta,
		T sigmaMax,
		T Delta
		){
		for (isize i = 0; i < n_eq ; ++i){
			if (primal_residual_eq_scaled(i) >= theta * primal_residual_eq_scaled_old(i)){
				T mu_eq_new = std::min( sigmaMax , max2(mu(i) *  Delta * primal_residual_eq_scaled(i) / primal_feasibility_lhs , mu(i) ) ) ;
				
				if (n_eq > 0){
					LDLT_MULTI_WORKSPACE_MEMORY(
					(e_k_,Init, Vec(dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T)
					);
					auto e_k = e_k_.to_eigen().eval();
					T diff = T(1)/mu(i) - T(1)/mu_eq_new ;
					e_k(dim+i) = T(1);
					ldl.rank_one_update(e_k,diff);
					e_k(dim+i) = T(0);
					mu(i) = mu_eq_new;
				}
			}
		}

		for (isize i = 0 ; i<n_c;++i){
			if (primal_residual_in_scaled_l(i) >= theta * primal_residual_in_scaled_in_old(i)){
				T mu_in_new = std::min( sigmaMax , max2(mu(n_eq+i) *  Delta * primal_residual_in_scaled_l(i) / primal_feasibility_lhs , mu(n_eq+i) ) ) ;
				
				if (n_c > 0){
					LDLT_MULTI_WORKSPACE_MEMORY(
					(e_k_,Init, Vec(dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T)
					);
					auto e_k = e_k_.to_eigen().eval();
					T diff = T(1)/mu(n_eq+i) - T(1)/mu_in_new ;
					e_k(dim+i) = T(1);
					ldl.rank_one_update(e_k,diff);
					e_k(dim+i) = T(0);
					mu(n_eq+i)= mu_in_new;
				}
			}
		}
}


template <typename T>
void QPALM_update_fact(
		T& primal_feasibility_lhs,	
		T& bcl_eta_ext,
		T& bcl_eta_in,
		T eps_abs,
		isize& n_mu_updates,
		T& bcl_mu_in,
		T& bcl_mu_eq,
		VectorViewMut<T> xe,
		VectorViewMut<T> ye,
		VectorViewMut<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
		VectorViewMut<T> z,

		isize dim,
		isize n_eq,
		isize& n_c,
		ldlt::Ldlt<T>& ldl,
		qp::QpViewBox<T> qp_scaled,
		T rho
		){ 

		if (primal_feasibility_lhs <= bcl_eta_ext) {
			std::cout << "good step"<< std::endl;
			bcl_eta_ext = bcl_eta_ext / T(10);
			bcl_eta_in = max2(bcl_eta_in /T(10),eps_abs);
			ye.to_eigen() = y.to_eigen();
			ze.to_eigen() = z.to_eigen();
			xe.to_eigen() = x.to_eigen();
		} else {
			std::cout << "bad step"<< std::endl;
			bcl_eta_in = max2(bcl_eta_in /T(10),eps_abs);
			T new_bcl_mu_in = std::min(bcl_mu_in * T(10), T(1e8));
			T new_bcl_mu_eq = std::min(bcl_mu_eq * T(10), T(1e10));
			if (bcl_mu_in != new_bcl_mu_in || bcl_mu_eq != new_bcl_mu_eq) {
					{
					++n_mu_updates;
					}
			}	
			qp::detail::mu_update(
				bcl_mu_eq,
				new_bcl_mu_eq,
				bcl_mu_in,
				new_bcl_mu_in, 
				dim,
				n_eq,
				n_c,
				ldl,

				qp_scaled,
				rho
			);
			bcl_mu_eq = new_bcl_mu_eq;
			bcl_mu_in = new_bcl_mu_in;
			//bcl_eta_ext = (T(1)/pow(T(10),T(0.1))) / pow(bcl_mu_in, T(0.1));
			//bcl_eta_in = max2(  T(1) / bcl_mu_in  ,eps_abs);
	}
}


template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
void global_primal_residual(
			T& primal_feasibility_lhs,
			T& primal_feasibility_eq_rhs_0,
        	T& primal_feasibility_in_rhs_0,
			//T& primal_feasibility_eq_lhs,
			//T& primal_feasibility_in_lhs,
			Eigen::Matrix<T, Eigen::Dynamic, 1>&  primal_residual_eq_scaled,
			Eigen::Matrix<T, Eigen::Dynamic, 1>&  primal_residual_in_scaled_u,
			Eigen::Matrix<T, Eigen::Dynamic, 1>&  primal_residual_in_scaled_l,
			//Eigen::Matrix<T, Eigen::Dynamic, 1>&  residual_scaled_tmp,
			qp::QpViewBox<T> qp,
			qp::QpViewBoxMut<T> qp_scaled,
			Preconditioner precond,
			VectorViewMut<T> x,
			isize dim,
			isize n_eq,
			isize n_in
		){
				LDLT_DECL_SCOPE_TIMER("in solver", "primal residual", T);
				auto A_ = qp_scaled.A.as_const().to_eigen();
				auto C_ = qp_scaled.C.as_const().to_eigen();
				auto x_ = x.as_const().to_eigen();
				auto b_ = qp_scaled.b.as_const().to_eigen();
				//auto l_ = qp_scaled.l.as_const().to_eigen();
				//auto u_ = qp_scaled.u.as_const().to_eigen();

				//residual_scaled_tmp.setZero();
				// A×x - b and Cx - u and Cx - l  /!\ optimization surely possible
				primal_residual_eq_scaled.setZero();
				primal_residual_eq_scaled.noalias() += A_ * x_;
    
				primal_residual_in_scaled_u.setZero();
				primal_residual_in_scaled_u.noalias() += C_ * x_;

				precond.unscale_primal_residual_in_place_eq(
							VectorViewMut<T>{from_eigen, primal_residual_eq_scaled});
				primal_feasibility_eq_rhs_0 = infty_norm(primal_residual_eq_scaled);
				precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, primal_residual_in_scaled_u});
				primal_feasibility_in_rhs_0 = infty_norm(primal_residual_in_scaled_u);

				primal_residual_eq_scaled -= qp.b.to_eigen();
				primal_residual_in_scaled_l = ( (primal_residual_in_scaled_u-qp.u.to_eigen() ).array() > T(0)).select(primal_residual_in_scaled_u-qp.u.to_eigen(), Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in))
											  + ( (primal_residual_in_scaled_u-qp.l.to_eigen() ).array() < T(0)).select(primal_residual_in_scaled_u-qp.l.to_eigen(), Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)) ;
				
				
				/*		
				primal_residual_in_scaled_l.setZero();
				primal_residual_in_scaled_l.noalias() += C_ * x_;

				
				{
					auto w_eq = residual_scaled_tmp.middleRows(dim,n_eq);
                    auto w_in = residual_scaled_tmp.bottomRows(n_in);
					w_eq = primal_residual_eq_scaled;
                    w_in = primal_residual_in_scaled_u;
					precond.unscale_primal_residual_in_place_eq(
							VectorViewMut<T>{from_eigen, w_eq});
                    precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, w_in});
					primal_feasibility_eq_rhs_0 = infty_norm(w_eq);
                    primal_feasibility_in_rhs_0 = infty_norm(w_in);
				}
				
				primal_residual_eq_scaled -= b_;
                primal_residual_in_scaled_u -= u_ ;
                primal_residual_in_scaled_l -= l_ ;

                primal_residual_in_scaled_u = ( primal_residual_in_scaled_u.array() > T(0)).select(primal_residual_in_scaled_u, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)); 
				primal_residual_in_scaled_l = ( primal_residual_in_scaled_l.array() < T(0)).select(primal_residual_in_scaled_l, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)); 

				precond.unscale_primal_residual_in_place_eq(
						VectorViewMut<T>{from_eigen, primal_residual_eq_scaled});
                precond.unscale_primal_residual_in_place_in(
						VectorViewMut<T>{from_eigen, primal_residual_in_scaled_u});
                precond.unscale_primal_residual_in_place_in(
						VectorViewMut<T>{from_eigen, primal_residual_in_scaled_l});
				*/
				//primal_feasibility_eq_lhs = infty_norm(primal_residual_eq_scaled);
				primal_feasibility_lhs = max2(infty_norm(primal_residual_in_scaled_l),infty_norm(primal_residual_eq_scaled));
				precond.scale_primal_residual_in_place_eq(
						VectorViewMut<T>{from_eigen, primal_residual_eq_scaled});

                //primal_feasibility_in_lhs = max2(infty_norm(primal_residual_in_scaled_l),infty_norm(primal_residual_in_scaled_u));
                //primal_feasibility_lhs = max2(primal_feasibility_eq_lhs,primal_feasibility_in_lhs);
				
				
				
}


template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
void global_dual_residual(
			T& dual_feasibility_lhs,
			T& dual_feasibility_rhs_0,
			T& dual_feasibility_rhs_1,
        	T& dual_feasibility_rhs_3,
			Eigen::Matrix<T, Eigen::Dynamic, 1>&  dual_residual_scaled,
			Eigen::Matrix<T, Eigen::Dynamic, 1>&  dw_aug,
			qp::QpViewBoxMut<T> qp_scaled,
			Preconditioner precond,
			VectorViewMut<T> x,
			VectorViewMut<T> y,
			VectorViewMut<T> z,
			isize dim,
			isize n_eq,
			isize n_in
		){
			LDLT_DECL_SCOPE_TIMER("in solver", "dual residual", T);
			auto H_ = qp_scaled.H.as_const().to_eigen();
			auto A_ = qp_scaled.A.as_const().to_eigen();
            auto C_ = qp_scaled.C.as_const().to_eigen();
			auto x_ = x.as_const().to_eigen();
			auto y_ = y.as_const().to_eigen();
            auto z_ = z.as_const().to_eigen();
			auto g_ = qp_scaled.g.as_const().to_eigen();
			

			dual_residual_scaled = g_;
			
			//auto w = residual_scaled_tmp.topRows(dim);
			//w.setZero();
			dw_aug.topRows(dim).setZero();

			dw_aug.topRows(dim).noalias() = H_ * x_;
			dual_residual_scaled += dw_aug.topRows(dim); 
			precond.unscale_dual_residual_in_place(VectorViewMut<T>{from_eigen, dw_aug.topRows(dim)});
			dual_feasibility_rhs_0 = infty_norm(dw_aug.topRows(dim));

			dw_aug.topRows(dim).setZero();
			dw_aug.topRows(dim).noalias() = A_.transpose() * y_;
			dual_residual_scaled += dw_aug.topRows(dim); 
			precond.unscale_dual_residual_in_place(VectorViewMut<T>{from_eigen, dw_aug.topRows(dim)});
			dual_feasibility_rhs_1 = infty_norm(dw_aug.topRows(dim));

			dw_aug.topRows(dim).setZero();
			dw_aug.topRows(dim).noalias() = C_.transpose() * z_;
			dual_residual_scaled += dw_aug.topRows(dim); 
			precond.unscale_dual_residual_in_place(VectorViewMut<T>{from_eigen, dw_aug.topRows(dim)});
			dual_feasibility_rhs_3 = infty_norm(dw_aug.topRows(dim));

			precond.unscale_dual_residual_in_place(
					VectorViewMut<T>{from_eigen, dual_residual_scaled});

			dual_feasibility_lhs = infty_norm(dual_residual_scaled);

			precond.scale_dual_residual_in_place(
					VectorViewMut<T>{from_eigen, dual_residual_scaled});
}

template<typename T> 
T SaddlePointError(
			qp::QpViewBox<T> qp_scaled,
			VectorViewMut<T> x,
			VectorViewMut<T> y,
        	VectorViewMut<T> z,
			VectorView<T> xe,
			VectorView<T> ye,
        	VectorView<T> ze,
			T mu_eq,
			T mu_in,
			T rho,
			isize n_in
			){
			
			auto H_ = qp_scaled.H.to_eigen();
			auto g_ = qp_scaled.g.to_eigen();
			auto A_ = qp_scaled.A.to_eigen();
			auto C_ = qp_scaled.C.to_eigen();
			auto x_ = x.as_const().to_eigen();
			auto x_e = xe.to_eigen();
			auto y_ = y.as_const().to_eigen();
			auto y_e = ye.to_eigen();
			auto z_ = z.as_const().to_eigen();
			auto z_e = ze.to_eigen();
			auto b_ = qp_scaled.b.to_eigen();
			auto l_ = qp_scaled.l.to_eigen();
			auto u_ = qp_scaled.u.to_eigen();

			auto prim_in_u =  C_*x_-u_-(z_-z_e)/mu_in ; 
			auto prim_in_l = C_*x_-l_-(z_-z_e)/mu_in ; 

			T prim_eq_e = infty_norm(A_*x_-b_-(y_-y_e)/mu_eq) ; 
			T dual_e = infty_norm(H_*x_ + rho * (x_-x_e) + g_ + A_.transpose()*y_ + C_.transpose()*z_);
			T err = max2(prim_eq_e,dual_e);

			T prim_in_e(0);

			for (isize i = 0 ; i< n_in ; i=i+1){
				if (z_(i) >0){
					prim_in_e = max2(prim_in_e,std::abs(prim_in_u(i)));
				}else if (z_(i) < 0){
					prim_in_e = max2(prim_in_e,std::abs(prim_in_l(i)));
				}else{
					prim_in_e = max2(prim_in_e,max2(prim_in_u(i),T(0))) ;
					prim_in_e = max2(prim_in_e, std::abs(std::min(prim_in_l(i),T(0))) );
				}
			}
			err = max2(err,prim_in_e);
			return err;
}

template<typename T> 
T SaddlePoint(
			qp::QpViewBox<T> qp_scaled,
			VectorViewMut<T> x,
			VectorViewMut<T> y,
        	VectorViewMut<T> z,
			VectorView<T> xe,
			VectorView<T> ye,
        	VectorView<T> ze,
			T mu_eq,
			T mu_in,
			T rho,
			isize n_in,
			VectorViewMut<T> prim_in_u,
			VectorViewMut<T> prim_in_l,
			VectorViewMut<T> prim_eq,
			VectorViewMut<T> dual_eq
			){
			
			auto H_ = qp_scaled.H.to_eigen();
			auto g_ = qp_scaled.g.to_eigen();
			auto A_ = qp_scaled.A.to_eigen();
			auto C_ = qp_scaled.C.to_eigen();
			auto x_ = x.as_const().to_eigen();
			auto x_e = xe.to_eigen();
			auto y_ = y.as_const().to_eigen();
			auto y_e = ye.to_eigen();
			auto z_ = z.as_const().to_eigen();
			auto z_e = ze.to_eigen();
			auto b_ = qp_scaled.b.to_eigen();
			auto l_ = qp_scaled.l.to_eigen();
			auto u_ = qp_scaled.u.to_eigen();

			prim_in_u.to_eigen().array() -=  (z_/mu_in).array() ; 
			prim_in_l.to_eigen().array() -= (z_/mu_in).array() ; 
			T prim_eq_e = infty_norm(prim_eq.to_eigen()) ; 
			dual_eq.to_eigen().array() += (C_.transpose()*z_).array();
			T dual_e = infty_norm(dual_eq.to_eigen());
			T err = max2(prim_eq_e,dual_e);

			T prim_in_e(0);

			for (isize i = 0 ; i< n_in ; i=i+1){
				if (z_(i) >0){
					prim_in_e = max2(prim_in_e,std::abs(prim_in_u(i)));
				}else if (z_(i) < 0){
					prim_in_e = max2(prim_in_e,std::abs(prim_in_l(i)));
				}else{
					prim_in_e = max2(prim_in_e,max2(prim_in_u(i),T(0))) ;
					prim_in_e = max2(prim_in_e, std::abs(std::min(prim_in_l(i),T(0))) );
				}
			}
			err = max2(err,prim_in_e);
			return err;
}

template<typename T>
void newton_step(
		qp::QpViewBox<T> qp_scaled,
		VectorView<T> x,
		VectorView<T> xe,
		VectorView<T> ye,
		VectorView<T> ze,
		VectorViewMut<T> dx,
		T mu_eq,
		T mu_in,
		T rho,
		T eps,
		isize dim,
		isize n_eq,
		isize n_in
	){
		
		auto H_ = qp_scaled.H.to_eigen();
		auto g_ = qp_scaled.g.to_eigen();
		auto A_ = qp_scaled.A.to_eigen();
		auto C_ = qp_scaled.C.to_eigen();
		auto x_ = x.to_eigen();
		auto x_e = xe.to_eigen();
		auto y_e = ye.to_eigen();
		auto z_e = ze.to_eigen().eval();
		auto b_ = qp_scaled.b.to_eigen();
		auto l_ = qp_scaled.l.to_eigen();
		auto u_ = qp_scaled.u.to_eigen();
		
		auto prim_in_u =  (C_*x_-u_).eval() ; 
		auto prim_in_l = (C_*x_-l_).eval() ; 
		
		Eigen::Matrix<T, Eigen::Dynamic, 1> tmp_u = prim_in_u + z_e/mu_in ; 
		Eigen::Matrix<T, Eigen::Dynamic, 1> tmp_l = prim_in_l + z_e/mu_in ; 
		Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_u = (tmp_u.array() > T(0)).matrix();
		Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_l = (tmp_l.array() < T(0)).matrix();

		Eigen::Matrix<bool, Eigen::Dynamic, 1> active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

		isize num_active_inequalities = active_inequalities.count();
		isize inner_pb_dim = dim + n_eq + num_active_inequalities;
		
		LDLT_MULTI_WORKSPACE_MEMORY(
			(_htot,Uninit, Mat(inner_pb_dim, inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T)
		);
		
		auto Htot = _htot.to_eigen().eval();
		auto rhs = _rhs.to_eigen().eval();
		//rhs.setZero();
		Htot.setZero();

		auto dw = _dw.to_eigen().eval();
		dw.setZero();
		
		{

				auto z_pos = (C_*x_-u_ + z_e/mu_in).eval() ;
				auto z_neg = (C_*x_-l_ + z_e/mu_in).eval() ; 

				z_pos =  (z_pos.array() >= T(0)).select(z_pos, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in));

				z_neg =  (z_neg.array() <= T(0)).select(z_neg, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in));

				rhs.topRows(dim) =  -(H_*x_ + g_ + (x_-x_e) * rho + mu_eq * A_.transpose() * ( A_*x_-b_ +y_e/mu_eq ) + mu_in * C_.transpose() * (  z_pos + z_neg )  );
				LDLT_DECL_SCOPE_TIMER("in solver", "set H", T);
				Htot.topLeftCorner(dim, dim) = H_;
				for (isize i = 0; i < dim; ++i) {
					Htot(i, i) += rho;
				}
				
				Htot.block(0,dim,dim,n_eq) = A_.transpose();
				Htot.block(dim,0,n_eq,dim) = A_;
				Htot.bottomRightCorner(n_eq+num_active_inequalities, n_eq+num_active_inequalities).setZero();
				{
					T tmp_eq = -T(1) / mu_eq;
					T tmp_in = -T(1) / mu_in;
					for (isize i = 0; i < n_eq; ++i) {
						Htot(dim + i, dim + i) = tmp_eq;
					}
					for (isize i = 0; i < num_active_inequalities; ++i) {
						Htot(dim + n_eq + i, dim + n_eq + i) = tmp_in;
					}
				}
				isize j = 0;
				for (isize i = 0; i< n_in ; ++i){
					if (l_active_set_n_u(i)){
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						j+=1 ;
					}else if (l_active_set_n_l(i)){
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						j+=1;
					}	
				}
		}

		iterative_solve_with_permut_fact( //
			rhs,
			dw,
			Htot,
			eps,
			isize(2));
		
		dx.to_eigen() = dw.topRows(dim);
}


template<typename T>
void newton_step_osqp(
		qp::QpViewBox<T> qp_scaled,
		VectorView<T> xe,
		VectorView<T> ye,
		VectorView<T> ze,
		VectorViewMut<T> dw_,
		VectorViewMut<T> err_,
		T mu_eq,
		T mu_in,
		T rho,
		isize dim,
		isize n_eq,
		isize n_in,
		ldlt::Ldlt<T>& ldl,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& rhs,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dual_residual,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_in
	){
		
		auto C_ = qp_scaled.C.to_eigen();
		auto x_e = xe.to_eigen();
		auto z_e = ze.to_eigen().eval();
		auto dw = dw_.to_eigen().eval();
		auto res =  err_.to_eigen().eval();
		dw.setZero();
		res.setZero();
		
		isize inner_pb_dim = dim + n_eq + n_in;

		/*
		rhs.topRows(dim) =  rho * x_e - g_;
		rhs.middleRows(dim,n_eq) = z_e.middleRows(dim,n_eq) - 1./mu_eq * y_e.middleRows(dim,n_eq);
		rhs.tail(n_in) =  z_e.tail(n_in) - 1./mu_in * y_e.tail(n_in) ; 
		*/

		rhs.topRows(dim) = -dual_residual;
		rhs.middleRows(dim,n_eq) = -primal_residual_eq ;
		rhs.tail(n_in) =  -(C_ * x_e - z_e.tail(n_in)); 

		iterative_solve_with_permut_fact_osqp(
				rhs,
				dw,
				res,
				ldl,
				T(1e-5),
				isize(10),
				qp_scaled,
				dim,
				n_eq,
				n_in,
				mu_eq,
				mu_in,
				rho
		);
		/*
		iterative_solve_with_permut_fact( //
			rhs,
			dw,
			Htot,
			T(1),
			isize(10));
		*/
		dw_.to_eigen() = dw ;

}

template<typename T>
void newton_step_new(
		qp::QpViewBox<T> qp_scaled,
		VectorView<T> x,
		VectorView<T> xe,
		VectorView<T> ye,
		VectorView<T> ze,
		VectorViewMut<T> dx,
		T mu_eq,
		T mu_in,
		T rho,
		T eps,
		isize dim,
		isize n_eq,
		isize n_in,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_pos,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_neg,
		Eigen::Matrix<T,Eigen::Dynamic,1>& res_y,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dual_for_eq,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_u,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_l,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& active_inequalities
	){
		
		auto H_ = qp_scaled.H.to_eigen();
		auto A_ = qp_scaled.A.to_eigen();
		auto C_ = qp_scaled.C.to_eigen();
		
		l_active_set_n_u = (z_pos.array() > T(0)).matrix();
		l_active_set_n_l = (z_neg.array() < T(0)).matrix();

		active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

		isize num_active_inequalities = active_inequalities.count();
		isize inner_pb_dim = dim + n_eq + num_active_inequalities;
		
		LDLT_MULTI_WORKSPACE_MEMORY(
			(_htot,Uninit, Mat(inner_pb_dim, inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T)
		);
		
		auto Htot = _htot.to_eigen().eval();
		auto rhs = _rhs.to_eigen().eval();
		//rhs.setZero();
		Htot.setZero();

		auto dw = _dw.to_eigen().eval();
		dw.setZero();
		
		{
				rhs.topRows(dim) -=  dual_for_eq ;
				for (isize j = 0 ; j < n_in; ++j){
					rhs.topRows(dim) -= mu_in*(max2(z_pos(j),T(0)) + std::min(z_neg(j),T(0))) * C_.row(j) ; 
				}
				LDLT_DECL_SCOPE_TIMER("in solver", "set H", T);
				Htot.topLeftCorner(dim, dim) = H_;
				for (isize i = 0; i < dim; ++i) {
					Htot(i, i) += rho;
				}
				
				Htot.block(0,dim,dim,n_eq) = A_.transpose();
				Htot.block(dim,0,n_eq,dim) = A_;
				Htot.bottomRightCorner(n_eq+num_active_inequalities, n_eq+num_active_inequalities).setZero();
				{
					T tmp_eq = -T(1) / mu_eq;
					T tmp_in = -T(1) / mu_in;
					for (isize i = 0; i < n_eq; ++i) {
						Htot(dim + i, dim + i) = tmp_eq;
					}
					for (isize i = 0; i < num_active_inequalities; ++i) {
						Htot(dim + n_eq + i, dim + n_eq + i) = tmp_in;
					}
				}
				isize j = 0;
				for (isize i = 0; i< n_in ; ++i){
					if (l_active_set_n_u(i)){
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						j+=1 ;
					}else if (l_active_set_n_l(i)){
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						j+=1;
					}	
				}
		}

		iterative_solve_with_permut_fact( //
			rhs,
			dw,
			Htot,
			eps,
			isize(2));
		
		dx.to_eigen() = dw.topRows(dim);
}


template<typename T>
void newton_step_fact(
		qp::QpViewBox<T> qp_scaled,
		VectorView<T> x,
		VectorView<T> xe,
		VectorView<T> ye,
		VectorView<T> ze,
		VectorViewMut<T> dx,
		T mu_eq,
		T mu_in,
		T rho,
		T eps,
		isize dim,
		isize n_eq,
		isize n_in,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_pos,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_neg,
		Eigen::Matrix<T,Eigen::Dynamic,1>& res_y,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dual_for_eq,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_u,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_l,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& active_inequalities,
		ldlt::Ldlt<T>& ldl,
		VectorViewMut<isize> current_bijection_map,
		isize& n_c
	){
		
		auto H_ = qp_scaled.H.to_eigen();
		auto A_ = qp_scaled.A.to_eigen();
		auto C_ = qp_scaled.C.to_eigen();
		
		l_active_set_n_u = (z_pos.array() > T(0)).matrix();
		l_active_set_n_l = (z_neg.array() < T(0)).matrix();

		active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

		isize num_active_inequalities = active_inequalities.count();
		isize inner_pb_dim = dim + n_eq + num_active_inequalities;
		
		LDLT_MULTI_WORKSPACE_MEMORY(
			(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_err,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T)
		);
		
		auto rhs = _rhs.to_eigen().eval();
		auto dw = _dw.to_eigen().eval();
		auto err = _err.to_eigen().eval();
		{
				qp::line_search::active_set_change_new(VectorView<bool>{from_eigen,active_inequalities},
												   current_bijection_map,
												   n_c,
												   n_in,
												   dim,
												   n_eq,
												   ldl,
												   qp_scaled,
												   mu_in,
												   mu_eq,
												   rho
													);
				rhs.topRows(dim) -=  dual_for_eq ;
				for (isize j = 0 ; j < n_in; ++j){
					rhs.topRows(dim) -= mu_in*(max2(z_pos(j),T(0)) + std::min(z_neg(j),T(0))) * C_.row(j) ; 
				}
		}

		iterative_solve_with_permut_fact_new( //
			rhs,
			dw,
			err,
			ldl,
			eps,
			isize(5),
			qp_scaled,
			current_bijection_map,
			dim,
			n_eq,
			n_c,
			n_in,
			mu_eq,
			mu_in,
			rho
			);
		
		dx.to_eigen() = dw.topRows(dim);
}

template<typename T>
void newton_step_QPALM(
		qp::QpViewBox<T> qp_scaled,
		VectorView<T> x,
		VectorView<T> xe,
		VectorView<T> ye,
		VectorView<T> ze,
		VectorViewMut<T> dx,
		Eigen::Matrix<T,Eigen::Dynamic,1>& mu,
		T rho,
		T eps,
		isize dim,
		isize n_eq,
		isize n_in,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_pos,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_neg,
		Eigen::Matrix<T,Eigen::Dynamic,1>& res_y,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dual_for_eq,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_u,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_l,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& active_inequalities,
		ldlt::Ldlt<T>& ldl,
		VectorViewMut<isize> current_bijection_map,
		isize& n_c
	){
		
		auto H_ = qp_scaled.H.to_eigen();
		auto A_ = qp_scaled.A.to_eigen();
		auto C_ = qp_scaled.C.to_eigen();
		
		l_active_set_n_u = (z_pos.array() > T(0)).matrix();
		l_active_set_n_l = (z_neg.array() < T(0)).matrix();

		active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

		isize num_active_inequalities = active_inequalities.count();
		isize inner_pb_dim = dim + n_eq + num_active_inequalities;
		
		LDLT_MULTI_WORKSPACE_MEMORY(
			(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_err,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T)
		);
		
		auto rhs = _rhs.to_eigen().eval();
		auto dw = _dw.to_eigen().eval();
		auto err = _err.to_eigen().eval();
		{
				qp::line_search::active_set_change_QPALM(VectorView<bool>{from_eigen,active_inequalities},
												   current_bijection_map,
												   n_c,
												   n_in,
												   dim,
												   n_eq,
												   ldl,
												   qp_scaled,
												   mu,
												   rho
													);
				rhs.topRows(dim) -=  dual_for_eq ;
				for (isize j = 0 ; j < n_in; ++j){
					rhs.topRows(dim) -= mu(n_eq+j)*(max2(z_pos(j),T(0)) + std::min(z_neg(j),T(0))) * C_.row(j) ; 
				}
		}

		iterative_solve_with_permut_fact_QPALM( //
			rhs,
			dw,
			err,
			ldl,
			eps,
			isize(5),
			qp_scaled,
			current_bijection_map,
			dim,
			n_eq,
			n_c,
			n_in,
			mu,
			rho
			);
		
		dx.to_eigen() = dw.topRows(dim);
}

template<typename T,typename Preconditioner>
T initial_guess_fact(
		VectorView<T> xe,
		VectorView<T> ye,
        VectorView<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBoxMut<T> qp_scaled,
		T mu_in,
		T mu_eq,
		T rho,
		T eps_int,
		Preconditioner precond,
		isize dim,
		isize n_eq,
		isize n_in,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& prim_in_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& prim_in_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dual_for_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& d_dual_for_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& Cdx_,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& d_primal_residual_eq,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_u,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_l,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& active_inequalities,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dw_aug,
		ldlt::Ldlt<T>& ldl,
		VectorViewMut<isize> current_bijection_map,
		isize& n_c,
		T R
		){
			
			auto H_ = qp_scaled.H.as_const().to_eigen();
			auto g_ = qp_scaled.g.as_const().to_eigen();
			auto A_ = qp_scaled.A.as_const().to_eigen();
			auto C_ = qp_scaled.C.as_const().to_eigen();
			auto x_ = x.to_eigen();
			auto z_ = z.to_eigen();
			auto z_e = ze.to_eigen().eval();
			auto l_ = qp_scaled.l.as_const().to_eigen();
			auto u_ = qp_scaled.u.as_const().to_eigen();

			prim_in_u =  (C_*x_-u_) ; 
			prim_in_l = (C_*x_-l_) ; 

			
			precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_u});
			precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_l});
			precond.unscale_dual_in_place_in(
							VectorViewMut<T>{from_eigen, z_e}); 
			
			prim_in_u.array() += (z_e/mu_in).array() ; 

			prim_in_l.array() += (z_e/mu_in).array() ; 

			l_active_set_n_u = (prim_in_u.array() >= T(0)).matrix();
			l_active_set_n_l = (prim_in_l.array() <= T(0)).matrix();

			active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

			prim_in_u.array() -= (z_e/mu_in).array() ; 
			prim_in_l.array() -= (z_e/mu_in).array() ; 

			
			precond.scale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_u});
			precond.scale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_l});
			precond.scale_dual_in_place_in(
							VectorViewMut<T>{from_eigen, z_e});
			
			// rescale value
			isize num_active_inequalities = active_inequalities.count();
			isize inner_pb_dim = dim + n_eq + num_active_inequalities;

			qp::line_search::active_set_change_new(VectorView<bool>{from_eigen,active_inequalities},
												   current_bijection_map,
												   n_c,
												   n_in,
												   dim,
												   n_eq,
												   ldl,
												   qp_scaled.as_const(),
												   mu_in,
												   mu_eq,
												   rho
												);

			LDLT_MULTI_WORKSPACE_MEMORY(
				//(_C_p,Uninit, Mat(n_in, dim),LDLT_CACHELINE_BYTES, T),
				(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
				(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
				//(_dw_p,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
				(err_it_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T)
				//(prim_p_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
				//(z_p_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T)
			);
			//auto C_p = _C_p.to_eigen().eval(); 
			auto rhs = _rhs.to_eigen().eval();
			auto dw = _dw.to_eigen().eval();
			auto err_it = err_it_.to_eigen().eval();
			//auto dz = _dw_p.to_eigen().eval();
			//auto prim_p = prim_p_.to_eigen().eval();
			//auto z_p = z_p_.to_eigen().eval();
			
			/*
			for (isize i=0;i<n_in;i++){
				
				if (l_active_set_n_u(i)){
					prim_p(current_bijection_map(i)) = prim_in_u(i);
				}else if (l_active_set_n_l(i)){
					prim_p(current_bijection_map(i)) = prim_in_l(i);
				}
				C_p.row(current_bijection_map(i)) = C_.row(i);
				
				z_p(current_bijection_map(i)) = z_(i);
			}
			*/

			// TO TEST
			
			for (isize i=0;i<n_in;i++){
				isize j = current_bijection_map(i);
				if (j <n_c){
					if (l_active_set_n_u(i)){
						rhs(j+dim+n_eq) = -prim_in_u(i);
					}else if (l_active_set_n_l(i)){
						rhs(j+dim+n_eq) = -prim_in_l(i);
					}
				}else{
					rhs.topRows(dim) += z_(i) * C_.row(i) ; 
				}
			}
			
			// END TO TEST
			
			rhs.topRows(dim) = -dual_for_eq ;
			rhs.middleRows(dim,n_eq) = -primal_residual_eq ;
			
			/*
			for (isize i = 0; i < n_c; ++i){
				rhs(i+dim+n_eq) = -prim_p(i);
			}
			for (isize i = n_c ; i < n_in ; ++i){ 
				rhs.topRows(dim) += z_p(i) * C_p.row(i) ; //unactive unrelevant columns
			}
			*/
			iterative_solve_with_permut_fact_new( //
				rhs,
				dw,
				err_it,
				ldl,
				eps_int,
				isize(5),
				qp_scaled.as_const(),
				current_bijection_map,
				dim,
				n_eq,
				n_c,
				n_in,
				mu_eq,
				mu_in,
				rho
				);

			dw_aug.setZero();
			dw_aug.topRows(dim+n_eq) = dw.topRows(dim+n_eq) ;

			/* OLD
			for (isize i = 0; i < n_c; ++i){
				dz(i) = dw(dim+n_eq+i);
			}
			for (isize i = n_c ; i < n_in ; ++i){
				dz(i) = -z_p(i)  ;
			}
			*/

			//PERMUT BACK
			/* OLD
			for (isize j = 0; j < n_in; ++j){
				isize i = current_bijection_map(j);
				dw_aug(j+dim+n_eq) = dz(i);
			}	
			*/
			for (isize j = 0; j < n_in; ++j){
				isize i = current_bijection_map(j);
				if (i < n_c){
					dw_aug(j+dim+n_eq) = dw(dim+n_eq+i);
				}else{
					dw_aug(j+dim+n_eq) = -z_(j) ; 
				}
				
			}	 

			prim_in_u.array() += (z_e/mu_in).array() ; 
			prim_in_l.array() += (z_e/mu_in).array() ; 

			d_primal_residual_eq = (A_*dw_aug.topRows(dim)- dw_aug.middleRows(dim,n_eq) / mu_eq).eval() ;
			d_dual_for_eq = (H_*dw_aug.topRows(dim)+A_.transpose()*dw_aug.middleRows(dim,n_eq)+rho*dw_aug.topRows(dim)).eval() ;
			Cdx_ = C_*dw_aug.topRows(dim) ;
			dual_for_eq -= C_.transpose()*z_e ; 
			T alpha_step = qp::line_search::initial_guess_LS(
									  ze,
						VectorView<T>{from_eigen,dw_aug.tail(n_in)},
						VectorView<T>{from_eigen,prim_in_l},
						VectorView<T>{from_eigen,prim_in_u},
						VectorView<T>{from_eigen,Cdx_},
						VectorView<T>{from_eigen,d_dual_for_eq},
						VectorView<T>{from_eigen,dual_for_eq},
						VectorView<T>{from_eigen,d_primal_residual_eq},
						VectorView<T>{from_eigen,primal_residual_eq},
						qp_scaled.C.as_const(),
						mu_eq,
						mu_in,
						rho,
						dim,
						n_eq,
						n_in,
						R
			);	
			
			std::cout << "alpha from initial guess " << alpha_step << std::endl;

			prim_in_u.array() += (alpha_step*Cdx_).array();
			prim_in_l.array() += (alpha_step*Cdx_).array();
			l_active_set_n_u = (prim_in_u.array() >= T(0)).matrix();
			l_active_set_n_l = (prim_in_l.array() <= T(0)).matrix();
			active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

			x.to_eigen().array() += (alpha_step * dw_aug.topRows(dim).array()) ; 
			y.to_eigen().array() += (alpha_step * dw_aug.middleRows(dim,n_eq).array()) ; 

			for (isize i = 0; i< n_in ; ++i){
				if (l_active_set_n_u(i)){
					z(i) = max2(z(i)+alpha_step*dw_aug(dim+n_eq+i),T(0)) ; 
				}else if (l_active_set_n_l(i)){
					z(i) = std::min(z(i)+alpha_step*dw_aug(dim+n_eq+i),T(0)) ; 
				} else{
					z(i) += alpha_step*dw_aug(dim+n_eq+i) ; 
				}
			}
			primal_residual_eq.array() += (alpha_step*d_primal_residual_eq).array();
			dual_for_eq.array() += alpha_step* (d_dual_for_eq.array()) ;

			T err = SaddlePoint(
				qp_scaled.as_const(),
				x,
				y,
				z,
				xe,
				ye,
				ze,
				mu_eq, 
				mu_in,
				rho,
				n_in,
				VectorViewMut<T>{from_eigen,prim_in_u},
				VectorViewMut<T>{from_eigen,prim_in_l},
				VectorViewMut<T>{from_eigen,primal_residual_eq},
				VectorViewMut<T>{from_eigen,dual_for_eq}
			);
			return err;
}

template<typename T,typename Preconditioner>
T initial_guess(
		VectorView<T> xe,
		VectorView<T> ye,
        VectorView<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBoxMut<T> qp_scaled,
		T mu_in,
		T mu_eq,
		T rho,
		T eps_int,
		Preconditioner precond,
		isize dim,
		isize n_eq,
		isize n_in,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& prim_in_u,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& prim_in_l,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dual_for_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& d_dual_for_eq,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& Cdx_,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& d_primal_residual_eq,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_u,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& l_active_set_n_l,
		Eigen::Matrix<bool, Eigen::Dynamic, 1>& active_inequalities,
		Eigen::Matrix<T, Eigen::Dynamic, 1>& dw_aug
		){

			auto H_ = qp_scaled.H.as_const().to_eigen();
			auto g_ = qp_scaled.g.as_const().to_eigen();
			auto A_ = qp_scaled.A.as_const().to_eigen();
			auto C_ = qp_scaled.C.as_const().to_eigen();
			auto x_ = x.to_eigen();
			auto z_ = z.to_eigen();
			auto z_e = ze.to_eigen().eval();
			auto l_ = qp_scaled.l.as_const().to_eigen();
			auto u_ = qp_scaled.u.as_const().to_eigen();

			prim_in_u =  (C_*x_-u_) ; 
			prim_in_l = (C_*x_-l_) ; 

			precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_u});
			precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_l});
			precond.unscale_dual_in_place_in(
							VectorViewMut<T>{from_eigen, z_e}); 

			prim_in_u.array() += (z_e/mu_in).array() ; 

			prim_in_l.array() += (z_e/mu_in).array() ; 

			l_active_set_n_u = (prim_in_u.array() >= T(0)).matrix();
			l_active_set_n_l = (prim_in_l.array() <= T(0)).matrix();

			active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

			prim_in_u.array() -= (z_e/mu_in).array() ; 
			prim_in_l.array() -= (z_e/mu_in).array() ; 

			precond.scale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_u});
			precond.scale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_l});
			precond.scale_dual_in_place_in(
							VectorViewMut<T>{from_eigen, z_e});
			// rescale value
			isize num_active_inequalities = active_inequalities.count();
			isize inner_pb_dim = dim + n_eq + num_active_inequalities;

			LDLT_MULTI_WORKSPACE_MEMORY(
			(_htot,Uninit, Mat(inner_pb_dim, inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T)
			);
			auto Htot = _htot.to_eigen().eval();
			Htot.setZero();
			auto rhs = _rhs.to_eigen().eval();
			auto dw = _dw.to_eigen().eval();
			
			rhs.topRows(dim) = -dual_for_eq ;
			rhs.middleRows(dim,n_eq) = -primal_residual_eq ;
			
			LDLT_DECL_SCOPE_TIMER("in solver", "set H", T);
			Htot.topLeftCorner(dim, dim) = H_;
			for (isize i = 0; i < dim; ++i) {
					Htot(i, i) += rho;
			}
			
			Htot.block(0,dim,dim,n_eq) = A_.transpose();
			Htot.block(dim,0,n_eq,dim) = A_;
			//Htot.bottomRightCorner(n_eq+num_active_inequalities, n_eq+num_active_inequalities).setZero();
			{
					T tmp_eq = -T(1) / mu_eq;
					T tmp_in = -T(1) / mu_in;
					for (isize i = 0; i < n_eq; ++i) {
						Htot(dim + i, dim + i) = tmp_eq;
					}
					for (isize i = 0; i < num_active_inequalities; ++i) {
						Htot(dim + n_eq + i, dim + n_eq + i) = tmp_in;
					}
			}
			isize j = 0;
			for (isize i = 0; i< n_in ; ++i){
					if (l_active_set_n_u(i)){
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						rhs(j+dim+n_eq) = -prim_in_u(i);
						//rhs.topRows(dim) -= C_.transpose().col(i) * z_(i);
						j+=1 ;
					}else if (l_active_set_n_l(i)){
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						rhs(j+dim+n_eq) = -prim_in_l(i);
						//rhs.topRows(dim) -= C_.transpose().col(i) * z_(i);
						j+=1;
					}else{
						rhs.topRows(dim) += C_.transpose().col(i) * z_(i); //unactive unrelevant columns
					}
			}
			iterative_solve_with_permut_fact( //
				rhs,
				dw,
				Htot,
				eps_int,
				isize(2));

			dw_aug.setZero();
			dw_aug.topRows(dim+n_eq) = dw.topRows(dim+n_eq) ;
			isize j_aug = 0;
			for (isize i =0 ; i<n_in ; ++i){
				if (l_active_set_n_u(i)){
					dw_aug(dim+n_eq+i) = dw(dim+n_eq+j_aug) ; 
					j_aug +=1;
				}else if (l_active_set_n_l(i)){
					dw_aug(dim+n_eq+i) = dw(dim+n_eq+j_aug) ; 
					j_aug +=1;
				} else{
					dw_aug(dim+n_eq+i) -= z(i) ; 
				}
			}

			prim_in_u.array() += (z_e/mu_in).array() ; 
			prim_in_l.array() += (z_e/mu_in).array() ; 

			d_primal_residual_eq = (A_*dw_aug.topRows(dim)- dw_aug.middleRows(dim,n_eq) / mu_eq).eval() ;
			d_dual_for_eq = (H_*dw_aug.topRows(dim)+A_.transpose()*dw_aug.middleRows(dim,n_eq)+rho*dw_aug.topRows(dim)).eval() ;
			Cdx_ = C_*dw_aug.topRows(dim) ;
			dual_for_eq -= C_.transpose()*z_e ; 

			T alpha_step = qp::line_search::initial_guess_LS(
									  ze,
						VectorView<T>{from_eigen,dw_aug.tail(n_in)},
						VectorView<T>{from_eigen,prim_in_l},
						VectorView<T>{from_eigen,prim_in_u},
						VectorView<T>{from_eigen,Cdx_},
						VectorView<T>{from_eigen,d_dual_for_eq},
						VectorView<T>{from_eigen,dual_for_eq},
						VectorView<T>{from_eigen,d_primal_residual_eq},
						VectorView<T>{from_eigen,primal_residual_eq},
						qp_scaled.C.as_const(),
						mu_eq,
						mu_in,
						rho,
						dim,
						n_eq,
						n_in
			);
			
			std::cout << "alpha from initial guess " << alpha_step << std::endl;

			prim_in_u.array() += (alpha_step*Cdx_).array();
			prim_in_l.array() += (alpha_step*Cdx_).array();
			l_active_set_n_u = (prim_in_u.array() >= T(0)).matrix();
			l_active_set_n_l = (prim_in_l.array() <= T(0)).matrix();
			active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

			x.to_eigen().array() += (alpha_step * dw_aug.topRows(dim).array()) ; 
			y.to_eigen().array() += (alpha_step * dw_aug.middleRows(dim,n_eq).array()) ; 

			for (isize i = 0; i< n_in ; ++i){
				if (l_active_set_n_u(i)){
					z(i) = max2(z(i)+alpha_step*dw_aug(dim+n_eq+i),T(0)) ; 
				}else if (l_active_set_n_l(i)){
					z(i) = std::min(z(i)+alpha_step*dw_aug(dim+n_eq+i),T(0)) ; 
				} else{
					z(i) += alpha_step*dw_aug(dim+n_eq+i) ; 
				}
			}
			primal_residual_eq.array() += (alpha_step*d_primal_residual_eq).array();
			dual_for_eq.array() += alpha_step* (d_dual_for_eq.array()) ;

			T err = SaddlePoint(
				qp_scaled.as_const(),
				x,
				y,
				z,
				xe,
				ye,
				ze,
				mu_eq, 
				mu_in,
				rho,
				n_in,
				VectorViewMut<T>{from_eigen,prim_in_u},
				VectorViewMut<T>{from_eigen,prim_in_l},
				VectorViewMut<T>{from_eigen,primal_residual_eq},
				VectorViewMut<T>{from_eigen,dual_for_eq}
			);
			return err;
}


template<typename T,typename Preconditioner>
T initial_guess_loop(
		VectorView<T> xe,
		VectorView<T> ye,
        VectorView<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBoxMut<T> qp_scaled,
		T mu_in,
		T mu_eq,
		T rho,
		T eps_int,
		Preconditioner precond,
		isize dim,
		isize n_eq,
		isize n_in
		){

			auto H_ = qp_scaled.H.as_const().to_eigen();
			auto g_ = qp_scaled.g.as_const().to_eigen();
			auto A_ = qp_scaled.A.as_const().to_eigen();
			auto C_ = qp_scaled.C.as_const().to_eigen();
			auto x_ = x.to_eigen();
			auto x_e = xe.to_eigen();
			auto y_ = y.to_eigen();
			auto y_e = ye.to_eigen();
			auto z_ = z.to_eigen();
			auto z_e = ze.to_eigen().eval();
			auto b_ = qp_scaled.b.as_const().to_eigen();
			auto l_ = qp_scaled.l.as_const().to_eigen();
			auto u_ = qp_scaled.u.as_const().to_eigen();

			/*
			std::cout << "x_e " << x_e << std::endl;
			std::cout << "y_e " << y_e << std::endl;
			std::cout << "z_e " << z_e << std::endl;
			*/
			auto prim_in_u =  (C_*x_-u_).eval() ; 
			auto prim_in_l = (C_*x_-l_).eval() ; 
			/*
			std::cout << "prim_in_u " << prim_in_u << std::endl;
			std::cout << "prim_in_l " << prim_in_l << std::endl;
			std::cout << "tmp_u unscaled " << prim_in_u +z_e/mu_in << std::endl;
			std::cout << "tmp_l unscaled " << prim_in_l +z_e/mu_in<< std::endl;		
			*/
			auto prim_eq = (A_*x_-b_).eval() ;

			
			precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_u});
			precond.unscale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_l});
			precond.unscale_dual_in_place_in(
							VectorViewMut<T>{from_eigen, z_e});

			Eigen::Matrix<T, Eigen::Dynamic, 1> tmp_u = prim_in_u + z_e/mu_in ; 
			Eigen::Matrix<T, Eigen::Dynamic, 1> tmp_l = prim_in_l + z_e/mu_in ; 
			Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_u = (tmp_u.array() >= T(0)).matrix();
			Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_l = (tmp_l.array() <= T(0)).matrix();

			//std::cout << "tmp_u before updating "<< tmp_u << std::endl;
			//std::cout << "tmp_l before updating "<< tmp_l << std::endl;

			Eigen::Matrix<bool, Eigen::Dynamic, 1> active_inequalities = l_active_set_n_u || l_active_set_n_l ; 

			precond.scale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_u});
			precond.scale_primal_residual_in_place_in(
							VectorViewMut<T>{from_eigen, prim_in_l});
			precond.scale_dual_in_place_in(
							VectorViewMut<T>{from_eigen, z_e});

			//std::cout << "active_inequalities " << active_inequalities << std::endl;
			isize num_active_inequalities = active_inequalities.count();
			//std::cout << "num_active_inequalities " << num_active_inequalities << std::endl;
			isize inner_pb_dim = dim + n_eq + num_active_inequalities;

			LDLT_MULTI_WORKSPACE_MEMORY(
			(_htot,Uninit, Mat(inner_pb_dim, inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_rhs,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(inner_pb_dim),LDLT_CACHELINE_BYTES, T),
			(_dw_aug,Init, Vec(dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T)
			);
			auto Htot = _htot.to_eigen().eval();
			Htot.setZero();
			auto rhs = _rhs.to_eigen().eval();
			auto dw = _dw.to_eigen().eval();
			auto dw_aug = _dw_aug.to_eigen().eval();
			
			//std::cout << "H_*x_ + g_ + A_.transpose() * y_   " << H_*x_ + g_ + A_.transpose() * y_  << std::endl;
			//std::cout << "C_.transpose() * z_ " << C_.transpose() * z_ << std::endl;
			rhs.topRows(dim) =  -(H_*x_ + g_ + A_.transpose() * y_);
			rhs.middleRows(dim,n_eq) = -prim_eq ;
			
			LDLT_DECL_SCOPE_TIMER("in solver", "set H", T);
			Htot.topLeftCorner(dim, dim) = H_;
			for (isize i = 0; i < dim; ++i) {
					Htot(i, i) += rho;
			}
			
			Htot.block(0,dim,dim,n_eq) = A_.transpose();
			Htot.block(dim,0,n_eq,dim) = A_;
			//Htot.bottomRightCorner(n_eq+num_active_inequalities, n_eq+num_active_inequalities).setZero(); # LDLT_MULTI_WORKSPACE_MEMORY set zero automatically
			{
					T tmp_eq = -T(1) / mu_eq;
					T tmp_in = -T(1) / mu_in;
					for (isize i = 0; i < n_eq; ++i) {
						Htot(dim + i, dim + i) = tmp_eq;
					}
					for (isize i = 0; i < num_active_inequalities; ++i) {
						Htot(dim + n_eq + i, dim + n_eq + i) = tmp_in;
					}
			}
			isize j = 0;
			for (isize i = 0; i< n_in ; ++i){
					if (l_active_set_n_u(i)){
						//std::cout << " i " << i <<  "l_active_set_n_u(i) " << l_active_set_n_u(i)<< " j " << j << " prim_in_u.row(i) " << prim_in_u(i) << std::endl;
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						rhs(j+dim+n_eq) = -prim_in_u(i);
						rhs.topRows(dim) -= C_.transpose().col(i) * z_(i);
						j+=1 ;
					}else if (l_active_set_n_l(i)){
						//std::cout << " i " << i << "l_active_set_n_l(i) " << l_active_set_n_l(i)<< " j " << j << " prim_in_l.row(i) "  << prim_in_l(i) << std::endl;
						Htot.block(j+dim+n_eq,0,1,dim) = C_.row(i) ; 
						Htot.block(0,j+dim+n_eq,dim,1) = C_.transpose().col(i) ; 
						rhs(j+dim+n_eq) = -prim_in_l(i);
						rhs.topRows(dim) -= C_.transpose().col(i) * z_(i);
						j+=1;
					}	
			}
			//std::cout << "Ok here too " << std::endl;
			
			//std::cout << "rhs before iteration solve topRows" << rhs.topRows(dim+n_eq) << std::endl;
			//std::cout << "rhs before iteration solve tail" << rhs.tail(num_active_inequalities) << std::endl;
			//std::cout << "rhs before iteration solve" << rhs << std::endl;
			//std::cout << "dw  before iteration solve" << dw << std::endl;
			
			iterative_solve_with_permut_fact( //
				rhs,
				dw,
				Htot,
				eps_int,
				isize(2));
			//std::cout << "rhs after iteration solve" << rhs << std::endl;
			//std::cout << "dw  after iteration solve" << dw << std::endl;

			dw_aug.topRows(dim+n_eq) = dw.topRows(dim+n_eq) ;
			isize j_aug = 0;
			for (isize i =0 ; i<n_in ; ++i){
				if (l_active_set_n_u(i)){
					dw_aug(dim+n_eq+i) = dw(dim+n_eq+j_aug) ; 
					j_aug +=1;
				}else if (l_active_set_n_l(i)){
					dw_aug(dim+n_eq+i) = dw(dim+n_eq+j_aug) ; 
					j_aug +=1;
				} else{
					dw_aug(dim+n_eq+i) -= z(i) ; 
				}
			}

			//std::cout << "dw_aug" << dw_aug << std::endl;

			//std::cout << "x LS" << x.to_eigen() << std::endl;
			//std::cout << "y LS" << y.to_eigen() << std::endl;
			//std::cout << "ze LS" << ze.to_eigen() << std::endl;


			T alpha_step = qp::line_search::initial_guess_line_search_box(
					x.as_const(),
					y.as_const(),
					ze,
					VectorView<T>{from_eigen,dw_aug},
					mu_eq,
					mu_in,
					rho,
					qp_scaled.as_const()) ; 
			
			std::cout << "alpha from initial guess " << alpha_step << std::endl;
			//std::cout << "Ok here as well too " << std::endl;

			//tmp_u += C_ *(alpha_step*dw_aug.topRows(dim)) ; gives an error
			tmp_u = C_*(x_+alpha_step*dw_aug.topRows(dim))-u_+z_e/mu_in;
			//tmp_l += C_ * (alpha_step*dw_aug.topRows(dim)) ; 
			tmp_l = C_*(x_+alpha_step*dw_aug.topRows(dim))-l_+z_e/mu_in;
			//std::cout << "tmp_u after updating " << tmp_u << std::endl;
			//std::cout << "tmp_l after updating " << tmp_l << std::endl;
			l_active_set_n_u = (tmp_u.array() >= T(0)).matrix();
			l_active_set_n_l = (tmp_l.array() <= T(0)).matrix();
			active_inequalities = l_active_set_n_u || l_active_set_n_l ; 
			//Eigen::Matrix<bool, Eigen::Dynamic, 1> inactive_set =  !(active_inequalities.array()) ;

			x.to_eigen().array() += (alpha_step * dw_aug.topRows(dim).array()) ; 
			y.to_eigen().array() += (alpha_step * dw_aug.middleRows(dim,n_eq).array()) ; 

			//isize j_ = 0;
			for (isize i = 0; i< n_in ; ++i){
				if (l_active_set_n_u(i)){
					//std::cout << "i " <<  i << " tmp_u(i) " << tmp_u(i) << std::endl; 
					//std::cout << "active u i :" << i << " j_ " << j_ << std::endl;
					//z(i) = max2(z(i)+alpha_step*dw(dim+n_eq+j_),T(0)) ; 
					z(i) = max2(z(i)+alpha_step*dw_aug(dim+n_eq+i),T(0)) ; 
					//j_ +=1;
				}else if (l_active_set_n_l(i)){
					//std::cout << "i " <<  i << " tmp_l(i) " << tmp_l(i) << std::endl; 
					//std::cout << "i " <<  i << std::endl; 
					//std::cout << "active l i :" << i << " j_ " << j_ << std::endl;
					//z(i) = std::min(z(i)+alpha_step*dw(dim+n_eq+j_),T(0)) ; 
					z(i) = std::min(z(i)+alpha_step*dw_aug(dim+n_eq+i),T(0)) ; 
					//j_ +=1;
				} else{
					//std::cout << "i " <<  i << " tmp_u(i) " << tmp_u(i) << " tmp_l(i) " << tmp_l(i) << std::endl; 
					//std::cout << "i " <<  i << std::endl; 
					//std::cout << "inactive i :" << i << " j_ " << j_ << std::endl;
					//z(i) -= alpha_step*z(i) ; 
					z(i) += alpha_step*dw_aug(dim+n_eq+i) ; 
				}
			}
			
			//std::cout << "new x " << x.to_eigen() <<  std::endl;
			//std::cout << "new y " << y.to_eigen() <<  std::endl;
			//std::cout << "new z " << z.to_eigen() <<  std::endl;
			
			T err = SaddlePointError(
				qp_scaled.as_const(),
				x,
				y,
				z,
				xe,
				ye,
				ze,
				mu_eq,
				mu_in,
				rho,
				n_in
			);

			//std::cout << "Ok Ok here as well too too " << std::endl;

			return err;
}


template<typename T>
T correction_guess_loop(
		VectorView<T> xe,
		VectorView<T> ye,
        VectorView<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBoxMut<T> qp_scaled,
		T mu_in,
		T mu_eq,
		T rho,
		T eps_int,
		isize dim,
		isize n_eq,
		isize n_in,
		isize max_iter_in,
		isize& n_tot
		){
		
		T err_in = T(0);
		for (i64 iter = 0; iter <= max_iter_in; ++iter) {

			if (iter == max_iter_in) {
				n_tot +=max_iter_in;
				break;
			}
			
			/*
			std::cout << "xe " << xe.to_eigen() << std::endl;
			std::cout << "ye " << ye.to_eigen() << std::endl;
			std::cout << "ze " << ze.to_eigen() << std::endl;
			std::cout << "x " << x.to_eigen() << std::endl;
			std::cout << "y " << y.to_eigen() << std::endl;
			std::cout << "z " << z.to_eigen() << std::endl;
			*/
			Eigen::Matrix<T, Eigen::Dynamic, 1> dx(dim);
			dx.setZero();
			qp::detail::newton_step<T>(
											qp_scaled.as_const(),
											x.as_const(),
											xe,
											ye,
											ze,
											VectorViewMut<T>{from_eigen,dx},
											mu_eq,
											mu_in,
											rho,
											eps_int,
											dim,
											n_eq,
											n_in
			);
			//std::cout <<  "dx " << dx << std::endl;
			T alpha_step = T(1);
			/*
			std::cout << "x correc " << x.to_eigen() << std::endl;
			std::cout << "xe " << xe.to_eigen() << std::endl;
			std::cout << "ye " << ye.to_eigen() << std::endl;
			std::cout << "ze "  << ze.to_eigen() << std::endl;
			std::cout << "dx " << dx << std::endl; 
			*/
			if (n_in > isize(0)){
				alpha_step = qp::line_search::correction_guess_line_search_box(
						x.as_const(),
						xe,
						ye,
						ze,
						VectorView<T>{from_eigen, dx},
						mu_eq,
						mu_in,
						rho,
						qp_scaled.as_const()) ;
			}
			//std::cout << "alpha passed " << alpha_step << std::endl;
			if (infty_norm(alpha_step * dx)< T(1.E-11)){
				n_tot += iter+1;
				break;
			}
			x.to_eigen().array() += (alpha_step *dx.array()) ; 

			//std::cout << "x " << x.to_eigen() << std::endl;
			Eigen::Matrix<T,Eigen::Dynamic,1> z_pos = ( ((qp_scaled.C).to_eigen()*(x.to_eigen())-(qp_scaled.u).to_eigen())*mu_in + ze.to_eigen()).eval() ;
			Eigen::Matrix<T,Eigen::Dynamic,1> z_neg = (((qp_scaled.C).to_eigen()*x.to_eigen()-(qp_scaled.l).to_eigen())*mu_in + ze.to_eigen()).eval() ; 

			z_pos =  (z_pos.array() >= T(0)).select(z_pos, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in));

			z_neg =  (z_neg.array() <= T(0)).select(z_neg, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in));


			auto tmp1 = (qp_scaled.H).to_eigen() *x.to_eigen() ;
			y.to_eigen() = ((qp_scaled.A).to_eigen()*x.to_eigen()-(qp_scaled.b).to_eigen())*mu_eq +ye.to_eigen();
			z.to_eigen() = z_pos + z_neg ; 
			auto tmp2 = (qp_scaled.A.to_eigen().transpose()) * ( y.to_eigen() );
			auto tmp3 = (qp_scaled.C.to_eigen().transpose()) * ( z.to_eigen() )   ; 
			auto tmp4 = (qp_scaled.g).to_eigen() ;
			auto grad_n = tmp1 +  tmp2 + tmp3  + tmp4;

			err_in = infty_norm(  grad_n + (x.to_eigen()-xe.to_eigen()) * rho );
			std::cout << "---it in " << iter << " projection norm " << err_in << " alpha " << alpha_step << std::endl;
			
			/*
			std::cout << "z " << z.to_eigen() << std::endl;
			std::cout << "norm Hx " << infty_norm(tmp1)  << std::endl;
			std::cout << "norm A.T(mu_eq*(Ax-b)+ye) " << infty_norm(tmp2)  << std::endl;
			std::cout << " norm C.T z " <<   infty_norm(tmp3)  << std::endl;
			std::cout << "norm g " <<  infty_norm(tmp4) << std::endl;
			
			std::cout << "rhs " << (1 + max2(max2(max2(infty_norm(tmp1), infty_norm(tmp2)), infty_norm(tmp3)), infty_norm(tmp4)) ) << std::endl;
			*/
			if (err_in<= eps_int * (1 + max2(max2(max2(infty_norm(tmp1), infty_norm(tmp2)), infty_norm(tmp3)), infty_norm(tmp4)) )  ){
				n_tot +=iter+1;
				break;
			}
			
		}
	
		return err_in;

}


template<typename T>
T correction_guess(
		VectorView<T> xe,
		VectorView<T> ye,
        VectorView<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBoxMut<T> qp_scaled,
		T mu_in,
		T mu_eq,
		T rho,
		T eps_int,
		isize dim,
		isize n_eq,
		isize n_in,
		isize max_iter_in,
		isize& n_tot,
		Eigen::Matrix<T,Eigen::Dynamic,1>& residual_in_y,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_pos,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_neg,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dual_for_eq,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Hdx,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Adx,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Cdx,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_u,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_l,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& active_inequalities,
		ldlt::Ldlt<T>& ldl,
		VectorViewMut<isize> current_bijection_map,
		isize& n_c,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dw_aug,
		T& correction_guess_rhs_g
		){

		T err_in = T(0);

		for (i64 iter = 0; iter <= max_iter_in; ++iter) {

			if (iter == max_iter_in) {
				n_tot +=max_iter_in;
				break;
			}

			//Eigen::Matrix<T, Eigen::Dynamic, 1> dx(dim);
			//dx.setZero();
			dw_aug.topRows(dim).setZero();
			/*
			qp::detail::newton_step_new<T>(
											qp_scaled.as_const(),
											x.as_const(),
											xe,
											ye,
											ze,
											VectorViewMut<T>{from_eigen,dx},
											mu_eq,
											mu_in,
											rho,
											eps_int,
											dim,
											n_eq,
											n_in,
											z_pos,
											z_neg,
											residual_in_y,
											dual_for_eq,
											l_active_set_n_u,
											l_active_set_n_l,
											active_inequalities
			);
			*/
			qp::detail::newton_step_fact<T>(
						qp_scaled.as_const(),
						x.as_const(),
						xe,
						ye,
						ze,
						//VectorViewMut<T>{from_eigen,dx},
						VectorViewMut<T>{from_eigen,dw_aug.topRows(dim)},
						mu_eq,
						mu_in,
						rho,
						eps_int,
						dim,
						n_eq,
						n_in,
						z_pos,
						z_neg,
						residual_in_y,
						dual_for_eq,
						l_active_set_n_u,
						l_active_set_n_l,
						active_inequalities,

						ldl,
						current_bijection_map,
						n_c
			);
			T alpha_step = T(1);
			
			Hdx = (qp_scaled.H).to_eigen() * dw_aug.topRows(dim) ; 
			Adx = (qp_scaled.A).to_eigen() * dw_aug.topRows(dim) ; 
			Cdx = (qp_scaled.C).to_eigen() * dw_aug.topRows(dim) ; 
			/*
			Hdx = (qp_scaled.H).to_eigen() * dx ; 
			Adx = (qp_scaled.A).to_eigen() * dx; 
			Cdx = (qp_scaled.C).to_eigen() * dx;
			*/
			if (n_in > isize(0)){
				alpha_step = qp::line_search::correction_guess_LS(
										Hdx,
									 	VectorView<T>{from_eigen,dw_aug.topRows(dim)},
										//VectorView<T>{from_eigen,dx},
										(qp_scaled.g).as_const(),
										Adx,  
										Cdx,
										residual_in_y,
										z_pos,
										z_neg,
										x.as_const(),
										xe,
										ye,
										ze,
										mu_eq,
										mu_in,
										rho,
										n_in
						
				) ;
			}
			
			if (infty_norm(alpha_step * dw_aug.topRows(dim))< T(1.E-11)){
				n_tot += iter+1;
				break;
			}
			/*
			if (infty_norm(alpha_step * dx)< T(1.E-11)){
				n_tot += iter+1;
				break;
			}*/
			x.to_eigen().array() += (alpha_step *dw_aug.topRows(dim).array()) ; 
			//x.to_eigen().array() += (alpha_step *dx.array()) ; 
			z_pos.array() += (alpha_step *Cdx).array() ;
			z_neg.array() += (alpha_step *Cdx).array() ; 
			residual_in_y.array() += (alpha_step * Adx).array();
 			y.to_eigen() = mu_eq *  residual_in_y  ;
			dual_for_eq.array() += (alpha_step * ( mu_eq * (qp_scaled.A).to_eigen().transpose() * Adx + rho * dw_aug.topRows(dim) + Hdx  )).array() ;
			for (isize j = 0 ; j < n_in; ++j){
				z(j) = mu_in*(max2(z_pos(j),T(0)) + std::min(z_neg(j),T(0))) ; 
			}

			Hdx = (qp_scaled.H).to_eigen() *x.to_eigen() ;
			T rhs_c = max2(correction_guess_rhs_g,infty_norm(Hdx)) ; 
			/*
			auto tmp1 = (qp_scaled.H).to_eigen() *x.to_eigen() ;
			auto tmp2 =  (qp_scaled.A.to_eigen().transpose()) * ( y.to_eigen() );
			auto tmp3 = (qp_scaled.C.to_eigen().transpose()) * ( z.to_eigen() )   ; 
			*/
			dw_aug.topRows(dim) = (qp_scaled.A.to_eigen().transpose()) * ( y.to_eigen() );
			rhs_c = max2(rhs_c,infty_norm(dw_aug.topRows(dim))) ; 
			Hdx.array()+=(dw_aug.topRows(dim)).array();

			dw_aug.topRows(dim) = (qp_scaled.C.to_eigen().transpose()) * ( z.to_eigen() )   ; 
			rhs_c = max2(rhs_c,infty_norm(dw_aug.topRows(dim))) ; 
			Hdx.array()+=(dw_aug.topRows(dim)).array();

			Hdx.array()+= ((qp_scaled.g).to_eigen()).array(); 
			
			
			//auto grad_n = tmp1 + tmp2 + tmp3  + (qp_scaled.g).to_eigen() ;

			err_in = infty_norm(  Hdx + rho* (x.to_eigen()-xe.to_eigen()) );
			std::cout << "---it in " << iter << " projection norm " << err_in << " alpha " << alpha_step << std::endl;

			//if (err_in<= eps_int * (1 + max2(max2(max2(infty_norm(tmp1), infty_norm(tmp2)), infty_norm(tmp3)), correction_guess_rhs_g ) )  ){
			if (err_in<= eps_int * (1 + rhs_c  )  ){
				n_tot +=iter+1;
				break;
			}
		}
	
		return err_in;

}


template<typename T>
T correction_guess_QPALM(
		VectorView<T> xe,
		VectorView<T> ye,
        VectorView<T> ze,
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBoxMut<T> qp_scaled,
		Eigen::Matrix<T,Eigen::Dynamic,1>& mu,
		T rho,
		T eps_int,
		isize dim,
		isize n_eq,
		isize n_in,
		isize max_iter_in,
		isize& n_tot,
		Eigen::Matrix<T,Eigen::Dynamic,1>& residual_in_y,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_pos,
		Eigen::Matrix<T,Eigen::Dynamic,1>& z_neg,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dual_for_eq,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Hdx,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Adx,
		Eigen::Matrix<T,Eigen::Dynamic,1>& Cdx,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_u,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& l_active_set_n_l,
		Eigen::Matrix<bool,Eigen::Dynamic,1>& active_inequalities,
		ldlt::Ldlt<T>& ldl,
		VectorViewMut<isize> current_bijection_map,
		isize& n_c,
		Eigen::Matrix<T,Eigen::Dynamic,1>& dw_aug,
		T& correction_guess_rhs_g
		){

		T err_in = T(0);

		for (i64 iter = 0; iter <= max_iter_in; ++iter) {

			if (iter == max_iter_in) {
				n_tot +=max_iter_in;
				break;
			}

			dw_aug.topRows(dim).setZero();

			qp::detail::newton_step_QPALM<T>(
						qp_scaled.as_const(),
						x.as_const(),
						xe,
						ye,
						ze,
						VectorViewMut<T>{from_eigen,dw_aug.topRows(dim)},
						mu,
						rho,
						eps_int,
						dim,
						n_eq,
						n_in,
						z_pos,
						z_neg,
						residual_in_y,
						dual_for_eq,
						l_active_set_n_u,
						l_active_set_n_l,
						active_inequalities,

						ldl,
						current_bijection_map,
						n_c
			);
			T alpha_step = T(1);
			
			Hdx = (qp_scaled.H).to_eigen() * dw_aug.topRows(dim) ; 
			Adx = (qp_scaled.A).to_eigen() * dw_aug.topRows(dim) ; 
			Cdx = (qp_scaled.C).to_eigen() * dw_aug.topRows(dim) ; 

			if (n_in > isize(0)){
				alpha_step = qp::line_search::correction_guess_LS_QPALM(
										Hdx,
									 	VectorView<T>{from_eigen,dw_aug.topRows(dim)},
										(qp_scaled.g).as_const(),
										Adx,  
										Cdx,
										residual_in_y,
										z_pos,
										z_neg,
										x.as_const(),
										xe,
										ye, 
										ze,
										mu,
										rho,
										n_in,
										n_eq
						
				) ;
			}
			
			if (infty_norm(alpha_step * dw_aug.topRows(dim))< T(1.E-11)){
				n_tot += iter+1;
				break;
			}

			x.to_eigen().array() += (alpha_step *dw_aug.topRows(dim).array()) ; 
			z_pos.array() += (alpha_step *Cdx).array() ;
			z_neg.array() += (alpha_step *Cdx).array() ; 
			residual_in_y.array() += (alpha_step * Adx).array();
 			y.to_eigen().array() = mu.topRows(n_eq).array() *  residual_in_y.array()  ;
			dual_for_eq.array() += (alpha_step * mu.topRows(n_eq).array() * (qp_scaled.A.to_eigen().transpose() * Adx).array() ) + ( rho * dw_aug.topRows(dim) + Hdx  ).array() ;
			for (isize j = 0 ; j < n_in; ++j){
				z(j) = mu(n_eq+j)*(max2(z_pos(j),T(0)) + std::min(z_neg(j),T(0))) ; 
			}

			Hdx = (qp_scaled.H).to_eigen() *x.to_eigen() ;
			T rhs_c = max2(correction_guess_rhs_g,infty_norm(Hdx)) ; 

			dw_aug.topRows(dim) = (qp_scaled.A.to_eigen().transpose()) * ( y.to_eigen() );
			rhs_c = max2(rhs_c,infty_norm(dw_aug.topRows(dim))) ; 
			Hdx.array()+=(dw_aug.topRows(dim)).array();

			dw_aug.topRows(dim) = (qp_scaled.C.to_eigen().transpose()) * ( z.to_eigen() )   ; 
			rhs_c = max2(rhs_c,infty_norm(dw_aug.topRows(dim))) ; 
			Hdx.array()+=(dw_aug.topRows(dim)).array();

			Hdx.array()+= ((qp_scaled.g).to_eigen()).array(); 

			err_in = infty_norm(  Hdx + rho* (x.to_eigen()-xe.to_eigen()) );
			std::cout << "---it in " << iter << " projection norm " << err_in << " alpha " << alpha_step << std::endl;

			if (err_in<= eps_int * (1 + rhs_c  )  ){
				n_tot +=iter+1;
				break;
			}
		}
	
		return err_in;

}

template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
QpSolveStats solve_qp_in( //
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBox<T> qp,
		isize max_iter,
		isize max_iter_in,
		T eps_abs,
		T eps_rel,
		Preconditioner precond = Preconditioner{}) {

	using namespace ldlt::tags;

	isize dim = qp.H.rows;
	isize n_eq = qp.A.rows;
    isize n_in = qp.C.rows;
	isize n_mu_updates = 0;
	isize n_tot = 0;
	isize n_ext = 0;

	T machine_eps = std::numeric_limits<T>::epsilon();
	auto rho = T(1e-6);
	auto bcl_mu_eq = T(1e3);
    auto bcl_mu_in = T(1e1);
	T bcl_eta_ext = 1 / pow(bcl_mu_in, T(0.1));
    T bcl_eta_in = T(1);
	
	LDLT_MULTI_WORKSPACE_MEMORY(
			(_h_scaled,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
			(_h_ws,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
	     	(_g_scaled,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
	     	(_a_scaled,Uninit, Mat(n_eq, dim),LDLT_CACHELINE_BYTES, T),
		 	(_c_scaled,Uninit, Mat(n_in, dim),LDLT_CACHELINE_BYTES, T),
	     	(_b_scaled,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_u_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
         	(_l_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
	     	(_residual_scaled,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	(_residual_scaled_tmp,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	//(_residual_unscaled,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	(_y,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_z,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
	     	(_diag_diff_eq,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_diag_diff_in,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T)
		);

	auto H_copy = _h_scaled.to_eigen();
	auto H_ws = _h_ws.to_eigen();
	auto q_copy = _g_scaled.to_eigen();
	auto A_copy = _a_scaled.to_eigen();
	auto C_copy = _c_scaled.to_eigen();
	auto b_copy = _b_scaled.to_eigen();
    auto u_copy = _u_scaled.to_eigen();
    auto l_copy = _l_scaled.to_eigen();

	H_copy = qp.H.to_eigen();
	q_copy = qp.g.to_eigen();
	A_copy = qp.A.to_eigen();
	b_copy = qp.b.to_eigen();
    C_copy = qp.C.to_eigen();
    u_copy = qp.u.to_eigen();
    l_copy = qp.l.to_eigen();

	auto qp_scaled = qp::QpViewBoxMut<T>{
			{from_eigen, H_copy},
			{from_eigen, q_copy},
			{from_eigen, A_copy},
			{from_eigen, b_copy},
			{from_eigen, C_copy},
			{from_eigen, u_copy},
            {from_eigen, l_copy}
	};
	//std::cout << "g before scaling " <<  (qp_scaled.g).to_eigen() << std::endl;
	
	//LDLT_DECL_SCOPE_TIMER("in solver", "scale qp", T);
	precond.scale_qp_in_place(qp_scaled);
	
	/*
	std::cout << "g after scaling " <<  (qp_scaled.g).to_eigen() << std::endl;
	std::cout << "c " <<  precond.c << std::endl;
	std::cout << "delta " << precond.delta << std::endl;
	
	std::cout << "H after scaling before warm start " << (qp_scaled.H).to_eigen() << std::endl;
	std::cout << "H_copy after scaling before warm start " << H_copy << std::endl;
	*/
	H_ws = H_copy;
	for (isize i = 0;i< dim ; ++i){
		H_ws(i,i) += rho ;
	}
	/*
	std::cout << "H after scaling after warm start " << (qp_scaled.H).to_eigen() << std::endl;
	std::cout << "H_copy after scaling after warm start " << H_copy << std::endl;
	*/
	ldlt::Ldlt<T> ldl{decompose, H_ws};
	//std::cout << "x before warm start " <<  (x.to_eigen()) << std::endl;
	x.to_eigen().array() = -(qp_scaled.g).to_eigen().array();
	ldl.solve_in_place(x.to_eigen());
	
	//std::cout << "g after warm start " <<  (qp_scaled.g).to_eigen() << std::endl;
	//std::cout << "x after warm start " <<  (x.to_eigen()) << std::endl;
	/*
	std::cout << "x before scaling " <<  (x.to_eigen()) << std::endl;
	std::cout << "z before scaling " <<  (z.to_eigen()) << std::endl;
	// USELESS AS y and z zero and x is defined in scaled format above
	{
		LDLT_DECL_SCOPE_TIMER("in solver", "scale solution", T);
		precond.scale_primal_in_place(x);
		precond.scale_dual_in_place_eq(y);
        precond.scale_dual_in_place_in(z);
	}
	*/
	/*
	std::cout << "x after scaling " <<  (x.to_eigen()) << std::endl;
	std::cout << "z after scaling " <<  (z.to_eigen()) << std::endl;
	*/
	auto residual_scaled = _residual_scaled.to_eigen();
	Eigen::Matrix<T, Eigen::Dynamic, 1> residual_scaled_tmp = _residual_scaled_tmp.to_eigen();
	//auto residual_unscaled = _residual_unscaled.to_eigen();

	auto y_new = _y.to_eigen();
    auto z_new = _z.to_eigen();
	auto diag_diff_in = _diag_diff_in.to_eigen();
	auto diag_diff_eq = _diag_diff_eq.to_eigen();

	T primal_feasibility_rhs_1_eq = infty_norm(qp.b.to_eigen());
    T primal_feasibility_rhs_1_in_u = infty_norm(qp.u.to_eigen());
    T primal_feasibility_rhs_1_in_l = infty_norm(qp.l.to_eigen());
	T dual_feasibility_rhs_2 = infty_norm(qp.g.to_eigen());
	
	for (i64 iter = 0; iter <= max_iter; ++iter) {
		n_ext +=1;
		if (iter == max_iter) {
			break;
		}

		Eigen::Matrix<T, Eigen::Dynamic, 1> dual_residual_scaled = residual_scaled.topRows(dim);
		Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_eq_scaled = residual_scaled.middleRows(dim,n_eq);
        Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_u = residual_scaled.bottomRows(n_in);
        Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_l = residual_scaled.bottomRows(n_in);

		// compute primal residual
		T primal_feasibility_eq_rhs_0(0);
        T primal_feasibility_in_rhs_0(0);
		T dual_feasibility_rhs_0(0);
		T dual_feasibility_rhs_1(0);
        T dual_feasibility_rhs_3(0);

		T primal_feasibility_lhs(0);
		//T primal_feasibility_eq_lhs(0);
		//T primal_feasibility_in_lhs(0);
		T dual_feasibility_lhs(0);
		
		qp::detail::global_primal_residual(
				primal_feasibility_lhs,
				primal_feasibility_eq_rhs_0,
				primal_feasibility_in_rhs_0,
				//primal_feasibility_eq_lhs,
				//primal_feasibility_in_lhs,
				primal_residual_eq_scaled,
				primal_residual_in_scaled_u,
				primal_residual_in_scaled_l,
				qp,
				//residual_scaled_tmp,
				qp_scaled,
				precond,
				x,
				dim,
				n_eq,
				n_in
		);
		qp::detail::global_dual_residual(
			dual_feasibility_lhs,
			dual_feasibility_rhs_0,
			dual_feasibility_rhs_1,
        	dual_feasibility_rhs_3,
			dual_residual_scaled,
			residual_scaled_tmp,
			qp_scaled,
			precond,
			x,
			y,
			z,
			dim,
			n_eq,
			n_in
		);
		std::cout << "---------------it : " << iter << " primal residual : " << primal_feasibility_lhs << " dual residual : " << dual_feasibility_lhs << std::endl;
		std::cout << "bcl_eta_ext : " << bcl_eta_ext << " bcl_eta_in : " << bcl_eta_in <<  " rho : " << rho << " bcl_mu_eq : " << bcl_mu_eq << " bcl_mu_in : " << bcl_mu_in <<std::endl;
  		bool is_primal_feasible =
				primal_feasibility_lhs <=
				(eps_abs + eps_rel * max2(
                                          max2(
																 primal_feasibility_eq_rhs_0,
                                                                 primal_feasibility_in_rhs_0),
                                          max2(
										                    max2(
                                                                 primal_feasibility_rhs_1_eq,
                                                                 primal_feasibility_rhs_1_in_u
                                                                ),
                                                            primal_feasibility_rhs_1_in_l
                                              ) 
                                         
                                        ));

		bool is_dual_feasible =
				dual_feasibility_lhs <=
				(eps_abs + eps_rel * max2(                      
                                                                max2(   dual_feasibility_rhs_3,
																        dual_feasibility_rhs_0
                                                                ),
																max2( //
																		 dual_feasibility_rhs_1,
																		 dual_feasibility_rhs_2
																	)
										  )
																		 );

		if (is_primal_feasible){

			rho = max2(rho / T(10), T(1e-7));
			if (is_dual_feasible){
				{
				LDLT_DECL_SCOPE_TIMER("in solver", "unscale solution", T);
				precond.unscale_primal_in_place(x); 
				precond.unscale_dual_in_place_eq(y);
				precond.unscale_dual_in_place_in(z);
				}
				//std::cout << "dual unscaled " << (qp.H.to_eigen())*x.to_eigen() + qp.g.to_eigen()+ (qp.A.to_eigen()).transpose()*y.to_eigen() + (qp.C.to_eigen()).transpose()*z.to_eigen() << std::endl;
				return {n_ext, n_mu_updates,n_tot};

			}
		}

		auto xe = x.to_eigen().eval(); 
		auto ye = y.to_eigen().eval(); 
		auto ze = z.to_eigen().eval(); 
		/*
		std::cout << "x before initial guess " << x.to_eigen() << std::endl;
		std::cout << "y before initial guess " << y.to_eigen() << std::endl;
		std::cout << "z before initial guess " << z.to_eigen() << std::endl;
		*/
		T err_in = qp::detail::initial_guess_loop<T,Preconditioner>(
						VectorView<T>{from_eigen,xe},
						VectorView<T>{from_eigen,ye},
						VectorView<T>{from_eigen,ze},
						x,
						y,
						z,
						qp_scaled,
						bcl_mu_in,
						bcl_mu_eq,
						rho,
						bcl_eta_in,
						precond,
						dim,
						n_eq,
						n_in
		);
		n_tot +=1;
		//std::cout << "x after initial guess " << x.to_eigen() << std::endl;
		std::cout << " error from initial guess : " << err_in << " bcl_eta_in " << bcl_eta_in << std::endl;
		
		if (err_in >= bcl_eta_in){

			err_in = qp::detail::correction_guess_loop(
						VectorView<T>{from_eigen,xe},
						VectorView<T>{from_eigen,ye},
						VectorView<T>{from_eigen,ze},
						x,
						y,
						z,
						qp_scaled,
						bcl_mu_in,
						bcl_mu_eq,
						rho,
						bcl_eta_in,
						dim,
						n_eq,
						n_in,
						max_iter_in,
						n_tot
			);
			//std::cout << " error from correction guess : " << err_in << std::endl;
		}
		
		
		// CHECK  CHANGE INPLACE OK BELOW
		
		T primal_feasibility_lhs_new(primal_feasibility_lhs) ; 

		qp::detail::global_primal_residual(
						primal_feasibility_lhs_new,
						primal_feasibility_eq_rhs_0,
						primal_feasibility_in_rhs_0,
						//primal_feasibility_eq_lhs,
						//primal_feasibility_in_lhs,
						primal_residual_eq_scaled,
						primal_residual_in_scaled_u,
						primal_residual_in_scaled_l,
						qp,
						//residual_scaled_tmp,
						qp_scaled,
						precond,
						x,
						dim,
						n_eq,
						n_in
		);

		//std::cout << "primal_feasibility_lhs_new for BCL " << primal_feasibility_lhs_new << std::endl;

		qp::detail::BCL_update(
					primal_feasibility_lhs_new,
					VectorViewMut<T>{from_eigen,primal_residual_in_scaled_u},
					VectorViewMut<T>{from_eigen,primal_residual_in_scaled_l},
					VectorViewMut<T>{from_eigen,primal_residual_eq_scaled},
					precond,
					bcl_eta_ext,
					bcl_eta_in,
					eps_abs,
					n_mu_updates,
					bcl_mu_in,
					bcl_mu_eq,
					VectorViewMut<T>{from_eigen,ye},
					VectorViewMut<T>{from_eigen,ze},
					y,
					z
		);

		
		// COLD RESTART
		
		T dual_feasibility_lhs_new(dual_feasibility_lhs) ; 

		qp::detail::global_dual_residual(
			dual_feasibility_lhs_new,
			dual_feasibility_rhs_0,
			dual_feasibility_rhs_1,
        	dual_feasibility_rhs_3,
			dual_residual_scaled,
			residual_scaled_tmp,
			qp_scaled,
			precond,
			x,
			y,
			z,
			dim,
			n_eq,
			n_in
		);

		if ((primal_feasibility_lhs_new / max2(primal_feasibility_lhs,machine_eps) >= T(1)) && (dual_feasibility_lhs_new / max2(primal_feasibility_lhs,machine_eps) >= T(1)) && bcl_mu_in >= T(1.E5)){
			std::cout << "cold restart" << std::endl;
			bcl_mu_in = T(1.1);
			bcl_mu_eq = T(1.1);
		}
		
		
	}
	
	return {max_iter, n_mu_updates, n_tot};
}


template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
QpSolveStats qpSolve( //
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBox<T> qp,
		isize max_iter,
		isize max_iter_in,
		T eps_abs,
		T eps_rel,
		T err_IG,
		T beta,
		T R,
		Preconditioner precond = Preconditioner{}) {

	using namespace ldlt::tags;

	isize dim = qp.H.rows;
	isize n_eq = qp.A.rows;
    isize n_in = qp.C.rows;
	isize n_c = 0 ;

	isize n_mu_updates = 0;
	isize n_tot = 0;
	isize n_ext = 0;

	T machine_eps = std::numeric_limits<T>::epsilon();
	auto rho = T(1e-6);
	auto bcl_mu_eq = T(1e3);
    auto bcl_mu_in = T(1e1);
	T bcl_eta_ext = 1 / pow(bcl_mu_in, T(0.1));
    T bcl_eta_in = T(1);
	
	LDLT_MULTI_WORKSPACE_MEMORY(
			(_h_scaled,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
			(_h_ws,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
	     	(_g_scaled,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
	     	(_a_scaled,Uninit, Mat(n_eq, dim),LDLT_CACHELINE_BYTES, T),
		 	(_c_scaled,Uninit, Mat(n_in, dim),LDLT_CACHELINE_BYTES, T),
	     	(_b_scaled,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_u_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
         	(_l_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
	     	(_residual_scaled,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	//(_residual_scaled_tmp,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	(_y,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_z,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(xe_,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
	     	(_diag_diff_eq,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_diag_diff_in,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			
			(_kkt,Uninit, Mat(dim+n_eq+n_c, dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T),

			(current_bijection_map_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, isize),

			(_dw_aug,Init, Vec(dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(d_dual_for_eq_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(Cdx__,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(d_primal_residual_eq_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(l_active_set_n_u_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool),
			(l_active_set_n_l_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool),
			(active_inequalities_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool)
		);

	Eigen::Matrix<T, Eigen::Dynamic, 1> d_dual_for_eq = d_dual_for_eq_.to_eigen();
	Eigen::Matrix<T, Eigen::Dynamic, 1> Cdx_ = Cdx__.to_eigen();
	Eigen::Matrix<T, Eigen::Dynamic, 1> d_primal_residual_eq = d_primal_residual_eq_.to_eigen();
	Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_u = l_active_set_n_u_.to_eigen();
	Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_l = l_active_set_n_l_.to_eigen();
	Eigen::Matrix<bool, Eigen::Dynamic, 1> active_inequalities = active_inequalities_.to_eigen();	
	Eigen::Matrix<T, Eigen::Dynamic, 1> dw_aug = _dw_aug.to_eigen(); 

	Eigen::Matrix<isize, Eigen::Dynamic, 1> current_bijection_map = current_bijection_map_.to_eigen(); 
	for (isize i =0;i<n_in;i++){
		current_bijection_map(i) = i;
	}

	auto H_copy = _h_scaled.to_eigen();
	auto kkt = _kkt.to_eigen();
	auto H_ws = _h_ws.to_eigen();
	auto q_copy = _g_scaled.to_eigen();
	auto A_copy = _a_scaled.to_eigen();
	auto C_copy = _c_scaled.to_eigen();
	auto b_copy = _b_scaled.to_eigen();
    auto u_copy = _u_scaled.to_eigen();
    auto l_copy = _l_scaled.to_eigen();

	H_copy = qp.H.to_eigen();
	q_copy = qp.g.to_eigen();
	A_copy = qp.A.to_eigen();
	b_copy = qp.b.to_eigen();
    C_copy = qp.C.to_eigen();
    u_copy = qp.u.to_eigen();
    l_copy = qp.l.to_eigen();

	auto qp_scaled = qp::QpViewBoxMut<T>{
			{from_eigen, H_copy},
			{from_eigen, q_copy},
			{from_eigen, A_copy},
			{from_eigen, b_copy},
			{from_eigen, C_copy},
			{from_eigen, u_copy},
            {from_eigen, l_copy}
	};
	precond.scale_qp_in_place(qp_scaled);

	kkt.topLeftCorner(dim, dim) = qp_scaled.H.to_eigen();
	for (isize i = 0; i < dim; ++i) {
		kkt(i, i) += rho;
	}
	kkt.block(0,dim,dim,n_eq) = qp_scaled.A.to_eigen().transpose();
	kkt.block(dim,0,n_eq,dim) = qp_scaled.A.to_eigen();
	kkt.bottomRightCorner(n_eq+n_c, n_eq+n_c).setZero();
	{
		T tmp_eq = -T(1) / bcl_mu_eq;
		T tmp_in = -T(1) / bcl_mu_in;
		for (isize i = 0; i < n_eq; ++i) {
			kkt(dim + i, dim + i) = tmp_eq;
		}
		for (isize i = 0; i < n_c; ++i) {
			kkt(dim + n_eq + i, dim + n_eq + i) = tmp_in;
		}
	}
	
	ldlt::Ldlt<T> ldl{decompose, kkt};

	H_ws = H_copy;
	for (isize i = 0;i< dim ; ++i){
		H_ws(i,i) += rho ;
	}

	ldlt::Ldlt<T> ldl_ws{decompose, H_ws};
	x.to_eigen().array() = -(qp_scaled.g).to_eigen().array();
	ldl_ws.solve_in_place(x.to_eigen());

	auto residual_scaled = _residual_scaled.to_eigen();
	//Eigen::Matrix<T, Eigen::Dynamic, 1> residual_scaled_tmp = _residual_scaled_tmp.to_eigen();

	auto ye = _y.to_eigen();
    auto ze = _z.to_eigen();
	auto xe = xe_.to_eigen();
	auto diag_diff_in = _diag_diff_in.to_eigen();
	auto diag_diff_eq = _diag_diff_eq.to_eigen();

	T primal_feasibility_rhs_1_eq = infty_norm(qp.b.to_eigen());
    T primal_feasibility_rhs_1_in_u = infty_norm(qp.u.to_eigen());
    T primal_feasibility_rhs_1_in_l = infty_norm(qp.l.to_eigen());
	T dual_feasibility_rhs_2 = infty_norm(qp.g.to_eigen());

	T correction_guess_rhs_g =  infty_norm( (qp_scaled.g).to_eigen()  );
	
	Eigen::Matrix<T, Eigen::Dynamic, 1> dual_residual_scaled = residual_scaled.topRows(dim);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_eq_scaled = residual_scaled.middleRows(dim,n_eq);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_u = residual_scaled.bottomRows(n_in);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_l = residual_scaled.bottomRows(n_in);

	T primal_feasibility_eq_rhs_0(0);
	T primal_feasibility_in_rhs_0(0);
	T dual_feasibility_rhs_0(0);
	T dual_feasibility_rhs_1(0);
	T dual_feasibility_rhs_3(0);

	T primal_feasibility_lhs(0);
	T primal_feasibility_eq_lhs(0);
	T primal_feasibility_in_lhs(0);
	T dual_feasibility_lhs(0);

	for (i64 iter = 0; iter <= max_iter; ++iter) {
		n_ext +=1;
		if (iter == max_iter) {
			break;
		}

		// compute primal residual
		
		qp::detail::global_primal_residual(
				primal_feasibility_lhs,
				primal_feasibility_eq_rhs_0,
				primal_feasibility_in_rhs_0,
				//primal_feasibility_eq_lhs,
				//primal_feasibility_in_lhs,
				primal_residual_eq_scaled,
				primal_residual_in_scaled_u,
				primal_residual_in_scaled_l,
				qp,
				//residual_scaled_tmp,
				qp_scaled,
				precond,
				x,
				dim,
				n_eq,
				n_in
		);
		qp::detail::global_dual_residual(
			dual_feasibility_lhs,
			dual_feasibility_rhs_0,
			dual_feasibility_rhs_1,
        	dual_feasibility_rhs_3,
			dual_residual_scaled,
			//residual_scaled_tmp,
			dw_aug,
			qp_scaled,
			precond,
			x,
			y,
			z,
			dim,
			n_eq,
			n_in
		);

		std::cout << "---------------it : " << iter << " primal residual : " << primal_feasibility_lhs << " dual residual : " << dual_feasibility_lhs << std::endl;
		std::cout << "bcl_eta_ext : " << bcl_eta_ext << " bcl_eta_in : " << bcl_eta_in <<  " rho : " << rho << " bcl_mu_eq : " << bcl_mu_eq << " bcl_mu_in : " << bcl_mu_in <<std::endl;
  		bool is_primal_feasible =
				primal_feasibility_lhs <=
				(eps_abs + eps_rel * max2(
                                          max2(
																 primal_feasibility_eq_rhs_0,
                                                                 primal_feasibility_in_rhs_0),
                                          max2(
										                    max2(
                                                                 primal_feasibility_rhs_1_eq,
                                                                 primal_feasibility_rhs_1_in_u
                                                                ),
                                                            primal_feasibility_rhs_1_in_l
                                              ) 
                                         
                                        ));

		bool is_dual_feasible =
				dual_feasibility_lhs <=
				(eps_abs + eps_rel * max2(                      
                                                                max2(   dual_feasibility_rhs_3,
																        dual_feasibility_rhs_0
                                                                ),
																max2( //
																		 dual_feasibility_rhs_1,
																		 dual_feasibility_rhs_2
																	)
										  ) 
																		 );

		if (is_primal_feasible){
			
			if (dual_feasibility_lhs > T(1e-2) && rho != T(1e-7)){
				T rho_new = max2(rho / T(10), T(1e-7));  
				qp::detail::refactorize(
							qp_scaled.as_const(),
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							MatrixViewMut<T,colmajor>{from_eigen,kkt},
							dim,
							n_eq,
							n_c,
							n_in,
							bcl_mu_eq,
							bcl_mu_in,
							rho,
							rho_new,
							ldl
				);
				rho = rho_new;
			}
			
			if (is_dual_feasible){
				{
				LDLT_DECL_SCOPE_TIMER("in solver", "unscale solution", T);
				precond.unscale_primal_in_place(x); 
				precond.unscale_dual_in_place_eq(y);
				precond.unscale_dual_in_place_in(z);
				}
				return {n_ext, n_mu_updates,n_tot};
			}
		}

		xe = x.to_eigen().eval(); 
		ye = y.to_eigen().eval(); 
		ze = z.to_eigen().eval(); 
		/*
		T err_in = qp::detail::initial_guess<T,Preconditioner>(
						VectorView<T>{from_eigen,xe},
						VectorView<T>{from_eigen,ye},
						VectorView<T>{from_eigen,ze},
						x,
						y,
						z,
						qp_scaled,
						bcl_mu_in,
						bcl_mu_eq,
						rho,
						bcl_eta_in,
						precond,
						dim,
						n_eq,
						n_in,
						primal_residual_eq_scaled,
						primal_residual_in_scaled_u,
						primal_residual_in_scaled_l,
						dual_residual_scaled,
						d_dual_for_eq,
						Cdx_,
						d_primal_residual_eq,
						l_active_set_n_u,
						l_active_set_n_l,
						active_inequalities,
						dw_aug
		);
		*/
		if (primal_feasibility_lhs < err_IG){

			T err_in = qp::detail::initial_guess_fact(
							VectorView<T>{from_eigen,xe},
							VectorView<T>{from_eigen,ye},
							VectorView<T>{from_eigen,ze},
							x,
							y,
							z,
							qp_scaled,
							bcl_mu_in,
							bcl_mu_eq,
							rho,
							bcl_eta_in,
							precond,
							dim,
							n_eq,
							n_in,
							primal_residual_eq_scaled,
							primal_residual_in_scaled_u,
							primal_residual_in_scaled_l,
							dual_residual_scaled,
							d_dual_for_eq,
							Cdx_,
							d_primal_residual_eq,
							l_active_set_n_u,
							l_active_set_n_l,
							active_inequalities,
							dw_aug,

							ldl,
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							n_c,
							R
			);
			n_tot +=1;

			if (err_in >= bcl_eta_in){
				
				dual_residual_scaled.array() += (-(qp_scaled.C).to_eigen().transpose()*z.to_eigen() + bcl_mu_eq * (qp_scaled.A).to_eigen().transpose()*primal_residual_eq_scaled ).array();
				primal_residual_eq_scaled.array() += (y.to_eigen()/bcl_mu_eq).array();
				primal_residual_in_scaled_u.array() += (z.to_eigen()/bcl_mu_in).array();
				primal_residual_in_scaled_l.array() += (z.to_eigen()/bcl_mu_in).array();

				err_in = qp::detail::correction_guess(
							VectorView<T>{from_eigen,xe},
							VectorView<T>{from_eigen,ye},
							VectorView<T>{from_eigen,ze},
							x,
							y,
							z,
							qp_scaled,
							bcl_mu_in,
							bcl_mu_eq,
							rho,
							bcl_eta_in,
							dim,
							n_eq,
							n_in,
							max_iter_in,
							n_tot,
							primal_residual_eq_scaled,
							primal_residual_in_scaled_u,
							primal_residual_in_scaled_l,
							dual_residual_scaled,
							d_primal_residual_eq,
							Cdx_,
							d_dual_for_eq,
							l_active_set_n_u,
							l_active_set_n_l,
							active_inequalities,

							ldl,
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							n_c,
							dw_aug,
							correction_guess_rhs_g
				);
				std::cout <<"primal_feasibility_lhs "<<primal_feasibility_lhs<< " error from initial guess : " << err_in << " bcl_eta_in " << bcl_eta_in << std::endl;
			}

		}else{
				dual_residual_scaled.array() += (-(qp_scaled.C).to_eigen().transpose()*z.to_eigen() + bcl_mu_eq * (qp_scaled.A).to_eigen().transpose()*primal_residual_eq_scaled ).array();
				primal_residual_eq_scaled.array() += (ye/bcl_mu_eq).array();
				primal_residual_in_scaled_u = qp_scaled.C.to_eigen() * x.to_eigen() - qp_scaled.u.to_eigen() + ze / bcl_mu_in ;
				primal_residual_in_scaled_l = qp_scaled.C.to_eigen() * x.to_eigen() - qp_scaled.l.to_eigen() + ze / bcl_mu_in ;

				T err_in = qp::detail::correction_guess(
							VectorView<T>{from_eigen,xe},
							VectorView<T>{from_eigen,ye},
							VectorView<T>{from_eigen,ze},
							x,
							y,
							z,
							qp_scaled,
							bcl_mu_in,
							bcl_mu_eq,
							rho,
							bcl_eta_in,
							dim,
							n_eq,
							n_in,
							max_iter_in,
							n_tot,
							primal_residual_eq_scaled,
							primal_residual_in_scaled_u,
							primal_residual_in_scaled_l,
							dual_residual_scaled,
							d_primal_residual_eq,
							Cdx_,
							d_dual_for_eq,
							l_active_set_n_u,
							l_active_set_n_l,
							active_inequalities,

							ldl,
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							n_c,
							dw_aug,
							correction_guess_rhs_g
				);
				std::cout <<"primal_feasibility_lhs "<<primal_feasibility_lhs<< " error from initial guess : " << err_in << " bcl_eta_in " << bcl_eta_in << std::endl;
		}
		
		
		T primal_feasibility_lhs_new(primal_feasibility_lhs) ; 

		qp::detail::global_primal_residual(
						primal_feasibility_lhs_new,
						primal_feasibility_eq_rhs_0,
						primal_feasibility_in_rhs_0,
						//primal_feasibility_eq_lhs,
						//primal_feasibility_in_lhs,
						primal_residual_eq_scaled,
						primal_residual_in_scaled_u,
						primal_residual_in_scaled_l,
						qp,
						//primal_residual_eq_unscaled,
						//residual_scaled_tmp,
						qp_scaled,
						precond,
						x,
						dim,
						n_eq,
						n_in
		);

		/*
		qp::detail::BCL_update(
					primal_feasibility_lhs_new,
					VectorViewMut<T>{from_eigen,primal_residual_in_scaled_u},
					VectorViewMut<T>{from_eigen,primal_residual_in_scaled_l},
					VectorViewMut<T>{from_eigen,primal_residual_eq_scaled},
					precond,
					bcl_eta_ext,
					bcl_eta_in,
					eps_abs,
					n_mu_updates,
					bcl_mu_in,
					bcl_mu_eq,
					VectorViewMut<T>{from_eigen,ye},
					VectorViewMut<T>{from_eigen,ze},
					y,
					z
		);
		*/
		qp::detail::BCL_update_fact(
					primal_feasibility_lhs_new,
					bcl_eta_ext,
					bcl_eta_in,
					eps_abs,
					n_mu_updates,
					bcl_mu_in,
					bcl_mu_eq,
					VectorViewMut<T>{from_eigen,ye},
					VectorViewMut<T>{from_eigen,ze},
					y,
					z,
					dim,
					n_eq,
					n_c,
					ldl,
					qp_scaled.as_const(),
					rho,
					beta

		);

		// COLD RESTART
		
		T dual_feasibility_lhs_new(dual_feasibility_lhs) ; 
		
		qp::detail::global_dual_residual(
			dual_feasibility_lhs_new,
			dual_feasibility_rhs_0,
			dual_feasibility_rhs_1,
        	dual_feasibility_rhs_3,
			dual_residual_scaled,
			//dual_residual_unscaled,
			//residual_scaled_tmp,
			dw_aug,
			qp_scaled,
			precond,
			x,
			y,
			z,
			dim,
			n_eq,
			n_in
		);

		if ((primal_feasibility_lhs_new / max2(primal_feasibility_lhs,machine_eps) >= T(1)) && (dual_feasibility_lhs_new / max2(primal_feasibility_lhs,machine_eps) >= T(1)) && bcl_mu_in >= T(1.E5)){
			std::cout << "cold restart" << std::endl;
			
			T new_bcl_mu_eq = T(1.1);
			T new_bcl_mu_in = T(1.1);

			qp::detail::mu_update(
				bcl_mu_eq,
				new_bcl_mu_eq,
				bcl_mu_in,
				new_bcl_mu_in,
				dim,
				n_eq,
				n_c,
				ldl,
				qp_scaled.as_const(),
				rho
				//VectorViewMut<isize>{from_eigen,current_bijection_map}
			);
			bcl_mu_in = new_bcl_mu_in;
			bcl_mu_eq = new_bcl_mu_eq;
		}
		
	}
	
	return {max_iter, n_mu_updates, n_tot};
}


template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
QpSolveStats QPALMSolve( //
		VectorViewMut<T> x,
		VectorViewMut<T> y,
        VectorViewMut<T> z,
		qp::QpViewBox<T> qp,
		isize max_iter,
		isize max_iter_in,
		T eps_abs,
		T eps_rel,
		T eps_IG,
		T R,
		Preconditioner precond = Preconditioner{}) {

	using namespace ldlt::tags;

	isize dim = qp.H.rows;
	isize n_eq = qp.A.rows;
    isize n_in = qp.C.rows;
	isize n_c = 0 ;

	isize n_mu_updates = 0;
	isize n_tot = 0;
	isize n_ext = 0;

	T machine_eps = std::numeric_limits<T>::epsilon();
	auto rho = T(1e-6);
	auto bcl_mu_eq = T(1e3);
    auto bcl_mu_in = T(1e1);
	T bcl_eta_ext = T(1);
    T bcl_eta_in = T(1);
	
	LDLT_MULTI_WORKSPACE_MEMORY(
			(_h_scaled,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
			(_h_ws,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
	     	(_g_scaled,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
	     	(_a_scaled,Uninit, Mat(n_eq, dim),LDLT_CACHELINE_BYTES, T),
		 	(_c_scaled,Uninit, Mat(n_in, dim),LDLT_CACHELINE_BYTES, T),
	     	(_b_scaled,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_u_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
         	(_l_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
	     	(_residual_scaled,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	(_y,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_z,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(xe_,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
	     	(_diag_diff_eq,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_diag_diff_in,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			
			(_kkt,Uninit, Mat(dim+n_eq+n_c, dim+n_eq+n_c),LDLT_CACHELINE_BYTES, T),

			(current_bijection_map_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, isize),

			(_dw_aug,Init, Vec(dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(d_dual_for_eq_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(Cdx__,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(d_primal_residual_eq_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
			(l_active_set_n_u_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool),
			(l_active_set_n_l_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool),
			(active_inequalities_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool)
		);

	Eigen::Matrix<T, Eigen::Dynamic, 1> d_dual_for_eq = d_dual_for_eq_.to_eigen();
	Eigen::Matrix<T, Eigen::Dynamic, 1> Cdx_ = Cdx__.to_eigen();
	Eigen::Matrix<T, Eigen::Dynamic, 1> d_primal_residual_eq = d_primal_residual_eq_.to_eigen();
	Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_u = l_active_set_n_u_.to_eigen();
	Eigen::Matrix<bool, Eigen::Dynamic, 1> l_active_set_n_l = l_active_set_n_l_.to_eigen();
	Eigen::Matrix<bool, Eigen::Dynamic, 1> active_inequalities = active_inequalities_.to_eigen();	
	Eigen::Matrix<T, Eigen::Dynamic, 1> dw_aug = _dw_aug.to_eigen(); 

	Eigen::Matrix<isize, Eigen::Dynamic, 1> current_bijection_map = current_bijection_map_.to_eigen(); 
	for (isize i =0;i<n_in;i++){
		current_bijection_map(i) = i;
	}

	auto H_copy = _h_scaled.to_eigen();
	auto kkt = _kkt.to_eigen();
	auto H_ws = _h_ws.to_eigen();
	auto q_copy = _g_scaled.to_eigen();
	auto A_copy = _a_scaled.to_eigen();
	auto C_copy = _c_scaled.to_eigen();
	auto b_copy = _b_scaled.to_eigen();
    auto u_copy = _u_scaled.to_eigen();
    auto l_copy = _l_scaled.to_eigen();

	H_copy = qp.H.to_eigen();
	q_copy = qp.g.to_eigen();
	A_copy = qp.A.to_eigen();
	b_copy = qp.b.to_eigen();
    C_copy = qp.C.to_eigen();
    u_copy = qp.u.to_eigen();
    l_copy = qp.l.to_eigen();

	auto qp_scaled = qp::QpViewBoxMut<T>{
			{from_eigen, H_copy},
			{from_eigen, q_copy},
			{from_eigen, A_copy},
			{from_eigen, b_copy},
			{from_eigen, C_copy},
			{from_eigen, u_copy},
            {from_eigen, l_copy}
	};
	precond.scale_qp_in_place(qp_scaled);

	kkt.topLeftCorner(dim, dim) = qp_scaled.H.to_eigen();
	for (isize i = 0; i < dim; ++i) {
		kkt(i, i) += rho;
	}
	kkt.block(0,dim,dim,n_eq) = qp_scaled.A.to_eigen().transpose();
	kkt.block(dim,0,n_eq,dim) = qp_scaled.A.to_eigen();
	kkt.bottomRightCorner(n_eq+n_c, n_eq+n_c).setZero();
	{
		T tmp_eq = -T(1) / bcl_mu_eq;
		T tmp_in = -T(1) / bcl_mu_in;
		for (isize i = 0; i < n_eq; ++i) {
			kkt(dim + i, dim + i) = tmp_eq;
		}
		for (isize i = 0; i < n_c; ++i) {
			kkt(dim + n_eq + i, dim + n_eq + i) = tmp_in;
		}
	}
	
	ldlt::Ldlt<T> ldl{decompose, kkt};

	H_ws = H_copy;
	for (isize i = 0;i< dim ; ++i){
		H_ws(i,i) += rho ;
	}

	ldlt::Ldlt<T> ldl_ws{decompose, H_ws};
	x.to_eigen().array() = -(qp_scaled.g).to_eigen().array();
	ldl_ws.solve_in_place(x.to_eigen());

	auto residual_scaled = _residual_scaled.to_eigen();

	auto ye = _y.to_eigen();
    auto ze = _z.to_eigen();
	auto xe = xe_.to_eigen();
	auto diag_diff_in = _diag_diff_in.to_eigen();
	auto diag_diff_eq = _diag_diff_eq.to_eigen();

	T primal_feasibility_rhs_1_eq = infty_norm(qp.b.to_eigen());
    T primal_feasibility_rhs_1_in_u = infty_norm(qp.u.to_eigen());
    T primal_feasibility_rhs_1_in_l = infty_norm(qp.l.to_eigen());
	T dual_feasibility_rhs_2 = infty_norm(qp.g.to_eigen());

	T correction_guess_rhs_g =  infty_norm( (qp_scaled.g).to_eigen()  );
	
	Eigen::Matrix<T, Eigen::Dynamic, 1> dual_residual_scaled = residual_scaled.topRows(dim);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_eq_scaled = residual_scaled.middleRows(dim,n_eq);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_u = residual_scaled.bottomRows(n_in);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_l = residual_scaled.bottomRows(n_in);

	T primal_feasibility_eq_rhs_0(0);
	T primal_feasibility_in_rhs_0(0);
	T dual_feasibility_rhs_0(0);
	T dual_feasibility_rhs_1(0);
	T dual_feasibility_rhs_3(0);

	T primal_feasibility_lhs(0);
	T primal_feasibility_eq_lhs(0);
	T primal_feasibility_in_lhs(0);
	T dual_feasibility_lhs(0);

	xe = x.to_eigen().eval(); 
	ye = y.to_eigen().eval(); 
	ze = z.to_eigen().eval(); 
	for (i64 iter = 0; iter <= max_iter; ++iter) {
		n_ext +=1;
		if (iter == max_iter) {
			break;
		}

		// compute primal residual
		
		qp::detail::global_primal_residual(
				primal_feasibility_lhs,
				primal_feasibility_eq_rhs_0,
				primal_feasibility_in_rhs_0,
				primal_residual_eq_scaled,
				primal_residual_in_scaled_u,
				primal_residual_in_scaled_l,
				qp,
				qp_scaled,
				precond,
				x,
				dim,
				n_eq,
				n_in
		);
		//auto primal_residual_eq_scaled_old = primal_residual_eq_scaled;
		//auto primal_residual_in_scaled_in_old =  primal_residual_in_scaled_l ; 

		qp::detail::global_dual_residual(
			dual_feasibility_lhs,
			dual_feasibility_rhs_0,
			dual_feasibility_rhs_1,
        	dual_feasibility_rhs_3,
			dual_residual_scaled,
			dw_aug,
			qp_scaled,
			precond,
			x,
			y,
			z,
			dim,
			n_eq,
			n_in
		);

		std::cout << "---------------it : " << iter << " primal residual : " << primal_feasibility_lhs << " dual residual : " << dual_feasibility_lhs << std::endl;
		std::cout << "bcl_eta_ext : " << bcl_eta_ext << " bcl_eta_in : " << bcl_eta_in <<  " rho : " << rho << " bcl_mu_eq : " << bcl_mu_eq << " bcl_mu_in : " << bcl_mu_in <<std::endl;
  		bool is_primal_feasible =
				primal_feasibility_lhs <=
				(eps_abs + eps_rel * max2(
                                          max2(
																 primal_feasibility_eq_rhs_0,
                                                                 primal_feasibility_in_rhs_0),
                                          max2(
										                    max2(
                                                                 primal_feasibility_rhs_1_eq,
                                                                 primal_feasibility_rhs_1_in_u
                                                                ),
                                                            primal_feasibility_rhs_1_in_l
                                              ) 
                                         
                                        ));

		bool is_dual_feasible =
				dual_feasibility_lhs <=
				(eps_abs + eps_rel * max2(                      
                                                                max2(   dual_feasibility_rhs_3,
																        dual_feasibility_rhs_0
                                                                ),
																max2( //
																		 dual_feasibility_rhs_1,
																		 dual_feasibility_rhs_2
																	)
										  ) 
																		 );

		if (is_primal_feasible){
			
			if (is_dual_feasible){
				{
				LDLT_DECL_SCOPE_TIMER("in solver", "unscale solution", T);
				precond.unscale_primal_in_place(x); 
				precond.unscale_dual_in_place_eq(y);
				precond.unscale_dual_in_place_in(z);
				}
				return {n_ext, n_mu_updates,n_tot};
			}
		}

		if (primal_feasibility_lhs < eps_IG){

			T err_in = qp::detail::initial_guess_fact(
							VectorView<T>{from_eigen,xe},
							VectorView<T>{from_eigen,ye},
							VectorView<T>{from_eigen,ze},
							x,
							y,
							z,
							qp_scaled,
							bcl_mu_in,
							bcl_mu_eq,
							rho,
							bcl_eta_in,
							precond,
							dim,
							n_eq,
							n_in,
							primal_residual_eq_scaled,
							primal_residual_in_scaled_u,
							primal_residual_in_scaled_l,
							dual_residual_scaled,
							d_dual_for_eq,
							Cdx_,
							d_primal_residual_eq,
							l_active_set_n_u,
							l_active_set_n_l,
							active_inequalities,
							dw_aug,

							ldl,
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							n_c,
							R 
			);
			n_tot +=1;

			if (err_in >= bcl_eta_in){
				
				dual_residual_scaled.array() += (-(qp_scaled.C).to_eigen().transpose()*z.to_eigen() + bcl_mu_eq * (qp_scaled.A).to_eigen().transpose()*primal_residual_eq_scaled ).array();
				primal_residual_eq_scaled.array() += (y.to_eigen()/bcl_mu_eq).array();
				primal_residual_in_scaled_u.array() += (z.to_eigen()/bcl_mu_in).array();
				primal_residual_in_scaled_l.array() += (z.to_eigen()/bcl_mu_in).array();

				err_in = qp::detail::correction_guess(
							VectorView<T>{from_eigen,xe},
							VectorView<T>{from_eigen,ye},
							VectorView<T>{from_eigen,ze},
							x,
							y,
							z,
							qp_scaled,
							bcl_mu_in,
							bcl_mu_eq,
							rho,
							bcl_eta_in,
							dim,
							n_eq,
							n_in,
							max_iter_in,
							n_tot,
							primal_residual_eq_scaled,
							primal_residual_in_scaled_u,
							primal_residual_in_scaled_l,
							dual_residual_scaled,
							d_primal_residual_eq,
							Cdx_,
							d_dual_for_eq,
							l_active_set_n_u,
							l_active_set_n_l,
							active_inequalities,

							ldl,
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							n_c,
							dw_aug,
							correction_guess_rhs_g
				);
				std::cout <<"primal_feasibility_lhs "<<primal_feasibility_lhs<< " error from initial guess : " << err_in << " bcl_eta_in " << bcl_eta_in << std::endl;
			}

		}else{
				/*
				dual_residual_scaled.array() += (-(qp_scaled.C).to_eigen().transpose()*z.to_eigen() + bcl_mu_eq * (qp_scaled.A).to_eigen().transpose()*primal_residual_eq_scaled ).array();
				primal_residual_eq_scaled.array() += (ye/bcl_mu_eq).array();
				primal_residual_in_scaled_u = qp_scaled.C.to_eigen() * x.to_eigen() - qp_scaled.u.to_eigen() + ze / bcl_mu_in ;
				primal_residual_in_scaled_l = qp_scaled.C.to_eigen() * x.to_eigen() - qp_scaled.l.to_eigen() + ze / bcl_mu_in ;
				*/

				primal_residual_eq_scaled.array() += (ye/bcl_mu_eq).array();
				dual_residual_scaled.array() += (rho * (x.to_eigen()-xe) -(qp_scaled.A).to_eigen().transpose()*y.to_eigen() -(qp_scaled.C).to_eigen().transpose()*z.to_eigen() + bcl_mu_eq * (qp_scaled.A).to_eigen().transpose()*primal_residual_eq_scaled ).array();
				primal_residual_in_scaled_u = qp_scaled.C.to_eigen() * x.to_eigen() - qp_scaled.u.to_eigen() + ze / bcl_mu_in ;
				primal_residual_in_scaled_l = qp_scaled.C.to_eigen() * x.to_eigen() - qp_scaled.l.to_eigen() + ze / bcl_mu_in ;
				T err_in = qp::detail::correction_guess(
							VectorView<T>{from_eigen,xe},
							VectorView<T>{from_eigen,ye},
							VectorView<T>{from_eigen,ze},
							x,
							y,
							z,
							qp_scaled,
							bcl_mu_in,
							bcl_mu_eq,
							rho,
							bcl_eta_in,
							dim,
							n_eq,
							n_in,
							max_iter_in,
							n_tot,
							primal_residual_eq_scaled,
							primal_residual_in_scaled_u,
							primal_residual_in_scaled_l,
							dual_residual_scaled,
							d_primal_residual_eq,
							Cdx_,
							d_dual_for_eq,
							l_active_set_n_u,
							l_active_set_n_l,
							active_inequalities,

							ldl,
							VectorViewMut<isize>{from_eigen,current_bijection_map},
							n_c,
							dw_aug,
							correction_guess_rhs_g
				);
				std::cout <<"primal_feasibility_lhs "<<primal_feasibility_lhs<< " error from inner loop : " << err_in << " bcl_eta_in " << bcl_eta_in << std::endl;
		}

		T primal_feasibility_lhs_new(primal_feasibility_lhs) ; 

		qp::detail::global_primal_residual(
						primal_feasibility_lhs_new,
						primal_feasibility_eq_rhs_0,
						primal_feasibility_in_rhs_0,
						primal_residual_eq_scaled,
						primal_residual_in_scaled_u,
						primal_residual_in_scaled_l,
						qp,
						qp_scaled,
						precond,
						x,
						dim,
						n_eq,
						n_in
		);

		qp::detail::QPALM_update_fact(
					primal_feasibility_lhs_new,
					bcl_eta_ext,
					bcl_eta_in,
					eps_abs,
					n_mu_updates,
					bcl_mu_in,
					bcl_mu_eq,
					VectorViewMut<T>{from_eigen,xe},
					VectorViewMut<T>{from_eigen,ye},
					VectorViewMut<T>{from_eigen,ze},
					x,
					y,
					z,
					dim,
					n_eq,
					n_c,
					ldl,
					qp_scaled.as_const(),
					rho

		);
		/*
		qp::detail::QPALM_mu_update(
					T& primal_feasibility_lhs,
					Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq_scaled,
					Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_in_scaled_l,
					Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_eq_scaled_old,
					Eigen::Matrix<T, Eigen::Dynamic, 1>& primal_residual_in_scaled_in_old,	
					T& bcl_eta_ext,
					T& bcl_eta_in,
					T eps_abs,
					isize& n_mu_updates,
					Eigen::Matrix<T, Eigen::Dynamic, 1>& mu,
					isize dim,
					isize n_eq,
					isize& n_c,
					ldlt::Ldlt<T>& ldl,
					qp::QpViewBox<T> qp_scaled,
					T rho,
					T theta,
					T sigmaMax,
					T Delta
		);
		*/
	}
	
	return {max_iter, n_mu_updates, n_tot};
}

template <typename T,typename Preconditioner = qp::preconditioner::IdentityPrecond>
QpSolveStats osqpSolve( //
		VectorViewMut<T> xe,
		VectorViewMut<T> ye,
		qp::QpViewBox<T> qp,
		isize max_iter,
		isize max_iter_in,
		T eps_abs,
		T eps_rel,
		Preconditioner precond = Preconditioner{}) {

	using namespace ldlt::tags;

	isize dim = qp.H.rows;
	isize n_eq = qp.A.rows;
    isize n_in = qp.C.rows;
	isize n_mu_updates = 0;
	isize n_tot = 0;
	isize n_ext = 0;

	T machine_eps = std::numeric_limits<T>::epsilon();
	auto rho = T(1e-6);
	auto mu_eq = T(1e4);
    auto mu_in = T(1e1);
	T alpha = T(1.6);
	
	LDLT_MULTI_WORKSPACE_MEMORY(
			(_h_scaled,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
			(_h_ws,Uninit, Mat(dim, dim),LDLT_CACHELINE_BYTES, T),
	     	(_g_scaled,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
	     	(_a_scaled,Uninit, Mat(n_eq, dim),LDLT_CACHELINE_BYTES, T),
		 	(_c_scaled,Uninit, Mat(n_in, dim),LDLT_CACHELINE_BYTES, T),
	     	(_b_scaled,Init, Vec(n_eq),LDLT_CACHELINE_BYTES, T),
         	(_u_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
         	(_l_scaled,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T),
	     	(_residual_scaled,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	//(_residual_scaled_tmp,Init, Vec(dim + n_eq+n_in),LDLT_CACHELINE_BYTES, T),
	     	//(y_,Init, Vec(n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(ze_,Init, Vec(n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(z_,Init, Vec(n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			//(x_,Init, Vec(dim),LDLT_CACHELINE_BYTES, T),
			(_dw,Init, Vec(dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(_htot,Uninit, Mat(dim+n_eq+n_in, dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(_rhs,Init, Vec(dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(err_,Init, Vec(dim+n_eq+n_in),LDLT_CACHELINE_BYTES, T),
			(tmp_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, T)
		);

	Eigen::Matrix<T, Eigen::Dynamic, 1> dw = _dw.to_eigen(); 
	Eigen::Matrix<T, Eigen::Dynamic, 1> err = err_.to_eigen(); 
	Eigen::Matrix<T, Eigen::Dynamic, 1> tmp = tmp_.to_eigen(); 
	err.setZero();
	dw.setZero();
	tmp.setZero();

	auto H_copy = _h_scaled.to_eigen();
	auto H_ws = _h_ws.to_eigen();
	auto q_copy = _g_scaled.to_eigen();
	auto A_copy = _a_scaled.to_eigen();
	auto C_copy = _c_scaled.to_eigen();
	auto b_copy = _b_scaled.to_eigen();
    auto u_copy = _u_scaled.to_eigen();
    auto l_copy = _l_scaled.to_eigen();

	H_copy = qp.H.to_eigen();
	q_copy = qp.g.to_eigen();
	A_copy = qp.A.to_eigen();
	b_copy = qp.b.to_eigen();
    C_copy = qp.C.to_eigen();
    u_copy = qp.u.to_eigen();
    l_copy = qp.l.to_eigen();

	auto qp_scaled = qp::QpViewBoxMut<T>{
			{from_eigen, H_copy},
			{from_eigen, q_copy},
			{from_eigen, A_copy},
			{from_eigen, b_copy},
			{from_eigen, C_copy},
			{from_eigen, u_copy},
            {from_eigen, l_copy}
	};
	precond.scale_qp_in_place(qp_scaled);
	
	H_ws = H_copy;
	for (isize i = 0;i< dim ; ++i){
		H_ws(i,i) += rho ;
	}

	ldlt::Ldlt<T> ldl_ws{decompose, H_ws};
	xe.to_eigen().array() = -(qp_scaled.g).to_eigen().array();
	ldl_ws.solve_in_place(xe.to_eigen());
		
	auto Htot = _htot.to_eigen().eval();
	auto rhs = _rhs.to_eigen().eval();
	Htot.setZero();
	//rhs.setZero();	

	Htot.topLeftCorner(dim, dim) = qp_scaled.H.to_eigen();
	for (isize i = 0; i < dim; ++i) {
		Htot(i, i) += rho;
	}

	T tmp_eq = -T(1) / mu_eq;
	T tmp_in = -T(1) / mu_in;
	for (isize i = 0; i < n_eq; ++i) {
		Htot(dim + i, dim + i) = tmp_eq;
	}
	for (isize i = 0; i < n_in; ++i) {
		Htot(dim + n_eq + i, dim + n_eq + i) = tmp_in;
	}
	
	Htot.block(0,dim,dim,n_eq) = qp_scaled.A.to_eigen().transpose();
	Htot.block(dim,0,n_eq,dim) = qp_scaled.A.to_eigen();

	Htot.block(0,dim+n_eq,dim,n_in) = qp_scaled.C.to_eigen().transpose();
	Htot.block(dim+n_eq,0,n_in,dim) = qp_scaled.C.to_eigen();

	ldlt::Ldlt<T> ldl{decompose, Htot};

	auto residual_scaled = _residual_scaled.to_eigen();
	//Eigen::Matrix<T, Eigen::Dynamic, 1> residual_scaled_tmp = _residual_scaled_tmp.to_eigen();

    auto ze = ze_.to_eigen();
	ze.topRows(n_eq) = qp_scaled.b.to_eigen();
	auto z = z_.to_eigen();
	z.topRows(n_eq) = qp_scaled.b.to_eigen() ;
	//auto y = y_.to_eigen();
	//auto x = x_.to_eigen();
	//x = xe.to_eigen();

	T primal_feasibility_rhs_1_eq = infty_norm(qp.b.to_eigen());
    T primal_feasibility_rhs_1_in_u = infty_norm(qp.u.to_eigen());
    T primal_feasibility_rhs_1_in_l = infty_norm(qp.l.to_eigen());
	T dual_feasibility_rhs_2 = infty_norm(qp.g.to_eigen());
	
	Eigen::Matrix<T, Eigen::Dynamic, 1> dual_residual_scaled = residual_scaled.topRows(dim);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_eq_scaled = residual_scaled.middleRows(dim,n_eq);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_u = residual_scaled.bottomRows(n_in);
	Eigen::Matrix<T, Eigen::Dynamic, 1> primal_residual_in_scaled_l = residual_scaled.bottomRows(n_in);

	T primal_feasibility_eq_rhs_0(0);
	T primal_feasibility_in_rhs_0(0);
	T dual_feasibility_rhs_0(0);
	T dual_feasibility_rhs_1(0);
	T dual_feasibility_rhs_3(0);

	T primal_feasibility_lhs(0);
	T primal_feasibility_eq_lhs(0);
	T primal_feasibility_in_lhs(0);
	T dual_feasibility_lhs(0);
	T rhs_d(0);
	T rhs_p(0);
	T fact(0);

	for (i64 iter = 0; iter <= max_iter; ++iter) {
		n_ext +=1;
		if (iter == max_iter) {
			break;
		}

		// compute primal residual
		
		qp::detail::global_primal_residual(
				primal_feasibility_lhs,
				primal_feasibility_eq_rhs_0,
				primal_feasibility_in_rhs_0,
				//primal_feasibility_eq_lhs,
				//primal_feasibility_in_lhs,
				primal_residual_eq_scaled,
				primal_residual_in_scaled_u,
				primal_residual_in_scaled_l,
				qp,
				//residual_scaled_tmp,
				qp_scaled,
				precond,
				xe,
				dim,
				n_eq,
				n_in
		);
		qp::detail::global_dual_residual(
			dual_feasibility_lhs,
			dual_feasibility_rhs_0,
			dual_feasibility_rhs_1,
        	dual_feasibility_rhs_3,
			dual_residual_scaled,
			dw,
			//residual_scaled_tmp,
			qp_scaled,
			precond,
			xe,
			VectorViewMut<T>{from_eigen,ye.to_eigen().topRows(n_eq)},
			VectorViewMut<T>{from_eigen,ye.to_eigen().tail(n_in)},
			dim,
			n_eq,
			n_in
		);

		std::cout << "---------------it : " << iter << " primal residual : " << primal_feasibility_lhs << " dual residual : " << dual_feasibility_lhs << std::endl;
		std::cout << " rho : " << rho << " mu_eq : " << mu_eq << " mu_in : " << mu_in <<std::endl;
		
		rhs_d = max2(  max2( primal_feasibility_eq_rhs_0, primal_feasibility_in_rhs_0),  max2( max2( primal_feasibility_rhs_1_eq, primal_feasibility_rhs_1_in_u  ), primal_feasibility_rhs_1_in_l )  );
  		rhs_p = max2(  max2(   dual_feasibility_rhs_3,dual_feasibility_rhs_0 ),max2(  dual_feasibility_rhs_1, dual_feasibility_rhs_2) );
		bool is_primal_feasible =
				primal_feasibility_lhs <=
				(eps_abs + eps_rel * rhs_p);

		bool is_dual_feasible =
				dual_feasibility_lhs <=
				(eps_abs + eps_rel * rhs_d );

		if (is_primal_feasible){

			if (is_dual_feasible){
				
				// POLISHING IFF IT HAS CONVERGED
				rhs.topRows(dim) = -dual_residual_scaled;
				rhs.middleRows(dim,n_eq) = -primal_residual_eq_scaled ;
				//rhs.tail(n_in) =  -(qp_scaled.C.to_eigen() * x_e - z_e.tail(n_in)); 
				{
				LDLT_MULTI_WORKSPACE_MEMORY(
							(test_,Init, Vec(n_in),LDLT_CACHELINE_BYTES, bool)
				);
				auto test = test_.to_eigen();
				isize j(0);
				for (isize i = 0 ; i = n_in ; ++i){
					test(i) = qp_scaled.u.to_eigen()(i) - ze(i) >= ye.to_eigen()(i) && ze(i)-qp_scaled.l.to_eigen()(i) >= -ye.to_eigen()(i) ;
					if( test(i) ) {
						ldl.delete_at(j+dim+n_eq);
					}else{
						rhs.topRows(dim) += qp_scaled.C.to_eigen().row(i) * ze(i);
						rhs(dim+n_eq+j) = -(qp_scaled.C.to_eigen().row(i).dot(xe.to_eigen())-ze(i));
						j+=1;
					}
				}
				LDLT_MULTI_WORKSPACE_MEMORY(
							(dw___,Init, Vec(dim+n_eq+j),LDLT_CACHELINE_BYTES, T),
							(err___,Init, Vec(dim+n_eq+j),LDLT_CACHELINE_BYTES, T),
							(rhs___,Init, Vec(dim+n_eq+j),LDLT_CACHELINE_BYTES, T)
				);
				dw.setZero();
				err.topRows(dim+n_eq+j).setZero();

				auto dw__ = dw___.to_eigen().eval();
				auto err__ = err___.to_eigen().eval();
				auto rhs__ = rhs___.to_eigen().eval();

				dw__ = dw.topRows(dim+n_eq+j);
				err__ = err.topRows(dim+n_eq+j);
				rhs__ = rhs.topRows(dim+n_eq+j);

				iterative_solve_with_permut_fact_osqp( 
						rhs__,
						dw__,
						err__,
						ldl,
						T(1e-5),
						isize(3),
						qp_scaled.as_const(),
						dim,
						n_eq,
						j,
						mu_eq,
						mu_in,
						rho
				);
				j = isize(0);
				for (isize i =0 ; i<n_in;++i){
					if (test(i)){
						dw(dim+n_eq+i) = dw__(dim+n_eq+j);
						j+=1;
					}
				}
				}
				tmp =  alpha * (ze.tail(n_in) + dw.tail(n_in)/mu_in) + (T(1)-alpha) * ze.tail(n_in) + ye.to_eigen().tail(n_in)/mu_in ; 
				z.tail(n_in) = tmp + ( qp_scaled.l.to_eigen().array() >= tmp.array() ).select(qp_scaled.l.to_eigen() - tmp, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)) - ( qp_scaled.u.to_eigen().array() <= tmp.array() ).select(tmp -qp_scaled.u.to_eigen() , Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)) ; 
				ye.to_eigen().topRows(n_eq).array() += ( mu_eq * ( alpha * (qp_scaled.b.to_eigen() + dw.middleRows(dim,n_eq) /mu_eq) + (T(1)-alpha)*ze.topRows(n_eq) -z.topRows(n_eq)   )   ).array() ;
				ye.to_eigen().tail(n_in).array() = mu_in * (tmp - z.tail(n_in));
				xe.to_eigen().array() = (alpha * dw.topRows(dim)  +  xe.to_eigen()).array() ; 
				
				// UNSCALE POLISHED SOLUTION
				{
				LDLT_DECL_SCOPE_TIMER("in solver", "unscale solution", T);
				precond.unscale_primal_in_place(xe); 
				precond.unscale_dual_in_place_eq(VectorViewMut<T>{from_eigen,ye.to_eigen().topRows(n_eq)});
				precond.unscale_dual_in_place_in(VectorViewMut<T>{from_eigen,ye.to_eigen().tail(n_in)});
				}

				return {n_ext, n_mu_updates,n_tot};
			}
		}

		// mu_update

		if(iter> 1){
			fact = std::sqrt(  (primal_feasibility_lhs * rhs_d) / (dual_feasibility_lhs * rhs_p + machine_eps)  ) ; 
			if (fact > T(5) || fact < T(0.2)){
				T mu_in_new = std::min(mu_in * fact,T(1e6)) ; 
				T mu_eq_new =  std::min(mu_eq * fact,T(1e6)) ; 
				if (mu_in_new != T(1.e6) || mu_eq_new != T(1e6)){
					qp::detail::mu_update(
						mu_eq,
						mu_eq_new,
						mu_in,
						mu_in_new,
						dim,
						n_eq,
						n_in,
						ldl,
						qp_scaled.as_const(),
						rho
					);
				}
				
				mu_in = mu_in_new;
				mu_eq = mu_eq_new;
				n_mu_updates +=1;
			}
		}

		// NEWTON STEP

		qp::detail::newton_step_osqp(
			qp_scaled.as_const(),
			xe.as_const(),
			ye.as_const(),
			VectorView<T>{from_eigen,ze},
			VectorViewMut<T>{from_eigen,dw},
			VectorViewMut<T>{from_eigen,err},
			mu_eq,
			mu_in,
			rho,
			dim,
			n_eq,
			n_in,
			ldl,
			rhs,
			dual_residual_scaled,
			primal_residual_eq_scaled,
			primal_residual_in_scaled_l
		) ;
		// ITERATES UPDATES according to OSQP algorithm 1 page 9 using 
		/*
		* x_tild = xe.to_eigen() + dw.topRows(dim)
		* z_eq_tild = ye.to_eigen().topRows(n_eq) + dw.middleRows(dim,n_eq)
		* z_in_tild = ye.to_eigen().tail(n_in) + dw.tail(n_in)
		*
		* hence after simplifying calculus one gets :
		* 
		* z_pot_eq = qp_scaled.b.to_eigen() + dw.middleRows(dim,n_eq) /mu_eq
		* z_pot_in = ze.tail(n_in) + dw.tail(n_in)/mu_in
		* 
		* and z defined as follows 
		*
		for (isize i = 0;i<n_in;i++){
			T tmp = alpha * z_pot_in(i) + (T(1)-alpha) * ze(n_eq+i) + ye.to_eigen()(n_eq+i)/mu_in ; 
			if (tmp <= qp_scaled.u.to_eigen()(i) && tmp >= qp_scaled.l.to_eigen()(i)){
				z(n_eq+i) = tmp ;
			} else if (tmp > qp_scaled.u.to_eigen()(i)){
				
				z(n_eq+i) = qp_scaled.u.to_eigen()(i) ;
			}else{
				
				z(n_eq+i) = qp_scaled.l.to_eigen()(i) ; 
			}
		}
		*/

		tmp =  alpha * (ze.tail(n_in) + dw.tail(n_in)/mu_in) + (T(1)-alpha) * ze.tail(n_in) + ye.to_eigen().tail(n_in)/mu_in ; 
		z.tail(n_in) = tmp + ( qp_scaled.l.to_eigen().array() >= tmp.array() ).select(qp_scaled.l.to_eigen() - tmp, Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)) - ( qp_scaled.u.to_eigen().array() <= tmp.array() ).select(tmp -qp_scaled.u.to_eigen() , Eigen::Matrix<T,Eigen::Dynamic,1>::Zero(n_in)) ; 
		ye.to_eigen().topRows(n_eq).array() += ( mu_eq * ( alpha * (qp_scaled.b.to_eigen() + dw.middleRows(dim,n_eq) /mu_eq) + (T(1)-alpha)*ze.topRows(n_eq) -z.topRows(n_eq)   )   ).array() ;
		//ye.to_eigen().tail(n_in).array() += ( mu_in * (alpha * z_pot_in +(T(1)-alpha)*ze.tail(n_in)-z.tail(n_in))).array() ;
		ye.to_eigen().tail(n_in).array() = mu_in * (tmp - z.tail(n_in));
		xe.to_eigen().array() = (alpha * dw.topRows(dim)  +  xe.to_eigen()).array() ; 
		//ye.to_eigen() = y.eval(); 
		ze = z ; 

	}
	
	return {max_iter, n_mu_updates, n_tot};
}


} // namespace detail

} // namespace qp

#endif /* end of include guard INRIA_LDLT_EQ_SOLVER_HPP_HDWGZKCLS */
