export module zpp.bits:protobuf;
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
    struct pb_reserved
    {
    };

    template <std::size_t From, std::size_t To> struct pb_map
    {
            static_assert(From != 0 && To != 0);

            constexpr static unsigned int mapped_field(auto index)
            {
                return ((index + 1) == From) ? To : 0u;
            }
    };

    template <typename Type, auto FieldNumber> struct pb_field_fundamental
    {
            using value_type = Type;
            using pb_field_type = Type;

            constexpr static auto pb_field_number = FieldNumber;

            constexpr pb_field_fundamental() = default;

            constexpr pb_field_fundamental(Type value) : value(value)
            {
            }

            constexpr operator Type &() &
            {
                return value;
            }

            constexpr operator Type() const
            {
                return value;
            }

            Type value {};
    };

    template <typename Type, auto FieldNumber>
    constexpr decltype(auto) pb_value(pb_field_fundamental<Type, FieldNumber> &pb)
    {
        return static_cast<typename pb_field_fundamental<Type, FieldNumber>::pb_field_type &>(pb);
    }

    template <typename Type, auto FieldNumber>
    constexpr auto pb_value(const pb_field_fundamental<Type, FieldNumber> &pb)
    {
        return static_cast<typename pb_field_fundamental<Type, FieldNumber>::pb_field_type>(pb);
    }

    template <typename Type, auto FieldNumber> struct pb_field_struct : Type
    {
            using Type::Type;
            using Type::operator=;
            using pb_field_type = Type;

            static constexpr auto pb_field_number = FieldNumber;

            constexpr pb_field_struct(Type &&other) noexcept(std::is_nothrow_move_constructible_v<Type>)
                : Type(std::move(other))
            {
            }

            constexpr pb_field_struct(const Type &other) : Type(other)
            {
            }
    };

    template <typename Type, auto FieldNumber> constexpr decltype(auto) pb_value(pb_field_struct<Type, FieldNumber> &pb)
    {
        return static_cast<typename pb_field_struct<Type, FieldNumber>::pb_field_type &>(pb);
    }

    template <typename Type, auto FieldNumber>
    constexpr decltype(auto) pb_value(const pb_field_struct<Type, FieldNumber> &pb)
    {
        return static_cast<const typename pb_field_struct<Type, FieldNumber>::pb_field_type &>(pb);
    }

    template <typename Type, auto FieldNumber>
    constexpr decltype(auto) pb_value(pb_field_struct<Type, FieldNumber> &&pb)
    {
        return static_cast<typename pb_field_struct<Type, FieldNumber>::pb_field_type &&>(pb);
    }

    template <typename Type, auto FieldNumber>
    using pb_field = std::conditional_t<std::is_class_v<Type>,
                                        pb_field_struct<Type, FieldNumber>,
                                        pb_field_fundamental<Type, FieldNumber>>;

    template <typename... Options> struct pb
    {
            using pb_default = pb<>;

            constexpr pb(Options &&...)
            {
            }

            template <std::size_t Index> constexpr static auto has_mapped_field(auto option)
            {
                return requires { requires decltype(option)::mapped_field(Index) != 0; };
            }

            template <std::size_t Index> constexpr static auto get_mapped_field(auto option)
            {
                if constexpr (requires { requires decltype(option)::mapped_field(Index) != 0; })
                {
                    return decltype(option)::mapped_field(Index);
                }
                else
                {
                    return 0u;
                }
            }

            template <std::size_t Index> struct field_number_visitor
            {
                    template <typename... Types> constexpr auto operator()() const
                    {
                        if constexpr (requires {
                                          std::remove_cvref_t<decltype(std::get<Index>(
                                              std::declval<std::tuple<Types...>>()))>::pb_field_number;
                                      })
                        {
                            static_assert(0 != std::remove_cvref_t<decltype(std::get<Index>(
                                                   std::declval<std::tuple<Types...>>()))>::pb_field_number);
                            return std::integral_constant<
                                unsigned int,
                                std::remove_cvref_t<decltype(std::get<Index>(
                                    std::declval<std::tuple<Types...>>()))>::pb_field_number>();
                        }
                        else
                        {
                            return std::integral_constant<unsigned int, 0> {};
                        }
                    }
            };

            template <typename Type, std::size_t Index> constexpr static auto field_number_from_struct()
            {
                constexpr auto explicit_field_number = visit_members_types<Type>(field_number_visitor<Index> {})();
                if constexpr (explicit_field_number > 0)
                {
                    return explicit_field_number;
                }
                else
                {
                    static_assert((0 + ... + std::size_t(has_mapped_field<Index>(Options {}))) <= 1);

                    constexpr auto mapped_field = (0 + ... + get_mapped_field<Index>(Options {}));
                    if constexpr (mapped_field != 0)
                    {
                        return mapped_field;
                    }
                    else
                    {
                        return Index + 1;
                    }
                }
            }

            template <typename Type, std::size_t Index> constexpr static auto field_number()
            {
                if constexpr (requires { requires(Type::pb_field_number > 0); })
                {
                    return Type::pb_field_number;
                }
                else
                {
                    static_assert((0 + ... + std::size_t(has_mapped_field<Index>(Options {}))) <= 1);

                    constexpr auto mapped_field = (0 + ... + get_mapped_field<Index>(Options {}));
                    if constexpr (mapped_field != 0)
                    {
                        return mapped_field;
                    }
                    else
                    {
                        return Index + 1;
                    }
                }
            }

            template <typename Type, std::size_t... Indices>
            constexpr static auto unique_field_numbers(std::index_sequence<Indices...>)
            {
                return traits::unique(std::size_t {field_number_from_struct<Type, Indices>()}...);
            }

            template <typename Type> constexpr static auto unique_field_numbers()
            {
                constexpr auto members = number_of_members<std::remove_cvref_t<Type>>();
                if constexpr (members >= 0)
                {
                    return unique_field_numbers<std::remove_cvref_t<Type>>(std::make_index_sequence<members>());
                }
                else
                {
                    static_assert(members >= 0);
                }
            }

            template <typename Type> constexpr static auto is_pb_field()
            {
                using type = std::remove_cvref_t<Type>;
                return requires {
                    requires std::same_as<type, pb_field<typename type::pb_field_type, type::pb_field_number>>;
                };
            }

            template <typename Type> constexpr static auto check_type()
            {
                using type = std::remove_cvref_t<Type>;
                if constexpr (is_pb_field<type>())
                {
                    return check_type<typename type::pb_field_type>();
                }
                else if constexpr (!std::is_class_v<type> || concepts::varint<type> || concepts::empty<type>)
                {
                    return true;
                }
                else if constexpr (concepts::associative_container<type> && requires { typename type::mapped_type; })
                {
                    static_assert(
                        requires { type {}.push_back(typename type::value_type {}); } ||
                        requires { type {}.insert(typename type::value_type {}); });
                    static_assert(check_type<typename type::key_type>());
                    static_assert(check_type<typename type::mapped_type>());
                    return true;
                }
                else if constexpr (concepts::container<type>)
                {
                    static_assert(
                        requires { type {}.push_back(typename type::value_type {}); } ||
                        requires { type {}.insert(typename type::value_type {}); });
                    static_assert(check_type<typename type::value_type>());
                    return true;
                }
                else if constexpr (concepts::by_protocol<type>)
                {
                    static_assert(
                        std::same_as<pb_default, typename decltype(access::get_protocol<type>())::pb_default>);
                    static_assert(unique_field_numbers<type>());
                    return true;
                }
                else
                {
                    static_assert(!sizeof(Type));
                }
            }

            enum class wire_type : unsigned int
            {
                varint = 0,
                fixed_64 = 1,
                length_delimited = 2,
                fixed_32 = 5,
            };

            constexpr static auto make_tag_explicit(wire_type type, auto field_number)
            {
                return varint {(field_number << 3) | std::underlying_type_t<wire_type>(type)};
            }

            constexpr static auto tag_type(auto tag)
            {
                return wire_type(tag & 0x7);
            }

            constexpr static auto tag_number(auto tag)
            {
                return (unsigned int)(tag >> 3);
            }

            template <typename Type> constexpr static auto tag_type()
            {
                using type = std::remove_cvref_t<Type>;
                if constexpr (is_pb_field<type>())
                {
                    return tag_type<typename type::pb_field_type>();
                }
                else if constexpr (concepts::varint<type> || (std::is_enum_v<type> && !std::same_as<type, std::byte>) ||
                                   std::same_as<type, bool>)
                {
                    return wire_type::varint;
                }
                else if constexpr (std::is_integral_v<type> || std::is_floating_point_v<type>)
                {
                    if constexpr (sizeof(type) == 4)
                    {
                        return wire_type::fixed_32;
                    }
                    else if constexpr (sizeof(type) == 8)
                    {
                        return wire_type::fixed_64;
                    }
                    else
                    {
                        static_assert(!sizeof(type));
                    }
                }
                else
                {
                    return wire_type::length_delimited;
                }
            }

            template <typename Type> constexpr static auto make_tag_explicit(auto field_number)
            {
                return make_tag_explicit(tag_type<Type>(), field_number);
            }

            template <typename Type, auto Index> constexpr static auto make_tag()
            {
                return make_tag_explicit(tag_type<Type>(), field_number<Type, Index>());
            }

            template <wire_type WireType, typename Type, auto Index> constexpr static auto make_tag()
            {
                return make_tag_explicit(WireType, field_number<Type, Index>());
            }

            constexpr auto operator()(auto &archive, auto &item) const
                requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::out)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                static_assert(check_type<type>());

                using archive_type = typename std::remove_cvref_t<decltype(archive)>;
                if constexpr (!concepts::varint<typename archive_type::default_size_type> ||
                              ((std::endian::little != std::endian::native) && !archive_type::endian_aware))
                {
                    out out {
                        archive.data(),
                        size_varint {},
                        no_fit_size {},
                        endian::little {},
                        enlarger<std::get<0>(archive_type::enlarger), std::get<1>(archive_type::enlarger)> {},
                        std::conditional_t<archive_type::no_enlarge_overflow, no_enlarge_overflow, enlarge_overflow> {},
                        alloc_limit<archive_type::allocation_limit> {}};
                    out.position() = archive.position();
                    if constexpr (concepts::self_referencing<type>)
                    {
                        auto result = visit_members(item, [&](auto &&...items) constexpr {
                            static_assert((... && check_type<decltype(items)>()));
                            return serialize_many(std::make_index_sequence<sizeof...(items)> {}, out, items...);
                        });
                        archive.position() = out.position();
                        return result;
                    }
                    else
                    {
                        auto result = visit_members(item, [&](auto &&...items) constexpr {
                            static_assert((... && check_type<decltype(items)>()));
                            return serialize_many(std::make_index_sequence<sizeof...(items)> {}, out, items...);
                        });
                        archive.position() = out.position();
                        return result;
                    }
                }
                else if constexpr (concepts::self_referencing<type>)
                {
                    return visit_members(item, [&](auto &&...items) constexpr {
                        static_assert((... && check_type<decltype(items)>()));
                        return serialize_many(std::make_index_sequence<sizeof...(items)> {}, archive, items...);
                    });
                }
                else
                {
                    return visit_members(item, [&](auto &&...items) constexpr {
                        static_assert((... && check_type<decltype(items)>()));
                        return serialize_many(std::make_index_sequence<sizeof...(items)> {}, archive, items...);
                    });
                }
            }

            template <std::size_t FirstIndex, std::size_t... Indices>
            constexpr static auto serialize_many(std::index_sequence<FirstIndex, Indices...>,
                                                 auto &archive,
                                                 auto &first_item,
                                                 auto &...items)
                requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::out)
            {
                if (auto result = serialize_one<FirstIndex>(archive, first_item); failure(result)) [[unlikely]]
                {
                    return result;
                }

                return serialize_many(std::index_sequence<Indices...> {}, archive, items...);
            }

            constexpr static errc serialize_many(std::index_sequence<>, auto &archive)
                requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::out)
            {
                return {};
            }

            template <std::size_t Index, typename TagType = void>
            constexpr static errc serialize_one(auto &archive, auto &item)
                requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::out)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                using tag_type = std::conditional_t<std::is_void_v<TagType>, type, TagType>;

                if constexpr (concepts::empty<type>)
                {
                    return {};
                }
                else if constexpr (is_pb_field<type>())
                {
                    return serialize_one<Index, tag_type>(archive,
                                                          static_cast<const typename type::pb_field_type &>(item));
                }
                else if constexpr (std::is_enum_v<type> && !std::same_as<type, std::byte>)
                {
                    constexpr auto tag = make_tag<tag_type, Index>();
                    if (auto result = archive(tag, varint {std::underlying_type_t<type>(item)}); failure(result))
                        [[unlikely]]
                    {
                        return result;
                    }
                    return {};
                }
                else if constexpr (!concepts::container<type>)
                {
                    constexpr auto tag = make_tag<tag_type, Index>();
                    if (auto result = archive(tag, item); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    return {};
                }
                else if constexpr (concepts::associative_container<type> && requires { typename type::mapped_type; })
                {
                    constexpr auto tag = make_tag<tag_type, Index>();

                    using key_type = std::conditional_t<std::is_enum_v<typename type::key_type> &&
                                                            !std::same_as<typename type::key_type, std::byte>,
                                                        varint<typename type::key_type>,
                                                        typename type::key_type>;

                    using mapped_type = std::conditional_t<std::is_enum_v<typename type::mapped_type> &&
                                                               !std::same_as<typename type::mapped_type, std::byte>,
                                                           varint<typename type::mapped_type>,
                                                           typename type::mapped_type>;

                    struct value_type
                    {
                            const key_type &key;
                            const mapped_type &value;

                            using serialize = protocol<pb_default {}>;
                            serialize use();
                    };

                    for (auto &[key, value] : item)
                    {
                        if (auto result = archive(tag, value_type {.key = key, .value = value}); failure(result))
                            [[unlikely]]
                        {
                            return result;
                        }
                    }

                    return {};
                }
                else if constexpr (requires {
                                       requires std::is_fundamental_v<typename type::value_type> ||
                                                    std::same_as<typename type::value_type, std::byte>;
                                   })
                {
                    constexpr auto tag = make_tag<tag_type, Index>();
                    auto size = item.size();
                    if (!size) [[unlikely]]
                    {
                        return {};
                    }
                    if (auto result = archive(tag, varint {size * sizeof(typename type::value_type)}, unsized(item));
                        failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    return {};
                }
                else if constexpr (requires { requires concepts::varint<typename type::value_type>; })
                {
                    constexpr auto tag = make_tag<tag_type, Index>();

                    std::size_t size = {};
                    for (auto &element : item)
                    {
                        size += varint_size<type::value_type::encoding>(element.value);
                    }
                    if (!size) [[unlikely]]
                    {
                        return {};
                    }
                    if (auto result = archive(tag, varint {size}, unsized(item)); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    return {};
                }
                else if constexpr (requires { requires std::is_enum_v<typename type::value_type>; })
                {
                    constexpr auto tag = make_tag<tag_type, Index>();

                    using type = typename type::value_type;
                    std::size_t size = {};
                    for (auto &element : item)
                    {
                        size += varint_size(std::underlying_type_t<type>(element));
                    }
                    if (!size) [[unlikely]]
                    {
                        return {};
                    }
                    if (auto result = archive(tag, varint {size}); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    for (auto &element : item)
                    {
                        if (auto result = archive(varint {std::underlying_type_t<type>(element)}); failure(result))
                            [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return {};
                }
                else
                {
                    constexpr auto tag = make_tag<typename type::value_type, Index>();
                    for (auto &element : item)
                    {
                        if (auto result = archive(tag, element); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return {};
                }
            }

            constexpr errc operator()(auto &archive,
                                      auto &item,
                                      std::size_t size = std::numeric_limits<std::size_t>::max()) const
                requires(std::remove_cvref_t<decltype(archive)>::kind() == kind::in)
            {
                auto data = archive.remaining_data();
                in in {std::span {data.data(), std::min(size, data.size())},
                       size_varint {},
                       endian::little {},
                       alloc_limit<std::remove_cvref_t<decltype(archive)>::allocation_limit> {}};
                auto result = deserialize_fields(in, item);
                archive.position() += in.position();
                return result;
            }

            constexpr static errc deserialize_fields(auto &archive, auto &item)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                static_assert(check_type<type>());

                auto size = archive.data().size();
                visit_members(item, [](auto &&...members) constexpr {
                    (
                        [](auto &&member) constexpr {
                            using type = std::remove_cvref_t<decltype(member)>;
                            if constexpr (concepts::container<type> && !std::is_fundamental_v<type> &&
                                          !std::same_as<type, std::byte> && requires { member.clear(); })
                            {
                                member.clear();
                            }
                        }(members),
                        ...);
                });

                while (archive.position() < size)
                {
                    vuint32_t tag;
                    if (auto result = archive(tag); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    if (auto result = deserialize_field(archive, item, tag_number(tag), tag_type(tag)); failure(result))
                        [[unlikely]]
                    {
                        return result;
                    }
                }

                return {};
            }

            template <std::size_t Index = 0>
            constexpr static auto deserialize_field(auto &archive, auto &&item, auto field_num, wire_type field_type)
            {
                using type = std::remove_reference_t<decltype(item)>;
                if constexpr (Index >= number_of_members<type>())
                {
                    if (!field_num) [[unlikely]]
                    {
                        return errc {std::errc::protocol_error};
                    }
                    return errc {};
                }
                else if (field_number_from_struct<type, Index>() != field_num)
                {
                    return deserialize_field<Index + 1>(archive, item, field_num, field_type);
                }
                else if constexpr (concepts::self_referencing<type>)
                {
                    return visit_members(item, [&](auto &&...items) constexpr {
                        std::tuple<decltype(items) &...> refs = {items...};
                        auto &item = std::get<Index>(refs);
                        using type = std::remove_reference_t<decltype(item)>;
                        static_assert(check_type<type>());

                        return deserialize_field(archive, field_type, item);
                    });
                }
                else
                {
                    return visit_members(item, [&](auto &&...items) constexpr {
                        std::tuple<decltype(items) &...> refs = {items...};
                        auto &item = std::get<Index>(refs);
                        using type = std::remove_reference_t<decltype(item)>;
                        static_assert(check_type<type>());

                        return deserialize_field(archive, field_type, item);
                    });
                }
            }

            constexpr static auto deserialize_field(auto &archive, wire_type field_type, auto &item)
            {
                using type = std::remove_reference_t<decltype(item)>;
                using archive_type = std::remove_reference_t<decltype(archive)>;
                static_assert(check_type<type>());

                if constexpr (std::is_enum_v<type>)
                {
                    varint<type> value;
                    if (auto result = archive(value); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    item = value;
                    return errc {};
                }
                else if constexpr (is_pb_field<type>())
                {
                    return deserialize_field(archive, field_type, static_cast<typename type::pb_field_type &>(item));
                }
                else if constexpr (!concepts::container<type>)
                {
                    return archive(item);
                }
                else if constexpr (concepts::associative_container<type> && requires { typename type::mapped_type; })
                {
                    using key_type = std::conditional_t<std::is_enum_v<typename type::key_type> &&
                                                            !std::same_as<typename type::key_type, std::byte>,
                                                        varint<typename type::key_type>,
                                                        typename type::key_type>;

                    using mapped_type = std::conditional_t<std::is_enum_v<typename type::mapped_type> &&
                                                               !std::same_as<typename type::mapped_type, std::byte>,
                                                           varint<typename type::mapped_type>,
                                                           typename type::mapped_type>;

                    struct value_type
                    {
                            key_type key;
                            mapped_type value;

                            using serialize = protocol<pb_default {}>;
                            serialize use();
                    };

                    alignas(value_type) std::byte storage[sizeof(value_type)];

                    auto object = access::placement_new<value_type>(std::addressof(storage));
                    destructor_guard guard {*object};
                    if (auto result = archive(*object); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    item.emplace(std::move(object->key), std::move(object->value));
                    return errc {};
                }
                else
                {
                    using orig_value_type = typename type::value_type;
                    using value_type =
                        std::conditional_t<std::is_enum_v<orig_value_type> && !std::same_as<orig_value_type, std::byte>,
                                           varint<orig_value_type>,
                                           orig_value_type>;

                    if constexpr (std::is_fundamental_v<value_type> || std::same_as<std::byte, value_type> ||
                                  concepts::varint<value_type>)
                    {
                        auto fetch = [&]() constexpr {
                            value_type value;
                            if (auto result = archive(value); failure(result)) [[unlikely]]
                            {
                                return result;
                            }

                            if constexpr (requires { item.push_back(orig_value_type(value)); })
                            {
                                item.push_back(orig_value_type(value));
                            }
                            else
                            {
                                item.insert(orig_value_type(value));
                            }

                            return errc {};
                        };
                        if (field_type != wire_type::length_delimited) [[unlikely]]
                        {
                            return fetch();
                        }
                        vsize_t length;
                        if (auto result = archive(length); failure(result)) [[unlikely]]
                        {
                            return result;
                        }

                        if constexpr (requires { item.resize(1); } &&
                                      (std::is_fundamental_v<value_type> || std::same_as<value_type, std::byte>))
                        {
                            if constexpr (archive_type::allocation_limit != std::numeric_limits<std::size_t>::max())
                            {
                                if (length > archive_type::allocation_limit) [[unlikely]]
                                {
                                    return errc {std::errc::message_size};
                                }
                            }
                            item.resize(length / sizeof(value_type));
                            return archive(unsized(item));
                        }
                        else
                        {
                            if constexpr (requires { item.reserve(1); })
                            {
                                item.reserve(length);
                            }

                            auto end_position = length + archive.position();
                            while (archive.position() < end_position)
                            {
                                if (auto result = fetch(); failure(result)) [[unlikely]]
                                {
                                    return result;
                                }
                            }

                            return errc {};
                        }
                    }
                    else
                    {
                        alignas(value_type) std::byte storage[sizeof(value_type)];

                        auto object = access::placement_new<value_type>(std::addressof(storage));
                        destructor_guard guard {*object};
                        if (auto result = archive(*object); failure(result)) [[unlikely]]
                        {
                            return result;
                        }

                        if constexpr (requires { item.push_back(std::move(*object)); })
                        {
                            item.push_back(std::move(*object));
                        }
                        else
                        {
                            item.insert(std::move(*object));
                        }

                        return errc {};
                    }
                }
            }
    };

    using pb_protocol = protocol<pb {}>;

    template <std::size_t Members = std::numeric_limits<std::size_t>::max()>
    using pb_members = protocol<pb {}, Members>;
} // namespace zpp::bits
