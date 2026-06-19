#pragma once

#include "duckdb.hpp"

// duckdb_compat.hpp — cross-version shim for DuckDB extensions.
//
// Pattern from @bendrucker's teaguesterling/duckdb_webbed#76 (May 2026):
// detect the new API via __has_include of headers that moved in the same DuckDB
// refactor ([duckdb/duckdb#22377](https://github.com/duckdb/duckdb/pull/22377) —
// "mandatory per-vector size tracking" landed alongside the vector-buffer
// header reshuffle), then dispatch via a single #ifdef block.
//
// Cross-version coverage:
//   - duckdb v1.4.x / v1.5.x: old API everywhere (built with -std=c++11)
//   - duckdb main / v1.6.x:   new API everywhere (built with -std=c++17)
//
// Important: this header is included on BOTH sides; nothing in it must require
// C++17 unconditionally. `std::optional` is only used inside the
// `DUCKDB_HAS_NEW_VECTOR_HEADERS` branches (which only compile on duckdb main,
// where C++17 is available). Forcing the whole extension to C++17 against
// duckdb v1.5.x's C++11 internals breaks linkage (static const data members
// in duckdb headers acquire implicit inline linkage in C++17 but not in C++11
// — multiple-definition errors at link time).
//
// See teaguesterling/duckdb_markdown's docs/DUCKDB_API_MIGRATION.md for the
// long-form rationale + upgrade checklist for other extensions.

#if __has_include("duckdb/common/vector/list_vector.hpp")
#define DUCKDB_HAS_NEW_VECTOR_HEADERS 1
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#include <optional> // C++17, only needed on the new-API path
#endif

namespace duckdb {

//===--------------------------------------------------------------------===//
// CompatSetOutputCardinality
//===--------------------------------------------------------------------===//

inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
	chunk.SetChildCardinality(count);
#else
	chunk.SetCardinality(count);
#endif
}

//===--------------------------------------------------------------------===//
// SetValueCasted
//===--------------------------------------------------------------------===//
//
// Cross-version helper. On duckdb main, VectorStringBuffer::SetValue and
// StandardVectorBuffer::SetValue fall back to Value::DefaultCastAs(target_type)
// when val.type() != column.type(). DefaultCastAs uses a stack-local
// CastFunctionSet that does NOT see extension-registered casts
// (loader.RegisterCastFunction); the cast silently returns NULL and SetValue
// writes NULL. Pre-casting via Value::CastAs(ClientContext&, target_type) uses
// the catalog's cast set, which includes extension casts. Behaves identically
// on v1.4.x / v1.5.x where the old SetValue tolerated alias-only mismatches.
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
// Callsites use the OLD mask-based signature for the lambda:
//
//   CompatUnaryExecuteWithNulls<INPUT, RESULT>(
//       input, result, count,
//       [&](INPUT v, ValidityMask &mask, idx_t idx) -> RESULT {
//           if (!mask.RowIsValid(idx)) return RESULT{};       // NULL input
//           if (some_condition) {
//               mask.SetInvalid(idx);                          // explicit NULL output
//               return RESULT{};
//           }
//           return compute(v);
//       });
//
// The same lambda compiles against both v1.5.x and main:
//   - v1.5.x: forwarded directly to `UnaryExecutor::ExecuteWithNulls`.
//   - main: `ExecuteWithNulls` was removed in `987ea2c409`; we adapt by
//     constructing a fresh `ValidityMask` per row, calling the lambda with
//     it (idx=0), and translating its post-state to `std::optional<RESULT>`
//     for `UnaryExecutor::Execute`'s SFINAE-detected null-emitting overload.
//
// The "scratch mask" pattern keeps the C++17-only `std::optional` confined to
// the main-only branch, so the v1.5.x extension keeps building with `-std=c++11`
// alongside duckdb's C++11 internals.

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS

template <class INPUT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatUnaryExecuteWithNulls(Vector &input, Vector &result, idx_t count, FUNC fun) {
	UnaryExecutor::Execute<INPUT_TYPE, RESULT_TYPE>(input, result, count, [fun](INPUT_TYPE in) -> std::optional<RESULT_TYPE> {
		ValidityMask scratch; // default-constructed → all rows valid
		RESULT_TYPE val = fun(in, scratch, idx_t(0));
		if (!scratch.RowIsValid(0)) {
			return std::nullopt;
		}
		return val;
	});
}

template <class LEFT_TYPE, class RIGHT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatBinaryExecuteWithNulls(Vector &left, Vector &right, Vector &result, idx_t count, FUNC fun) {
	BinaryExecutor::Execute<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE>(
	    left, right, result, count, [fun](LEFT_TYPE l, RIGHT_TYPE r) -> std::optional<RESULT_TYPE> {
		    ValidityMask scratch;
		    RESULT_TYPE val = fun(l, r, scratch, idx_t(0));
		    if (!scratch.RowIsValid(0)) {
			    return std::nullopt;
		    }
		    return val;
	    });
}

#else // v1.4.x / v1.5.x — pass the callsite lambda straight through

template <class INPUT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatUnaryExecuteWithNulls(Vector &input, Vector &result, idx_t count, FUNC fun) {
	UnaryExecutor::ExecuteWithNulls<INPUT_TYPE, RESULT_TYPE>(input, result, count, fun);
}

template <class LEFT_TYPE, class RIGHT_TYPE, class RESULT_TYPE, class FUNC>
inline void CompatBinaryExecuteWithNulls(Vector &left, Vector &right, Vector &result, idx_t count, FUNC fun) {
	BinaryExecutor::ExecuteWithNulls<LEFT_TYPE, RIGHT_TYPE, RESULT_TYPE>(left, right, result, count, fun);
}

#endif

} // namespace duckdb

// --- duckdb::Identifier cross-version helpers (appended) ---
// DuckDB main introduced duckdb::Identifier, which replaced std::string as the key of
// child_list_t (STRUCT field names) and several name-typed fields (TableFunctionRef::alias,
// named_parameters keys). Identifier does not implicitly convert to/from std::string, so
// reads/constructions at that boundary go through these helpers (no-ops on stable DuckDB).
#if __has_include("duckdb/common/identifier.hpp")
#define DUCKDB_HAS_IDENTIFIER 1
#include "duckdb/common/identifier.hpp"
#endif

namespace duckdb {

#ifdef DUCKDB_HAS_IDENTIFIER
inline const string &CompatIdentifierName(const Identifier &id) {
	return id.GetIdentifierName();
}
inline const string &CompatIdentifierName(const string &name) {
	return name;
}
inline Identifier CompatMakeIdentifier(string name) {
	return Identifier(std::move(name));
}
#else
inline const string &CompatIdentifierName(const string &name) {
	return name;
}
inline string CompatMakeIdentifier(string name) {
	return name;
}
#endif

} // namespace duckdb

// === duckdb main API compat (appended for v1.6.x/main) ===
#if __has_include("duckdb/planner/expression/bound_function_expression.hpp")
#include "duckdb/planner/expression/bound_function_expression.hpp"
#endif
#if __has_include("duckdb/function/scalar_function.hpp")
#include "duckdb/function/scalar_function.hpp"
#endif

// Scalar bind-function signature changed on duckdb main:
//   old: (ClientContext&, ScalarFunction&, vector<unique_ptr<Expression>>&)
//   new: (BindScalarFunctionInput&)
// Define bind functions with DUCKDB_SCALAR_BIND_PARAMS and read inputs via the
// DUCKDB_SCALAR_BIND_CONTEXT / DUCKDB_SCALAR_BIND_ARGS macros.
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
#define DUCKDB_SCALAR_BIND_PARAMS  duckdb::BindScalarFunctionInput &bind_input
#define DUCKDB_SCALAR_BIND_CONTEXT bind_input.GetClientContext()
#define DUCKDB_SCALAR_BIND_ARGS    bind_input.GetArguments()
#else
#define DUCKDB_SCALAR_BIND_PARAMS                                                                                       \
	duckdb::ClientContext &context, duckdb::ScalarFunction &bound_function,                                             \
	    duckdb::vector<duckdb::unique_ptr<duckdb::Expression>> &arguments
#define DUCKDB_SCALAR_BIND_CONTEXT context
#define DUCKDB_SCALAR_BIND_ARGS    arguments
#endif

namespace duckdb {

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
inline const LogicalType &CompatExprReturnType(const Expression &e) {
	return e.GetReturnType();
}
inline vector<unique_ptr<Expression>> &CompatBoundChildren(BoundFunctionExpression &e) {
	return e.GetChildrenMutable();
}
inline unique_ptr<FunctionData> &CompatBoundBindInfo(BoundFunctionExpression &e) {
	return e.BindInfoMutable();
}
inline void CompatSetScalarReturnType(ScalarFunction &f, LogicalType t) {
	f.SetReturnType(std::move(t));
}
inline void CompatSetScalarNullHandling(ScalarFunction &f, FunctionNullHandling h) {
	f.SetNullHandling(h);
}
inline void CompatSetScalarVarArgs(ScalarFunction &f, LogicalType v) {
	f.SetVarArgs(std::move(v));
}
inline string CompatExprAlias(const BaseExpression &e) {
	return CompatIdentifierName(e.GetAlias());
}
#else
inline const LogicalType &CompatExprReturnType(const Expression &e) {
	return e.return_type;
}
inline vector<unique_ptr<Expression>> &CompatBoundChildren(BoundFunctionExpression &e) {
	return e.children;
}
inline unique_ptr<FunctionData> &CompatBoundBindInfo(BoundFunctionExpression &e) {
	return e.bind_info;
}
inline void CompatSetScalarReturnType(ScalarFunction &f, LogicalType t) {
	f.return_type = std::move(t);
}
inline void CompatSetScalarNullHandling(ScalarFunction &f, FunctionNullHandling h) {
	f.null_handling = h;
}
inline void CompatSetScalarVarArgs(ScalarFunction &f, LogicalType v) {
	f.varargs = std::move(v);
}
inline string CompatExprAlias(const BaseExpression &e) {
	return e.alias;
}
#endif

} // namespace duckdb

// === bind-return-type + aggregate-finalize compat (appended) ===
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
#define DUCKDB_AGG_FINALIZE_INPUT_TYPE duckdb::AggregateFinalizeInputData
#else
#define DUCKDB_AGG_FINALIZE_INPUT_TYPE duckdb::AggregateInputData
#endif

namespace duckdb {
// Set the return type from inside a scalar bind function. On duckdb main the bind
// receives a BoundScalarFunction (SetReturnType); on stable it is a ScalarFunction
// (public return_type field). Templated so it works for whichever type the
// DUCKDB_SCALAR_BIND_PARAMS macro put in scope as `bound_function`.
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
template <class F>
inline void CompatBindSetReturnType(F &f, LogicalType t) {
	f.SetReturnType(std::move(t));
}
#else
template <class F>
inline void CompatBindSetReturnType(F &f, LogicalType t) {
	f.return_type = std::move(t);
}
#endif
} // namespace duckdb

// === const overloads for bound-expression accessors (execute fns read a const Expression) ===
namespace duckdb {
#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS
inline const vector<unique_ptr<Expression>> &CompatBoundChildren(const BoundFunctionExpression &e) {
	return e.GetChildren();
}
inline const unique_ptr<FunctionData> &CompatBoundBindInfo(const BoundFunctionExpression &e) {
	return e.BindInfo();
}
#else
inline const vector<unique_ptr<Expression>> &CompatBoundChildren(const BoundFunctionExpression &e) {
	return e.children;
}
inline const unique_ptr<FunctionData> &CompatBoundBindInfo(const BoundFunctionExpression &e) {
	return e.bind_info;
}
#endif
} // namespace duckdb
