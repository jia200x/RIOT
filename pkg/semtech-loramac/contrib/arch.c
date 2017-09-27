#include "loramac/board_definitions.h"
#include "random.h"

void srand1( uint32_t seed )
{
    random_init(seed);
}

int32_t randr( int32_t min, int32_t max )
{
    return random_uint32_range(min, max + 1);
}


void memcpy1( uint8_t *dst, const uint8_t *src, uint16_t size )
{
    while( size-- )
    {
        *dst++ = *src++;
    }
}


void memcpyr( uint8_t *dst, const uint8_t *src, uint16_t size )
{
   dst = dst + ( size - 1 );
   while( size-- )
   {
       *dst-- = *src++;
   }
}

void memset1( uint8_t *dst, uint8_t value, uint16_t size )
{
    while( size-- )
    {
        *dst++ = value;
    }
}

int8_t Nibble2HexChar( uint8_t a )
{
    if( a < 10 )
    {
        return '0' + a;
    }
    else if( a < 16 )
    {
        return 'A' + ( a - 10 );
    }
    else
    {
        return '?';
    }
}

