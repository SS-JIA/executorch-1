/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cmath>

#include <executorch/kernels/portable/cpu/scalar_utils.h>
#include <executorch/kernels/portable/cpu/util/broadcast_util.h>
#include <executorch/kernels/portable/cpu/util/functional_util.h>
#include <executorch/runtime/kernel/kernel_includes.h>

namespace torch {
namespace executor {
namespace native {

namespace {

template <typename CTYPE>
CTYPE bitwise_xor(CTYPE a, CTYPE b) {
  return a ^ b;
}

template <>
bool bitwise_xor<bool>(bool a, bool b) {
  return a != b;
}

} // namespace

using Tensor = exec_aten::Tensor;

Tensor& bitwise_xor_Tensor_out(
    RuntimeContext& ctx,
    const Tensor& a,
    const Tensor& b,
    Tensor& out) {
  // Determine output size and resize for dynamic shapes
  ET_KERNEL_CHECK(
      ctx,
      resize_to_broadcast_target_size(a, b, out) == Error::Ok,
      InvalidArgument,
      out);

  ScalarType a_type = a.scalar_type();
  ScalarType b_type = b.scalar_type();
  ScalarType common_type = promoteTypes(a_type, b_type);
  ScalarType out_type = out.scalar_type();

  ET_KERNEL_CHECK(ctx, canCast(common_type, out_type), InvalidArgument, out);

  ET_SWITCH_INT_TYPES_AND(
      Bool, a_type, ctx, "bitwise_xor.Tensor_out", CTYPE_A, [&]() {
        ET_SWITCH_INT_TYPES_AND(
            Bool, b_type, ctx, "bitwise_xor.Tensor_out", CTYPE_B, [&]() {
              ET_SWITCH_INT_TYPES_AND(
                  Bool,
                  common_type,
                  ctx,
                  "bitwise_xor.Tensor_out",
                  CTYPE_IN,
                  [&]() {
                    ET_SWITCH_REAL_TYPES_AND(
                        Bool,
                        out_type,
                        ctx,
                        "bitwise_xor.Tensor_out",
                        CTYPE_OUT,
                        [&]() {
                          apply_binary_elementwise_fn<
                              CTYPE_A,
                              CTYPE_B,
                              CTYPE_OUT>(
                              [](const CTYPE_A val_a, const CTYPE_B val_b) {
                                CTYPE_IN a_casted =
                                    static_cast<CTYPE_IN>(val_a);
                                CTYPE_IN b_casted =
                                    static_cast<CTYPE_IN>(val_b);
                                CTYPE_IN value =
                                    bitwise_xor(a_casted, b_casted);

                                return static_cast<CTYPE_OUT>(value);
                              },
                              a,
                              b,
                              out);
                        });
                  });
            });
      });

  return out;
}

Tensor& bitwise_xor_Scalar_out(
    RuntimeContext& ctx,
    const Tensor& a,
    const Scalar& b,
    Tensor& out) {
  (void)ctx;

  // Resize for dynamic shape
  ET_KERNEL_CHECK_MSG(
      ctx,
      resize_tensor(out, a.sizes()) == Error::Ok,
      InvalidArgument,
      out,
      "Failed to resize output tensor.");

  ScalarType a_type = a.scalar_type();
  ScalarType b_type = utils::get_scalar_dtype(b);
  ScalarType common_type = utils::promote_type_with_scalar(a_type, b);
  ScalarType out_type = out.scalar_type();

  ET_KERNEL_CHECK(ctx, canCast(common_type, out_type), InvalidArgument, out);

  ET_SWITCH_INT_TYPES_AND(
      Bool, a_type, ctx, "bitwise_xor.Scalar_out", CTYPE_A, [&]() {
        ET_SWITCH_SCALAR_OBJ_INTB_TYPES(
            b_type, ctx, "bitwise_xor.Scalar_out", CTYPE_B, [&]() {
              CTYPE_B val_b = 0;
              ET_EXTRACT_SCALAR(b, val_b);
              ET_SWITCH_INT_TYPES_AND(
                  Bool,
                  common_type,
                  ctx,
                  "bitwise_xor.Scalar_out",
                  CTYPE_IN,
                  [&]() {
                    ET_SWITCH_REAL_TYPES_AND(
                        Bool,
                        out_type,
                        ctx,
                        "bitwise_xor.Scalar_out",
                        CTYPE_OUT,
                        [&]() {
                          apply_unary_map_fn(
                              [val_b](const CTYPE_A val_a) {
                                CTYPE_IN a_casted =
                                    static_cast<CTYPE_IN>(val_a);
                                CTYPE_IN b_casted =
                                    static_cast<CTYPE_IN>(val_b);
                                CTYPE_IN value =
                                    bitwise_xor(a_casted, b_casted);

                                return static_cast<CTYPE_OUT>(value);
                              },
                              a.const_data_ptr<CTYPE_A>(),
                              out.mutable_data_ptr<CTYPE_OUT>(),
                              out.numel());
                        });
                  });
            });
      });

  return out;
}

} // namespace native
} // namespace executor
} // namespace torch
