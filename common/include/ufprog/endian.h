/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Byte-order related
 */
#pragma once

#ifndef _UFPROG_ENDIAN_H_
#define _UFPROG_ENDIAN_H_

#ifdef _WIN32

#include <ufprog/common.h>

EXTERN_C_BEGIN

static inline uint64_t bswap64(uint64_t _x)
{
	return ((_x >> 56) | ((_x >> 40) & 0xff00) | ((_x >> 24) & 0xff0000) |
		((_x >> 8) & 0xff000000) | ((_x << 8) & ((uint64_t)0xff << 32)) |
		((_x << 24) & ((uint64_t)0xff << 40)) |
		((_x << 40) & ((uint64_t)0xff << 48)) | ((_x << 56)));
}

static inline uint32_t bswap32(uint32_t _x)
{
	return ((_x & 0xff000000U) >> 24) | ((_x & 0x00ff0000U) >> 8) |
		((_x & 0x0000ff00U) << 8) | ((_x & 0x000000ffU) << 24);
}

static inline uint16_t bswap16(uint16_t _x)
{
	return ((_x & 0xff00) >> 8) | ((_x & 0x00ff) << 8);
}

#ifdef WIN32
#define __HOST_LE
#else
#ifndef __BYTE_ORDER__
#error __BYTE_ORDER__ is not defined!
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __HOST_LE
#endif
#endif

#ifdef __HOST_LE
#define ntohl(x)	(bswap32(x))
#define ntohs(x)	(bswap16(x))
#define htonl(x)	(bswap32(x))
#define htons(x)	(bswap16(x))

#define htobe16(x)	bswap16((x))
#define htobe32(x)	bswap32((x))
#define htobe64(x)	bswap64((x))
#define htole16(x)	((uint16_t)(x))
#define htole32(x)	((uint32_t)(x))
#define htole64(x)	((uint64_t)(x))

#define be16toh(x)	bswap16((x))
#define be32toh(x)	bswap32((x))
#define be64toh(x)	bswap64((x))
#define le16toh(x)	((uint16_t)(x))
#define le32toh(x)	((uint32_t)(x))
#define le64toh(x)	((uint64_t)(x))
#else
#define ntohl(x)	((uint32_t)(x))
#define ntohs(x)	((uint16_t)(x))
#define htonl(x)	((uint32_t)(x))
#define htons(x)	((uint16_t)(x))

#define htobe16(x)	((uint16_t)(x))
#define htobe32(x)	((uint32_t)(x))
#define htobe64(x)	((uint64_t)(x))
#define htole16(x)	bswap16((x))
#define htole32(x)	bswap32((x))
#define htole64(x)	bswap64((x))

#define be16toh(x)	((uint16_t)(x))
#define be32toh(x)	((uint32_t)(x))
#define be64toh(x)	((uint64_t)(x))
#define le16toh(x)	bswap16((x))
#define le32toh(x)	bswap32((x))
#define le64toh(x)	bswap64((x))
#endif

EXTERN_C_END

#endif /* _WIN32 */

#endif /* _UFPROG_ENDIAN_H_ */
