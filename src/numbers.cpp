export module zpp.bits:numbers;
import std;
import :core;
import :traits;
import :concepts;
import :options;
import :varint;
import :archive;
import :io;
import :rpc;

export namespace zpp::bits
{
    namespace numbers
    {
        template <typename Type> struct big_endian
        {
                struct emplace
                {
                };

                constexpr big_endian() = default;

                constexpr explicit big_endian(Type value, emplace) : value(value)
                {
                }

                constexpr explicit big_endian(Type value)
                {
                    std::array<std::byte, sizeof(value)> data;
                    for (std::size_t i = 0; i < sizeof(value); ++i)
                    {
                        data[sizeof(value) - 1 - i] = std::byte(value & 0xff);
                        value >>= std::numeric_limits<unsigned char>::digits;
                    }

                    this->value = std::bit_cast<Type>(data);
                }

                constexpr auto operator<<(auto value) const
                {
                    auto data = std::bit_cast<std::array<unsigned char, sizeof(*this)>>(*this);

                    for (std::size_t i = 0; i < sizeof(Type); ++i)
                    {
                        auto offset = (value % std::numeric_limits<unsigned char>::digits);
                        auto current = i + (value / std::numeric_limits<unsigned char>::digits);

                        if (current >= sizeof(Type))
                        {
                            data[i] = 0;
                            continue;
                        }

                        data[i] = data[current] << offset;
                        if (current == sizeof(Type) - 1)
                        {
                            continue;
                        }

                        offset = std::numeric_limits<unsigned char>::digits - offset;
                        if (offset >= 0)
                        {
                            data[i] |= data[current + 1] >> offset;
                        }
                        else
                        {
                            data[i] |= data[current + 1] << (-offset);
                        }
                    }

                    return std::bit_cast<big_endian>(data);
                }

                constexpr auto operator>>(auto value) const
                {
                    auto data = std::bit_cast<std::array<unsigned char, sizeof(*this)>>(*this);

                    for (std::size_t j = 0; j < sizeof(Type); ++j)
                    {
                        auto i = sizeof(Type) - 1 - j;
                        auto offset = (value % std::numeric_limits<unsigned char>::digits);
                        auto current = i - (value / std::numeric_limits<unsigned char>::digits);

                        if (current >= sizeof(Type))
                        {
                            data[i] = 0;
                            continue;
                        }

                        data[i] = data[current] >> offset;
                        if (!current)
                        {
                            continue;
                        }

                        offset = std::numeric_limits<unsigned char>::digits - offset;
                        if (offset >= 0)
                        {
                            data[i] |= data[current - 1] << offset;
                        }
                        else
                        {
                            data[i] |= data[current - 1] >> (-offset);
                        }
                    }

                    return std::bit_cast<big_endian>(data);
                }

                constexpr auto friend operator+(big_endian left, big_endian right)
                {
                    auto left_data = std::bit_cast<std::array<unsigned char, sizeof(left)>>(left);
                    auto right_data = std::bit_cast<std::array<unsigned char, sizeof(right)>>(right);
                    unsigned char remaining {};

                    for (std::size_t i = 0; i < sizeof(Type); ++i)
                    {
                        auto current = sizeof(Type) - 1 - i;
                        std::uint16_t byte_addition =
                            std::uint16_t(left_data[current]) + std::uint16_t(right_data[current]) + remaining;
                        left_data[current] = std::uint8_t(byte_addition & 0xff);
                        remaining = std::uint8_t((byte_addition >> std::numeric_limits<unsigned char>::digits) & 0xff);
                    }

                    return std::bit_cast<big_endian>(left_data);
                }

                constexpr big_endian operator~() const
                {
                    return big_endian {~value, emplace {}};
                }

                constexpr auto &operator+=(big_endian other)
                {
                    *this = (*this) + other;
                    return *this;
                }

                constexpr auto friend operator&(big_endian left, big_endian right)
                {
                    return big_endian {left.value & right.value, emplace {}};
                }

                constexpr auto friend operator^(big_endian left, big_endian right)
                {
                    return big_endian {left.value ^ right.value, emplace {}};
                }

                constexpr auto friend operator|(big_endian left, big_endian right)
                {
                    return big_endian {left.value | right.value, emplace {}};
                }

                constexpr auto friend operator<=>(big_endian left, big_endian right) = default;

                using serialize = members<1>;

                Type value {};
        };
    } // namespace numbers

    template <auto Object, typename Digest = std::array<std::byte, 20>>
        requires requires { requires success(in {Digest {}}(std::array<std::byte, 20> {})); }
    constexpr auto sha1()
    {
        using numbers::big_endian;
        auto rotate_left = [](auto n, auto c) {
            return (n << c) | (n >> ((sizeof(n) * std::numeric_limits<unsigned char>::digits) - c));
        };
        auto align = [](auto v, auto a) { return (v + (a - 1)) / a * a; };

        auto h0 = big_endian {std::uint32_t {0x67452301u}};
        auto h1 = big_endian {std::uint32_t {0xefcdab89u}};
        auto h2 = big_endian {std::uint32_t {0x98badcfeu}};
        auto h3 = big_endian {std::uint32_t {0x10325476u}};
        auto h4 = big_endian {std::uint32_t {0xc3d2e1f0u}};

        constexpr auto original_message = to_bytes<Object>();
        constexpr auto chunk_size = 512 / std::numeric_limits<unsigned char>::digits;
        constexpr auto message = to_bytes<
            original_message,
            std::byte {0x80},
            std::array<std::byte,
                       align(original_message.size() + sizeof(std::byte {0x80}), chunk_size) - original_message.size() -
                           sizeof(std::byte {0x80}) - sizeof(std::uint64_t {original_message.size()})> {},
            big_endian<std::uint64_t> {original_message.size() * std::numeric_limits<unsigned char>::digits}>();

        for (auto chunk :
             from_bytes<message, std::array<std::array<big_endian<std::uint32_t>, 16>, message.size() / chunk_size>>())
        {
            std::array<big_endian<std::uint32_t>, 80> w;
            std::copy(std::begin(chunk), std::end(chunk), std::begin(w));

            for (std::size_t i = 16; i < w.size(); ++i)
            {
                w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            }

            auto a = h0;
            auto b = h1;
            auto c = h2;
            auto d = h3;
            auto e = h4;

            for (std::size_t i = 0; i < w.size(); ++i)
            {
                auto f = big_endian {std::uint32_t {}};
                auto k = big_endian {std::uint32_t {}};
                if (i <= 19)
                {
                    f = (b & c) | ((~b) & d);
                    k = big_endian {std::uint32_t {0x5a827999u}};
                }
                else if (i <= 39)
                {
                    f = b ^ c ^ d;
                    k = big_endian {std::uint32_t {0x6ed9eba1u}};
                }
                else if (i <= 59)
                {
                    f = (b & c) | (b & d) | (c & d);
                    k = big_endian {std::uint32_t {0x8f1bbcdcu}};
                }
                else
                {
                    f = b ^ c ^ d;
                    k = big_endian {std::uint32_t {0xca62c1d6u}};
                }

                auto temp = rotate_left(a, 5) + f + e + k + w[i];
                e = d;
                d = c;
                c = rotate_left(b, 30);
                b = a;
                a = temp;
            }

            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
        }

        std::array<std::byte, 20> digest_data;
        out {digest_data}(h0, h1, h2, h3, h4).or_throw();

        Digest digest;
        in {digest_data}(digest).or_throw();
        return digest;
    }

    template <auto Object, typename Digest = std::array<std::byte, 32>>
        requires requires { requires success(in {Digest {}}(std::array<std::byte, 32> {})); }
    constexpr auto sha256()
    {
        using numbers::big_endian;
        auto rotate_right = [](auto n, auto c) {
            return (n >> c) | (n << ((sizeof(n) * std::numeric_limits<unsigned char>::digits) - c));
        };
        auto align = [](auto v, auto a) { return (v + (a - 1)) / a * a; };

        auto h0 = big_endian {std::uint32_t {0x6a09e667u}};
        auto h1 = big_endian {std::uint32_t {0xbb67ae85u}};
        auto h2 = big_endian {std::uint32_t {0x3c6ef372u}};
        auto h3 = big_endian {std::uint32_t {0xa54ff53au}};
        auto h4 = big_endian {std::uint32_t {0x510e527fu}};
        auto h5 = big_endian {std::uint32_t {0x9b05688cu}};
        auto h6 = big_endian {std::uint32_t {0x1f83d9abu}};
        auto h7 = big_endian {std::uint32_t {0x5be0cd19u}};

        std::array k {big_endian {std::uint32_t {0x428a2f98u}}, big_endian {std::uint32_t {0x71374491u}},
                      big_endian {std::uint32_t {0xb5c0fbcfu}}, big_endian {std::uint32_t {0xe9b5dba5u}},
                      big_endian {std::uint32_t {0x3956c25bu}}, big_endian {std::uint32_t {0x59f111f1u}},
                      big_endian {std::uint32_t {0x923f82a4u}}, big_endian {std::uint32_t {0xab1c5ed5u}},
                      big_endian {std::uint32_t {0xd807aa98u}}, big_endian {std::uint32_t {0x12835b01u}},
                      big_endian {std::uint32_t {0x243185beu}}, big_endian {std::uint32_t {0x550c7dc3u}},
                      big_endian {std::uint32_t {0x72be5d74u}}, big_endian {std::uint32_t {0x80deb1feu}},
                      big_endian {std::uint32_t {0x9bdc06a7u}}, big_endian {std::uint32_t {0xc19bf174u}},
                      big_endian {std::uint32_t {0xe49b69c1u}}, big_endian {std::uint32_t {0xefbe4786u}},
                      big_endian {std::uint32_t {0x0fc19dc6u}}, big_endian {std::uint32_t {0x240ca1ccu}},
                      big_endian {std::uint32_t {0x2de92c6fu}}, big_endian {std::uint32_t {0x4a7484aau}},
                      big_endian {std::uint32_t {0x5cb0a9dcu}}, big_endian {std::uint32_t {0x76f988dau}},
                      big_endian {std::uint32_t {0x983e5152u}}, big_endian {std::uint32_t {0xa831c66du}},
                      big_endian {std::uint32_t {0xb00327c8u}}, big_endian {std::uint32_t {0xbf597fc7u}},
                      big_endian {std::uint32_t {0xc6e00bf3u}}, big_endian {std::uint32_t {0xd5a79147u}},
                      big_endian {std::uint32_t {0x06ca6351u}}, big_endian {std::uint32_t {0x14292967u}},
                      big_endian {std::uint32_t {0x27b70a85u}}, big_endian {std::uint32_t {0x2e1b2138u}},
                      big_endian {std::uint32_t {0x4d2c6dfcu}}, big_endian {std::uint32_t {0x53380d13u}},
                      big_endian {std::uint32_t {0x650a7354u}}, big_endian {std::uint32_t {0x766a0abbu}},
                      big_endian {std::uint32_t {0x81c2c92eu}}, big_endian {std::uint32_t {0x92722c85u}},
                      big_endian {std::uint32_t {0xa2bfe8a1u}}, big_endian {std::uint32_t {0xa81a664bu}},
                      big_endian {std::uint32_t {0xc24b8b70u}}, big_endian {std::uint32_t {0xc76c51a3u}},
                      big_endian {std::uint32_t {0xd192e819u}}, big_endian {std::uint32_t {0xd6990624u}},
                      big_endian {std::uint32_t {0xf40e3585u}}, big_endian {std::uint32_t {0x106aa070u}},
                      big_endian {std::uint32_t {0x19a4c116u}}, big_endian {std::uint32_t {0x1e376c08u}},
                      big_endian {std::uint32_t {0x2748774cu}}, big_endian {std::uint32_t {0x34b0bcb5u}},
                      big_endian {std::uint32_t {0x391c0cb3u}}, big_endian {std::uint32_t {0x4ed8aa4au}},
                      big_endian {std::uint32_t {0x5b9cca4fu}}, big_endian {std::uint32_t {0x682e6ff3u}},
                      big_endian {std::uint32_t {0x748f82eeu}}, big_endian {std::uint32_t {0x78a5636fu}},
                      big_endian {std::uint32_t {0x84c87814u}}, big_endian {std::uint32_t {0x8cc70208u}},
                      big_endian {std::uint32_t {0x90befffau}}, big_endian {std::uint32_t {0xa4506cebu}},
                      big_endian {std::uint32_t {0xbef9a3f7u}}, big_endian {std::uint32_t {0xc67178f2u}}};

        constexpr auto original_message = to_bytes<Object>();
        constexpr auto chunk_size = 512 / std::numeric_limits<unsigned char>::digits;
        constexpr auto message = to_bytes<
            original_message,
            std::byte {0x80},
            std::array<std::byte,
                       align(original_message.size() + sizeof(std::byte {0x80}), chunk_size) - original_message.size() -
                           sizeof(std::byte {0x80}) - sizeof(std::uint64_t {original_message.size()})> {},
            big_endian<std::uint64_t> {original_message.size() * std::numeric_limits<unsigned char>::digits}>();

        for (auto chunk :
             from_bytes<message, std::array<std::array<big_endian<std::uint32_t>, 16>, message.size() / chunk_size>>())
        {
            std::array<big_endian<std::uint32_t>, 64> w;
            std::copy(std::begin(chunk), std::end(chunk), std::begin(w));

            for (std::size_t i = 16; i < w.size(); ++i)
            {
                auto s0 = rotate_right(w[i - 15], 7) ^ rotate_right(w[i - 15], 18) ^ (w[i - 15] >> 3);
                auto s1 = rotate_right(w[i - 2], 17) ^ rotate_right(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            auto a = h0;
            auto b = h1;
            auto c = h2;
            auto d = h3;
            auto e = h4;
            auto f = h5;
            auto g = h6;
            auto h = h7;

            for (std::size_t i = 0; i < w.size(); ++i)
            {
                auto s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
                auto ch = (e & f) ^ ((~e) & g);
                auto temp1 = h + s1 + ch + k[i] + w[i];
                auto s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
                auto maj = (a & b) ^ (a & c) ^ (b & c);
                auto temp2 = s0 + maj;

                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h0 = h0 + a;
            h1 = h1 + b;
            h2 = h2 + c;
            h3 = h3 + d;
            h4 = h4 + e;
            h5 = h5 + f;
            h6 = h6 + g;
            h7 = h7 + h;
        }

        std::array<std::byte, 32> digest_data;
        out {digest_data}(h0, h1, h2, h3, h4, h5, h6, h7).or_throw();

        Digest digest;
        in {digest_data}(digest).or_throw();
        return digest;
    }

    inline namespace literals
    {
        inline namespace string_literals
        {
            template <string_literal String> constexpr auto operator""_s()
            {
                return String;
            }

            template <string_literal String> constexpr auto operator""_b()
            {
                return to_bytes<String>();
            }

            template <string_literal String> constexpr auto operator""_decode_hex()
            {
                constexpr auto tolower = [](auto c) {
                    if ('A' <= c && c <= 'Z')
                    {
                        return decltype(c)(c - 'A' + 'a');
                    }
                    return c;
                };

                static_assert(String.size() % 2 == 0);

                static_assert(std::find_if(std::begin(String), std::end(String), [&](char c) {
                                  return !(('0' <= c && c <= '9') || ('a' <= tolower(c) && tolower(c) <= 'f'));
                              }) == std::end(String));

                auto hex = [](auto c) {
                    if ('a' <= c)
                    {
                        return c - 'a' + 0xa;
                    }
                    else
                    {
                        return c - '0';
                    }
                };

                std::array<std::byte, String.size() / 2> data;
                for (std::size_t i = 0; auto &b : data)
                {
                    auto left = tolower(String[i]);
                    auto right = tolower(String[i + 1]);
                    b = std::byte((hex(left) << (std::numeric_limits<unsigned char>::digits / 2)) | hex(right));
                    i += 2;
                }
                return data;
            }

            template <string_literal String> constexpr auto operator""_sha1()
            {
                return sha1<String>();
            }

            template <string_literal String> constexpr auto operator""_sha256()
            {
                return sha256<String>();
            }

            template <string_literal String> constexpr auto operator""_sha1_int()
            {
                return id_v<sha1<String>(), sizeof(int)>;
            }

            template <string_literal String> constexpr auto operator""_sha256_int()
            {
                return id_v<sha256<String>(), sizeof(int)>;
            }
        } // namespace string_literals
    } // namespace literals

    template <typename... Arguments> using vector1b = sized_t<std::vector<Arguments...>, unsigned char>;
    template <typename... Arguments> using vector2b = sized_t<std::vector<Arguments...>, std::uint16_t>;
    template <typename... Arguments> using vector4b = sized_t<std::vector<Arguments...>, std::uint32_t>;
    template <typename... Arguments> using vector8b = sized_t<std::vector<Arguments...>, std::uint64_t>;
    template <typename... Arguments> using static_vector = unsized_t<std::vector<Arguments...>>;
    template <typename... Arguments>
    using native_vector = sized_t<std::vector<Arguments...>, typename std::vector<Arguments...>::size_type>;

    template <typename... Arguments> using span1b = sized_t<std::span<Arguments...>, unsigned char>;
    template <typename... Arguments> using span2b = sized_t<std::span<Arguments...>, std::uint16_t>;
    template <typename... Arguments> using span4b = sized_t<std::span<Arguments...>, std::uint32_t>;
    template <typename... Arguments> using span8b = sized_t<std::span<Arguments...>, std::uint64_t>;
    template <typename... Arguments> using static_span = unsized_t<std::span<Arguments...>>;
    template <typename... Arguments>
    using native_span = sized_t<std::span<Arguments...>, typename std::span<Arguments...>::size_type>;

    using string1b = sized_t<std::string, unsigned char>;
    using string2b = sized_t<std::string, std::uint16_t>;
    using string4b = sized_t<std::string, std::uint32_t>;
    using string8b = sized_t<std::string, std::uint64_t>;
    using static_string = unsized_t<std::string>;
    using native_string = sized_t<std::string, std::string::size_type>;

    using string_view1b = sized_t<std::string_view, unsigned char>;
    using string_view2b = sized_t<std::string_view, std::uint16_t>;
    using string_view4b = sized_t<std::string_view, std::uint32_t>;
    using string_view8b = sized_t<std::string_view, std::uint64_t>;
    using static_string_view = unsized_t<std::string_view>;
    using native_string_view = sized_t<std::string_view, std::string_view::size_type>;

    using wstring1b = sized_t<std::wstring, unsigned char>;
    using wstring2b = sized_t<std::wstring, std::uint16_t>;
    using wstring4b = sized_t<std::wstring, std::uint32_t>;
    using wstring8b = sized_t<std::wstring, std::uint64_t>;
    using static_wstring = unsized_t<std::wstring>;
    using native_wstring = sized_t<std::wstring, std::wstring::size_type>;

    using wstring_view1b = sized_t<std::wstring_view, unsigned char>;
    using wstring_view2b = sized_t<std::wstring_view, std::uint16_t>;
    using wstring_view4b = sized_t<std::wstring_view, std::uint32_t>;
    using wstring_view8b = sized_t<std::wstring_view, std::uint64_t>;
    using static_wstring_view = unsized_t<std::wstring_view>;
    using native_wstring_view = sized_t<std::wstring_view, std::wstring_view::size_type>;

    using u8string1b = sized_t<std::u8string, unsigned char>;
    using u8string2b = sized_t<std::u8string, std::uint16_t>;
    using u8string4b = sized_t<std::u8string, std::uint32_t>;
    using u8string8b = sized_t<std::u8string, std::uint64_t>;
    using static_u8string = unsized_t<std::u8string>;
    using native_u8string = sized_t<std::u8string, std::u8string::size_type>;

    using u8string_view1b = sized_t<std::u8string_view, unsigned char>;
    using u8string_view2b = sized_t<std::u8string_view, std::uint16_t>;
    using u8string_view4b = sized_t<std::u8string_view, std::uint32_t>;
    using u8string_view8b = sized_t<std::u8string_view, std::uint64_t>;
    using static_u8string_view = unsized_t<std::u8string_view>;
    using native_u8string_view = sized_t<std::u8string_view, std::u8string_view::size_type>;

    using u16string1b = sized_t<std::u16string, unsigned char>;
    using u16string2b = sized_t<std::u16string, std::uint16_t>;
    using u16string4b = sized_t<std::u16string, std::uint32_t>;
    using u16string8b = sized_t<std::u16string, std::uint64_t>;
    using static_u16string = unsized_t<std::u16string>;
    using native_u16string = sized_t<std::u16string, std::u16string::size_type>;

    using u16string_view1b = sized_t<std::u16string_view, unsigned char>;
    using u16string_view2b = sized_t<std::u16string_view, std::uint16_t>;
    using u16string_view4b = sized_t<std::u16string_view, std::uint32_t>;
    using u16string_view8b = sized_t<std::u16string_view, std::uint64_t>;
    using static_u16string_view = unsized_t<std::u16string_view>;
    using native_u16string_view = sized_t<std::u16string_view, std::u16string_view::size_type>;

    using u32string1b = sized_t<std::u32string, unsigned char>;
    using u32string2b = sized_t<std::u32string, std::uint16_t>;
    using u32string4b = sized_t<std::u32string, std::uint32_t>;
    using u32string8b = sized_t<std::u32string, std::uint64_t>;
    using static_u32string = unsized_t<std::u32string>;
    using native_u32string = sized_t<std::u32string, std::u32string::size_type>;

    using u32string_view1b = sized_t<std::u32string_view, unsigned char>;
    using u32string_view2b = sized_t<std::u32string_view, std::uint16_t>;
    using u32string_view4b = sized_t<std::u32string_view, std::uint32_t>;
    using u32string_view8b = sized_t<std::u32string_view, std::uint64_t>;
    using static_u32string_view = unsized_t<std::u32string_view>;
    using native_u32string_view = sized_t<std::u32string_view, std::u32string_view::size_type>;
} // namespace zpp::bits
