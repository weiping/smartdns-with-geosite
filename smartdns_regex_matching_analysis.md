# Regex Initialization and Storage

SmartDNS employs a centralized system for managing and matching regular expressions, primarily used in features like `geosite` domain sets. This section details how these regex patterns are initialized, compiled, and stored.

## 1. Global Regex List

The foundation of SmartDNS's regex handling is a global list designed to store all compiled regular expression patterns.

*   **Initialization via `dns_regexp_init()`**:
    *   Located in `src/regexp.c`, the `dns_regexp_init()` function is responsible for setting up this global regex management system.
    *   Its primary action is to initialize a global head for a linked list, specifically `dns_regexp_head.regexp_list`. This list will subsequently hold all the compiled regex patterns that SmartDNS uses.
    *   `dns_regexp_init()` is typically called once during the SmartDNS startup sequence, ensuring that the regex infrastructure is ready before any configuration files are parsed or rules are processed.

## 2. Regex Insertion and Compilation

When SmartDNS encounters a regex pattern in its configuration (e.g., from `geosite` files), it uses a specific function to compile and store it.

*   **`dns_regexp_insert(char *regexp)`**:
    *   This function, also found in `src/regexp.c`, is the entry point for adding a new regular expression to the system.
    *   It is invoked whenever a regex pattern needs to be processed and stored. A key example is during the parsing of `geosite` type domain set files by `_config_domain_rule_each_from_geosite` (in `src/dns_conf/set_file.c`). When this parser encounters lines with `keyword:` or `regexp:` prefixes, the extracted pattern is passed to `dns_regexp_insert()`.

*   **Process within `dns_regexp_insert()`**:
    1.  **Allocation**: A new `struct dns_regexp` is dynamically allocated. This structure is defined in `src/regexp.h` and serves as a container for both the original regex string and its compiled form.
    2.  **Compilation with RE2**:
        *   The core of the processing involves compiling the input `regexp` string (e.g., `^example.*\.com$`) into a compiled pattern. SmartDNS uses the RE2 library for this purpose.
        *   The compilation is performed by `cre2_new(regexp, strlen(regexp), NULL)`, which returns a `cre2_regexp_t* rex` object. RE2 is chosen for its operational efficiency and, critically, its protection against Regular Expression Denial of Service (ReDoS) attacks, which can occur with overly complex patterns in some other regex engines.
    3.  **Error Handling**: After attempting compilation, `cre2_error_code(rex)` is checked. If the error code is not `CRE2_ERROR_NONE`, it indicates a compilation failure (e.g., invalid regex syntax). In such cases, an error message is logged, the partially allocated `rex` object is freed using `cre2_delete(rex)`, the `struct dns_regexp` is freed, and the function returns an error code.
    4.  **Storage**: If compilation is successful:
        *   The original `regexp` string is duplicated (using `strdup()`) and stored in the `pattern` field of the `struct dns_regexp`.
        *   The compiled `cre2_regexp_t* rex` object is stored in the `rex` field of the same structure.
        *   The `pattern_len` (length of the original string) is also stored.
    5.  **List Addition**: The newly populated `struct dns_regexp` (containing both the original string and the compiled RE2 object) is then added to the global linked list `dns_regexp_head.regexp_list`. This makes the compiled regex available for subsequent matching operations.
    6.  **Regex Count and Limit**: A counter for the number of stored regexes (`dns_regexp_head.num`) is incremented. SmartDNS imposes a limit on the total number of regexes that can be stored, defined by `DNS_MAX_REGEXP_NUM`. If this limit is reached, `dns_regexp_insert()` will log an error and refuse to add more regexes.

This mechanism ensures that all regex patterns are pre-compiled and stored efficiently, ready to be used for matching against DNS query domains. The use of RE2 provides a balance of performance and security for this critical function.

# Regex Matching During a DNS Request

Once regular expressions are initialized and stored, SmartDNS can use them to match against incoming DNS query domains. This matching primarily occurs within the rule processing logic.

## 1. Invocation Context

*   **Primary Handler: `_dns_server_get_domain_rule_by_domain_ext`**:
    *   The core function responsible for determining which rule, if any, applies to a given DNS query domain is `_dns_server_get_domain_rule_by_domain_ext`, located in `src/dns_server/rules.c`.
    *   This function first attempts to find a match using more direct methods, such as looking up the domain and its parent domains in an ART (Adaptive Radix Tree) for exact or suffix matches. These correspond to `full:` and `domain:` entries in `geosite` files, or `domain-rules` directly defined with domain names.
    *   Regex matching is typically a secondary step. If the initial ART lookups do not result in a definitive match (indicated by `!walk_args.match`), the function then proceeds to check if any of the globally loaded regular expressions match the query domain.

## 2. Checking for Loaded Regexes

*   **`has_regexp()`**:
    *   Before attempting any regex matching, `_dns_server_get_domain_rule_by_domain_ext` calls `has_regexp()` (defined in `src/regexp.c`).
    *   This function is a simple check that returns true if `dns_regexp_head.num > 0` (i.e., if at least one regex pattern has been successfully compiled and stored in the global list). This prevents unnecessary calls to the matching logic if no regexes are loaded.

## 3. The `dns_regexp_match` Function Call

*   **Invocation**: If `has_regexp()` returns true, indicating that compiled regex patterns are available, `_dns_server_get_domain_rule_by_domain_ext` then calls `dns_regexp_match(domain, regexp_out_str)`.
    *   `domain`: This is the current DNS query domain string (e.g., `www.example.com`).
    *   `regexp_out_str`: This is a character buffer provided by the caller. If a match is found, `dns_regexp_match` will populate this buffer with the *original regex string* that matched the domain. The size of this buffer is `DOMAIN_NAME_LEN`.

## 4. Internal Logic of `dns_regexp_match`

The `dns_regexp_match` function (in `src/regexp.c`) implements the core matching logic:

*   **Iteration**: It iterates through the global linked list `dns_regexp_head.regexp_list`, examining each stored `struct dns_regexp` entry one by one.
*   **Matching with `cre2_match`**:
    *   For each `struct dns_regexp` in the list, it uses the pre-compiled RE2 pattern (`dns_regexp->rex`) to test against the live `domain` string.
    *   The actual matching is performed by the RE2 library function `cre2_match(dns_regexp->rex, domain, strlen(domain), 0, strlen(domain), CRE2_UNANCHORED, NULL, 0)`.
    *   **`CRE2_UNANCHORED`**: This option is crucial. It means the regex pattern can match anywhere within the `domain` string. If the pattern requires matching from the start or end of the domain, anchors (`^` for start, `$` for end) must be explicitly included in the regex pattern itself (e.g., `^ads\.` or `\.com$`).
*   **Handling a Match**:
    *   `cre2_match()` returns `1` if the pattern matches the domain and `0` if it does not.
    *   If a match occurs (`cre2_match()` returns `1`):
        *   The original regex string that caused the match, which was stored as `dns_regexp->pattern`, is copied into the output parameter `regexp_out_str` using `strncpy()`.
        *   `dns_regexp_match` immediately returns `0`, indicating success and that a matching regex was found. It does not continue checking other regexes in the list; the first match takes precedence.
*   **No Match**: If the function iterates through all the compiled regexes in the list and none of them match the input `domain`, `dns_regexp_match` returns `-1`.

## 5. Outcome for `_dns_server_get_domain_rule_by_domain_ext`

*   The return value from `dns_regexp_match` and the content of `regexp_out_str` are critical for the subsequent logic in `_dns_server_get_domain_rule_by_domain_ext`:
    *   If `dns_regexp_match` returns `0` (success), it means `regexp_out_str` now contains the original string of the regex pattern that matched the query domain. This original regex string is then used by `_dns_server_get_domain_rule_by_domain_ext` to perform a *second lookup* in the ART tree. This is because the ART tree stores rules indexed by the original domain/regex string (after transformation, e.g., reversal). This second lookup retrieves the actual rule actions (like specific nameservers or ipsets) associated with that regex pattern.
    *   If `dns_regexp_match` returns `-1` (no match), `_dns_server_get_domain_rule_by_domain_ext` knows that no loaded regex pattern matched the current domain, and it will proceed with other rule-finding logic or default handling.

This two-stage process (first, matching against compiled regexes; second, looking up the original matched regex string in the ART tree) is a key aspect of how SmartDNS efficiently applies rules associated with complex patterns.

# Linking Matched Regexes to DNS Rules

When `dns_regexp_match()` successfully matches a query domain against a compiled regex pattern, it returns the *original regex string* (e.g., `^ad.*\.example\.com$`) that was used to generate the compiled pattern. This string is the crucial link to retrieve the actual DNS rules (like nameservers, ipsets, etc.) associated with that pattern. This linkage happens in a second lookup step within `_dns_server_get_domain_rule_by_domain_ext`.

## 1. Context: Post-Regex Match

*   Following a successful call to `dns_regexp_match(domain, regexp_out_str)` within `_dns_server_get_domain_rule_by_domain_ext`, the `regexp_out_str` variable holds the original, human-readable regex pattern string.
*   The system now needs to find the specific set of actions (e.g., use nameserver 8.8.8.8, add to ipset 'gfwlist') that the user configured for this pattern.

## 2. The Second ART Tree Lookup

This is where the original regex string plays its vital role:

*   **Input for Lookup**: The `_dns_server_get_domain_rule_by_domain_ext` function takes the `regexp_out_str` (containing the original regex pattern).
*   **Key Preparation**: This string is then processed by `_config_setup_domain_key_for_search()` (or a similar internal function like `_config_setup_domain_key()`). This preparation is the same as for normal domain names when they are inserted or searched in the ART tree:
    *   The string is typically reversed. For example, `^ad.*\.example\.com$` might become `moc.elpmaxe.\*da.^`.
    *   A dot (`.`) might be prepended or appended depending on the exact internal conventions for keys in the ART tree.
    *   The key idea is that this transformed version of the *original regex string* is what was used as the key when the rule was initially stored.
*   **ART Tree Search**: A search is then performed on the main domain rule ART tree, typically `conf->domain_rule.tree` (or `_config_current_rule_group()->domain_rule.tree` depending on the context). This search uses the transformed original regex string as the lookup key. Functions like `art_substring_search_ext_str` (which is called by `art_substring_walk` in `_dns_server_get_domain_rule_by_domain_ext`) are employed to find an exact match for this key in the tree.

## 3. Rationale for the Second Lookup

The necessity for this second lookup stems from how rules, especially those from `geosite` files, are loaded and stored:

*   **Configuration Loading Reminder**: During the configuration loading phase (e.g., when `_config_domain_rule_each_from_geosite` processes a `geosite` file line like `regexp:^ad.*\.example\.com$`):
    1.  The pattern `^ad.*\.example\.com$` is sent to `dns_regexp_insert()` to be compiled and stored in the global regex list (as `struct dns_regexp` containing both original and compiled forms).
    2.  Crucially, the *original string* `^ad.*\.example\.com$` is then passed to `_config_domain_rule_add_callback` and subsequently to `_config_domain_rule_add`.
    3.  `_config_domain_rule_add` (in `src/dns_conf/domain_rule.c`) takes this original regex string, prepares it using `_config_setup_domain_key` (reversing it, etc.), and inserts it as a key into the ART tree (`conf->domain_rule.tree`).
    4.  The value associated with this key in the ART tree is a pointer to the `struct request_domain_rule_args` which contains the actual DNS actions (nameservers, ipsets, flags, etc.) specified in the `domain-rules` line that referenced the `domain-set`.

*   **Separation of Concerns**:
    *   The global regex list (`dns_regexp_head.regexp_list`) is optimized for *fast matching* of many patterns against a live domain. It stores compiled RE2 objects.
    *   The ART tree (`conf->domain_rule.tree`) is optimized for *storing and retrieving rules* based on keys (which can be domains, or, in this case, original regex strings).
    *   The original regex string acts as the bridge: `dns_regexp_match` finds *which* pattern matched, and the subsequent ART lookup uses that pattern's string to find *what to do* for that match.

## 4. Outcome and Rule Application

*   **Successful Lookup**: If the configuration was loaded correctly, the transformed original regex string (from `regexp_out_str`) will be found as a key in the ART tree.
*   **Populating Rules**: The value retrieved from the ART tree (e.g., via `walk_args.args` in `_dns_server_get_domain_rule_by_domain_ext`, which is often a `struct request_domain_rule *`) will point to the structure containing the DNS rules associated with that specific regex.
*   **Rule Application**: With the rules now retrieved, the DNS server can proceed to apply them to the current DNS request (e.g., forwarding the query to the specified nameservers, adding the resulting IPs to an ipset, etc.).

This two-step mechanism allows SmartDNS to efficiently handle a large number of potentially complex regex patterns without embedding rule data directly with compiled regex objects, keeping the matching and rule storage/retrieval processes distinct and optimized.

# Summary of Regex Application and Performance Considerations

The application of regular expressions in SmartDNS for domain matching is a powerful feature, primarily utilized through `geosite` domain sets. Understanding its flow and performance implications is key for effective configuration.

## 1. Concise Overview of the Flow

1.  **DNS Request Arrival**: A DNS query for a specific domain is received by SmartDNS.
2.  **Initial ART Lookup**: The system first attempts to match the domain (and its parent domains) against rules stored in an Adaptive Radix Tree (ART). This tree contains rules from direct `domain-rules` configurations and non-regex entries from `domain-set` files (like `full:` and `domain:` types).
3.  **Fallback to Regex Matching**: If the initial ART lookup does not yield a match, and if regex patterns are loaded (checked via `has_regexp()`), the system proceeds to regex matching.
4.  **Regex List Iteration (`dns_regexp_match`)**: The query domain is compared against each pre-compiled regex pattern stored in the global `dns_regexp_head.regexp_list`.
5.  **Original String Retrieval**: If a compiled regex matches the domain, `dns_regexp_match` returns the *original regex string* associated with that compiled pattern.
6.  **Second ART Lookup for Rules**: This original regex string is then transformed (e.g., reversed) and used as a key for a second lookup in the main domain rule ART tree (`conf->domain_rule.tree`).
7.  **Rule Application**: If this second lookup is successful, it retrieves the specific DNS actions (nameservers, ipsets, etc.) associated with that regex, and these rules are then applied to the DNS query.

## 2. The Two-Stage Lookup for Regexes

It's important to emphasize that applying rules based on regex patterns involves a two-stage process:

*   **Stage 1: Pattern Matching (`dns_regexp_match`)**:
    *   The incoming query domain is matched against the list of *compiled* regular expression objects.
    *   This stage identifies *if* any regex pattern matches and *which* one it is by returning its original string representation.

*   **Stage 2: Rule Retrieval (ART Lookup)**:
    *   The *original string* of the matched regex pattern is then used as a key to look up the actual DNS rule (actions, parameters) in the main ART tree.
    *   This stage determines *what to do* now that a regex match has occurred.

## 3. Performance Considerations

*   **Computational Cost**: Regex matching is inherently more computationally intensive than the simple string prefix/suffix matching employed by ART tree lookups for direct domain rules. Each regex pattern evaluation can involve complex state machine transitions.
*   **Fallback Mechanism**: Due to this higher cost, regex matching is strategically positioned as a fallback mechanism in SmartDNS. It's attempted only after faster ART-based lookups for specific domains or domain suffixes have failed to produce a match. This design prioritizes performance for common cases.
*   **RE2 Library Benefits**: The use of the RE2 regex library is a significant advantage. RE2 is designed for efficiency and, crucially, provides protection against Regular Expression Denial of Service (ReDoS) attacks. ReDoS can occur when poorly written or malicious regex patterns cause exponential backtracking in some regex engines, leading to extreme CPU usage. RE2's linear-time matching algorithm avoids this, mitigating major performance and security concerns.
*   **Judicious Use Recommended**: Despite RE2's strengths, the sheer number and complexity of regex patterns can still impact performance.
    *   Each regex added to the global list means another pattern to check for queries that don't match earlier rules.
    *   Highly complex regex patterns, even if safe from ReDoS in RE2, can still take more time to evaluate than simpler ones.
    *   Therefore, while `keyword:` and `regexp:` entries in `geosite` files offer great flexibility, they should be used judiciously. Prefer simpler `full:` or `domain:` matches where possible, and ensure regex patterns are as efficient and specific as practical. Large numbers of very broad or complex regexes could potentially become a bottleneck on very high-query-load systems.

By understanding this flow and the associated performance trade-offs, users can configure SmartDNS to leverage the power of regex matching effectively while maintaining optimal DNS resolution speed.
