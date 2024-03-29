/***************************************************************************
*                        Array Stream Implementation
*
*   File    : array.c
*   Purpose : This file implements a simple library allowing arrays of
*             unsigned char to be treated as input/output streams of bits.
*             This library was created with in-memory data compression in
*             mind, but may be used for a number of other applications.
*
*             Array streams are FIFO (first in/first out) structures.  The
*             first bit written is the first bit returned.  This
*             implementation writes/reads starting with the MSB of index 0.
*
*   Author  : Michael Dipperstein
*   Date    : November 23, 2004
*
****************************************************************************
*   UPDATES
*
*   $Id: arraystream.c,v 1.2 2007/08/27 13:01:21 michael Exp $
*   $Log: arraystream.c,v $
*   Revision 1.2  2007/08/27 13:01:21  michael
*   Changes required for LGPL v3.
*
*   Revision 1.1.1.1  2004/12/06 13:41:26  michael
*   initial release
*
*
****************************************************************************
*
* Array Stream: Bit stream operations on arrays
* Copyright (C) 2004, 2007 by Michael Dipperstein (mdipper@cs.ucsb.edu)
*
* This file is part of the array stream library.
*
* The array stream library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the
* License, or (at your option) any later version.
*
* The array stream library is distributed in the hope that it will be
* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
* General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
//#define DEBUG
#ifdef DEBUG
#include <stdio.h>
#endif
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "arraystream.h"

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/

struct array_stream_t
{
    unsigned char *array;       /* pointer to storage array */
    as_mode_t     mode;         /* read or write stream */
    unsigned char bitCount;     /* number of bits in bitBuffer */
    unsigned int  index;        /* index of byte being read/written */
    int len;			/* size of storage array */
};

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/***************************************************************************
*   Function   : MakeArrayStream
*   Description: This function naively wraps an array of unsigned char in an
*                array_stream_t structure.
*   Parameters : array - pointer to the array being wrapped.
*                mode - read/write mode of stream
*   Effects    : An array_stream_t structure will be created for the array
*                passed as a parameter.
*   Returned   : Pointer to the array_stream_t structure for the array
*                stream or NULL on failure.  errno will be set for all
*                failure cases.
***************************************************************************/
array_stream_t *MakeArrayStream(unsigned char *array, as_mode_t mode, int len)
{
    array_stream_t *stream;

    if (array == NULL)
    {
        /* can't wrapper empty steam */
        errno = EBADF;
        stream = NULL;
    }
    else
    {
        stream = (array_stream_t *)malloc(sizeof(array_stream_t));

        if (stream == NULL)
        {
            /* malloc failed */
            errno = ENOMEM;
        }
        else
        {
            /* set structure data */
            stream->array = array;
            stream->mode = mode;
            stream->index = 0;
            stream->bitCount = 0;
	    stream->len = len;
        }
    }

    return (stream);
}

/***************************************************************************
*   Function   : ReleaseArrayStream
*   Description: This function releases the memory allocated to turn an
*                array into an array steam, and returns a pointer to the
*                initial array.
*   Parameters : stream - pointer to the array stream being released
*   Effects    : The structure used to wrap an array, making it an
*                array stream, is freed.
*   Returned   : Pointer to wrapped array.  NULL for failure.
***************************************************************************/
unsigned char *ReleaseArrayStream(array_stream_t *stream)
{
    unsigned char *array = NULL;

    if (stream == NULL)
    {
        return(NULL);
    }

    /* keep pointer to array */
    array = stream->array;

    /* free memory allocated for array stream */
    free(stream);

    return(array);
}

/***************************************************************************
*   Function   : ArrayStreamGetChar
*   Description: This function returns the next byte from the array stream
*                passed as a parameter.
*   Parameters : stream - pointer to the array stream to read from
*   Effects    : Reads a char from the array stream.  The stream index and
*                bit count will be modified as necessary.
*   Returned   : EOF if a whole byte cannot be obtained.  Otherwise,
*                the character read.
***************************************************************************/
int ArrayStreamGetChar(array_stream_t *stream)
{
    int returnValue;
    unsigned char tmp;

    if ((stream == NULL) || (stream->mode != AS_READ) || (stream->len <= 0))
    {
        return(EOF);
    }

    /* read current char and increment index */
    returnValue = stream->array[stream->index];
    stream->index++;
    stream->len--;

    if (stream->bitCount == 0)
    {
        /* all of the bits we need were in the byte we read */
        return returnValue;
    }

    /* we have to return bits from two array elements */
    tmp = ((unsigned char)returnValue) << stream->bitCount;
    tmp |= (stream->array[stream->index] >> (CHAR_BIT - stream->bitCount));

    returnValue = tmp;

    return returnValue;
}

/***************************************************************************
*   Function   : ArrayStreamPutChar
*   Description: This function writes the char passed as a parameter to the
*                array stream passed a parameter.
*   Parameters : c - the character to be written
*                stream - pointer to array stream to write to
*   Effects    : Writes a char to the array stream and advances the
*                internal counters one char.
*   Returned   : On success, the character written, otherwise EOF.
***************************************************************************/
int ArrayStreamPutChar(const int c, array_stream_t *stream)
{
    unsigned char tmp;

    if ((stream == NULL) || (stream->mode != AS_WRITE) || (stream->len <= 0))
    {
	stream->len = -1;
        return(EOF);
    }

    if (stream->bitCount == 0)
    {
        /* we can just put byte into array */
        stream->array[stream->index] = c;
        stream->index++;
	stream->len--;
        return c;
    }

    /* split char across array elements */

    /* clear out uninitialized junk */
    stream->array[stream->index] &= (0xFF << (CHAR_BIT - stream->bitCount));

    /* shift in MS part of char */
    tmp = ((unsigned char)c) >> stream->bitCount;
    stream->array[stream->index] |= tmp;
    stream->index++;
    stream->len--;

    /* clear out uninitialized junk */
    stream->array[stream->index] &= (0xFF >> stream->bitCount);

    /* shift in LS part of char */
    tmp = ((unsigned char)c) << (CHAR_BIT - stream->bitCount);
    stream->array[stream->index] |= tmp;

    return c;
}

/***************************************************************************
*   Function   : ArrayStreamGetBit
*   Description: This function returns the next bit from the array stream
*                passed as a parameter.  The bit value returned is the value
*                of the next unread bit in the stream.
*   Parameters : stream - pointer to the array stream to read from
*   Effects    : Returns the next bit from the array stream and advances the
*                internal counters by one bit.
*   Returned   : 0 if bit == 0, 1 if bit == 1, and EOF if operation fails.
***************************************************************************/
int ArrayStreamGetBit(array_stream_t *stream)
{
    int returnValue;

    if ((stream == NULL) || (stream->mode != AS_READ) || (stream->len <= 0))
    {
        /* nothing valid to read from */
	stream->len = -1;
        return(EOF);
    }

    /* bit to return is the bitCount bit */
    returnValue = (stream->array[stream->index]) &
        (0x80 >> stream->bitCount);

    /* advance count for next bit */
    if (stream->bitCount == (CHAR_BIT - 1))
    {
        /* we already read all the bits at this index.  go to the next one */
        stream->index++;
	stream->len--;
        stream->bitCount = 0;
    }
    else
    {
        stream->bitCount++;
    }

    return (returnValue != 0);
}

/***************************************************************************
*   Function   : ArrayStreamPutBit
*   Description: This function writes the bit passed as a parameter to the
*                array stream passed a parameter.
*   Parameters : c - the bit value to be written
*                stream - pointer to array stream to write to
*   Effects    : Writes a bit to the array stream and advances the internal
*                counters one bit.
*   Returned   : On success, the bit value written, otherwise EOF.
***************************************************************************/
int ArrayStreamPutBit(const int c, array_stream_t *stream)
{
    if ((stream == NULL) || (stream->mode != AS_WRITE) || (stream->len <= 0))
    {
        /* nothing valid to write to */
	stream->len = -1;
        return(EOF);
    }

    /* write bit */
    if (c == 0)
    {
        stream->array[stream->index] &= ~(0x80 >> stream->bitCount);
    }
    else
    {
        stream->array[stream->index] |= (0x80 >> stream->bitCount);
    }

    /* advance count for next bit */
    if (stream->bitCount == (CHAR_BIT -1))
    {
        /* we already read all the bits at this index.  go to the next one */
        stream->index++;
	stream->len--;
        stream->bitCount = 0;
    }
    else
    {
        stream->bitCount++;
    }

    return c;
}

/***************************************************************************
*   Function   : ArrayStreamGetBits
*   Description: This function reads the specified number of bits from the
*                array stream passed as a parameter and writes them to the
*                requested memory location (msb to lsb).
*   Parameters : stream - pointer to array stream to read from
*                bits - address to store bits read
*                count - number of bits to read
*   Effects    : Reads bits from the array stream.  The stream index and
*                bit count will be modified as necessary.
*   Returned   : EOF for failure, otherwise the number of bits read.
***************************************************************************/
int ArrayStreamGetBits(array_stream_t *stream, void *bits,
    const unsigned int count)
{
    unsigned char *bytes, shifts;
    int offset, remaining, returnValue;

    bytes = (unsigned char *)bits;

    if ((stream == NULL) || (bits == NULL) || (stream->mode != AS_READ))
    {
        return(EOF);
    }

    offset = 0;
    remaining = count;

    /* read whole bytes */
    while (remaining >= CHAR_BIT)
    {
        returnValue = ArrayStreamGetChar(stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        bytes[offset] = (unsigned char)returnValue;
        remaining -= CHAR_BIT;
        offset++;
#ifdef DEBUG
    	printf("%X (%d) ", returnValue, remaining);
#endif
    }

    /* read remaining bits */
    shifts = CHAR_BIT - remaining;
    while (remaining > 0)
    {
        returnValue = ArrayStreamGetBit(stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        bytes[offset] <<= 1;
        bytes[offset] |= (returnValue & 0x01);
        remaining--;
    }
#ifdef DEBUG
    if (shifts%8 != 0)
	    printf("%X (%d)\n", bytes[offset], remaining);
    else
            printf("\n");
#endif

    /* shift last bits into position */
    bytes[offset] <<= shifts;

    return count;
}

/***************************************************************************
*   Function   : ArrayStreamPutBits
*   Description: This function writes the specified number of bits from the
*                memory location passed as a parameter to the array sream
*                passed as a parameter.   Bits are written msb to lsb.
*   Parameters : stream - pointer to array stream to write to
*                bits - pointer to bits to write
*                count - number of bits to write
*   Effects    : Writes bits to the array stream and advances the internal
*                counters as necessary.
*   Returned   : EOF for failure, otherwise the number of bits written.  If
*                an error occurs after a partial write, the partially
*                written bits will not be unwritten.
***************************************************************************/
int ArrayStreamPutBits(array_stream_t *stream, void *bits,
    const unsigned int count)
{
    unsigned char *bytes, tmp;
    int offset, remaining, returnValue;

    bytes = (unsigned char *)bits;
#ifdef DEBUG
    printf("PutBits: ");
#endif

    if ((stream == NULL) || (bits == NULL) || (stream->mode != AS_WRITE) || (stream->len <= 0))
    {
	stream->len = -1;
        return(EOF);
    }

    offset = 0;
    remaining = count;

    /* write whole bytes */
    while (remaining >= CHAR_BIT)
    {
        returnValue = ArrayStreamPutChar(bytes[offset], stream);
#ifdef DEBUG
	printf("%X ", bytes[offset]);
	printf("[%X] ", stream->array[stream->index]);
#endif

        if (returnValue == EOF)
        {
            return EOF;
        }

        remaining -= CHAR_BIT;
        offset++;
    }

    tmp = bytes[offset];
#ifdef DEBUG
    if (remaining > 0)
	    printf("%X ", bytes[offset]);
#endif
    while (remaining > 0)
    {
	returnValue = ArrayStreamPutBit((tmp & 0x80), stream);

        if (returnValue == EOF)
        {
            return EOF;
        }
        tmp <<= 1;
        remaining--;
    }
#ifdef DEBUG
    printf("[%X] ", stream->array[stream->index]);
    printf("(%d)\n", count);
#endif
    return count;
}

/***************************************************************************
*   Function   : ArrayStreamGetBitCount
*   Description: This function returns the number of bits read/written so
*                far.
*   Parameters : stream - pointer to array stream in question
*   Effects    : None
*   Returned   : The number of bits read from or written to the array
*                stream.  Only bits read/written since MakeArrayStream
*                are counted.
***************************************************************************/
unsigned long ArrayStreamGetBitCount(array_stream_t *stream)
{
    unsigned long returnValue;

    if (stream == NULL)
    {
        return 0;
    }

    if (stream->len == -1)
    {
    	return(EOF);
    }

    returnValue = 8 * (unsigned long)stream->index;
    returnValue += stream->bitCount;
    return (returnValue);
}

uint16_t PackWord(unsigned int v, short n) {
	if (n > 16)
		n %= 16; // Clamp n to 16 bits
	else if (n == 16)
		return v;
	int s = 8-(n%8); // Shift amount
	uint16_t retVal = ((v>>(8-s))|(v&0xFF)>>(8-s))<<8|(((v&0xFF)<<s)&0xFF);
	return retVal;
}

uint8_t PackByte(unsigned int v, short n) {
	if (n > 8)
		n %=8; // Clamp n to 8 bits
	else if (n == 8)
		return v;
	int s = 8-(n%8); // Shift amount
	uint8_t retVal = (v<<s);
	return retVal;
}

uint16_t UnpackWord(unsigned short v, short n) {
	if (n > 16)
		n %= 16;
	else if (n == 16)
		return v;
	int s = 8-(n%8);
	uint16_t retVal = (v&0xFF>>s)|(v>>(8-s));
	return retVal;
}

uint8_t UnpackByte(unsigned char v, short n) {
	if (n > 8)
		n %= 8;
	else if (n == 8)
		return v;
	int s = 8-(n%8);
	uint8_t retVal = (v>>s);
	return retVal;
}

int ArrayStreamPut8(array_stream_t *stream, uint8_t value,
    const unsigned int count) {
	if (count > 8) {
		errno = EOVERFLOW;
		perror("ArrayStreamPut8");
	}
	uint8_t num[1] = {0};
	num[0] = PackByte(value, count);
	return ArrayStreamPutBits(stream, num, count);
}

int ArrayStreamPut16(array_stream_t *stream, uint16_t value,
    const unsigned int count) {
	if (count > 16) {
		errno = EOVERFLOW;
		perror("ArrayStreamPut16");
	} else if (count <= 8) {
		return ArrayStreamPut8(stream, value, count);
	}
	uint8_t num[2] = {0, 0};
	unsigned short t = htons(PackWord(value, count));
	num[1] = (t>>8)&0xff;
	num[0] = t&0xff;
	return ArrayStreamPutBits(stream, num, count);
}

uint8_t ArrayStreamGet8(array_stream_t *stream, const unsigned int count) {
	if (count > 8) {
		errno = EOVERFLOW;
		perror("ArrayStreamGet8");
	}

	// Wierd bug using anything smaller than 16 bit... screws up arraystream
	//  or the underlying buffer. uint16_t works fine
	unsigned short num = 0;
	ArrayStreamGetBits(stream, &num, count);
	return UnpackByte(num, count);
}

uint16_t ArrayStreamGet16(array_stream_t *stream, const unsigned int count) {
	if (count > 16) {
		errno = EOVERFLOW;
		perror("ArrayStreamGet16");
	} else if (count <= 8) {
		return ArrayStreamGet8(stream, count);
	}
	unsigned short num = 0;
	ArrayStreamGetBits(stream, &num, count);
	return UnpackWord(ntohs(num), count);
}
