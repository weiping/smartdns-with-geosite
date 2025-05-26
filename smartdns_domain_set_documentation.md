# SmartDNS `domain-set` Configuration

The `domain-set` directive in the `smartdns.conf` file allows users to define collections of domains that can be used in various SmartDNS features, such as routing, filtering, or applying specific DNS servers.

## Basic Syntax

The basic syntax for the `domain-set` directive is as follows:

```
domain-set -name <set_name> -type <set_type> -file <file_path>
```

## Parameters

The `domain-set` directive requires the following parameters:

*   **`-name <set_name>`**: This parameter defines a unique name for the domain set. This name is used to reference the set in other parts of the SmartDNS configuration. The `<set_name>` must be unique across all domain sets.

*   **`-type <set_type>`**: This parameter specifies the type of the domain set, which determines how the data in the associated file is interpreted and processed. The supported types are `list`, `geosite`, and `geositelist`.

*   **`-file <file_path>`**: This parameter specifies the absolute or relative path to the data file containing the domain entries for this set. The format of this file depends on the `<set_type>` chosen.

## Supported `<set_type>` Options

SmartDNS supports the following types for domain sets:

### 1. `list`

*   **Description**: This is the simplest type of domain set. It is used for basic lists of domain names.
*   **File Format**: The data file for a `list` type set should contain one domain name per line. Lines starting with `#` are treated as comments and are ignored.
    ```
    # Example list file
    example.com
    another-example.org
    test.net
    ```

### 2. `geosite`

*   **Description**: This type is used for more complex domain definitions, often sourced from projects like V2Ray's GeoSite. It allows for different matching strategies for domains.
*   **File Format**: The data file for a `geosite` type set can include entries with specific matching types:
    *   `full:<domain>`: Matches the exact domain name.
    *   `domain:<domain>`: Matches the domain and all its subdomains (e.g., `domain:example.com` matches `example.com`, `www.example.com`, `sub.example.com`).
    *   `keyword:<keyword>`: Matches domains containing the specified keyword.
    *   `regexp:<pattern>`: Matches domains against the provided regular expression.
    ```
    # Example geosite file
    full:exactmatch.com
    domain:example.org # Matches example.org and its subdomains
    keyword:advertisement
    regexp:.*\.ad\.com$
    ```

### 3. `geositelist`

*   **Description**: This type is similar to `geosite` but has a more restricted set of supported entry types. It is designed for compatibility with certain data file formats that only use basic domain and full match entries.
*   **File Format**: The data file for a `geositelist` type set can include:
    *   `full:<domain>`: Matches the exact domain name.
    *   `<domain>` or `domain:<domain>`: Matches the domain and all its subdomains. Entries without a prefix are treated as `domain` type.
    ```
    # Example geositelist file
    full:exactmatch.com
    example.org # Treated as domain:example.org
    domain:another-example.net
    ```

## Geosite File Format Details

Files used with `domain-set` of type `geosite` are processed line by line. This format provides flexibility in defining how domains should be matched.

*   **Comments and Empty Lines**: Lines that start with a hash symbol (`#`) are considered comments and are ignored by the parser. Empty lines (lines containing only whitespace) are also ignored. This allows for clear and well-documented `geosite` files.

*   **Entry Prefixes**: Each non-comment, non-empty line is expected to be an entry that defines a domain or a pattern. The behavior of each entry is determined by its prefix:

    *   **`full:<domain>`**:
        *   **Purpose**: This prefix ensures an exact match for the specified domain. Only the exact domain name provided will be matched.
        *   **Example**: `full:example.com` will match `example.com` but not `www.example.com` or `sub.example.com`.

    *   **`domain:<domain>`**:
        *   **Purpose**: This prefix is used for suffix domain matching. It matches the specified domain and all its subdomains.
        *   **Example**: `domain:example.com` will match `example.com`, `www.example.com`, `sub.example.com`, `another.sub.example.com`, and so on.

    *   **`keyword:<text>`**:
        *   **Purpose**: This prefix allows for matching domains that contain the given `<text>` as a substring. SmartDNS internally converts this into a regular expression for matching.
        *   **Example**: `keyword:ad` will be converted to a regular expression like `^.*ad.*$`, which would match domains like `advert.com`, `tradex.org`, or `leadingads.io`.
        *   **Applicability**: This prefix is specific to the `geosite` type and is **not** supported by the `geositelist` type.

    *   **`regexp:<pattern>`**:
        *   **Purpose**: This prefix allows users to provide a full regular expression (`<pattern>`) for domain matching. This offers the most powerful and flexible matching capability.
        *   **Example**: `regexp:^ad[0-9]+\.example\.com$` would match `ad1.example.com`, `ad123.example.com`, but not `ad.example.com` or `adtext.example.com`.
        *   **Applicability**: This prefix is specific to the `geosite` type and is **not** supported by the `geositelist` type.

    *   **Lines Without Explicit Prefixes**:
        *   While `geosite` files are typically structured with explicit prefixes for clarity and precise control, if a line contains a domain name without one of the recognized prefixes (`full:`, `domain:`, `keyword:`, `regexp:`), SmartDNS's parsing logic (specifically `_config_domain_rule_each_from_geosite` which calls `_config_setup_domain_key`) will generally treat such an entry as a domain for suffix matching, similar to how `domain:` works. For example, an entry like `example.com` would typically match `example.com` and all its subdomains. However, for maximum clarity and to avoid ambiguity, using explicit prefixes is highly recommended in `geosite` files.

## Internal Processing of Geosite Data

Understanding how SmartDNS internally handles `geosite` data involves two main phases: the loading phase (when configuration is read) and the request matching phase (when DNS queries are processed).

### 1. Loading Phase

This phase describes how `domain-set` entries of type `geosite` are parsed and stored when SmartDNS starts or reloads its configuration.

1.  **Initial `domain-set` Processing**:
    *   When SmartDNS parses `smartdns.conf`, `domain-set` directives are processed by the `_config_domain_set` function.
    *   This function primarily stores the set's name (e.g., `myset`), its type (`geosite`), and the path to the associated data file. At this stage, the file itself is not yet read.

2.  **Triggering by `domain-rules`**:
    *   The actual loading and processing of the `geosite` file are deferred until a `domain-rules` directive references the `domain-set`.
    *   For example, a rule like `domain-rules /domain-set:myset/ -nameserver 8.8.8.8` will trigger the loading of the `myset` geosite data.

3.  **Delegation to `geosite` Parser**:
    *   The `_config_domain_rule_set_each` function (in `domain_rule.c`) handles the `domain-rules` entry. When it identifies that the rule pertains to a `domain-set` of type `geosite`, it calls `_config_domain_rule_each_from_geosite` (found in `set_file.c`) to process the file associated with that set.

4.  **Parsing the `geosite` File**:
    *   `_config_domain_rule_each_from_geosite` reads the specified `geosite` file line by line.
    *   For entries like `full:<domain>` and `domain:<domain>`, it extracts the domain part (e.g., `example.com`).
    *   For `keyword:<text>` and `regexp:<pattern>` entries:
        *   The pattern (e.g., `ad` from `keyword:ad`, or `^.*foo.*$` from `regexp:^.*foo.*$`) is passed to `dns_regexp_insert` (in `regexp.c`).
        *   `dns_regexp_insert` takes this pattern/keyword. If it's a `keyword`, it's first converted into a proper regex (e.g., `ad` becomes `^.*ad.*$`).
        *   It then compiles the regex pattern using the RE2 library via `cre2_new()`.
        *   The compiled RE2 object and the *original regex string* (e.g., `^.*ad.*$` or `^.*foo.*$`) are stored in a global linked list (`dns_regexp_head.regexp_list`). This list holds all compiled regex patterns from all `geosite` files.

5.  **Callback and Rule Addition**:
    *   After parsing each line in the `geosite` file, `_config_domain_rule_each_from_geosite` passes a string to `_config_domain_rule_add_callback`.
        *   For `full:<domain>` and `domain:<domain>` entries, this string is the extracted domain itself.
        *   For `keyword:<text>` and `regexp:<pattern>` entries, this string is the *original regex string* (e.g., `^.*ad.*$` or `^.*foo.*$`), not the compiled object.
    *   `_config_domain_rule_add_callback` then invokes `_config_domain_rule_add` (in `domain_rule.c`) with this string and the actions specified in the `domain-rules` directive (e.g., use nameserver X, add to ipset Y).

6.  **Storing in ART Tree**:
    *   `_config_domain_rule_add` takes the received string (either a domain or an original regex string).
    *   It uses `_config_setup_domain_key` to prepare this string for use as a key in the ART (Adaptive Radix Tree). This preparation involves:
        *   Reversing the domain/string: `example.com` becomes `com.example.`.
        *   For regex strings, they are also reversed: `^.*foo.*$` might become something like `$.*oof.*.^` (the exact transformation ensures proper sorting and matching within the ART).
    *   This transformed key is then inserted into an ART tree (`_config_current_rule_group()->domain_rule.tree`). The value associated with this key in the tree will be the set of actions defined by the `domain-rules` directive.

### 2. Request Matching Phase

This phase describes how SmartDNS uses the processed `geosite` data to match incoming DNS queries.

1.  **Entry Point for Rule Matching**:
    *   When a DNS request for a domain (e.g., `www.example.com`) arrives, the `_dns_server_get_domain_rule_by_domain_ext` function (in `dns_server/rules.c`) is a key function called to find any matching rules.

2.  **Direct ART Lookup (for `full:` and `domain:` types)**:
    *   The requested domain is first prepared by `_config_setup_domain_key_for_search` (similar to `_config_setup_domain_key` but for searching): it's reversed and a dot is prepended (e.g., `www.example.com` becomes `.com.example.www`).
    *   SmartDNS then performs a lookup in the ART tree using this prepared domain. This lookup will attempt to find matches for:
        *   Exact matches (corresponding to `full:<domain>` entries, e.g. `.com.example.www` matching a stored key `com.example.www.`).
        *   Suffix matches (corresponding to `domain:<domain>` entries, e.g. `.com.example.www` matching stored keys like `com.example.` or `com.example.www.`).

3.  **Regex Matching Logic (for `keyword:` and `regexp:` types)**:
    *   If the direct ART lookup does **not** yield a match AND the `has_regexp()` function (from `regexp.c`) indicates that there are compiled regular expressions loaded (i.e., `dns_regexp_head.regexp_list` is not empty):
        *   The system calls `dns_regexp_match(requested_domain, matched_regex_string_output)`. The `requested_domain` here is the original, non-reversed domain (e.g., `www.example.com`).
        *   `dns_regexp_match` iterates through the global list of compiled regex objects (`dns_regexp_head.regexp_list`) that were stored during the loading phase.
        *   For each compiled regex, it uses `cre2_match` to test if the `requested_domain` matches the pattern.
        *   If a match is found:
            *   `dns_regexp_match` returns `0` (success).
            *   Crucially, it populates `matched_regex_string_output` with the *original regex string* (e.g., `^.*foo.*$`) that was stored alongside the compiled object.
    *   If `dns_regexp_match` returns a match (i.e., `matched_regex_string_output` is populated):
        *   The `_dns_server_get_domain_rule_by_domain_ext` function now takes this `matched_regex_string_output`.
        *   This string (the original regex pattern) is then prepared using `_config_setup_domain_key_for_search` (reversed, e.g., `^.*foo.*$` becomes `$.*oof.*.^`).
        *   A **second lookup** is performed in the ART tree, this time using the (reversed) original regex string as the key.
        *   Since this exact (reversed) original regex string was inserted into the ART tree during the loading phase (step 6 of Loading Phase), this lookup will now find the entry and retrieve the associated rule actions (e.g., use nameserver X).

This two-step process for regexes (first matching against compiled patterns, then using the original matched pattern string to look up actions in the ART) allows SmartDNS to efficiently handle a large number of regex rules without repeatedly performing expensive regex operations for every rule on every DNS query. The ART tree provides fast lookups for both direct domain matches and the second stage of regex match resolution.

## Differences for `geositelist` Type

The `geositelist` type is a specialized variation of the `geosite` type, designed for simpler list formats that do not require advanced pattern matching capabilities like keywords or regular expressions. Its primary differences lie in how it processes entries from the data file:

*   **Supported Entries**:
    *   Like `geosite`, `geositelist` correctly processes entries with `full:<domain>` for exact domain matching and `domain:<domain>` for suffix domain matching.
    *   Lines that contain only a domain name without a prefix (e.g., `example.com`) are also treated as `domain:<domain>`, meaning they match the domain and its subdomains.

*   **Ignored Entries**:
    *   Crucially, `geositelist` **ignores** lines with `keyword:<text>` and `regexp:<pattern>` prefixes.
    *   If such entries are present in a file loaded by a `domain-set` of type `geositelist`, they will be skipped during the loading phase. No error will be generated, but these rules will not be loaded or used.

*   **No Regex Mechanism**:
    *   As a direct consequence of ignoring `keyword:` and `regexp:` entries, `geositelist` does not utilize SmartDNS's regex engine.
    *   There is no interaction with `dns_regexp_insert` for compiling patterns from a `geositelist` file, and `dns_regexp_match` will not be involved in matching domains against rules from such a set.
    *   This makes `geositelist` a lighter-weight option when only direct and suffix domain matching are needed, potentially offering a slight performance advantage in loading and lookup if regex features are not required for a particular set.

## Usage Example

This section provides a practical example of how to define and use a `domain-set` of type `geosite`.

### 1. Define the `domain-set` in `smartdns.conf`

First, you need to define your `domain-set` in the `smartdns.conf` file. This tells SmartDNS about your set and where to find its data.

```
# smartdns.conf example
domain-set -name my-geosite-rules -type geosite -file /etc/smartdns/my_custom_geosite.txt
```
*(Note: Ensure the path `/etc/smartdns/my_custom_geosite.txt` is accessible by SmartDNS and adjust it according to your system's directory structure.)*

### 2. Create the `geosite` Data File

Next, create the file specified in the `domain-set` directive. In this example, it's `my_custom_geosite.txt`. Populate it with your domain rules.

**File: `/etc/smartdns/my_custom_geosite.txt`**
```
# my_custom_geosite.txt

# Comments are ignored. Lines starting with # are skipped.
# Empty lines are also skipped.

# Match only the exact domain: login.exact.match.com
full:login.exact.match.com

# Match suffix.match.net and all its subdomains (e.g., www.suffix.match.net, api.suffix.match.net)
domain:suffix.match.net

# Match any domain containing the word "analytics" (e.g., web-analytics.com, siteanalytics.org)
keyword:analytics

# Match domains using a regular expression.
# This example matches adserver1.example.org, adserver123.example.org, etc.
regexp:^adserver[0-9]+\.example\.org$

# The following line, if uncommented and without a prefix, would typically be treated
# as 'domain:example.default.org' by the geosite parser.
# However, for clarity in geosite files, explicit prefixes are recommended.
# example.default.org
```

### 3. Use the `domain-set` in `domain-rules`

Finally, reference your `domain-set` in a `domain-rules` directive within `smartdns.conf` to apply actions to matching domains.

```
# smartdns.conf example (continued)

# First, ensure you have a server group defined.
# If 'overseas-dns' is not already defined, you might define it like this:
# server-group overseas-dns
# nameserver 8.8.8.8 # Google DNS
# nameserver 1.1.1.1 # Cloudflare DNS

# Now, use the domain-set in a rule:
domain-rules /domain-set:my-geosite-rules/ -nameserver overseas-dns
```

**Explanation:**

The `domain-rules /domain-set:my-geosite-rules/ -nameserver overseas-dns` line instructs SmartDNS to:
1.  Check the incoming query domain against all the patterns defined in the `my_custom_geosite.txt` file (which is associated with the `my-geosite-rules` set).
2.  If the domain matches any of the rules in the file (e.g., `login.exact.match.com`, `www.suffix.match.net`, `some-analytics-domain.com`, `adserver10.example.org`), SmartDNS will then use the nameservers defined in the `overseas-dns` server group to resolve this query.
3.  Queries for domains not matching any rule in this set will not be affected by this specific `domain-rules` line (though they might be by other rules).

This allows for powerful and organized routing of DNS queries based on flexible, file-defined domain collections.

This documentation provides a comprehensive overview of the `domain-set` configuration in SmartDNS.
