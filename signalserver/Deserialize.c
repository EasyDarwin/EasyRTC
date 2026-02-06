#define LOG_CLASS "SDP"
#include "Sdp.h"
#include <string.h>

#define STRNCPY    strncpy
#define STRNCMP    strncmp
#define STRLEN     strlen

#define ENTERS()
#define LEAVES()
#define STATUS_FAILED(x)    (((STATUS) (x)) != STATUS_SUCCESS)
#define STATUS_SUCCEEDED(x) (!STATUS_FAILED(x))

#define STATUS_BASE                              0x00000000
#define STATUS_NULL_ARG                          STATUS_BASE + 0x00000001
#define STATUS_INVALID_ARG                       STATUS_BASE + 0x00000002
#define STATUS_BUFFER_TOO_SMALL                  STATUS_BASE + 0x00000005

#define STATUS_UTILS_BASE                            0x40000000
#define STATUS_INVALID_BASE                          STATUS_UTILS_BASE + 0x00000002
#define STATUS_INVALID_DIGIT                         STATUS_UTILS_BASE + 0x00000003
#define STATUS_INT_OVERFLOW                          STATUS_UTILS_BASE + 0x00000004
#define STATUS_EMPTY_STRING                          STATUS_UTILS_BASE + 0x00000005

#define STATUS_WEBRTC_BASE 0x55000000
#define STATUS_SDP_BASE                         STATUS_WEBRTC_BASE + 0x01000000
#define STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED       STATUS_SDP_BASE + 0x0000000F
#define STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION     STATUS_WEBRTC_BASE + 0x00000007

#define MAX_STRING_CONVERSION_BASE 36

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof *(array))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

////////////////////////////////////////////////////
// Conditional checks
////////////////////////////////////////////////////
#define CHK(condition, errRet)                                                                                                                       \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_ERR(condition, errRet, errorMessage, ...)                                                                                                \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            DLOGE(errorMessage, ##__VA_ARGS__);                                                                                                      \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_WARN(condition, errRet, errorMessage, ...)                                                                                               \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            DLOGW(errorMessage, ##__VA_ARGS__);                                                                                                      \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_STATUS_ERR(condition, errRet, errorMessage, ...)                                                                                         \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            retStatus = (__status);                                                                                                                  \
            DLOGE(errorMessage, ##__VA_ARGS__);                                                                                                      \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_STATUS(condition)                                                                                                                        \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            retStatus = (__status);                                                                                                                  \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_STATUS_CONTINUE(condition)                                                                                                               \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            retStatus = __status;                                                                                                                    \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_HANDLE(handle)                                                                                                                           \
    do {                                                                                                                                             \
        if (IS_VALID_HANDLE(handle)) {                                                                                                               \
            retStatus = (STATUS_INVALID_HANDLE_ERROR);                                                                                               \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_LOG(condition, logMessage)                                                                                                               \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            DLOGS("%s Returned status code: 0x%08x", logMessage, __status);                                                                          \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_LOG_ERR(condition)                                                                                                                       \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            DLOGE("operation returned status code: 0x%08x", __status);                                                                               \
        }                                                                                                                                            \
    } while (FALSE)

// Checks if the data provided is in range [low, high]
#define CHECK_IN_RANGE(data, low, high) ((data) >= (low) && (data) <= (high))

PCHAR strnchr(PCHAR pStr, UINT32 strLen, CHAR ch)
{
	if (pStr == NULL || strLen == 0) {
		return NULL;
	}

	UINT32 i = 0;

	while (*pStr != ch) {
		if (*pStr++ == '\0' || i++ == strLen - 1) {
			return NULL;
		}
	}

	return pStr;
}

#define STRNCHR    strnchr

STATUS unsignedSafeMultiplyAdd(UINT64 multiplicand, UINT64 multiplier, UINT64 addend, PUINT64 pResult)
{
	STATUS retStatus = STATUS_SUCCESS;
	UINT64 multiplicandHi, multiplicandLo, multiplierLo, multiplierHi, intermediate, result;

	CHK(pResult != NULL, STATUS_NULL_ARG);

	// Set the result to 0 to start with
	*pResult = 0;

	// Quick special case handling
	if (multiplicand == 0 || multiplier == 0) {
		// Early successful return - just return the addend
		*pResult = addend;

		CHK(FALSE, STATUS_SUCCESS);
	}

	// Perform the multiplication first
	// multiplicand * multiplier == (multiplicandHi + multiplicandLo) * (multiplierHi + multiplierLo)
	// which evaluates to
	// multiplicandHi * multiplierHi +
	// multiplicandHi * multiplierLo + multiplicandLo * multiplierHi +
	// multiplicandLo * multiplierLo
	multiplicandLo = multiplicand & 0x00000000ffffffff;
	multiplicandHi = (multiplicand & 0xffffffff00000000) >> (UINT64)32;
	multiplierLo = multiplier & 0x00000000ffffffff;
	multiplierHi = (multiplier & 0xffffffff00000000) >> (UINT64)32;

	// If both high parts are non-0 then we do have an overflow
	if (multiplicandHi != 0 && multiplierHi != 0) {
		CHK_STATUS(STATUS_INT_OVERFLOW);
	}

	// Intermediate result shouldn't overflow
	// intermediate = multiplicandHi * multiplierLo + multiplicandLo * multiplierHi;
	// as we have multiplicandHi or multiplierHi being 0
	intermediate = multiplicandHi * multiplierLo + multiplicandLo * multiplierHi;

	// Check if we overflowed the 32 bit
	if (intermediate > 0x00000000ffffffff) {
		CHK_STATUS(STATUS_INT_OVERFLOW);
	}

	// The resulting multiplication is
	// result = intermediate << 32 + multiplicandLo * multiplierLo
	// after which we need to add the addend
	intermediate <<= 32;

	result = intermediate + multiplicandLo * multiplierLo;

	if (result < intermediate) {
		CHK_STATUS(STATUS_INT_OVERFLOW);
	}

	// Finally, add the addend
	intermediate = result;
	result += addend;

	if (result < intermediate) {
		CHK_STATUS(STATUS_INT_OVERFLOW);
	}

	*pResult = result;

CleanUp:

	return retStatus;
}

STATUS strtoint(PCHAR pStart, PCHAR pEnd, UINT32 base, PUINT64 pRet, PBOOL pSign)
{
	STATUS retStatus = STATUS_SUCCESS;
	PCHAR pCur = pStart;
	BOOL seenChars = FALSE;
	BOOL positive = TRUE;
	UINT64 result = 0;
	UINT64 digit;
	CHAR curChar;

	// Simple check for NULL
	CHK(pCur != NULL && pRet != NULL && pSign != NULL, STATUS_NULL_ARG);

	// Check for start and end pointers if end is specified
	CHK(pEnd == NULL || pEnd >= pCur, STATUS_INVALID_ARG);

	// Check the base
	CHK(base >= 2 && base <= MAX_STRING_CONVERSION_BASE, STATUS_INVALID_BASE);

	// Check the sign
	switch (*pCur) {
	case '-':
		positive = FALSE;
		// Deliberate fall-through
	case '+':
		pCur++;
	default:
		break;
	}

	while (pCur != pEnd && *pCur != '\0') {
		curChar = *pCur;
		if (curChar >= '0' && curChar <= '9') {
			digit = (UINT64)(curChar - '0');
		}
		else if (curChar >= 'a' && curChar <= 'z') {
			digit = (UINT64)(curChar - 'a') + 10;
		}
		else if (curChar >= 'A' && curChar <= 'Z') {
			digit = (UINT64)(curChar - 'A') + 10;
		}
		else {
			CHK(FALSE, STATUS_INVALID_DIGIT);
		}

		// Set as processed
		seenChars = TRUE;

		// Check against the base
		CHK(digit < base, STATUS_INVALID_BASE);

		// Safe operation which results in
		// result = result * base + digit;
		CHK_STATUS(unsignedSafeMultiplyAdd(result, base, digit, &result));

		pCur++;
	}

	CHK(seenChars, STATUS_EMPTY_STRING);

	if (!positive) {
		result = (UINT64)((INT64)result * -1);
	}

	*pRet = result;
	*pSign = positive;

CleanUp:

	return retStatus;
}

STATUS strtoui64(PCHAR pStart, PCHAR pEnd, UINT32 base, PUINT64 pRet)
{
	STATUS retStatus = STATUS_SUCCESS;
	UINT64 result;
	BOOL sign;

	CHK(pRet != NULL, STATUS_NULL_ARG);

	// Convert to UINT64
	CHK_STATUS(strtoint(pStart, pEnd, base, &result, &sign));

	// Check for the overflow
	CHK(sign, STATUS_INVALID_DIGIT);

	*pRet = result;

CleanUp:

	return retStatus;
}

#define STRTOUI64 strtoui64

STATUS parseMediaName(PSessionDescription pSessionDescription, PCHAR pch, UINT32 lineLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_BUFFER_TOO_SMALL);

    STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName, (pch + SDP_ATTRIBUTE_LENGTH),
            MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
    pSessionDescription->mediaCount++;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS parseSessionAttributes(PSessionDescription pSessionDescription, PCHAR pch, UINT32 lineLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR search;

    CHK(pSessionDescription->sessionAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    if ((search = STRNCHR(pch, lineLen, ':')) == NULL) {
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName, pch + SDP_ATTRIBUTE_LENGTH,
                MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
    } else {
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName, pch + SDP_ATTRIBUTE_LENGTH,
                (search - (pch + SDP_ATTRIBUTE_LENGTH)));
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue, search + 1,
                MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, lineLen - (search - pch + 1)));
    }

    pSessionDescription->sessionAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS parseMediaAttributes(PSessionDescription pSessionDescription, PCHAR pch, UINT32 lineLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR search;
    UINT16 currentMediaAttributesCount;

    currentMediaAttributesCount = pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaAttributesCount;

    CHK(currentMediaAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    if ((search = STRNCHR(pch, lineLen, ':')) == NULL) {
        STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].sdpAttributes[currentMediaAttributesCount].attributeName,
                pch + SDP_ATTRIBUTE_LENGTH, MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
    } else {
        STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].sdpAttributes[currentMediaAttributesCount].attributeName,
                pch + SDP_ATTRIBUTE_LENGTH, (search - (pch + SDP_ATTRIBUTE_LENGTH)));
        STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].sdpAttributes[currentMediaAttributesCount].attributeValue,
                search + 1, MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, lineLen - (search - pch + 1)));
    }
    pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS deserializeSessionDescription(PSessionDescription pSessionDescription, PCHAR sdpBytes)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR curr, tail, next;
    UINT32 lineLen;
    CHK(sdpBytes != NULL, STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION);

    curr = sdpBytes;
    tail = sdpBytes + STRLEN(sdpBytes);

    while ((next = STRNCHR(curr, tail - curr, '\n')) != NULL) {
        lineLen = (UINT32) (next - curr);

        if (lineLen > 0 && curr[lineLen - 1] == '\r') {
            lineLen--;
        }

        if (0 == STRNCMP(curr, SDP_MEDIA_NAME_MARKER, (ARRAY_SIZE(SDP_MEDIA_NAME_MARKER) - 1))) {
            CHK_STATUS(parseMediaName(pSessionDescription, curr, lineLen));
        }

        if (pSessionDescription->mediaCount != 0) {
            if (0 == STRNCMP(curr, SDP_ATTRIBUTE_MARKER, (ARRAY_SIZE(SDP_ATTRIBUTE_MARKER) - 1))) {
                CHK_STATUS(parseMediaAttributes(pSessionDescription, curr, lineLen));
            }

            // Media Title
            if (0 == STRNCMP(curr, SDP_INFORMATION_MARKER, (ARRAY_SIZE(SDP_INFORMATION_MARKER) - 1))) {
                STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle, (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }
        } else {
            // SDP Session Name
            if (0 == STRNCMP(curr, SDP_SESSION_NAME_MARKER, (ARRAY_SIZE(SDP_SESSION_NAME_MARKER) - 1))) {
                STRNCPY(pSessionDescription->sessionName, (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP Session Name
            if (0 == STRNCMP(curr, SDP_INFORMATION_MARKER, (ARRAY_SIZE(SDP_INFORMATION_MARKER) - 1))) {
                STRNCPY(pSessionDescription->sessionInformation, (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP URI
            if (0 == STRNCMP(curr, SDP_URI_MARKER, (ARRAY_SIZE(SDP_URI_MARKER) - 1))) {
                STRNCPY(pSessionDescription->uri, (curr + SDP_ATTRIBUTE_LENGTH), MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP Email Address
            if (0 == STRNCMP(curr, SDP_EMAIL_ADDRESS_MARKER, (ARRAY_SIZE(SDP_EMAIL_ADDRESS_MARKER) - 1))) {
                STRNCPY(pSessionDescription->emailAddress, (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP Phone number
            if (0 == STRNCMP(curr, SDP_PHONE_NUMBER_MARKER, (ARRAY_SIZE(SDP_PHONE_NUMBER_MARKER) - 1))) {
                STRNCPY(pSessionDescription->phoneNumber, (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            if (0 == STRNCMP(curr, SDP_VERSION_MARKER, (ARRAY_SIZE(SDP_VERSION_MARKER) - 1))) {
                STRTOUI64(curr + SDP_ATTRIBUTE_LENGTH, curr + MIN(lineLen, MAX_SDP_TOKEN_LENGTH), 10, &pSessionDescription->version);
            }

            if (0 == STRNCMP(curr, SDP_ATTRIBUTE_MARKER, (ARRAY_SIZE(SDP_ATTRIBUTE_MARKER) - 1))) {
                CHK_STATUS(parseSessionAttributes(pSessionDescription, curr, lineLen));
            }
        }

        curr = next + 1;
    }

CleanUp:

    LEAVES();
    return retStatus;
}
