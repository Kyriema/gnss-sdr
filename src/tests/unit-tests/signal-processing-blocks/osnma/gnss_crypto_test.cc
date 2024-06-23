#include "gnss_crypto.h"
#include <gtest/gtest.h>
class GnssCryptoTest : public ::testing::Test
{
};


TEST(GnssCryptoTest, VerifySignature)
{
    // "../data/OSNMA_PublicKey_20240115100000_newPKID_1.pem"
    const std::string fake("fake");
    std::unique_ptr<Gnss_Crypto> d_crypto = std::make_unique<Gnss_Crypto>(fake, fake);

    //     RG example  - import crt certificate - result: FAIL
    std::vector<uint8_t> message = {0x82, 0x10, 0x49, 0x22, 0x04, 0xE0, 0x60, 0x61, 0x0B, 0xDF, 0x26, 0xD7, 0x7B, 0x5B, 0xF8, 0xC9, 0xCB, 0xFC, 0xF7, 0x04, 0x22, 0x08, 0x14, 0x75, 0xFD, 0x44, 0x5D, 0xF0, 0xFF};
    std::vector<uint8_t> signature = {0xF8, 0xCD, 0x88, 0x29, 0x9F, 0xA4, 0x60, 0x58, 0x00, 0x20, 0x7B, 0xFE, 0xBE, 0xAC, 0x55, 0x02, 0x40, 0x53, 0xF3, 0x0F, 0x7C, 0x69, 0xB3, 0x5C, 0x15, 0xE6, 0x08, 0x00, 0xAC, 0x3B, 0x6F, 0xE3, 0xED, 0x06, 0x39, 0x95, 0x2F, 0x7B, 0x02, 0x8D, 0x86, 0x86, 0x74, 0x45, 0x96, 0x1F, 0xFE, 0x94, 0xFB, 0x22, 0x6B, 0xFF, 0x70, 0x06, 0xE0, 0xC4, 0x51, 0xEE, 0x3F, 0x87, 0x28, 0xC1, 0x77, 0xFB};
    std::vector<uint8_t> publicKey{// PEM format - 1000 bits
        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A,
        0x4D, 0x46, 0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49,
        0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x63, 0x44, 0x51, 0x67, 0x41, 0x45, 0x41, 0x37, 0x4C, 0x4F, 0x5A, 0x4C, 0x77, 0x67, 0x65, 0x39, 0x32, 0x4C, 0x78, 0x4E, 0x2B, 0x46, 0x6B, 0x59, 0x66, 0x38, 0x74, 0x6F, 0x59, 0x79, 0x44, 0x57, 0x50, 0x2F, 0x0A, 0x6F, 0x4A, 0x46, 0x42, 0x44, 0x38, 0x46, 0x59, 0x2B, 0x37,
        0x64, 0x35, 0x67, 0x4F, 0x71, 0x49, 0x61, 0x45, 0x32, 0x52, 0x6A, 0x50, 0x41, 0x6E, 0x4B, 0x49, 0x36, 0x38, 0x73, 0x2F, 0x4F, 0x4B, 0x2F, 0x48, 0x50, 0x67, 0x6F, 0x4C, 0x6B, 0x4F, 0x32, 0x69, 0x6A, 0x51, 0x38, 0x78, 0x41, 0x5A, 0x79, 0x44, 0x64, 0x50, 0x42, 0x31, 0x64, 0x48, 0x53, 0x51, 0x3D, 0x3D,
        0x0A,
        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A};

    // own ECDSA-P256 key and message generated and signed and verified successfully with openssl
    //    std::vector<uint8_t> message{0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x0A }; // Hello world con 0x0A al final. Raw message (unhashed)
    //    std::vector<uint8_t> signature{0x30, 0x45, 0x02, 0x21, 0x00, 0xFB, 0xE6, 0x09, 0x74, 0x5C, 0x12, 0xE8, 0x2C, 0x0C, 0xC9, 0x7A, 0x8E, 0x13, 0x88, 0x87, 0xDA, 0xBF, 0x08, 0x43, 0xF8, 0xC8, 0x93, 0x16, 0x5A,
    //    0x0F, 0x7A, 0xA4, 0xBF, 0x4A, 0xE1, 0xE1, 0xDB, 0x02, 0x20, 0x6B, 0xCB, 0x2F, 0x80, 0x69, 0xBB, 0xDE, 0xC9, 0x11, 0x1D, 0x51, 0x2B, 0x9F, 0x61, 0xA0, 0xC1, 0x29, 0xD1, 0x0B,
    //    0x58, 0x09, 0x82, 0x58, 0xFC, 0x9E, 0x00, 0xC7, 0xEE, 0xA5, 0xB9, 0xB2, 0x56}; // Hello world hashed and then encrypted with PrK
    // raw r and s values
    //    std::vector<uint8_t> signature = {
    //       0x00, 0xFB, 0xE6, 0x09, 0x74, 0x5C, 0x12, 0xE8, 0x2C, 0x0C, 0xC9, 0x7A, 0x8E, 0x13, 0x88, 0x87, 0xDA, 0xBF, 0x08, 0x43, 0xF8,
    //       0xC8, 0x93, 0x16, 0x5A, 0x0F, 0x7A, 0xA4, 0xBF, 0x4A, 0xE1, 0xE1, 0xDB, 0x02, 0x20, 0x6B, 0xCB, 0x2F, 0x80, 0x69, 0xBB, 0xDE,
    //       0xC9, 0x11, 0x1D, 0x51, 0x2B, 0x9F, 0x61, 0xA0, 0xC1, 0x29, 0xD1, 0x0B, 0x58, 0x09, 0x82, 0x58, 0xFC, 0x9E, 0x00, 0xC7, 0xEE,
    //       0xA5, 0xB9, 0xB2, 0x56 };

    //    std::vector<uint8_t> publicKey{// PK associated to the PrK, in der format ---test
    //       0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x4A, 0xF3,
    //       0xEE, 0x3A, 0x94, 0x25, 0x25, 0x3D, 0x55, 0xC2, 0x5A, 0xC2, 0x2D, 0xCF, 0x14, 0x4D, 0x39, 0x0D, 0xB1, 0xFC, 0x7F, 0x31, 0x5A, 0x2A, 0x19, 0xAE, 0x4E, 0xD6, 0xCB, 0xA6, 0x59,
    //       0xD6, 0x99, 0x7C, 0xE8, 0xBD, 0x1F, 0x43, 0x34, 0x1C, 0x59, 0xD9, 0xD9, 0xCA, 0xC3, 0xEE, 0x58, 0xE5, 0xEA, 0xD3, 0x55, 0x44, 0xEA, 0x89, 0x71, 0x65, 0xD0, 0x92, 0x72, 0xA2,
    //       0xC8, 0x3C, 0x87, 0x5D };
    //    std::vector<uint8_t> publicKey{ // PK associated to the PrK, in pem format
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A,
    //
    //        0x4D, 0x46,
    //        0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x63,
    //        0x44, 0x51, 0x67, 0x41, 0x45, 0x53, 0x76, 0x50, 0x75, 0x4F, 0x70, 0x51, 0x6C, 0x4A, 0x54, 0x31, 0x56, 0x77, 0x6C, 0x72, 0x43, 0x4C, 0x63, 0x38, 0x55, 0x54, 0x54, 0x6B, 0x4E,
    //        0x73, 0x66, 0x78, 0x2F, 0x0A, 0x4D, 0x56, 0x6F, 0x71, 0x47, 0x61, 0x35, 0x4F, 0x31, 0x73, 0x75, 0x6D, 0x57, 0x64, 0x61, 0x5A, 0x66, 0x4F, 0x69, 0x39, 0x48, 0x30, 0x4D, 0x30,
    //        0x48, 0x46, 0x6E, 0x5A, 0x32, 0x63, 0x72, 0x44, 0x37, 0x6C, 0x6A, 0x6C, 0x36, 0x74, 0x4E, 0x56, 0x52, 0x4F, 0x71, 0x4A, 0x63, 0x57, 0x58, 0x51, 0x6B, 0x6E, 0x4B, 0x69, 0x79,
    //        0x44, 0x79, 0x48, 0x58, 0x51, 0x3D, 0x3D, 0x0A,
    //
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D,
    //        0x2D, 0x2D, 0x2D, 0x0A };

    // own key - GnuTLS error: The curve is unsupported... x192 EC unsupported??
    //    std::vector<uint8_t> message = {0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64 }; // hello world
    //    std::vector<uint8_t> signature = {0x30, 0x34, 0x02, 0x18, 0x4F, 0xAC, 0x9C, 0x5A, 0x44, 0xCF, 0xFD, 0x42, 0x6A, 0x58, 0x97, 0xA4, 0x94, 0x53, 0x2C, 0x79, 0xD1, 0x7B, 0x8B, 0xF9, 0x93, 0x03, 0xA2, 0xAF, 0x02, 0x18, 0x46, 0xF2, 0xF3, 0xCF, 0x9A, 0x23, 0x39, 0xB4, 0x25, 0x11, 0x89, 0x9A, 0x44, 0x7E, 0x2F, 0xB1, 0xE1, 0x58, 0xAF, 0xCE, 0xC1,0xB4, 0xA1, 0x38 };
    //    std::vector<uint8_t> publicKey = {
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x4D, 0x45, 0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49,
    //        0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x49, 0x44, 0x4D, 0x67, 0x41, 0x45, 0x51, 0x55, 0x61, 0x30, 0x6C, 0x38, 0x4D, 0x35, 0x76, 0x50, 0x58, 0x2B, 0x74, 0x4A, 0x76, 0x63, 0x4C, 0x2B, 0x45, 0x45, 0x4C, 0x34, 0x6E, 0x71, 0x79, 0x75, 0x53, 0x43, 0x0A, 0x4D, 0x4E, 0x46, 0x4A, 0x64, 0x43, 0x5A, 0x62, 0x62, 0x58,
    //        0x35, 0x70, 0x4D, 0x36, 0x69, 0x4C, 0x52, 0x53, 0x30, 0x43, 0x51, 0x59, 0x45, 0x67, 0x56, 0x47, 0x51, 0x6B, 0x65, 0x75, 0x74, 0x74, 0x35, 0x78, 0x2F, 0x45, 0x0A, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    //        0x0A };
    //    std::vector<uint8_t> ecparam = {
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x45, 0x43, 0x20, 0x50, 0x41, 0x52, 0x41, 0x4D, 0x45, 0x54, 0x45, 0x52, 0x53, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x42, 0x67, 0x67, 0x71, 0x68, 0x6B, 0x6A, 0x4F, 0x50, 0x51, 0x4D, 0x42, 0x41, 0x67, 0x3D, 0x3D, 0x0A, 0x2D, 0x2D, 0x2D, 0x2D,
    //        0x2D, 0x45, 0x4E, 0x44, 0x20, 0x45, 0x43, 0x20, 0x50, 0x41, 0x52, 0x41, 0x4D, 0x45, 0x54, 0x45, 0x52, 0x53, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A };

    d_crypto->set_public_key(publicKey);
    bool result = d_crypto->verify_signature(message, signature);

    ASSERT_TRUE(result);
}
// TEST(GnssCryptoTest, sha256Test)
//{
//     std::unique_ptr<Gnss_Crypto> d_crypto;
//
//     auto str = "Hello World!";
//     std::vector<uint8_t> input (str, str + strlen(str));
//
//     auto expectedOutputStr = "86933b0b147ac4c010266b99004158fa17937db89a03dd7bb2ca5ef7f43c325a";
//     std::vector<uint8_t> expectedOutput(expectedOutputStr, expectedOutputStr + strlen(expectedOutputStr));
//
//     std::vector<uint8_t> computedOutput = d_crypto->computeSHA256(input);
//
//     ASSERT_TRUE(computedOutput == expectedOutput


TEST(GnssCryptoTest, VerifyPubKeyImport)
{
    // "../data/OSNMA_PublicKey_20240115100000_newPKID_1.pem"
    const std::string fake("fake");
    std::unique_ptr<Gnss_Crypto> d_crypto = std::make_unique<Gnss_Crypto>(fake, fake);

    // RG example - key is raw 520 bits example shown
    //    std::vector<uint8_t> publicKey = { // base64 decoding error
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A,
    //
    //        0x04, 0x03, 0xB2, 0xCE, 0x64, 0xBC, 0x20, 0x7B, 0xDD, 0x8B, 0xC4, 0xDF, 0x85, 0x91, 0x87, 0xFC,
    //        0xB6, 0x86, 0x32, 0x0D, 0x63, 0xFF, 0xA0, 0x91, 0x41, 0x0F, 0xC1, 0x58, 0xFB, 0xB7, 0x79, 0x80,
    //        0xEA, 0x88, 0x68, 0x4D, 0x91, 0x8C, 0xF0, 0x27, 0x28, 0x8E, 0xBC, 0xB3, 0xF3, 0x8A, 0xFC, 0x73,
    //        0xE0, 0xA0, 0xB9, 0x0E, 0xDA, 0x28, 0xD0, 0xF3, 0x10, 0x19, 0xC8, 0x37, 0x4F, 0x07, 0x57, 0x47, 0x49,
    //
    //        0x0A, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A
    //
    //    };

    // RG example  crt exported and convert PK.pem - key is raw 1000 bits ,..., why mismatch!? does key get truncated?
    //        std::vector<uint8_t> publicKey {
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x4D, 0x46, 0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49,
    //        0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x63, 0x44, 0x51, 0x67, 0x41, 0x45, 0x41, 0x37, 0x4C, 0x4F, 0x5A, 0x4C, 0x77, 0x67, 0x65, 0x39, 0x32, 0x4C, 0x78, 0x4E, 0x2B, 0x46, 0x6B, 0x59, 0x66, 0x38, 0x74, 0x6F, 0x59, 0x79, 0x44, 0x57, 0x50, 0x2F, 0x0A, 0x6F, 0x4A, 0x46, 0x42, 0x44, 0x38, 0x46, 0x59, 0x2B, 0x37,
    //        0x64, 0x35, 0x67, 0x4F, 0x71, 0x49, 0x61, 0x45, 0x32, 0x52, 0x6A, 0x50, 0x41, 0x6E, 0x4B, 0x49, 0x36, 0x38, 0x73, 0x2F, 0x4F, 0x4B, 0x2F, 0x48, 0x50, 0x67, 0x6F, 0x4C, 0x6B, 0x4F, 0x32, 0x69, 0x6A, 0x51, 0x38, 0x78, 0x41, 0x5A, 0x79, 0x44, 0x64, 0x50, 0x42, 0x31, 0x64, 0x48, 0x53, 0x51, 0x3D, 0x3D, 0x0A,
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A } ;
    // own ECDSA P 256 public key and own message generated (2024-02-19-Own-Key-ECDSA-openssl)
    std::vector<uint8_t> publicKey{// PEM
        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A,
        0x4D, 0x46,
        0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x63,
        0x44, 0x51, 0x67, 0x41, 0x45, 0x53, 0x76, 0x50, 0x75, 0x4F, 0x70, 0x51, 0x6C, 0x4A, 0x54, 0x31, 0x56, 0x77, 0x6C, 0x72, 0x43, 0x4C, 0x63, 0x38, 0x55, 0x54, 0x54, 0x6B, 0x4E,
        0x73, 0x66, 0x78, 0x2F, 0x0A, 0x4D, 0x56, 0x6F, 0x71, 0x47, 0x61, 0x35, 0x4F, 0x31, 0x73, 0x75, 0x6D, 0x57, 0x64, 0x61, 0x5A, 0x66, 0x4F, 0x69, 0x39, 0x48, 0x30, 0x4D, 0x30,
        0x48, 0x46, 0x6E, 0x5A, 0x32, 0x63, 0x72, 0x44, 0x37, 0x6C, 0x6A, 0x6C, 0x36, 0x74, 0x4E, 0x56, 0x52, 0x4F, 0x71, 0x4A, 0x63, 0x57, 0x58, 0x51, 0x6B, 0x6E, 0x4B, 0x69, 0x79,
        0x44, 0x79, 0x48, 0x58, 0x51, 0x3D, 0x3D, 0x0A,
        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D,
        0x2D, 0x2D, 0x2D, 0x0A};

    d_crypto->set_public_key(publicKey);

    ASSERT_TRUE(d_crypto->have_public_key());

    //    std::vector<uint8_t> publicKey = { // DER format
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x4D, 0x46, 0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49,
    //        0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x63, 0x44, 0x51, 0x67, 0x41, 0x45, 0x41, 0x37, 0x4C, 0x4F, 0x5A, 0x4C, 0x77, 0x67, 0x65, 0x39, 0x32, 0x4C, 0x78, 0x4E, 0x2B, 0x46, 0x6B, 0x59, 0x66, 0x38, 0x74, 0x6F, 0x59, 0x79, 0x44, 0x57, 0x50, 0x2F, 0x0A, 0x6F, 0x4A, 0x46, 0x42, 0x44, 0x38, 0x46, 0x59, 0x2B, 0x37,
    //        0x64, 0x35, 0x67, 0x4F, 0x71, 0x49, 0x61, 0x45, 0x32, 0x52, 0x6A, 0x50, 0x41, 0x6E, 0x4B, 0x49, 0x36, 0x38, 0x73, 0x2F, 0x4F, 0x4B, 0x2F, 0x48, 0x50, 0x67, 0x6F, 0x4C, 0x6B, 0x4F, 0x32, 0x69, 0x6A, 0x51, 0x38, 0x78, 0x41, 0x5A, 0x79, 0x44, 0x64, 0x50, 0x42, 0x31, 0x64, 0x48, 0x53, 0x51, 0x3D, 0x3D, 0x0A,
    //        0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x30, 0x82, 0x02, 0x6C, 0x30, 0x82, 0x02, 0x12, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x10, 0x47, 0xC4, 0xF1, 0x43, 0xC3, 0xFA, 0x61, 0xA5, 0x29, 0x4E, 0x63,
    //        0xD5, 0x57, 0x2B, 0x01, 0x62, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30, 0x37, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x45, 0x53, 0x31, 0x0E, 0x30, 0x0C, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x05, 0x45, 0x55, 0x53, 0x50, 0x41, 0x31, 0x18, 0x30,
    //        0x16, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0F, 0x45, 0x55, 0x53, 0x50, 0x41, 0x20, 0x4F, 0x53, 0x4E, 0x4D, 0x41, 0x20, 0x49, 0x43, 0x41, 0x30, 0x1E, 0x17, 0x0D, 0x32, 0x33, 0x30, 0x37, 0x32, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x30, 0x5A, 0x17, 0x0D, 0x32, 0x35, 0x30, 0x38, 0x30, 0x38, 0x31, 0x31, 0x33,
    //        0x33, 0x30, 0x30, 0x5A, 0x30, 0x3A, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x45, 0x53, 0x31, 0x0E, 0x30, 0x0C, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x05, 0x45, 0x55, 0x53, 0x50, 0x41, 0x31, 0x1B, 0x30, 0x19, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x12, 0x45, 0x55, 0x53, 0x50, 0x41,
    //        0x20, 0x4F, 0x53, 0x4E, 0x4D, 0x41, 0x20, 0x45, 0x45, 0x20, 0x50, 0x4B, 0x52, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x03, 0xB2, 0xCE, 0x64, 0xBC, 0x20, 0x7B, 0xDD, 0x8B, 0xC4, 0xDF,
    //        0x85, 0x91, 0x87, 0xFC, 0xB6, 0x86, 0x32, 0x0D, 0x63, 0xFF, 0xA0, 0x91, 0x41, 0x0F, 0xC1, 0x58, 0xFB, 0xB7, 0x79, 0x80, 0xEA, 0x88, 0x68, 0x4D, 0x91, 0x8C, 0xF0, 0x27, 0x28, 0x8E, 0xBC, 0xB3, 0xF3, 0x8A, 0xFC, 0x73, 0xE0, 0xA0, 0xB9, 0x0E, 0xDA, 0x28, 0xD0, 0xF3, 0x10, 0x19, 0xC8, 0x37, 0x4F, 0x07, 0x57,
    //        0x47, 0x49, 0xA3, 0x81, 0xFC, 0x30, 0x81, 0xF9, 0x30, 0x1D, 0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04, 0x14, 0x6A, 0x22, 0x16, 0x58, 0x9B, 0x23, 0xC9, 0x43, 0x41, 0x3C, 0xB6, 0xF8, 0x9D, 0x93, 0x0F, 0xE0, 0xFE, 0x6A, 0x3C, 0x54, 0x30, 0x1F, 0x06, 0x03, 0x55, 0x1D, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80,
    //        0x14, 0x20, 0xC0, 0x54, 0x85, 0xAF, 0x82, 0xAE, 0x96, 0x3C, 0xBC, 0xDF, 0xC1, 0xB9, 0x05, 0xDE, 0xD7, 0x46, 0x72, 0x32, 0xA3, 0x30, 0x63, 0x06, 0x03, 0x55, 0x1D, 0x20, 0x04, 0x5C, 0x30, 0x5A, 0x30, 0x4E, 0x06, 0x0B, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0xD5, 0x11, 0x01, 0x01, 0x01, 0x30, 0x3F, 0x30, 0x3D,
    //        0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x01, 0x16, 0x31, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F, 0x77, 0x77, 0x77, 0x2E, 0x67, 0x73, 0x63, 0x2D, 0x65, 0x75, 0x72, 0x6F, 0x70, 0x61, 0x2E, 0x65, 0x75, 0x2F, 0x67, 0x73, 0x63, 0x2D, 0x70, 0x72, 0x6F, 0x64, 0x75, 0x63, 0x74, 0x73, 0x2F,
    //        0x4F, 0x53, 0x4E, 0x4D, 0x41, 0x2F, 0x50, 0x4B, 0x49, 0x2F, 0x30, 0x08, 0x06, 0x06, 0x04, 0x00, 0x8F, 0x7A, 0x01, 0x02, 0x30, 0x42, 0x06, 0x03, 0x55, 0x1D, 0x1F, 0x04, 0x3B, 0x30, 0x39, 0x30, 0x37, 0xA0, 0x35, 0xA0, 0x33, 0x86, 0x31, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F, 0x77, 0x77, 0x77, 0x2E,
    //        0x67, 0x73, 0x63, 0x2D, 0x65, 0x75, 0x72, 0x6F, 0x70, 0x61, 0x2E, 0x65, 0x75, 0x2F, 0x67, 0x73, 0x63, 0x2D, 0x70, 0x72, 0x6F, 0x64, 0x75, 0x63, 0x74, 0x73, 0x2F, 0x4F, 0x53, 0x4E, 0x4D, 0x41, 0x2F, 0x50, 0x4B, 0x49, 0x2F, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03, 0x02,
    //        0x07, 0x80, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0xE9, 0xBB, 0x90, 0x8E, 0xE5, 0x0C, 0xF3, 0xDA, 0x57, 0x71, 0xE3, 0xD0, 0xD2, 0xEA, 0xAC, 0x1B, 0x00, 0xF3, 0x51, 0xE9, 0xD8, 0xBB, 0x0A, 0xB2, 0x4C, 0x8A, 0x65, 0x52, 0x79,
    //        0x9F, 0x43, 0xF6, 0x02, 0x20, 0x10, 0x65, 0x2F, 0x6A, 0xF8, 0x26, 0x20, 0x42, 0xFF, 0x09, 0x6B, 0xD0, 0x8D, 0x0B, 0x75, 0x15, 0x24, 0xBF, 0xE4, 0xFE, 0x60, 0xC3, 0x6E, 0x2D, 0x31, 0x32, 0xED, 0x65, 0x6C, 0x5C, 0x8B, 0x14 };

    //            std::vector<uint8_t> publicKey= { // PEM format
    //                0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x4D, 0x46, 0x6B, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4B, 0x6F, 0x5A, 0x49, 0x7A, 0x6A, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4B, 0x6F, 0x5A, 0x49,
    //                0x7A, 0x6A, 0x30, 0x44, 0x41, 0x51, 0x63, 0x44, 0x51, 0x67, 0x41, 0x45, 0x41, 0x37, 0x4C, 0x4F, 0x5A, 0x4C, 0x77, 0x67, 0x65, 0x39, 0x32, 0x4C, 0x78, 0x4E, 0x2B, 0x46, 0x6B, 0x59, 0x66, 0x38, 0x74, 0x6F, 0x59, 0x79, 0x44, 0x57, 0x50, 0x2F, 0x0A, 0x6F, 0x4A, 0x46, 0x42, 0x44, 0x38, 0x46, 0x59, 0x2B, 0x37,
    //                0x64, 0x35, 0x67, 0x4F, 0x71, 0x49, 0x61, 0x45, 0x32, 0x52, 0x6A, 0x50, 0x41, 0x6E, 0x4B, 0x49, 0x36, 0x38, 0x73, 0x2F, 0x4F, 0x4B, 0x2F, 0x48, 0x50, 0x67, 0x6F, 0x4C, 0x6B, 0x4F, 0x32, 0x69, 0x6A, 0x51, 0x38, 0x78, 0x41, 0x5A, 0x79, 0x44, 0x64, 0x50, 0x42, 0x31, 0x64, 0x48, 0x53, 0x51, 0x3D, 0x3D, 0x0A,
    //                0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4C, 0x49, 0x43, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A };
}


// Unit test for computeHMAC_SHA_256 function.
TEST(GnssCryptoTest, TestComputeHMACSHA256)
{
    // key and message generated with openssl
    const std::string fake("fake");
    std::unique_ptr<Gnss_Crypto> d_crypto = std::make_unique<Gnss_Crypto>(fake, fake);
    std::vector<uint8_t> key = {
        0x24, 0x24, 0x3B, 0x76, 0xF9, 0x14, 0xB1, 0xA7,
        0x7D, 0x48, 0xE7, 0xF1, 0x48, 0x0C, 0xC2, 0x98,
        0xEB, 0x62, 0x3E, 0x95, 0x6B, 0x2B, 0xCE, 0xA3,
        0xB4, 0xD4, 0xDB, 0x31, 0xEE, 0x96, 0xAB, 0xFA};

    std::vector<uint8_t> message{0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x0A};  // Hello world con 0x0A
    std::vector<uint8_t> expected_output = {
        0xC3, 0x51, 0xF6, 0xFD, 0xDD, 0xC9, 0x8B, 0x41,
        0xD6, 0xF4, 0x77, 0x6D, 0xAC, 0xE8, 0xE0, 0x14,
        0xB2, 0x7A, 0xCC, 0x22, 0x00, 0xAA, 0xD2, 0x37,
        0xD0, 0x79, 0x06, 0x12, 0x83, 0x40, 0xB7, 0xA6};

    std::vector<uint8_t> output = d_crypto->computeHMAC_SHA_256(key, message);

    ASSERT_EQ(expected_output, output);
}


TEST(GnssCryptoTest, TestComputeHMACSHA256_m0)
{  // key and message generated from RG A.6.5.1
    const std::string fake("fake");
    std::unique_ptr<Gnss_Crypto> d_crypto = std::make_unique<Gnss_Crypto>(fake, fake);
    std::vector<uint8_t> key = {// RG K4 @ 345690
        0x69, 0xC0, 0x0A, 0xA7, 0x36, 0x42, 0x37, 0xA6,
        0x5E, 0xBF, 0x00, 0x6A, 0xD8, 0xDD, 0xBC, 0x73};

    std::vector<uint8_t> message{// m0
        0x02, 0x4E, 0x05, 0x46, 0x3C, 0x01, 0x83, 0xA5, 0x91, 0x05, 0x1D, 0x69, 0x25, 0x80, 0x07, 0x6B,
        0x3E, 0xEA, 0x81, 0x41, 0xBF, 0x03, 0xAD, 0xCB, 0x5A, 0xAD, 0xB2, 0x77, 0xAF, 0x6F, 0xCF, 0x21,
        0xFB, 0x98, 0xFF, 0x7E, 0x83, 0xAF, 0xFC, 0x37, 0x02, 0x03, 0xB0, 0xD8, 0xE1, 0x0E, 0xB1, 0x4D,
        0x11, 0x18, 0xE6, 0xB0, 0xE8, 0x20, 0x01, 0xA0, 0x00, 0xE5, 0x91, 0x00, 0x06, 0xD3, 0x1F, 0x00,
        0x02, 0x68, 0x05, 0x4A, 0x02, 0xC2, 0x26, 0x07, 0xF7, 0xFC, 0x00};

    std::vector<uint8_t> expected_output = {
        0xE3, 0x7B, 0xC4, 0xF8, 0x58, 0xAE, 0x1E, 0x5C,
        0xFD, 0xC4, 0x6F, 0x05, 0x4B, 0x1F, 0x47, 0xB9, 0xD2, 0xEA, 0x61, 0xE1,
        0xEF, 0x09, 0x11, 0x5C, 0xFE, 0x70, 0x68, 0x52, 0xBF, 0xF2, 0x3A, 0x83};

    std::vector<uint8_t> output = d_crypto->computeHMAC_SHA_256(key, message);

    ASSERT_EQ(expected_output, output);
}


TEST(GnssCryptoTest, TestComputeHMACSHA256_adkd4)
{  // key and message generated from RG A.6.5.2
    const std::string fake("fake");
    std::unique_ptr<Gnss_Crypto> d_crypto = std::make_unique<Gnss_Crypto>(fake, fake);
    std::vector<uint8_t> key = {// RG K4 @ 345690
        0x69, 0xC0, 0x0A, 0xA7, 0x36, 0x42, 0x37, 0xA6,
        0x5E, 0xBF, 0x00, 0x6A, 0xD8, 0xDD, 0xBC, 0x73};

    std::vector<uint8_t> message{
        0x02, 0x02, 0x4E, 0x05, 0x46, 0x3C, 0x03, 0xBF,
        0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x44, 0x92, 0x38,
        0x22, 0x78, 0x97, 0xFD, 0xEF, 0xF9, 0x30, 0x40};

    std::vector<uint8_t> expected_output = {
        0x7B, 0xB2, 0x38, 0xC8, 0x83, 0xC0, 0x6A, 0x2B, 0x50, 0x8F,
        0xE6, 0x3F, 0xB7, 0xF4, 0xF5, 0x4D, 0x44, 0xAB, 0xEE, 0x4D,
        0xCE, 0xB9, 0x3D, 0xCF, 0x65, 0xCB, 0x3A, 0x5B, 0x81, 0x4A, 0x34, 0xE9};

    std::vector<uint8_t> output = d_crypto->computeHMAC_SHA_256(key, message);

    ASSERT_EQ(expected_output, output);
}

// TODO extend to HMAC-AES