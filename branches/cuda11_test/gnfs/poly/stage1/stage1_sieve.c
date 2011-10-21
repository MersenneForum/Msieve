/*--------------------------------------------------------------------
This source distribution is placed in the public domain by its author,
Jason Papadopoulos. You may use it for any purpose, free of charge,
without having to notify anyone. I disclaim any responsibility for any
errors.

Optionally, please be nice and tell me if you find this source to be
useful. Again optionally, if you add to the functionality present here
please consider making those additions public too, so that others may 
benefit from your work.	

$Id$
--------------------------------------------------------------------*/

#include <stage1.h>

/* wrapper for the collision search. Depending on build
   options, the collision search may be performed either
   via a hashtable method on the CPU or via a massively
   parallel all-against-all method on nvidia GPU cards. */

/*------------------------------------------------------------------------*/
void
handle_collision(poly_search_t *poly, uint64 p, uint32 special_q,
		uint64 special_q_root, int64 res)
{
	/* the proposed rational coefficient is p*special_q;
	   p and special_q must be coprime. The 'trivial
	   special q' has special_q = 1 and special_q_root = 0 */

	uint64_2gmp(p, poly->p);
	mpz_gcd_ui(poly->tmp1, poly->p, (unsigned long)special_q);
	if (mpz_cmp_ui(poly->tmp1, (unsigned long)1))
		return;

	mpz_mul_ui(poly->p, poly->p, (unsigned long)special_q);

	/* the corresponding correction to trans_m0 is 
	   special_q_root + res * special_q^2, and can be
	   positive or negative */

	uint64_2gmp(special_q_root, poly->tmp1);
	int64_2gmp(res, poly->tmp2);
	mpz_set_ui(poly->tmp3, (unsigned long)special_q);

	mpz_mul(poly->tmp3, poly->tmp3, poly->tmp3);
	mpz_addmul(poly->tmp1, poly->tmp2, poly->tmp3);
	mpz_add(poly->m, poly->trans_m0, poly->tmp1);

	/* a lot can go wrong before this function is called!
	   Check that Kleinjung's modular condition is satisfied */

	mpz_pow_ui(poly->tmp1, poly->m, (mp_limb_t)poly->degree);
	mpz_mul(poly->tmp2, poly->p, poly->p);
	mpz_sub(poly->tmp1, poly->trans_N, poly->tmp1);
	mpz_tdiv_r(poly->tmp3, poly->tmp1, poly->tmp2);
	if (mpz_cmp_ui(poly->tmp3, (mp_limb_t)0)) {
		gmp_printf("poly %Zd %Zd %Zd\n",
				poly->high_coeff, poly->p, poly->m);
		printf("crap\n");
		return;
	}

	/* the pair works, now translate the computed m back
	   to the original polynomial. We have

	   computed_m = degree * high_coeff * real_m +
	   			(second_highest_coeff) * p

	   and need to solve for real_m and second_highest_coeff.
	   Per the CADO code: reducing the above modulo
	   degree*high_coeff causes the first term on the right
	   to disappear, so second_highest_coeff can be found
	   modulo degree*high_coeff and real_m then follows */

	mpz_mul_ui(poly->tmp1, poly->high_coeff, (mp_limb_t)poly->degree);
	mpz_tdiv_r(poly->tmp2, poly->m, poly->tmp1);
	mpz_invert(poly->tmp3, poly->p, poly->tmp1);
	mpz_mul(poly->tmp2, poly->tmp3, poly->tmp2);
	mpz_tdiv_r(poly->tmp2, poly->tmp2, poly->tmp1);

	/* make second_highest_coeff as small as possible in
	   absolute value */

	mpz_tdiv_q_2exp(poly->tmp3, poly->tmp1, 1);
	if (mpz_cmp(poly->tmp2, poly->tmp3) > 0) {
		mpz_sub(poly->tmp2, poly->tmp2, poly->tmp1);
	}

	/* solve for real_m */
	mpz_submul(poly->m, poly->tmp2, poly->p);
	mpz_tdiv_q(poly->m, poly->m, poly->tmp1);

	gmp_printf("poly %Zd %Zd %Zd\n",
			poly->high_coeff, poly->p, poly->m);

	poly->callback(poly->high_coeff, poly->p, poly->m,
			poly->coeff_max, poly->callback_data);
}

/*------------------------------------------------------------------------*/
double
sieve_lattice(msieve_obj *obj, poly_search_t *poly, double deadline)
{
	lattice_fb_t L;

	L.poly = poly;
	L.deadline = deadline;

#ifdef HAVE_CUDA
	if (poly->degree <= 4)
		sieve_lattice_gpu_2prog(obj, &L);
	else
		sieve_lattice_gpu_3prog(obj, &L);
#else
	sieve_lattice_cpu(obj, &L);
#endif

	return deadline - L.deadline;
}
