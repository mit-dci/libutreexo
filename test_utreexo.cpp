#include <crypto/sha256.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    std::string sha2_impl = SHA256AutoDetect();
    printf("sha2 implementation: %s\n", sha2_impl.c_str());
}
