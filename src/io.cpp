export module zpp.bits:io;
import std;
import :core;
import :traits;
import :concepts;
import :options;
import :varint;
import :archive;

export namespace zpp::bits
{
    constexpr auto input(auto &&view, auto &&...option)
    {
        return in(std::forward<decltype(view)>(view), std::forward<decltype(option)>(option)...);
    }

    constexpr auto output(auto &&view, auto &&...option)
    {
        return out(std::forward<decltype(view)>(view), std::forward<decltype(option)>(option)...);
    }

    constexpr auto in_out(auto &&view, auto &&...option)
    {
        return std::tuple {in<std::remove_reference_t<typename decltype(in {view})::view_type>, decltype(option) &...>(
                               view, option...),
                           out(std::forward<decltype(view)>(view), std::forward<decltype(option)>(option)...)};
    }

    template <typename ByteType = std::byte> constexpr auto data_in_out(auto &&...option)
    {
        struct data_in_out
        {
                data_in_out(decltype(option) &&...option)
                    : input(data, option...), output(data, std::forward<decltype(option)>(option)...)
                {
                }

                std::vector<ByteType> data;
                in<decltype(data), decltype(option) &...> input;
                out<decltype(data), decltype(option)...> output;
        };
        return data_in_out {std::forward<decltype(option)>(option)...};
    }

    template <typename ByteType = std::byte> constexpr auto data_in(auto &&...option)
    {
        struct data_in
        {
                data_in(decltype(option) &&...option) : input(data, std::forward<decltype(option)>(option)...)
                {
                }

                std::vector<ByteType> data;
                in<decltype(data), decltype(option)...> input;
        };
        return data_in {std::forward<decltype(option)>(option)...};
    }

    template <typename ByteType = std::byte> constexpr auto data_out(auto &&...option)
    {
        struct data_out
        {
                data_out(decltype(option) &&...option) : output(data, std::forward<decltype(option)>(option)...)
                {
                }

                std::vector<ByteType> data;
                out<decltype(data), decltype(option)...> output;
        };
        return data_out {std::forward<decltype(option)>(option)...};
    }

    template <auto Object, std::size_t MaxSize = 0x1000> constexpr auto to_bytes_one()
    {
        constexpr auto size = [] {
            std::array<std::byte, MaxSize> data;
            out out {data};
            out(Object).or_throw();
            return out.position();
        }();

        if constexpr (!size)
        {
            return string_literal<std::byte, 0> {};
        }
        else
        {
            std::array<std::byte, size> data;
            out {data}(Object).or_throw();
            return data;
        }
    }

    template <auto... Data> constexpr auto join()
    {
        constexpr auto size = (0 + ... + Data.size());
        if constexpr (!size)
        {
            return string_literal<std::byte, 0> {};
        }
        else
        {
            std::array<std::byte, size> data;
            out {data}(Data...).or_throw();
            return data;
        }
    }

    template <auto Left, auto Right = -1> constexpr auto slice(auto array)
    {
        constexpr auto left = Left;
        constexpr auto right = (-1 == Right) ? array.size() : Right;
        constexpr auto size = right - left;
        static_assert(Left < Right || -1 == Right);

        std::array<std::remove_reference_t<decltype(array[0])>, size> sliced;
        std::copy(std::begin(array) + left, std::begin(array) + right, std::begin(sliced));
        return sliced;
    }

    template <auto... Object> constexpr auto to_bytes()
    {
        return join<to_bytes_one<Object>()...>();
    }

    template <auto Data, typename Type> constexpr auto from_bytes()
    {
        Type object;
        in {Data}(object).or_throw();
        return object;
    }

    template <auto Data, typename... Types>
    constexpr auto from_bytes()
        requires(sizeof...(Types) > 1)
    {
        std::tuple<Types...> object;
        in {Data}(object).or_throw();
        return object;
    }

    template <auto Id, auto MaxSize = -1> constexpr auto serialize_id()
    {
        constexpr auto serialized_id = slice<0, MaxSize>(to_bytes<Id>());
        if constexpr (sizeof(serialized_id) == 1)
        {
            return serialization_id<from_bytes<serialized_id, std::byte>()> {};
        }
        else if constexpr (sizeof(serialized_id) == 2)
        {
            return serialization_id<from_bytes<serialized_id, std::uint16_t>()> {};
        }
        else if constexpr (sizeof(serialized_id) == 4)
        {
            return serialization_id<from_bytes<serialized_id, std::uint32_t>()> {};
        }
        else if constexpr (sizeof(serialized_id) == 8)
        {
            return serialization_id<from_bytes<serialized_id, std::uint64_t>()> {};
        }
        else
        {
            return serialization_id<serialized_id> {};
        }
    }

    template <auto Id, auto MaxSize = -1> using id = decltype(serialize_id<Id, MaxSize>());

    template <auto Id, auto MaxSize = -1> constexpr auto id_v = id<Id, MaxSize>::value;

    template <typename Id, concepts::variant Variant> struct known_id_variant
    {
            constexpr explicit known_id_variant(Variant &variant) : variant(variant)
            {
            }

            constexpr static auto serialize(auto &serializer, auto &self)
            {
                return serializer.template serialize_one<Id>(self.variant);
            }

            Variant &variant;
    };

    template <auto Id, auto MaxSize = -1, typename Variant> constexpr auto known_id(Variant &&variant)
    {
        return known_id_variant<id<Id, MaxSize>, std::remove_reference_t<Variant>>(variant);
    }

    template <typename Id, concepts::variant Variant> struct known_dynamic_id_variant
    {
            using id_type = std::conditional_t<std::is_integral_v<std::remove_cvref_t<Id>> ||
                                                   std::is_enum_v<std::remove_cvref_t<Id>>,
                                               std::remove_cvref_t<Id>,
                                               Id &>;

            constexpr explicit known_dynamic_id_variant(Variant &variant, id_type id) : variant(variant), id(id)
            {
            }

            constexpr static auto serialize(auto &serializer, auto &self)
            {
                return serializer.serialize_one(self.variant, self.id);
            }

            Variant &variant;
            id_type id;
    };

    template <typename Id, typename Variant> constexpr auto known_id(Id &&id, Variant &&variant)
    {
        return known_dynamic_id_variant<Id, std::remove_reference_t<Variant>>(variant, id);
    }
} // namespace zpp::bits
