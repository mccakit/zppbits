export module zpp.bits:core;
import std;

export namespace zpp::bits
{
    using default_size_type = std::uint32_t;

    enum class kind
    {
        in,
        out
    };

    template <std::size_t Count = std::numeric_limits<std::size_t>::max()> struct members
    {
            constexpr static std::size_t value = Count;
    };

    template <auto Protocol, std::size_t Members = std::numeric_limits<std::size_t>::max()> struct protocol
    {
            constexpr static auto value = Protocol;
            constexpr static auto members = Members;
    };

    template <auto Id> struct serialization_id
    {
            constexpr static auto value = Id;
    };

    constexpr auto success(std::errc code)
    {
        return std::errc {} == code;
    }

    constexpr auto failure(std::errc code)
    {
        return std::errc {} != code;
    }

    struct [[nodiscard]] errc
    {
            constexpr errc(std::errc code = {}) : code(code)
            {
            }

            constexpr operator std::errc() const
            {
                return code;
            }

            constexpr void or_throw() const
            {
                if (failure(code)) [[unlikely]]
                {
                    throw std::system_error(std::make_error_code(code));
                }
            }

            std::errc code;
    };

    constexpr auto success(errc code)
    {
        return std::errc {} == code;
    }

    constexpr auto failure(errc code)
    {
        return std::errc {} != code;
    }

    struct access
    {
            struct any
            {
                    template <typename Type> operator Type();
            };

            template <typename Item> constexpr static auto make(auto &&...arguments)
            {
                return Item {std::forward<decltype(arguments)>(arguments)...};
            }

            template <typename Item> constexpr static auto placement_new(void *address, auto &&...arguments)
            {
                return ::new (address) Item(std::forward<decltype(arguments)>(arguments)...);
            }

            template <typename Item> constexpr static auto make_unique(auto &&...arguments)
            {
                return std::unique_ptr<Item>(new Item(std::forward<decltype(arguments)>(arguments)...));
            }

            template <typename Item> constexpr static void destruct(Item &item)
            {
                item.~Item();
            }

            template <typename Type> constexpr static auto number_of_members();

            constexpr static decltype(auto) visit_members(auto &&object, auto &&visitor)
            {
                auto &&[... members] = object;
                return visitor(members...);
            }

            template <typename Type> constexpr static auto visit_members_types(auto &&visitor)
            {
                using type = std::remove_cvref_t<Type>;

                auto f = [&](auto &&object) {
                    auto &&[... members] = object;
                    return visitor.template operator()<decltype(members)...>();
                };
                return decltype(f(std::declval<type>()))();
            }

            constexpr static auto try_serialize(auto &&item)
            {
                if constexpr (requires { serialize(item); })
                {
                    return serialize(item);
                }
            }

            template <typename Type, typename Archive> constexpr static auto has_serialize()
            {
                return requires {
                    requires std::same_as<typename std::remove_cvref_t<Type>::serialize,
                                          members<std::remove_cvref_t<Type>::serialize::value>>;
                } || requires(Type &&item) {
                    requires std::same_as<std::remove_cvref_t<decltype(try_serialize(item))>,
                                          members<std::remove_cvref_t<decltype(try_serialize(item))>::value>>;
                } || requires {
                    requires std::same_as<typename std::remove_cvref_t<Type>::serialize,
                                          protocol<std::remove_cvref_t<Type>::serialize::value,
                                                   std::remove_cvref_t<Type>::serialize::members>>;
                } || requires(Type &&item) {
                    requires std::same_as<std::remove_cvref_t<decltype(try_serialize(item))>,
                                          protocol<std::remove_cvref_t<decltype(try_serialize(item))>::value,
                                                   std::remove_cvref_t<decltype(try_serialize(item))>::members>>;
                } || requires(Type &&item, Archive &&archive) {
                    std::remove_cvref_t<Type>::serialize(archive, item);
                } || requires(Type &&item, Archive &&archive) { serialize(archive, item); };
            }

            template <typename Type, typename Archive> constexpr static auto has_explicit_serialize()
            {
                return requires(Type &&item, Archive &&archive) {
                    std::remove_cvref_t<Type>::serialize(archive, item);
                } || requires(Type &&item, Archive &&archive) { serialize(archive, item); };
            }

            template <typename Type> struct byte_serializable_visitor;

            template <typename Type> constexpr static auto byte_serializable();

            template <typename Type> struct endian_independent_byte_serializable_visitor;

            template <typename Type> constexpr static auto endian_independent_byte_serializable();

            template <typename Type, typename Self, typename... Visited> struct self_referencing_visitor;

            template <typename Type, typename Self = Type, typename... Visited>
            constexpr static auto self_referencing();

            template <typename Type> constexpr static auto has_protocol()
            {
                return requires {
                    requires std::same_as<typename std::remove_cvref_t<Type>::serialize,
                                          protocol<std::remove_cvref_t<Type>::serialize::value,
                                                   std::remove_cvref_t<Type>::serialize::members>>;
                } || requires(Type &&item) {
                    requires std::same_as<std::remove_cvref_t<decltype(try_serialize(item))>,
                                          protocol<std::remove_cvref_t<decltype(try_serialize(item))>::value,
                                                   std::remove_cvref_t<decltype(try_serialize(item))>::members>>;
                };
            }

            template <typename Type> constexpr static auto get_protocol()
            {
                if constexpr (requires {
                                  requires std::same_as<typename std::remove_cvref_t<Type>::serialize,
                                                        protocol<std::remove_cvref_t<Type>::serialize::value,
                                                                 std::remove_cvref_t<Type>::serialize::members>>;
                              })
                {
                    return std::remove_cvref_t<Type>::serialize::value;
                }
                else if constexpr (requires(Type &&item) {
                                       requires std::same_as<
                                           std::remove_cvref_t<decltype(try_serialize(item))>,
                                           protocol<std::remove_cvref_t<decltype(try_serialize(item))>::value,
                                                    std::remove_cvref_t<decltype(try_serialize(item))>::members>>;
                                   })
                {
                    return std::remove_cvref_t<decltype(try_serialize(std::declval<Type>()))>::value;
                }
                else
                {
                    static_assert(!sizeof(Type));
                }
            }
    };

    template <typename Type> struct destructor_guard
    {
            constexpr ~destructor_guard()
            {
                access::destruct(object);
            }

            Type &object;
    };

    template <typename Type> destructor_guard(Type) -> destructor_guard<Type>;
} // namespace zpp::bits
