export module zpp.bits:archive;
import std;
import :core;
import :traits;
import :concepts;
import :options;
import :varint;

export namespace zpp::bits
{
    template <concepts::byte_view ByteView, typename... Options> class basic_out
    {
        public:
            template <concepts::byte_view, typename...> friend class basic_out;

            template <typename> friend struct option;

            template <typename... Types> using template_type = basic_out<Types...>;

            friend access;

            template <typename, typename> friend struct sized_item;

            template <typename, typename> friend struct sized_item_ref;

            template <typename, concepts::variant> friend struct known_id_variant;

            template <typename, concepts::variant> friend struct known_dynamic_id_variant;

            using byte_type = typename ByteView::value_type;

            static constexpr auto endian_aware = (... || std::same_as<std::remove_cvref_t<Options>, endian::swapped>);

            using default_size_type = traits::default_size_type_t<Options...>;

            constexpr static auto allocation_limit = traits::alloc_limit<Options...>();

            constexpr static auto enlarger = traits::enlarger<Options...>();

            constexpr static auto no_enlarge_overflow =
                (... || std::same_as<std::remove_cvref_t<Options>, options::no_enlarge_overflow>);

            constexpr static bool resizable = requires(ByteView view) { view.resize(1); };

            using view_type = std::conditional_t<resizable,
                                                 ByteView &,
                                                 std::remove_cvref_t<decltype(std::span {std::declval<ByteView &>()})>>;

            constexpr explicit basic_out(ByteView &&view, Options &&...options) : m_data(view)
            {
                static_assert(!resizable);
                (options(*this), ...);
            }

            constexpr explicit basic_out(ByteView &view, Options &&...options) : m_data(view)
            {
                (options(*this), ...);
            }

            constexpr auto operator()(auto &&...items)
            {
                return serialize_many(items...);
            }

            constexpr decltype(auto) data()
            {
                return m_data;
            }

            constexpr std::size_t position() const
            {
                return m_position;
            }

            constexpr std::size_t &position()
            {
                return m_position;
            }

            constexpr auto remaining_data()
            {
                return std::span<byte_type> {m_data.data() + m_position, m_data.size() - m_position};
            }

            constexpr auto processed_data()
            {
                return std::span<byte_type> {m_data.data(), m_position};
            }

            constexpr void reset(std::size_t position = 0)
            {
                m_position = position;
            }

            constexpr static auto kind()
            {
                return kind::out;
            }

            constexpr errc enlarge_for(auto additional_size)
            {
                auto size = m_data.size();
                if (additional_size > size - m_position) [[unlikely]]
                {
                    constexpr auto multiplier = std::get<0>(enlarger);
                    constexpr auto divisor = std::get<1>(enlarger);
                    static_assert(multiplier != 0 && divisor != 0);

                    auto required_size = size + additional_size;
                    if constexpr (!no_enlarge_overflow)
                    {
                        if (required_size < size) [[unlikely]]
                        {
                            return std::errc::no_buffer_space;
                        }
                    }

                    auto new_size = required_size;
                    if constexpr (multiplier != 1)
                    {
                        new_size *= multiplier;
                        if constexpr (!no_enlarge_overflow)
                        {
                            if (new_size / multiplier != required_size) [[unlikely]]
                            {
                                return std::errc::no_buffer_space;
                            }
                        }
                    }
                    if constexpr (divisor != 1)
                    {
                        new_size /= divisor;
                    }
                    if constexpr (allocation_limit != std::numeric_limits<std::size_t>::max())
                    {
                        if (new_size > allocation_limit) [[unlikely]]
                        {
                            return std::errc::no_buffer_space;
                        }
                    }
                    m_data.resize(new_size);
                }
                return {};
            }

        protected:
            constexpr errc serialize_many(auto &&first_item, auto &&...items)
            {
                if (auto result = serialize_one(first_item); failure(result)) [[unlikely]]
                {
                    return result;
                }

                return serialize_many(items...);
            }

            constexpr errc serialize_many()
            {
                return {};
            }

            constexpr auto option(append)
            {
                static_assert(resizable);
                m_position = m_data.size();
            }

            constexpr auto option(reserve size)
            {
                static_assert(resizable);
                m_data.reserve(size.size);
            }

            constexpr auto option(resize size)
            {
                static_assert(resizable);
                m_data.resize(size.size);
                if (m_position > size.size)
                {
                    m_position = size.size;
                }
            }

            constexpr errc serialize_one(concepts::unspecialized auto &&item)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                static_assert(!std::is_pointer_v<type>);

                if constexpr (requires { type::serialize(*this, item); })
                {
                    return type::serialize(*this, item);
                }
                else if constexpr (requires { serialize(*this, item); })
                {
                    return serialize(*this, item);
                }
                else if constexpr (std::is_fundamental_v<type> || std::is_enum_v<type>)
                {
                    if constexpr (resizable)
                    {
                        if (auto result = enlarge_for(sizeof(item)); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    else if (sizeof(item) > m_data.size() - m_position) [[unlikely]]
                    {
                        return std::errc::result_out_of_range;
                    }

                    if (std::is_constant_evaluated())
                    {
                        auto value = std::bit_cast<std::array<std::remove_const_t<byte_type>, sizeof(item)>>(item);
                        for (std::size_t i = 0; i < sizeof(value); ++i)
                        {
                            if constexpr (endian_aware)
                            {
                                m_data[m_position + i] = value[sizeof(value) - 1 - i];
                            }
                            else
                            {
                                m_data[m_position + i] = value[i];
                            }
                        }
                    }
                    else
                    {
                        if constexpr (endian_aware)
                        {
                            std::reverse_copy(reinterpret_cast<const byte_type *>(&item),
                                              reinterpret_cast<const byte_type *>(&item) + sizeof(item),
                                              m_data.data() + m_position);
                        }
                        else
                        {
                            std::memcpy(m_data.data() + m_position, &item, sizeof(item));
                        }
                    }
                    m_position += sizeof(item);
                    return {};
                }
                else if constexpr (requires { requires std::same_as<bytes<typename type::value_type>, type>; })
                {
                    static_assert(!endian_aware || concepts::byte_type<std::remove_cvref_t<decltype(*item.data())>>);

                    auto item_size_in_bytes = item.size_in_bytes();
                    if (!item_size_in_bytes) [[unlikely]]
                    {
                        return {};
                    }

                    if constexpr (resizable)
                    {
                        if (auto result = enlarge_for(item_size_in_bytes); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    else if (item_size_in_bytes > m_data.size() - m_position) [[unlikely]]
                    {
                        return std::errc::result_out_of_range;
                    }

                    if (std::is_constant_evaluated())
                    {
                        auto count = item.count();
                        for (std::size_t index = 0; index < count; ++index)
                        {
                            auto value = std::bit_cast<
                                std::array<std::remove_const_t<byte_type>, sizeof(typename type::value_type)>>(
                                item.data()[index]);
                            for (std::size_t i = 0; i < sizeof(typename type::value_type); ++i)
                            {
                                m_data[m_position + index * sizeof(typename type::value_type) + i] = value[i];
                            }
                        }
                    }
                    else
                    {
                        // Ignore GCC Issue.
                        std::memcpy(m_data.data() + m_position, item.data(), item_size_in_bytes);
                    }
                    m_position += item_size_in_bytes;
                    return {};
                }
                else if constexpr (concepts::empty<type>)
                {
                    return {};
                }
                else if constexpr (concepts::serialize_as_bytes<decltype(*this), type>)
                {
                    return serialize_one(as_bytes(item));
                }
                else if constexpr (concepts::self_referencing<type>)
                {
                    return visit_members(item, [&](auto &&...items) constexpr { return serialize_many(items...); });
                }
                else
                {
                    return visit_members(item, [&](auto &&...items) constexpr { return serialize_many(items...); });
                }
            }

            template <typename SizeType = default_size_type> constexpr errc serialize_one(concepts::array auto &&array)
            {
                using value_type = std::remove_cvref_t<decltype(array[0])>;

                if constexpr (concepts::serialize_as_bytes<decltype(*this), value_type>)
                {
                    return serialize_one(bytes(array));
                }
                else
                {
                    for (auto &item : array)
                    {
                        if (auto result = serialize_one(item); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return {};
                }
            }

            template <typename SizeType = default_size_type>
            constexpr errc serialize_one(concepts::container auto &&container)
            {
                using type = std::remove_cvref_t<decltype(container)>;
                using value_type = typename type::value_type;

                if constexpr (concepts::serialize_as_bytes<decltype(*this), value_type> &&
                              std::is_base_of_v<
                                  std::random_access_iterator_tag,
                                  typename std::iterator_traits<typename type::iterator>::iterator_category> &&
                              requires { container.data(); })
                {
                    auto size = container.size();
                    if constexpr (!std::is_void_v<SizeType> &&
                          (concepts::associative_container<
                               decltype(container)> ||
                           requires(type container) {
                               container.resize(1);
                           } ||
                           (
                               requires(type container) {
                                   container = {container.data(), 1};
                               } &&
                               !requires {
                                   requires(type::extent !=
                                            std::dynamic_extent);
                                   requires concepts::
                                       has_fixed_nonzero_size<type>;
                               })))
                    {
                        if (auto result = serialize_one(static_cast<SizeType>(size)); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return serialize_one(bytes(container, size));
                }
                else
                {
                    if constexpr (!std::is_void_v<SizeType> &&
                          (concepts::associative_container<
                               decltype(container)> ||
                           requires(type container) {
                               container.resize(1);
                           } ||
                           (
                               requires(type container) {
                                   container = {container.data(), 1};
                               } &&
                               !requires {
                                   requires(type::extent !=
                                            std::dynamic_extent);
                                   requires concepts::
                                       has_fixed_nonzero_size<type>;
                               })))
                    {
                        if (auto result = serialize_one(static_cast<SizeType>(container.size())); failure(result))
                            [[unlikely]]
                        {
                            return result;
                        }
                    }
                    for (auto &item : container)
                    {
                        if (auto result = serialize_one(item); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return {};
                }
            }

            constexpr errc serialize_one(concepts::tuple auto &&tuple)
            {
                return serialize_one(
                    tuple, std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(tuple)>>>());
            }

            template <std::size_t... Indices>
            constexpr errc serialize_one(concepts::tuple auto &&tuple, std::index_sequence<Indices...>)
            {
                return serialize_many(std::get<Indices>(tuple)...);
            }

            constexpr errc serialize_one(concepts::optional auto &&optional)
            {
                if (!optional) [[unlikely]]
                {
                    return serialize_one(std::byte(false));
                }
                else
                {
                    return serialize_many(std::byte(true), *optional);
                }
            }

            constexpr errc serialize_one(concepts::expected auto &&expected)
            {
                using type = std::remove_cvref_t<decltype(expected)>;
                using value_type = typename type::value_type;

                if (!expected) [[unlikely]]
                {
                    return serialize_many(std::byte(false), expected.error());
                }
                else
                {
                    if constexpr (std::is_void_v<value_type>)
                    {
                        return serialize_one(std::byte(true));
                    }
                    else
                    {
                        return serialize_many(std::byte(true), *expected);
                    }
                }
            }

            template <typename KnownId = void> constexpr errc serialize_one(concepts::variant auto &&variant)
            {
                using type = std::remove_cvref_t<decltype(variant)>;

                if constexpr (!std::is_void_v<KnownId>)
                {
                    return serialize_one(
                        *std::get_if<traits::variant<type>::template index<KnownId::value>()>(std::addressof(variant)));
                }
                else
                {
                    auto variant_index = variant.index();
                    if (std::variant_npos == variant_index) [[unlikely]]
                    {
                        return std::errc::invalid_argument;
                    }

                    return std::visit(
                        [index = variant_index, this](auto &object) constexpr {
                            return this->serialize_many(traits::variant<type>::id(index), object);
                        },
                        variant);
                }
            }

            constexpr errc serialize_one(concepts::owning_pointer auto &&pointer)
            {
                if (nullptr == pointer) [[unlikely]]
                {
                    return std::errc::invalid_argument;
                }

                return serialize_one(*pointer);
            }

            constexpr errc serialize_one(concepts::bitset auto &&bitset)
            {
                constexpr auto size = std::remove_cvref_t<decltype(bitset)> {}.size();
                constexpr auto size_in_bytes = (size + (std::numeric_limits<unsigned char>::digits - 1)) /
                                               std::numeric_limits<unsigned char>::digits;

                if constexpr (resizable)
                {
                    if (auto result = enlarge_for(size_in_bytes); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                }
                else if (size_in_bytes > m_data.size() - m_position) [[unlikely]]
                {
                    return std::errc::result_out_of_range;
                }

                auto data = m_data.data() + m_position;
                for (std::size_t i = 0; i < size; ++i)
                {
                    auto &value = data[i / std::numeric_limits<unsigned char>::digits];
                    value = byte_type(static_cast<unsigned char>(value) | (bitset[i] << (i & 0x7)));
                }

                m_position += size_in_bytes;
                return {};
            }

            template <typename SizeType = default_size_type>
            constexpr errc serialize_one(concepts::by_protocol auto &&item)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                if constexpr (!std::is_void_v<SizeType>)
                {
                    auto size_position = m_position;
                    if (auto result = serialize_one(SizeType {}); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    if constexpr (requires { typename type::serialize; })
                    {
                        constexpr auto protocol = type::serialize::value;
                        if (auto result = protocol(*this, item); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    else
                    {
                        constexpr auto protocol = decltype(serialize(item))::value;
                        if (auto result = protocol(*this, item); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }

                    auto current_position = m_position;
                    std::size_t message_size = current_position - size_position - sizeof(SizeType);
                    if constexpr (concepts::varint<SizeType>)
                    {
                        constexpr auto preserialized_varint_size = 1;
                        message_size = current_position - size_position - preserialized_varint_size;
                        auto move_ahead_count = varint_size(message_size) - preserialized_varint_size;
                        if (move_ahead_count)
                        {
                            if constexpr (resizable)
                            {
                                if (auto result = enlarge_for(move_ahead_count); failure(result)) [[unlikely]]
                                {
                                    return result;
                                }
                            }
                            else if (move_ahead_count > m_data.size() - current_position) [[unlikely]]
                            {
                                return std::errc::result_out_of_range;
                            }
                            auto data = m_data.data();
                            auto message_start = data + size_position + preserialized_varint_size;
                            auto message_end = data + current_position;
                            if (std::is_constant_evaluated())
                            {
                                for (auto p = message_end - 1; p >= message_start; --p)
                                {
                                    *(p + move_ahead_count) = *p;
                                }
                            }
                            else
                            {
                                std::memmove(message_start + move_ahead_count, message_start, message_size);
                            }
                            m_position += move_ahead_count;
                        }
                    }
                    return basic_out<std::span<byte_type, sizeof(SizeType)>> {std::span<byte_type, sizeof(SizeType)> {
                        m_data.data() + size_position, sizeof(SizeType)}}(SizeType(message_size));
                }
                else
                {
                    if constexpr (requires { typename type::serialize; })
                    {
                        constexpr auto protocol = type::serialize::value;
                        return protocol(*this, item);
                    }
                    else
                    {
                        constexpr auto protocol = decltype(serialize(item))::value;
                        return protocol(*this, item);
                    }
                }
            }

            constexpr ~basic_out() = default;

            view_type m_data {};
            std::size_t m_position {};
    };

    template <concepts::byte_view ByteView = std::vector<std::byte>, typename... Options>
    class out : public basic_out<ByteView, Options...>
    {
        public:
            template <typename... Types> using template_type = out<Types...>;

            using base = basic_out<ByteView, Options...>;
            using base::basic_out;

            friend access;

            using base::enlarger;
            using base::resizable;

            constexpr static auto no_fit_size =
                (... || std::same_as<std::remove_cvref_t<Options>, options::no_fit_size>);

            constexpr auto operator()(auto &&...items)
            {
                if constexpr (resizable && !no_fit_size && enlarger != std::tuple {1, 1})
                {
                    auto end = m_data.size();
                    auto result = serialize_many(items...);
                    if (m_position >= end)
                    {
                        m_data.resize(m_position);
                    }
                    return result;
                }
                else
                {
                    return serialize_many(items...);
                }
            }

        private:
            using base::m_data;
            using base::m_position;
            using base::serialize_many;
    };

    template <typename Type, typename... Options> out(Type &, Options &&...) -> out<Type, Options...>;

    template <typename Type, typename... Options> out(Type &&, Options &&...) -> out<Type, Options...>;

    template <typename Type, std::size_t Size, typename... Options>
    out(Type (&)[Size], Options &&...) -> out<std::span<Type, Size>, Options...>;

    template <typename Type, typename SizeType, typename... Options>
    out(sized_item<Type, SizeType> &, Options &&...) -> out<Type, Options...>;

    template <typename Type, typename SizeType, typename... Options>
    out(const sized_item<Type, SizeType> &, Options &&...) -> out<const Type, Options...>;

    template <typename Type, typename SizeType, typename... Options>
    out(sized_item<Type, SizeType> &&, Options &&...) -> out<Type, Options...>;

    template <concepts::byte_view ByteView = std::vector<std::byte>, typename... Options> class in
    {
        public:
            template <typename... Types> using template_type = in<Types...>;

            template <typename> friend struct option;

            friend access;

            template <typename, typename> friend struct sized_item;

            template <typename, typename> friend struct sized_item_ref;

            template <typename, concepts::variant> friend struct known_id_variant;

            template <typename, concepts::variant> friend struct known_dynamic_id_variant;

            using byte_type = std::add_const_t<typename ByteView::value_type>;

            constexpr static auto endian_aware = (... || std::same_as<std::remove_cvref_t<Options>, endian::swapped>);

            using default_size_type = traits::default_size_type_t<Options...>;

            constexpr static auto allocation_limit = traits::alloc_limit<Options...>();

            constexpr explicit in(ByteView &&view, Options &&...options) : m_data(view)
            {
                static_assert(!resizable);
                (options(*this), ...);
            }

            constexpr explicit in(ByteView &view, Options &&...options) : m_data(view)
            {
                (options(*this), ...);
            }

            constexpr auto operator()(auto &&...items)
            {
                return serialize_many(items...);
            }

            constexpr decltype(auto) data()
            {
                return m_data;
            }

            constexpr std::size_t position() const
            {
                return m_position;
            }

            constexpr std::size_t &position()
            {
                return m_position;
            }

            constexpr auto remaining_data()
            {
                return std::span<byte_type> {m_data.data() + m_position, m_data.size() - m_position};
            }

            constexpr auto processed_data()
            {
                return std::span<byte_type> {m_data.data(), m_position};
            }

            constexpr void reset(std::size_t position = 0)
            {
                m_position = position;
            }

            constexpr static auto kind()
            {
                return kind::in;
            }

            constexpr static bool resizable = requires(ByteView view) { view.resize(1); };

            using view_type = std::conditional_t<resizable,
                                                 ByteView &,
                                                 std::remove_cvref_t<decltype(std::span {std::declval<ByteView &>()})>>;

        private:
            constexpr errc serialize_many(auto &&first_item, auto &&...items)
            {
                if (auto result = serialize_one(first_item); failure(result)) [[unlikely]]
                {
                    return result;
                }

                return serialize_many(items...);
            }

            constexpr errc serialize_many()
            {
                return {};
            }

            constexpr errc serialize_one(concepts::unspecialized auto &&item)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                static_assert(!std::is_pointer_v<type>);

                if constexpr (requires { type::serialize(*this, item); })
                {
                    return type::serialize(*this, item);
                }
                else if constexpr (requires { serialize(*this, item); })
                {
                    return serialize(*this, item);
                }
                else if constexpr (std::is_fundamental_v<type> || std::is_enum_v<type>)
                {
                    auto size = m_data.size();
                    if (sizeof(item) > size - m_position) [[unlikely]]
                    {
                        return std::errc::result_out_of_range;
                    }
                    if (std::is_constant_evaluated())
                    {
                        std::array<std::remove_const_t<byte_type>, sizeof(item)> value;
                        for (std::size_t i = 0; i < sizeof(value); ++i)
                        {
                            if constexpr (endian_aware)
                            {
                                value[sizeof(value) - 1 - i] = byte_type(m_data[m_position + i]);
                            }
                            else
                            {
                                value[i] = byte_type(m_data[m_position + i]);
                            }
                        }
                        item = std::bit_cast<type>(value);
                    }
                    else
                    {
                        if constexpr (endian_aware)
                        {
                            auto begin = m_data.data() + m_position;
                            std::reverse_copy(
                                begin, begin + sizeof(item), reinterpret_cast<std::remove_const_t<byte_type> *>(&item));
                        }
                        else
                        {
                            std::memcpy(&item, m_data.data() + m_position, sizeof(item));
                        }
                    }
                    m_position += sizeof(item);
                    return {};
                }
                else if constexpr (requires { requires std::same_as<bytes<typename type::value_type>, type>; })
                {
                    static_assert(!endian_aware || concepts::byte_type<std::remove_cvref_t<decltype(*item.data())>>);

                    auto size = m_data.size();
                    auto item_size_in_bytes = item.size_in_bytes();
                    if (!item_size_in_bytes) [[unlikely]]
                    {
                        return {};
                    }

                    if (item_size_in_bytes > size - m_position) [[unlikely]]
                    {
                        return std::errc::result_out_of_range;
                    }
                    if (std::is_constant_evaluated())
                    {
                        std::size_t count = item.count();
                        for (std::size_t index = 0; index < count; ++index)
                        {
                            std::array<std::remove_const_t<byte_type>, sizeof(typename type::value_type)> value;
                            for (std::size_t i = 0; i < sizeof(typename type::value_type); ++i)
                            {
                                value[i] =
                                    byte_type(m_data[m_position + index * sizeof(typename type::value_type) + i]);
                            }
                            item.data()[index] = std::bit_cast<typename type::value_type>(value);
                        }
                    }
                    else
                    {
                        std::memcpy(item.data(), m_data.data() + m_position, item_size_in_bytes);
                    }
                    m_position += item_size_in_bytes;
                    return {};
                }
                else if constexpr (concepts::empty<type>)
                {
                    return {};
                }
                else if constexpr (concepts::serialize_as_bytes<decltype(*this), type>)
                {
                    return serialize_one(as_bytes(item));
                }
                else if constexpr (concepts::self_referencing<type>)
                {
                    return visit_members(item, [&](auto &&...items) constexpr { return serialize_many(items...); });
                }
                else
                {
                    return visit_members(item, [&](auto &&...items) constexpr { return serialize_many(items...); });
                }
            }

            template <typename SizeType = default_size_type> constexpr errc serialize_one(concepts::array auto &&array)
            {
                using value_type = std::remove_cvref_t<decltype(array[0])>;

                if constexpr (concepts::serialize_as_bytes<decltype(*this), value_type>)
                {
                    return serialize_one(bytes(array));
                }
                else
                {
                    for (auto &item : array)
                    {
                        if (auto result = serialize_one(item); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return {};
                }
            }

            template <typename SizeType = default_size_type>
            constexpr errc serialize_one(concepts::container auto &&container)
            {
                using type = std::remove_cvref_t<decltype(container)>;
                using value_type = typename type::value_type;
                constexpr auto is_const = std::is_const_v<std::remove_reference_t<value_type>> || requires {
                    requires std::is_const_v<std::remove_reference_t<decltype(container[0])>>;
                };

                if constexpr (!std::is_void_v<SizeType> &&
                      (requires(type container) { container.resize(1); } ||
                       (
                           requires(type container) {
                               container = {container.data(), 1};
                           } &&
                           !requires {
                               requires(type::extent !=
                                        std::dynamic_extent);
                               requires concepts::has_fixed_nonzero_size<
                                   type>;
                           })))
                {
                    SizeType size {};
                    if (auto result = serialize_one(size); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    if constexpr (requires(type container) { container.resize(size); })
                    {
                        if constexpr (allocation_limit != std::numeric_limits<std::size_t>::max())
                        {
                            constexpr auto limit = allocation_limit / sizeof(value_type);
                            if (size > limit) [[unlikely]]
                            {
                                return std::errc::message_size;
                            }
                        }
                        container.resize(size);
                    }
                    else if constexpr (is_const &&
                                       (std::same_as<std::byte, value_type> || std::same_as<char, value_type> ||
                                        std::same_as<unsigned char, value_type>))
                    {
                        if (size > m_data.size() - m_position) [[unlikely]]
                        {
                            return std::errc::result_out_of_range;
                        }
                        container = {m_data.data() + m_position, size};
                        m_position += size;
                    }
                    else
                    {
                        if (size > container.size()) [[unlikely]]
                        {
                            return std::errc::result_out_of_range;
                        }
                        container = {container.data(), size};
                    }

                    if constexpr (concepts::serialize_as_bytes<decltype(*this), value_type> &&
                                  std::is_base_of_v<
                                      std::random_access_iterator_tag,
                                      typename std::iterator_traits<typename type::iterator>::iterator_category> &&
                                  requires { container.data(); } &&
                                  !(is_const &&
                                    (std::same_as<std::byte, value_type> || std::same_as<char, value_type> ||
                                     std::same_as<unsigned char, value_type>) &&
                                    requires(type container) { container = {m_data.data(), 1}; }))
                    {
                        return serialize_one(bytes(container, size));
                    }
                }

                if constexpr (concepts::serialize_as_bytes<decltype(*this), value_type> &&
                              std::is_base_of_v<
                                  std::random_access_iterator_tag,
                                  typename std::iterator_traits<typename type::iterator>::iterator_category> &&
                              requires { container.data(); })
                {
                    if constexpr (is_const &&
                                  (std::same_as<std::byte, value_type> || std::same_as<char, value_type> ||
                                   std::same_as<unsigned char, value_type>) &&
                                  requires(type container) { container = {m_data.data(), 1}; })
                    {
                        if constexpr (requires {
                                          requires(type::extent != std::dynamic_extent);
                                          requires concepts::has_fixed_nonzero_size<type>;
                                      })
                        {
                            if (type::extent > m_data.size() - m_position) [[unlikely]]
                            {
                                return std::errc::result_out_of_range;
                            }
                            container = {m_data.data() + m_position, type::extent};
                            m_position += type::extent;
                        }
                        else if constexpr (std::is_void_v<SizeType>)
                        {
                            auto size = m_data.size();
                            container = {m_data.data() + m_position, size - m_position};
                            m_position = size;
                        }
                        return {};
                    }
                    else
                    {
                        return serialize_one(bytes(container));
                    }
                }
                else
                {
                    for (auto &item : container)
                    {
                        if (auto result = serialize_one(item); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                    }
                    return {};
                }
            }

            template <typename SizeType = default_size_type>
            constexpr errc serialize_one(concepts::associative_container auto &&container)
            {
                using type = typename std::remove_cvref_t<decltype(container)>;

                SizeType size {};

                if constexpr (!std::is_void_v<SizeType>)
                {
                    if (auto result = serialize_one(size); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                }
                else
                {
                    size = container.size();
                }

                container.clear();

                for (std::size_t index {}; index < size; ++index)
                {
                    if constexpr (requires { typename type::mapped_type; })
                    {
                        using value_type = std::pair<typename type::key_type, typename type::mapped_type>;
                        alignas(value_type) std::byte storage[sizeof(value_type)];

                        auto object = access::placement_new<value_type>(std::addressof(storage));
                        destructor_guard guard {*object};
                        if (auto result = serialize_one(*object); failure(result)) [[unlikely]]
                        {
                            return result;
                        }

                        container.insert(std::move(*object));
                    }
                    else
                    {
                        using value_type = typename type::value_type;

                        alignas(value_type) std::byte storage[sizeof(value_type)];

                        auto object = access::placement_new<value_type>(std::addressof(storage));
                        destructor_guard guard {*object};
                        if (auto result = serialize_one(*object); failure(result)) [[unlikely]]
                        {
                            return result;
                        }

                        container.insert(std::move(*object));
                    }
                }

                return {};
            }

            constexpr errc serialize_one(concepts::tuple auto &&tuple)
            {
                return serialize_one(
                    tuple, std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(tuple)>>>());
            }

            template <std::size_t... Indices>
            constexpr errc serialize_one(concepts::tuple auto &&tuple, std::index_sequence<Indices...>)
            {
                return serialize_many(std::get<Indices>(tuple)...);
            }

            constexpr errc serialize_one(concepts::optional auto &&optional)
            {
                using value_type = std::remove_reference_t<decltype(*optional)>;

                std::byte has_value {};
                if (auto result = serialize_one(has_value); failure(result)) [[unlikely]]
                {
                    return result;
                }

                if (!bool(has_value)) [[unlikely]]
                {
                    optional = std::nullopt;
                    return {};
                }

                if constexpr (std::is_default_constructible_v<value_type>)
                {
                    if (!optional)
                    {
                        optional = value_type {};
                    }

                    if (auto result = serialize_one(*optional); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                }
                else
                {
                    alignas(value_type) std::byte storage[sizeof(value_type)];

                    auto object = access::placement_new<value_type>(std::addressof(storage));
                    destructor_guard guard {*object};

                    if (auto result = serialize_one(*object); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    optional = std::move(*object);
                }

                return {};
            }

            constexpr errc serialize_one(concepts::expected auto &&expected)
            {
                using type = std::remove_cvref_t<decltype(expected)>;
                using value_type = typename type::value_type;
                using error_type = typename type::error_type;
                using unexpected_type = typename type::unexpected_type;

                std::byte has_value {};
                if (auto result = serialize_one(has_value); failure(result)) [[unlikely]]
                {
                    return result;
                }

                if (!bool(has_value)) [[unlikely]]
                {
                    if (expected.has_value())
                    {
                        if constexpr (std::is_default_constructible_v<error_type>)
                        {
                            expected = unexpected_type(std::in_place_t {});
                        }
                        else
                        {
                            alignas(error_type) std::byte storage[sizeof(error_type)];
                            auto object = access::placement_new<error_type>(std::addressof(storage));
                            destructor_guard guard {*object};
                            expected = unexpected_type(std::move(*object));
                        }
                    }
                    if (auto result = serialize_one(expected.error()); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                    return {};
                }

                if constexpr (std::is_void_v<value_type>)
                {
                    expected.emplace();
                }
                else
                {
                    if (!expected.has_value()) [[unlikely]]
                    {
                        if constexpr (std::is_default_constructible_v<value_type>)
                        {
                            expected.emplace();
                        }
                        else
                        {
                            alignas(value_type) std::byte storage[sizeof(value_type)];
                            auto object = access::placement_new<value_type>(std::addressof(storage));
                            destructor_guard guard {*object};
                            expected = std::move(*object);
                        }
                    }
                    if (auto result = serialize_one(*expected); failure(result)) [[unlikely]]
                    {
                        return result;
                    }
                }
                return {};
            }

            template <typename KnownId = void, typename... Types, template <typename...> typename Variant>
            constexpr errc serialize_one(Variant<Types...> &variant)
                requires concepts::variant<Variant<Types...>>
            {
                using type = std::remove_cvref_t<decltype(variant)>;

                if constexpr (!std::is_void_v<KnownId>)
                {
                    constexpr auto index = traits::variant<type>::template index<KnownId::value>();

                    using element_type = std::remove_reference_t<decltype(std::get<index>(variant))>;

                    if constexpr (std::is_default_constructible_v<element_type>)
                    {
                        if (variant.index() != traits::variant<type>::template index_by_type<element_type>())
                        {
                            variant = element_type {};
                        }
                        return serialize_one(*std::get_if<element_type>(&variant));
                    }
                    else
                    {
                        alignas(element_type) std::byte storage[sizeof(element_type)];

                        auto object = access::placement_new<element_type>(std::addressof(storage));
                        destructor_guard guard {*object};

                        if (auto result = serialize_one(*object); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                        variant = std::move(*object);
                    }
                }
                else
                {
                    typename traits::variant<type>::id_type id;
                    if (auto result = serialize_one(id); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    return serialize_one(variant, id);
                }
            }

            template <typename... Types, template <typename...> typename Variant>
            constexpr errc serialize_one(Variant<Types...> &variant, auto &&id)
                requires concepts::variant<Variant<Types...>>
            {
                using type = std::remove_cvref_t<decltype(variant)>;

                auto index = traits::variant<type>::index(id);
                if (index > sizeof...(Types)) [[unlikely]]
                {
                    return std::errc::bad_message;
                }

                constexpr std::tuple loaders {[](auto &self, auto &variant) constexpr {
                    if constexpr (std::is_default_constructible_v<Types>)
                    {
                        if (variant.index() != traits::variant<type>::template index_by_type<Types>())
                        {
                            variant = Types {};
                        }
                        return self.serialize_one(*std::get_if<Types>(&variant));
                    }
                    else
                    {
                        alignas(Types) std::byte storage[sizeof(Types)];

                        auto object = access::placement_new<Types>(std::addressof(storage));
                        destructor_guard guard {*object};

                        if (auto result = self.serialize_one(*object); failure(result)) [[unlikely]]
                        {
                            return result;
                        }
                        variant = std::move(*object);
                        return errc {};
                    }
                }...};

                return traits::tuple<std::remove_cvref_t<decltype(loaders)>>::visit(
                    loaders, index, [&](auto &&loader) constexpr { return loader(*this, variant); });
            }

            constexpr errc serialize_one(concepts::owning_pointer auto &&pointer)
            {
                using type = std::remove_reference_t<decltype(*pointer)>;

                auto loaded = access::make_unique<type>();
                ;
                if (auto result = serialize_one(*loaded); failure(result)) [[unlikely]]
                {
                    return result;
                }

                pointer.reset(loaded.release());
                return {};
            }

            constexpr errc serialize_one(concepts::bitset auto &&bitset)
            {
                constexpr auto size = std::remove_cvref_t<decltype(bitset)> {}.size();
                constexpr auto size_in_bytes = (size + (std::numeric_limits<unsigned char>::digits - 1)) /
                                               std::numeric_limits<unsigned char>::digits;

                if (size_in_bytes > m_data.size() - m_position) [[unlikely]]
                {
                    return std::errc::result_out_of_range;
                }

                auto data = m_data.data() + m_position;
                for (std::size_t i = 0; i < size; ++i)
                {
                    bitset[i] = (static_cast<unsigned char>(data[i / std::numeric_limits<unsigned char>::digits]) >>
                                 (i & 0x7)) &
                                0x1;
                }

                m_position += size_in_bytes;
                return {};
            }

            template <typename SizeType = default_size_type>
            constexpr errc serialize_one(concepts::by_protocol auto &&item)
            {
                using type = std::remove_cvref_t<decltype(item)>;
                if constexpr (!std::is_void_v<SizeType>)
                {
                    SizeType size {};
                    if (auto result = serialize_one(size); failure(result)) [[unlikely]]
                    {
                        return result;
                    }

                    if constexpr (requires { typename type::serialize; })
                    {
                        constexpr auto protocol = type::serialize::value;
                        return protocol(*this, item, size);
                    }
                    else
                    {
                        constexpr auto protocol = decltype(serialize(item))::value;
                        return protocol(*this, item, size);
                    }
                }
                else
                {
                    if constexpr (requires { typename type::serialize; })
                    {
                        constexpr auto protocol = type::serialize::value;
                        return protocol(*this, item);
                    }
                    else
                    {
                        constexpr auto protocol = decltype(serialize(item))::value;
                        return protocol(*this, item);
                    }
                }
            }

            view_type m_data {};
            std::size_t m_position {};
    };

    template <typename Type, std::size_t Size, typename... Options>
    in(Type (&)[Size], Options &&...) -> in<std::span<Type, Size>, Options...>;

    template <typename Type, typename SizeType, typename... Options>
    in(sized_item<Type, SizeType> &, Options &&...) -> in<Type, Options...>;

    template <typename Type, typename SizeType, typename... Options>
    in(const sized_item<Type, SizeType> &, Options &&...) -> in<const Type, Options...>;

    template <typename Type, typename SizeType, typename... Options>
    in(sized_item<Type, SizeType> &&, Options &&...) -> in<Type, Options...>;
} // namespace zpp::bits
