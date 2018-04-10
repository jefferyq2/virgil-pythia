/**
 * Copyright (C) 2015-2018 Virgil Security Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     (1) Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     (2) Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *     (3) Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Lead Maintainer: Virgil Security Inc. <support@virgilsecurity.com>
 */

#include <stdint.h>
#include "pythia_c.h"
#include "pythia_init.h"
#include "pythia_conf.h"
#include "pythia_init_c.h"

static bn_t g1_ord;
static g1_t g1_gen;
static bn_t gt_ord;
static gt_t gt_gen;

int pythia_init(const pythia_init_args_t *init_args) {
    if (core_get())
        return 0;

    if (core_init() != STS_OK)
        return -1;

    pythia_err_init();

#if RELIC_USE_EXT_RNG
    if (!init_args || !init_args->callback)
        return -1;

    rand_seed(init_args->callback, init_args->args);
#endif // RELIC_USE_EXT_RNG

    if (ep_param_set_any_pairf() != STS_OK)
        return -1;

    bn_null(g1_ord);
    g1_null(g1_gen);
    bn_null(gt_ord);
    gt_null(gt_gen);

    TRY {
        bn_new(g1_ord);
        g1_get_ord(g1_ord);

        g1_new(g1_gen);
        g1_get_gen(g1_gen);

        bn_new(gt_ord);
        gt_get_ord(gt_ord);

        gt_new(gt_gen);
        gt_get_gen(gt_gen);
    }
    CATCH_ANY {
        gt_free(gt_gen);
        bn_free(gt_ord);
        g1_free(g1_gen);
        bn_free(g1_ord);

        return -1;
    }

    return 0;
}

void pythia_deinit(void) {
    core_clean();
}

void pythia_err_init(void) {
    err_core_reset_default();
}

static void random_bn_mod(bn_t r, bn_t max) {
    if (!max) {
        bn_rand(r, BN_POS, 256);
    }
    else {
        bn_rand_mod(r, max);
    }
}

static void hashG1(g1_t g1, const uint8_t *msg, size_t msg_size) {
    g1_map(g1, msg, (int)msg_size);
}

static void hashG2(g2_t g2, const uint8_t *msg, size_t msg_size) {
    g2_map(g2, msg, (int)msg_size);
}

void pythia_blind(const uint8_t *m, size_t m_size, g1_t x, bn_t rInv) {
    bn_t r; bn_null(r);

    bn_t gcd, bn_one;
    bn_null(gcd); bn_null(bn_one);

    g1_t g1; g1_null(g1);

    TRY {
        bn_new(r);
        bn_new(bn_one);
        bn_set_bit(bn_one, 1, 1);
        bn_new(gcd);
        do {
            random_bn_mod(r, NULL);
            bn_gcd_ext(gcd, rInv, NULL, r, g1_ord);
        } while (!bn_cmp(gcd, bn_one));

        g1_new(g1);
        hashG1(g1, m, m_size);

        g1_mul(x, g1, r);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        g1_free(g1);

        bn_free(gcd);
        bn_free(bn_one);
        bn_free(r);
    }
}

static void compute_kw(bn_t kw, const uint8_t *w, size_t w_size,
                       const uint8_t *msk, size_t msk_size,
                       const uint8_t *s, size_t s_size) {
    uint8_t mac[MD_LEN];
    uint8_t *zw = NULL;

    bn_t b; bn_null(b);

    TRY {
        zw = calloc(s_size + w_size, sizeof(uint8_t));
        memcpy(zw, s, s_size);
        memcpy(zw + s_size, w, w_size);

        md_hmac(mac, zw, (int)(s_size + w_size), msk, (int)msk_size);

        bn_new(b);
        bn_read_bin(b, mac, MD_LEN);

        bn_mod(kw, b, gt_ord);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        bn_free(b);
        free(zw);
    }
}

void pythia_eval(g1_t x, const uint8_t *w, size_t w_size,
                 const uint8_t *t, size_t t_size, const uint8_t *msk, size_t msk_size,
                 const uint8_t *s, size_t s_size, gt_t y, bn_t kw, g2_t tTilde) {
    g1_t xKw; g1_null(xKw);

    TRY {
        compute_kw(kw, w, w_size, msk, msk_size, s, s_size);

        hashG2(tTilde, t, t_size);

        g1_new(xKw);
        g1_mul(xKw, x, kw);

        pc_map(y, xKw, tTilde);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        g1_free(xKw);
    }
}

static void gt_pow(gt_t res, gt_t a, bn_t exp) {
    bn_t e; bn_null(e);

    TRY {
        bn_new(e);
        bn_mod(e, exp, gt_ord);

        gt_exp(res, a, e);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        bn_free(e);
    }
}

void pythia_deblind(gt_t y, bn_t rInv, gt_t u) {
    TRY {
        gt_pow(u, y, rInv);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {}
}

static void hashZ(bn_t hash, const uint8_t* const * args, size_t args_size, const size_t* args_sizes) {
    const uint8_t tag_msg[31] = "TAG_RELIC_HASH_ZMESSAGE_HASH_Z";
    uint8_t *c = NULL;

    TRY {
        size_t total_size = 0;
        for (size_t i = 0; i < args_size; i++)
            total_size += args_sizes[i];

        c = calloc((size_t)total_size, sizeof(uint8_t));

        uint8_t *p = c;
        for (size_t i = 0; i < args_size; i++) {
            memcpy(p, args[i], args_sizes[i]);
            p += args_sizes[i];
        }

        uint8_t mac[MD_LEN];
        md_hmac(mac, c, (int)total_size, tag_msg, 31);

        bn_read_bin(hash, mac, MD_LEN_SH256); // We need only 256 bits from that number
    }
    CATCH_ANY {
            THROW(ERR_CAUGHT);
    }
    FINALLY {
        free(c);
    }
}

static void scalar_mul_g1(g1_t r, const g1_t p, bn_t a, bn_t n) {
    bn_t mod; bn_null(mod);

    TRY {
        bn_new(mod);
        bn_mod(mod, a, n);

        g1_mul(r, p, mod);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        bn_free(mod);
    }
}

static void serialize_g1(uint8_t *r, size_t size, const g1_t x) {
    g1_write_bin(r, (int)size, x, 1);
}

static void serialize_gt(uint8_t *r, size_t size, gt_t x) {
    gt_write_bin(r, (int)size, x, 1);
}

void pythia_prove(gt_t y, g1_t x, g2_t tTilde, bn_t kw,
                  g1_t pi_p, bn_t pi_c, bn_t pi_u) {
    gt_t beta; gt_null(beta);
    bn_t v; bn_null(v);
    g1_t t1; g1_null(t1);
    gt_t t2; gt_null(t2);

    uint8_t *q_bin = NULL, *p_bin = NULL, *beta_bin = NULL, *y_bin = NULL, *t1_bin = NULL, *t2_bin = NULL;

    bn_t cpkw; bn_null(cpkw);
    bn_t vscpkw; bn_null(vscpkw);

    TRY {
        gt_new(beta);
        pc_map(beta, x, tTilde);

        scalar_mul_g1(pi_p, g1_gen, kw, g1_ord);

        bn_new(v);

        random_bn_mod(v, gt_ord);

        g1_new(t1);
        scalar_mul_g1(t1, g1_gen, v, g1_ord);

        gt_new(t2);
        gt_pow(t2, beta, v);

        size_t q_bin_size = (size_t)g1_size_bin(g1_gen, 1);
        q_bin = calloc((size_t) q_bin_size, sizeof(uint8_t));
        serialize_g1(q_bin, q_bin_size, g1_gen);

        size_t p_bin_size = (size_t)g1_size_bin(pi_p, 1);
        p_bin = calloc((size_t) p_bin_size, sizeof(uint8_t));
        serialize_g1(p_bin, p_bin_size, pi_p);

        size_t beta_bin_size = (size_t)gt_size_bin(beta, 1);
        beta_bin = calloc((size_t) beta_bin_size, sizeof(uint8_t));
        serialize_gt(beta_bin, beta_bin_size, beta);

        size_t y_bin_size = (size_t)gt_size_bin(y, 1);
        y_bin = calloc((size_t) y_bin_size, sizeof(uint8_t));
        serialize_gt(y_bin, y_bin_size, y);

        size_t t1_bin_size = (size_t)g1_size_bin(t1, 1);
        t1_bin = calloc((size_t) t1_bin_size, sizeof(uint8_t));
        serialize_g1(t1_bin, t1_bin_size, t1);

        size_t t2_bin_size = (size_t)gt_size_bin(t2, 1);
        t2_bin = calloc((size_t) t2_bin_size, sizeof(uint8_t));
        serialize_gt(t2_bin, t2_bin_size, t2);

        const uint8_t *const args[6] = {q_bin, p_bin, beta_bin, y_bin, t1_bin, t2_bin};
        const size_t args_sizes[6] = {q_bin_size, p_bin_size, beta_bin_size, y_bin_size, t1_bin_size, t2_bin_size};
        hashZ(pi_c, args, 6, args_sizes);

        bn_new(cpkw);
        bn_mul(cpkw, pi_c, kw);

        bn_new(vscpkw);
        bn_sub(vscpkw, v, cpkw);

        bn_mod(pi_u, vscpkw, gt_ord);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        bn_free(vscpkw);
        bn_free(cpkw);

        free(t2_bin);
        free(t1_bin);
        free(y_bin);
        free(beta_bin);
        free(p_bin);
        free(q_bin);

        gt_free(t2);
        g1_free(t1);

        bn_free(v);

        gt_free(beta);
    }
}

void pythia_verify(gt_t y, g1_t x, const uint8_t *t, size_t t_size,
                   g1_t pi_p, bn_t pi_c, bn_t pi_u, int *verified) {
    g2_t tTilde; g2_null(tTilde);
    gt_t beta; gt_null(beta);
    g1_t pc; g1_null(pc);
    g1_t qu; g1_null(qu);
    g1_t t1; g1_null(t1);
    gt_t yc; gt_null(yc);
    gt_t betau; gt_null(betau);
    gt_t t2; gt_null(t2);

    uint8_t *q_bin = NULL, *p_bin = NULL, *beta_bin = NULL, *y_bin = NULL, *t1_bin = NULL, *t2_bin = NULL;

    bn_t cPrime;

    TRY {
        g2_new(tTilde);
        hashG2(tTilde, t, t_size);

        gt_new(beta);
        pc_map(beta, x, tTilde);

        g1_new(pc);

        scalar_mul_g1(pc, pi_p, pi_c, g1_ord);

        g1_new(qu);
        scalar_mul_g1(qu, g1_gen, pi_u, g1_ord);

        g1_new(t1);
        g1_add(t1, qu, pc);

        gt_new(yc);
        gt_pow(yc, y, pi_c);

        gt_new(betau);
        gt_pow(betau, beta, pi_u);

        gt_new(t2);
        gt_mul(t2, betau, yc);

        size_t q_bin_size = (size_t)g1_size_bin(g1_gen, 1);
        q_bin = calloc((size_t) q_bin_size, sizeof(uint8_t));
        serialize_g1(q_bin, q_bin_size, g1_gen);

        size_t p_bin_size = (size_t)g1_size_bin(pi_p, 1);
        p_bin = calloc((size_t) p_bin_size, sizeof(uint8_t));
        serialize_g1(p_bin, p_bin_size, pi_p);

        size_t beta_bin_size = (size_t)gt_size_bin(beta, 1);
        beta_bin = calloc((size_t) beta_bin_size, sizeof(uint8_t));
        serialize_gt(beta_bin, beta_bin_size, beta);

        size_t y_bin_size = (size_t)gt_size_bin(y, 1);
        y_bin = calloc((size_t) y_bin_size, sizeof(uint8_t));
        serialize_gt(y_bin, y_bin_size, y);

        size_t t1_bin_size = (size_t)g1_size_bin(t1, 1);
        t1_bin = calloc((size_t) t1_bin_size, sizeof(uint8_t));
        serialize_g1(t1_bin, t1_bin_size, t1);

        size_t t2_bin_size = (size_t)gt_size_bin(t2, 1);
        t2_bin = calloc((size_t) t2_bin_size, sizeof(uint8_t));
        serialize_gt(t2_bin, t2_bin_size, t2);

        const uint8_t *const args[6] = {q_bin, p_bin, beta_bin, y_bin, t1_bin, t2_bin};
        const size_t args_sizes[6] = {q_bin_size, p_bin_size, beta_bin_size, y_bin_size, t1_bin_size,
                                   t2_bin_size};

        bn_new(cPrime);
        hashZ(cPrime, args, 6, args_sizes);

        *verified = bn_cmp(cPrime, pi_c) == CMP_EQ;
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        bn_free(cPrime)

        free(q_bin);
        free(p_bin);
        free(beta_bin);
        free(y_bin);
        free(t1_bin);
        free(t2_bin);

        gt_free(t2);
        gt_free(betau);
        gt_free(yc);
        g1_free(t1);
        g1_free(qu);
        g1_free(pc);
        gt_free(beta);
        g2_free(tTilde);
    }
}

void get_delta(const uint8_t *w0, size_t w0_size, const uint8_t *msk0, size_t msk0_size,
               const uint8_t *s0, size_t s0_size,
               const uint8_t *w1, size_t w1_size, const uint8_t *msk1, size_t msk1_size,
               const uint8_t *s1, size_t s1_size, bn_t delta, g1_t pPrime) {
    bn_t kw1; bn_null(kw1);
    bn_t kw0; bn_null(kw0);
    bn_t kw0Inv; bn_null(kw0Inv);
    bn_t gcd; bn_null(gcd);
    bn_t kw1kw0Inv; bn_null(kw1kw0Inv);

    TRY {
        bn_new(kw1);
        compute_kw(kw1, w1, w1_size, msk1, msk1_size, s1, s1_size);

        bn_new(kw0);
        compute_kw(kw0, w0, w0_size, msk0, msk0_size, s0, s0_size);

        bn_new(kw0Inv);

        bn_new(gcd);

        bn_gcd_ext(gcd, kw0Inv, NULL, kw0, gt_ord);

        bn_new(kw1kw0Inv);
        bn_mul(kw1kw0Inv, kw1, kw0Inv);

        bn_mod(delta, kw1kw0Inv, gt_ord);

        g1_mul_gen(pPrime, kw1);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {
        bn_free(kw1kw0Inv);
        bn_free(gcd);
        bn_free(kw0Inv);
        bn_free(kw0);
        bn_free(kw1);
    }
}

void pythia_update_with_delta(gt_t u0, bn_t delta,
                              gt_t u1) {
    TRY {
        gt_pow(u1, u0, delta);
    }
    CATCH_ANY {
        THROW(ERR_CAUGHT);
    }
    FINALLY {}
}
