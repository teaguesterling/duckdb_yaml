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

// Reading is safe to overload unconditionally -- whichever type shows up at the
// call site picks its overload, and the Identifier one only has to EXIST when
// the type does.
inline const string &CompatIdentifierName(const string &name) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
inline const string &CompatIdentifierName(const Identifier &id) {
	return id.GetIdentifierName();
}
#endif

// Constructing is where the choice has to be right, and `__has_include` is the
// WRONG basis for it -- see the long note on CompatName below. identifier.hpp
// has been backported to the stable v1.5-variegata branch without child_list_t
// changing its key type, so the header probe would flip this to Identifier on a
// DuckDB that still wants string. Derive it from the boundary instead: this
// helper's job is to produce a child_list_t key (STRUCT field names), and
// TableFunctionRef::alias and FunctionSet::name move with it.
//
// No `typename`: child_list_t<LogicalType> is a concrete type here, and
// `typename` outside a template is only valid from C++20 (this header compiles
// at C++11).
using CompatIdentifierKey = child_list_t<LogicalType>::value_type::first_type;

inline string CompatMakeIdentifierImpl(string name, const string *) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
inline Identifier CompatMakeIdentifierImpl(string name, const Identifier *) {
	return Identifier(std::move(name));
}
#endif
inline CompatIdentifierKey CompatMakeIdentifier(string name) {
	return CompatMakeIdentifierImpl(std::move(name), static_cast<const CompatIdentifierKey *>(nullptr));
}

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

// === DuckDB v2.0 (duckdb `main`) shims (appended) ===
//
// These cover the change classes documented in duckdb_markdown's
// docs/duckdb_v2_migration.md that the sections above do not already handle.
//
// FEATURE DETECTION, NOT VERSION NUMBERS, and each change is probed
// SEPARATELY. A version macro says *when* a thing changed; a probe says whether
// it changed *here*, which keeps working when a change is backported, reverted,
// or lands on a branch nobody expected. Tying several changes to one macro
// silently picks the wrong branch the moment they land in different releases --
// so these deliberately do NOT reuse DUCKDB_HAS_NEW_VECTOR_HEADERS.
//
// <type_traits> is C++11, so it is safe to include unconditionally: this header
// is compiled at -std=c++11 against the pinned v1.5.x DuckDB (see the note at
// the top of the file about why the extension must not be forced to C++17).

#include <type_traits>
#include <utility>
#include "duckdb/function/table_function.hpp"

namespace duckdb {

//===--------------------------------------------------------------------===//
// CompatName / CompatNameStr / CompatMakeName -- bind-signature name type
//===--------------------------------------------------------------------===//
//
// v2.0 changed `table_function_bind_t` and `copy_to_bind_t` to take
// `vector<Identifier> &names` where v1.5 took `vector<string> &names`.
//
// DO NOT SELECT THIS TYPE WITH `__has_include("duckdb/common/identifier.hpp")`.
// That is the obvious probe and it is a TIME BOMB. identifier.hpp has already
// been BACKPORTED to the stable v1.5-variegata branch WITHOUT changing the bind
// signature, so on three real heads:
//
//   v1.5-variegata @ b155d6f63c (our pin)   no identifier.hpp   bind: vector<string>
//   v1.5-variegata @ branch tip             HAS identifier.hpp  bind: vector<string>
//   main (v2.0)                             HAS identifier.hpp  bind: vector<Identifier>
//
// the header probe agrees with reality only because our pin predates the
// backport. The next submodule bump would flip CompatName to Identifier on a
// DuckDB that still wants strings, and every bind signature in the extension
// would stop compiling at once.
//
// So derive the type from the boundary itself. TableFunctionBindInput's
// `input_table_names` has the same element type as the bind out-parameter on
// both lines (table_function.hpp:110 / :289 on the pin; :123 / :319 on main),
// which makes this exact by construction rather than by correlation.
using CompatName =
    std::remove_reference<decltype(std::declval<TableFunctionBindInput &>().input_table_names)>::type::value_type;

// The Identifier overloads below are still gated on the header, because the
// TYPE has to exist before it can be named. That gate decides only whether an
// overload can be declared -- never which type CompatName is.
inline string CompatNameStr(const string &name) {
	return name;
}
inline string CompatMakeNameImpl(string name, const string *) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
inline string CompatNameStr(const Identifier &name) {
	return name.GetIdentifierName();
}
inline Identifier CompatMakeNameImpl(string name, const Identifier *) {
	return Identifier(std::move(name));
}
#endif
//! Promote a RUNTIME string to whatever the bind signature wants. Literals need
//! no helper: Identifier(const char *) is implicit by design, precisely so that
//! only deliberate runtime promotions have to be spelled out.
inline CompatName CompatMakeName(string name) {
	return CompatMakeNameImpl(std::move(name), static_cast<const CompatName *>(nullptr));
}

//! Whole-vector conversions for the same boundary. A bind function fills a
//! `vector<CompatName>` but the bind DATA keeps plain `vector<string>` (it is
//! compared and sliced with string operations downstream), so the two vectors
//! have to be converted rather than assigned. No-ops on v1.5.x.
inline vector<string> CompatNameStrings(const vector<CompatName> &names) {
	vector<string> result;
	result.reserve(names.size());
	for (const auto &name : names) {
		result.push_back(CompatNameStr(name));
	}
	return result;
}
inline vector<CompatName> CompatMakeNames(const vector<string> &names) {
	vector<CompatName> result;
	result.reserve(names.size());
	for (const auto &name : names) {
		result.push_back(CompatMakeName(name));
	}
	return result;
}

//===--------------------------------------------------------------------===//
// CompatWithAlias -- LogicalType alias
//===--------------------------------------------------------------------===//
//
// v1.5: void SetAlias(string)               -- mutates in place
// v2.0: LogicalType WithAlias(string) const -- returns a copy, so it never
//       mutates a type whose type-info is shared with other types.
//
// SetAlias is REMOVED on v2.0, not deprecated.
//
// Dispatched on a tag rather than with `if constexpr`, so this compiles at
// C++11 (which is what this extension builds at -- see the header preamble).
// Tag dispatch has the property that matters here: only the selected overload
// is instantiated, so the branch naming the absent member is never compiled.
template <class T, class = void>
struct CompatHasWithAlias : std::false_type {};
template <class T>
struct CompatHasWithAlias<T, decltype(void(std::declval<const T &>().WithAlias(string())))> : std::true_type {};

template <class TYPE>
inline LogicalType CompatWithAliasImpl(TYPE type, string alias, std::true_type) {
	return type.WithAlias(std::move(alias));
}
template <class TYPE>
inline LogicalType CompatWithAliasImpl(TYPE type, string alias, std::false_type) {
	type.SetAlias(std::move(alias));
	return type;
}
// The entry point is deliberately NOT a template: `LogicalType::VARCHAR` and
// friends are `LogicalTypeId` constants, so a deduced parameter binds TYPE to
// LogicalTypeId and then fails inside the shim ("request for member 'SetAlias'
// in 'type', which is of non-class type 'duckdb::LogicalTypeId'"). A concrete
// LogicalType parameter makes the usual implicit conversion happen at the call
// site instead. The dispatch below is still a template, so only the selected
// overload is instantiated and the branch naming the absent member is never
// compiled.
inline LogicalType CompatWithAlias(LogicalType type, string alias) {
	return CompatWithAliasImpl(std::move(type), std::move(alias), CompatHasWithAlias<LogicalType>());
}

//===--------------------------------------------------------------------===//
// CompatSetCaptureArgumentAliases -- named arguments derived from aliases
//===--------------------------------------------------------------------===//
//
// A SILENT RUNTIME BREAK, not a compile error: v2.0 added
// FunctionProperties::capture_argument_aliases, DEFAULTING TO FALSE. On v1.5 the
// binder always recorded a named argument's alias on the bound child expression,
// so a bind callback could recover `style := 'block'` with child->GetAlias(). On
// v2.0 that alias comes back EMPTY unless the function opts in, so a function
// that derives named parameters from argument aliases binds every argument as
// unnamed and fails at RUNTIME -- with a completely green build and nothing to
// grep for. (duckdb's own struct_pack/row call SetCaptureArgumentAliases(true)
// for exactly this reason.)
//
// No-op on v1.5.x, where the capture was unconditional.
template <class T, class = void>
struct CompatHasSetCaptureArgumentAliases : std::false_type {};
template <class T>
struct CompatHasSetCaptureArgumentAliases<T, decltype(void(std::declval<T &>().SetCaptureArgumentAliases(true)))>
    : std::true_type {};

template <class FUNC>
inline void CompatSetCaptureArgumentAliasesImpl(FUNC &fun, std::true_type) {
	fun.SetCaptureArgumentAliases(true);
}
template <class FUNC>
inline void CompatSetCaptureArgumentAliasesImpl(FUNC &, std::false_type) {
}
template <class FUNC>
inline void CompatSetCaptureArgumentAliases(FUNC &fun) {
	CompatSetCaptureArgumentAliasesImpl(fun, CompatHasSetCaptureArgumentAliases<FUNC>());
}

//===--------------------------------------------------------------------===//
// CompatSetFallible -- functions that can throw at execution time
//===--------------------------------------------------------------------===//
//
// A SILENT RUNTIME BREAK. v2.0 requires a scalar function that can throw during
// execution to say so; throwing from one that has not becomes
//
//   INTERNAL Error: Scalar function "f" threw an execution error, but the
//   function is not marked as fallible - the function must call SetFallible().
//
// There is no compile error and nothing in the DuckDB API to grep for -- the
// thing to grep is your own `throw` statements inside execution bodies, plus
// the transitive callers of throwing helpers. Enforcement is an assertion, so
// a canary leg built without assertions can be green while the contract is
// violated; do not read a green arch as proof.
//
// IMPORTANT: this is NOT a v2.0-only concept and the shim is NOT a no-op on
// v1.5.x. BaseScalarFunction::SetFallible() exists on the pinned v1.5.4 too
// (function.hpp), and `errors` feeds Expression::CanThrow(), which gates
// conjunct reordering, filter pushdown and dictionary caching. v2.0 only added
// ENFORCEMENT of a contract that already existed. So mark PRECISELY: declaring
// a function fallible when it cannot throw is itself an optimizer-visible
// change on the version this extension ships.
//
// The shim exists only for the FunctionSet case. A plain ScalarFunction (or
// AggregateFunction) has SetFallible() on both lines and could call it
// directly; a FunctionSet gained a set-level SetFallible() only on v2.0, where
// its members became immutable shared_ptr<const T> and can no longer be
// iterated and mutated the way v1.5's vector<T> can.
template <class T, class = void>
struct CompatHasSetFallible : std::false_type {};
template <class T>
struct CompatHasSetFallible<T, decltype(void(std::declval<T &>().SetFallible()))> : std::true_type {};

//! Direct: a function on either line, or a v2.0 set (which applies it to every overload).
template <class FUNC>
inline void CompatSetFallibleImpl(FUNC &fun, std::true_type) {
	fun.SetFallible();
}
//! v1.5 FunctionSet: no set-level setter, but the overloads are still mutable.
template <class SET>
inline void CompatSetFallibleImpl(SET &set, std::false_type) {
	for (auto &fun : set.functions) {
		fun.SetFallible();
	}
}
template <class FUNC>
inline void CompatSetFallible(FUNC &fun) {
	CompatSetFallibleImpl(fun, CompatHasSetFallible<FUNC>());
}

//===--------------------------------------------------------------------===//
// CompatFlatValidityMutable -- FlatVector validity write access
//===--------------------------------------------------------------------===//
//
// The same const split as CompatFlatDataMutable, applied to the validity mask:
//   v2.0: FlatVector::Validity(const Vector &)  -> const ValidityMask &  (Buffer())
//         FlatVector::ValidityMutable(Vector &) ->       ValidityMask &  (BufferMutable())
// BufferMutable() un-shares a copy-on-write buffer first; Buffer() does not.
//
// This one hides better than the data split: the accessor call still COMPILES,
// silently deducing a const reference, and the diagnostic only appears at the
// mutation as "passing 'const duckdb::ValidityMask' as 'this' argument discards
// qualifiers". So grep the MUTATION, not the accessor:
//   grep -rn 'SetInvalid\|SetValid(\|SetAllInvalid\|SetAllValid' src/
//
// (This extension has no direct FlatVector::Validity call site today -- every
// SetInvalid here is on the ValidityMask the executor hands to a lambda -- but
// the shim lives here because this header is the shared reference for the
// duck_block extension family.)
template <class T, class = void>
struct CompatHasValidityMutable : std::false_type {};
template <class T>
struct CompatHasValidityMutable<T, decltype(void(T::ValidityMutable(std::declval<Vector &>())))> : std::true_type {};

template <class FV>
inline ValidityMask &CompatFlatValidityMutableImpl(Vector &vec, std::true_type) {
	return FV::ValidityMutable(vec);
}
template <class FV>
inline ValidityMask &CompatFlatValidityMutableImpl(Vector &vec, std::false_type) {
	return FV::Validity(vec);
}
template <class FV = FlatVector>
inline ValidityMask &CompatFlatValidityMutable(Vector &vec) {
	return CompatFlatValidityMutableImpl<FV>(vec, CompatHasValidityMutable<FV>());
}

//===--------------------------------------------------------------------===//
// CompatFlatDataMutable -- FlatVector write access
//===--------------------------------------------------------------------===//
//
// v1.5: FlatVector::GetData<T>(vec)        returns T*
// v2.0: FlatVector::GetData<T>(vec)        returns const T*
//       FlatVector::GetDataMutable<T>(vec) returns T*
//
// Writing through the v2.0 read accessor is a compile error, which is the whole
// point of the split, so the WRITE path has to ask for mutability explicitly.
// Probing for GetDataMutable (the member that exists only on v2.0) rather than
// for GetData (which exists on both) is what makes the probe discriminate.
//
// ConstantVector::GetData<T> kept its non-const overload, so writes through
// *it* need no shim.
template <class T, class = void>
struct CompatHasFlatGetDataMutable : std::false_type {};
template <class T>
struct CompatHasFlatGetDataMutable<T, decltype(void(T::template GetDataMutable<bool>(std::declval<Vector &>())))>
    : std::true_type {};

template <class VALUE, class FV>
inline VALUE *CompatFlatDataMutableImpl(Vector &vec, std::true_type) {
	return FV::template GetDataMutable<VALUE>(vec);
}
template <class VALUE, class FV>
inline VALUE *CompatFlatDataMutableImpl(Vector &vec, std::false_type) {
	return FV::template GetData<VALUE>(vec);
}
template <class VALUE, class FV = FlatVector>
inline VALUE *CompatFlatDataMutable(Vector &vec) {
	return CompatFlatDataMutableImpl<VALUE, FV>(vec, CompatHasFlatGetDataMutable<FV>());
}

} // namespace duckdb
