// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// Name-based version 5 UUIDs against RFC 9562 Appendix A.4's example and Python's uuid.uuid5, and the text form.

#include "padl/uuid/uuid_value.hpp"

#include "statusbar/test/test.hpp"

#include <cstdint>
#include <string_view>

using padl::uuid::name_based_v5;
using padl::uuid::NAMESPACE_DNS;
using padl::uuid::NAMESPACE_URL;
using padl::uuid::Uuid;
using padl::uuid::VARIANT_OCTET;

// RFC 9562 Appendix A.4: the DNS namespace and "www.example.com".
TEST(uuid_value, the_rfc_9562_version_5_example_derives_as_published)
{
    auto const uuid = name_based_v5(NAMESPACE_DNS, "www.example.com");
    EXPECT_EQ(uuid.text().view(), std::string_view{"2ed6657d-e927-568b-95e1-2665a8aea6a2"});
    EXPECT_EQ(uuid.version(), uint8_t{5});
    EXPECT_EQ(uuid.octets[VARIANT_OCTET] & 0xC0U, 0x80U);
}

// uuid.uuid5() of Python 3, which implements RFC 9562 Section 5.5: another namespace, an empty name, UTF-8.
TEST(uuid_value, names_derive_as_python_s_uuid5)
{
    EXPECT_EQ(
        name_based_v5(NAMESPACE_URL, "https://www.example.com/").text().view(),
        std::string_view{"3d3ed9d2-aa3d-5fa6-90e8-ed662e90f559"});
    EXPECT_EQ(name_based_v5(NAMESPACE_DNS, "").text().view(), std::string_view{"4ebd0208-8328-5d69-8c44-ec50939c0967"});
    EXPECT_EQ(name_based_v5(NAMESPACE_DNS, "\xC3\xA9").text().view(), std::string_view{"ebfe0af8-3997-5ade-b634-ba92cf69f557"});
}

TEST(uuid_value, the_same_name_gives_the_same_uuid_and_another_another)
{
    EXPECT_EQ(name_based_v5(NAMESPACE_DNS, "a"), name_based_v5(NAMESPACE_DNS, "a"));
    EXPECT_NE(name_based_v5(NAMESPACE_DNS, "a"), name_based_v5(NAMESPACE_DNS, "b"));
    EXPECT_NE(name_based_v5(NAMESPACE_DNS, "a"), name_based_v5(NAMESPACE_URL, "a"));
}

TEST(uuid_value, the_text_form_is_lower_case_8_4_4_4_12)
{
    EXPECT_EQ(NAMESPACE_DNS.text().view(), std::string_view{"6ba7b810-9dad-11d1-80b4-00c04fd430c8"});
    EXPECT_EQ(Uuid{}.text().view(), std::string_view{"00000000-0000-0000-0000-000000000000"});
    EXPECT_EQ(NAMESPACE_DNS.version(), uint8_t{1});
}

TEST_MAIN(padl_uuid, uuid_value_test)
