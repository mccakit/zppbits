export module zpp.bits:concepts;
import std;
import :core;
import :traits;

export namespace zpp::bits
{
    namespace concepts
    {
        template <typename Type>
        concept byte_type =
            std::same_as<std::remove_cv_t<Type>, char> || std::same_as<std::remove_cv_t<Type>, unsigned char> ||
            std::same_as<std::remove_cv_t<Type>, std::byte>;

        template <typename Type>
        concept byte_view = byte_type<typename std::remove_cvref_t<Type>::value_type> && requires(Type value) {
            value.data();
            value.size();
        };

        template <typename Type>
        concept has_serialize = access::has_serialize<Type, traits::visitor<std::remove_cvref_t<Type>>>();

        template <typename Type>
        concept has_explicit_serialize =
            access::has_explicit_serialize<Type, traits::visitor<std::remove_cvref_t<Type>>>();

        template <typename Type>
        concept variant = !has_serialize<Type> && requires(Type variant) {
            variant.index();
            std::get_if<0>(&variant);
            std::variant_size_v<std::remove_cvref_t<Type>>;
        };

        template <typename Type>
        concept expected = !has_serialize<Type> && requires(Type expected) {
            typename std::remove_cvref_t<Type>::value_type;
            typename std::remove_cvref_t<Type>::error_type;
            typename std::remove_cvref_t<Type>::unexpected_type;
            expected.value();
            expected.error();
            expected.has_value();
            expected.operator bool();
            expected.operator*();
        };

        template <typename Type>
        concept optional = !has_serialize<Type> && !expected<Type> && requires(Type optional) {
            optional.value();
            optional.has_value();
            optional.operator bool();
            optional.operator*();
        };

        template <typename Type>
        concept container = !has_serialize<Type> && !optional<Type> && requires(Type container) {
            typename std::remove_cvref_t<Type>::value_type;
            container.size();
            container.begin();
            container.end();
        };

        template <typename Type>
        concept associative_container =
            container<Type> && requires(Type container) { typename std::remove_cvref_t<Type>::key_type; };

        template <typename Type>
        concept tuple = !has_serialize<Type> && !container<Type> && requires(Type tuple) {
            sizeof(std::tuple_size<std::remove_cvref_t<Type>>);
        } && !requires(Type tuple) { tuple.index(); };

        template <typename Type>
        concept owning_pointer = !optional<Type> && (traits::is_unique_ptr<std::remove_cvref_t<Type>>::value ||
                                                     traits::is_shared_ptr<std::remove_cvref_t<Type>>::value);

        template <typename Type>
        concept bitset = !has_serialize<Type> && requires(std::remove_cvref_t<Type> bitset) {
            bitset.flip();
            bitset.set();
            bitset.test(0);
            bitset.to_ullong();
        };

        template <typename Type>
        concept has_protocol = access::has_protocol<Type>();

        template <typename Type>
        concept by_protocol = has_protocol<Type> && !has_explicit_serialize<Type>;

        template <typename Type>
        concept basic_array = std::is_array_v<std::remove_cvref_t<Type>>;

        template <typename Type>
        concept unspecialized =
            !container<Type> && !owning_pointer<Type> && !tuple<Type> && !variant<Type> && !optional<Type> &&
            !expected<Type> && !bitset<Type> && !std::is_array_v<std::remove_cvref_t<Type>> && !by_protocol<Type>;

        template <typename Type>
        concept empty = requires {
            std::integral_constant<std::size_t, sizeof(Type)>::value;
            requires std::is_empty_v<std::remove_cvref_t<Type>>;
        };

        template <typename Type>
        concept byte_serializable = access::byte_serializable<Type>();

        template <typename Type>
        concept endian_independent_byte_serializable = access::endian_independent_byte_serializable<Type>();

        template <typename Archive>
        concept endian_aware_archive = requires { requires std::remove_cvref_t<Archive>::endian_aware; };

        template <typename Archive, typename Type>
        concept serialize_as_bytes =
            endian_independent_byte_serializable<Type> || (!endian_aware_archive<Archive> && byte_serializable<Type>);

        template <typename Type, typename Reference>
        concept type_references = requires {
            requires container<Type>;
            requires std::same_as<typename std::remove_cvref_t<Type>::value_type, std::remove_cvref_t<Reference>>;
        } || requires {
            requires associative_container<Type>;
            requires std::same_as<typename std::remove_cvref_t<Type>::key_type, std::remove_cvref_t<Reference>>;
        } || requires {
            requires associative_container<Type>;
            requires std::same_as<typename std::remove_cvref_t<Type>::mapped_type, std::remove_cvref_t<Reference>>;
        } || requires(Type &&value) {
            requires owning_pointer<Type>;
            requires std::same_as<std::remove_cvref_t<decltype(*value)>, std::remove_cvref_t<Reference>>;
        } || requires(Type &&value) {
            requires optional<Type>;
            requires std::same_as<std::remove_cvref_t<decltype(*value)>, std::remove_cvref_t<Reference>>;
        };

        template <typename Type>
        concept self_referencing = access::self_referencing<Type>();

        template <typename Type>
        concept has_fixed_nonzero_size =
            requires { requires std::integral_constant<std::size_t, std::remove_cvref_t<Type> {}.size()>::value != 0; };

        template <typename Type>
        concept array = basic_array<Type> || (container<Type> && has_fixed_nonzero_size<Type> && requires {
                            requires Type {}
                            .size() * sizeof(typename Type::value_type) == sizeof(Type);
                            Type {}.data();
                        });

        template <typename Type>
        concept inspection_guarded = (container<Type> && !array<Type>) || owning_pointer<Type> || variant<Type> ||
                                     optional<Type> || bitset<Type>;

    } // namespace concepts
} // namespace zpp::bits
