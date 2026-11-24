#include "src/crypto.h"

#include <gtest/gtest.h>
#include <openssl/asn1.h>

TEST(CryptoCertificate, EmptySignatureIsSafe) {
  EXPECT_TRUE(crypto::signature(static_cast<const X509 *>(nullptr)).empty());
  crypto::x509_t unsigned_cert {X509_new()};
  ASSERT_TRUE(unsigned_cert);
  EXPECT_TRUE(crypto::signature(unsigned_cert).empty());
}

TEST(CryptoCertificate, GeneratedCredentialsPreservePairingContract) {
  const auto creds = crypto::gen_creds("Vibebeam test", 2048);
  auto cert = crypto::x509(creds.x509);
  auto key = crypto::pkey(creds.pkey);
  ASSERT_TRUE(cert);
  ASSERT_TRUE(key);
  EXPECT_EQ(X509_get_version(cert.get()), 2);
  EXPECT_EQ(X509_check_private_key(cert.get(), key.get()), 1);
  EXPECT_EQ(X509_verify(cert.get(), key.get()), 1);
  EXPECT_EQ(X509_NAME_cmp(X509_get_subject_name(cert.get()), X509_get_issuer_name(cert.get())), 0);
  char cn[64] {};
  ASSERT_EQ(X509_NAME_get_text_by_NID(X509_get_subject_name(cert.get()), NID_commonName, cn, sizeof(cn)), 13);
  EXPECT_STREQ(cn, "Vibebeam test");

  const ASN1_BIT_STRING *asn1 = nullptr;
  X509_get0_signature(&asn1, nullptr, cert.get());
  ASSERT_NE(asn1, nullptr);
  const auto signature = crypto::signature(cert);
  EXPECT_EQ(signature.size(), 256u);
  EXPECT_EQ(signature, std::string_view(reinterpret_cast<const char *>(ASN1_STRING_get0_data(asn1)), ASN1_STRING_length(asn1)));
  // PEM reloads must preserve the exact binary bytes used during pairing.
  auto reloaded = crypto::x509(crypto::pem(cert));
  ASSERT_TRUE(reloaded);
  EXPECT_EQ(crypto::signature(reloaded), signature);

  const std::string challenge {"pairing\0challenge", 17};
  const auto signed_challenge = crypto::sign256(key, challenge);
  ASSERT_EQ(signed_challenge.size(), 256u);
  const std::string_view proof {reinterpret_cast<const char *>(signed_challenge.data()), signed_challenge.size()};
  EXPECT_TRUE(crypto::verify256(cert, challenge, proof));
  EXPECT_FALSE(crypto::verify256(cert, "altered", proof));
}
