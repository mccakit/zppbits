export module zpp.bits:traits;
import std;
import :core;

export namespace zpp::bits
{
    namespace traits
    {
        template <typename Type> struct is_unique_ptr : std::false_type
        {
        };

        template <typename Type> struct is_unique_ptr<std::unique_ptr<Type, std::default_delete<Type>>> : std::true_type
        {
        };

        template <typename Type> struct is_shared_ptr : std::false_type
        {
        };

        template <typename Type> struct is_shared_ptr<std::shared_ptr<Type>> : std::true_type
        {
        };

        template <typename Variant> struct variant_impl;

        template <typename... Types, template <typename...> typename Variant> struct variant_impl<Variant<Types...>>
        {
                using variant_type = Variant<Types...>;

                template <std::size_t Index, std::size_t CurrentIndex, typename FirstType, typename... OtherTypes>
                constexpr static auto get_id()
                {
                    if constexpr (Index == CurrentIndex)
                    {
                        if constexpr (requires {
                                          requires std::same_as<serialization_id<FirstType::serialize_id::value>,
                                                                typename FirstType::serialize_id>;
                                      })
                        {
                            return FirstType::serialize_id::value;
                        }
                        else if constexpr (requires {
                                               requires std::same_as<serialization_id<decltype(serialize_id(
                                                                         std::declval<FirstType>()))::value>,
                                                                     decltype(serialize_id(std::declval<FirstType>()))>;
                                           })
                        {
                            return decltype(serialize_id(std::declval<FirstType>()))::value;
                        }
                        else
                        {
                            return std::byte {Index};
                        }
                    }
                    else
                    {
                        return get_id<Index, CurrentIndex + 1, OtherTypes...>();
                    }
                }

                template <std::size_t Index> constexpr static auto id()
                {
                    return get_id<Index, 0, Types...>();
                }

                template <std::size_t CurrentIndex = 0> constexpr static auto id(auto index)
                {
                    if constexpr (CurrentIndex == (sizeof...(Types) - 1))
                    {
                        return id<CurrentIndex>();
                    }
                    else
                    {
                        if (index == CurrentIndex)
                        {
                            return id<CurrentIndex>();
                        }
                        else
                        {
                            return id<CurrentIndex + 1>(index);
                        }
                    }
                }

                template <auto Id, std::size_t CurrentIndex = 0> constexpr static std::size_t index()
                {
                    static_assert(CurrentIndex < sizeof...(Types));

                    if constexpr (variant_impl::id<CurrentIndex>() == Id)
                    {
                        return CurrentIndex;
                    }
                    else
                    {
                        return index<Id, CurrentIndex + 1>();
                    }
                }

                template <std::size_t CurrentIndex = 0> constexpr static std::size_t index(auto &&id)
                {
                    if constexpr (CurrentIndex == sizeof...(Types))
                    {
                        return std::numeric_limits<std::size_t>::max();
                    }
                    else
                    {
                        if (variant_impl::id<CurrentIndex>() == id)
                        {
                            return CurrentIndex;
                        }
                        else
                        {
                            return index<CurrentIndex + 1>(id);
                        }
                    }
                    return std::numeric_limits<std::size_t>::max();
                }

                template <std::size_t... LeftIndices, std::size_t... RightIndices>
                constexpr static auto unique_ids(std::index_sequence<LeftIndices...>,
                                                 std::index_sequence<RightIndices...>)
                {
                    auto unique_among_rest = []<auto LeftIndex, auto LeftId>() {
                        return (... && ((LeftIndex == RightIndices) || (LeftId != id<RightIndices>())));
                    };
                    return (... && unique_among_rest.template operator()<LeftIndices, id<LeftIndices>()>());
                }

                template <std::size_t... LeftIndices, std::size_t... RightIndices>
                constexpr static auto same_id_types(std::index_sequence<LeftIndices...>,
                                                    std::index_sequence<RightIndices...>)
                {
                    auto same_among_rest = []<auto LeftIndex, auto LeftId>() {
                        return (... && (std::same_as<std::remove_cv_t<decltype(LeftId)>,
                                                     std::remove_cv_t<decltype(id<RightIndices>())>>));
                    };
                    return (... && same_among_rest.template operator()<LeftIndices, id<LeftIndices>()>());
                }

                template <typename Type, std::size_t... Indices>
                constexpr static std::size_t index_by_type(std::index_sequence<Indices...>)
                {
                    return ((std::same_as<Type, std::variant_alternative_t<Indices, variant_type>> * Indices) + ...);
                }

                template <typename Type> constexpr static std::size_t index_by_type()
                {
                    return index_by_type<Type>(std::make_index_sequence<std::variant_size_v<variant_type>> {});
                }

                using id_type = decltype(id<0>());
        };

        template <typename Variant> struct variant_checker;

        template <typename... Types, template <typename...> typename Variant> struct variant_checker<Variant<Types...>>
        {
                using type = variant_impl<Variant<Types...>>;
                static_assert(type::unique_ids(std::make_index_sequence<sizeof...(Types)>(),
                                               std::make_index_sequence<sizeof...(Types)>()));
                static_assert(type::same_id_types(std::make_index_sequence<sizeof...(Types)>(),
                                                  std::make_index_sequence<sizeof...(Types)>()));
        };

        template <typename Variant> using variant = typename variant_checker<Variant>::type;

        template <typename Tuple> struct tuple;

        template <typename... Types, template <typename...> typename Tuple> struct tuple<Tuple<Types...>>
        {
                template <std::size_t Index = 0> constexpr static auto visit(auto &&tuple, auto &&index, auto &&visitor)
                {
                    if constexpr (Index + 1 == sizeof...(Types))
                    {
                        return visitor(std::get<Index>(tuple));
                    }
                    else
                    {
                        if (Index == index)
                        {
                            return visitor(std::get<Index>(tuple));
                        }
                        return visit<Index + 1>(tuple, index, visitor);
                    }
                }
        };

        template <typename Type, typename Visitor = std::monostate> struct visitor
        {
                using byte_type = std::byte;
                using view_type = std::span<std::byte>;

                static constexpr bool resizable = false;

                constexpr auto operator()(auto &&...arguments) const
                {
                    if constexpr (requires { visitor(std::forward<decltype(arguments)>(arguments)...); })
                    {
                        return visitor(std::forward<decltype(arguments)>(arguments)...);
                    }
                    else
                    {
                        return sizeof...(arguments);
                    }
                }

                template <typename...> constexpr auto serialize_one(auto &&...arguments) const
                {
                    return (*this)(std::forward<decltype(arguments)>(arguments)...);
                }

                template <typename...> constexpr auto serialize_many(auto &&...arguments) const
                {
                    return (*this)(std::forward<decltype(arguments)>(arguments)...);
                }

                constexpr static auto kind()
                {
                    return kind::out;
                }

                std::span<std::byte> data();
                std::span<std::byte> remaining_data();
                std::span<std::byte> processed_data();
                std::size_t position() const;
                std::size_t &position();
                errc enlarge_for(std::size_t);
                void reset(std::size_t = 0);

                [[no_unique_address]] Visitor visitor;
        };

        constexpr auto get_default_size_type()
        {
            return default_size_type {};
        }

        constexpr auto get_default_size_type(auto option, auto... options)
        {
            if constexpr (requires { typename decltype(option)::default_size_type; })
            {
                if constexpr (std::is_void_v<typename decltype(option)::default_size_type>)
                {
                    return std::monostate {};
                }
                else
                {
                    return typename decltype(option)::default_size_type {};
                }
            }
            else
            {
                return get_default_size_type(options...);
            }
        }

        template <typename... Options>
        using default_size_type_t = std::conditional_t<
            std::same_as<std::monostate, decltype(get_default_size_type(std::declval<Options>()...))>,
            void,
            decltype(get_default_size_type(std::declval<Options>()...))>;

        template <typename Option, typename... Options> constexpr auto get_alloc_limit()
        {
            if constexpr (requires { std::remove_cvref_t<Option>::alloc_limit_value; })
            {
                return std::remove_cvref_t<Option>::alloc_limit_value;
            }
            else if constexpr (sizeof...(Options) != 0)
            {
                return get_alloc_limit<Options...>();
            }
            else
            {
                return std::numeric_limits<std::size_t>::max();
            }
        }

        template <typename... Options> constexpr auto alloc_limit()
        {
            if constexpr (sizeof...(Options) != 0)
            {
                return get_alloc_limit<Options...>();
            }
            else
            {
                return std::numeric_limits<std::size_t>::max();
            }
        }

        template <typename Option, typename... Options> constexpr auto get_enlarger()
        {
            if constexpr (requires { std::remove_cvref_t<Option>::enlarger_value; })
            {
                return std::remove_cvref_t<Option>::enlarger_value;
            }
            else if constexpr (sizeof...(Options) != 0)
            {
                return get_enlarger<Options...>();
            }
            else
            {
                return std::tuple {3, 2};
            }
        }

        template <typename... Options> constexpr auto enlarger()
        {
            if constexpr (sizeof...(Options) != 0)
            {
                return get_enlarger<Options...>();
            }
            else
            {
                return std::tuple {3, 2};
            }
        }

        template <typename Type> constexpr auto underlying_type_generic()
        {
            if constexpr (std::is_enum_v<Type>)
            {
                return std::underlying_type_t<Type> {};
            }
            else
            {
                return Type {};
            }
        }

        template <typename Type> using underlying_type_t = decltype(underlying_type_generic<Type>());

        template <typename Id> struct id_serializable
        {
                using serialize_id = Id;
        };

        constexpr auto unique(auto &&...values)
        {
            auto unique_among_rest = [](auto &&value, auto &&...values) {
                return (... && ((&value == &values) || (value != values)));
            };
            return (... && unique_among_rest(values, values...));
        }
    } // namespace traits
} // namespace zpp::bits
