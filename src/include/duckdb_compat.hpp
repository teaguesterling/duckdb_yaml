#pragma once

#include "duckdb.hpp"
#include <optional>

// duckdb_compat.hpp — cross-version shim for DuckDB extensions.
//
// Pattern from @bendrucker's teaguesterling/duckdb_webbed#76 (May 2026):
// detect the new API via __has_include of headers that moved in the same DuckDB
// refactor ([duckdb/duckdb#22377](https://github.com/duckdb/duckdb/pull/22377) —
// "mandatory per-vector size tracking" landed alongside the vector-buffer
// header reshuffle), then dispatch via a single #ifdef block.
//
// Cross-version coverage:
//   - duckdb v1.4.x / v1.5.x: old API everywhere
//   - duckdb main / v1.6.x:   new API everywhere
//
// See teaguesterling/duckdb_markdown's docs/DUCKDB_API_MIGRATION.md for the
// long-form rationale + upgrade checklist for other extensions.

#if __has_include("duckdb/common/vector/list_vector.hpp")
#define DUCKDB_HAS_NEW_VECTOR_HEADERS 1
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#endif

namespace duckdb {

//===--------------------------------------------------------------------===//
// CompatSetOutputCardinality
//===--------------------------------------------------------------------===//
//
// DuckDB main mandates per-vector Size() tracking; DataChunk::SetCardinality
// only updates chunk.count. SetChildCardinality additionally calls
// FlatVector::SetSize on every column so query operators reading vec.Size()
// see the right value. Without this, VariadicExecutor (and similar) reports:
//   "Mismatch in input vector sizes ... expected 0 rows but got N"

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetChildCardinality(count);
}
#else
inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetCardinality(count);
}
#endif

//===--------------------------------------------------------------------===//
// SetValueCasted
//===--------------------------------------------------------------------===//
//
// On duckdb main, VectorStringBuffer::SetValue and StandardVectorBuffer::SetValue
// fall back to Value::DefaultCastAs(target_type) when val.type() != column.type().
// DefaultCastAs uses a stack-local CastFunctionSet that does NOT see
// extension-registered casts (loader.RegisterCastFunction); the cast silently
// returns NULL and SetValue writes NULL. Pre-casting via Value::CastAs(
// ClientContext&, target_type) uses the catalog's cast set, which includes
// extension casts. Behaves identically on v1.4.x / v1.5.x where the old
// SetValue tolerated alias-only mismatches.
//
// yaml has a YAMLType alias on VARCHAR (see yaml_types.cpp), so this matters
// anywhere a Value(string) is written to a YAMLType-typed output column.

inline void SetValueCasted(ClientContext &context, Vector &vec, idx_t idx, const Value &val) {
	vec.SetValue(idx, val.CastAs(context, vec.GetType()));
}

//===--------------------------------------------------------------------===//
// CompatUnaryExecuteWithNulls / CompatBinaryExecuteWithNulls
//===--------------------------------------------------------------------===//
//
// `UnaryExecutor::ExecuteWithNulls` and `BinaryExecutor::ExecuteWithNulls` were
// removed from duckdb main (commit `987ea2c409` — "Functions can throw errors",
// #15166). On main, the equivalent is `Execute<INPUT, RESULT>` where the
// lambda returns `std::optional<RESULT>` (auto-detected via SFINAE inside the
// template — see duckdb/common/vector_operations/unary_executor.hpp:241). The
// framework reads validity from the input and writes NULL to the output if the
// optional is `nullopt`.
//
// We expose a single API used by the call-sites:
//
//   CompatUnaryExecuteWithNulls<INPUT, RESULT>(
//       input, result, count,
//       [&](INPUT v) -> std::optional<RESULT> {
//           if (some_condition) return std::nullopt;  // emit SQL NULL
//           return compute(v);
//       });
//
// On duckdb main, this delegates directly to `UnaryExecutor::Execute<INPUT,
// RESULT>` (which sees the `std::optional<RESULT>` return and turns on
// per-row null propagation).
//
// On v1.4.x / v1.5.x, we wrap the lambda into the old `ExecuteWithNulls`
// signature `(INPUT, ValidityMask&, idx_t) -> RESULT`, mapping `nullopt`
// to `mask.SetInvalid(idx)`.
//
// `INPUT NULL → output NULL` is handled by the framework on both sides; the
// lambda only sees defined inputs.

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS

template <class INPUT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatUnaryExecuteWithNulls(Vector &input, Vector &result, idx_t count, FUNC fun) {
	UnaryExecutor::Execute<INPUT_TYPE, RESULT_TYPE>(input, result, count, fun);
}

template <class LEFT_TYPE, class RIGHT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatBinaryExecuteWithNulls(Vector &left, Vector &right, Vector &result, idx_t count, FUNC fun) {
	BinaryExecutor::Execute<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE>(left, right, result, count, fun);
}

#else // v1.4.x / v1.5.x — wrap optional<RESULT> lambdas into the mask-based form

template <class INPUT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatUnaryExecuteWithNulls(Vector &input, Vector &result, idx_t count, FUNC fun) {
	UnaryExecutor::ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(
	    input, result, count, [fun](INPUT_TYPE in, ValidityMask &mask, idx_t idx) -> RESULT_TYPE {
		    if (!mask.RowIsValid(idx)) {
			    return RESULT_TYPE{};
		    }
		    std::optional<RESULT_TYPE> r = fun(in);
		    if (!r.has_value()) {
			    mask.SetInvalid(idx);
			    return RESULT_TYPE{};
		    }
		    return *r;
	    });
}

template <class LEFT_TYPE, class RIGHT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatBinaryExecuteWithNulls(Vector &left, Vector &right, Vector &result, idx_t count, FUNC fun) {
	BinaryExecutor::ExecuteWithNulls<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE>(
	    left, right, result, count,
	    [fun](LEFT_TYPE l, RIGHT_TYPE r, ValidityMask &mask, idx_t idx) -> RESULT_TYPE {
		    if (!mask.RowIsValid(idx)) {
			    return RESULT_TYPE{};
		    }
		    std::optional<RESULT_TYPE> opt = fun(l, r);
		    if (!opt.has_value()) {
			    mask.SetInvalid(idx);
			    return RESULT_TYPE{};
		    }
		    return *opt;
	    });
}

#endif

} // namespace duckdb
