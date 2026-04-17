#pragma once

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/// Configuration file key for the port to listen on.
static constexpr auto CONFIG_PORT = "port";

/// Configuration file key for the content-type map.
static constexpr auto CONFIG_CONTENT_TYPES = "content-types";

/// Key specifying the filename regex for a content type.
static constexpr auto CONFIG_CONTENT_TYPE_FILENAME = "if";

/// Key specifying the content type used for matching filenames.
static constexpr auto CONFIG_CONTENT_TYPE_RESULT = "then";

/// Configuration file key for the content-type map.
static constexpr auto CONFIG_CUSTOM_STRUCTURES = "custom-structures";

/// Key that is used to configure the input subsystem.
static constexpr auto CONFIG_INPUT = "input";

/// Subkey of input that defines binary inputs. If specified, it must map to an
/// object with semantical mix names from the game as the keys, and
/// player-friendly input names as the values (the game will show these names in
/// input hints).
static constexpr auto CONFIG_INPUT_BINARY = "binary";

/// Like CONFIG_INPUT_BINARY but for floating-point mixes.
static constexpr auto CONFIG_INPUT_FLOAT = "float";

/// Format specification key that specifies a static value.
static constexpr auto CONFIG_FORMAT_STATIC = "static";

/// Format specification key that specifies the key to take data from.
static constexpr auto CONFIG_FORMAT_KEY = "key";

/// Format specification key that specifies whether to disambiguate keys as
/// a structure or as a single value.
static constexpr auto CONFIG_FORMAT_STRUCT = "struct";

/// Format specification key that specifies a numeric offset be applied.
static constexpr auto CONFIG_FORMAT_OFFSET = "offset";

/// Format specification key that specifies scaling be applied.
static constexpr auto CONFIG_FORMAT_SCALE = "scale";

/// Format specification key that specifies a misc operator be applied.
static constexpr auto CONFIG_FORMAT_OPERATOR = "operator";

/// Format specification operator that specifies casting a value to a boolean,
/// using rounding to nearest integer for floats prior to the cast, and ignoring
/// null.
static constexpr auto CONFIG_OPERATOR_BOOL = "bool";

/// Complementary operator to CONFIG_OPERATOR_BOOL.
static constexpr auto CONFIG_OPERATOR_ZERO = "zero";

/// Format specification operator that specifies casting a value to an integer
/// by rounding to nearest.
static constexpr auto CONFIG_OPERATOR_ROUND = "round";

/// Format specification operator that specifies converting game time to an ISO
/// date starting at 0001-01-01T00:00:00Z.
static constexpr auto CONFIG_OPERATOR_DATE = "date";

/// Configuration object for TruckTel.
struct Configuration {
    /// Port to listen on.
    uint16_t port;

    /// Document root to serve from. Empty disables serving static files.
    std::filesystem::path document_root;

    /// Filename to content-type mapping.
    std::vector<std::pair<std::regex, std::string>> content_types;

    /// Custom output structures.
    nlohmann::json custom_structures;

    /// Input configuration.
    nlohmann::json input_configuration;
};

/// Loads the configuration file. If the file does not exist, an attempt is
/// first made to write the default configuration file, and then that is loaded
/// as it normally would be if successful.
Configuration load_config_file(const std::filesystem::path &path);
