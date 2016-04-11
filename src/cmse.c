/*
 * Copyright (C) 2016 Mathias Brossard <mathias@brossard.org>
 */

#include "common.h"
#include "decrypt.h"
#include "encrypt.h"

static char *app_name = "cmse";

static const struct option options[] = {
    { "help",               0, 0,           'h' },
    { "decrypt",            1, 0,           'd' },
    { "encrypt",            1, 0,           'e' },
    { "output",             1, 0,           'o' },
    { "password",           1, 0,           'p' },
    { "verbose",            0, 0,           'v' },
    { 0, 0, 0, 0 }
};

static const char *option_help[] = {
    "Print this help and exit",
    "Decrypt file",
    "Encrypt file",
    "Output file",
    "Password used to encrypt",
    "Display additional information",
};

unsigned int read_length(BIO *in)
{
    unsigned int l = 0, i, j;
    unsigned char c;

    BIO_read(in, (char *)&c, 1);
    if(c <= 127) {
        l = c;
    } else {
        j = c - 128;
        /* Ugly kludge to 32 bits... */
        for(i = 0; i < j; i++) {
            BIO_read(in, (char *)&c, 1);
            l = l << 8;
            l |= c;
        }
    }

    return l;
}

int main(int argc, char **argv)
{
    char *opt_input = NULL,
        *opt_output = NULL,
        *opt_password = NULL;
    int long_optind = 0, ret = 1;
    int encrypt = 0, decrypt = 0, verbose = 0;

    init_crypto();

    while (1) {
        char c = getopt_long(argc, argv, "d:e:ho:p:v",
                             options, &long_optind);
        if (c == -1)
            break;
        switch (c) {
            case 'd':
                decrypt = 1;
                opt_input = optarg;
                break;
            case 'e':
                encrypt = 1;
                opt_input = optarg;
                break;
            case 'o':
                opt_output = optarg;
                break;
            case 'p':
                opt_password = optarg;
                break;
            case 'v':
                verbose += 1;
                break;
            case 'h':
            default:
                print_usage_and_die(app_name, options, option_help);
        }
    }

    BIO *in = NULL, *out = NULL;

    in = BIO_new_file(opt_input, "rb");
    // in = BIO_new_fp(stdin, BIO_NOCLOSE);

    if(opt_output) {
        out = BIO_new_file(opt_output, "wb");
    } else {
        out = BIO_new_fp(stdout, BIO_NOCLOSE);
    }

    if(encrypt) {
        ret = encrypt_cms(in, out, opt_password);
    } else if(decrypt) {
        ret = decrypt_cms(in, out, opt_password);
    }

    return ret;
}
