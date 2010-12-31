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

#define P_PRIME_LIMIT 0xfffff000

/*------------------------------------------------------------------------*/
static uint32
lift_root_32(uint32 n, uint32 r, uint32 old_power, 
		uint32 p, uint32 d)
{
	uint32 q;
	uint32 p2 = old_power * p;
	uint64 rsave = r;

	q = mp_modsub_1(n % p2, mp_expo_1(r, d, p2), p2) / old_power;
	r = mp_modmul_1(d, mp_expo_1(r % p, d - 1, p), p);
	r = mp_modmul_1(q, mp_modinv_1(r, p), p);
	return rsave + old_power * r;
}

/*------------------------------------------------------------------------*/
void
sieve_fb_free(sieve_fb_t *s)
{
	free_prime_sieve(&s->p_prime);

	free(s->aprog_data.aprogs);

	mpz_clear(s->p);
	mpz_clear(s->p2);
	mpz_clear(s->m0);
	mpz_clear(s->nmodp2);
	mpz_clear(s->tmp1);
	mpz_clear(s->tmp2);
	mpz_clear(s->gmp_root);
}

/*------------------------------------------------------------------------*/
static uint32
get_prime_roots(poly_search_t *poly, uint32 p, uint32 *roots)
{
	mp_poly_t tmp_poly;
	mp_t *low_coeff;
	uint32 high_coeff;
	uint32 degree = poly->degree;

	memset(&tmp_poly, 0, sizeof(mp_poly_t));
	tmp_poly.degree = degree;
	tmp_poly.coeff[degree].num.nwords = 1;
	tmp_poly.coeff[degree].num.val[0] = p - 1;

	if (mp_gcd_1(p, (uint32)mpz_tdiv_ui(
			poly->high_coeff, (mp_limb_t)p)) > 1)
		return 0;

	low_coeff = &tmp_poly.coeff[0].num;
	low_coeff->val[0] = mpz_tdiv_ui(poly->trans_N, (mp_limb_t)p);
	if (low_coeff->val[0])
		low_coeff->nwords = 1;

	return poly_get_zeros(roots, &tmp_poly, 
				p, &high_coeff, 0);
}

/*------------------------------------------------------------------------*/
static void
sieve_add_aprog(sieve_fb_t *s, poly_search_t *poly, uint32 p, 
		uint32 fb_roots_min, uint32 fb_roots_max)
{
	uint32 i, j, nmodp, num_roots;
	uint32 degree = poly->degree;
	uint32 power, power_limit;
	uint32 roots[MAX_POLYSELECT_DEGREE];
	aprog_list_t *list = &s->aprog_data;
	aprog_t *a;

	if (list->num_aprogs == list->num_aprogs_alloc) {
		list->num_aprogs_alloc *= 2;
		list->aprogs = (aprog_t *)xrealloc(list->aprogs,
						list->num_aprogs_alloc *
						sizeof(aprog_t));
	}

	a = list->aprogs + list->num_aprogs;
	a->p = p;
	num_roots = get_prime_roots(poly, p, roots);

	if (num_roots == 0 ||
	    num_roots < fb_roots_min ||
	    num_roots > fb_roots_max)
		return;

	list->num_aprogs++;
	a->num_roots = num_roots;
	for (i = 0; i < num_roots; i++)
		a->roots[0][i] = roots[i];

	power = p;
	a->power[0] = power;
	power_limit = (uint32)(-1) / p;
	for (i = 1; i < MAX_POWER && power < power_limit; i++) {

		power *= p;
		a->power[i] = power;

		nmodp = mpz_tdiv_ui(poly->trans_N, (mp_limb_t)power);

		for (j = 0; j < num_roots; j++)
			a->roots[i][j] = lift_root_32(nmodp, 
						a->roots[i - 1][j],
						a->power[i - 1], p, degree);
	}
	a->max_power = i;
}

/*------------------------------------------------------------------------*/
void
sieve_fb_init(sieve_fb_t *s, poly_search_t *poly,
		uint32 factor_min, uint32 factor_max,
		uint32 fb_roots_min, uint32 fb_roots_max,
		uint32 fb_only)
{
	uint32 i, p;
	aprog_list_t *aprog = &s->aprog_data;

	memset(s, 0, sizeof(sieve_fb_t));
	s->degree = poly->degree;
	s->fb_only = fb_only;

	mpz_init(s->p);
	mpz_init(s->p2);
	mpz_init(s->m0);
	mpz_init(s->nmodp2);
	mpz_init(s->tmp1);
	mpz_init(s->tmp2);
	mpz_init(s->gmp_root);

	if (factor_max <= factor_min)
		return;

	aprog->num_aprogs_alloc = 500;
	aprog->aprogs = (aprog_t *)xmalloc(aprog->num_aprogs_alloc *
						sizeof(aprog_t));

	for (i = p = 0; i < PRECOMPUTED_NUM_PRIMES; i++) {
		p += prime_delta[i];

		if (p <= factor_min)
			continue;
		else if (p >= factor_max)
			break;

		sieve_add_aprog(s, poly, p, fb_roots_min, fb_roots_max);
	}
}

/*------------------------------------------------------------------------*/
void 
sieve_fb_reset(sieve_fb_t *s, uint32 p_min, uint32 p_max,
		uint32 num_roots_min, uint32 num_roots_max)
{
	uint32 i;
	aprog_t *aprogs = s->aprog_data.aprogs;
	uint32 num_aprogs = s->aprog_data.num_aprogs;

	free_prime_sieve(&s->p_prime);

	if (num_roots_max > MAX_ROOTS) {
		printf("error: num_roots_max exceeds %d\n", MAX_ROOTS);
		exit(-1);
	}

	s->p_min = p_min;
	s->p_max = p_max;
	s->num_roots_min = num_roots_min;
	s->num_roots_max = num_roots_max;
	s->avail_algos = 0;

	if (s->fb_only == 0 &&
	    p_min < P_PRIME_LIMIT &&
	    s->degree >= num_roots_min) {
		uint32 last_p;

		if (num_aprogs == 0)
			last_p = 0;
		else
			last_p = aprogs[num_aprogs - 1].p;

		if (p_max > last_p) {
			s->avail_algos |= ALGO_PRIME;

			init_prime_sieve(&s->p_prime, MAX(p_min, last_p + 1),
					 MIN(p_max, P_PRIME_LIMIT));
		}
	}

	if (num_aprogs > 0) {
		p_enum_t *p_enum = &s->p_enum; 

		s->avail_algos |= ALGO_ENUM;

		p_enum->factors[0] = 0;
		p_enum->num_factors = 0;
		p_enum->powers[0] = 0;
		p_enum->cofactors[0] = 1;
		p_enum->cofactor_roots[0] = 1;

		for (i = 0; i < num_aprogs; i++) {
			aprog_t *a = aprogs + i;

			a->cofactor_max = p_max / a->p;
			a->cofactor_roots_max = num_roots_max / a->num_roots;
		}
	}
}

/*------------------------------------------------------------------------*/
static void
lift_roots(sieve_fb_t *s, poly_search_t *poly, uint32 p, uint32 num_roots)
{
	uint32 i;
	unsigned long degree = s->degree;

	mpz_set_ui(s->p, (unsigned long)p);
	mpz_mul(s->p2, s->p, s->p);
	mpz_tdiv_r(s->nmodp2, poly->trans_N, s->p2);
	mpz_sub(s->tmp1, poly->trans_m0, poly->mp_sieve_size);
	mpz_tdiv_r(s->m0, s->tmp1, s->p2);

	for (i = 0; i < num_roots; i++) {

		uint64_2gmp(s->roots[i], s->gmp_root);

		mpz_powm_ui(s->tmp1, s->gmp_root, degree, s->p2);
		mpz_sub(s->tmp1, s->nmodp2, s->tmp1);
		if (mpz_cmp_ui(s->tmp1, (mp_limb_t)0) < 0)
			mpz_add(s->tmp1, s->tmp1, s->p2);
		mpz_tdiv_q(s->tmp1, s->tmp1, s->p);

		mpz_powm_ui(s->tmp2, s->gmp_root, degree-1, s->p);
		mpz_mul_ui(s->tmp2, s->tmp2, degree);
		mpz_invert(s->tmp2, s->tmp2, s->p);

		mpz_mul(s->tmp1, s->tmp1, s->tmp2);
		mpz_tdiv_r(s->tmp1, s->tmp1, s->p);
		mpz_addmul(s->gmp_root, s->tmp1, s->p);
		mpz_sub(s->gmp_root, s->gmp_root, s->m0);
		if (mpz_cmp_ui(s->gmp_root, (unsigned long)0) < 0)
			mpz_add(s->gmp_root, s->gmp_root, s->p2);

		s->roots[i] = gmp2uint64(s->gmp_root);
	}
}

/*------------------------------------------------------------------------*/
static uint32
get_composite_roots(sieve_fb_t *s, uint32 p)
{
	uint32 i, j, i0, i1, i2, i3, i4, i5, i6;
	aprog_t *aprogs = s->aprog_data.aprogs;
	p_enum_t *p_enum = &s->p_enum;
	uint32 *factors = p_enum->factors;
	uint32 *powers = p_enum->powers;
	uint32 num_factors = p_enum->num_factors;

	uint32 num_roots[MAX_P_FACTORS];
	uint32 prod[MAX_P_FACTORS];
	uint32 roots[MAX_P_FACTORS][MAX_POLYSELECT_DEGREE];
	uint64 accum[MAX_P_FACTORS + 1];

	for (i = 0; i < num_factors; i++) {
		aprog_t *a = aprogs + factors[i];

		num_roots[i] = a->num_roots;
		for (j = 0; j < num_roots[i]; j++)
			roots[i][j] = a->roots[powers[i]][j];
	}

	if (num_factors == 1) {
		for (i = 0; i < num_roots[0]; i++)
			s->roots[i] = roots[0][i];

		return num_roots[0];
	}

	for (i = 0; i < num_factors; i++) {
		aprog_t *a = aprogs + factors[i];

		prod[i] = p / a->power[powers[i]];
		prod[i] *= mp_modinv_1(prod[i] % a->power[powers[i]],
					a->power[powers[i]]);
	}
	accum[i] = 0;

#if MAX_P_FACTORS > 7
#error "MAX_P_FACTORS exceeds 7"
#endif
	i0 = i1 = i2 = i3 = i4 = i5 = i6 = i = 0;
	switch (num_factors) {
	case 7:
		for (i6 = num_roots[6] - 1; (int32)i6 >= 0; i6--) {
			accum[6] = accum[7] + (uint64)prod[6] * roots[6][i6];
	case 6:
		for (i5 = num_roots[5] - 1; (int32)i5 >= 0; i5--) {
			accum[5] = accum[6] + (uint64)prod[5] * roots[5][i5];
	case 5:
		for (i4 = num_roots[4] - 1; (int32)i4 >= 0; i4--) {
			accum[4] = accum[5] + (uint64)prod[4] * roots[4][i4];
	case 4:
		for (i3 = num_roots[3] - 1; (int32)i3 >= 0; i3--) {
			accum[3] = accum[4] + (uint64)prod[3] * roots[3][i3];
	case 3:
		for (i2 = num_roots[2] - 1; (int32)i2 >= 0; i2--) {
			accum[2] = accum[3] + (uint64)prod[2] * roots[2][i2];
	case 2:
		for (i1 = num_roots[1] - 1; (int32)i1 >= 0; i1--) {
			accum[1] = accum[2] + (uint64)prod[1] * roots[1][i1];

		for (i0 = num_roots[0] - 1; (int32)i0 >= 0; i0--) {
			accum[0] = accum[1] + (uint64)prod[0] * roots[0][i0];
			s->roots[i++] = accum[0] % p;
		}}}}}}}
	}

	return i;
}

/*------------------------------------------------------------------------*/
static uint32
get_next_enum(sieve_fb_t *s)
{
	p_enum_t *p_enum = &s->p_enum;
	uint32 *factors = p_enum->factors;
	uint32 *powers = p_enum->powers;
	uint32 *cofactors = p_enum->cofactors;
	uint32 *cofac_roots = p_enum->cofactor_roots;

	while (1) {

		uint32 i = p_enum->num_factors;
		uint32 power_up = (i && factors[i] == factors[i - 1]);
		uint32 num_roots = cofac_roots[i];
		aprog_t *a = NULL;

		if (factors[i] < s->aprog_data.num_aprogs)
			a = s->aprog_data.aprogs + factors[i];

		if (a != NULL && cofactors[i] <= a->cofactor_max
		    && !(power_up && ++powers[i - 1] >= a->max_power)
		    && (power_up || (cofac_roots[i] <= a->cofactor_roots_max
				     && i < MAX_P_FACTORS))) {

			uint32 p = cofactors[i] * a->p;

			if (!power_up) {
				p_enum->num_factors = ++i;
				num_roots *= a->num_roots;
				cofac_roots[i] = num_roots;
			}

			cofactors[i] = p;
			factors[i] = 0;
			powers[i] = 0;

			if (p >= s->p_min && num_roots >= s->num_roots_min)
				return p;
		}
		else if (i) {

			while (--i) {
				if (factors[i] != factors[i - 1])
					break;
			}

			factors[i]++;
			powers[i] = 0;
			p_enum->num_factors = i;
		}
		else {
			return P_SEARCH_DONE;
		}
	}
}

/*------------------------------------------------------------------------*/
uint32
sieve_fb_next(sieve_fb_t *s, poly_search_t *poly,
		root_callback callback, void *extra)
{
	uint32 i, p, num_roots;

	while (1) {
		if (s->avail_algos & ALGO_ENUM) {

			p = get_next_enum(s);

			if (p == P_SEARCH_DONE) {
				s->avail_algos &= ~ALGO_ENUM;
				continue;
			}

			num_roots = get_composite_roots(s, p);
		}
		else if (s->avail_algos & ALGO_PRIME) {

			uint32 roots[MAX_POLYSELECT_DEGREE];

			p = get_next_prime(&s->p_prime);

			if (p >= s->p_max || p >= P_PRIME_LIMIT) {
				s->avail_algos &= ~ALGO_PRIME;
				continue;
			}

			num_roots = get_prime_roots(poly, p, roots);

			if (num_roots == 0 ||
			    num_roots < s->num_roots_min ||
			    num_roots > s->num_roots_max)
				continue;

			for (i = 0; i < num_roots; i++)
				s->roots[i] = roots[i];
		}
		else {
			return P_SEARCH_DONE;
		}

		lift_roots(s, poly, p, num_roots);
		callback(p, num_roots, s->roots, extra);

		return p;
	}
}
