/* md5.c
 *
 * Copyright (C) 2006-2017 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */



#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#if !defined(NO_MD5)

#if defined(WOLFSSL_TI_HASH)
    /* #include <wolfcrypt/src/port/ti/ti-hash.c> included by wc_port.c */

#else

#include <wolfssl/wolfcrypt/md5.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/logging.h>

#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif
#ifdef INLINE
#undef INLINE
#endif // DEBUG
#define INLINE static __inline


/* Hardware Acceleration */
#if defined(STM32_HASH)

    /* Supports CubeMX HAL or Standard Peripheral Library */
	#define HAVE_MD5_CUST_API

    int wc_InitMd5_ex(wc_Md5* md5, void* heap, int devId)
    {
        if (md5 == NULL) {
            return BAD_FUNC_ARG;
        }

        (void)devId;
        (void)heap;

        wc_Stm32_Hash_Init(&md5->stmCtx);

        return 0;
    }

    int wc_Md5Update(wc_Md5* md5, const byte* data, word32 len)
    {
        int ret;

        if (md5 == NULL || (data == NULL && len > 0)) {
            return BAD_FUNC_ARG;
        }

        ret = wolfSSL_CryptHwMutexLock();
        if (ret == 0) {
            ret = wc_Stm32_Hash_Update(&md5->stmCtx, HASH_AlgoSelection_MD5,
                data, len);
            wolfSSL_CryptHwMutexUnLock();
        }
        return ret;
    }

    int wc_Md5Final(wc_Md5* md5, byte* hash)
    {
        int ret;

        if (md5 == NULL || hash == NULL) {
            return BAD_FUNC_ARG;
        }

        ret = wolfSSL_CryptHwMutexLock();
        if (ret == 0) {
            ret = wc_Stm32_Hash_Final(&md5->stmCtx, HASH_AlgoSelection_MD5,
                hash, WC_MD5_DIGEST_SIZE);
            wolfSSL_CryptHwMutexUnLock();
        }

        (void)wc_InitMd5(md5);  /* reset state */

        return ret;
    }

#elif defined(FREESCALE_MMCAU_SHA)
    #include "cau_api.h"
    #define XTRANSFORM(S,B)  Transform((S), (B))

    static int Transform(wc_Md5* md5, byte* data)
    {
        int ret = wolfSSL_CryptHwMutexLock();
        if(ret == 0) {
        #ifdef FREESCALE_MMCAU_CLASSIC_SHA
            cau_md5_hash_n(data, 1, (unsigned char*)md5->digest);
        #else
            MMCAU_MD5_HashN(data, 1, (uint32_t*)md5->digest);
        #endif
            wolfSSL_CryptHwMutexUnLock();
        }
        return ret;
    }

#elif defined(WOLFSSL_PIC32MZ_HASH)
    #include <wolfssl/wolfcrypt/port/pic32/pic32mz-crypt.h>
    #define HAVE_MD5_CUST_API

#elif defined(WOLFSSL_IMX6_CAAM) && !defined(NO_IMX6_CAAM_HASH)
    /* functions implemented in wolfcrypt/src/port/caam/caam_sha.c */
    #define HAVE_MD5_CUST_API
#else
    #define NEED_SOFT_MD5

#endif /* End Hardware Acceleration */


#ifdef NEED_SOFT_MD5

    #define XTRANSFORM(S,B)  Transform((S))

    #define F1(x, y, z) (z ^ (x & (y ^ z)))
    #define F2(x, y, z) F1(z, x, y)
    #define F3(x, y, z) (x ^ y ^ z)
    #define F4(x, y, z) (y ^ (x | ~z))

    #define MD5STEP(f, w, x, y, z, data, s) \
        w = rotlFixed(w + f(x, y, z) + data, s) + x

    static int Transform(wc_Md5* md5)
    {
        /* Copy context->state[] to working vars  */
        word32 a = md5->digest[0];
        word32 b = md5->digest[1];
        word32 c = md5->digest[2];
        word32 d = md5->digest[3];

        MD5STEP(F1, a, b, c, d, md5->buffer[0]  + 0xd76aa478,  7);
        MD5STEP(F1, d, a, b, c, md5->buffer[1]  + 0xe8c7b756, 12);
        MD5STEP(F1, c, d, a, b, md5->buffer[2]  + 0x242070db, 17);
        MD5STEP(F1, b, c, d, a, md5->buffer[3]  + 0xc1bdceee, 22);
        MD5STEP(F1, a, b, c, d, md5->buffer[4]  + 0xf57c0faf,  7);
        MD5STEP(F1, d, a, b, c, md5->buffer[5]  + 0x4787c62a, 12);
        MD5STEP(F1, c, d, a, b, md5->buffer[6]  + 0xa8304613, 17);
        MD5STEP(F1, b, c, d, a, md5->buffer[7]  + 0xfd469501, 22);
        MD5STEP(F1, a, b, c, d, md5->buffer[8]  + 0x698098d8,  7);
        MD5STEP(F1, d, a, b, c, md5->buffer[9]  + 0x8b44f7af, 12);
        MD5STEP(F1, c, d, a, b, md5->buffer[10] + 0xffff5bb1, 17);
        MD5STEP(F1, b, c, d, a, md5->buffer[11] + 0x895cd7be, 22);
        MD5STEP(F1, a, b, c, d, md5->buffer[12] + 0x6b901122,  7);
        MD5STEP(F1, d, a, b, c, md5->buffer[13] + 0xfd987193, 12);
        MD5STEP(F1, c, d, a, b, md5->buffer[14] + 0xa679438e, 17);
        MD5STEP(F1, b, c, d, a, md5->buffer[15] + 0x49b40821, 22);

        MD5STEP(F2, a, b, c, d, md5->buffer[1]  + 0xf61e2562,  5);
        MD5STEP(F2, d, a, b, c, md5->buffer[6]  + 0xc040b340,  9);
        MD5STEP(F2, c, d, a, b, md5->buffer[11] + 0x265e5a51, 14);
        MD5STEP(F2, b, c, d, a, md5->buffer[0]  + 0xe9b6c7aa, 20);
        MD5STEP(F2, a, b, c, d, md5->buffer[5]  + 0xd62f105d,  5);
        MD5STEP(F2, d, a, b, c, md5->buffer[10] + 0x02441453,  9);
        MD5STEP(F2, c, d, a, b, md5->buffer[15] + 0xd8a1e681, 14);
        MD5STEP(F2, b, c, d, a, md5->buffer[4]  + 0xe7d3fbc8, 20);
        MD5STEP(F2, a, b, c, d, md5->buffer[9]  + 0x21e1cde6,  5);
        MD5STEP(F2, d, a, b, c, md5->buffer[14] + 0xc33707d6,  9);
        MD5STEP(F2, c, d, a, b, md5->buffer[3]  + 0xf4d50d87, 14);
        MD5STEP(F2, b, c, d, a, md5->buffer[8]  + 0x455a14ed, 20);
        MD5STEP(F2, a, b, c, d, md5->buffer[13] + 0xa9e3e905,  5);
        MD5STEP(F2, d, a, b, c, md5->buffer[2]  + 0xfcefa3f8,  9);
        MD5STEP(F2, c, d, a, b, md5->buffer[7]  + 0x676f02d9, 14);
        MD5STEP(F2, b, c, d, a, md5->buffer[12] + 0x8d2a4c8a, 20);

        MD5STEP(F3, a, b, c, d, md5->buffer[5]  + 0xfffa3942,  4);
        MD5STEP(F3, d, a, b, c, md5->buffer[8]  + 0x8771f681, 11);
        MD5STEP(F3, c, d, a, b, md5->buffer[11] + 0x6d9d6122, 16);
        MD5STEP(F3, b, c, d, a, md5->buffer[14] + 0xfde5380c, 23);
        MD5STEP(F3, a, b, c, d, md5->buffer[1]  + 0xa4beea44,  4);
        MD5STEP(F3, d, a, b, c, md5->buffer[4]  + 0x4bdecfa9, 11);
        MD5STEP(F3, c, d, a, b, md5->buffer[7]  + 0xf6bb4b60, 16);
        MD5STEP(F3, b, c, d, a, md5->buffer[10] + 0xbebfbc70, 23);
        MD5STEP(F3, a, b, c, d, md5->buffer[13] + 0x289b7ec6,  4);
        MD5STEP(F3, d, a, b, c, md5->buffer[0]  + 0xeaa127fa, 11);
        MD5STEP(F3, c, d, a, b, md5->buffer[3]  + 0xd4ef3085, 16);
        MD5STEP(F3, b, c, d, a, md5->buffer[6]  + 0x04881d05, 23);
        MD5STEP(F3, a, b, c, d, md5->buffer[9]  + 0xd9d4d039,  4);
        MD5STEP(F3, d, a, b, c, md5->buffer[12] + 0xe6db99e5, 11);
        MD5STEP(F3, c, d, a, b, md5->buffer[15] + 0x1fa27cf8, 16);
        MD5STEP(F3, b, c, d, a, md5->buffer[2]  + 0xc4ac5665, 23);

        MD5STEP(F4, a, b, c, d, md5->buffer[0]  + 0xf4292244,  6);
        MD5STEP(F4, d, a, b, c, md5->buffer[7]  + 0x432aff97, 10);
        MD5STEP(F4, c, d, a, b, md5->buffer[14] + 0xab9423a7, 15);
        MD5STEP(F4, b, c, d, a, md5->buffer[5]  + 0xfc93a039, 21);
        MD5STEP(F4, a, b, c, d, md5->buffer[12] + 0x655b59c3,  6);
        MD5STEP(F4, d, a, b, c, md5->buffer[3]  + 0x8f0ccc92, 10);
        MD5STEP(F4, c, d, a, b, md5->buffer[10] + 0xffeff47d, 15);
        MD5STEP(F4, b, c, d, a, md5->buffer[1]  + 0x85845dd1, 21);
        MD5STEP(F4, a, b, c, d, md5->buffer[8]  + 0x6fa87e4f,  6);
        MD5STEP(F4, d, a, b, c, md5->buffer[15] + 0xfe2ce6e0, 10);
        MD5STEP(F4, c, d, a, b, md5->buffer[6]  + 0xa3014314, 15);
        MD5STEP(F4, b, c, d, a, md5->buffer[13] + 0x4e0811a1, 21);
        MD5STEP(F4, a, b, c, d, md5->buffer[4]  + 0xf7537e82,  6);
        MD5STEP(F4, d, a, b, c, md5->buffer[11] + 0xbd3af235, 10);
        MD5STEP(F4, c, d, a, b, md5->buffer[2]  + 0x2ad7d2bb, 15);
        MD5STEP(F4, b, c, d, a, md5->buffer[9]  + 0xeb86d391, 21);

        /* Add the working vars back into digest state[]  */
        md5->digest[0] += a;
        md5->digest[1] += b;
        md5->digest[2] += c;
        md5->digest[3] += d;

        return 0;
    }
#endif /* NEED_SOFT_MD5 */

#ifndef HAVE_MD5_CUST_API

INLINE void AddLength(wc_Md5* md5, word32 len)
{
    word32 tmp = md5->loLen;
    if ((md5->loLen += len) < tmp) {
        md5->hiLen++;                       /* carry low to high */
    }
}

static int _InitMd5(wc_Md5* md5)
{
    int ret = 0;

    md5->digest[0] = 0x67452301L;
    md5->digest[1] = 0xefcdab89L;
    md5->digest[2] = 0x98badcfeL;
    md5->digest[3] = 0x10325476L;

    md5->buffLen = 0;
    md5->loLen   = 0;
    md5->hiLen   = 0;

    return ret;
}

int wc_InitMd5_ex(wc_Md5* md5, void* heap, int devId)
{
    int ret = 0;

    if (md5 == NULL)
        return BAD_FUNC_ARG;

    md5->heap = heap;

    ret = _InitMd5(md5);
    if (ret != 0)
        return ret;

#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_MD5)
    ret = wolfAsync_DevCtxInit(&md5->asyncDev, WOLFSSL_ASYNC_MARKER_MD5,
                                                            md5->heap, devId);
#else
    (void)devId;
#endif
    return ret;
}

int wc_Md5Update(wc_Md5* md5, const byte* data, word32 len)
{
    int ret = 0;
    byte* local;

    if (md5 == NULL || (data == NULL && len > 0)) {
        return BAD_FUNC_ARG;
    }

#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_MD5)
    if (md5->asyncDev.marker == WOLFSSL_ASYNC_MARKER_MD5) {
    #if defined(HAVE_INTEL_QA)
        return IntelQaSymMd5(&md5->asyncDev, NULL, data, len);
    #endif
    }
#endif /* WOLFSSL_ASYNC_CRYPT */

    /* do block size increments */
    local = (byte*)md5->buffer;

    /* check that internal buffLen is valid */
    if (md5->buffLen >= WC_MD5_BLOCK_SIZE)
        return BUFFER_E;

    while (len) {
        word32 add = min(len, WC_MD5_BLOCK_SIZE - md5->buffLen);
        XMEMCPY(&local[md5->buffLen], data, add);

        md5->buffLen += add;
        data         += add;
        len          -= add;

        if (md5->buffLen == WC_MD5_BLOCK_SIZE) {
        #if defined(BIG_ENDIAN_ORDER) && !defined(FREESCALE_MMCAU_SHA)
            ByteReverseWords(md5->buffer, md5->buffer, WC_MD5_BLOCK_SIZE);
        #endif
            XTRANSFORM(md5, local);
            AddLength(md5, WC_MD5_BLOCK_SIZE);
            md5->buffLen = 0;
        }
    }
    return ret;
}

int wc_Md5Final(wc_Md5* md5, byte* hash)
{
    byte* local;

    if (md5 == NULL || hash == NULL) {
        return BAD_FUNC_ARG;
    }

#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_MD5)
    if (md5->asyncDev.marker == WOLFSSL_ASYNC_MARKER_MD5) {
    #if defined(HAVE_INTEL_QA)
        return IntelQaSymMd5(&md5->asyncDev, hash, NULL, WC_MD5_DIGEST_SIZE);
    #endif
    }
#endif /* WOLFSSL_ASYNC_CRYPT */

    local = (byte*)md5->buffer;

    AddLength(md5, md5->buffLen);  /* before adding pads */
    local[md5->buffLen++] = 0x80;  /* add 1 */

    /* pad with zeros */
    if (md5->buffLen > WC_MD5_PAD_SIZE) {
        XMEMSET(&local[md5->buffLen], 0, WC_MD5_BLOCK_SIZE - md5->buffLen);
        md5->buffLen += WC_MD5_BLOCK_SIZE - md5->buffLen;

    #if defined(BIG_ENDIAN_ORDER) && !defined(FREESCALE_MMCAU_SHA)
        ByteReverseWords(md5->buffer, md5->buffer, WC_MD5_BLOCK_SIZE);
    #endif
        XTRANSFORM(md5, local);
        md5->buffLen = 0;
    }
    XMEMSET(&local[md5->buffLen], 0, WC_MD5_PAD_SIZE - md5->buffLen);

#if defined(BIG_ENDIAN_ORDER) && !defined(FREESCALE_MMCAU_SHA)
    ByteReverseWords(md5->buffer, md5->buffer, WC_MD5_BLOCK_SIZE);
#endif

    /* put lengths in bits */
    md5->hiLen = (md5->loLen >> (8*sizeof(md5->loLen) - 3)) +
                 (md5->hiLen << 3);
    md5->loLen = md5->loLen << 3;

    /* store lengths */
    /* ! length ordering dependent on digest endian type ! */
    XMEMCPY(&local[WC_MD5_PAD_SIZE], &md5->loLen, sizeof(word32));
    XMEMCPY(&local[WC_MD5_PAD_SIZE + sizeof(word32)], &md5->hiLen, sizeof(word32));

    /* final transform and result to hash */
    XTRANSFORM(md5, local);
#ifdef BIG_ENDIAN_ORDER
    ByteReverseWords(md5->digest, md5->digest, WC_MD5_DIGEST_SIZE);
#endif
    XMEMCPY(hash, md5->digest, WC_MD5_DIGEST_SIZE);

    return _InitMd5(md5); /* reset state */
}
#endif /* !HAVE_MD5_CUST_API */


int wc_InitMd5(wc_Md5* md5)
{
    if (md5 == NULL) {
        return BAD_FUNC_ARG;
    }
    return wc_InitMd5_ex(md5, NULL, INVALID_DEVID);
}

void wc_Md5Free(wc_Md5* md5)
{
    if (md5 == NULL)
        return;
#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_MD5)
    wolfAsync_DevCtxFree(&md5->asyncDev, WOLFSSL_ASYNC_MARKER_MD5);
#endif /* WOLFSSL_ASYNC_CRYPT */
}

int wc_Md5GetHash(wc_Md5* md5, byte* hash)
{
    int ret;
    wc_Md5 tmpMd5;

    if (md5 == NULL || hash == NULL)
        return BAD_FUNC_ARG;

    ret = wc_Md5Copy(md5, &tmpMd5);
    if (ret == 0) {
        ret = wc_Md5Final(&tmpMd5, hash);
    }

    return ret;
}

int wc_Md5Copy(wc_Md5* src, wc_Md5* dst)
{
    int ret = 0;

    if (src == NULL || dst == NULL)
        return BAD_FUNC_ARG;

    XMEMCPY(dst, src, sizeof(wc_Md5));

#ifdef WOLFSSL_ASYNC_CRYPT
    ret = wolfAsync_DevCopy(&src->asyncDev, &dst->asyncDev);
#endif
#ifdef WOLFSSL_PIC32MZ_HASH
    ret = wc_Pic32HashCopy(&src->cache, &dst->cache);
#endif

    return ret;
}

#endif /* WOLFSSL_TI_HASH */
#endif /* NO_MD5 */
