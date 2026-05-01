#include "domain/auth/auth_service.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <mutex>

namespace auth {
namespace {

constexpr int kSessionTtlHours = 24 * 14;

bool is_username_char(unsigned char ch) {
    return std::isalnum(ch) || ch == '_' || ch == '-';
}

std::string trim_copy(std::string text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

} // namespace

AuthService::AuthService(AuthRepository& repository)
    : repository_(repository) {
    ensureSodiumReady();
}

LoginResult AuthService::registerUser(
    const std::string& username,
    const std::optional<std::string>& email,
    const std::string& display_name,
    const std::string& password
) const {
    const auto clean_username = trim_copy(username);
    const auto clean_display_name = trim_copy(display_name).empty()
        ? clean_username
        : trim_copy(display_name);
    validateNewUser(clean_username, clean_display_name, password);

    const auto password_hash = hashPassword(password);
    const auto user_id = repository_.createUser(
        clean_username,
        email,
        clean_display_name,
        password_hash,
        "user"
    );

    auto user = repository_.findUserById(user_id);
    if (!user) {
        throw AuthError{"created user cannot be loaded"};
    }

    const auto token = generateToken();
    const auto expires_at = repository_.createSession(
        user->id,
        hashToken(token),
        std::nullopt,
        std::nullopt,
        kSessionTtlHours
    );

    return LoginResult{
        .user = toPublicUser(*user),
        .token = token,
        .expires_at = expires_at
    };
}

LoginResult AuthService::login(
    const std::string& identifier,
    const std::string& password,
    const std::optional<std::string>& user_agent,
    const std::optional<std::string>& ip_address
) const {
    const auto user = repository_.findUserByIdentifier(trim_copy(identifier));
    if (!user || !verifyPassword(user->password_hash, password)) {
        throw AuthError{"invalid username or password"};
    }
    if (user->status != "active") {
        throw AuthError{"user is disabled"};
    }

    const auto token = generateToken();
    const auto expires_at = repository_.createSession(
        user->id,
        hashToken(token),
        user_agent,
        ip_address,
        kSessionTtlHours
    );
    repository_.markLogin(user->id);

    return LoginResult{
        .user = toPublicUser(*user),
        .token = token,
        .expires_at = expires_at
    };
}

std::optional<PublicUser> AuthService::currentUser(const std::string& bearer_token) const {
    if (bearer_token.empty()) {
        return std::nullopt;
    }

    const auto token_hash = hashToken(bearer_token);
    auto user = repository_.findUserByActiveSession(token_hash);
    if (!user) {
        return std::nullopt;
    }

    repository_.touchSession(token_hash);
    return toPublicUser(*user);
}

bool AuthService::logout(const std::string& bearer_token) const {
    if (bearer_token.empty()) {
        return false;
    }
    return repository_.revokeSession(hashToken(bearer_token));
}

std::string AuthService::hashPassword(const std::string& password) {
    ensureSodiumReady();
    std::array<char, crypto_pwhash_STRBYTES> output{};
    if (crypto_pwhash_str(
            output.data(),
            password.c_str(),
            password.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        ) != 0) {
        throw AuthError{"password hashing failed"};
    }
    return output.data();
}

bool AuthService::verifyPassword(const std::string& password_hash, const std::string& password) {
    ensureSodiumReady();
    return crypto_pwhash_str_verify(
        password_hash.c_str(),
        password.c_str(),
        password.size()
    ) == 0;
}

std::string AuthService::generateToken() {
    ensureSodiumReady();
    std::array<unsigned char, 32> token_bytes{};
    randombytes_buf(token_bytes.data(), token_bytes.size());

    std::array<char, sodium_base64_ENCODED_LEN(32, sodium_base64_VARIANT_URLSAFE_NO_PADDING)> output{};
    sodium_bin2base64(
        output.data(),
        output.size(),
        token_bytes.data(),
        token_bytes.size(),
        sodium_base64_VARIANT_URLSAFE_NO_PADDING
    );
    return output.data();
}

std::string AuthService::hashToken(const std::string& token) {
    ensureSodiumReady();
    std::array<unsigned char, crypto_generichash_BYTES> hash{};
    crypto_generichash(
        hash.data(),
        hash.size(),
        reinterpret_cast<const unsigned char*>(token.data()),
        token.size(),
        nullptr,
        0
    );

    std::array<char, crypto_generichash_BYTES * 2 + 1> hex{};
    sodium_bin2hex(hex.data(), hex.size(), hash.data(), hash.size());
    return hex.data();
}

void AuthService::ensureSodiumReady() {
    static std::once_flag once;
    static bool ready = false;

    std::call_once(once, [] {
        ready = sodium_init() >= 0;
    });

    if (!ready) {
        throw AuthError{"libsodium initialization failed"};
    }
}

void AuthService::validateNewUser(
    const std::string& username,
    const std::string& display_name,
    const std::string& password
) {
    if (username.size() < 3 || username.size() > 64) {
        throw AuthError{"username must be 3 to 64 characters"};
    }
    if (!std::all_of(username.begin(), username.end(), [](unsigned char ch) {
            return is_username_char(ch);
        })) {
        throw AuthError{"username may only contain letters, numbers, underscores, and hyphens"};
    }
    if (display_name.empty() || display_name.size() > 64) {
        throw AuthError{"display name must be 1 to 64 characters"};
    }
    if (password.size() < 8 || password.size() > 256) {
        throw AuthError{"password must be 8 to 256 characters"};
    }
}

} // namespace auth
