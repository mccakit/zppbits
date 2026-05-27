export module zpp.bits:rpc;
import std;
import :core;
import :traits;
import :concepts;
import :options;
import :varint;
import :archive;
import :io;

export namespace zpp::bits
{
    template <typename Function> struct function_traits;

    template <typename Return, typename... Arguments> struct function_traits<Return (*)(Arguments...)>
    {
            using parameters_type = std::tuple<std::remove_cvref_t<Arguments>...>;
            using return_type = Return;
    };

    template <typename Return, typename... Arguments> struct function_traits<Return (*)(Arguments...) noexcept>
    {
            using parameters_type = std::tuple<std::remove_cvref_t<Arguments>...>;
            using return_type = Return;
    };

    template <typename This, typename Return, typename... Arguments>
    struct function_traits<Return (This::*)(Arguments...)>
    {
            using parameters_type = std::tuple<std::remove_cvref_t<Arguments>...>;
            using return_type = Return;
    };

    template <typename This, typename Return, typename... Arguments>
    struct function_traits<Return (This::*)(Arguments...) noexcept>
    {
            using parameters_type = std::tuple<std::remove_cvref_t<Arguments>...>;
            using return_type = Return;
    };

    template <typename This, typename Return, typename... Arguments>
    struct function_traits<Return (This::*)(Arguments...) const>
    {
            using parameters_type = std::tuple<std::remove_cvref_t<Arguments>...>;
            using return_type = Return;
    };

    template <typename This, typename Return, typename... Arguments>
    struct function_traits<Return (This::*)(Arguments...) const noexcept>
    {
            using parameters_type = std::tuple<std::remove_cvref_t<Arguments>...>;
            using return_type = Return;
    };

    template <typename Return> struct function_traits<Return (*)()>
    {
            using parameters_type = void;
            using return_type = Return;
    };

    template <typename Return> struct function_traits<Return (*)() noexcept>
    {
            using parameters_type = void;
            using return_type = Return;
    };

    template <typename This, typename Return> struct function_traits<Return (This::*)()>
    {
            using parameters_type = void;
            using return_type = Return;
    };

    template <typename This, typename Return> struct function_traits<Return (This::*)() noexcept>
    {
            using parameters_type = void;
            using return_type = Return;
    };

    template <typename This, typename Return> struct function_traits<Return (This::*)() const>
    {
            using parameters_type = void;
            using return_type = Return;
    };

    template <typename This, typename Return> struct function_traits<Return (This::*)() const noexcept>
    {
            using parameters_type = void;
            using return_type = Return;
    };

    template <typename Function>
    using function_parameters_t = typename function_traits<std::remove_cvref_t<Function>>::parameters_type;

    template <typename Function>
    using function_return_type_t = typename function_traits<std::remove_cvref_t<Function>>::return_type;

    constexpr auto success(auto &&value_or_errc)
        requires std::same_as<decltype(value_or_errc.error()), errc>
    {
        return value_or_errc.success();
    }

    constexpr auto failure(auto &&value_or_errc)
        requires std::same_as<decltype(value_or_errc.error()), errc>
    {
        return value_or_errc.failure();
    }

    template <typename Type> struct [[nodiscard]] value_or_errc
    {
            using error_type = errc;
            using value_type = std::conditional_t<
                std::is_void_v<Type>,
                std::nullptr_t,
                std::conditional_t<std::is_reference_v<Type>, std::add_pointer_t<std::remove_reference_t<Type>>, Type>>;

            constexpr value_or_errc() = default;

            constexpr explicit value_or_errc(auto &&value) : m_return_value(std::forward<decltype(value)>(value))
            {
            }

            constexpr explicit value_or_errc(error_type error)
                : m_error(std::forward<decltype(error)>(error)), m_failure(true)
            {
            }

            constexpr value_or_errc(value_or_errc &&other) noexcept
            {
                if (other.success())
                {
                    if constexpr (!std::is_void_v<Type>)
                    {
                        if constexpr (!std::is_reference_v<Type>)
                        {
                            ::new (std::addressof(m_return_value)) Type(std::move(other.m_return_value));
                        }
                        else
                        {
                            m_return_value = other.m_return_value;
                        }
                    }
                }
                else
                {
                    m_failure = other.m_failure;
                    std::memcpy(&m_error, &other.m_error, sizeof(m_error));
                }
            }

            constexpr ~value_or_errc()
            {
                if constexpr (!std::is_void_v<Type> && !std::is_trivially_destructible_v<Type>)
                {
                    if (success())
                    {
                        m_return_value.~Type();
                    }
                }
            }

            constexpr bool success() const noexcept
            {
                return !m_failure;
            }

            constexpr bool failure() const noexcept
            {
                return m_failure;
            }

            constexpr decltype(auto) value() & noexcept
            {
                if constexpr (std::is_same_v<Type, decltype(m_return_value)>)
                {
                    return (m_return_value);
                }
                else
                {
                    return (*m_return_value);
                }
            }

            constexpr decltype(auto) value() && noexcept
            {
                if constexpr (std::is_same_v<Type, decltype(m_return_value)>)
                {
                    return std::forward<Type>(m_return_value);
                }
                else
                {
                    return std::forward<Type>(*m_return_value);
                }
            }

            constexpr decltype(auto) value() const & noexcept
            {
                if constexpr (std::is_same_v<Type, decltype(m_return_value)>)
                {
                    return (m_return_value);
                }
                else
                {
                    return (*m_return_value);
                }
            }

            constexpr auto error() const noexcept
            {
                return m_error;
            }

            constexpr decltype(auto) or_throw() &
            {
                if (failure()) [[unlikely]]
                {
                    throw std::system_error(std::make_error_code(error().code));
                }
                return value();
            }

            constexpr decltype(auto) or_throw() &&
            {
                if (failure()) [[unlikely]]
                {
                    throw std::system_error(std::make_error_code(error().code));
                }
                return std::move(*this).value();
            }

            constexpr decltype(auto) or_throw() const &
            {
                if (failure()) [[unlikely]]
                {
                    throw std::system_error(std::make_error_code(error().code));
                }
                return value();
            }

            union {
                    error_type m_error {};
                    value_type m_return_value;
            };
            bool m_failure {};
    };

    constexpr auto apply(auto &&function, auto &&archive)
        requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::in)
    {
        using function_type = std::decay_t<decltype(function)>;

        if constexpr (requires { &function_type::operator(); })
        {
            using parameters_type = function_parameters_t<decltype(&function_type::operator())>;
            using return_type = function_return_type_t<decltype(&function_type::operator())>;
            if constexpr (std::is_void_v<parameters_type>)
            {
                return std::forward<decltype(function)>(function)();
            }
            else
            {
                parameters_type parameters;
                if constexpr (std::is_void_v<return_type>)
                {
                    if (auto result = archive(parameters); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    std::apply(std::forward<decltype(function)>(function), std::move(parameters));
                    return errc {};
                }
                else
                {
                    if (auto result = archive(parameters); failure(result)) [[unlikely]]
                    {
                        return value_or_errc<return_type> {result};
                    }
                    return value_or_errc<return_type> {
                        std::apply(std::forward<decltype(function)>(function), std::move(parameters))};
                }
            }
        }
        else
        {
            using parameters_type = function_parameters_t<function_type>;
            using return_type = function_return_type_t<function_type>;
            if constexpr (std::is_void_v<parameters_type>)
            {
                return std::forward<decltype(function)>(function)();
            }
            else
            {
                parameters_type parameters;
                if constexpr (std::is_void_v<return_type>)
                {
                    if (auto result = archive(parameters); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    std::apply(std::forward<decltype(function)>(function), std::move(parameters));
                    return errc {};
                }
                else
                {
                    if (auto result = archive(parameters); failure(result)) [[unlikely]]
                    {
                        return value_or_errc<return_type> {result};
                    }
                    return value_or_errc<return_type> {
                        std::apply(std::forward<decltype(function)>(function), std::move(parameters))};
                }
            }
        }
    }

    constexpr auto apply(auto &&self, auto &&function, auto &&archive)
        requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::in)
    {
        using parameters_type = function_parameters_t<std::remove_cvref_t<decltype(function)>>;
        using return_type = function_return_type_t<std::remove_cvref_t<decltype(function)>>;
        if constexpr (std::is_void_v<parameters_type>)
        {
            return (std::forward<decltype(self)>(self).*std::forward<decltype(function)>(function))();
        }
        else
        {
            parameters_type parameters;
            if constexpr (std::is_void_v<return_type>)
            {
                if (auto result = archive(parameters); failure(result)) [[unlikely]]
                {
                    return result;
                }
                // Ignore GCC issue.
                std::apply(
                    [&](auto &&...arguments) -> decltype(auto) {
                        return (std::forward<decltype(self)>(self).*std::forward<decltype(function)>(function))(
                            std::forward<decltype(arguments)>(arguments)...);
                    },
                    std::move(parameters));
                return errc {};
            }
            else
            {
                if (auto result = archive(parameters); failure(result)) [[unlikely]]
                {
                    return value_or_errc<return_type> {result};
                }
                return value_or_errc<return_type>(std::apply(
                    [&](auto &&...arguments) -> decltype(auto) {
                        // Ignore GCC issue.
                        return (std::forward<decltype(self)>(self).*std::forward<decltype(function)>(function))(
                            std::forward<decltype(arguments)>(arguments)...);
                    },
                    std::move(parameters)));
            }
        }
    }

    template <auto Function, auto Id, auto MaxSize = -1> struct bind
    {
            using id = zpp::bits::id<Id, MaxSize>;
            using function_type = decltype(Function);
            using parameters_type = typename function_traits<function_type>::parameters_type;
            using return_type = typename function_traits<function_type>::return_type;
            static constexpr auto opaque = false;

            constexpr static decltype(auto) call(auto &&archive, auto &&context)
            {
                if constexpr (std::is_member_function_pointer_v<std::remove_cvref_t<decltype(Function)>>)
                {
                    return apply(context, Function, archive);
                }
                else
                {
                    return apply(Function, archive);
                }
            }
    };

    template <auto Function, auto Id, auto MaxSize = -1> struct bind_opaque
    {
            using id = zpp::bits::id<Id, MaxSize>;
            using function_type = decltype(Function);
            using parameters_type = typename function_traits<function_type>::parameters_type;
            using return_type = typename function_traits<function_type>::return_type;
            static constexpr auto opaque = true;

            constexpr static decltype(auto) call(auto &&in, auto &&out, auto &&context)
            {
                if constexpr (std::is_member_function_pointer_v<std::remove_cvref_t<decltype(Function)>>)
                {
                    if constexpr (requires { (context.*Function)(in, out); })
                    {
                        return (context.*Function)(in, out);
                    }
                    else if constexpr (requires { (context.*Function)(in); })
                    {
                        return (context.*Function)(in);
                    }
                    else if constexpr (requires { (context.*Function)(out); })
                    {
                        return (context.*Function)(out);
                    }
                    else if constexpr (requires(decltype(in.remaining_data()) &data) { (context.*Function)(data); })
                    {
                        struct guard
                        {
                                decltype(in) archive;
                                decltype(in.remaining_data()) data;
                                constexpr ~guard()
                                {
                                    archive.position() += data.size();
                                }
                        } guard {in, in.remaining_data()};
                        return (context.*Function)(guard.data);
                    }
                    else
                    {
                        return (context.*Function)();
                    }
                }
                else
                {
                    if constexpr (requires { Function(in, out); })
                    {
                        return Function(in, out);
                    }
                    else if constexpr (requires { Function(in); })
                    {
                        return Function(in);
                    }
                    else if constexpr (requires { Function(out); })
                    {
                        return Function(out);
                    }
                    else if constexpr (requires(decltype(in.remaining_data()) &data) { Function(data); })
                    {
                        struct guard
                        {
                                decltype(in) archive;
                                decltype(in.remaining_data()) data;
                                constexpr ~guard()
                                {
                                    archive.position() += data.size();
                                }
                        } guard {in, in.remaining_data()};
                        return Function(guard.data);
                    }
                    else
                    {
                        return Function();
                    }
                }
            }
    };

    template <typename... Bindings> struct rpc_impl
    {
            using id = std::remove_cvref_t<
                decltype(std::remove_cvref_t<decltype(get<0>(std::tuple<Bindings...> {}))>::id::value)>;

            template <typename In, typename Out> struct client
            {
                    constexpr client(In &&in, Out &&out) : in(in), out(out)
                    {
                    }

                    constexpr client(client &&other) = default;

                    constexpr ~client()
                    {
                        static_assert(std::remove_cvref_t<decltype(in)>::kind() == kind::in);
                        static_assert(std::remove_cvref_t<decltype(out)>::kind() == kind::out);
                    }

                    template <typename Id, typename FirstBinding, typename... OtherBindings> constexpr auto binding()
                    {
                        if constexpr (std::same_as<Id, typename FirstBinding::id>)
                        {
                            return FirstBinding {};
                        }
                        else
                        {
                            static_assert(sizeof...(OtherBindings));
                            return binding<Id, OtherBindings...>();
                        }
                    }

                    template <typename Id, std::size_t... Indices>
                    constexpr auto request(std::index_sequence<Indices...>, auto &&...arguments)
                    {
                        using request_binding = decltype(binding<Id, Bindings...>());
                        using parameters_type = typename request_binding::parameters_type;

                        if constexpr (std::is_void_v<parameters_type>)
                        {
                            static_assert(!sizeof...(arguments));
                            return out(Id::value);
                        }
                        else if constexpr (request_binding::opaque)
                        {
                            return out(Id::value, arguments...);
                            ;
                        }
                        else if constexpr (std::same_as<std::tuple<std::remove_cvref_t<decltype(arguments)>...>,
                                                        parameters_type>

                        )
                        {
                            return out(Id::value, arguments...);
                        }
                        else
                        {
                            static_assert(requires {
                                {
                                    parameters_type
                                    {
                                        std::forward_as_tuple<decltype(arguments)...>(arguments...)
                                    }
                                };
                            });

                            return out(
                                Id::value,
                                static_cast<std::conditional_t<
                                    std::is_fundamental_v<
                                        std::remove_cvref_t<decltype(get<Indices>(std::declval<parameters_type>()))>> ||
                                        std::is_enum_v<std::remove_cvref_t<decltype(get<Indices>(
                                            std::declval<parameters_type>()))>>,
                                    std::remove_cvref_t<decltype(get<Indices>(std::declval<parameters_type>()))>,
                                    const decltype(get<Indices>(std::declval<parameters_type>())) &>>(arguments)...);
                        }
                    }

                    template <typename Id> constexpr auto request(auto &&...arguments)
                    {
                        return request<Id>(std::make_index_sequence<sizeof...(arguments)> {}, arguments...);
                    }

                    template <auto Id, auto MaxSize = -1> constexpr auto request(auto &&...arguments)
                    {
                        return request<zpp::bits::id<Id, MaxSize>>(arguments...);
                    }

                    template <typename Id, std::size_t... Indices>
                    constexpr auto request_body(std::index_sequence<Indices...>, auto &&...arguments)
                    {
                        using request_binding = decltype(binding<Id, Bindings...>());
                        using parameters_type = typename request_binding::parameters_type;

                        if constexpr (std::is_void_v<parameters_type>)
                        {
                            static_assert(!sizeof...(arguments));
                            return;
                        }
                        else if constexpr (request_binding::opaque)
                        {
                            return out(arguments...);
                            ;
                        }
                        else if constexpr (std::same_as<std::tuple<std::remove_cvref_t<decltype(arguments)>...>,
                                                        parameters_type>

                        )
                        {
                            return out(arguments...);
                        }
                        else
                        {
                            static_assert(requires {
                                {
                                    parameters_type
                                    {
                                        std::forward_as_tuple<decltype(arguments)...>(arguments...)
                                    }
                                };
                            });

                            return out(
                                static_cast<std::conditional_t<
                                    std::is_fundamental_v<
                                        std::remove_cvref_t<decltype(get<Indices>(std::declval<parameters_type>()))>> ||
                                        std::is_enum_v<std::remove_cvref_t<decltype(get<Indices>(
                                            std::declval<parameters_type>()))>>,
                                    std::remove_cvref_t<decltype(get<Indices>(std::declval<parameters_type>()))>,
                                    const decltype(get<Indices>(std::declval<parameters_type>())) &>>(arguments)...);
                        }
                    }

                    template <typename Id> constexpr auto request_body(auto &&...arguments)
                    {
                        return request_body<Id>(std::make_index_sequence<sizeof...(arguments)> {}, arguments...);
                    }

                    template <auto Id, auto MaxSize = -1> constexpr auto request_body(auto &&...arguments)
                    {
                        return request_body<zpp::bits::id<Id, MaxSize>>(arguments...);
                    }

                    template <typename Id> constexpr auto response()
                    {
                        using request_binding = decltype(binding<Id, Bindings...>());
                        using return_type = typename request_binding::return_type;

                        if constexpr (std::is_void_v<return_type>)
                        {
                            return;
                        }
                        else
                        {
                            return_type return_value;
                            if (auto result = in(return_value); failure(result)) [[unlikely]]
                            {
                                return value_or_errc<return_type> {result};
                            }
                            return value_or_errc<return_type> {std::move(return_value)};
                        }
                    }

                    template <auto Id, auto MaxSize = -1> constexpr auto response()
                    {
                        return response<zpp::bits::id<Id, MaxSize>>();
                    }

                    In &in;
                    Out &out;
            };

            template <typename... Types> client(Types &&...) -> client<Types &&...>;

            template <typename In, typename Out, typename Context = std::monostate> struct server
            {
                    constexpr server(In &&in, Out &&out) : in(in), out(out)
                    {
                    }

                    constexpr server(In &&in, Out &&out, Context &&context) : in(in), out(out), context(context)
                    {
                    }

                    constexpr server(server &&other) = default;

                    constexpr ~server()
                    {
                        static_assert(std::remove_cvref_t<decltype(in)>::kind() == kind::in);
                        static_assert(std::remove_cvref_t<decltype(out)>::kind() == kind::out);
                    }

                    template <typename FirstBinding, typename... OtherBindings>
                    constexpr auto call_binding(auto &id)
                        requires(!FirstBinding::opaque)
                    {
                        if (FirstBinding::id::value == id)
                        {
                            if constexpr (std::is_void_v<decltype(FirstBinding::call(in, context))>)
                            {
                                FirstBinding::call(in, context);
                                return errc {};
                            }
                            else if constexpr (std::same_as<decltype(FirstBinding::call(in, context)), errc>)
                            {
                                if (auto result = FirstBinding::call(in, context); failure(result)) [[unlikely]]
                                {
                                    return result;
                                }
                                return errc {};
                            }
                            else if constexpr (std::is_void_v<typename FirstBinding::parameters_type>)
                            {
                                return out(FirstBinding::call(in, context));
                            }
                            else
                            {
                                if (auto result = FirstBinding::call(in, context); failure(result)) [[unlikely]]
                                {
                                    return result.error();
                                }
                                else
                                {
                                    return out(result.value());
                                }
                            }
                        }
                        else
                        {
                            if constexpr (!sizeof...(OtherBindings))
                            {
                                return errc {std::errc::not_supported};
                            }
                            else
                            {
                                return call_binding<OtherBindings...>(id);
                            }
                        }
                    }

                    template <typename FirstBinding, typename... OtherBindings>
                    constexpr auto call_binding(auto &id)
                        requires FirstBinding::opaque
                    {
                        if (FirstBinding::id::value == id)
                        {
                            if constexpr (std::is_void_v<decltype(FirstBinding::call(in, out, context))>)
                            {
                                FirstBinding::call(in, out, context);
                                return errc {};
                            }
                            else if constexpr (std::same_as<decltype(FirstBinding::call(in, out, context)), errc>)
                            {
                                if (auto result = FirstBinding::call(in, out, context); failure(result)) [[unlikely]]
                                {
                                    return result;
                                }
                                return errc {};
                            }
                            else if constexpr (requires {
                                                   requires std::same_as<
                                                       typename decltype(FirstBinding::call(
                                                           in, out, context))::value_type,
                                                       value_or_errc<decltype(FirstBinding::call(in, out, context))>>;
                                               })
                            {
                                if (auto result = FirstBinding::call(in, out, context); failure(result)) [[unlikely]]
                                {
                                    return result.error();
                                }
                                else
                                {
                                    return out(result.value());
                                }
                            }
                            else
                            {
                                return out(FirstBinding::call(in, out, context));
                            }
                        }
                        else
                        {
                            if constexpr (!sizeof...(OtherBindings))
                            {
                                return errc {std::errc::not_supported};
                            }
                            else
                            {
                                return call_binding<OtherBindings...>(id);
                            }
                        }
                    }

                    constexpr auto serve(auto &&id)
                    {
                        return call_binding<Bindings...>(id);
                    }

                    constexpr auto serve()
                    {
                        rpc_impl::id id;
                        if (auto result = in(id); failure(result)) [[unlikely]]
                        {
                            return decltype(serve(rpc_impl::id {})) {result.code};
                        }

                        return serve(id);
                    }

                    In &in;
                    Out &out;
                    [[no_unique_address]] Context context;
            };

            template <typename... Types> server(Types &&...) -> server<Types &&...>;

            constexpr static auto client_server(auto &&in, auto &&out, auto &&...context)
            {
                return std::tuple {client {in, out}, server {in, out, context...}};
            }
    };

    template <typename... Bindings> struct rpc_checker
    {
            using check_unique_id = traits::variant<std::variant<traits::id_serializable<typename Bindings::id>...>>;
            using type = rpc_impl<Bindings...>;
    };

    template <typename... Bindings> using rpc = typename rpc_checker<Bindings...>::type;
} // namespace zpp::bits
