#include "common.h"
#include "decrypt.h"

#include <string.h>
#include <openssl/cms.h>
#include <openssl/asn1t.h>

DECLARE_ASN1_ITEM(CMS_RecipientInfo)

CMS_RecipientInfo *d2i_CMS_RecipientInfo_bio(BIO *bp, CMS_RecipientInfo **cms)
{
    return ASN1_item_d2i_bio(ASN1_ITEM_rptr(CMS_RecipientInfo), bp, cms);
}

typedef struct CMS_EnvelopedData_st CMS_EnvelopedData;
typedef struct CMS_OriginatorInfo_st CMS_OriginatorInfo;
typedef struct CMS_EncryptedContentInfo_st CMS_EncryptedContentInfo;

struct CMS_EncryptedContentInfo_st {
    ASN1_OBJECT *contentType;
    X509_ALGOR *contentEncryptionAlgorithm;
    ASN1_OCTET_STRING *encryptedContent;
    /* Content encryption algorithm and key */
    const EVP_CIPHER *cipher;
    unsigned char *key;
    size_t keylen;
    int debug;
};

struct CMS_EnvelopedData_st {
    long version;
    CMS_OriginatorInfo *originatorInfo;
    STACK_OF(CMS_RecipientInfo) *recipientInfos;
    CMS_EncryptedContentInfo *encryptedContentInfo;
    STACK_OF(X509_ATTRIBUTE) *unprotectedAttrs;
};

struct CMS_ContentInfo_st {
    ASN1_OBJECT *contentType;
    union {
        CMS_EnvelopedData *envelopedData;
    } d;
};

X509_ALGOR *d2i_X509_ALGOR_bio(BIO *bp, X509_ALGOR **x509_a)
{
    return ASN1_item_d2i_bio(ASN1_ITEM_rptr(X509_ALGOR), bp, x509_a);
}

BIO *cms_EncryptedContent_init_bio(CMS_EncryptedContentInfo *ec);

int decrypt_cms(BIO *in, BIO *out, char *password, X509 *x509, EVP_PKEY *key)
{
    int ret = 1;
    CMS_ContentInfo *cms;
    char header[21];
    if(BIO_read(in, header, sizeof(header)) != sizeof(header)) {
        goto end;
    }
    
    unsigned int l = read_length(in);

    cms = CMS_EnvelopedData_create(NULL);
    
    unsigned int i = BIO_number_read(in) + l;
    STACK_OF(CMS_RecipientInfo) *recipientInfos = sk_CMS_RecipientInfo_new_null();
    do {
        CMS_RecipientInfo *ri = d2i_CMS_RecipientInfo_bio(in, NULL);
        if(ri) {
            if(!sk_CMS_RecipientInfo_push(recipientInfos, ri)) {
                goto end;
            }
            if(!sk_CMS_RecipientInfo_push(CMS_get0_RecipientInfos(cms), ri)) {
                goto end;
            }
        } else {
            goto end;
        }
    } while(BIO_number_read(in) < i);
    
    if(key && x509) {
        if (!CMS_decrypt_set1_pkey(cms, key, x509)) {
            goto end;
        }
    }
    
    if (password) {
        unsigned char *tmp = (unsigned char *)BUF_strdup((char *)password);
        if (!CMS_decrypt_set1_password(cms, tmp, -1)) {
            goto end;
        }
    }
    
    CMS_EncryptedContentInfo *ec = cms->d.envelopedData->encryptedContentInfo;
    if(BIO_read(in, header, 13) != 13) {
        goto end;
    }

    X509_ALGOR *calg = d2i_X509_ALGOR_bio(in, NULL);
    ec->contentEncryptionAlgorithm = calg;
    
    BIO *cmsbio = cms_EncryptedContent_init_bio(ec);
    out = BIO_push(cmsbio, out);
        
    if(BIO_read(in, header, 2) != 2) {
        goto end;
    }

    unsigned char c;
    do {
        BIO_read(in, (char *)&c, 1);
        l = read_length(in);
        if(c) {
            char *buffer[4096];
            do {
                unsigned int i = l > sizeof(buffer) ? sizeof(buffer) : l;
                if(BIO_read(in, buffer, i) != i) {
                    goto end;
                }
                BIO_write(out, buffer, i);
                l -= i;
            } while(l > 0);
        }
    } while(c != 0);
    BIO_flush(out);

    ret = 0;
    
 end:
    return ret;
}

int decrypt_cms_legacy(BIO *in, BIO *out, char *password, X509 *x509, EVP_PKEY *key)
{
    int flags = CMS_PARTIAL | CMS_STREAM | CMS_BINARY, ret = 1;
    CMS_ContentInfo *cms;

    cms = d2i_CMS_bio(in, NULL);

    if (password) {
        unsigned char *tmp = (unsigned char *)BUF_strdup((char *)password);
        if (!CMS_decrypt_set1_password(cms, tmp, -1)) {
            goto end;
        }
    } else {
        if(key && x509) {
            if (!CMS_decrypt_set1_pkey(cms, key, x509)) {
                goto end;
            }
        }
    }

    if (!CMS_decrypt(cms, NULL, NULL, NULL, out, flags)) {
        goto end;
    }
    ret = 0;

 end:
    return ret;
}
