export module zpp.bits:options;
import std;
import :core;
import :traits;
import :concepts;

export namespace zpp::bits
{
    template <typename CharType, std::size_t Size> struct string_literal : public std::array<CharType, Size + 1>
    {
            using base = std::array<CharType, Size + 1>;
            using value_type = typename base::value_type;
            using pointer = typename base::pointer;
            using const_pointer = typename base::const_pointer;
            using iterator = typename base::iterator;
            using const_iterator = typename base::const_iterator;
            using reference = typename base::const_pointer;
            using const_reference = typename base::const_pointer;
            using size_type = default_size_type;

            constexpr string_literal() = default;
            constexpr string_literal(const CharType (&value)[Size + 1])
            {
                std::copy_n(std::begin(value), Size + 1, std::begin(*this));
            }

            constexpr auto operator<=>(const string_literal &) const = default;

            constexpr default_size_type size() const
            {
                return Size;
            }

            constexpr bool empty() const
            {
                return !Size;
            }

            using base::begin;

            constexpr auto end()
            {
                return base::end() - 1;
            }

            constexpr auto end() const
            {
                return base::end() - 1;
            }

            using base::data;
            using base::operator[];
            using base::at;

        private:
            using base::cbegin;
            using base::cend;
            using base::rbegin;
            using base::rend;
    };

    template <typename CharType, std::size_t Size>
    string_literal(const CharType (&value)[Size]) -> string_literal<CharType, Size - 1>;

    template <typename Item> class bytes
    {
        public:
            using value_type = Item;

            constexpr explicit bytes(std::span<Item> items) : m_items(items.data()), m_size(items.size())
            {
            }

            constexpr explicit bytes(std::span<Item> items, auto size)
                : m_items(items.data()), m_size(std::size_t(size))
            {
            }

            constexpr auto data() const
            {
                return m_items;
            }

            constexpr std::size_t size_in_bytes() const
            {
                return m_size * sizeof(Item);
            }

            constexpr std::size_t count() const
            {
                return m_size;
            }

        private:
            static_assert(std::is_trivially_copyable_v<Item>);

            Item *m_items;
            std::size_t m_size;
    };

    template <typename Item> bytes(std::span<Item>) -> bytes<Item>;

    template <typename Item> bytes(std::span<Item>, std::size_t) -> bytes<Item>;

    template <typename Item, std::size_t Count> bytes(Item (&)[Count]) -> bytes<Item>;

    template <concepts::container Container>
    bytes(Container &&container) -> bytes<std::remove_reference_t<decltype(container[0])>>;

    template <concepts::container Container>
    bytes(Container &&container, std::size_t) -> bytes<std::remove_reference_t<decltype(container[0])>>;

    constexpr auto as_bytes(auto &&object)
    {
        return bytes(std::span {&object, 1});
    }

    template <typename Option> struct option
    {
            using zpp_bits_option = void;
            constexpr auto operator()(auto &&archive)
            {
                if constexpr (requires { archive.option(static_cast<Option &>(*this)); })
                {
                    archive.option(static_cast<Option &>(*this));
                }
            }
    };

    inline namespace options
    {
        struct append : option<append>
        {
        };

        struct reserve : option<reserve>
        {
                constexpr explicit reserve(std::size_t size) : size(size)
                {
                }
                std::size_t size {};
        };

        struct resize : option<resize>
        {
                constexpr explicit resize(std::size_t size) : size(size)
                {
                }
                std::size_t size {};
        };

        template <std::size_t Size> struct alloc_limit : option<alloc_limit<Size>>
        {
                constexpr static auto alloc_limit_value = Size;
        };

        template <std::size_t Multiplier, std::size_t Divisor = 1>
        struct enlarger : option<enlarger<Multiplier, Divisor>>
        {
                constexpr static auto enlarger_value = std::tuple {Multiplier, Divisor};
        };

        using exact_enlarger = enlarger<1, 1>;

        namespace endian
        {
            struct big : option<big>
            {
                    constexpr static auto value = std::endian::big;
            };

            struct little : option<little>
            {
                    constexpr static auto value = std::endian::little;
            };

            using network = big;

            using native = std::conditional_t<std::endian::native == std::endian::little, little, big>;

            using swapped = std::conditional_t<std::endian::native == std::endian::little, big, little>;
        } // namespace endian

        struct no_fit_size : option<no_fit_size>
        {
        };

        struct no_enlarge_overflow : option<no_enlarge_overflow>
        {
        };

        struct enlarge_overflow : option<enlarge_overflow>
        {
        };

        struct no_size : option<no_size>
        {
                using default_size_type = void;
        };

        struct size1b : option<size1b>
        {
                using default_size_type = unsigned char;
        };

        struct size2b : option<size2b>
        {
                using default_size_type = std::uint16_t;
        };

        struct size4b : option<size4b>
        {
                using default_size_type = std::uint32_t;
        };

        struct size8b : option<size8b>
        {
                using default_size_type = std::uint64_t;
        };

        struct size_native : option<size_native>
        {
                using default_size_type = std::size_t;
        };
    } // namespace options

    template <typename Type> constexpr auto access::number_of_members()
    {
        using type = std::remove_cvref_t<Type>;
        if constexpr (std::is_array_v<type>)
        {
            return std::extent_v<type>;
        }
        else if constexpr (!std::is_class_v<type>)
        {
            return 0;
        }
        else if constexpr (concepts::container<type> && concepts::has_fixed_nonzero_size<type>)
        {
            return type {}.size();
        }
        else if constexpr (concepts::tuple<type>)
        {
            return std::tuple_size_v<type>;
        }
        else if constexpr (requires {
                               requires std::same_as<typename type::serialize, members<type::serialize::value>>;
                               requires type::serialize::value != std::numeric_limits<std::size_t>::max();
                           })
        {
            return type::serialize::value;
        }
        else if constexpr (requires(Type &&item) {
                               requires std::same_as<decltype(try_serialize(item)),
                                                     members<decltype(try_serialize(item))::value>>;
                               requires decltype(try_serialize(item))::value != std::numeric_limits<std::size_t>::max();
                           })
        {
            return decltype(serialize(std::declval<type>()))::value;
        }
        else if constexpr (requires {
                               requires std::same_as<typename type::serialize,
                                                     protocol<type::serialize::value, type::serialize::members>>;
                               requires type::serialize::members != std::numeric_limits<std::size_t>::max();
                           })
        {
            return type::serialize::members;
        }
        else if constexpr (requires(Type &&item) {
                               requires std::same_as<decltype(try_serialize(item)),
                                                     protocol<decltype(try_serialize(item))::value,
                                                              decltype(try_serialize(item))::members>>;
                               requires decltype(try_serialize(item))::members !=
                                            std::numeric_limits<std::size_t>::max();
                           })
        {
            return decltype(serialize(std::declval<type>()))::members;
        }
        else if constexpr (!concepts::inspection_guarded<type>)
        {
            return decltype([](Type &&item) {
                auto &&[... members] = item;
                return std::integral_constant<std::size_t, sizeof...(members)> {};
            }(std::declval<Type>()))::value;
        }
        else
        {
            return -1;
        }
    }

    template <typename Type> struct access::byte_serializable_visitor
    {
            template <typename... Types> constexpr auto operator()()
            {
                using type = std::remove_cvref_t<Type>;

                if constexpr (concepts::empty<type>)
                {
                    return std::false_type {};
                }
                else if constexpr ((... || has_explicit_serialize<Types, traits::visitor<Types>>()))
                {
                    return std::false_type {};
                }
                else if constexpr ((... || !byte_serializable<Types>()))
                {
                    return std::false_type {};
                }
                else if constexpr ((0 + ... + sizeof(Types)) != sizeof(type))
                {
                    return std::false_type {};
                }
                else if constexpr ((... || concepts::empty<Types>))
                {
                    return std::false_type {};
                }
                else
                {
                    return std::true_type {};
                }
            }
    };

    template <typename Type> constexpr auto access::byte_serializable()
    {
        constexpr auto members_count = number_of_members<Type>();
        using type = std::remove_cvref_t<Type>;

        if constexpr (members_count < 0)
        {
            return false;
        }
        else if constexpr (!std::is_trivially_copyable_v<type>)
        {
            return false;
        }
        else if constexpr (has_explicit_serialize<type, traits::visitor<type>>())
        {
            return false;
        }
        else if constexpr (!requires {
                               requires std::integral_constant<
                                            int,
                                            (std::bit_cast<std::remove_all_extents_t<type>>(
                                                 std::array<std::byte, sizeof(std::remove_all_extents_t<type>)>()),
                                             0)>::value == 0;
                           })
        {
            return false;
        }
        else if constexpr (concepts::array<type>)
        {
            return byte_serializable<std::remove_cvref_t<decltype(std::declval<type>()[0])>>();
        }
        else if constexpr (members_count > 0)
        {
            return visit_members_types<type>(byte_serializable_visitor<type> {})();
        }
        else
        {
            return true;
        }
    }

    template <typename Type> struct access::endian_independent_byte_serializable_visitor
    {
            template <typename... Types> constexpr auto operator()()
            {
                using type = std::remove_cvref_t<Type>;

                if constexpr (concepts::empty<type>)
                {
                    return std::false_type {};
                }
                else if constexpr ((... || has_explicit_serialize<Types, traits::visitor<Types>>()))
                {
                    return std::false_type {};
                }
                else if constexpr ((... || !endian_independent_byte_serializable<Types>()))
                {
                    return std::false_type {};
                }
                else if constexpr ((0 + ... + sizeof(Types)) != sizeof(type))
                {
                    return std::false_type {};
                }
                else if constexpr ((... || concepts::empty<Types>))
                {
                    return std::false_type {};
                }
                else if constexpr (!concepts::byte_type<type>)
                {
                    return std::false_type {};
                }
                else
                {
                    return std::true_type {};
                }
            }
    };

    template <typename Type> constexpr auto access::endian_independent_byte_serializable()
    {
        constexpr auto members_count = number_of_members<Type>();
        using type = std::remove_cvref_t<Type>;

        if constexpr (members_count < 0)
        {
            return false;
        }
        else if constexpr (!std::is_trivially_copyable_v<type>)
        {
            return false;
        }
        else if constexpr (has_explicit_serialize<type, traits::visitor<type>>())
        {
            return false;
        }
        else if constexpr (!requires {
                               requires std::integral_constant<
                                            int,
                                            (std::bit_cast<std::remove_all_extents_t<type>>(
                                                 std::array<std::byte, sizeof(std::remove_all_extents_t<type>)>()),
                                             0)>::value == 0;
                           })
        {
            return false;
        }
        else if constexpr (concepts::array<type>)
        {
            return endian_independent_byte_serializable<std::remove_cvref_t<decltype(std::declval<type>()[0])>>();
        }
        else if constexpr (members_count > 0)
        {
            return visit_members_types<type>(endian_independent_byte_serializable_visitor<type> {})();
        }
        else
        {
            return concepts::byte_type<type>;
        }
    }

    template <typename Type, typename Self, typename... Visited> struct access::self_referencing_visitor
    {
            template <typename... Types> constexpr auto operator()()
            {
                using type = std::remove_cvref_t<Type>;
                using self = std::remove_cvref_t<Self>;

                if constexpr (concepts::empty<type>)
                {
                    return std::false_type {};
                }
                else if constexpr ((... || concepts::type_references<std::remove_cvref_t<Types>, self>))
                {
                    return std::true_type {};
                }
                else if constexpr ((sizeof...(Visited) != 0) && (... || std::same_as<type, Visited>))
                {
                    return std::false_type {};
                }
                else if constexpr ((... || self_referencing<std::remove_cvref_t<Types>, self, type, Visited...>()))
                {
                    return std::true_type {};
                }
                else
                {
                    return std::false_type {};
                }
            }
    };

    template <typename Type, typename Self /* = Type*/, typename... Visited> constexpr auto access::self_referencing()
    {
        constexpr auto members_count = number_of_members<Type>();
        using type = std::remove_cvref_t<Type>;
        using self = std::remove_cvref_t<Self>;

        if constexpr (members_count < 0)
        {
            return false;
        }
        else if constexpr (has_explicit_serialize<type, traits::visitor<type>>())
        {
            return false;
        }
        else if constexpr (members_count == 0)
        {
            return false;
        }
        else if constexpr (concepts::array<type>)
        {
            return self_referencing<std::remove_cvref_t<decltype(std::declval<type>()[0])>, self, Visited...>();
        }
        else
        {
            return visit_members_types<type>(self_referencing_visitor<type, self, Visited...> {})();
        }
    }

    template <typename Type> constexpr auto number_of_members()
    {
        return access::number_of_members<Type>();
    }

    constexpr decltype(auto) visit_members(auto &&object, auto &&visitor)
    {
        return access::visit_members(object, visitor);
    }

    template <typename Type> constexpr decltype(auto) visit_members_types(auto &&visitor)
    {
        return access::visit_members_types<Type>(visitor);
    }

    template <typename Type> struct optional_ptr : std::unique_ptr<Type>
    {
            using base = std::unique_ptr<Type>;
            using base::base;
            using base::operator=;

            constexpr optional_ptr(base &&other) noexcept : base(std::move(other))
            {
            }
    };

    template <typename Type, typename...> optional_ptr(Type *) -> optional_ptr<Type>;

    template <typename Type, typename...> optional_ptr(std::unique_ptr<Type>) -> optional_ptr<Type>;

    template <typename Archive, typename Type>
    constexpr auto serialize(Archive &archive, const optional_ptr<Type> &self)
        requires(Archive::kind() == kind::out)
    {
        if (!self) [[unlikely]]
        {
            return archive(std::byte(false));
        }
        else
        {
            return archive(std::byte(true), *self);
        }
    }

    template <typename Archive, typename Type>
    constexpr auto serialize(Archive &archive, optional_ptr<Type> &self)
        requires(Archive::kind() == kind::in)
    {
        std::byte has_value {};
        if (auto result = archive(has_value); failure(result)) [[unlikely]]
        {
            return result;
        }

        if (!bool(has_value)) [[unlikely]]
        {
            self = {};
            return errc {};
        }

        if (auto result = archive(static_cast<std::unique_ptr<Type> &>(self)); failure(result)) [[unlikely]]
        {
            return result;
        }

        return errc {};
    }

    template <typename Type, typename SizeType> struct sized_item : public Type
    {
            using Type::Type;
            using Type::operator=;

            constexpr sized_item(Type &&other) noexcept(std::is_nothrow_move_constructible_v<Type>)
                : Type(std::move(other))
            {
            }

            constexpr sized_item(const Type &other) : Type(other)
            {
            }

            constexpr static auto serialize(auto &archive, auto &self)
            {
                if constexpr (std::remove_cvref_t<decltype(archive)>::kind() == kind::out)
                {
                    return archive.template serialize_one<SizeType>(static_cast<const Type &>(self));
                }
                else
                {
                    return archive.template serialize_one<SizeType>(static_cast<Type &>(self));
                }
            }
    };

    template <typename Type, typename SizeType>
    auto serialize(const sized_item<Type, SizeType> &) -> members<number_of_members<Type>()>;

    template <typename Type, typename SizeType> using sized_t = sized_item<Type, SizeType>;

    template <typename Type> using unsized_t = sized_t<Type, void>;

    template <typename Type, typename SizeType> struct sized_item_ref
    {
            constexpr explicit sized_item_ref(Type &&value) : value(std::forward<Type>(value))
            {
            }

            constexpr static auto serialize(auto &serializer, auto &self)
            {
                return serializer.template serialize_one<SizeType>(self.value);
            }

            Type &&value;
    };

    template <typename SizeType, typename Type> constexpr auto sized(Type &&value)
    {
        return sized_item_ref<Type &, SizeType>(value);
    }

    template <typename Type> constexpr auto unsized(Type &&value)
    {
        return sized_item_ref<Type &, void>(value);
    }
} // namespace zpp::bits
