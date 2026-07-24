#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Format a raw 2-byte SAE J2012 diagnostic trouble code (the on-wire form the
// J1850 modules report) into its 5-character text, e.g.
// dtc_format(0xD2, 0x55, out) -> "U1255". `out` must hold >= 6 bytes
// (5 chars + NUL). Returns out.
//
// Encoding (SAE J2012): the top 2 bits of `hi` pick the system letter
// (00 = P powertrain, 01 = C chassis, 10 = B body, 11 = U network); the next
// 2 bits are the first digit (0-3); the low nibble of `hi` plus both nibbles of
// `lo` are the remaining three hex digits.
//
// This helper only turns a KNOWN code pair into text. The request that asks a
// module for its codes, and the layout of the reply, are the HD-specific
// J1850 diagnostic protocol below (ported from HarleyDroid, github.com/
// stelian42/HarleyDroid — src/org/harleydroid/{HarleyDroidDiagnostics,J1850}.java).
char *dtc_format(uint8_t hi, uint8_t lo, char out[6]);

// J1850 diagnostic module addresses HarleyDroid targets. Used as the target
// byte in a request and as the responder id (3rd header byte) in the reply.
#define DTC_MODULE_ECM   0x10  // engine control (ICM/ECM)
#define DTC_MODULE_TSM   0x40  // turn-signal / security (TSM/TSSM)
#define DTC_MODULE_OTHER 0x60  // speedo / tach / other

// Build the "read stored DTCs" request for one module into out[7]; returns 7.
// On the wire (HD J1850 VPW): priority 6C, target = module, source F1 (tester),
// service 19 52 (readDTCByStatus) with mask FF 00. The TX driver appends the
// CRC. e.g. dtc_request(0x10, out) -> 6C 10 F1 19 52 FF 00.
size_t dtc_request(uint8_t module, uint8_t out[7]);

// Build the "clear stored DTCs" request (service 14) into out[4]; returns 4.
// e.g. dtc_clear_request(0x40, out) -> 6C 40 F1 14.
size_t dtc_clear_request(uint8_t module, uint8_t out[4]);

// Decode a captured frame as a DTC read RESPONSE (6C F1 <module> 59 hi lo ...),
// one code per frame. Returns true and fills *module/*hi/*lo when the frame is
// such a response carrying the two code bytes; false otherwise. Any out pointer
// may be NULL. A (hi,lo) of (0,0) is the module's "no (more) codes" terminator
// - still a valid response; the caller decides whether to format it. Mirrors
// HarleyDroid's (x & 0xffff0fff) == 0x6cf10059 header match.
bool dtc_response(const uint8_t *f, size_t len, uint8_t *module, uint8_t *hi, uint8_t *lo);
