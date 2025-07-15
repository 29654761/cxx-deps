#pragma once

#include <openssl/sha.h>
#include <string>
#include <iomanip>
#include <sstream>


namespace sys2 {
    namespace security {

        class sha
        {
        public:
            static int encode_sha256(const char* input, unsigned int inputSize, std::string& out) {

                unsigned char hash[SHA256_DIGEST_LENGTH];

                SHA256_CTX ctx;
                SHA256_Init(&ctx);


                SHA256_Update(&ctx, (unsigned char*)input, inputSize);        // input is OK; &input is WRONG !!!  
                SHA256_Final(hash, &ctx);

                out.assign((const char*)hash, SHA256_DIGEST_LENGTH);
                return 0;
            }


            static void hex_string(const std::string& hex, std::string& out)
            {
                std::stringstream ss;
                for (int i = 0; i < hex.size(); i++) {
                    ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(2) << (int)(unsigned char)hex[i];
                }
                out = ss.str();
            }
        };

    }

}
