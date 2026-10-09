/*
 * SPDX-FileCopyrightText: 2001-2002 Georges Menie
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * Conversion engine descended from Georges Menie's printf-stdarg, rewritten
 * around a sink that takes runs of characters (a literal stretch, a number's
 * digits, a %s body) in one call rather than one character at a time, so a
 * queued destination pushes a block instead of a byte. The buffer entry
 * points are the same engine over a sink that copies instead of queuing.
 *
 * The contract - what is accepted and what happens otherwise - is stated in
 * format/format.h.
 */

#include "format/format.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "format_settings.h"

/** Characters a padding run writes at a time. */
#define FORMAT_FILL_MAX 16u

/** What a precision below zero says: the conversion was given none. */
#define FORMAT_PRECISION_NONE (-1)

/** The two bases the conversions ask for. */
#define FORMAT_BASE_DECIMAL     10u
#define FORMAT_BASE_HEXADECIMAL 16u

/** What a null pointer prints as, for %s as much as for %p. */
#define FORMAT_NULL_TEXT "(null)"

/** The widest integer the engine converts, and its signed counterpart. */
#if FORMAT_CONFIG_LONG_LONG
typedef unsigned long long format_uint_t;
typedef long long format_int_t;
#else
typedef unsigned long format_uint_t;
typedef long format_int_t;
#endif

/**
 * Room for the longest conversion: base ten of the widest value, where three
 * digits per byte is the bound, plus the two characters of the longest head.
 */
#define FORMAT_NUMBER_MAX ((sizeof(format_uint_t) * 3u) + 2u)

/** The flags of a conversion, a set rather than a value. */
typedef enum
{
    FORMAT_PAD_NONE  = 0, /**< Pad on the left, with spaces. */
    FORMAT_PAD_RIGHT = 1, /**< '-': the value, then the padding. */
    FORMAT_PAD_ZERO  = 2  /**< '0': pad with zeros, not spaces. */
} format_pad_t;

/** How wide the argument of a numeric conversion is. */
typedef enum
{
    FORMAT_MODIFIER_INT = 0, /**< No modifier. */
    FORMAT_MODIFIER_CHAR,    /**< hh */
    FORMAT_MODIFIER_SHORT,   /**< h */
    FORMAT_MODIFIER_LONG,    /**< l */
#if FORMAT_CONFIG_LONG_LONG
    FORMAT_MODIFIER_LONG_LONG, /**< ll */
#endif
    FORMAT_MODIFIER_SIZE /**< z */
} format_modifier_t;

/**
 * Where a conversion goes. Either a callback, or a buffer, never both: the
 * buffer entry points are the engine with `write` left null.
 */
typedef struct
{
    format_write_callback_t write; /**< The sink, null for a buffer. */
    void *context;                 /**< Handed back to write. */
    char *out;     /**< Where the next character goes, null without a buffer. */
    size_t room;   /**< What still fits in out, its terminator excluded. */
    size_t length; /**< What the whole output would be, its terminator
                      excluded: what the call returns. */
} format_sink_t;

/**
 * @brief Take one run of characters, and count it whether or not it was taken.
 *
 * Counting a run the buffer could not hold is what makes the return value of
 * the buffer entry points the length the output would have had.
 */
static void format_sink_write(format_sink_t *sink,
                              const char *data,
                              size_t length)
{
    if (length > 0u)
    {
        if (sink->write != NULL)
        {
            sink->write(data, length, sink->context);
        }
        else
        {
            const size_t fits = (length < sink->room) ? length : sink->room;

            if (fits > 0u)
            {
                memcpy(sink->out, data, fits);
                sink->out += fits;
                sink->room -= fits;
            }
        }

        sink->length += length;
    }
}

/** @brief Take @p count copies of @p fill, by runs rather than one by one. */
static void format_sink_fill(format_sink_t *sink, char fill, size_t count)
{
    if (count > 0u)
    {
        char run[FORMAT_FILL_MAX];
        size_t remaining = count;

        memset(run, fill, sizeof(run));

        while (remaining > 0u)
        {
            const size_t chunk =
                (remaining < sizeof(run)) ? remaining : sizeof(run);

            format_sink_write(sink, run, chunk);
            remaining -= chunk;
        }
    }
}

/**
 * @brief Take @p head then @p body, padded together to @p width.
 *
 * The two are padded as one, and the fill goes where it cannot change what is
 * read: spaces in front of the head, zeros behind it - "  -42" and "-0042",
 * never "-  42" nor "00-42".
 *
 * @param sink        Where the result goes.
 * @param head        The sign or the base marker, "" when there is none.
 * @param body        The digits, or whatever else the conversion produced.
 * @param body_length How many of them.
 * @param width       The width the conversion asked for.
 * @param pad         Its flags.
 */
static void format_put_padded(format_sink_t *sink,
                              const char *head,
                              const char *body,
                              size_t body_length,
                              int width,
                              unsigned int pad)
{
    const size_t head_length = strlen(head);
    const size_t length      = head_length + body_length;
    const size_t requested   = (width > 0) ? (size_t)width : 0u;
    const size_t fill_count  = (requested > length) ? (requested - length) : 0u;
    const bool right         = ((pad & (unsigned int)FORMAT_PAD_RIGHT) != 0u);
    /* Zeros to the right of a value would change it, so the left justified
     * conversion pads with spaces whatever its flags say - as C does. */
    const bool zero = ((pad & (unsigned int)FORMAT_PAD_ZERO) != 0u) && !right;

    if (!right && !zero)
    {
        format_sink_fill(sink, ' ', fill_count);
    }

    format_sink_write(sink, head, head_length);

    if (zero)
    {
        format_sink_fill(sink, '0', fill_count);
    }

    format_sink_write(sink, body, body_length);

    if (right)
    {
        format_sink_fill(sink, ' ', fill_count);
    }
}

/** @brief Take a run with no head, padded to @p width. */
static void format_put_run(format_sink_t *sink,
                           const char *data,
                           size_t length,
                           int width,
                           unsigned int pad)
{
    format_put_padded(sink, "", data, length, width, pad);
}

/**
 * @brief Write @p value in @p base into @p text, most significant digit first.
 *
 * @param text        At least FORMAT_NUMBER_MAX characters.
 * @param value       What to convert.
 * @param base        Ten or sixteen.
 * @param letter_base 'a' or 'A', the case of the digits above nine.
 *
 * @return How many digits were written; one at least, since zero is a digit.
 */
static size_t format_digits(char *text,
                            format_uint_t value,
                            unsigned int base,
                            char letter_base)
{
    char reversed[FORMAT_NUMBER_MAX];
    format_uint_t remaining = value;
    size_t count            = 0u;
    size_t index            = 0u;

    /* The divisions produce the digits backwards; they are turned round
     * below rather than written into a buffer whose length is not known yet. */
    do
    {
        const unsigned int digit = (unsigned int)(remaining % base);
        const char letter        = (char)((int)letter_base + (int)digit - 10);

        reversed[count] = (digit < 10u) ? (char)('0' + (int)digit) : letter;
        count++;
        remaining /= base;
    } while (remaining != 0u);

    for (index = 0u; index < count; index++)
    {
        text[index] = reversed[count - 1u - index];
    }

    return count;
}

/**
 * @brief Convert @p value in @p base, behind @p head, and take the result.
 *
 * @param sink        Where the result goes.
 * @param value       The magnitude to convert; the sign belongs to head.
 * @param head        What comes before the digits - "-", "0x", or "".
 * @param base        Ten or sixteen.
 * @param letter_base 'a' or 'A', the case of the digits above nine.
 * @param width       The width the conversion asked for.
 * @param pad         Its flags.
 */
static void format_put_number(format_sink_t *sink,
                              format_uint_t value,
                              const char *head,
                              unsigned int base,
                              char letter_base,
                              int width,
                              unsigned int pad)
{
    char digits[FORMAT_NUMBER_MAX];
    const size_t length = format_digits(digits, value, base, letter_base);

    format_put_padded(sink, head, digits, length, width, pad);
}

#if FORMAT_CONFIG_FLOAT

/**
 * Digits after the point, or after the first significant one. A double
 * carries nothing past seventeen, so more is met with this many rather than
 * invented digits. The four extra cover %g: turning its precision into a
 * decimal count, the smallest fixed-notation value pushes four zeros before
 * the first significant digit.
 */
#    define FORMAT_FLOAT_PRECISION_MAX 17u
#    define FORMAT_FLOAT_DECIMALS_MAX  (FORMAT_FLOAT_PRECISION_MAX + 4u)

/** What a conversion that was given no precision uses, as C states. */
#    define FORMAT_FLOAT_PRECISION_DEFAULT 6u

/** Twenty digits of integer part, the point, the decimals, the exponent. */
#    define FORMAT_FLOAT_MAX 48u

/** Bit for bit, what a double is, so that the classes are read not computed. */
#    define FORMAT_FLOAT_EXPONENT_MASK 0x7FF0000000000000u
#    define FORMAT_FLOAT_MANTISSA_MASK 0x000FFFFFFFFFFFFFu
#    define FORMAT_FLOAT_SIGN_MASK     0x8000000000000000u

/** The decimal expansion of a value, rounded at the precision asked for. */
typedef struct
{
    format_uint_t whole;                    /**< Before the point. */
    char digits[FORMAT_FLOAT_DECIMALS_MAX]; /**< After it. */
    unsigned int count;                     /**< How many of them. */
} format_decimal_t;

/** A finite double taken apart: its magnitude is significand * 2^exponent. */
typedef struct
{
    uint64_t significand; /**< 53 bits, the implicit one included. */
    int exponent;         /**< What the binary point is shifted by. */
} format_binary_t;

/**
 * The fractional part of a magnitude, as binary places in 32-bit limbs from
 * the most significant. Wide enough that no fixed conversion loses a place:
 * a magnitude needing more limbs has decimals that are all zero at any
 * precision this engine allows.
 */
#    define FORMAT_FRACTION_LIMBS  4u
#    define FORMAT_FRACTION_PLACES (FORMAT_FRACTION_LIMBS * 32u)

typedef struct
{
    uint32_t limb[FORMAT_FRACTION_LIMBS];
} format_fraction_t;

_Static_assert(sizeof(double) == sizeof(uint64_t),
               "the float conversions read a double as an IEEE 754 binary64");
_Static_assert(sizeof(format_uint_t) >= sizeof(uint64_t),
               "the integer part of a double is carried by the accumulator");

/** @brief What @p value is made of, as the bits the format defines. */
static uint64_t format_float_bits(double value)
{
    uint64_t bits = 0u;

    memcpy(&bits, &value, sizeof(bits));

    return bits;
}

/** @brief Whether @p value carries the sign bit, which a zero can too. */
static bool format_float_is_negative(double value)
{
    const uint64_t bits = format_float_bits(value);

    return (bits & FORMAT_FLOAT_SIGN_MASK) != 0u;
}

/** @brief What @p value prints as when it is not a number, or null. */
static const char *format_float_word(double value)
{
    const uint64_t bits = format_float_bits(value);
    const char *word    = NULL;

    if ((bits & FORMAT_FLOAT_EXPONENT_MASK) == FORMAT_FLOAT_EXPONENT_MASK)
    {
        word = ((bits & FORMAT_FLOAT_MANTISSA_MASK) == 0u) ? "inf" : "nan";
    }

    return word;
}

/** @brief Whether the magnitude of @p value is past what fixed notation holds.
 */
static bool format_float_is_huge(double value)
{
    const uint64_t bits      = format_float_bits(value);
    const unsigned int field = (unsigned int)((bits >> 52) & 0x7FFu);

    /* A magnitude sits in [2^e, 2^(e+1)), so the exponent field alone says
     * whether it reached two to the sixty fourth. */
    return field >= (1023u + 64u);
}

/**
 * @brief Take the magnitude of @p value apart, sign dropped.
 *
 * @param binary Where the pieces are written.
 * @param value  A finite value, of either sign.
 */
static void format_float_split(format_binary_t *binary, double value)
{
    const uint64_t bits      = format_float_bits(value);
    const unsigned int field = (unsigned int)((bits >> 52) & 0x7FFu);
    const uint64_t mantissa  = bits & FORMAT_FLOAT_MANTISSA_MASK;

    if (field == 0u)
    {
        /* A subnormal carries no implicit one, and its exponent is the one of
         * the smallest normal rather than the field read as zero. */
        binary->significand = mantissa;
        binary->exponent    = -1022 - 52;
    }
    else
    {
        binary->significand = mantissa | (1ull << 52);
        binary->exponent    = (int)field - 1023 - 52;
    }
}

/**
 * @brief Set @p fraction to @p value divided by two to the @p places.
 *
 * Places past what the limbs hold are dropped. They are worth less than the
 * last place kept, which no rounding this engine performs can reach.
 */
static void format_fraction_set(format_fraction_t *fraction,
                                uint64_t value,
                                unsigned int places)
{
    unsigned int index = 0u;
    unsigned int bit   = 0u;

    for (index = 0u; index < FORMAT_FRACTION_LIMBS; index++)
    {
        fraction->limb[index] = 0u;
    }

    for (bit = 0u; bit < 64u; bit++)
    {
        if ((((value >> bit) & 1u) != 0u) && (bit < places))
        {
            /* The bit of weight two to the bit, over two to the places, lands
             * that many binary places below the point. */
            const unsigned int place = places - bit;

            if (place <= FORMAT_FRACTION_PLACES)
            {
                const unsigned int limb  = (place - 1u) / 32u;
                const unsigned int shift = 31u - ((place - 1u) % 32u);

                fraction->limb[limb] |= 1u << shift;
            }
        }
    }
}

/** @brief Multiply by ten, and hand back the digit that came out of it. */
static unsigned int format_fraction_times_ten(format_fraction_t *fraction)
{
    uint64_t carry     = 0u;
    unsigned int index = FORMAT_FRACTION_LIMBS;

    while (index > 0u)
    {
        uint64_t product = 0u;

        index--;
        product               = ((uint64_t)fraction->limb[index] * 10u) + carry;
        fraction->limb[index] = (uint32_t)product;
        carry                 = product >> 32;
    }

    return (unsigned int)carry;
}

/**
 * @brief Compare what is left of a fraction with one half.
 *
 * @return Negative below it, zero on it, positive above it. The exact answer:
 *         it is what decides a rounding, and the tie is a case of its own.
 */
static int format_fraction_compare_half(const format_fraction_t *fraction)
{
    const uint32_t half = 0x80000000u;
    int comparison      = 0;

    if (fraction->limb[0] != half)
    {
        comparison = (fraction->limb[0] > half) ? 1 : -1;
    }
    else
    {
        unsigned int index = 0u;

        for (index = 1u; index < FORMAT_FRACTION_LIMBS; index++)
        {
            if (fraction->limb[index] != 0u)
            {
                comparison = 1;
            }
        }
    }

    return comparison;
}

/**
 * @brief Expand @p value into digits, rounded at @p precision decimals.
 *
 * Every digit comes from the bits of the value, never from float arithmetic:
 * multiplying a fraction by ten in floating point pushes low bits out of the
 * mantissa before they are read, which made the engine this descends from
 * disagree with a C library from the fifteenth digit on.
 *
 * @param decimal   What the expansion is written to.
 * @param value     A finite value, of a magnitude that
 *                  format_float_is_huge() rejects.
 * @param precision Decimals to produce, at most
 *                  FORMAT_FLOAT_DECIMALS_MAX.
 */
static void format_float_expand(format_decimal_t *decimal,
                                double value,
                                unsigned int precision)
{
    format_binary_t binary;
    format_fraction_t fraction;
    unsigned int index = 0u;
    int comparison     = 0;
    bool round_up      = false;

    assert(precision <= FORMAT_FLOAT_DECIMALS_MAX);
    assert(!format_float_is_huge(value));

    format_float_split(&binary, value);
    decimal->count = precision;

    if (binary.exponent >= 0)
    {
        /* Nothing below the point: the value is an integer, and one that fits
         * since the magnitude was checked against the limit. */
        decimal->whole = binary.significand << (unsigned int)binary.exponent;
        format_fraction_set(&fraction, 0u, 0u);
    }
    else
    {
        const unsigned int places = (unsigned int)(-binary.exponent);

        if (places < 64u)
        {
            const uint64_t mask = (1ull << places) - 1ull;

            decimal->whole = binary.significand >> places;
            format_fraction_set(&fraction, binary.significand & mask, places);
        }
        else
        {
            decimal->whole = 0u;
            format_fraction_set(&fraction, binary.significand, places);
        }
    }

    for (index = 0u; index < precision; index++)
    {
        const unsigned int digit = format_fraction_times_ten(&fraction);

        decimal->digits[index] = (char)('0' + (int)digit);
    }

    /* Round to nearest, tie to even - what a C library follows under the
     * current rounding mode. Truncating, as the ancestor engine did, loses
     * half a digit every time. */
    comparison = format_fraction_compare_half(&fraction);
    round_up   = (comparison > 0);

    if (comparison == 0)
    {
        const unsigned int last =
            (precision > 0u)
                ? (unsigned int)(decimal->digits[precision - 1u] - '0')
                : (unsigned int)(decimal->whole % 2u);

        round_up = ((last % 2u) != 0u);
    }

    if (round_up)
    {
        unsigned int position = precision;
        bool carry            = true;

        while (carry && (position > 0u))
        {
            position--;

            if (decimal->digits[position] == '9')
            {
                decimal->digits[position] = '0';
            }
            else
            {
                decimal->digits[position]++;
                carry = false;
            }
        }

        if (carry)
        {
            decimal->whole++;
        }
    }
}

/**
 * @brief Write @p value in fixed notation into @p text.
 *
 * @param text      At least FORMAT_FLOAT_MAX characters.
 * @param value     A finite value, of a magnitude that
 *                  format_float_is_huge() rejects.
 * @param precision Decimals to write.
 *
 * @return How many characters were written.
 */
static size_t format_float_fixed(char *text,
                                 double value,
                                 unsigned int precision)
{
    format_decimal_t decimal;
    size_t length = 0u;

    format_float_expand(&decimal, value, precision);

    length = format_digits(text, decimal.whole, FORMAT_BASE_DECIMAL, 'a');

    if (precision > 0u)
    {
        text[length] = '.';
        length++;
        memcpy(&text[length], decimal.digits, precision);
        length += precision;
    }

    return length;
}

#    if FORMAT_CONFIG_FLOAT_BIG

/** @brief @p value with its sign dropped, which costs no arithmetic. */
static double format_float_magnitude(double value)
{
    const uint64_t bits = format_float_bits(value) & ~FORMAT_FLOAT_SIGN_MASK;
    double magnitude    = 0.0;

    memcpy(&magnitude, &bits, sizeof(magnitude));

    return magnitude;
}

/** A power of ten and what it is worth, for bringing a value into [1, 10). */
typedef struct
{
    double factor; /**< Ten to the step. */
    int step;      /**< What applying it moves the exponent by. */
} format_power_t;

/**
 * Halving steps, so that any exponent a double can carry is reached in a few
 * multiplications rather than in three hundred.
 */
static const format_power_t format_powers[] = {
    { 1e256, 256 }, { 1e128, 128 }, { 1e64, 64 }, { 1e32, 32 }, { 1e16, 16 },
    { 1e8, 8 },     { 1e4, 4 },     { 1e2, 2 },   { 1e1, 1 }
};

#        define FORMAT_POWER_COUNT                                             \
            (sizeof(format_powers) / sizeof(format_powers[0]))

/**
 * @brief Bring @p value into [1, 10), and say by how much.
 *
 * @param value    A finite magnitude above zero.
 * @param exponent Where the power of ten that was taken out is written.
 *
 * @return The value, scaled.
 */
static double format_float_normalise(double value, int *exponent)
{
    double scaled = value;
    size_t index  = 0u;

    *exponent = 0;

    for (index = 0u; index < FORMAT_POWER_COUNT; index++)
    {
        while (scaled >= format_powers[index].factor)
        {
            scaled /= format_powers[index].factor;
            *exponent += format_powers[index].step;
        }
    }

    for (index = 0u; index < FORMAT_POWER_COUNT; index++)
    {
        while ((scaled * format_powers[index].factor) < 10.0)
        {
            scaled *= format_powers[index].factor;
            *exponent -= format_powers[index].step;
        }
    }

    return scaled;
}

/**
 * @brief Write @p value in exponent notation into @p text.
 *
 * @param text      At least FORMAT_FLOAT_MAX characters.
 * @param value     A finite magnitude, zero included.
 * @param precision Digits to write after the first significant one.
 * @param exponent  Where the exponent that was written is reported, which a
 *                  rounding onto ten may have moved.
 *
 * @return How many characters were written.
 */
static size_t format_float_scientific(char *text,
                                      double value,
                                      unsigned int precision,
                                      int *exponent)
{
    format_decimal_t decimal;
    char magnitude[FORMAT_NUMBER_MAX];
    double scaled = value;
    size_t length = 0u;
    size_t count  = 0u;

    *exponent = 0;

    if (value != 0.0)
    {
        scaled = format_float_normalise(value, exponent);
    }

    format_float_expand(&decimal, scaled, precision);

    /* Rounding can carry the single digit onto ten: it becomes a one one
     * exponent higher, the carry having already zeroed every digit after. */
    if (decimal.whole >= 10u)
    {
        decimal.whole = 1u;
        (*exponent)++;
    }

    text[length] = (char)('0' + (int)decimal.whole);
    length++;

    if (precision > 0u)
    {
        text[length] = '.';
        length++;
        memcpy(&text[length], decimal.digits, precision);
        length += precision;
    }

    text[length] = 'e';
    length++;
    text[length] = (*exponent < 0) ? '-' : '+';
    length++;

    count = format_digits(
        magnitude,
        (format_uint_t)((*exponent < 0) ? -(*exponent) : *exponent),
        FORMAT_BASE_DECIMAL,
        'a');

    /* Two digits at least, which is what C asks of the exponent. */
    if (count < 2u)
    {
        text[length] = '0';
        length++;
    }

    memcpy(&text[length], magnitude, count);
    length += count;

    return length;
}

/**
 * @brief Drop the zeros a %g conversion is not allowed to keep.
 *
 * Trailing fractional zeros go, and the point too if none are left; what
 * follows the exponent marker is kept, moved back over the drop.
 *
 * @param text   What was written.
 * @param length How much of it.
 *
 * @return What is left of it.
 */
static size_t format_float_trim(char *text, size_t length)
{
    size_t point  = length;
    size_t marker = length;
    size_t result = length;
    size_t index  = 0u;

    for (index = 0u; index < length; index++)
    {
        if ((text[index] == '.') && (point == length))
        {
            point = index;
        }
        if ((text[index] == 'e') && (marker == length))
        {
            marker = index;
        }
    }

    if (point < length)
    {
        size_t end = marker;

        while ((end > (point + 1u)) && (text[end - 1u] == '0'))
        {
            end--;
        }

        if (end == (point + 1u))
        {
            end = point;
        }

        for (index = 0u; index < (length - marker); index++)
        {
            text[end + index] = text[marker + index];
        }

        result = end + (length - marker);
    }

    return result;
}

/**
 * @brief Write @p value in the shorter of the two notations, into @p text.
 *
 * @param text      At least FORMAT_FLOAT_MAX characters.
 * @param value     A finite magnitude, zero included.
 * @param precision Significant digits, as %g counts them.
 *
 * @return How many characters were written.
 */
static size_t format_float_general(char *text,
                                   double value,
                                   unsigned int precision)
{
    /* C: precision zero counts as one; the notation is picked from the
     * exponent the scientific form gives, computed first for that reason. */
    const unsigned int significant = (precision > 0u) ? precision : 1u;
    int exponent                   = 0;
    size_t length =
        format_float_scientific(text, value, significant - 1u, &exponent);

    if ((exponent >= -4) && (exponent < (int)significant))
    {
        unsigned int decimals = (unsigned int)((int)significant - 1 - exponent);

        if (decimals > FORMAT_FLOAT_DECIMALS_MAX)
        {
            decimals = FORMAT_FLOAT_DECIMALS_MAX;
        }

        length = format_float_fixed(text, value, decimals);
    }

    return format_float_trim(text, length);
}

#    endif /* FORMAT_CONFIG_FLOAT_BIG */

/** Which of the notations a conversion asked for. */
typedef enum
{
    FORMAT_FORM_FIXED = 0 /**< %f */
#    if FORMAT_CONFIG_FLOAT_BIG
        ,
    FORMAT_FORM_SCIENTIFIC, /**< %e */
    FORMAT_FORM_GENERAL     /**< %g */
#    endif
} format_form_t;

/**
 * @brief Convert @p value in the notation @p form asks for, and take it.
 *
 * @param sink      Where the result goes.
 * @param value     What to convert, of any class.
 * @param precision What the conversion asked for, or none.
 * @param width     The width it asked for.
 * @param pad       Its flags.
 * @param form      The notation.
 */
static void format_put_float(format_sink_t *sink,
                             double value,
                             int precision,
                             int width,
                             unsigned int pad,
                             format_form_t form)
{
    char text[FORMAT_FLOAT_MAX];
    const bool negative    = format_float_is_negative(value);
    const char *const head = negative ? "-" : "";
    const char *word       = format_float_word(value);
    unsigned int decimals  = (precision > FORMAT_PRECISION_NONE)
                                 ? (unsigned int)precision
                                 : FORMAT_FLOAT_PRECISION_DEFAULT;
    size_t length          = 0u;

    if (decimals > FORMAT_FLOAT_PRECISION_MAX)
    {
        decimals = FORMAT_FLOAT_PRECISION_MAX;
    }

#    if !FORMAT_CONFIG_FLOAT_BIG
    /* Fixed notation only holds what an integer accumulator fits; with no
     * exponent form to fall back on, a wrong number must never be written. */
    if ((word == NULL) && format_float_is_huge(value))
    {
        word = "ovf";
    }
#    endif

    if (word != NULL)
    {
        /* A zero fill would read as part of a number, and these are words. */
        format_put_padded(sink,
                          head,
                          word,
                          strlen(word),
                          width,
                          pad & ~(unsigned int)FORMAT_PAD_ZERO);
    }
    else
    {
        switch (form)
        {
            case FORMAT_FORM_FIXED:
#    if FORMAT_CONFIG_FLOAT_BIG
                if (format_float_is_huge(value))
                {
                    int exponent = 0;

                    length =
                        format_float_scientific(text,
                                                format_float_magnitude(value),
                                                decimals,
                                                &exponent);
                }
                else
                {
                    length = format_float_fixed(text, value, decimals);
                }
#    else
                length = format_float_fixed(text, value, decimals);
#    endif
                break;
#    if FORMAT_CONFIG_FLOAT_BIG
            case FORMAT_FORM_SCIENTIFIC: {
                int exponent = 0;

                length = format_float_scientific(text,
                                                 format_float_magnitude(value),
                                                 decimals,
                                                 &exponent);
                break;
            }
            case FORMAT_FORM_GENERAL:
                length = format_float_general(text,
                                              format_float_magnitude(value),
                                              decimals);
                break;
#    endif
        }

        format_put_padded(sink, head, text, length, width, pad);
    }
}

#endif /* FORMAT_CONFIG_FLOAT */

/** @brief How much of @p string a precision of @p precision shows. */
static size_t format_string_length(const char *string, int precision)
{
    const size_t limit =
        (precision > FORMAT_PRECISION_NONE) ? (size_t)precision : SIZE_MAX;
    size_t length = 0u;

    while ((length < limit) && (string[length] != '\0'))
    {
        length++;
    }

    return length;
}

/** @brief Read a signed argument of the width @p modifier asked for. */
static format_int_t format_signed_argument(va_list *args,
                                           format_modifier_t modifier)
{
    format_int_t value = 0;

    /* int, long and size_t share a width on every 32-bit target, so some
     * branches read the same thing; kept apart since a host may differ.
     * NOLINTBEGIN(bugprone-branch-clone) */
    switch (modifier)
    {
        case FORMAT_MODIFIER_CHAR:
            value = (format_int_t)(signed char)va_arg(*args, int);
            break;
        case FORMAT_MODIFIER_SHORT:
            value = (format_int_t)(short)va_arg(*args, int);
            break;
        case FORMAT_MODIFIER_INT:
            value = va_arg(*args, int);
            break;
        case FORMAT_MODIFIER_LONG:
            value = va_arg(*args, long);
            break;
#if FORMAT_CONFIG_LONG_LONG
        case FORMAT_MODIFIER_LONG_LONG:
            value = va_arg(*args, long long);
            break;
#endif
        case FORMAT_MODIFIER_SIZE:
            value = (format_int_t)va_arg(*args, size_t);
            break;
    }
    /* NOLINTEND(bugprone-branch-clone) */

    return value;
}

/** @brief Read an unsigned argument of the width @p modifier asked for. */
static format_uint_t format_unsigned_argument(va_list *args,
                                              format_modifier_t modifier)
{
    format_uint_t value = 0u;

    /* int, long and size_t share a width on every 32-bit target, so some
     * branches read the same thing; kept apart since a host may differ.
     * NOLINTBEGIN(bugprone-branch-clone) */
    switch (modifier)
    {
        case FORMAT_MODIFIER_CHAR:
            value = (unsigned char)va_arg(*args, unsigned int);
            break;
        case FORMAT_MODIFIER_SHORT:
            value = (unsigned short)va_arg(*args, unsigned int);
            break;
        case FORMAT_MODIFIER_INT:
            value = va_arg(*args, unsigned int);
            break;
        case FORMAT_MODIFIER_LONG:
            value = va_arg(*args, unsigned long);
            break;
#if FORMAT_CONFIG_LONG_LONG
        case FORMAT_MODIFIER_LONG_LONG:
            value = va_arg(*args, unsigned long long);
            break;
#endif
        case FORMAT_MODIFIER_SIZE:
            value = va_arg(*args, size_t);
            break;
    }
    /* NOLINTEND(bugprone-branch-clone) */

    return value;
}

/**
 * @brief Convert the one conversion @p spec opens, and take its result.
 *
 * @param sink What the conversion produces.
 * @param spec The '%' that opens it.
 * @param args The arguments, advanced by what the conversion reads.
 *
 * @return What follows the conversion in the format.
 */
static const char *format_convert(format_sink_t *sink,
                                  const char *spec,
                                  va_list *args)
{
    const char *cursor         = &spec[1];
    unsigned int pad           = (unsigned int)FORMAT_PAD_NONE;
    int width                  = 0;
    int precision              = FORMAT_PRECISION_NONE;
    format_modifier_t modifier = FORMAT_MODIFIER_INT;
    bool flagged               = true;
    char conversion            = '\0';

    while (flagged)
    {
        if (*cursor == '-')
        {
            pad |= (unsigned int)FORMAT_PAD_RIGHT;
            cursor++;
        }
        else if (*cursor == '0')
        {
            pad |= (unsigned int)FORMAT_PAD_ZERO;
            cursor++;
        }
        else
        {
            flagged = false;
        }
    }

    while ((*cursor >= '0') && (*cursor <= '9'))
    {
        width = (width * 10) + (*cursor - '0');
        cursor++;
    }

    if (*cursor == '.')
    {
        cursor++;

        if (*cursor == '*')
        {
            cursor++;
            precision = va_arg(*args, int);
        }
        else
        {
            precision = 0;

            while ((*cursor >= '0') && (*cursor <= '9'))
            {
                precision = (precision * 10) + (*cursor - '0');
                cursor++;
            }
        }
    }

    if ((cursor[0] == 'h') && (cursor[1] == 'h'))
    {
        modifier = FORMAT_MODIFIER_CHAR;
        cursor   = &cursor[2];
    }
    else if (cursor[0] == 'h')
    {
        modifier = FORMAT_MODIFIER_SHORT;
        cursor++;
    }
#if FORMAT_CONFIG_LONG_LONG
    else if ((cursor[0] == 'l') && (cursor[1] == 'l'))
    {
        modifier = FORMAT_MODIFIER_LONG_LONG;
        cursor   = &cursor[2];
    }
#endif
    else if (cursor[0] == 'l')
    {
        modifier = FORMAT_MODIFIER_LONG;
        cursor++;
    }
    else if (cursor[0] == 'z')
    {
        modifier = FORMAT_MODIFIER_SIZE;
        cursor++;
    }

    conversion = *cursor;

    switch (conversion)
    {
        case 'd':
        case 'i': {
            const format_int_t value = format_signed_argument(args, modifier);
            const bool negative      = (value < 0);
            const char *const head   = negative ? "-" : "";
            /* Negated in two steps: the magnitude of the most negative value
             * has no signed counterpart to negate it through. */
            const format_uint_t magnitude =
                negative ? ((format_uint_t)(-(value + 1)) + 1u)
                         : (format_uint_t)value;

            format_put_number(sink,
                              magnitude,
                              head,
                              FORMAT_BASE_DECIMAL,
                              'a',
                              width,
                              pad);
            cursor++;
            break;
        }
        case 'u': {
            const format_uint_t value =
                format_unsigned_argument(args, modifier);

            format_put_number(sink,
                              value,
                              "",
                              FORMAT_BASE_DECIMAL,
                              'a',
                              width,
                              pad);
            cursor++;
            break;
        }
        case 'x':
        case 'X': {
            const format_uint_t value =
                format_unsigned_argument(args, modifier);
            const char letter_base = (conversion == 'x') ? 'a' : 'A';

            format_put_number(sink,
                              value,
                              "",
                              FORMAT_BASE_HEXADECIMAL,
                              letter_base,
                              width,
                              pad);
            cursor++;
            break;
        }
        case 'p': {
            const void *const pointer = va_arg(*args, const void *);

            if (pointer == NULL)
            {
                format_put_run(sink,
                               FORMAT_NULL_TEXT,
                               strlen(FORMAT_NULL_TEXT),
                               width,
                               pad);
            }
            else
            {
                const format_uint_t value = (format_uint_t)(uintptr_t)pointer;

                format_put_number(sink,
                                  value,
                                  "0x",
                                  FORMAT_BASE_HEXADECIMAL,
                                  'a',
                                  width,
                                  pad);
            }

            cursor++;
            break;
        }
        case 'c': {
            /* Anything narrower than an int is promoted on its way here. */
            const char value = (char)va_arg(*args, int);

            format_put_run(sink, &value, 1u, width, pad);
            cursor++;
            break;
        }
        case 's': {
            const char *const argument = va_arg(*args, const char *);
            const char *const string =
                (argument != NULL) ? argument : FORMAT_NULL_TEXT;
            const size_t length = format_string_length(string, precision);

            format_put_run(sink, string, length, width, pad);
            cursor++;
            break;
        }
#if FORMAT_CONFIG_FLOAT
        case 'f': {
            const double value = va_arg(*args, double);

            format_put_float(sink,
                             value,
                             precision,
                             width,
                             pad,
                             FORMAT_FORM_FIXED);
            cursor++;
            break;
        }
#endif
#if FORMAT_CONFIG_FLOAT_BIG
        case 'e': {
            const double value = va_arg(*args, double);

            format_put_float(sink,
                             value,
                             precision,
                             width,
                             pad,
                             FORMAT_FORM_SCIENTIFIC);
            cursor++;
            break;
        }
        case 'g': {
            const double value = va_arg(*args, double);

            format_put_float(sink,
                             value,
                             precision,
                             width,
                             pad,
                             FORMAT_FORM_GENERAL);
            cursor++;
            break;
        }
#endif
        case '%':
            format_sink_write(sink, cursor, 1u);
            cursor++;
            break;
        default: {
            /* Unknown conversion, or a trailing '%': echo the spec back so
             * a bad format shows in the output instead of vanishing. The
             * argument stays unread, shifting later ones - see the header. */
            const size_t shown =
                (size_t)(cursor - spec) + ((conversion != '\0') ? 1u : 0u);

            format_sink_write(sink, spec, shown);

            if (conversion != '\0')
            {
                cursor++;
            }

            break;
        }
    }

    return cursor;
}

/** @brief Walk @p format, taking its literal runs and its conversions. */
static void format_format(format_sink_t *sink,
                          const char *format,
                          va_list *args)
{
    const char *cursor = format;

    while (*cursor != '\0')
    {
        const char *const literal = cursor;

        while ((*cursor != '\0') && (*cursor != '%'))
        {
            cursor++;
        }

        format_sink_write(sink, literal, (size_t)(cursor - literal));

        if (*cursor == '%')
        {
            cursor = format_convert(sink, cursor, args);
        }
    }
}

/**
 * @brief Format into a sink, which takes the result one run at a time.
 *
 * Nothing is buffered and nothing is truncated: the sink sees the line as it is
 * produced, so the caller has no length to size and no line to cut.
 *
 * @param write   Takes each run. Must not be null.
 * @param context Handed back to write, untouched.
 * @param format  What to produce, see the header for what it accepts.
 *
 * @return How many characters were produced.
 */
int format_emit(format_write_callback_t write,
                void *context,
                const char *format,
                ...)
{
    va_list args;
    int length = 0;

    va_start(args, format);
    length = format_vemit(write, context, format, args);
    va_end(args);

    return length;
}

/**
 * @brief format_emit() with the arguments already opened.
 *
 * @param write   Takes each run. Must not be null.
 * @param context Handed back to write, untouched.
 * @param format  What to produce, see the header for what it accepts.
 * @param args    The arguments, advanced by what the format reads.
 *
 * @return How many characters were produced.
 */
int format_vemit(format_write_callback_t write,
                 void *context,
                 const char *format,
                 va_list args)
{
    format_sink_t sink = { .write   = write,
                           .context = context,
                           .out     = NULL,
                           .room    = 0u,
                           .length  = 0u };
    /* Copied because the engine takes the address of the list, and the address
     * of a parameter of this type is not the address of a list on every ABI. */
    va_list walked;

    assert(write != NULL);
    assert(format != NULL);

    va_copy(walked, args);
    format_format(&sink, format, &walked);
    va_end(walked);

    return (int)sink.length;
}

/**
 * @brief Format into a buffer, under the rules POSIX snprintf() states.
 *
 * @param out    Where the result goes, terminator included. May be null only
 *               when size is zero, which is how the length is measured.
 * @param size   What out holds, its terminator included.
 * @param format What to produce, see the header for what it accepts.
 *
 * @return The length the result would have had, its terminator excluded:
 *         a value of size or more says the result was truncated.
 */
int format_snprintf(char *out, size_t size, const char *format, ...)
{
    va_list args;
    int length = 0;

    va_start(args, format);
    length = format_vsnprintf(out, size, format, args);
    va_end(args);

    return length;
}

/**
 * @brief format_snprintf() with the arguments already opened.
 *
 * @param out    Where the result goes, terminator included. May be null only
 *               when size is zero, which is how the length is measured.
 * @param size   What out holds, its terminator included.
 * @param format What to produce, see the header for what it accepts.
 * @param args   The arguments, advanced by what the format reads.
 *
 * @return The length the result would have had, its terminator excluded:
 *         a value of size or more says the result was truncated.
 */
int format_vsnprintf(char *out, size_t size, const char *format, va_list args)
{
    /* One character of the buffer is the terminator's, from the start: the
     * result is terminated whatever it costs the body. */
    format_sink_t sink = { .write   = NULL,
                           .context = NULL,
                           .out     = out,
                           .room    = (size > 0u) ? (size - 1u) : 0u,
                           .length  = 0u };
    /* See format_vemit(). */
    va_list walked;

    assert((out != NULL) || (size == 0u));
    assert(format != NULL);

    va_copy(walked, args);
    format_format(&sink, format, &walked);
    va_end(walked);

    if (size > 0u)
    {
        *sink.out = '\0';
    }

    return (int)sink.length;
}
