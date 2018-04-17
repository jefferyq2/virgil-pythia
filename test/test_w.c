/**
 * Copyright (C) 2015-2018 Virgil Security Inc.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <memory.h>
#include "pythia.h"
#include "unity.h"

static const char deblinded_hex[769] = "13273238e3119262f86d3213b8eb6b99c093ef48737dfcfae96210f7350e096cbc7e6b992e4e6f705ac3f0a915d1622c1644596408e3d16126ddfa9ce594e9f361b21ef9c82309e5714c09bcd7f7ec5c2666591134c645d45ed8c9703e718ee005fe4b97fc40f69b424728831d0a889cd39be04683dd380daa0df67c38279e3b9fe32f6c40780311f2dfbb6e89fc90ef15fb2c7958e387182dc7ef57f716fdd152a58ac1d3f0d19bfa2f789024333976c69fbe9e24b58d6cd8fa49c5f4d642b00f8e390c199f37f7b3125758ef284ae10fd9c2da7ea280550baccd55dadd70873a063bcfc9cac9079042af88a543a6cc09aaed6ba4954d6ee8ccc6e1145944328266616cd00f8a616f0e79e52ddd2ef970c8ba8f8ffce35505dc643c8e2b6e430a1474a6d043a4daf9b62af87c1d45ca994d23f908f7898a3f44ca7bb642122087ca819308b3d8afad17ca1f6148e8750870336ca68eb783c89b0dc9d92392f453c650e9f09232b9fcffd1c2cad24b14d2b4952b7f54552295ce0e854996913c";
static const uint8_t password[9] = "password";
static const uint8_t w[11] = "virgil.com";
static const uint8_t t[6] = "alice";
static const uint8_t msk[14] = "master secret";
static const uint8_t msk1[14] = "secret master";
static const uint8_t ssk[14] = "server secret";

void blind_eval_deblind(pythia_buf_t *deblinded_password) {
    pythia_buf_t blinded_password, blinding_secret, transformed_password,
            transformation_private_key, transformed_tweak,
            transformation_key_id_buf, tweak_buf, pythia_secret_buf,
            pythia_scope_secret_buf, password_buf;

    blinded_password.p = (uint8_t *)malloc(PYTHIA_G1_BUF_SIZE);
    blinded_password.allocated = PYTHIA_G1_BUF_SIZE;

    blinding_secret.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    blinding_secret.allocated = PYTHIA_BN_BUF_SIZE;

    transformed_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
    transformed_password.allocated = PYTHIA_GT_BUF_SIZE;

    transformation_private_key.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    transformation_private_key.allocated = PYTHIA_BN_BUF_SIZE;

    transformed_tweak.p = (uint8_t *)malloc(PYTHIA_G2_BUF_SIZE);
    transformed_tweak.allocated = PYTHIA_G2_BUF_SIZE;

    transformation_key_id_buf.p = (uint8_t *)w;
    transformation_key_id_buf.len = 10;

    tweak_buf.p = (uint8_t *)t;
    tweak_buf.len = 5;

    pythia_secret_buf.p = (uint8_t *)msk;
    pythia_secret_buf.len = 13;

    pythia_scope_secret_buf.p = (uint8_t *)ssk;
    pythia_scope_secret_buf.len = 13;

    password_buf.p = (uint8_t *)password;
    password_buf.len = 8;

    if (pythia_w_blind(&password_buf, &blinded_password, &blinding_secret))
        TEST_FAIL();

    if (pythia_w_transform(&blinded_password, &transformation_key_id_buf, &tweak_buf, &pythia_secret_buf,
                           &pythia_scope_secret_buf, &transformed_password, &transformation_private_key,
                           &transformed_tweak))
        TEST_FAIL();

    if (pythia_w_deblind(&transformed_password, &blinding_secret, deblinded_password))
        TEST_FAIL();

    free(blinded_password.p);
    free(blinding_secret.p);
    free(transformed_password.p);
    free(transformation_private_key.p);
    free(transformed_tweak.p);
}

void test1_DeblindStability() {
    TEST_ASSERT_EQUAL_INT(pythia_init(NULL), 0);

    uint8_t deblinded_bin[384];
    const char *pos = deblinded_hex;
    for (size_t count = 0; count < 384; count++) {
        sscanf(pos, "%2hhx", &deblinded_bin[count]);
        pos += 2;
    }

    const int iterations = 10;

    pythia_buf_t deblinded_password;

    for (int i = 0; i < iterations; i++) {
        deblinded_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
        deblinded_password.allocated = PYTHIA_GT_BUF_SIZE;

        blind_eval_deblind(&deblinded_password);

        TEST_ASSERT_EQUAL_MEMORY(deblinded_bin, deblinded_password.p, 384);

        free(deblinded_password.p);
        deblinded_password.allocated = 0;
    }

    pythia_deinit();
}

void test2_BlindEvalProveVerify() {
    TEST_ASSERT_EQUAL_INT(pythia_init(NULL), 0);

    pythia_buf_t blinded_password, blinding_secret, transformed_password,
            transformation_private_key, transformed_tweak,
            transformation_public_key, proof_value_c, proof_value_u,
            transformation_key_id_buf, tweak_buf, pythia_secret_buf,
            pythia_scope_secret_buf, password_buf;

    blinded_password.p = (uint8_t *)malloc(PYTHIA_G1_BUF_SIZE);
    blinded_password.allocated = PYTHIA_G1_BUF_SIZE;

    blinding_secret.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    blinding_secret.allocated = PYTHIA_BN_BUF_SIZE;

    transformed_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
    transformed_password.allocated = PYTHIA_GT_BUF_SIZE;

    transformation_private_key.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    transformation_private_key.allocated = PYTHIA_BN_BUF_SIZE;

    transformed_tweak.p = (uint8_t *)malloc(PYTHIA_G2_BUF_SIZE);
    transformed_tweak.allocated = PYTHIA_G2_BUF_SIZE;

    transformation_public_key.p = (uint8_t *)malloc(PYTHIA_G1_BUF_SIZE);
    transformation_public_key.allocated = PYTHIA_G1_BUF_SIZE;

    proof_value_c.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    proof_value_c.allocated = PYTHIA_BN_BUF_SIZE;

    proof_value_u.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    proof_value_u.allocated = PYTHIA_BN_BUF_SIZE;

    transformation_key_id_buf.p = (uint8_t *)w;
    transformation_key_id_buf.len = 10;

    tweak_buf.p = (uint8_t *)t;
    tweak_buf.len = 5;

    pythia_secret_buf.p = (uint8_t *)msk;
    pythia_secret_buf.len = 13;

    pythia_scope_secret_buf.p = (uint8_t *)ssk;
    pythia_scope_secret_buf.len = 13;

    password_buf.p = (uint8_t *)password;
    password_buf.len = 8;

    if (pythia_w_blind(&password_buf, &blinded_password, &blinding_secret))
        TEST_FAIL();

    if (pythia_w_transform(&blinded_password, &transformation_key_id_buf, &tweak_buf, &pythia_secret_buf,
                           &pythia_scope_secret_buf, &transformed_password, &transformation_private_key,
                           &transformed_tweak))
        TEST_FAIL();

    if (pythia_w_prove(&transformed_password, &blinded_password, &transformed_tweak, &transformation_private_key,
                       &transformation_public_key, &proof_value_c, &proof_value_u))
        TEST_FAIL();

    int verified = 0;
    if (pythia_w_verify(&transformed_password, &blinded_password, &tweak_buf, &transformation_public_key,
                        &proof_value_c, &proof_value_u, &verified))
        TEST_FAIL();

    TEST_ASSERT_NOT_EQUAL(0, verified);

    free(blinded_password.p);
    free(blinding_secret.p);
    free(transformed_password.p);
    free(transformation_private_key.p);
    free(transformed_tweak.p);
    free(transformation_public_key.p);
    free(proof_value_c.p);
    free(proof_value_u.p);

    pythia_deinit();
}

void test3_UpdateDelta() {
    TEST_ASSERT_EQUAL_INT(pythia_init(NULL), 0);

    pythia_buf_t blinded_password, blinding_secret, transformed_password,
            transformation_private_key, transformed_tweak,
            password_update_token, updated_transformation_public_key,
            transformation_public_key, proof_value_c, proof_value_u,
            transformation_key_id_buf, tweak_buf, pythia_secret_buf,
            pythia_scope_secret_buf, password_buf, new_pythia_secret_buf,
            updated_deblinded_password, deblinded_password, new_deblinded_password;

    blinded_password.p = (uint8_t *)malloc(PYTHIA_G1_BUF_SIZE);
    blinded_password.allocated = PYTHIA_G1_BUF_SIZE;

    blinding_secret.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    blinding_secret.allocated = PYTHIA_BN_BUF_SIZE;

    transformed_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
    transformed_password.allocated = PYTHIA_GT_BUF_SIZE;

    transformation_private_key.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    transformation_private_key.allocated = PYTHIA_BN_BUF_SIZE;

    transformed_tweak.p = (uint8_t *)malloc(PYTHIA_G2_BUF_SIZE);
    transformed_tweak.allocated = PYTHIA_G2_BUF_SIZE;

    transformation_public_key.p = (uint8_t *)malloc(PYTHIA_G1_BUF_SIZE);
    transformation_public_key.allocated = PYTHIA_G1_BUF_SIZE;

    proof_value_c.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    proof_value_c.allocated = PYTHIA_BN_BUF_SIZE;

    proof_value_u.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    proof_value_u.allocated = PYTHIA_BN_BUF_SIZE;

    password_update_token.p = (uint8_t *)malloc(PYTHIA_BN_BUF_SIZE);
    password_update_token.allocated = PYTHIA_BN_BUF_SIZE;

    updated_transformation_public_key.p = (uint8_t *)malloc(PYTHIA_G1_BUF_SIZE);
    updated_transformation_public_key.allocated = PYTHIA_G1_BUF_SIZE;

    tweak_buf.p = (uint8_t *)t;
    tweak_buf.len = 5;

    transformation_key_id_buf.p = (uint8_t *)w;
    transformation_key_id_buf.len = 10;

    pythia_secret_buf.p = (uint8_t *)msk;
    pythia_secret_buf.len = 13;

    new_pythia_secret_buf.p = (uint8_t *)msk1;
    new_pythia_secret_buf.len = 13;

    pythia_scope_secret_buf.p = (uint8_t *)ssk;
    pythia_scope_secret_buf.len = 13;

    password_buf.p = (uint8_t *)password;
    password_buf.len = 8;

    deblinded_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
    deblinded_password.allocated = PYTHIA_GT_BUF_SIZE;

    new_deblinded_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
    new_deblinded_password.allocated = PYTHIA_GT_BUF_SIZE;

    updated_deblinded_password.p = (uint8_t *)malloc(PYTHIA_GT_BUF_SIZE);
    updated_deblinded_password.allocated = PYTHIA_GT_BUF_SIZE;

    if (pythia_w_blind(&password_buf, &blinded_password, &blinding_secret))
        TEST_FAIL();

    if (pythia_w_transform(&blinded_password, &transformation_key_id_buf, &tweak_buf, &pythia_secret_buf,
                           &pythia_scope_secret_buf, &transformed_password, &transformation_private_key,
                           &transformed_tweak))
        TEST_FAIL();

    if (pythia_w_deblind(&transformed_password, &blinding_secret, &deblinded_password))
        TEST_FAIL();

    if (pythia_w_get_password_update_token(&transformation_key_id_buf, &pythia_secret_buf, &pythia_scope_secret_buf,
                                           &transformation_key_id_buf, &new_pythia_secret_buf, &pythia_scope_secret_buf,
                                           &password_update_token, &updated_transformation_public_key))
        TEST_FAIL();

    if (pythia_w_update_deblinded_with_token(&deblinded_password, &password_update_token, &updated_deblinded_password))
        TEST_FAIL();

    if (pythia_w_blind(&password_buf, &blinded_password, &blinding_secret))
        TEST_FAIL();

    if (pythia_w_transform(&blinded_password, &transformation_key_id_buf, &tweak_buf, &new_pythia_secret_buf,
                           &pythia_scope_secret_buf, &transformed_password, &transformation_private_key,
                           &transformed_tweak))
        TEST_FAIL();

    if (pythia_w_deblind(&transformed_password, &blinding_secret, &new_deblinded_password))
        TEST_FAIL();

    TEST_ASSERT_EQUAL_INT(updated_deblinded_password.len, new_deblinded_password.len);
    TEST_ASSERT_EQUAL_MEMORY(updated_deblinded_password.p, new_deblinded_password.p, updated_deblinded_password.len);

    if (pythia_w_prove(&transformed_password, &blinded_password, &transformed_tweak, &transformation_private_key,
                       &transformation_public_key, &proof_value_c, &proof_value_u))
        TEST_FAIL();

    TEST_ASSERT_EQUAL_INT(transformation_public_key.len, updated_transformation_public_key.len);
    TEST_ASSERT_EQUAL_MEMORY(transformation_public_key.p, updated_transformation_public_key.p, transformation_public_key.len);


    free(updated_deblinded_password.p);
    free(new_deblinded_password.p);
    free(deblinded_password.p);
    free(updated_transformation_public_key.p);
    free(password_update_token.p);
    free(proof_value_u.p);
    free(proof_value_c.p);
    free(transformation_public_key.p);
    free(transformed_tweak.p);
    free(transformation_private_key.p);
    free(transformed_password.p);
    free(blinding_secret.p);
    free(blinded_password.p);

    pythia_deinit();
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test1_DeblindStability);
    RUN_TEST(test2_BlindEvalProveVerify);
    RUN_TEST(test3_UpdateDelta);


    return UNITY_END();
}
