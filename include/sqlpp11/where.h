/*
 * Copyright (c) 2013-2015, Roland Bock
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SQLPP_WHERE_H
#define SQLPP_WHERE_H

#include <sqlpp11/statement_fwd.h>
#include <sqlpp11/type_traits.h>
#include <sqlpp11/parameter_list.h>
#include <sqlpp11/expression.h>
#include <sqlpp11/interpret_tuple.h>
#include <sqlpp11/interpretable_list.h>
#include <sqlpp11/logic.h>

namespace sqlpp
{
  // WHERE DATA
  template <typename Database, typename... Expressions>
  struct where_data_t
  {
    where_data_t(Expressions... expressions) : _expressions(expressions...)
    {
    }

    where_data_t(const where_data_t&) = default;
    where_data_t(where_data_t&&) = default;
    where_data_t& operator=(const where_data_t&) = default;
    where_data_t& operator=(where_data_t&&) = default;
    ~where_data_t() = default;

    std::tuple<Expressions...> _expressions;
    interpretable_list_t<Database> _dynamic_expressions;
  };

  SQLPP_PORTABLE_STATIC_ASSERT(
      assert_no_unknown_tables_in_where_t,
      "at least one expression in where() requires a table which is otherwise not known in the statement");

  // WHERE(EXPR)
  template <typename Database, typename... Expressions>
  struct where_t
  {
    using _traits = make_traits<no_value_t, tag::is_where>;
    using _nodes = detail::type_vector<Expressions...>;

    using _is_dynamic = is_database<Database>;

    // Data
    using _data_t = where_data_t<Database, Expressions...>;

    // Member implementation with data and methods
    template <typename Policies>
    struct _impl_t
    {
      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      _impl_t() = default;
      _impl_t(const _data_t& data) : _data(data)
      {
      }

      template <typename Expression>
      void add_ntc(Expression expression)
      {
        add<Expression, std::false_type>(expression);
      }

      template <typename Expression, typename TableCheckRequired = std::true_type>
      void add(Expression expression)
      {
        static_assert(_is_dynamic::value, "where::add() can only be called for dynamic_where");
        static_assert(is_expression_t<Expression>::value, "invalid expression argument in where::add()");
        static_assert(not TableCheckRequired::value or Policies::template _no_unknown_tables<Expression>::value,
                      "expression uses tables unknown to this statement in where::add()");
        static_assert(not contains_aggregate_function_t<Expression>::value,
                      "where expression must not contain aggregate functions");
        using _serialize_check = sqlpp::serialize_check_t<typename Database::_serializer_context_t, Expression>;
        _serialize_check::_();

        using ok = logic::all_t<_is_dynamic::value, is_expression_t<Expression>::value, _serialize_check::type::value>;

        _add_impl(expression, ok());  // dispatch to prevent compile messages after the static_assert
      }

    private:
      template <typename Expression>
      void _add_impl(Expression expression, const std::true_type&)
      {
        return _data._dynamic_expressions.emplace_back(expression);
      }

      template <typename Expression>
      void _add_impl(Expression expression, const std::false_type&);

    public:
      _data_t _data;
    };

    // Base template to be inherited by the statement
    template <typename Policies>
    struct _base_t
    {
      using _data_t = where_data_t<Database, Expressions...>;

      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      template <typename... Args>
      _base_t(Args&&... args)
          : where{std::forward<Args>(args)...}
      {
      }

      _impl_t<Policies> where;
      _impl_t<Policies>& operator()()
      {
        return where;
      }
      const _impl_t<Policies>& operator()() const
      {
        return where;
      }

      template <typename T>
      static auto _get_member(T t) -> decltype(t.where)
      {
        return t.where;
      }

      using _consistency_check = typename std::conditional<Policies::template _no_unknown_tables<where_t>::value,
                                                           consistent_t,
                                                           assert_no_unknown_tables_in_where_t>::type;
    };
  };

  template <>
  struct where_data_t<void, bool>
  {
    bool _condition;
  };

  // WHERE(BOOL)
  template <>
  struct where_t<void, bool>
  {
    using _traits = make_traits<no_value_t, tag::is_where>;
    using _nodes = detail::type_vector<>;

    // Data
    using _data_t = where_data_t<void, bool>;

    // Member implementation with data and methods
    template <typename Policies>
    struct _impl_t
    {
      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      _impl_t() = default;
      _impl_t(const _data_t& data) : _data(data)
      {
      }

      _data_t _data;
    };

    // Base template to be inherited by the statement
    template <typename Policies>
    struct _base_t
    {
      using _data_t = where_data_t<void, bool>;

      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      template <typename... Args>
      _base_t(Args&&... args)
          : where{std::forward<Args>(args)...}
      {
      }

      _impl_t<Policies> where;
      _impl_t<Policies>& operator()()
      {
        return where;
      }
      const _impl_t<Policies>& operator()() const
      {
        return where;
      }

      template <typename T>
      static auto _get_member(T t) -> decltype(t.where)
      {
        return t.where;
      }

      using _consistency_check = consistent_t;
    };
  };

  SQLPP_PORTABLE_STATIC_ASSERT(assert_where_t, "where expression required, e.g. where(true)");

  SQLPP_PORTABLE_STATIC_ASSERT(assert_where_expressions_t,
                               "at least one argument is not a boolean expression in where()");
  SQLPP_PORTABLE_STATIC_ASSERT(assert_where_boolean_t, "at least one argument is not a boolean expression in where()");
  SQLPP_PORTABLE_STATIC_ASSERT(assert_where_no_aggregate_functions_t,
                               "at least one aggregate function used in where()");
  SQLPP_PORTABLE_STATIC_ASSERT(assert_where_static_count_args_t, "missing argument in where()");
  SQLPP_PORTABLE_STATIC_ASSERT(assert_where_dynamic_statement_dynamic_t,
                               "dynamic_where() must not be called in a static statement");

  template <typename... Expressions>
  using check_where_t = static_combined_check_t<
      static_check_t<logic::all_t<detail::is_expression_impl<Expressions>::type::value...>::value,
                     assert_where_expressions_t>,
      static_check_t<
          logic::all_t<std::is_same<typename detail::value_type_of_impl<Expressions>::type, boolean>::value...>::value,
          assert_where_boolean_t>,
      static_check_t<logic::all_t<(not detail::contains_aggregate_function_impl<Expressions>::type::value)...>::value,
                     assert_where_no_aggregate_functions_t>>;

  template <typename... Expressions>
  using check_where_static_t =
      static_combined_check_t<check_where_t<Expressions...>,
                              static_check_t<sizeof...(Expressions) != 0, assert_where_static_count_args_t>>;

  template <typename Database, typename... Expressions>
  using check_where_dynamic_t = static_combined_check_t<
      static_check_t<not std::is_same<Database, void>::value, assert_where_dynamic_statement_dynamic_t>,
      check_where_t<Expressions...>>;

  // NO WHERE YET
  template <bool WhereRequired>
  struct no_where_t
  {
    using _traits = make_traits<no_value_t, tag::is_where>;
    using _nodes = detail::type_vector<>;

    // Data
    using _data_t = no_data_t;

    // Member implementation with data and methods
    template <typename Policies>
    struct _impl_t
    {
      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      _impl_t() = default;
      _impl_t(const _data_t& data) : _data(data)
      {
      }

      _data_t _data;
    };

    // Base template to be inherited by the statement
    template <typename Policies>
    struct _base_t
    {
      using _data_t = no_data_t;

      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      template <typename... Args>
      _base_t(Args&&... args)
          : no_where{std::forward<Args>(args)...}
      {
      }

      _impl_t<Policies> no_where;
      _impl_t<Policies>& operator()()
      {
        return no_where;
      }
      const _impl_t<Policies>& operator()() const
      {
        return no_where;
      }

      template <typename T>
      static auto _get_member(T t) -> decltype(t.no_where)
      {
        return t.no_where;
      }

      using _database_t = typename Policies::_database_t;

      // workaround for msvc bug https://connect.microsoft.com/VisualStudio/Feedback/Details/2173269
      //	  template <typename... T>
      //	  using _check = logic::all_t<is_expression_t<T>::value...>;
      template <typename... T>
      struct _check : logic::all_t<is_expression_t<T>::value...>
      {
      };

      template <typename Check, typename T>
      using _new_statement_t = new_statement_t<Check::value, Policies, no_where_t, T>;

      using _consistency_check =
          typename std::conditional<WhereRequired and (Policies::_all_provided_tables::size::value > 0),
                                    assert_where_t,
                                    consistent_t>::type;

      auto where(bool b) const -> _new_statement_t<std::true_type, where_t<void, bool>>
      {
        return {static_cast<const derived_statement_t<Policies>&>(*this), where_data_t<void, bool>{b}};
      }

      template <typename... Expressions>
      auto where(Expressions... expressions) const
          -> _new_statement_t<check_where_static_t<Expressions...>, where_t<void, Expressions...>>
      {
        using Check = check_where_static_t<Expressions...>;
        Check{}._();

        return _where_impl<void>(Check{}, expressions...);
      }

      auto dynamic_where() const -> _new_statement_t<check_where_dynamic_t<_database_t>, where_t<_database_t>>
      {
        using Check = check_where_dynamic_t<_database_t>;
        Check{}._();

        return _where_impl<_database_t>(Check{});
      }

      template <typename... Expressions>
      auto dynamic_where(Expressions... expressions) const
          -> _new_statement_t<check_where_dynamic_t<_database_t, Expressions...>, where_t<_database_t, Expressions...>>
      {
        using Check = check_where_dynamic_t<_database_t, Expressions...>;
        Check{}._();

        return _where_impl<_database_t>(Check{}, expressions...);
      }

    private:
      template <typename Database, typename... Expressions>
      auto _where_impl(const std::false_type&, Expressions... expressions) const -> bad_statement;

      template <typename Database, typename... Expressions>
      auto _where_impl(const std::true_type&, Expressions... expressions) const
          -> _new_statement_t<std::true_type, where_t<Database, Expressions...>>
      {
        return {static_cast<const derived_statement_t<Policies>&>(*this),
                where_data_t<Database, Expressions...>{expressions...}};
      }
    };
  };

  // Interpreters
  template <typename Context, typename Database, typename... Expressions>
  struct serializer_t<Context, where_data_t<Database, Expressions...>>
  {
    using _serialize_check = serialize_check_of<Context, Expressions...>;
    using T = where_data_t<Database, Expressions...>;

    static Context& _(const T& t, Context& context)
    {
      if (sizeof...(Expressions) == 0 and t._dynamic_expressions.empty())
        return context;
      context << " WHERE ";
      interpret_tuple(t._expressions, " AND ", context);
      if (sizeof...(Expressions) and not t._dynamic_expressions.empty())
        context << " AND ";
      interpret_list(t._dynamic_expressions, " AND ", context);
      return context;
    }
  };

  template <typename Context>
  struct serializer_t<Context, where_data_t<void, bool>>
  {
    using _serialize_check = consistent_t;
    using T = where_data_t<void, bool>;

    static Context& _(const T& t, Context& context)
    {
      if (not t._condition)
        context << " WHERE NULL";
      return context;
    }
  };

  template <typename... T>
  auto where(T&&... t) -> decltype(statement_t<void, no_where_t<false>>().where(std::forward<T>(t)...))
  {
    return statement_t<void, no_where_t<false>>().where(std::forward<T>(t)...);
  }
}

#endif
