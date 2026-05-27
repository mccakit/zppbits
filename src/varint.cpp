export module zpp.bits:varint;
import std;
import :core;
import :traits;
import :concepts;
import :options;

export namespace zpp::bits
{
    enum class varint_encoding
    {
        normal,
        zig_zag,
    };

    template <typename Type, varint_encoding Encoding = varint_encoding::normal> struct varint
    {
            varint() = default;

            using value_type = Type;
            static constexpr auto encoding = Encoding;

            constexpr varint(Type value) : value(value)
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

            constexpr decltype(auto) operator*() &
            {
                return (value);
            }

            constexpr auto operator*() const &
            {
                return value;
            }

            Type value {};
    };

    namespace concepts
    {

        template <typename Type>
        concept varint =
            requires { requires std::same_as<Type, zpp::bits::varint<typename Type::value_type, Type::encoding>>; };

    } // namespace concepts

    // Support for std::complex tuple protocol

    template <typename Type>
    constexpr auto varint_max_size =
        sizeof(Type) * std::numeric_limits<unsigned char>::digits / (std::numeric_limits<unsigned char>::digits - 1) +
        1;

    template <varint_encoding Encoding = varint_encoding::normal> constexpr auto varint_size(auto value)
    {
        if constexpr (Encoding == varint_encoding::zig_zag)
        {
            return varint_size(std::make_unsigned_t<decltype(value)>(
                (value << 1) ^ (value >> (sizeof(value) * std::numeric_limits<unsigned char>::digits - 1))));
        }
        else
        {
            return ((sizeof(value) * std::numeric_limits<unsigned char>::digits) -
                    std::countl_zero(std::make_unsigned_t<decltype(value)>(value | 0x1)) +
                    (std::numeric_limits<unsigned char>::digits - 2)) /
                   (std::numeric_limits<unsigned char>::digits - 1);
        }
    }

    template <typename Archive, typename Type, varint_encoding Encoding>
    constexpr auto serialize(Archive &archive, varint<Type, Encoding> self)
        requires(Archive::kind() == kind::out)
    {
        auto orig_value = std::conditional_t<std::is_enum_v<Type>, traits::underlying_type_t<Type>, Type>(self.value);
        auto value = std::make_unsigned_t<Type>(orig_value);
        if constexpr (varint_encoding::zig_zag == Encoding)
        {
            value = (value << 1) ^ (orig_value >> (sizeof(Type) * std::numeric_limits<unsigned char>::digits - 1));
        }

        constexpr auto max_size = varint_max_size<Type>;
        if constexpr (Archive::resizable)
        {
            if (auto result = archive.enlarge_for(max_size); failure(result)) [[unlikely]]
            {
                return result;
            }
        }

        auto data = archive.remaining_data();
        if constexpr (!Archive::resizable)
        {
            auto data_size = data.size();
            if (data_size < max_size) [[unlikely]]
            {
                if (data_size < varint_size(value)) [[unlikely]]
                {
                    return errc {std::errc::result_out_of_range};
                }
            }
        }

        using byte_type = std::remove_cvref_t<decltype(data[0])>;
        std::size_t position = {};
        while (value >= 0x80)
        {
            data[position++] = byte_type((value & 0x7f) | 0x80);
            value >>= (std::numeric_limits<unsigned char>::digits - 1);
        }
        data[position++] = byte_type(value);

        archive.position() += position;
        return errc {};
    }

    constexpr auto decode_varint(auto data, auto &value, auto &position)
    {
        using value_type = std::remove_cvref_t<decltype(value)>;
        if (data.size() < varint_max_size<value_type>) [[unlikely]]
        {
            std::size_t shift = 0;
            for (auto &byte_value : data)
            {
                auto next_byte = value_type(byte_value);
                value |= (next_byte & 0x7f) << shift;
                if (next_byte >= 0x80) [[unlikely]]
                {
                    shift += std::numeric_limits<unsigned char>::digits - 1;
                    continue;
                }
                position += 1 + std::distance(data.data(), &byte_value);
                return errc {};
            }
            return errc {std::errc::result_out_of_range};
        }
        else
        {
            auto p = data.data();
            do
            {
                // clang-format off
            value_type next_byte;
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 0)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 1)); if (next_byte < 0x80) [[likely]] { break; }
            if constexpr (varint_max_size<value_type> > 2) {
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 2)); if (next_byte < 0x80) [[likely]] { break; }
            if constexpr (varint_max_size<value_type> > 3) {
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 3)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 4)); if (next_byte < 0x80) [[likely]] { break; }
            if constexpr (varint_max_size<value_type> > 5) {
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 5)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 6)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 7)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 8)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x01) << ((std::numeric_limits<unsigned char>::digits - 1) * 9)); if (next_byte < 0x80) [[likely]] { break; } }}}
            return errc{std::errc::value_too_large};
                // clang-format on
            } while (false);
            position += std::distance(data.data(), p);
            return errc {};
        }
    }

    template <typename Archive, typename Type, varint_encoding Encoding>
    constexpr auto serialize(Archive &archive, varint<Type, Encoding> &self)
        requires(Archive::kind() == kind::in)
    {
        using value_type = std::conditional_t<std::is_enum_v<Type>,
                                              std::make_unsigned_t<traits::underlying_type_t<Type>>,
                                              std::make_unsigned_t<Type>>;
        value_type value {};
        auto data = archive.remaining_data();

        if constexpr (!0)
        {
            auto &position = archive.position();
            if (!data.empty() && !(value_type(data[0]) & 0x80)) [[likely]]
            {
                value = value_type(data[0]);
                position += 1;
            }
            else if (auto result =
                         std::is_constant_evaluated()
                             ? decode_varint(data, value, position)
                             : decode_varint(std::span {reinterpret_cast<const std::byte *>(data.data()), data.size()},
                                             value,
                                             position);
                     failure(result)) [[unlikely]]
            {
                return result;
            }

            if constexpr (varint_encoding::zig_zag == Encoding)
            {
                self.value = decltype(self.value)((value >> 1) ^ -(value & 0x1));
            }
            else
            {
                self.value = decltype(self.value)(value);
            }
            return errc {};
        }
        else if (data.size() < varint_max_size<value_type>) [[unlikely]]
        {
            std::size_t shift = 0;
            for (auto &byte_value : data)
            {
                auto next_byte = decltype(value)(byte_value);
                value |= (next_byte & 0x7f) << shift;
                if (next_byte >= 0x80) [[unlikely]]
                {
                    shift += std::numeric_limits<unsigned char>::digits - 1;
                    continue;
                }
                if constexpr (varint_encoding::zig_zag == Encoding)
                {
                    self.value = decltype(self.value)((value >> 1) ^ -(value & 0x1));
                }
                else
                {
                    self.value = decltype(self.value)(value);
                }
                archive.position() += 1 + std::distance(data.data(), &byte_value);
                return errc {};
            }
            return errc {std::errc::result_out_of_range};
        }
        else
        {
            auto p = data.data();
            do
            {
                // clang-format off
            value_type next_byte;
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 0)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 1)); if (next_byte < 0x80) [[likely]] { break; }
            if constexpr (varint_max_size<value_type> > 2) {
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 2)); if (next_byte < 0x80) [[likely]] { break; }
            if constexpr (varint_max_size<value_type> > 3) {
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 3)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 4)); if (next_byte < 0x80) [[likely]] { break; }
            if constexpr (varint_max_size<value_type> > 5) {
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 5)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 6)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 7)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x7f) << ((std::numeric_limits<unsigned char>::digits - 1) * 8)); if (next_byte < 0x80) [[likely]] { break; }
            next_byte = value_type(*p++); value |= ((next_byte & 0x01) << ((std::numeric_limits<unsigned char>::digits - 1) * 9)); if (next_byte < 0x80) [[likely]] { break; } }}}
            return errc{std::errc::value_too_large};
                // clang-format on
            } while (false);
            if constexpr (varint_encoding::zig_zag == Encoding)
            {
                self.value = decltype(self.value)((value >> 1) ^ -(value & 0x1));
            }
            else
            {
                self.value = decltype(self.value)(value);
            }
            archive.position() += std::distance(data.data(), p);
            return errc {};
        }
    }

    template <typename Archive, typename Type, varint_encoding Encoding>
    constexpr auto serialize(Archive &archive, varint<Type, Encoding> &&self)
        requires(Archive::kind() == kind::in)
    = delete;

    using vint32_t = varint<std::int32_t>;
    using vint64_t = varint<std::int64_t>;

    using vuint32_t = varint<std::uint32_t>;
    using vuint64_t = varint<std::uint64_t>;

    using vsint32_t = varint<std::int32_t, varint_encoding::zig_zag>;
    using vsint64_t = varint<std::int64_t, varint_encoding::zig_zag>;

    using vsize_t = varint<std::size_t>;

    inline namespace options
    {
        struct size_varint : option<size_varint>
        {
                using default_size_type = vsize_t;
        };
    } // namespace options
} // namespace zpp::bits
